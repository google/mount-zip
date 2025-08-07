// Copyright 2021 Google LLC
// Copyright 2008-2019 Alexander Galanin <al@galanin.nnov.ru>
// http://galanin.nnov.ru/~al
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#define PROGRAM_NAME "mount-zip"

// Odd minor versions (e.g. 1.1 or 1.3) are development versions.
// Even minor versions (e.g. 1.2 or 1.4) are stable versions.
#define PROGRAM_VERSION "1.11"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <locale>
#include <new>
#include <string_view>

#include <fuse.h>
#include <fuse_opt.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "data_node.h"
#include "error.h"
#include "log.h"
#include "path.h"
#include "reader.h"
#include "tree.h"

#if (LIBZIP_VERSION_MAJOR < 1)
#error "libzip >= 1.0 is required!"
#endif

#ifndef O_PATH
#define O_PATH 0
#endif

// Prints usage information.
void PrintUsage() {
  std::cout
      << R"(Mounts one or several ZIP archives as a read-only FUSE file system.

Usage:
    )" PROGRAM_NAME
         R"( [options] zip-file
    )" PROGRAM_NAME
         R"( [options] zip-file mount-point
    )" PROGRAM_NAME
         R"( [options] zip-file-1 zip-file-2 ... mount-point

General options:
    -h   --help            print help
    -V   --version         print version
    -q   -o quiet          print fewer log messages
    -v   -o verbose        print more log messages
    -o redact              redact file names from log messages
    -o force               mount ZIP even if password is wrong or missing, or
                           if the encryption or compression method is unsupported
    -o precache            preemptively decompress and cache data
    -o cache=DIR           cache dir (default is $TMPDIR or /tmp)
    -o memcache            cache decompressed data in memory
    -o nocache             no caching of decompressed data
    -o dmask=M             directory permission mask in octal (default 0022)
    -o fmask=M             file permission mask in octal (default 0022)
    -o encoding=CHARSET    original encoding of file names
    -o nomerge             don't merge multiple ZIPs in the same directory
    -o notrim              don't trim the base of the tree
    -o nospecials          no special files (FIFOs, sockets, devices)
    -o nosymlinks          no symbolic links
    -o nohardlinks         no hard links)"
#if FUSE_USE_VERSION >= 30
         R"(
    -o direct_io           use direct I/O)"
#endif
         "\n\n"
      << std::flush;
}

// Parameters for command-line argument processing function.
struct Param {
  // ZIP file paths.
  std::vector<std::string> paths;
  // Cache dir.
  char* cache_dir = nullptr;
  // Access mask for directories.
  unsigned int dmask = 0022;
  // Access mask for files.
  unsigned int fmask = 0022;
  // Conversion options.
  Tree::Options opts;

  ~Param() {
    if (cache_dir) {
      free(cache_dir);
    }
  }
};

#if FUSE_USE_VERSION >= 30
static bool g_direct_io = false;
#endif

Tree* g_tree = nullptr;

// FUSE operations
struct Operations : fuse_operations {
 private:
  struct FileHandle {
    const FileNode* const node;
    Reader::Ptr reader;
  };

  // Converts a C++ exception into a negative error code.
  // Also logs the error.
  // Must be called from within a catch block.
  static int ToError(std::string_view const action, const FileNode& n) {
    try {
      throw;
    } catch (const std::bad_alloc&) {
      LOG(ERROR) << "Cannot " << action << ' ' << n << ": No memory";
      return -ENOMEM;
    } catch (const std::exception& e) {
      LOG(ERROR) << "Cannot " << action << ' ' << n << ": " << e.what();
      return -EIO;
    } catch (...) {
      LOG(ERROR) << "Cannot " << action << ' ' << n << ": Unexpected error";
      return -EIO;
    }
  }

  // Finds a node by full path.
  static const FileNode* FindNode(std::string_view const path) {
    assert(g_tree);
    return g_tree->Find(path);
  }

  static int GetAttr(const char* const path,
#if FUSE_USE_VERSION >= 30
                     struct stat* const z,
                     fuse_file_info* const fi) {
#else
                     struct stat* const z) {
    fuse_file_info* const fi = nullptr;
#endif

    const FileNode* n;

    if (fi) {
      FileHandle* const h = reinterpret_cast<FileHandle*>(fi->fh);
      assert(h);
      n = h->node;
      assert(n);
    } else {
      assert(path);
      n = FindNode(path);
      if (!n) {
        LOG(DEBUG) << "Cannot stat " << Path(path) << ": No such item";
        return -ENOENT;
      }
    }

    *z = *n;
    return 0;
  }

  static int OpenDir(const char* const path, fuse_file_info* const fi) {
    assert(path);
    const FileNode* const n = FindNode(path);
    if (!n) {
      LOG(ERROR) << "Cannot open " << Path(path) << ": No such item";
      return -ENOENT;
    }

    if (!n->IsDir()) {
      LOG(ERROR) << "Cannot open " << *n << ": Not a directory";
      return -ENOTDIR;
    }

    assert(fi);
    static_assert(sizeof(fi->fh) >= sizeof(FileNode*));
    fi->fh = reinterpret_cast<uintptr_t>(n);
#if FUSE_USE_VERSION >= 30
    fi->cache_readdir = true;
#endif
    return 0;
  }

  static int ReadDir(const char*,
                     void* const buf,
                     fuse_fill_dir_t const filler,
                     off_t,
#if FUSE_USE_VERSION >= 30
                     fuse_file_info* const fi,
                     fuse_readdir_flags) try {
#else
                     fuse_file_info* const fi) try {
#endif
    assert(filler);
    assert(fi);
    const FileNode* const n = reinterpret_cast<const FileNode*>(fi->fh);
    assert(n);
    assert(n->IsDir());

    const auto add = [buf, filler, n](const char* const name,
                                      const struct stat* const z) {
#if FUSE_USE_VERSION >= 30
      if (filler(buf, name, z, 0, FUSE_FILL_DIR_PLUS)) {
#else
      if (filler(buf, name, z, 0)) {
#endif
        LOG(ERROR) << "Cannot list items in " << *n
                   << ": Cannot allocate memory";
        throw std::bad_alloc();
      }
    };

    Timer const timer;
    struct stat z = *n;
    add(".", &z);

    if (const FileNode* const parent = n->parent) {
      z = *parent;
      add("..", &z);
    } else {
      add("..", nullptr);
    }

    for (const FileNode& child : n->children) {
      z = child;
      add(child.name.c_str(), &z);
    }

    LOG(DEBUG) << "List " << *n << " -> " << n->children.size() << " items in "
               << timer;
    return 0;
  } catch (const std::bad_alloc&) {
    return -ENOMEM;
  }

  static int Open(const char* const path, fuse_file_info* const fi) {
    assert(path);
    assert(fi);

    const FileNode* const n = FindNode(path);
    if (!n) {
      LOG(ERROR) << "Cannot open " << Path(path) << ": No such item";
      return -ENOENT;
    }

    if (n->IsDir()) {
      LOG(ERROR) << "Cannot open " << *n << ": It is a directory";
      return -EISDIR;
    }

    try {
      Reader::Ptr reader = n->GetReader();
      static_assert(sizeof(fi->fh) >= sizeof(FileHandle*));
      fi->fh = reinterpret_cast<uintptr_t>(
          new FileHandle{.node = n, .reader = std::move(reader)});
      return 0;
    } catch (...) {
      return ToError("open", *n);
    }
  }

  static int Read(const char*,
                  char* const buf,
                  size_t const size,
                  off_t const offset,
                  fuse_file_info* const fi) {
    assert(buf);
    assert(size > 0);

    if (offset < 0 || size > std::numeric_limits<int>::max()) {
      return -EINVAL;
    }

    assert(fi);
    FileHandle* const h = reinterpret_cast<FileHandle*>(fi->fh);
    assert(h);
    const FileNode* const n = h->node;
    assert(n);

    try {
      assert(h->reader);
      return static_cast<int>(
          h->reader->Read(
              buf,
              buf + std::min<size_t>(size, std::numeric_limits<int>::max()),
              offset) -
          buf);
    } catch (...) {
      assert(h->node);
      return ToError("read", *n);
    }
  }

  static int Release(const char*, fuse_file_info* const fi) {
    assert(fi);
    FileHandle* const h = reinterpret_cast<FileHandle*>(fi->fh);
    assert(h);

    const FileNode* const n = h->node;
    assert(n);
    delete h;

    LOG(DEBUG) << "Closed " << *n;
    return 0;
  }

  static int ReadLink(const char* const path,
                      char* const buf,
                      size_t const size) {
    assert(path);
    assert(buf);
    assert(size > 1);

    const FileNode* const n = FindNode(path);
    if (!n) {
      LOG(ERROR) << "Cannot read link " << Path(path) << ": No such item";
      return -ENOENT;
    }

    if (n->GetType() != FileType::Symlink) {
      LOG(ERROR) << "Cannot read link " << *n << ": Not a symlink";
      return -ENOLINK;
    }

    try {
      const Reader::Ptr reader = n->GetReader();
      char* const end = reader->Read(buf, buf + size - 1, 0);
      *end = '\0';
      return 0;
    } catch (...) {
      return ToError("read link", *n);
    }
  }
  static int StatFs(const char*, struct statvfs* const z) {
    assert(z);
    assert(g_tree);
    z->f_bsize = Tree::block_size;
    z->f_frsize = Tree::block_size;
    z->f_blocks = g_tree->GetBlockCount();
    z->f_bfree = 0;
    z->f_bavail = 0;
    z->f_files = g_tree->GetNodeCount();
    z->f_ffree = 0;
    z->f_favail = 0;
    z->f_flag = ST_RDONLY;
    z->f_namemax = NAME_MAX;
    return 0;
  }

#if FUSE_USE_VERSION >= 30
  static void* Init(fuse_conn_info*, fuse_config* const cfg) {
    assert(cfg);
    // Respect inode numbers.
    cfg->use_ino = true;
    cfg->nullpath_ok = true;
    cfg->direct_io = g_direct_io;
    return nullptr;
  }
#endif

 public:
  Operations()
      : fuse_operations{
            .getattr = GetAttr,
            .readlink = ReadLink,
            .open = Open,
            .read = Read,
            .statfs = StatFs,
            .release = Release,
            .opendir = OpenDir,
            .readdir = ReadDir,
#if FUSE_USE_VERSION >= 30
            .init = Init,
#else
            .flag_nullpath_ok = true,
            .flag_nopath = true,
#endif
        } {
  }
};

static const Operations operations;

enum {
  KEY_HELP,
  KEY_VERSION,
  KEY_QUIET,
  KEY_VERBOSE,
  KEY_REDACT,
  KEY_FORCE,
  KEY_ENCODING,
  KEY_PRE_CACHE,
  KEY_MEM_CACHE,
  KEY_NO_MERGE,
  KEY_NO_TRIM,
  KEY_NO_CACHE,
  KEY_NO_SPECIALS,
  KEY_NO_SYMLINKS,
  KEY_NO_HARD_LINKS,
  KEY_DEFAULT_PERMISSIONS,
#if FUSE_USE_VERSION >= 30
  KEY_DIRECT_IO,
#endif
};

// Processes command line arguments.
// Called by fuse_opt_parse().
//
// @param data pointer to Param struct
// @param arg whole argument or option
// @param key reason this function is called
// @param outargs current output argument list
// @return -1 on error, 0 if arg is to be discarded, 1 if arg should be kept
static int ProcessArg(void* data,
                      const char* arg,
                      int key,
                      fuse_args* outargs) {
  assert(data);
  Param& param = *static_cast<Param*>(data);

  // 'magic' fuse_opt_proc return codes
  const int KEEP = 1;
  const int DISCARD = 0;
  const int ERROR = -1;

  switch (key) {
    case KEY_HELP:
      PrintUsage();
      {
#if FUSE_USE_VERSION >= 30
        fuse_opt_add_arg(outargs, "--help");
        char empty[] = "";
        outargs->argv[0] = empty;
#else
        fuse_opt_add_arg(outargs, "-ho");  // I think ho means "help output".
#endif
        fuse_main(outargs->argc, outargs->argv, &operations, nullptr);
      }
      std::exit(EXIT_SUCCESS);

    case KEY_VERSION:
      std::cout << PROGRAM_NAME " version: " PROGRAM_VERSION "\n"
                << "libzip version: " LIBZIP_VERSION "\n"
                << std::flush;
      fuse_opt_add_arg(outargs, "--version");
      fuse_main(outargs->argc, outargs->argv, &operations, nullptr);
      std::exit(EXIT_SUCCESS);

    case FUSE_OPT_KEY_NONOPT:
      if (param.paths.emplace_back(arg).empty()) {
        LOG(ERROR) << "Empty path";
        return ERROR;
      }
      return DISCARD;

    case KEY_QUIET:
      SetLogLevel(LogLevel::ERROR);
      return DISCARD;

    case KEY_VERBOSE:
      SetLogLevel(LogLevel::DEBUG);
      return DISCARD;

    case KEY_REDACT:
      Path::redact = true;
      return DISCARD;

    case KEY_FORCE:
      param.opts.check_password = false;
      param.opts.check_compression = false;
      return DISCARD;

    case KEY_PRE_CACHE:
      param.opts.pre_cache = true;
      return DISCARD;

    case KEY_MEM_CACHE:
      Reader::SetCacheStrategy(CacheStrategy::InMemory);
      return DISCARD;

    case KEY_NO_MERGE:
      param.opts.merge = false;
      return DISCARD;

    case KEY_NO_TRIM:
      param.opts.trim = false;
      return DISCARD;

    case KEY_NO_CACHE:
      Reader::SetCacheStrategy(CacheStrategy::NoCache);
      return DISCARD;

    case KEY_NO_SPECIALS:
      param.opts.include_special_files = false;
      return DISCARD;

    case KEY_NO_SYMLINKS:
      param.opts.include_symlinks = false;
      return DISCARD;

    case KEY_NO_HARD_LINKS:
      param.opts.include_hard_links = false;
      return DISCARD;

    case KEY_DEFAULT_PERMISSIONS:
      DataNode::original_permissions = true;
      return KEEP;

#if FUSE_USE_VERSION >= 30
    case KEY_DIRECT_IO:
      g_direct_io = true;
      return DISCARD;
#endif

    default:
      return KEEP;
  }
}

// Removes directory |dir| in destructor.
struct Cleanup {
  const int dirfd = open(".", O_DIRECTORY | O_PATH);
  fuse_args* args = nullptr;
  std::string mount_point;

  ~Cleanup() {
    if (!mount_point.empty()) {
      if (unlinkat(dirfd, mount_point.c_str(), AT_REMOVEDIR) == 0) {
        LOG(DEBUG) << "Removed mount point " << Path(mount_point);
      } else {
        PLOG(ERROR) << "Cannot remove mount point " << Path(mount_point);
      }
    }

    if (args) {
      fuse_opt_free_args(args);
    }

    if (close(dirfd) < 0) {
      PLOG(ERROR) << "Cannot close file descriptor";
    }
  }
};

class NumPunct : public std::numpunct<char> {
 private:
  char do_thousands_sep() const override { return ','; }
  std::string do_grouping() const override { return "\3"; }
};

int main(int argc, char* argv[]) try {
  static_assert(sizeof(void*) <= sizeof(uint64_t));

  // Ensure that numbers in debug messages have thousands separators.
  // It makes big numbers much easier to read (eg sizes expressed in bytes).
  std::locale::global(std::locale(std::locale::classic(), new NumPunct));
  openlog(PROGRAM_NAME, LOG_PERROR, LOG_USER);
  SetLogLevel(LogLevel::INFO);

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  Cleanup cleanup{.args = &args};
  Param param;

  const fuse_opt opts[] = {
      FUSE_OPT_KEY("--help", KEY_HELP),
      FUSE_OPT_KEY("-h", KEY_HELP),
      FUSE_OPT_KEY("--version", KEY_VERSION),
      FUSE_OPT_KEY("-V", KEY_VERSION),
      FUSE_OPT_KEY("--quiet", KEY_QUIET),
      FUSE_OPT_KEY("quiet", KEY_QUIET),
      FUSE_OPT_KEY("-q", KEY_QUIET),
      FUSE_OPT_KEY("--verbose", KEY_VERBOSE),
      FUSE_OPT_KEY("verbose", KEY_VERBOSE),
      FUSE_OPT_KEY("-v", KEY_VERBOSE),
      FUSE_OPT_KEY("--redact", KEY_REDACT),
      FUSE_OPT_KEY("redact", KEY_REDACT),
      FUSE_OPT_KEY("--force", KEY_FORCE),
      FUSE_OPT_KEY("force", KEY_FORCE),
      FUSE_OPT_KEY("--precache", KEY_PRE_CACHE),
      FUSE_OPT_KEY("precache", KEY_PRE_CACHE),
      FUSE_OPT_KEY("--memcache", KEY_MEM_CACHE),
      FUSE_OPT_KEY("memcache", KEY_MEM_CACHE),
      FUSE_OPT_KEY("--nocache", KEY_NO_CACHE),
      FUSE_OPT_KEY("nocache", KEY_NO_CACHE),
      FUSE_OPT_KEY("nomerge", KEY_NO_MERGE),
      FUSE_OPT_KEY("notrim", KEY_NO_TRIM),
      FUSE_OPT_KEY("nospecials", KEY_NO_SPECIALS),
      FUSE_OPT_KEY("nosymlinks", KEY_NO_SYMLINKS),
      FUSE_OPT_KEY("nohardlinks", KEY_NO_HARD_LINKS),
      FUSE_OPT_KEY("default_permissions", KEY_DEFAULT_PERMISSIONS),
#if FUSE_USE_VERSION >= 30
      FUSE_OPT_KEY("direct_io", KEY_DIRECT_IO),
#endif
      {"--cache=%s", offsetof(Param, cache_dir)},
      {"encoding=%s", offsetof(Param, opts.encoding)},
      {"dmask=%o", offsetof(Param, dmask)},
      {"fmask=%o", offsetof(Param, fmask)},
      FUSE_OPT_END,
  };

  if (fuse_opt_parse(&args, &param, opts, ProcessArg)) {
    return EXIT_FAILURE;
  }

  DataNode::dmask = param.dmask & 0777;
  DataNode::fmask = param.fmask & 0777;

  // No ZIP archive paths.
  if (param.paths.empty()) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  std::string mount_point;
  if (param.paths.size() > 1) {
    mount_point = std::move(param.paths.back());
    param.paths.pop_back();
  }

  assert(!param.paths.empty());

  // Resolve path of cache dir if provided.
  if (param.cache_dir) {
    char buffer[PATH_MAX + 1];
    const char* const p = realpath(param.cache_dir, buffer);
    if (!p) {
      ThrowSystemError("Cannot use cache dir ", Path(param.cache_dir));
    }

    Reader::SetCacheDir(p);
  }

  // Open and index the ZIP archives.
  if (param.paths.size() == 1) {
    LOG(DEBUG) << "Indexing " << Path(param.paths.front()) << "...";
  } else {
    LOG(DEBUG) << "Indexing " << Path(param.paths.front()) << " and "
               << (param.paths.size() - 1) << " other archives...";
  }

  Timer const timer;
  std::unique_ptr tree = std::make_unique<Tree>(param.paths, param.opts);
  g_tree = tree.get();
#ifdef NDEBUG
  // For optimization, don't bother destructing the tree.
  tree.release();
#endif
  LOG(DEBUG) << "Indexed in " << timer;

  if (!mount_point.empty()) {
    // Try to create the mount point directory if it doesn't exist.
    if (mkdirat(cleanup.dirfd, mount_point.c_str(), 0777) == 0) {
      LOG(DEBUG) << "Created mount point " << Path(mount_point);
      cleanup.mount_point = mount_point;
    } else if (errno == EEXIST) {
      LOG(DEBUG) << "Mount point " << Path(mount_point) << " already exists";
    } else {
      PLOG(ERROR) << "Cannot create mount point " << Path(mount_point);
    }
  } else {
    assert(!param.paths.empty());
    mount_point = Path(param.paths.front()).Split().second.WithoutExtension();
    const auto n = mount_point.size();

    for (int i = 0;;) {
      if (mkdirat(cleanup.dirfd, mount_point.c_str(), 0777) == 0) {
        LOG(INFO) << "Created mount point " << Path(mount_point);
        cleanup.mount_point = mount_point;
        break;
      }

      if (errno != EEXIST) {
        PLOG(ERROR) << "Cannot create mount point " << Path(mount_point);
        return EXIT_FAILURE;
      }

      LOG(DEBUG) << "Mount point " << Path(mount_point) << " already exists";
      mount_point.resize(n);
      mount_point += StrCat(" (", ++i, ")");
    }
  }

  fuse_opt_add_arg(&args, mount_point.c_str());

#if FUSE_USE_VERSION < 30
  // Respect inode numbers.
  fuse_opt_add_arg(&args, "-ouse_ino");
#endif

  // Read-only mounting.
  fuse_opt_add_arg(&args, "-r");

  // Single-threaded operation.
  fuse_opt_add_arg(&args, "-s");

  return fuse_main(args.argc, args.argv, &operations, nullptr);
} catch (const ZipError& e) {
  LOG(ERROR) << e.what();
  // Shift libzip error codes in order to avoid collision with FUSE errors.
  const int ZIP_ER_BASE = 10;
  return ZIP_ER_BASE + e.code();
} catch (const std::exception& e) {
  LOG(ERROR) << e.what();
  return EXIT_FAILURE;
}

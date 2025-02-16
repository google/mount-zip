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
#define PROGRAM_VERSION "1.7"

#define FUSE_USE_VERSION 27

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
  std::cerr << R"(Mounts a ZIP archive as a FUSE filesystem

Usage: )" PROGRAM_NAME
               R"( [options] <archive_file> [mount_point]

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
    -o nospecials          no special files (FIFOs, sockets, devices)
    -o nosymlinks          no symbolic links
    -o nohardlinks         no hard links

)";
}

// Parameters for command-line argument processing function.
struct Param {
  // Number of string arguments
  int str_arg_count = 0;
  // ZIP file name
  std::string filename;
  // Mount point
  std::string mount_point;
  // Cache dir
  char* cache_dir = nullptr;
  // Access mask for directories.
  unsigned int dmask = 0022;
  // Access mask for files.
  unsigned int fmask = 0022;
  // Conversion options.
  Tree::Options opts;

  ~Param() {
    if (cache_dir)
      free(cache_dir);
  }
};

// FUSE operations
struct Operations : fuse_operations {
 private:
  // Converts a C++ exception into a negative error code.
  // Also logs the error.
  // Must be called from within a catch block.
  static int ToError(std::string_view action, Path path) {
    try {
      throw;
    } catch (const std::bad_alloc&) {
      LOG(ERROR) << "Cannot " << action << ' ' << path << ": No memory";
      return -ENOMEM;
    } catch (const std::exception& e) {
      LOG(ERROR) << "Cannot " << action << ' ' << path << ": " << e.what();
      return -EIO;
    } catch (...) {
      LOG(ERROR) << "Cannot " << action << ' ' << path << ": Unexpected error";
      return -EIO;
    }
  }

  static const FileNode* GetNode(std::string_view const fname) {
    Tree* const tree = static_cast<Tree*>(fuse_get_context()->private_data);
    assert(tree);
    const FileNode* const node = tree->Find(fname);
    if (!node)
      LOG(DEBUG) << "Cannot find " << Path(fname);
    return node;
  }

  static int GetAttr(const char* const path, struct stat* const st) try {
    assert(path);
    assert(st);

    const FileNode* const node = GetNode(path);
    if (!node)
      return -ENOENT;

    *st = *node;
    return 0;
  } catch (...) {
    return ToError("stat", path);
  }

  static int ReadDir(const char* const path,
                     void* const buf,
                     fuse_fill_dir_t const filler,
                     [[maybe_unused]] off_t const offset,
                     [[maybe_unused]] fuse_file_info* const fi) try {
    assert(path);
    assert(filler);

    const FileNode* const node = GetNode(path);
    if (!node)
      return -ENOENT;

    const auto add = [buf, filler](const char* const name,
                                   const struct stat* const st) {
      if (filler(buf, name, st, 0)) {
        throw std::bad_alloc();
      }
    };

    struct stat st = *node;
    add(".", &st);

    if (const FileNode* const parent = node->parent) {
      st = *parent;
      add("..", &st);
    } else {
      add("..", nullptr);
    }

    for (const FileNode& child : node->children) {
      st = child;
      add(child.name.c_str(), &st);
    }

    return 0;
  } catch (...) {
    return ToError("read dir", path);
  }

  static int Open(const char* const path, fuse_file_info* const fi) try {
    assert(path);
    assert(fi);

    const FileNode* const node = GetNode(path);
    if (!node)
      return -ENOENT;

    if (node->is_dir())
      return -EISDIR;

    Reader::Ptr reader = node->GetReader();
    fi->fh = reinterpret_cast<uint64_t>(reader.release());
    return 0;
  } catch (...) {
    return ToError("open", path);
  }

  static int Read(const char* const path,
                  char* const buf,
                  size_t const size,
                  off_t const offset,
                  fuse_file_info* const fi) try {
    assert(path);
    assert(buf);
    assert(size > 0);
    assert(fi);

    if (offset < 0)
      return -EINVAL;

    Reader* const r = reinterpret_cast<Reader*>(fi->fh);
    assert(r);
    return static_cast<int>(
        r->Read(buf,
                buf + std::min<size_t>(size, std::numeric_limits<int>::max()),
                offset) -
        buf);
  } catch (...) {
    return ToError("read", path);
  }

  static int Release([[maybe_unused]] const char* const path,
                     fuse_file_info* const fi) {
    assert(fi);
    Reader* const r = reinterpret_cast<Reader*>(fi->fh);
    assert(r);
    const Reader::Ptr to_delete(r);
    return 0;
  }

  static int ReadLink(const char* const path,
                      char* const buf,
                      size_t const size) try {
    assert(path);
    assert(buf);
    assert(size > 1);

    const FileNode* const node = GetNode(path);
    if (!node)
      return -ENOENT;

    if (node->type() != FileType::Symlink)
      return -EINVAL;

    const Reader::Ptr reader = node->GetReader();
    char* const end = reader->Read(buf, buf + size - 1, 0);
    *end = '\0';
    return 0;
  } catch (...) {
    return ToError("read link", path);
  }

  static int StatFs([[maybe_unused]] const char* const path,
                    struct statvfs* const st) {
    assert(st);
    const Tree* const tree =
        static_cast<const Tree*>(fuse_get_context()->private_data);
    assert(tree);
    st->f_bsize = Tree::block_size;
    st->f_frsize = Tree::block_size;
    st->f_blocks = tree->GetBlockCount();
    st->f_bfree = 0;
    st->f_bavail = 0;
    st->f_files = tree->GetNodeCount();
    st->f_ffree = 0;
    st->f_favail = 0;
    st->f_flag = ST_RDONLY;
    st->f_namemax = NAME_MAX;
    return 0;
  }

 public:
  Operations()
      : fuse_operations{
            .getattr = GetAttr,
            .readlink = ReadLink,
            .open = Open,
            .read = Read,
            .statfs = StatFs,
            .release = Release,
            .readdir = ReadDir,
        } {}
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
  KEY_NO_CACHE,
  KEY_NO_SPECIALS,
  KEY_NO_SYMLINKS,
  KEY_NO_HARDLINKS,
  KEY_DEFAULT_PERMISSIONS,
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
      fuse_opt_add_arg(outargs, "-ho");
      fuse_main(outargs->argc, outargs->argv, &operations, nullptr);
      std::exit(EXIT_SUCCESS);

    case KEY_VERSION:
      std::cerr << PROGRAM_NAME " version: " PROGRAM_VERSION "\n"
                << "libzip version: " LIBZIP_VERSION "\n";
      fuse_opt_add_arg(outargs, "--version");
      fuse_main(outargs->argc, outargs->argv, &operations, nullptr);
      std::exit(EXIT_SUCCESS);

    case FUSE_OPT_KEY_NONOPT:
      switch (++param.str_arg_count) {
        case 1:
          // zip file name
          param.filename = arg;
          return DISCARD;

        case 2:
          // mountpoint
          param.mount_point = arg;
          // keep it and then pass to FUSE initializer
          return KEEP;

        default:
          LOG(ERROR) << "Only two arguments allowed: filename and mountpoint";
          return ERROR;
      }

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

    case KEY_NO_CACHE:
      Reader::SetCacheStrategy(CacheStrategy::NoCache);
      return DISCARD;

    case KEY_NO_SPECIALS:
      param.opts.include_special_files = false;
      return DISCARD;

    case KEY_NO_SYMLINKS:
      param.opts.include_symlinks = false;
      return DISCARD;

    case KEY_NO_HARDLINKS:
      param.opts.include_hardlinks = false;
      return DISCARD;

    case KEY_DEFAULT_PERMISSIONS:
      DataNode::original_permissions = true;
      return KEEP;

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

    if (args)
      fuse_opt_free_args(args);

    if (close(dirfd) < 0)
      PLOG(ERROR) << "Cannot close file descriptor";
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
      FUSE_OPT_KEY("nospecials", KEY_NO_SPECIALS),
      FUSE_OPT_KEY("nosymlinks", KEY_NO_SYMLINKS),
      FUSE_OPT_KEY("nohardlinks", KEY_NO_HARDLINKS),
      FUSE_OPT_KEY("default_permissions", KEY_DEFAULT_PERMISSIONS),
      {"--cache=%s", offsetof(Param, cache_dir)},
      {"encoding=%s", offsetof(Param, opts.encoding)},
      {"dmask=%o", offsetof(Param, dmask)},
      {"fmask=%o", offsetof(Param, fmask)},
      FUSE_OPT_END,
  };

  if (fuse_opt_parse(&args, &param, opts, ProcessArg))
    return EXIT_FAILURE;

  DataNode::dmask = param.dmask & 0777;
  DataNode::fmask = param.fmask & 0777;

  // No ZIP archive name.
  if (param.filename.empty()) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  // Resolve path of cache dir if provided.
  if (param.cache_dir) {
    char buffer[PATH_MAX + 1];
    const char* const p = realpath(param.cache_dir, buffer);
    if (!p)
      ThrowSystemError("Cannot use cache dir ", Path(param.cache_dir));

    Reader::SetCacheDir(p);
  }

  // Open and index the ZIP archive.
  LOG(DEBUG) << "Indexing " << Path(param.filename) << "...";
  Timer timer;
  Tree::Ptr tree_ptr = Tree::Init(param.filename.c_str(), param.opts);
  Tree& tree = *tree_ptr;
#ifdef NDEBUG
  // For optimization, don't bother destructing the tree.
  tree_ptr.release();
#endif
  LOG(DEBUG) << "Indexed " << Path(param.filename) << " in " << timer;

  if (!param.mount_point.empty()) {
    // Try to create the mount point directory if it doesn't exist.
    if (mkdirat(cleanup.dirfd, param.mount_point.c_str(), 0777) == 0) {
      LOG(DEBUG) << "Created mount point " << Path(param.mount_point);
      cleanup.mount_point = param.mount_point;
    } else if (errno == EEXIST) {
      LOG(DEBUG) << "Mount point " << Path(param.mount_point)
                 << " already exists";
    } else {
      PLOG(ERROR) << "Cannot create mount point " << Path(param.mount_point);
    }
  } else {
    param.mount_point = Path(param.filename).Split().second.WithoutExtension();
    const auto n = param.mount_point.size();

    for (int i = 0;;) {
      if (mkdirat(cleanup.dirfd, param.mount_point.c_str(), 0777) == 0) {
        LOG(INFO) << "Created mount point " << Path(param.mount_point);
        cleanup.mount_point = param.mount_point;
        fuse_opt_add_arg(&args, param.mount_point.c_str());
        break;
      }

      if (errno != EEXIST) {
        PLOG(ERROR) << "Cannot create mount point " << Path(param.mount_point);
        return EXIT_FAILURE;
      }

      LOG(DEBUG) << "Mount point " << Path(param.mount_point)
                 << " already exists";
      param.mount_point.resize(n);
      param.mount_point += StrCat(" (", ++i, ")");
    }
  }

  // Respect inode numbers.
  fuse_opt_add_arg(&args, "-ouse_ino");

  // Read-only mounting.
  fuse_opt_add_arg(&args, "-r");

  // Single-threaded operation.
  fuse_opt_add_arg(&args, "-s");

  return fuse_main(args.argc, args.argv, &operations, &tree);
} catch (const ZipError& e) {
  LOG(ERROR) << e.what();
  // Shift libzip error codes in order to avoid collision with FUSE errors.
  const int ZIP_ER_BASE = 10;
  return ZIP_ER_BASE + e.code();
} catch (const std::exception& e) {
  LOG(ERROR) << e.what();
  return EXIT_FAILURE;
}

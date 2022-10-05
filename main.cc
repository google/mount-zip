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

#include "config.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <limits>
#include <locale>
#include <new>
#include <string_view>

#include <fuse.h>
#include <fuse_opt.h>
#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

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
void print_usage() {
  fprintf(stderr,
          R"(Mounts a ZIP archive as a FUSE filesystem

Usage: %s [options] <ZIP-file> [mount-point]

General options:
    --help    -h           print help
    --version              print version
    --quiet   -q           print fewer log messages
    --verbose              print more log messages
    --redact               redact file names from log messages
    --force                mount ZIP even if password is wrong or missing, or
                           if the encryption or compression method is unsupported
    --encoding=CHARSET     original encoding of file names
    --cache=DIR            cache dir (default is /tmp)
    --nocache              no caching of uncompressed data
    -o nospecials          no special files (FIFOs, sockets, devices)
    -o nosymlinks          no symbolic links
    -o nohardlinks         no hard links
)",
          PROGRAM);
}

// Prints version information.
void print_version() {
  fprintf(stderr, "%s version: %s\n", PROGRAM, VERSION);
  fprintf(stderr, "libzip version: %s\n", LIBZIP_VERSION);
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
      Log(LOG_ERR, "Cannot ", action, ' ', path, ": No memory");
      return -ENOMEM;
    } catch (const std::exception& e) {
      Log(LOG_ERR, "Cannot ", action, ' ', path, ": ", e.what());
      return -EIO;
    } catch (...) {
      Log(LOG_ERR, "Cannot ", action, ' ', path, ": Unexpected error");
      return -EIO;
    }
  }

  static const FileNode* GetNode(std::string_view fname) {
    Tree* const tree = static_cast<Tree*>(fuse_get_context()->private_data);
    assert(tree);
    const FileNode* const node = tree->Find(fname);
    if (!node)
      Log(LOG_DEBUG, "Cannot find ", Path(fname));
    return node;
  }

  static int GetAttr(const char* path, struct stat* st) try {
    const FileNode* const node = GetNode(path);
    if (!node)
      return -ENOENT;

    *st = *node;
    return 0;
  } catch (...) {
    return ToError("stat", path);
  }

  static int ReadDir(const char* path,
                     void* buf,
                     fuse_fill_dir_t filler,
                     [[maybe_unused]] off_t offset,
                     [[maybe_unused]] fuse_file_info* fi) try {
    const FileNode* const node = GetNode(path);
    if (!node)
      return -ENOENT;

    {
      const struct stat st = *node;
      filler(buf, ".", &st, 0);
    }

    if (const FileNode* const parent = node->parent) {
      const struct stat st = *parent;
      filler(buf, "..", &st, 0);
    } else {
      filler(buf, "..", nullptr, 0);
    }

    for (const FileNode& child : node->children) {
      const struct stat st = child;
      filler(buf, child.name.c_str(), &st, 0);
    }

    return 0;
  } catch (...) {
    return ToError("read dir", path);
  }

  static int Open(const char* path, fuse_file_info* fi) try {
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

  static int Read(const char* path,
                  char* buf,
                  size_t size,
                  off_t offset,
                  fuse_file_info* fi) try {
    if (offset < 0)
      return -EINVAL;

    return static_cast<int>(
        reinterpret_cast<Reader*>(fi->fh)->Read(
            buf, buf + std::min<size_t>(size, std::numeric_limits<int>::max()),
            offset) -
        buf);
  } catch (...) {
    return ToError("read", path);
  }

  static int Release([[maybe_unused]] const char* path, fuse_file_info* fi) {
    const Reader::Ptr p(reinterpret_cast<Reader*>(fi->fh));
    return 0;
  }

  static int ReadLink(const char* path, char* buf, size_t size) try {
    const FileNode* const node = GetNode(path);
    if (!node)
      return -ENOENT;

    if (node->type() != FileType::Symlink)
      return -EINVAL;

    const Reader::Ptr reader = node->GetReader();
    buf = reader->Read(buf, buf + size - 1, 0);
    *buf = '\0';
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
    return 0;
  }

 public:
  Operations() : fuse_operations {
    .getattr = GetAttr, .readlink = ReadLink, .open = Open, .read = Read,
    .statfs = StatFs, .release = Release, .readdir = ReadDir,
#if FUSE_VERSION >= 28
    .flag_nullpath_ok = 0,  // Don't allow null path
#endif
#if FUSE_VERSION == 29
        .flag_utime_omit_ok = 1,
#endif
  }
  {}
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
  KEY_NO_SPECIALS,
  KEY_NO_SYMLINKS,
  KEY_NO_HARDLINKS,
  KEY_NO_CACHE,
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
      print_usage();
      fuse_opt_add_arg(outargs, "-ho");
      fuse_main(outargs->argc, outargs->argv, &operations, nullptr);
      std::exit(EXIT_SUCCESS);

    case KEY_VERSION:
      print_version();
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
          fprintf(stderr,
                  "%s: only two arguments allowed: filename and mountpoint\n",
                  PROGRAM);
          return ERROR;
      }

    case KEY_QUIET:
      setlogmask(LOG_UPTO(LOG_ERR));
      return DISCARD;

    case KEY_VERBOSE:
      setlogmask(LOG_UPTO(LOG_DEBUG));
      return DISCARD;

    case KEY_REDACT:
      Path::redact = true;
      return DISCARD;

    case KEY_FORCE:
      param.opts.check_password = false;
      param.opts.check_compression = false;
      return DISCARD;

    case KEY_NO_CACHE:
      Reader::may_cache_ = false;
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
        Log(LOG_DEBUG, "Removed mount point ", Path(mount_point));
      } else {
        Log(LOG_ERR, "Cannot remove mount point ", Path(mount_point), ": ",
            strerror(errno));
      }
    }

    if (args)
      fuse_opt_free_args(args);

    if (close(dirfd) < 0)
      Log(LOG_ERR, "Cannot close file descriptor: ", strerror(errno));
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
  openlog(PROGRAM, LOG_PERROR, LOG_USER);
  setlogmask(LOG_UPTO(LOG_INFO));

  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  Cleanup cleanup{.args = &args};
  Param param;

  const fuse_opt opts[] = {FUSE_OPT_KEY("--help", KEY_HELP),
                           FUSE_OPT_KEY("-h", KEY_HELP),
                           FUSE_OPT_KEY("--version", KEY_VERSION),
                           FUSE_OPT_KEY("--quiet", KEY_QUIET),
                           FUSE_OPT_KEY("-q", KEY_QUIET),
                           FUSE_OPT_KEY("--verbose", KEY_VERBOSE),
                           FUSE_OPT_KEY("-v", KEY_VERBOSE),
                           FUSE_OPT_KEY("--redact", KEY_REDACT),
                           FUSE_OPT_KEY("--force", KEY_FORCE),
                           FUSE_OPT_KEY("--nocache", KEY_NO_CACHE),
                           FUSE_OPT_KEY("nospecials", KEY_NO_SPECIALS),
                           FUSE_OPT_KEY("nosymlinks", KEY_NO_SYMLINKS),
                           FUSE_OPT_KEY("nohardlinks", KEY_NO_HARDLINKS),
                           {"--cache=%s", offsetof(Param, cache_dir), 0},
                           {"encoding=%s", offsetof(Param, opts.encoding), 0},
                           {nullptr, 0, 0}};

  if (fuse_opt_parse(&args, &param, opts, ProcessArg))
    return EXIT_FAILURE;

  // No ZIP archive name.
  if (param.filename.empty()) {
    print_usage();
    return EXIT_FAILURE;
  }

  // Resolve path of cache dir if provided.
  if (param.cache_dir) {
    char buffer[PATH_MAX + 1];
    const char* const p = realpath(param.cache_dir, buffer);
    if (!p)
      ThrowSystemError("Cannot use cache dir ", Path(param.cache_dir));

    Reader::cache_dir_ = p;
    Log(LOG_DEBUG, "Using cache dir ", Path(Reader::cache_dir_));
  }

  // Open and index the ZIP archive.
  Log(LOG_DEBUG, "Indexing ", Path(param.filename), "...");
  Timer timer;
  const Tree::Ptr tree = Tree::Init(param.filename.c_str(), param.opts);
  Log(LOG_DEBUG, "Indexed ", Path(param.filename), " in ", timer);

  if (!param.mount_point.empty()) {
    // Try to create the mount point directory if it doesn't exist.
    if (mkdirat(cleanup.dirfd, param.mount_point.c_str(), 0777) == 0) {
      Log(LOG_DEBUG, "Created mount point ", Path(param.mount_point));
      cleanup.mount_point = param.mount_point;
    } else if (errno == EEXIST) {
      Log(LOG_DEBUG, "Mount point ", Path(param.mount_point),
          " already exists");
    } else {
      Log(LOG_ERR, "Cannot create mount point ", Path(param.mount_point), ": ",
          strerror(errno));
    }
  } else {
    param.mount_point = Path(param.filename).WithoutExtension();
    const auto n = param.mount_point.size();

    for (int i = 0;;) {
      if (mkdirat(cleanup.dirfd, param.mount_point.c_str(), 0777) == 0) {
        Log(LOG_INFO, "Created mount point ", Path(param.mount_point));
        cleanup.mount_point = param.mount_point;
        fuse_opt_add_arg(&args, param.mount_point.c_str());
        break;
      }

      if (errno != EEXIST) {
        Log(LOG_ERR, "Cannot create mount point ", Path(param.mount_point),
            ": ", strerror(errno));
        return EXIT_FAILURE;
      }

      Log(LOG_DEBUG, "Mount point ", Path(param.mount_point),
          " already exists");
      param.mount_point.resize(n);
      param.mount_point += StrCat(" (", ++i, ")");
    }
  }

  // Respect inodes number.
  fuse_opt_add_arg(&args, "-ouse_ino");

  // Read-only mounting.
  fuse_opt_add_arg(&args, "-r");

  // Single-threaded operation.
  fuse_opt_add_arg(&args, "-s");

  return fuse_main(args.argc, args.argv, &operations, tree.get());
} catch (const ZipError& e) {
  Log(LOG_ERR, e.what());
  // Shift libzip error codes in order to avoid collision with FUSE errors.
  const int ZIP_ER_BASE = 10;
  return ZIP_ER_BASE + e.code();
} catch (const std::exception& e) {
  Log(LOG_ERR, e.what());
  return EXIT_FAILURE;
}

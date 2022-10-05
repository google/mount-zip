// Copyright 2021 Google LLC
// Copyright 2008-2020 Alexander Galanin <al@galanin.nnov.ru>
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

#include "tree.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include <termios.h>
#include <unicode/putil.h>
#include <unicode/uclean.h>
#include <unicode/ucnv.h>
#include <unicode/ucsdet.h>
#include <unicode/udata.h>
#include <unistd.h>
#include <zip.h>

#include "error.h"
#include "extra_field.h"
#include "log.h"
#include "scoped_file.h"

enum CompressionMethod : int;

std::ostream& operator<<(std::ostream& out, const CompressionMethod cm) {
  out << "ZIP_CM_";

  switch (cm) {
#define PRINT(s)   \
  case ZIP_CM_##s: \
    return out << #s;

    PRINT(STORE)
    PRINT(SHRINK)
    PRINT(REDUCE_1)
    PRINT(REDUCE_2)
    PRINT(REDUCE_3)
    PRINT(REDUCE_4)
    PRINT(IMPLODE)
    PRINT(DEFLATE)
    PRINT(DEFLATE64)
    PRINT(PKWARE_IMPLODE)
    PRINT(BZIP2)
    PRINT(LZMA)
    PRINT(TERSE)
    PRINT(LZ77)
    PRINT(LZMA2)
    PRINT(XZ)
    PRINT(JPEG)
    PRINT(WAVPACK)
    PRINT(PPMD)
#undef PRINT
  }

  return out << static_cast<int>(cm);
}

enum EncryptionMethod : int;

std::ostream& operator<<(std::ostream& out, const EncryptionMethod em) {
  out << "ZIP_EM_";

  switch (em) {
#define PRINT(s)   \
  case ZIP_EM_##s: \
    return out << #s;

    PRINT(NONE)
    PRINT(TRAD_PKWARE)
    PRINT(AES_128)
    PRINT(AES_192)
    PRINT(AES_256)
    PRINT(UNKNOWN)
#undef PRINT
  }

  return out << static_cast<int>(em);
}

// Temporarily suppresses the echo on the terminal.
// Used when waiting for password to be typed.
class SuppressEcho {
 public:
  explicit SuppressEcho() {
    if (tcgetattr(STDIN_FILENO, &tattr_) < 0)
      return;

    reset_ = true;

    struct termios tattr = tattr_;
    tattr.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
  }

  ~SuppressEcho() {
    if (reset_)
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr_);
  }

  explicit operator bool() const { return reset_; }

 private:
  struct termios tattr_;
  bool reset_ = false;
};

bool Tree::ReadPasswordFromStdIn() {
  std::string password;

  {
    const SuppressEcho guard;
    if (guard)
      std::cout << "Password > " << std::flush;

    // Read password from standard input.
    if (!std::getline(std::cin, password))
      password.clear();

    if (guard)
      std::cout << "Got it!" << std::endl;
  }

  // Remove newline at the end of password.
  while (password.ends_with('\n'))
    password.pop_back();

  if (password.empty()) {
    Log(LOG_DEBUG, "Got an empty password");
    return false;
  }

  Log(LOG_DEBUG, "Got a password of ", password.size(), " bytes");

  if (zip_set_default_password(zip_, password.c_str()) < 0)
    throw ZipError("Cannot set password", zip_);

  return true;
}

namespace {

// Initializes and cleans up the ICU library.
class IcuGuard {
 public:
  // Initializes the ICU library with the given ICU data file.
  // Throws an runtime_error in case of error.
  IcuGuard(const char* const data_file) : mapped_file_(data_file) {
    UErrorCode error = U_ZERO_ERROR;
    udata_setCommonData(mapped_file_.data(), &error);
    udata_setFileAccess(UDATA_ONLY_PACKAGES, &error);
    u_init(&error);
    if (U_FAILURE(error))
      throw std::runtime_error(
          StrCat("Cannot initialize ICU: ", u_errorName(error)));
  }

  // Cleans up the ICU library.
  ~IcuGuard() { u_cleanup(); }

  // No copy.
  IcuGuard(const IcuGuard&) = delete;
  IcuGuard& operator=(const IcuGuard&) = delete;

 private:
  // Memory-mapped ICU data file.
  const FileMapping mapped_file_;
};

struct Closer {
  void operator()(UConverter* const conv) const { ucnv_close(conv); }
  void operator()(UCharsetDetector* const csd) const { ucsdet_close(csd); }
};

using ConverterPtr = std::unique_ptr<UConverter, Closer>;
using CharsetDetectorPtr = std::unique_ptr<UCharsetDetector, Closer>;

class ConverterToUtf8 {
 public:
  // Creates a converter that will convert strings from the given encoding to
  // UTF-8. Allocates internal buffers to handle strings up to
  // `maxInputLength`. Throws an exception in case of error.
  ConverterToUtf8(const char* const fromEncoding, const size_t maxInputLength)
      : from(Open(fromEncoding)), to(Open("UTF-8")) {
    utf16.resize(2 * maxInputLength + 1);
    utf8.resize(3 * maxInputLength + 1);
  }

  // Converts the given string to UTF-8. Returns a string_view to the internal
  // buffer holding the null-terminated result. Returns an empty string_view in
  // case of error.
  std::string_view operator()(const std::string_view in) {
    UErrorCode error = U_ZERO_ERROR;
    const int32_t len16 = ucnv_toUChars(from.get(), utf16.data(), utf16.size(),
                                        in.data(), in.size(), &error);

    if (U_FAILURE(error)) {
      Log(LOG_ERR, "Cannot convert to UTF-16: ", u_errorName(error));
      return {};
    }

    const int32_t len8 = ucnv_fromUChars(to.get(), utf8.data(), utf8.size(),
                                         utf16.data(), len16, &error);

    if (U_FAILURE(error)) {
      Log(LOG_ERR, "Cannot convert to UTF-8: ", u_errorName(error));
      return {};
    }

    assert(!utf8[len8]);
    return std::string_view(utf8.data(), len8);
  }

 private:
  // Opens an ICU converter for the given encoding.
  // Throws a runtime_error in case of error.
  static ConverterPtr Open(const char* const encoding) {
    UErrorCode error = U_ZERO_ERROR;
    ConverterPtr conv(ucnv_open(encoding, &error));

    if (U_FAILURE(error))
      throw std::runtime_error(StrCat("Cannot open converter for encoding '",
                                      encoding, "': ", u_errorName(error)));

    assert(conv);
    return conv;
  }

  const ConverterPtr from, to;
  std::vector<UChar> utf16;
  std::vector<char> utf8;
};

// Detects the encoding of the given string.
// Returns the encoding name, or an empty string in case of error.
std::string DetectEncoding(const std::string_view bytes) {
  UErrorCode error = U_ZERO_ERROR;
  const CharsetDetectorPtr csd(ucsdet_open(&error));
  ucsdet_setText(csd.get(), bytes.data(), static_cast<int32_t>(bytes.size()),
                 &error);

  // Get most plausible encoding.
  const UCharsetMatch* const ucm = ucsdet_detect(csd.get(), &error);
  const char* const encoding = ucsdet_getName(ucm, &error);
  if (U_FAILURE(error)) {
    Log(LOG_ERR, "Cannot detect encoding: ", u_errorName(error));
    return std::string();
  }

  Log(LOG_DEBUG, "Detected encoding ", encoding, " with ",
      ucsdet_getConfidence(ucm, &error), "% confidence");

  // Check if we want to convert the detected encoding via ICU.
  const std::string_view candidates[] = {
      "Shift_JIS",   "Big5",        "EUC-JP",      "EUC-KR", "GB18030",
      "ISO-2022-CN", "ISO-2022-JP", "ISO-2022-KR", "KOI8-R"};

  for (const std::string_view candidate : candidates) {
    if (candidate == encoding)
      return encoding;
  }

  // Not handled by ICU.
  return std::string();
}

}  // namespace

Tree::~Tree() {
#ifndef NDEBUG
  files_by_original_path_.clear();

  for (FileNode& node : files_by_path_)
    node.children.clear();
#endif

  files_by_path_.clear_and_dispose(std::default_delete<FileNode>());

  if (zip_close(zip_) != 0)
    Log(LOG_ERR, "Error while closing archive: ", zip_strerror(zip_));
}

size_t Tree::GetBucketCount(zip_t* zip) {
  size_t n = std::min<zip_int64_t>(zip_get_num_entries(zip, 0),
                                   std::numeric_limits<ssize_t>::max());
  // Floor the number of elements to a power of 2 with a minimum of 16.
  n |= 16;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  if constexpr (sizeof(n) > 4) {
    n |= n >> 32;
  }
  n ^= n >> 1;
  Log(LOG_DEBUG, "Allocating ", n, " buckets");
  return n;
}

void Tree::BuildTree() {
  const zip_int64_t n = zip_get_num_entries(zip_, 0);

  FileNode::Ptr root(new FileNode{
      .zip = zip_, .data = {.nlink = 2, .mode = S_IFDIR | 0755}, .name = "/"});
  assert(!root->parent);
  [[maybe_unused]] const auto [pos, ok] = files_by_path_.insert(*root);
  assert(ok);
  root.release();  // Now owned by |files_by_path_|.

  // Concatenate all the names in a buffer in order to guess the encoding.
  std::string allNames;
  allNames.reserve(10000);

  size_t maxNameLength = 0;

  // search for absolute or parent-relative paths
  for (zip_int64_t id = 0; id < n; ++id) {
    const char* const p = zip_get_name(zip_, id, ZIP_FL_ENC_RAW);
    if (!p)
      continue;

    const std::string_view name = p;
    if (maxNameLength < name.size())
      maxNameLength = name.size();

    if (allNames.size() + name.size() <= allNames.capacity())
      allNames.append(name);

    if (!need_prefix_)
      need_prefix_ = name.starts_with('/') || name.starts_with("../");
  }

  // Detect filename encoding.
  std::string encoding;
  if (opts_.encoding)
    encoding = opts_.encoding;
  if (encoding.empty() || encoding == "auto")
    encoding = DetectEncoding(allNames);
  allNames.clear();

  // Prepare functor to convert filenames to UTF-8.
  // By default, just rely on the conversion to UTF-8 provided by libzip.
  std::function<std::string_view(std::string_view)> toUtf8 =
      [](std::string_view s) { return s; };
  zip_flags_t zipFlags = ZIP_FL_ENC_GUESS;

  // But if the filename encoding is one of the encodings we want to convert
  // using ICU, prepare and use the ICU converter.
  if (!encoding.empty() && encoding != "libzip") {
    try {
      if (encoding != "raw")
        toUtf8 = [converter = std::make_shared<ConverterToUtf8>(
                      encoding.c_str(), maxNameLength)](std::string_view s) {
          return (*converter)(s);
        };
      zipFlags = ZIP_FL_ENC_RAW;
    } catch (const std::exception& e) {
      Log(LOG_ERR, e.what());
    }
  }

  zip_stat_t sb;

  struct Hardlink {
    zip_int64_t id;
    mode_t mode;
  };

  std::vector<Hardlink> hardlinks;
  std::string path;
  Beat should_display_progress;

  // Add zip entries for all items except hardlinks
  for (zip_int64_t id = 0; id < n; ++id) {
    if (should_display_progress)
      Log(LOG_INFO, "Loading ", 100 * id / n, "%");

    if (zip_stat_index(zip_, id, zipFlags, &sb) < 0)
      throw ZipError(StrCat("Cannot read entry #", id), zip_);

    if ((sb.valid & ZIP_STAT_NAME) == 0 || !sb.name || !*sb.name) {
      Log(LOG_ERR, "Skipped entry [", id, "]: No name");
      continue;
    }

    const Path original_path = sb.name;
    const auto [mode, is_hardlink] = GetEntryAttributes(id, original_path);
    const FileType type = GetFileType(mode);

    const Path original_path_utf8 = toUtf8(original_path);
    if (!Path::Normalize(&path, original_path_utf8, need_prefix_)) {
      Log(LOG_ERR, "Skipped ", type, " [", id, "]: Cannot normalize path ",
          original_path_utf8);
      continue;
    }

    if (type == FileType::Directory) {
      FileNode* const node = CreateDir(path);
      assert(node->link == &node->data);
      const nlink_t nlink = node->data.nlink;
      assert(nlink >= 2);
      node->data = DataNode::Make(zip_, id, mode);
      node->data.nlink = nlink;
      node->original_path = Path(original_path).WithoutTrailingSeparator();
      files_by_original_path_.insert(*node);
      total_block_count_ += 1;
      continue;
    }

    if (type != FileType::File &&
        (type == FileType::Symlink ? !opts_.include_symlinks
                                   : !opts_.include_special_files)) {
      Log(LOG_INFO, "Skipped ", type, " [", id, "] ", Path(path));
      continue;
    }

    if (is_hardlink) {
      if (opts_.include_hardlinks) {
        hardlinks.push_back({id, mode});
      } else {
        Log(LOG_INFO, "Skipped ", type, " [", id, "] ", Path(path));
      }
      continue;
    }

    const auto [parent_path, name] = Path(path).Split();
    FileNode* const parent = CreateDir(parent_path);
    FileNode* const node = CreateFile(id, parent, name, mode);
    assert(node->parent == parent);
    parent->AddChild(node);
    node->original_path = original_path;
    files_by_original_path_.insert(*node);
    total_block_count_ += 1;
    total_block_count_ += node->operator DataNode::Stat().st_blocks;

    if (!zip_encryption_method_supported(sb.encryption_method, 1)) {
      ZipError e(StrCat("Cannot decrypt ", *node, ": ",
                        EncryptionMethod(sb.encryption_method)),
                 ZIP_ER_ENCRNOTSUPP);
      if (opts_.check_compression)
        throw std::move(e);
      Log(LOG_ERR, e.what());
    }

    if (!zip_compression_method_supported(sb.comp_method, 1)) {
      ZipError e(StrCat("Cannot decompress ", *node, ": ",
                        CompressionMethod(sb.comp_method)),
                 ZIP_ER_COMPNOTSUPP);
      if (opts_.check_compression)
        throw std::move(e);
      Log(LOG_ERR, e.what());
    }

    // Check the password on encrypted files.
    if ((sb.valid & ZIP_STAT_ENCRYPTION_METHOD) != 0 &&
        sb.encryption_method != ZIP_EM_NONE)
      CheckPassword(node);
  }

  // Add hardlinks
  for (const auto [id, mode] : hardlinks) {
    const Path original_path = zip_get_name(zip_, id, zipFlags);
    const FileType type = GetFileType(mode);

    const Path original_path_utf8 = toUtf8(original_path);
    if (!Path::Normalize(&path, original_path_utf8, need_prefix_)) {
      Log(LOG_ERR, "Skipped ", type, " [", id, "]: Cannot normalize path ",
          original_path_utf8);
      continue;
    }

    const auto [parent_path, name] = Path(path).Split();
    FileNode* const parent = CreateDir(parent_path);
    FileNode* node = CreateHardlink(id, parent, name, mode);
    assert(node->parent == parent);
    parent->AddChild(node);
    node->original_path = original_path;
    files_by_original_path_.insert(*node);
    total_block_count_ += 1;
  }

  if (should_display_progress.Count())
    Log(LOG_INFO, "Loaded 100%");

  Log(LOG_DEBUG, "Nodes = ", files_by_path_.size());
  Log(LOG_DEBUG, "Blocks = ", total_block_count_);
}

void Tree::CheckPassword(const FileNode* const node) {
  assert(node);

  if (checked_password_)
    return;

  Log(LOG_INFO, "Need password for ", *node);
  ReadPasswordFromStdIn();

  try {
    Log(LOG_DEBUG, "Checking password on ", *node, "...");

    // Try to open the file and read a few bytes from it.
    const ZipFile file(zip_fopen_index(zip_, node->id, 0));
    if (!file)
      throw ZipError(StrCat("Cannot open ", *node), zip_);

    std::array<char, 16> buf;
    if (zip_fread(file.get(), buf.data(), buf.size()) < 0)
      throw ZipError(StrCat("Cannot read ", *node), file.get());

    Log(LOG_INFO, "Password is Ok");
  } catch (const ZipError& error) {
    if (opts_.check_password) {
      Log(LOG_INFO,
          "Use the --force option to mount an encrypted ZIP with a wrong "
          "password");
      throw;
    }

    Log(LOG_DEBUG, error.what());
    Log(LOG_INFO,
        "Continuing despite wrong password because of --force option");
  }

  checked_password_ = true;
}

Tree::EntryAttributes Tree::GetEntryAttributes(
    const zip_uint64_t id,
    const std::string_view original_path) {
  const bool is_dir = original_path.ends_with('/');

  zip_uint8_t opsys;
  zip_uint32_t attr;
  zip_file_get_external_attributes(zip_, id, 0, &opsys, &attr);

  mode_t unix_mode = attr >> 16;
  mode_t mode;
  bool is_hardlink = false;

  /*
   * PKWARE describes "OS made by" now (since 1998) as follows:
   * The upper byte indicates the compatibility of the file attribute
   * information. If the external file attributes are compatible with MS-DOS
   * and can be read by PKZIP for DOS version 2.04g then this value will be
   * zero.
   */
  if (opsys == ZIP_OPSYS_DOS && GetFileType(unix_mode) != FileType::Unknown)
    opsys = ZIP_OPSYS_UNIX;

  const zip_uint32_t FZ_ATTR_HARDLINK = 0x800;

  switch (opsys) {
    case ZIP_OPSYS_UNIX:
      mode = unix_mode;

      // force is_dir value
      if (is_dir) {
        SetFileType(&mode, FileType::Directory);
      } else {
        switch (GetFileType(mode)) {
          case FileType::Unknown:    // treat unknown file types as regular
          case FileType::Directory:  // mislabeled as directory
            SetFileType(&mode, FileType::File);
            break;
          default:
            break;
        }
      }

      // Always ignore hardlink flag for dirs
      is_hardlink = (attr & FZ_ATTR_HARDLINK) != 0 && !is_dir;
      break;

    case ZIP_OPSYS_DOS:
    case ZIP_OPSYS_WINDOWS_NTFS:
    case ZIP_OPSYS_MVS:
      /*
       * Both WINDOWS_NTFS and OPSYS_MVS used here because of
       * difference in constant assignment by PKWARE and Info-ZIP
       */
      mode = 0444;
      // http://msdn.microsoft.com/en-us/library/windows/desktop/gg258117%28v=vs.85%29.aspx
      // http://en.wikipedia.org/wiki/File_Allocation_Table#attributes
      // FILE_ATTRIBUTE_READONLY
      if ((attr & 1) == 0) {
        mode |= 0220;
      }
      // directory
      if (is_dir) {
        mode |= S_IFDIR | 0111;
      } else {
        mode |= S_IFREG;
      }

      break;

    default:
      if (is_dir) {
        mode = S_IFDIR | 0775;
      } else {
        mode = S_IFREG | 0664;
      }
  }

  return {mode, is_hardlink};
}

FileNode* Tree::Attach(FileNode::Ptr node) {
  assert(node);
  const auto [pos, ok] = files_by_path_.insert(*node);
  if (ok)
    return node.release();  // Now owned by |files_by_path_|.

  Log(LOG_DEBUG, *node, " conflicts with ", *pos);

  // Find extension start
  std::string& f = node->name;
  std::string::size_type e = f.find_last_of('.');
  if (e <= 0 || e >= f.size() - 1)
    e = f.size();

  const std::string ext(f, e);

  // Add a number before the extension
  int& i = pos->collision_count;
  while (true) {
    f.resize(e);
    f += " (";
    f += std::to_string(++i);
    f += ")";
    f += ext;

    const auto [pos, ok] = files_by_path_.insert(*node);
    if (ok) {
      Log(LOG_DEBUG, "Resolved conflict for ", *node);
      return node.release();  // Now owned by |files_by_path_|.
    }

    Log(LOG_DEBUG, *node, " conflicts with ", *pos);
  }
}

FileNode* Tree::CreateFile(zip_int64_t id,
                           FileNode* parent,
                           std::string_view name,
                           mode_t mode) {
  assert(parent);
  assert(!name.empty());
  assert(id >= 0);
  return Attach(
      FileNode::Ptr(new FileNode{.zip = zip_,
                                 .id = id,
                                 .data = DataNode::Make(zip_, id, mode),
                                 .parent = parent,
                                 .name = std::string(name)}));
}

FileNode* Tree::CreateHardlink(zip_int64_t id,
                               FileNode* parent,
                               std::string_view name,
                               mode_t mode) {
  assert(parent);
  assert(!name.empty());
  assert(id >= 0);

  FileNode::Ptr node(new FileNode{
      .zip = zip_, .id = id, .parent = parent, .name = std::string(name)});

  zip_uint16_t len;
  const zip_uint8_t* field = zip_file_extra_field_get_by_id(
      zip_, id, FZ_EF_PKWARE_UNIX, 0, &len, ZIP_FL_CENTRAL);

  if (!field)
    field = zip_file_extra_field_get_by_id(zip_, id, FZ_EF_PKWARE_UNIX, 0, &len,
                                           ZIP_FL_LOCAL);

  if (!field) {
    // Ignoring hardlink without PKWARE UNIX field
    Log(LOG_INFO, "Cannot find PkWare Unix field for hardlink ", *node);
    return CreateFile(id, parent, name, mode);
  }

  time_t mt, at;
  uid_t uid;
  gid_t gid;
  dev_t dev;
  const char* link;
  uint16_t link_len;

  if (!ExtraField::parsePkWareUnixField(len, field, mode, mt, at, uid, gid, dev,
                                        link, link_len)) {
    Log(LOG_WARNING, "Cannot parse PkWare Unix field for hardlink ", *node);
    return CreateFile(id, parent, name, mode);
  }

  if (link_len == 0 || !link) {
    Log(LOG_ERR, "Cannot get target for hardlink ", *node);
    return CreateFile(id, parent, name, mode);
  }

  const std::string_view target_path(link, link_len);

  const auto it = files_by_original_path_.find(
      Path(target_path).WithoutTrailingSeparator());
  if (it == files_by_original_path_.end()) {
    Log(LOG_ERR, "Cannot find target for hardlink ", *node, " -> ",
        Path(target_path));
    return CreateFile(id, parent, name, mode);
  }

  const FileNode& target = *it;

  if (target.type() != GetFileType(mode)) {
    // PkZip saves hard-link flag for symlinks with inode link count > 1.
    if (!S_ISLNK(mode))
      Log(LOG_ERR, "Mismatched types for hardlink ", *node, " -> ", target);

    return CreateFile(id, parent, name, mode);
  }

  node->link = target.link;
  node->link->nlink++;

  Log(LOG_DEBUG, "Created hardlink ", *node, " -> ", target);
  return Attach(std::move(node));
}

FileNode* Tree::Find(std::string_view path) {
  const auto it = files_by_path_.find(Path(path).WithoutTrailingSeparator(),
                                      files_by_path_.hash_function(),
                                      files_by_path_.key_eq());
  return it == files_by_path_.end() ? nullptr : &*it;
}

FileNode* Tree::CreateDir(std::string_view path) {
  const auto [parent_path, name] = Path(path).Split();
  FileNode::Ptr to_rename;
  FileNode* parent;

  if (FileNode* const node = Find(path)) {
    if (node->is_dir())
      return node;

    // There is an existing node with the given name, but it's not a
    // directory.
    Log(LOG_DEBUG, "Found conflicting ", *node, " while creating Dir ",
        Path(path));
    parent = node->parent;

    // Remove it from |files_by_path_|, in order to insert it again later with a
    // different name.
    files_by_path_.erase(files_by_path_.iterator_to(*node));
    to_rename.reset(node);
  } else {
    parent = CreateDir(parent_path);
  }

  assert(parent);
  FileNode::Ptr child(new FileNode{.zip = zip_,
                                   .data = {.nlink = 2, .mode = S_IFDIR | 0755},
                                   .parent = parent,
                                   .name = std::string(name)});
  assert(child->path() == path);
  parent->AddChild(child.get());
  [[maybe_unused]] const auto [pos, ok] = files_by_path_.insert(*child);
  assert(ok);
  parent->link->nlink++;

  if (to_rename)
    Attach(std::move(to_rename));

  return child.release();  // Now owned by |files_by_path_|.
}

Tree::Ptr Tree::Init(const char* const filename, Options opts) {
  assert(filename);
  int err;
  zip_t* const zip_file = zip_open(filename, ZIP_RDONLY, &err);

  if (!zip_file)
    throw ZipError(StrCat("Cannot open ZIP archive ", Path(filename)), err);

  Ptr tree(new Tree(zip_file, std::move(opts)));
  tree->BuildTree();
  return tree;
}

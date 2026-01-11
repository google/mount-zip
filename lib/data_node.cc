// Copyright 2021 Google LLC
// Copyright 2019 Alexander Galanin <al@galanin.nnov.ru>
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

#include <cassert>
#include <ctime>
#include <memory>

#include <zip.h>

#include "data_node.h"
#include "error.h"
#include "extra_field.h"
#include "file_node.h"

std::ostream& operator<<(std::ostream& out, const FileType t) {
  switch (t) {
    case FileType::BlockDevice:
      return out << "Block Device";
    case FileType::CharDevice:
      return out << "Character Device";
    case FileType::Directory:
      return out << "Directory";
    case FileType::Fifo:
      return out << "FIFO";
    case FileType::File:
      return out << "File";
    case FileType::Socket:
      return out << "Socket";
    case FileType::Symlink:
      return out << "Symlink";
    default:
      return out << "Unknown";
  }
}

const timespec DataNode::g_now = {.tv_sec = time(nullptr)};
const uid_t DataNode::g_uid = getuid();
const gid_t DataNode::g_gid = getgid();
mode_t DataNode::fmask = 0022;
mode_t DataNode::dmask = 0022;
bool DataNode::original_permissions = false;

ino_t DataNode::ino_count = 0;

DataNode DataNode::Make(zip_t* const zip, const i64 id, const mode_t mode) {
  assert(zip);
  zip_stat_t st;
  if (zip_stat_index(zip, id, 0, &st) < 0) {
    throw ZipError("Cannot stat file", zip);
  }

  // check that all used fields are valid
  [[maybe_unused]] const zip_uint64_t need_valid =
      ZIP_STAT_NAME | ZIP_STAT_INDEX | ZIP_STAT_SIZE | ZIP_STAT_MTIME;
  // required fields are always valid for existing items or newly added
  // directories (see zip_stat_index.c from libzip)
  assert((st.valid & need_valid) == need_valid);

  DataNode node{
      .id = id, .mode = mode, .size = st.size, .mtime = {.tv_sec = st.mtime}};

  bool has_pkware_field = false;

  // Gets timestamp, owner and group information from extra fields.
  // Process the extra fields in this precise order because of the following
  // precedences:
  // - Times from timestamp have precedence over the UNIX field times.
  // - High-precision NTFS timestamps precedence over the UNIX timestamps.
  // - UIDs and GIDs from UNIX fields with bigger field IDs have higher
  // - precedence.
  using enum FieldId;
  for (FieldId const field_id :
       {PKWARE_UNIX, INFOZIP_UNIX_1, INFOZIP_UNIX_2, INFOZIP_UNIX_3,
        UNIX_TIMESTAMP, NTFS_TIMESTAMP}) {
    zip_flags_t const flags = ZIP_FL_CENTRAL | ZIP_FL_LOCAL;
    zip_uint16_t const n = zip_file_extra_fields_count_by_id(
        zip, node.id, static_cast<zip_uint16_t>(field_id), flags);
    for (zip_uint16_t i = 0; i < n; ++i) {
      zip_uint16_t field_size;
      const auto* field_data = zip_file_extra_field_get_by_id(
          zip, node.id, static_cast<zip_uint16_t>(field_id), i, &field_size,
          flags);

      ExtraFields f;
      if (!field_data || field_size == 0 ||
          !f.Parse(field_id, Bytes(field_data, field_size), mode)) {
        continue;
      }

      if (f.mtime.tv_sec != -1) {
        node.mtime = f.mtime;
      }

      if (f.atime.tv_sec != -1) {
        node.atime = f.atime;
      }

      if (f.ctime.tv_sec != -1) {
        node.ctime = f.ctime;
      }

      if (f.uid != -1) {
        node.uid = f.uid;
      }

      if (f.gid != -1) {
        node.gid = f.gid;
      }

      if (f.dev != -1) {
        node.dev = f.dev;
      }

      // Use PKWARE link target only if link target in Info-ZIP format is not
      // specified (empty file content).
      if (!f.link_target.empty()) {
        node.target = std::string(f.link_target);
        if (S_ISLNK(mode) && node.size == 0) {
          node.size = f.link_target.size();
        }
      }

      if (field_id == PKWARE_UNIX) {
        has_pkware_field = true;
      }
    }
  }

  // InfoZIP may produce FIFO-marked node with content, PkZip - can't.
  if (S_ISFIFO(mode) && (node.size != 0 || !has_pkware_field)) {
    SetFileType(&node.mode, FileType::File);
  }

  return node;
}

DataNode::operator Stat() const {
  Stat st = {};
  st.st_ino = ino;
  st.st_nlink = nlink;
  st.st_blksize = block_size;
  st.st_blocks = (size + block_size - 1) / block_size;
  st.st_size = size;
  st.st_rdev = dev;

#if __APPLE__
  st.st_atimespec = atime;
  st.st_mtimespec = mtime;
  st.st_ctimespec = ctime;
#else
  st.st_atim = atime;
  st.st_mtim = mtime;
  st.st_ctim = ctime;
#endif

  if (original_permissions) {
    st.st_uid = uid;
    st.st_gid = gid;
    st.st_mode = mode;
  } else {
    st.st_uid = g_uid;
    st.st_gid = g_gid;
    const FileType ft = GetFileType(mode);
    switch (ft) {
      case FileType::Directory:
        st.st_mode = static_cast<mode_t>(S_IFDIR | (0777 & ~dmask));
        break;

      case FileType::Symlink:
        st.st_mode = static_cast<mode_t>(S_IFLNK | 0777);
        break;

      default:
        st.st_mode = 0666;
        if (const mode_t xbits = 0111; (mode & xbits) != 0) {
          st.st_mode |= xbits;
        }
        st.st_mode &= ~fmask;
        SetFileType(&st.st_mode, ft);
    }
  }

  return st;
}

bool DataNode::CacheAll(zip_t* const zip,
                        const FileNode& file_node,
                        std::function<void(ssize_t)> progress) {
  assert(!cached_reader);
  if (size == 0) {
    LOG(DEBUG) << "No need to cache " << file_node << ": Empty file";
    return false;
  }

  ZipFile file = Reader::Open(zip, id);
  assert(file);

#if LIBZIP_VERSION_MAJOR > 1 ||      \
    LIBZIP_VERSION_MAJOR == 1 &&     \
        (LIBZIP_VERSION_MINOR > 9 || \
         LIBZIP_VERSION_MINOR == 9 && LIBZIP_VERSION_MICRO >= 1)
  // For libzip >= 1.9.1
  const bool seekable = zip_file_is_seekable(file.get()) > 0;
#else
  // For libzip < 1.9.1
  zip_stat_t st;
  const bool seekable = zip_stat_index(zip, id, 0, &st) == 0 &&
                        (st.valid & ZIP_STAT_COMP_METHOD) != 0 &&
                        st.comp_method == ZIP_CM_STORE &&
                        (st.valid & ZIP_STAT_ENCRYPTION_METHOD) != 0 &&
                        st.encryption_method == ZIP_EM_NONE;
#endif

  if (seekable) {
    LOG(DEBUG) << "No need to cache " << file_node << ": File is seekable";
    return false;
  }

  cached_reader = CacheFile(std::move(file), id, size, std::move(progress));
  return true;
}

Reader::Ptr DataNode::GetReader(zip_t* const zip,
                                const FileNode& file_node) const {
  if (cached_reader) {
    LOG(DEBUG) << *cached_reader << ": Reusing Cached " << *cached_reader
               << " for " << file_node;
    return cached_reader->AddRef();
  }

  if (!target.empty()) {
    return Reader::Ptr(new StringReader(target));
  }

  ZipFile file = Reader::Open(zip, id);
  assert(file);

#if LIBZIP_VERSION_MAJOR > 1 ||      \
    LIBZIP_VERSION_MAJOR == 1 &&     \
        (LIBZIP_VERSION_MINOR > 9 || \
         LIBZIP_VERSION_MINOR == 9 && LIBZIP_VERSION_MICRO >= 1)
  // For libzip >= 1.9.1
  const bool seekable = zip_file_is_seekable(file.get()) > 0;
#else
  // For libzip < 1.9.1
  zip_stat_t st;
  const bool seekable = zip_stat_index(zip, id, 0, &st) == 0 &&
                        (st.valid & ZIP_STAT_COMP_METHOD) != 0 &&
                        st.comp_method == ZIP_CM_STORE &&
                        (st.valid & ZIP_STAT_ENCRYPTION_METHOD) != 0 &&
                        st.encryption_method == ZIP_EM_NONE;
#endif

  Reader::Ptr reader(seekable ? new UnbufferedReader(std::move(file), id, size)
                              : new BufferedReader(zip, std::move(file), id,
                                                   size, &cached_reader));

  LOG(DEBUG) << *reader << ": Opened " << file_node
             << ", seekable = " << seekable;
  return reader;
}

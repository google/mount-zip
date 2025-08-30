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

// Gets timestamp information from extra fields.
// Gets owner and group information.
static bool ProcessExtraFields(DataNode* const node, zip_t* const zip) {
  assert(node);
  assert(zip);

  // times from timestamp have precedence to UNIX field times
  bool mtime_from_timestamp = false;
  bool atime_from_timestamp = false;
  // high-precision timestamps have the highest precedence
  bool high_precision_time = false;
  bool has_pkware_field = false;
  // UIDs and GIDs from UNIX extra fields with bigger type IDs have precedence
  FieldId highest_field_id = FieldId();

  for (zip_flags_t const flags : {ZIP_FL_CENTRAL, ZIP_FL_LOCAL}) {
    zip_int16_t const n = zip_file_extra_fields_count(zip, node->id, flags);
    for (zip_int16_t i = 0; i < n; ++i) {
      zip_uint16_t field_type, field_size;
      const zip_uint8_t* const field_data = zip_file_extra_field_get(
          zip, node->id, i, &field_type, &field_size, flags);
      Bytes const b(field_data, field_size);
      FieldId const field_id = FieldId(field_type);

      switch (field_id) {
        case FieldId::UNIX_TIMESTAMP: {
          ExtraFields f;
          if (high_precision_time || !f.Parse(field_id, b)) {
            break;
          }

          if (f.mtime.tv_sec != -1) {
            node->mtime = f.mtime;
            mtime_from_timestamp = true;
          }

          if (f.atime.tv_sec != -1) {
            node->atime = f.atime;
            atime_from_timestamp = true;
          }

          break;
        }

        case FieldId::PKWARE_UNIX: {
          ExtraFields f;
          if (!f.Parse(field_id, b, node->mode)) {
            break;
          }

          has_pkware_field = true;
          assert(f.uid != -1);
          assert(f.gid != -1);

          if (field_id >= highest_field_id) {
            node->uid = f.uid;
            node->gid = f.gid;
            highest_field_id = field_id;
          }

          if (!high_precision_time) {
            if (!mtime_from_timestamp) {
              node->mtime = f.mtime;
            }

            if (!atime_from_timestamp) {
              node->atime = f.atime;
            }
          }

          if (f.dev != -1) {
            node->dev = f.dev;
          }

          // Use PKWARE link target only if link target in Info-ZIP format is
          // not specified (empty file content)
          if (S_ISLNK(node->mode) && node->size == 0 &&
              !f.link_target.empty()) {
            assert(link);
            node->target.assign(f.link_target);
            node->size = f.link_target.size();
          }

          break;
        }

        case FieldId::INFOZIP_UNIX_1:
        case FieldId::INFOZIP_UNIX_2:
        case FieldId::INFOZIP_UNIX_3: {
          ExtraFields f;
          if (!f.Parse(field_id, b)) {
            break;
          }

          if (f.uid != -1 && f.gid != -1 && field_id >= highest_field_id) {
            node->uid = f.uid;
            node->gid = f.gid;
            highest_field_id = field_id;
          }

          if (high_precision_time) {
            break;
          }

          if (!mtime_from_timestamp) {
            node->mtime = f.mtime;
          }

          if (!atime_from_timestamp) {
            node->atime = f.atime;
          }

          break;
        }

        case FieldId::NTFS_TIMESTAMP: {
          ExtraFields f;
          if (!f.Parse(field_id, b)) {
            break;
          }

          if (f.mtime.tv_sec != -1) {
            node->mtime = f.mtime;
          }
          if (f.atime.tv_sec != -1) {
            node->atime = f.atime;
          }
          if (f.ctime.tv_sec != -1) {
            node->ctime = f.ctime;
          }
          high_precision_time = true;
          break;
        }
      }
    }
  }

  return has_pkware_field;
}

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
  const bool has_pkware_field = ProcessExtraFields(&node, zip);

  // InfoZIP may produce FIFO-marked node with content, PkZip - can't.
  if (S_ISFIFO(node.mode) && (node.size != 0 || !has_pkware_field)) {
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

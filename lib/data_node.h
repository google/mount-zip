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

#ifndef DATA_NODE_H
#define DATA_NODE_H

#include <cassert>
#include <ostream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include "reader.h"

enum class FileType : mode_t {
  Unknown = 0,            // Unknown
  BlockDevice = S_IFBLK,  // Block-oriented device
  CharDevice = S_IFCHR,   // Character-oriented device
  Directory = S_IFDIR,    // Directory
  Fifo = S_IFIFO,         // FIFO or pipe
  File = S_IFREG,         // Regular file
  Socket = S_IFSOCK,      // Socket
  Symlink = S_IFLNK,      // Symbolic link
};

inline FileType GetFileType(const mode_t mode) {
  return FileType(mode & S_IFMT);
}

inline void SetFileType(mode_t* const mode, const FileType type) {
  assert(mode);
  *mode &= ~static_cast<mode_t>(S_IFMT);
  *mode |= static_cast<mode_t>(type);
}

std::ostream& operator<<(std::ostream& out, FileType t);

struct FileNode;

// Represents an inode.
struct DataNode {
  static const uid_t g_uid;
  static const gid_t g_gid;
  static mode_t fmask;
  static mode_t dmask;
  static bool original_permissions;

  static ino_t ino_count;
  ino_t ino = ++ino_count;
  mutable nlink_t nlink = 1;
  i64 id = -1;
  mode_t mode = 0;
  uid_t uid = g_uid;
  gid_t gid = g_gid;
  dev_t dev = 0;
  zip_uint64_t size = 0;
  timespec mtime = Now();
  timespec atime = mtime;
  timespec ctime = mtime;
  std::string target;  // Link target
  mutable Reader::Ptr cached_reader;
  static const blksize_t block_size = 512;

  // Get attributes.
  using Stat = struct stat;
  operator Stat() const;

  bool CacheAll(zip_t* zip,
                const FileNode& file_node,
                std::function<void(ssize_t)> progress = {});

  Reader::Ptr GetReader(zip_t* zip, const FileNode& file_node) const;

  static DataNode Make(zip_t* zip, i64 id, mode_t mode);

  static timespec Now();
};

#endif

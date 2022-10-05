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

inline FileType GetFileType(mode_t mode) {
  return FileType(mode & S_IFMT);
}

inline void SetFileType(mode_t* mode, FileType type) {
  assert(mode);
  *mode &= ~static_cast<mode_t>(S_IFMT);
  *mode |= static_cast<mode_t>(type);
}

std::ostream& operator<<(std::ostream& out, FileType t);

struct FileNode;

// Represents an inode.
struct DataNode {
  static ino_t ino_count;
  ino_t ino = ++ino_count;
  mutable nlink_t nlink = 1;
  zip_int64_t id = -1;
  mode_t mode = 0;
  uid_t uid = 0;
  gid_t gid = 0;
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
  operator Stat() const {
    Stat st = {};
    st.st_ino = ino;
    st.st_nlink = nlink;
    st.st_mode = mode;
    st.st_blksize = block_size;
    st.st_blocks = (size + block_size - 1) / block_size;
    st.st_size = size;
#if __APPLE__
    st.st_atimespec = atime;
    st.st_mtimespec = mtime;
    st.st_ctimespec = ctime;
#else
    st.st_atim = atime;
    st.st_mtim = mtime;
    st.st_ctim = ctime;
#endif
    st.st_uid = uid;
    st.st_gid = gid;
    st.st_rdev = dev;
    return st;
  }

  Reader::Ptr GetReader(zip_t* zip, const FileNode& file_node) const;

  static DataNode Make(zip_t* zip, zip_int64_t id, mode_t mode);

  static timespec Now();
};

#endif

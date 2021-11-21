// Copyright 2021 Google LLC
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

#ifndef SCOPED_FILE_H
#define SCOPED_FILE_H

#include <utility>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

// A scoped file descriptor.
class ScopedFile {
 public:
  ~ScopedFile() {
    if (IsValid() && close(fd_) < 0)
      Log(LOG_ERR, "Error while closing file descriptor ", fd_, ": ",
          strerror(errno));
  }

  explicit ScopedFile(int fd) noexcept : fd_(fd) {}
  ScopedFile(ScopedFile&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

  ScopedFile& operator=(ScopedFile other) noexcept {
    std::swap(fd_, other.fd_);
    return *this;
  }

  bool IsValid() const { return fd_ >= 0; }
  int GetDescriptor() const { return fd_; }

 private:
  int fd_;
};

#endif  // SCOPED_FILE_H

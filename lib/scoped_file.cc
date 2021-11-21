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

#include "scoped_file.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "error.h"
#include "log.h"
#include "path.h"

ScopedFile::~ScopedFile() {
  if (IsValid() && close(fd_) < 0)
    Log(LOG_ERR, "Error while closing file descriptor ", fd_, ": ",
        strerror(errno));
}

// Removes the memory mapping.
FileMapping::~FileMapping() {
  if (munmap(data_, size_) < 0)
    Log(LOG_ERR, "Cannot unmap file: ", strerror(errno));
}

// Maps a file to memory in read-only mode.
// Throws a system_error in case of error.
FileMapping::FileMapping(const char* const path) {
  // Open file in read-only mode.
  const ScopedFile file(open(path, O_RDONLY));
  if (!file.IsValid())
    ThrowSystemError("Cannot open file ", Path(path));

  const int fd = file.GetDescriptor();

  // Get file size.
  struct stat st;
  if (fstat(fd, &st) < 0)
    ThrowSystemError("Cannot fstat file ", Path(path));

  size_ = static_cast<size_t>(st.st_size);
  if (size_ != st.st_size) {
    errno = EOVERFLOW;
    ThrowSystemError("File ", Path(path), " is too big (", st.st_size,
                     " bytes) to be memory-mapped");
  }

  // Map file to memory.
  data_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_ == MAP_FAILED)
    ThrowSystemError("Cannot mmap file ", Path(path));
}

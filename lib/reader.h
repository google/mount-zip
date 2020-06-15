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

#ifndef READER_H
#define READER_H

#include <memory>
#include <ostream>
#include <string_view>

#include <zip.h>

#include "log.h"

struct ZipClose {
  void operator()(zip_file_t* const file) const { zip_fclose(file); }
};

using ZipFile = std::unique_ptr<zip_file_t, ZipClose>;

// Base abstract class for Reader objects that reads and return bytes from a
// file stored or compressed in a ZIP archive.
class Reader {
 public:
  virtual ~Reader() = default;
  Reader() = default;
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  using Ptr = std::unique_ptr<Reader>;

  // Reads |dest_end - dest| bytes at the given file |offset| and stores them
  // into |dest|. Tries to fill the |dest| buffer, and only returns a "short
  // read" with fewer than |dest_end - dest| bytes if the end of the file is
  // reached. Returns a pointer past the last byte written in |dest|, which
  // should be |dest_end| if the end of the file has not been reached. Throws
  // ZipError in case of error
  virtual char* Read(char* dest, char* dest_end, off_t offset) = 0;

  // Output operator for logging.
  friend std::ostream& operator<<(std::ostream& out, const Reader& reader) {
    return out << "Reader " << reader.reader_id_;
  }

 protected:
  // Number of created Reader objects.
  static zip_int64_t reader_count_;

  // ID of this Reader (for debug traces).
  const zip_int64_t reader_id_ = ++reader_count_;
};

// Reader taking its data from a string_view.
class StringReader : public Reader {
 public:
  explicit StringReader(std::string_view contents) : contents_(contents) {}

  char* Read(char* dest, char* dest_end, off_t offset) override {
    return offset < contents_.size()
               ? dest + contents_.copy(dest, dest_end - dest, offset)
               : dest;
  }

 private:
  std::string_view contents_;
};

// Reader used for uncompressed files, ie files that are simply stored without
// compression in the ZIP archive. These files can be accessed in random order,
// and don't require any buffering.
class UnbufferedReader : public Reader {
 public:
  ~UnbufferedReader() override { Log(LOG_DEBUG, *this, ": Closed"); }

  UnbufferedReader(zip_t* const zip,
                   const zip_int64_t file_id,
                   const off_t expected_size)
      : file_id_(file_id),
        expected_size_(expected_size),
        file_(Open(zip, file_id)) {
    Log(LOG_DEBUG, *this, ": Opened File [", file_id_, "]");
  }

  char* Read(char* dest, char* dest_end, off_t offset) override;

 protected:
  // Opens the file at index |file_id|. Throws ZipError in case of error.
  static ZipFile Open(zip_t* zip, zip_int64_t file_id);

  // Reads up to |size| bytes at the current position pos_ and stores them
  // into |dest|. Returns the number of bytes actually read, which could be
  // less than |size|. Returns 0 if |size| is 0. Returns 0 if the end of file
  // has been reached, and there is nothing left to be read. Updates the
  // current position pos_. Throws ZipError in case of error
  ssize_t ReadAtCurrentPosition(char* dest, ssize_t size);

  // ID of the file being read.
  const zip_int64_t file_id_;

  // Expected size of the file being read.
  const off_t expected_size_;

  // File being read.
  ZipFile file_;

  // Current position of the file being read.
  off_t pos_ = 0;
};

// Reader used for compressed files. It features a decompression engine and a
// rolling buffer holding the latest decompressed bytes.
//
// During the first decompression pass, the rolling buffer contains 500KB (or
// less if the |expected_size| is smaller). This is usually enough to
// accommodate the possible out-of-order read operations due to the kernel's
// readahead optimization.
//
// If a read operation starts at an offset located before the start of the
// rolling buffer, then this BufferedReader restarts decompressing the file from
// the beginning, but for this second pass it will use a rolling buffer as big
// as possible (as big as |expected_size| if there is enough addressable space).
class BufferedReader : public UnbufferedReader {
 public:
  BufferedReader(zip_t* const zip,
                 const zip_int64_t file_id,
                 const off_t expected_size)
      : UnbufferedReader(zip, file_id, expected_size), zip_(zip) {
    AllocateBuffer(500 << 10);  // 500KB
  }

  char* Read(char* dest, char* dest_end, off_t offset) override;

 protected:
  // Allocates a rolling buffer up to |buffer_size| or the |expected_size|
  // passed to the constructor, whichever is smaller.
  // Throws std::bad_alloc in case of memory allocation error.
  void AllocateBuffer(ssize_t buffer_size);

  // Allocates a bigger buffer and restarts decompressing from the beginning.
  // Throws std::bad_alloc in case of memory allocation error.
  // Throws a ZipError in case of error.
  void Restart();

  // Advances the position of the decompression engine by |jump| bytes.
  // Throws a ZipError in case of error.
  // Precondition: the buffer is allocated.
  // Precondition: |jump >= 0|
  void Advance(off_t jump);

  // Reads as many bytes as possible (up to |dest_end - dest| bytes) from the
  // rolling buffer and stores them in |dest|. If the start |offset| is not in
  // the rolling buffer, then advances the position of the decompression
  // engine (while keeping the rolling buffer up to date) to the position
  // |offset| or the end of the file, whichever comes first. Returns a pointer
  // past the last byte written in |dest|. Throws a ZipError in case of error.
  // Precondition: the buffer is allocated.
  char* ReadFromBufferAndAdvance(char* dest, char* dest_end, off_t offset);

  // Pointer to the ZIP structure. Used when starting a second decompression
  // pass.
  zip_t* const zip_;

  // Index of the rolling buffer where the oldest byte is currently stored
  // (and where the next decompressed byte at the file position |pos_| will be
  // stored).
  // Invariant: 0 <= buffer_start_ < buffer_size_ once the buffer is
  // allocated.
  ssize_t buffer_start_ = 0;

  // Size of the rolling buffer.
  // Invariant: 0 < buffer_size_ once the buffer is allocated.
  ssize_t buffer_size_ = 0;

  // Rolling buffer.
  // Invariant: buffer_ != nullptr once the buffer is allocated.
  std::unique_ptr<char[]> buffer_;
};

#endif

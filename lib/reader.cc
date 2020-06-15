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

#include "reader.h"

#include <cassert>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "error.h"

zip_int64_t Reader::reader_count_ = 0;

static void LimitSize(ssize_t* const a, off_t b) {
  if (*a > b)
    *a = static_cast<ssize_t>(b);
}

ZipFile UnbufferedReader::Open(zip_t* const zip, const zip_int64_t file_id) {
  ZipFile file(zip_fopen_index(zip, file_id, 0));
  if (!file)
    throw ZipError(StrCat("Cannot open File #", file_id), zip);
  return file;
}

ssize_t UnbufferedReader::ReadAtCurrentPosition(char* dest, ssize_t size) {
  assert(size >= 0);

  if (pos_ >= expected_size_)
    return 0;

  // Avoid reading bytes past the expected end of file.
  // This is a workaround for https://github.com/nih-at/libzip/issues/261
  //
  // TODO Remove this workaround once we use a version of libzip with
  // https://github.com/nih-at/libzip/commit/6cb36530deafc731cac277080a319d52e4233867
  LimitSize(&size, expected_size_ - pos_);

  if (size == 0)
    return 0;

  const ssize_t n = static_cast<ssize_t>(zip_fread(file_.get(), dest, size));

  if (n < 0)
    throw ZipError("Cannot read file", file_.get());

  pos_ += n;
  return n;
}

char* UnbufferedReader::Read(char* dest, char* dest_end, off_t offset) {
  if (pos_ != offset) {
    Log(LOG_DEBUG, *this, ": Jump ", offset - pos_, " from ", pos_, " to ",
        offset);

    if (zip_fseek(file_.get(), offset, SEEK_SET) < 0)
      throw ZipError("Cannot fseek file", file_.get());

    pos_ = offset;
  }

  assert(pos_ == offset);

  while (const ssize_t n = ReadAtCurrentPosition(dest, dest_end - dest)) {
    dest += n;
  }

  return dest;
}

void BufferedReader::AllocateBuffer(ssize_t buffer_size) {
  LimitSize(&buffer_size, expected_size_);

  const ssize_t min_size = 1024;
  if (buffer_size < min_size)
    buffer_size = min_size;

  if (buffer_size == buffer_size_) {
    assert(buffer_);
    // Already got a buffer of the right size.
    return;
  }

  buffer_.reset();
  buffer_size_ = 0;

  while (true) {
    // Try to allocate buffer.
    try {
      buffer_.reset(new char[buffer_size]);
      buffer_size_ = buffer_size;
      Log(LOG_DEBUG, *this, ": Allocated a ", buffer_size_ >> 10, " KB buffer");
      return;
    } catch (const std::bad_alloc& error) {
      // Probably too big.
      Log(LOG_ERR, *this, ": Cannot allocate a ", buffer_size >> 10,
          " KB buffer: ", error.what());

      // If we couldn't even allocate 1KB, we ran out of memory or of
      // addressable space. Simply propagate the error.
      if (buffer_size <= min_size)
        throw;

      // Try a smaller buffer.
      buffer_size >>= 1;
    }
  }
}

void BufferedReader::Restart() {
  Log(LOG_DEBUG, *this, ": Rewind");

  // Restart from the file beginning.
  file_ = Open(zip_, file_id_);
  pos_ = 0;
  buffer_start_ = 0;

  // Allocate a possibly bigger buffer. We have to be careful on 32-bit
  // devices, since they have a limited addressable space.
  AllocateBuffer((std::numeric_limits<ssize_t>::max() >> 1) + 1);
}

void BufferedReader::Advance(off_t jump) {
  assert(jump >= 0);

  if (jump <= 0)
    return;

  const Timer timer;
  const off_t start_pos = pos_;

  do {
    ssize_t count = buffer_size_ - buffer_start_;
    LimitSize(&count, jump);

    assert(count > 0);
    count = ReadAtCurrentPosition(&buffer_[buffer_start_], count);
    if (count == 0)
      break;

    buffer_start_ += count;
    if (buffer_start_ >= buffer_size_) {
      assert(buffer_start_ == buffer_size_);
      buffer_start_ = 0;
    }

    jump -= count;
  } while (jump > 0);

  Log(LOG_DEBUG, *this, ": Skipped ", pos_ - start_pos, " bytes from ",
      start_pos, " to ", pos_, " in ", timer);
}

char* BufferedReader::ReadFromBufferAndAdvance(char* dest,
                                               char* const dest_end,
                                               const off_t offset) {
  const off_t jump = offset - pos_;

  if (jump >= 0) {
    // Jump forwards.
    Advance(jump);
    return dest;
  }

  // Jump backwards.
  assert(jump < 0);

  if (jump + buffer_size_ < 0) {
    // The backwards jump is too big and falls outside the buffer.
    Restart();
    Advance(offset);
    return dest;
  }

  // The backwards jump is small enough to fall inside the buffer.
  assert(-jump <= buffer_size_);

  // Read data from the buffer.
  ssize_t i = buffer_start_ + jump;

  do {
    ssize_t size = -i;
    ssize_t start = i;
    if (i < 0) {
      start += buffer_size_;
    } else {
      size += buffer_start_;
    }

    LimitSize(&size, dest_end - dest);
    assert(size > 0);

    Log(LOG_DEBUG, *this, ": Read ", size, " bytes from cache position ",
        i - buffer_start_);

    std::memcpy(dest, &buffer_[start], size);
    dest += size;
    i += size;
  } while (i < buffer_start_ && dest < dest_end);

  return dest;
}

char* BufferedReader::Read(char* dest,
                           char* const dest_end,
                           const off_t offset) {
  if (dest == dest_end)
    return dest;

  // If we don't have a buffer, then we don't have enough memory.
  if (!buffer_)
    throw std::bad_alloc();

  assert(buffer_);
  assert(buffer_size_ > 0);

  // Read data from buffer if possible.
  dest = ReadFromBufferAndAdvance(dest, dest_end, offset);

  // Read data from file while keeping the rolling buffer up to date.
  while (
      const ssize_t size = ReadAtCurrentPosition(
          &buffer_[buffer_start_],
          std::min<ssize_t>(dest_end - dest, buffer_size_ - buffer_start_))) {
    memcpy(dest, &buffer_[buffer_start_], size);
    dest += size;
    buffer_start_ += size;
    if (buffer_start_ == buffer_size_)
      buffer_start_ = 0;
  }

  return dest;
}

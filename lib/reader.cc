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
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>

#include "error.h"
#include "path.h"
#include "scoped_file.h"

bool Reader::may_cache_ = true;
std::string Reader::cache_dir_ = "/tmp";
zip_int64_t Reader::reader_count_ = 0;

static void LimitSize(ssize_t* const a, off_t b) {
  if (*a > b)
    *a = static_cast<ssize_t>(b);
}

ZipFile Reader::Open(zip_t* const zip, const zip_int64_t file_id) {
  ZipFile file(zip_fopen_index(zip, file_id, 0));
  if (!file)
    throw ZipError(StrCat("Cannot open File [", file_id, "]"), zip);
  return file;
}

ssize_t UnbufferedReader::ReadAtCurrentPosition(char* dest, ssize_t size) {
  assert(size >= 0);

  if (pos_ >= expected_size_)
    return 0;

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

// Reader used for compressed files. It features a decompression engine and it
// caches the decompressed bytes in a cache file.
class CacheFileReader : public UnbufferedReader {
 public:
  CacheFileReader(zip_t* const zip,
                  const zip_int64_t file_id,
                  const off_t expected_size)
      : UnbufferedReader(Open(zip, file_id), file_id, expected_size) {}

 private:
  // Creates a new, empty and hidden cache file.
  // Throws std::system_error in case of error.
  static ScopedFile CreateCacheFile() {
    if (!may_cache_)
      throw std::runtime_error("Option --nocache is in use");

    ScopedFile file(open(cache_dir_.c_str(), O_TMPFILE | O_RDWR | O_EXCL, 0));

    if (!file.IsValid())
      ThrowSystemError("Cannot create cache file in ", Path(cache_dir_));

    Log(LOG_DEBUG, "Created cache file in ", Path(cache_dir_));
    return file;
  }

  // Gets the file descriptor to the global cache file.
  // Creates this cache file if necessary.
  // Throws std::system_error in case of error.
  static int GetCacheFile() {
    static const ScopedFile file = CreateCacheFile();
    return file.GetDescriptor();
  }

  // Reserves space in the cache file.
  // Returns the start position of the reserved space.
  off_t ReserveSpace() const {
    // Get current cache file size.
    struct stat st;
    if (fstat(cache_file_, &st) < 0)
      ThrowSystemError("Cannot stat cache file ", cache_file_);

    // Extend cache file.
    const off_t offset = st.st_size;
#if __APPLE__
    fstore_t st{.fst_flags = F_ALLOCATEALL,
                .fst_posmode = F_PEOFPOSMODE,
                .fst_offset = 0,
                .fst_length = expected_size_};
    if (fcntl(cache_file_, F_PREALLOCATE, &st) < 0 ||
        ftruncate(cache_file_, offset + expected_size_) < 0)
      ThrowSystemError("Cannot reserve ", expected_size_,
                       " bytes in cache file ", cache_file_, " at offset ",
                       offset);
#else
    if (const int err = posix_fallocate(cache_file_, offset, expected_size_)) {
      errno = err;  // posix_fallocate doesn't set errno
      ThrowSystemError("Cannot reserve ", expected_size_,
                       " bytes in cache file ", cache_file_, " at offset ",
                       offset);
    }
#endif

    Log(LOG_DEBUG, "Reserved ", expected_size_,
        " bytes in cache file at offset ", offset);
    return offset;
  }

  // Writes data to the global cache file.
  // Throws std::runtime_error in case of error.
  void WriteToCacheFile(const char* buf, ssize_t count, off_t offset) const {
    assert(buf);
    assert(count >= 0);
    assert(offset >= 0);

    while (count > 0) {
      const ssize_t n = pwrite(cache_file_, buf, count, offset);
      if (n < 0)
        ThrowSystemError("Cannot write ", count,
                         " bytes into cache file at offset ", offset);
      buf += n;
      count -= n;
      offset += n;
    }
  }

  // Ensures the decompressed data is cached at least up to the given offset.
  void EnsureCachedUpTo(const off_t offset) {
    const off_t start_pos = pos_;
    const off_t total_to_cache = offset - pos_;
    const Timer timer;
    Beat should_log_progress;

    while (pos_ < offset) {
      if (should_log_progress)
        Log(LOG_DEBUG, "Caching ", total_to_cache, " bytes... ",
            100 * (pos_ - start_pos) / total_to_cache, "%");

      const ssize_t buf_size = 64 * 1024;
      char buf[buf_size];
      const off_t store_offset = start_offset_ + pos_;
      const ssize_t n = ReadAtCurrentPosition(buf, buf_size);
      if (n == 0)
        break;

      WriteToCacheFile(buf, n, store_offset);
    }

    if (should_log_progress.Count())
      Log(LOG_DEBUG, "Cached ", pos_ - start_pos, " bytes from ", start_pos,
          " to ", pos_, " in ", timer);
  }

  char* Read(char* dest, char* const dest_end, off_t offset) override {
    if (expected_size_ <= offset)
      return dest;

    ssize_t count = dest_end - dest;
    LimitSize(&count, expected_size_ - offset);

    if (pos_ < offset)
      Log(LOG_DEBUG, *this, ": Jump ", offset - pos_, " from ", pos_, " to ",
          offset);

    EnsureCachedUpTo(offset + count);

    offset += start_offset_;
    const ssize_t n = pread(cache_file_, dest, count, offset);
    if (n < 0)
      ThrowSystemError("Cannot read ", count,
                       " bytes from cache file at offset ", offset);

    return dest + n;
  }

  // Cache file descriptor.
  const int cache_file_ = GetCacheFile();

  // Position at which the decompressed data is stored in the cache file.
  const off_t start_offset_ = ReserveSpace();
};

// Exception thrown by BufferedReader::Advance() when the decompression engine
// has to jump too far and a cached reader is to be used instead.
class TooFar : public std::exception {
 public:
  const char* what() const noexcept override { return "Too far"; }
};

void BufferedReader::Restart() {
  Log(LOG_DEBUG, *this, ": Rewind");

  // Restart from the file beginning.
  file_ = Open(zip_, file_id_);
  pos_ = 0;
  buffer_start_ = 0;
}

bool BufferedReader::CreateCachedReader() const noexcept {
  if (cached_reader_) {
    Log(LOG_DEBUG, *this, ": Switched to Cached ", *cached_reader_);
    return true;
  }

  try {
    cached_reader_.reset(new CacheFileReader(zip_, file_id_, expected_size_));
    Log(LOG_DEBUG, *this, ": Created Cached ", *cached_reader_);
    return true;
  } catch (const std::exception& e) {
    Log(LOG_ERR, *this, ": Cannot create Cached Reader: ", e.what());
    return false;
  }
}

void BufferedReader::Advance(off_t jump) {
  assert(jump >= 0);

  if (jump <= 0)
    return;

  if (jump > buffer_size_ && CreateCachedReader())
    throw TooFar();

  const off_t start_pos = pos_;
  const off_t total_to_cache = jump;
  const Timer timer;
  Beat should_log_progress;

  do {
    if (should_log_progress)
      Log(LOG_DEBUG, "Skipping ", total_to_cache, " bytes... ",
          100 * (pos_ - start_pos) / total_to_cache, "%");

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

  if (should_log_progress.Count())
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
                           const off_t offset) try {
  if (dest == dest_end)
    return dest;

  if (use_cached_reader_)
    return cached_reader_->Read(dest, dest_end, offset);

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
} catch (const TooFar&) {
  assert(cached_reader_);
  use_cached_reader_ = true;
  return cached_reader_->Read(dest, dest_end, offset);
}

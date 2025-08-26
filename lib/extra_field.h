// Copyright 2021 Google LLC
// Copyright 2014-2019 Alexander Galanin <al@galanin.nnov.ru>
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

#ifndef EXTRA_FIELD_H
#define EXTRA_FIELD_H

#include <ctime>
#include <span>
#include <string_view>

#include <sys/types.h>

// ZIP extra fields
enum FieldId {
  FZ_EF_TIMESTAMP = 0x5455,
  FZ_EF_NTFS = 0x000A,
  FZ_EF_PKWARE_UNIX = 0x000D,
  FZ_EF_INFOZIP_UNIX1 = 0x5855,
  FZ_EF_INFOZIP_UNIX2 = 0x7855,
  FZ_EF_INFOZIP_UNIXN = 0x7875,
};

struct Bytes : std::span<const std::byte> {
  using Base = std::span<const std::byte>;
  using Base::operator=;

  template <typename T, size_t N>
  Bytes(const T (&x)[N]) : Base(std::as_bytes(std::span(x))) {}

  template <typename T>
  Bytes(const T* const p, size_t n) : Base(std::as_bytes(std::span(p, n))) {}

  Bytes(const Base& x) : Base(x) {}

  void remove_prefix(size_t n) { *this = subspan(n); }
};

// 'Extended Timestamp' LOCAL extra field (0x5455).
struct ExtTimeStamp {
  time_t mtime = 0;
  time_t atime = 0;
  time_t ctime = 0;

  bool Parse(Bytes b);
};

// Info-ZIP UNIX extra field with timestamps and (maybe) UID/GID.
struct SimpleUnixField {
  time_t mtime = 0;
  time_t atime = 0;
  uid_t uid = -1;
  gid_t gid = -1;
  dev_t dev = -1;
  std::string_view link_target;

  bool Parse(FieldId id, Bytes b);
};

// PKWARE Unix Extra Field (000D).
struct PkWareField {
  time_t mtime = 0;
  time_t atime = 0;
  uid_t uid = -1;
  gid_t gid = -1;
  dev_t dev = 0;
  std::string_view link_target;

  bool Parse(Bytes b, mode_t mode);
};

// NTFS Extra Field
struct NtfsField {
  timespec mtime = {};
  timespec atime = {};
  timespec ctime = {};

  bool Parse(Bytes b);
};

#endif

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

// ZIP extra field ID.
enum class FieldId {
  UNIX_TIMESTAMP = 0x5455,
  NTFS_TIMESTAMP = 0x000A,
  PKWARE_UNIX = 0x000D,
  INFOZIP_UNIX_1 = 0x5855,
  INFOZIP_UNIX_2 = 0x7855,
  INFOZIP_UNIX_3 = 0x7875,
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

// UNIX extra fields.
struct ExtraFields {
  timespec mtime = {.tv_sec = -1};
  timespec atime = {.tv_sec = -1};
  timespec ctime = {.tv_sec = -1};
  uid_t uid = -1;
  gid_t gid = -1;
  dev_t dev = -1;
  std::string_view link_target;

  bool Parse(FieldId id, Bytes b, mode_t mode = 0);
};

#endif

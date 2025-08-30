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

#include "extra_field.h"

#include <sys/stat.h>
#if __has_include(<sys/sysmacros.h>)
#include <sys/sysmacros.h>
#endif

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "log.h"

namespace {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

template <typename T>
T letoh(T x) {
  return x;
}

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

u8 letoh(u8 x) {
  return x;
}

u16 letoh(u16 x) {
  return __builtin_bswap16(x);
}

u32 letoh(u32 x) {
  return __builtin_bswap32(x);
}

u64 letoh(u64 x) {
  return __builtin_bswap64(x);
}

#else
#error "Unexpected byte order"
#endif

template <typename T>
T Read(Bytes& b) {
  T res;
  if (b.size() < sizeof(res)) {
    throw std::out_of_range("Not enough bytes in buffer to read from");
  }
  assert(b.size() >= sizeof(res));
  std::memcpy(&res, b.data(), sizeof(res));
  b.remove_prefix(sizeof(res));
  return letoh(res);
}

template <typename T>
T ReadVariableLength(Bytes& b) {
  ssize_t const n = Read<u8>(b);
  if (b.size() < n) {
    throw std::out_of_range("Not enough bytes in buffer to read from");
  }

  Bytes p = b.first(n);
  b.remove_prefix(n);

  std::make_unsigned_t<T> res = 0;
  if (p.size() > sizeof(res)) {
    if (std::ranges::any_of(p.subspan(sizeof(res)),
                            [](std::byte c) { return c != std::byte(); })) {
      throw std::overflow_error("Too big");
    }

    p = p.first(sizeof(res));
  }

  for (size_t i = p.size(); i > 0;) {
    res <<= 8;
    res |= static_cast<u8>(p[--i]);
  }

  return res;
}

timespec ntfs2timespec(i64 const t) {
  i64 const offset = static_cast<i64>(369 * 365 + 89) * 24 * 3600 * 10'000'000;

  if (t < offset) {
    throw std::underflow_error("NTFS time stamp is too small");
  }

  const auto dm = std::div(t - offset, static_cast<i64>(10'000'000));

  if (dm.quot > std::numeric_limits<time_t>::max()) {
    throw std::overflow_error("NTFS time stamp is too big");
  }

  return {.tv_sec = static_cast<time_t>(dm.quot),
          .tv_nsec = static_cast<long int>(dm.rem) * 100};
}

}  // namespace

bool ExtraFields::Parse(FieldId const id, Bytes b, mode_t mode) try {
  switch (id) {
    case FieldId::UNIX_TIMESTAMP: {
      const u8 flags = Read<u8>(b);

      if (flags & 1) {
        mtime = {.tv_sec = Read<u32>(b)};
        if (b.empty()) {
          return true;
        }
      }

      if (flags & 2) {
        atime = {.tv_sec = Read<u32>(b)};
      }

      if (flags & 4) {
        ctime = {.tv_sec = Read<u32>(b)};
      }

      return true;
    }

    case FieldId::PKWARE_UNIX:
      atime = {.tv_sec=Read<u32>(b)};
      mtime = {.tv_sec=Read<u32>(b)};
      uid = Read<u16>(b);
      gid = Read<u16>(b);

      // variable data field
      if (S_ISBLK(mode) || S_ISCHR(mode)) {
        unsigned int const maj = Read<u32>(b);
        unsigned int const min = Read<u32>(b);
        dev = makedev(maj, min);
      } else {
        link_target =
            std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
      }

      return true;

    case FieldId::INFOZIP_UNIX_1:
      atime = {.tv_sec = Read<u32>(b)};
      mtime = {.tv_sec = Read<u32>(b)};
      [[fallthrough]];

    case FieldId::INFOZIP_UNIX_2:
      if (b.empty()) {
        return true;
      }

      uid = Read<u16>(b);
      gid = Read<u16>(b);
      return true;

    case FieldId::INFOZIP_UNIX_3:
      // Check version
      if (Read<u8>(b) != 1) {
        return false;
      }

      uid = ReadVariableLength<uid_t>(b);
      gid = ReadVariableLength<gid_t>(b);
      return true;

    case FieldId::NTFS_TIMESTAMP: {
      Read<u32>(b);  // skip 'Reserved' field

      bool has_times = false;
      while (!b.empty()) {
        u16 const tag = Read<u16>(b);
        u16 const size = Read<u16>(b);
        if (b.size() < size) {
          return false;
        }

        if (tag == 0x0001) {
          Bytes p = b.first(size);
          mtime = ntfs2timespec(Read<u64>(p));
          atime = ntfs2timespec(Read<u64>(p));
          ctime = ntfs2timespec(Read<u64>(p));
          has_times = true;
        }

        b.remove_prefix(size);
      }

      return has_times;
    }

    default:
      return false;
  }
} catch (...) {
  return false;
}

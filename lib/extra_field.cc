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
#include <span>
#include <stdexcept>

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

template <typename T, typename B>
T Read(std::span<const B>& b) {
  static_assert(sizeof(B) == 1);
  T res;
  if (b.size() < sizeof(res)) {
    throw std::out_of_range("Not enough bytes in buffer to read from");
  }
  assert(b.size() >= sizeof(res));
  std::memcpy(&res, b.data(), sizeof(res));
  b = b.subspan(sizeof(res));
  return letoh(res);
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

bool ExtraField::parseExtTimeStamp(Bytes b,
                                   bool& has_mtime,
                                   time_t& mtime,
                                   bool& has_atime,
                                   time_t& atime,
                                   bool& has_ctime,
                                   time_t& ctime) try {
  const u8 flags = Read<u8>(b);

  has_mtime = flags & 1;
  has_atime = flags & 2;
  has_ctime = flags & 4;

  if (has_mtime) {
    mtime = Read<u32>(b);
  }
  if (has_atime) {
    atime = Read<u32>(b);
  }
  if (has_ctime) {
    ctime = Read<u32>(b);
  }

  return true;
} catch (...) {
  return false;
}

bool ExtraField::parseSimpleUnixField(int const type,
                                      Bytes b,
                                      bool& hasUidGid,
                                      uid_t& uid,
                                      gid_t& gid,
                                      time_t& mtime,
                                      time_t& atime) try {
  switch (type) {
    case FZ_EF_PKWARE_UNIX:
    case FZ_EF_INFOZIP_UNIX1:
      atime = Read<u32>(b);
      mtime = Read<u32>(b);

      try {
        uid = Read<u16>(b);
        gid = Read<u16>(b);
        hasUidGid = true;
      } catch (...) {
        hasUidGid = false;
      }
      return true;

    default:
      return false;
  }
} catch (...) {
  return false;
}

bool ExtraField::parseUnixUidGidField(int const type,
                                      Bytes b,
                                      uid_t& uid,
                                      gid_t& gid) try {
  switch (type) {
    case FZ_EF_INFOZIP_UNIX2:
      uid = Read<u16>(b);
      gid = Read<u16>(b);
      return true;

    case FZ_EF_INFOZIP_UNIXN: {
      // Version
      if (Read<u8>(b) != 1) {
        // Unsupported version
        return false;
      }

      // UID
      {
        ssize_t const n = Read<u8>(b);
        if (b.size() < n) {
          return false;
        }

        std::span p = b.first(n);
        b = b.subspan(n);

        if (p.size() > sizeof(uid_t)) {
          if (std::ranges::any_of(p.subspan(sizeof(uid_t)),
                                  [](std::byte c) { return c != std::byte(); })) {
            return false;
          }

          p = p.first(sizeof(uid_t));
        }

        uid = 0;
        for (size_t i = p.size(); i > 0;) {
          uid <<= 8;
          uid |= static_cast<u8>(p[--i]);
        }
      }

      // GID
      {
        ssize_t const n = Read<u8>(b);
        if (b.size() < n) {
          return false;
        }

        std::span p = b.first(n);
        b = b.subspan(n);

        if (p.size() > sizeof(gid_t)) {
          if (std::ranges::any_of(p.subspan(sizeof(gid_t)), [](std::byte c) {
                return c != std::byte();
              })) {
            return false;
          }

          p = p.first(sizeof(gid_t));
        }

        gid = 0;
        for (size_t i = p.size(); i > 0;) {
          gid <<= 8;
          gid |= static_cast<u8>(p[--i]);
        }
      }

      return true;
    }
  }
  return false;
} catch (...) {
  return false;
}

bool ExtraField::parsePkWareUnixField(Bytes b,
                                      mode_t const mode,
                                      time_t& mtime,
                                      time_t& atime,
                                      uid_t& uid,
                                      gid_t& gid,
                                      dev_t& dev,
                                      const char*& link_target,
                                      size_t& link_target_len) try {
  atime = Read<u32>(b);
  mtime = Read<u32>(b);
  uid = Read<u16>(b);
  gid = Read<u16>(b);

  // variable data field
  dev = 0;
  link_target = NULL;
  link_target_len = 0;
  if (S_ISBLK(mode) || S_ISCHR(mode)) {
    unsigned int const maj = Read<u32>(b);
    unsigned int const min = Read<u32>(b);
    dev = makedev(maj, min);
  } else {
    link_target = reinterpret_cast<const char*>(b.data());
    link_target_len = static_cast<u16>(b.size());
  }

  return true;
} catch (...) {
  return false;
}

bool ExtraField::parseNtfsExtraField(Bytes b,
                                     timespec& mtime,
                                     timespec& atime,
                                     timespec& ctime) try {
  Read<u32>(b);  // skip 'Reserved' field

  bool hasTimes = false;
  while (!b.empty()) {
    u16 const tag = Read<u16>(b);
    u16 const size = Read<u16>(b);
    if (b.size() < size) {
      return false;
    }

    if (tag == 0x0001) {
      std::span p = b.first(size);
      mtime = ntfs2timespec(Read<u64>(p));
      atime = ntfs2timespec(Read<u64>(p));
      ctime = ntfs2timespec(Read<u64>(p));
      hasTimes = true;
    }

    b = b.subspan(size);
  }

  return hasTimes;
} catch (...) {
  return false;
}

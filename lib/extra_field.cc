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
#include <cstring>
#include <span>
#include <stdexcept>

#include "log.h"

namespace {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

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
#error "bad byte order"
#endif

template <typename T, typename B>
T Read(std::span<const B>& b) {
  static_assert(sizeof(B) == 1);
  T res;
  if (b.size() < sizeof(res)) {
    throw std::out_of_range(
        StrCat("Not enough bytes in buffer to read from: Only ", b.size(),
               " bytes when ", sizeof(res), " or more bytes are needed"));
  }
  assert(b.size() >= sizeof(res));
  std::memcpy(&res, b.data(), sizeof(res));
  b = b.subspan(sizeof(res));
  return letoh(res);
}

template <typename T>
T read(const u8*& data) {
  T res;
  std::memcpy(&res, data, sizeof(res));
  data += sizeof(res);
  return letoh(res);
}

}  // namespace

bool ExtraField::parseExtTimeStamp(u16 const len,
                                   const u8* const data,
                                   bool& has_mtime,
                                   time_t& mtime,
                                   bool& has_atime,
                                   time_t& atime,
                                   bool& has_ctime,
                                   time_t& ctime) try {
  std::span b(data, len);
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

bool ExtraField::parseSimpleUnixField(u16 const type,
                                      u16 const len,
                                      const u8* const data,
                                      bool& hasUidGid,
                                      uid_t& uid,
                                      gid_t& gid,
                                      time_t& mtime,
                                      time_t& atime) try {
  std::span b(data, len);
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

bool ExtraField::parseUnixUidGidField(u16 const type,
                                      u16 const len,
                                      const u8* const data,
                                      uid_t& uid,
                                      gid_t& gid) try {
  std::span b(data, len);
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
          if (std::ranges::any_of(p.subspan(sizeof(uid_t)), [](u8 c) { return c != 0; })) {
            return false;
          }

          p = p.first(sizeof(uid_t));
        }

        uid = 0;
        for (size_t i = p.size(); i > 0;) {
          uid <<= 8;
          uid |= p[--i];
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
          if (std::ranges::any_of(p.subspan(sizeof(gid_t)), [](u8 c) { return c != 0; })) {
            return false;
          }

          p = p.first(sizeof(gid_t));
        }

        gid = 0;
        for (size_t i = p.size(); i > 0;) {
          gid <<= 8;
          gid |= p[--i];
        }
      }

      return true;
    }
  }
  return false;
} catch (...) {
  return false;
}

#pragma pack(push, 1)
struct PkWareUnixExtraField {
  u32 atime;
  u32 mtime;
  u16 uid;
  u16 gid;
  struct {
    u32 major;
    u32 minor;
  } dev;
};
#pragma pack(pop)

bool ExtraField::parsePkWareUnixField(u16 len,
                                      const u8* data,
                                      mode_t mode,
                                      time_t& mtime,
                                      time_t& atime,
                                      uid_t& uid,
                                      gid_t& gid,
                                      dev_t& dev,
                                      const char*& link_target,
                                      u16& link_target_len) {
  const PkWareUnixExtraField* f =
      reinterpret_cast<const PkWareUnixExtraField*>(data);

  if (len < 12) {
    return false;
  }
  atime = static_cast<time_t>(letoh(f->atime));
  mtime = static_cast<time_t>(letoh(f->mtime));
  uid = letoh(f->uid);
  gid = letoh(f->gid);

  // variable data field
  dev = 0;
  link_target = NULL;
  link_target_len = 0;
  if (S_ISBLK(mode) || S_ISCHR(mode)) {
    if (len < 20) {
      return false;
    }

    unsigned int maj, min;
    maj = static_cast<unsigned int>(letoh(f->dev.major));
    min = static_cast<unsigned int>(letoh(f->dev.minor));

    dev = makedev(maj, min);
    link_target = NULL;
    link_target_len = 0;
  } else {
    link_target = reinterpret_cast<const char*>(data + 12);
    link_target_len = static_cast<u16>(len - 12);
  }

  return true;
}

inline static timespec ntfs2timespec(u64 t) {
  timespec ts;
  const u64 NTFS_TO_UNIX_OFFSET =
      static_cast<u64>(369 * 365 + 89) * 24 * 3600 * 10'000'000;
  t -= NTFS_TO_UNIX_OFFSET;
  ts.tv_sec = static_cast<time_t>(t / 10'000'000);
  ts.tv_nsec = static_cast<long int>(t % 10'000'000) * 100;
  return ts;
}

bool ExtraField::parseNtfsExtraField(u16 len,
                                     const u8* data,
                                     struct timespec& mtime,
                                     struct timespec& atime,
                                     struct timespec& ctime) {
  bool hasTimes = false;
  const u8* end = data + len;
  data += 4;  // skip 'Reserved' field

  while (data + 4 < end) {
    u16 tag = read<u16>(data);
    u16 size = read<u16>(data);
    if (data + size > end) {
      return false;
    }

    if (tag == 0x0001) {
      if (size < 24) {
        return false;
      }

      mtime = ntfs2timespec(read<u64>(data));
      atime = ntfs2timespec(read<u64>(data));
      ctime = ntfs2timespec(read<u64>(data));

      hasTimes = true;
    } else {
      data += size;
    }
  }

  return hasTimes;
}

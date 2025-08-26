// Copyright 2021 Google LLC
// Copyright 2014-2021 Alexander Galanin <al@galanin.nnov.ru>
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

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <sys/sysmacros.h>

using u8 = std::uint8_t;

/**
 * LOCAL extra field with both mtime and atime present in flags
 */
void timestamp_mtime_atime_present_local() {
  const u8 data[] = {1 | 2, 0xD4, 0x6F, 0xCE, 0x51, 0x72, 0xE3, 0xC7, 0x52};

  bool has_atime, has_mtime, has_ctime;
  time_t atime, mtime, ctime;
  assert(ExtraField::parseExtTimeStamp(data, has_mtime, mtime, has_atime, atime,
                                       has_ctime, ctime));
  assert(has_mtime);
  assert(has_atime);
  assert(!has_ctime);
  assert(mtime == 0x51CE6FD4);
  assert(atime == 0X52C7E372);
}

/**
 * LOCAL extra field with both mtime and creation time present in flags
 */
void timestamp_mtime_ctime_present_local() {
  const u8 data[] = {1 | 4, 0xD4, 0x6F, 0xCE, 0x51, 0x72, 0xE3, 0xC7, 0x52};

  bool has_atime, has_mtime, has_ctime;
  time_t atime, mtime, ctime;
  assert(ExtraField::parseExtTimeStamp(data, has_mtime, mtime, has_atime, atime,
                                       has_ctime, ctime));
  assert(has_mtime);
  assert(!has_atime);
  assert(has_ctime);
  assert(mtime == 0x51CE6FD4);
  assert(ctime == 0x52C7E372);
}

/**
 * Bad timestamp
 */
void timestamp_bad() {
  const u8 data[] = {1 | 2 | 4, 0x72, 0xE3, 0xC7, 0x52};

  bool has_atime, has_mtime, has_ctime;
  time_t atime, mtime, ctime;
  assert(!ExtraField::parseExtTimeStamp(data, has_mtime, mtime, has_atime,
                                        atime, has_ctime, ctime));
}

/**
 * Parse PKWARE Unix Extra Field - regular file
 */
void unix_pkware_regular() {
  const u8 data[] = {
      0xD4, 0x6F, 0xCE, 0x51,  // atime
      0x72, 0xE3, 0xC7, 0x52,  // mtime
      0x02, 0x01,              // UID
      0x04, 0x03               // GID
  };

  time_t atime, mtime;
  uid_t uid;
  gid_t gid;
  dev_t dev;
  const char* link;
  size_t link_len;
  assert(ExtraField::parsePkWareUnixField(data, S_IFREG | 0666, mtime, atime,
                                          uid, gid, dev, link, link_len));
  assert(atime == 0x51CE6FD4);
  assert(mtime == 0x52C7E372);
  assert(uid == 0x0102);
  assert(gid == 0x0304);
  assert(dev == 0);
  assert(link_len == 0);
}

/**
 * Parse PKWARE Unix Extra Field - block device
 */
void unix_pkware_device() {
  const u8 data[] = {
      0xC8, 0x76, 0x45, 0x5D,  // atime
      0xC8, 0x76, 0x45, 0x5D,  // mtime
      0x00, 0x00,              // UID
      0x06, 0x00,              // GID
      0x08, 0x00, 0x00, 0x00,  // major
      0x01, 0x00, 0x00, 0x00   // minor
  };

  time_t atime, mtime;
  uid_t uid;
  gid_t gid;
  dev_t dev;
  const char* link;
  size_t link_len;
  assert(ExtraField::parsePkWareUnixField(data, S_IFBLK | 0666, mtime, atime,
                                          uid, gid, dev, link, link_len));
  assert(atime == 0x5D4576C8);
  assert(mtime == 0x5D4576C8);
  assert(uid == 0x0000);
  assert(gid == 0x0006);
  assert(dev == makedev(8, 1));
  assert(link_len == 0);
}

/**
 * Parse PKWARE Unix Extra Field - symlink
 */
void unix_pkware_link() {
  const u8 data[] = {
      0xF3, 0x73, 0x49, 0x5D,                   // atime
      0xA9, 0x7B, 0x45, 0x5D,                   // mtime
      0xE8, 0x03,                               // UID
      0xE8, 0x03,                               // GID
      0x72, 0x65, 0x67, 0x75, 0x6C, 0x61, 0x72  // link target
  };

  time_t atime, mtime;
  uid_t uid;
  gid_t gid;
  dev_t dev;
  const char* link;
  size_t link_len;
  assert(ExtraField::parsePkWareUnixField(data, S_IFLNK | 0777, mtime, atime,
                                          uid, gid, dev, link, link_len));
  assert(atime == 0x5D4973F3);
  assert(mtime == 0x5D457BA9);
  assert(uid == 1000);
  assert(gid == 1000);
  assert(dev == 0);
  assert(link_len == 7);
  assert(strncmp(link, "regular", link_len) == 0);
}

/**
 * Parse Info-ZIP Unix Extra Field (type1)
 */
void unix_infozip1() {
  const u8 data_local[] = {0xD4, 0x6F, 0xCE, 0x51, 0x72, 0xE3,
                           0xC7, 0x52, 0x02, 0x01, 0x04, 0x03};
  const u8 data_central[] = {0x72, 0xE3, 0xC7, 0x52, 0xD4, 0x6F, 0xCE, 0x51};

  bool has_uid_gid;
  time_t atime, mtime;
  uid_t uid;
  gid_t gid;
  // local header
  assert(ExtraField::parseSimpleUnixField(0x5855, data_local, has_uid_gid, uid,
                                          gid, mtime, atime));
  assert(has_uid_gid);
  assert(atime == 0x51CE6FD4);
  assert(mtime == 0x52C7E372);
  assert(uid == 0x0102);
  assert(gid == 0x0304);
  // central header
  assert(ExtraField::parseSimpleUnixField(0x5855, data_central, has_uid_gid,
                                          uid, gid, mtime, atime));
  assert(!has_uid_gid);
  assert(atime == 0x52C7E372);
  assert(mtime == 0x51CE6FD4);
}

/**
 * Parse Info-ZIP Unix Extra Field (type2)
 */
void unix_infozip2() {
  const u8 data_local[] = {0x02, 0x01, 0x04, 0x03};
  const u8 data_central[] = {0};

  uid_t uid;
  gid_t gid;
  // local header
  assert(ExtraField::parseUnixUidGidField(0x7855, data_local, uid, gid));
  assert(uid == 0x0102);
  assert(gid == 0x0304);
  // central header
  assert(!ExtraField::parseUnixUidGidField(0x7855, data_central, uid, gid));
}

/**
 * Parse Info-ZIP New Unix Extra Field
 */
void unix_infozip_new() {
  const u8 data1[] = {1, 1, 0x01, 1, 0xF1};
  const u8 data4[] = {1, 4, 0x04, 0x03, 0x02, 0x01, 4, 0xF8, 0xF7, 0xF6, 0xF5};
  const u8 data16_fit[] = {1,    16,   0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           16,   0xF2, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const u8 data16_uid_overflow[] = {
      1,    16,   0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06,
      0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 16,   0xFF, 0xFE, 0xFD, 0xFC, 0xFB,
      0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0};
  const u8 data16_gid_overflow[] = {
      1,    16,   0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 16,   0xFF, 0xFE, 0xFD, 0xFC, 0xFB,
      0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0};

  uid_t uid;
  gid_t gid;
  // 8-bit
  assert(ExtraField::parseUnixUidGidField(0x7875, data1, uid, gid));
  assert(uid == 0x01);
  assert(gid == 0xF1);
  // 32-bit
  assert(ExtraField::parseUnixUidGidField(0x7875, data4, uid, gid));
  assert(uid == 0x01020304);
  assert(gid == 0xF5F6F7F8);
  // 128-bit fit into uid_t and gid_t
  assert(ExtraField::parseUnixUidGidField(0x7875, data16_fit, uid, gid));
  assert(uid == 0x0102);
  assert(gid == 0xF1F2);
  // 128-bit, UID doesn't fit into uid_t
  assert(
      !ExtraField::parseUnixUidGidField(0x7875, data16_uid_overflow, uid, gid));
  // 128-bit, GID doesn't fit into gid_t
  assert(
      !ExtraField::parseUnixUidGidField(0x7875, data16_gid_overflow, uid, gid));
}

/**
 * Parse NTFS Extra Field
 */
void ntfs_extra_field_parse() {
  const u8 data[] = {
      0x00, 0x00, 0x00, 0x00,                          // reserved
      0x01, 0x00,                                      // tag 1
      0x18, 0x00,                                      // size
      0x00, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01,  // ctime
      0x00, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01,  // atime
      0x00, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01,  // btime
      0xEF, 0xDC,                                      // unknown tag
      0x03, 0x00,                                      // size
      0x01, 0x02, 0x03,                                // unhandled data
      0x01, 0x00,                                      // tag 1 (again)
      0x18, 0x00,                                      // size
      0x1B, 0xFA, 0x1F, 0x5E, 0xF3, 0x21, 0xD5, 0x01,  // ctime
      0x87, 0xCB, 0xA9, 0x32, 0x33, 0x8E, 0xC9, 0x01,  // atime
      0xFF, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01   // btime
  };

  struct timespec mtime, atime, btime;
  assert(ExtraField::parseNtfsExtraField(data, mtime, atime, btime));

  assert(mtime.tv_sec == 1560435721);
  assert(mtime.tv_nsec == 722114700);
  assert(atime.tv_sec == 1234567890);
  assert(atime.tv_nsec == 123456700);
  assert(btime.tv_sec == 0);
  assert(btime.tv_nsec == 0xFF * 100);
}

int main(int, char**) {
  timestamp_mtime_atime_present_local();
  timestamp_mtime_ctime_present_local();

  timestamp_bad();

  unix_pkware_regular();
  unix_pkware_device();
  unix_pkware_link();

  unix_infozip1();
  unix_infozip2();
  unix_infozip_new();

  ntfs_extra_field_parse();

  return EXIT_SUCCESS;
}

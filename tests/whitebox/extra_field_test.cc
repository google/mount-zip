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

#include "../config.h"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <zip.h>

#define private public

#include "extra_field.h"

void test_getShort() {
  zip_uint8_t d[] = {0x01, 0x02, 0xF1, 0xF2};
  const zip_uint8_t* data = d;

  assert(ExtraField::getShort(data) == 0x0201);
  assert(ExtraField::getShort(data) == 0xF2F1);
}

void test_getLong() {
  zip_uint8_t d[] = {0x01, 0x02, 0x03, 0x04, 0xF1, 0xF2, 0xF3, 0xF4};
  const zip_uint8_t* data = d;

  assert(ExtraField::getLong(data) == 0x04030201);
  assert(ExtraField::getLong(data) == 0xF4F3F2F1);
}

void test_getLong64() {
  zip_uint8_t d[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                     0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8};
  const zip_uint8_t* data = d;

  assert(ExtraField::getLong64(data) == 0x0807060504030201);
  assert(ExtraField::getLong64(data) == 0xF8F7F6F5F4F3F2F1);
}

/**
 * LOCAL extra field with both mtime and atime present in flags
 */
void timestamp_mtime_atime_present_local() {
  const zip_uint8_t data[] = {1 | 2, 0xD4, 0x6F, 0xCE, 0x51,
                              0x72,  0xE3, 0xC7, 0x52};

  bool has_atime, has_mtime, has_cretime;
  time_t atime, mtime, cretime;
  assert(ExtraField::parseExtTimeStamp(sizeof(data), data, has_mtime, mtime,
                                       has_atime, atime, has_cretime, cretime));
  assert(has_mtime);
  assert(has_atime);
  assert(!has_cretime);
  assert(mtime == 0x51CE6FD4);
  assert(atime == 0X52C7E372);
}

/**
 * LOCAL extra field with both mtime and creation time present in flags
 */
void timestamp_mtime_cretime_present_local() {
  const zip_uint8_t data[] = {1 | 4, 0xD4, 0x6F, 0xCE, 0x51,
                              0x72,  0xE3, 0xC7, 0x52};

  bool has_atime, has_mtime, has_cretime;
  time_t atime, mtime, cretime;
  assert(ExtraField::parseExtTimeStamp(sizeof(data), data, has_mtime, mtime,
                                       has_atime, atime, has_cretime, cretime));
  assert(has_mtime);
  assert(!has_atime);
  assert(has_cretime);
  assert(mtime == 0x51CE6FD4);
  assert(cretime == 0x52C7E372);
}

/**
 * Bad timestamp
 */
void timestamp_bad() {
  const zip_uint8_t data[] = {1 | 2 | 4, 0x72, 0xE3, 0xC7, 0x52};

  bool has_atime, has_mtime, has_cretime;
  time_t atime, mtime, cretime;
  assert(!ExtraField::parseExtTimeStamp(sizeof(data), data, has_mtime, mtime,
                                        has_atime, atime, has_cretime,
                                        cretime));
}

/**
 * create timestamp extra field for CENTRAL directory
 */
void timestamp_create_central() {
  zip_uint16_t len;
  const zip_uint8_t* data;
  const zip_uint8_t expected[] = {1 | 2, 4, 3, 2, 1};

  data = ExtraField::createExtTimeStamp(ZIP_FL_CENTRAL, 0x01020304, 0x05060708,
                                        false, 0, len);
  assert(data != NULL);
  assert(len == sizeof(expected));

  for (int i = 0; i < len; ++i) {
    assert(data[i] == expected[i]);
  }
}

/**
 * create LOCAL timestamp extra field
 */
void timestamp_create_local() {
  zip_uint16_t len;
  const zip_uint8_t* data;
  const zip_uint8_t expected[] = {1 | 2, 4, 3, 2, 1, 8, 7, 6, 5};

  data = ExtraField::createExtTimeStamp(ZIP_FL_LOCAL, 0x01020304, 0x05060708,
                                        false, 0, len);
  assert(data != NULL);
  assert(len == sizeof(expected));

  for (int i = 0; i < len; ++i) {
    assert(data[i] == expected[i]);
  }
}

/**
 * create LOCAL timestamp extra field with creation time defined
 */
void timestamp_create_local_cretime() {
  zip_uint16_t len;
  const zip_uint8_t* data;
  const zip_uint8_t expected[] = {1 | 2 | 4, 4, 3,   2,   1,   8,  7,
                                  6,         5, 0xC, 0xB, 0xA, 0x9};

  data = ExtraField::createExtTimeStamp(ZIP_FL_LOCAL, 0x01020304, 0x05060708,
                                        true, 0x090A0B0C, len);
  assert(data != NULL);
  assert(len == sizeof(expected));

  for (int i = 0; i < len; ++i) {
    assert(data[i] == expected[i]);
  }
}

/**
 * Parse PKWARE Unix Extra Field - regular file
 */
void unix_pkware_regular() {
  const zip_uint8_t data[] = {
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
  uint16_t link_len;
  assert(ExtraField::parsePkWareUnixField(sizeof(data), data, S_IFREG | 0666,
                                          mtime, atime, uid, gid, dev, link,
                                          link_len));
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
  const zip_uint8_t data[] = {
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
  uint16_t link_len;
  assert(ExtraField::parsePkWareUnixField(sizeof(data), data, S_IFBLK | 0666,
                                          mtime, atime, uid, gid, dev, link,
                                          link_len));
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
  const zip_uint8_t data[] = {
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
  uint16_t link_len;
  assert(ExtraField::parsePkWareUnixField(sizeof(data), data, S_IFLNK | 0777,
                                          mtime, atime, uid, gid, dev, link,
                                          link_len));
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
  const zip_uint8_t data_local[] = {0xD4, 0x6F, 0xCE, 0x51, 0x72, 0xE3,
                                    0xC7, 0x52, 0x02, 0x01, 0x04, 0x03};
  const zip_uint8_t data_central[] = {0x72, 0xE3, 0xC7, 0x52,
                                      0xD4, 0x6F, 0xCE, 0x51};

  bool has_uid_gid;
  time_t atime, mtime;
  uid_t uid;
  gid_t gid;
  // local header
  assert(ExtraField::parseSimpleUnixField(0x5855, sizeof(data_local),
                                          data_local, has_uid_gid, uid, gid,
                                          mtime, atime));
  assert(has_uid_gid);
  assert(atime == 0x51CE6FD4);
  assert(mtime == 0x52C7E372);
  assert(uid == 0x0102);
  assert(gid == 0x0304);
  // central header
  assert(ExtraField::parseSimpleUnixField(0x5855, sizeof(data_central),
                                          data_central, has_uid_gid, uid, gid,
                                          mtime, atime));
  assert(!has_uid_gid);
  assert(atime == 0x52C7E372);
  assert(mtime == 0x51CE6FD4);
}

/**
 * Parse Info-ZIP Unix Extra Field (type2)
 */
void unix_infozip2() {
  const zip_uint8_t data_local[] = {0x02, 0x01, 0x04, 0x03};
  const zip_uint8_t data_central[] = {};

  uid_t uid;
  gid_t gid;
  // local header
  assert(ExtraField::parseUnixUidGidField(0x7855, sizeof(data_local),
                                          data_local, uid, gid));
  assert(uid == 0x0102);
  assert(gid == 0x0304);
  // central header
  assert(!ExtraField::parseUnixUidGidField(0x7855, sizeof(data_central),
                                           data_central, uid, gid));
}

/**
 * Parse Info-ZIP New Unix Extra Field
 */
void unix_infozip_new() {
  const zip_uint8_t data1[] = {1, 1, 0x01, 1, 0xF1};
  const zip_uint8_t data4[] = {1, 4,    0x04, 0x03, 0x02, 0x01,
                               4, 0xF8, 0xF7, 0xF6, 0xF5};
  const zip_uint8_t data16_fit[] = {
      1,    16,   0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 16,   0xF2, 0xF1, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const zip_uint8_t data16_uid_overflow[] = {
      1,    16,   0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06,
      0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 16,   0xFF, 0xFE, 0xFD, 0xFC, 0xFB,
      0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0};
  const zip_uint8_t data16_gid_overflow[] = {
      1,    16,   0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 16,   0xFF, 0xFE, 0xFD, 0xFC, 0xFB,
      0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0};

  uid_t uid;
  gid_t gid;
  // 8-bit
  assert(
      ExtraField::parseUnixUidGidField(0x7875, sizeof(data1), data1, uid, gid));
  assert(uid == 0x01);
  assert(gid == 0xF1);
  // 32-bit
  assert(
      ExtraField::parseUnixUidGidField(0x7875, sizeof(data4), data4, uid, gid));
  assert(uid == 0x01020304);
  assert(gid == 0xF5F6F7F8);
  // 128-bit fit into uid_t and gid_t
  assert(ExtraField::parseUnixUidGidField(0x7875, sizeof(data16_fit),
                                          data16_fit, uid, gid));
  assert(uid == 0x0102);
  assert(gid == 0xF1F2);
  // 128-bit, UID doesn't fit into uid_t
  assert(!ExtraField::parseUnixUidGidField(0x7875, sizeof(data16_uid_overflow),
                                           data16_uid_overflow, uid, gid));
  // 128-bit, GID doesn't fit into gid_t
  assert(!ExtraField::parseUnixUidGidField(0x7875, sizeof(data16_gid_overflow),
                                           data16_gid_overflow, uid, gid));
}

/**
 * Create Info-ZIP New Unix Extra Field
 */
void infozip_unix_new_create() {
  zip_uint16_t len;
  const zip_uint8_t* data;
  const int uidLen = sizeof(uid_t), gidLen = sizeof(gid_t);
  assert(uidLen >= 2 && "too short UID type to test");
  assert(gidLen >= 2 && "too short GID type to test");

  zip_uint8_t expected[3 + uidLen + gidLen];
  memset(expected, 0, sizeof(expected));
  expected[0] = 1;
  expected[1] = uidLen;
  expected[2] = 0x02;
  expected[3] = 0x01;
  expected[2 + uidLen] = gidLen;
  expected[3 + uidLen] = 0x04;
  expected[4 + uidLen] = 0x03;

  data = ExtraField::createInfoZipNewUnixField(0x0102, 0x0304, len);
  assert(data != NULL);
  assert(len == sizeof(expected));

  for (unsigned int i = 0; i < sizeof(expected); ++i) {
    assert(data[i] == expected[i]);
  }
}

/**
 * Create PKWARE Unix Extra Field - regular file
 */
void pkware_create_regular() {
  zip_uint8_t expected[] = {
      0xD4, 0x6F, 0xCE, 0x51,  // atime
      0x72, 0xE3, 0xC7, 0x52,  // mtime
      0x02, 0x01,              // UID
      0x04, 0x03               // GID
  };

  zip_uint16_t len;
  const zip_uint8_t* data;

  time_t atime, mtime;
  uid_t uid;
  gid_t gid;
  dev_t dev;

  atime = 0x51CE6FD4;
  mtime = 0x52C7E372;
  uid = 0x0102;
  gid = 0x0304;
  dev = 0;

  data = ExtraField::createPkWareUnixField(mtime, atime, S_IFREG | 0666, uid,
                                           gid, dev, len);
  assert(data != NULL);
  assert(len == sizeof(expected));
  for (int i = 0; i < len; ++i) {
    assert(data[i] == expected[i]);
  }
}

/**
 * Create PKWARE Unix Extra Field - device
 */
void pkware_create_device() {
  zip_uint8_t expected[] = {
      0xD4, 0x6F, 0xCE, 0x51,  // atime
      0x72, 0xE3, 0xC7, 0x52,  // mtime
      0x02, 0x01,              // UID
      0x04, 0x03,              // GID
      0x34, 0x12, 0x00, 0x00,  // device major
      0x78, 0x56, 0x00, 0x00   // device minor
  };

  zip_uint16_t len;
  const zip_uint8_t* data;

  time_t atime, mtime;
  uid_t uid;
  gid_t gid;
  dev_t dev;

  atime = 0x51CE6FD4;
  mtime = 0x52C7E372;
  uid = 0x0102;
  gid = 0x0304;
  dev = makedev(0x1234, 0x5678);

  data = ExtraField::createPkWareUnixField(mtime, atime, S_IFCHR | 0666, uid,
                                           gid, dev, len);
  assert(data != NULL);
  assert(len == sizeof(expected));
  for (int i = 0; i < len; ++i) {
    assert(data[i] == expected[i]);
  }
}

/**
 * Parse NTFS Extra Field
 */
void ntfs_extra_field_parse() {
  const zip_uint8_t data[] = {
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
  assert(
      ExtraField::parseNtfsExtraField(sizeof(data), data, mtime, atime, btime));

  assert(mtime.tv_sec == 1560435721);
  assert(mtime.tv_nsec == 722114700);
  assert(atime.tv_sec == 1234567890);
  assert(atime.tv_nsec == 123456700);
  assert(btime.tv_sec == 0);
  assert(btime.tv_nsec == 0xFF * 100);
}

/**
 * Create NTFS Extra Field
 */
void ntfs_extra_field_create() {
  zip_uint16_t len = 0;
  const zip_uint8_t* data;
  struct timespec mtime, atime, btime;
  zip_uint8_t expected[] = {
      0x00, 0x00, 0x00, 0x00,                          // reserved
      0x01, 0x00,                                      // tag 1
      0x18, 0x00,                                      // size
      0xFF, 0x60, 0x4A, 0x5E, 0xF3, 0x21, 0xD5, 0x01,  // mtime
      0x87, 0xCB, 0xA9, 0x32, 0x33, 0x8E, 0xC9, 0x01,  // atime
      0xFF, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01   // btime
  };

  mtime.tv_sec = 1560435721;
  mtime.tv_nsec = 999999900;
  atime.tv_sec = 1234567890;
  atime.tv_nsec = 123456700;
  btime.tv_sec = 0;
  btime.tv_nsec = 0xFF * 100;

  data = ExtraField::createNtfsExtraField(mtime, atime, btime, len);
  assert(data != NULL);
  assert(len == sizeof(expected));
  for (unsigned int i = 0; i < len; ++i) {
    assert(data[i] == expected[i]);
  }

  struct timespec mtime2, atime2, btime2;
  assert(ExtraField::parseNtfsExtraField(len, data, mtime2, atime2, btime2));

  assert(mtime.tv_sec == mtime2.tv_sec);
  assert(mtime.tv_nsec == mtime2.tv_nsec);
  assert(atime.tv_sec == atime2.tv_sec);
  assert(atime.tv_nsec == atime2.tv_nsec);
  assert(btime.tv_sec == btime2.tv_sec);
  assert(btime.tv_nsec == btime2.tv_nsec);
}

/**
 * Edit NTFS Extra Field
 */
void ntfs_extra_field_edit_replace() {
  struct timespec mtime, atime, btime;
  zip_uint8_t data[] = {
      0x12, 0x34, 0x56, 0x78,                          // reserved
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
  const zip_uint8_t expected[] = {
      0x12, 0x34, 0x56, 0x78,                          // reserved
      0xEF, 0xDC,                                      // unknown tag
      0x03, 0x00,                                      // size
      0x01, 0x02, 0x03,                                // unhandled data
      0x01, 0x00,                                      // tag 1
      0x18, 0x00,                                      // size
      0xFF, 0x60, 0x4A, 0x5E, 0xF3, 0x21, 0xD5, 0x01,  // mtime
      0x87, 0xCB, 0xA9, 0x32, 0x33, 0x8E, 0xC9, 0x01,  // atime
      0xFF, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01   // btime
  };

  mtime.tv_sec = 1560435721;
  mtime.tv_nsec = 999999900;
  atime.tv_sec = 1234567890;
  atime.tv_nsec = 123456700;
  btime.tv_sec = 0;
  btime.tv_nsec = 0xFF * 100;

  uint16_t len =
      ExtraField::editNtfsExtraField(sizeof(data), data, mtime, atime, btime);
  assert(len == sizeof(expected));
  for (unsigned int i = 0; i < len; ++i) {
    assert(data[i] == expected[i]);
  }

  struct timespec mtime2, atime2, btime2;
  assert(ExtraField::parseNtfsExtraField(len, data, mtime2, atime2, btime2));

  assert(mtime.tv_sec == mtime2.tv_sec);
  assert(mtime.tv_nsec == mtime2.tv_nsec);
  assert(atime.tv_sec == atime2.tv_sec);
  assert(atime.tv_nsec == atime2.tv_nsec);
  assert(btime.tv_sec == btime2.tv_sec);
  assert(btime.tv_nsec == btime2.tv_nsec);
}

void ntfs_extra_field_edit_add() {}

void ntfs_extra_field_edit_incomplete_reserved() {
  struct timespec mtime, atime, btime;
  zip_uint8_t data[] = {
      0x12, 0x34, 0x56, /* END */ 0x78,                          // reserved
      0x00, 0x00,                                                // tag 1
      0x00, 0x00,                                                // size
      0x00, 0x00, 0x00, 0x00,           0x00, 0x00, 0x00, 0x00,  // ctime
      0x00, 0x00, 0x00, 0x00,           0x00, 0x00, 0x00, 0x00,  // atime
      0x00, 0x00, 0x00, 0x00,           0x00, 0x00, 0x00, 0x00   // btime
  };
  const zip_uint8_t expected[] = {
      0x00, 0x00, 0x00, 0x00,                          // reserved
      0x01, 0x00,                                      // tag 1
      0x18, 0x00,                                      // size
      0xFF, 0x60, 0x4A, 0x5E, 0xF3, 0x21, 0xD5, 0x01,  // mtime
      0x87, 0xCB, 0xA9, 0x32, 0x33, 0x8E, 0xC9, 0x01,  // atime
      0xFF, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01   // btime
  };

  mtime.tv_sec = 1560435721;
  mtime.tv_nsec = 999999900;
  atime.tv_sec = 1234567890;
  atime.tv_nsec = 123456700;
  btime.tv_sec = 0;
  btime.tv_nsec = 0xFF * 100;

  uint16_t len = ExtraField::editNtfsExtraField(3, data, mtime, atime, btime);
  assert(len == sizeof(expected));
  for (unsigned int i = 0; i < len; ++i) {
    assert(data[i] == expected[i]);
  }

  struct timespec mtime2, atime2, btime2;
  assert(ExtraField::parseNtfsExtraField(len, data, mtime2, atime2, btime2));

  assert(mtime.tv_sec == mtime2.tv_sec);
  assert(mtime.tv_nsec == mtime2.tv_nsec);
  assert(atime.tv_sec == atime2.tv_sec);
  assert(atime.tv_nsec == atime2.tv_nsec);
  assert(btime.tv_sec == btime2.tv_sec);
  assert(btime.tv_nsec == btime2.tv_nsec);
}

void ntfs_extra_field_edit_incomplete_tag() {
  struct timespec mtime, atime, btime;
  zip_uint8_t data[] = {
      0x12, 0x34,           0x56, 0x78,  // reserved
      0xEF, 0xDC,                        // unknown tag
      0x03, 0x00,                        // size
      0x01, /* END */ 0x02, 0x03,        // unhandled data
      0x00, 0x00,                        // tag 1
      0x00, 0x00,                        // size
      0x00, 0x00,           0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // ctime
      0x00, 0x00,           0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // atime
      0x00, 0x00,           0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // btime
  };
  const zip_uint8_t expected[] = {
      0x12, 0x34, 0x56, 0x78,                          // reserved
      0x01, 0x00,                                      // tag 1
      0x18, 0x00,                                      // size
      0xFF, 0x60, 0x4A, 0x5E, 0xF3, 0x21, 0xD5, 0x01,  // mtime
      0x87, 0xCB, 0xA9, 0x32, 0x33, 0x8E, 0xC9, 0x01,  // atime
      0xFF, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01   // btime
  };

  mtime.tv_sec = 1560435721;
  mtime.tv_nsec = 999999900;
  atime.tv_sec = 1234567890;
  atime.tv_nsec = 123456700;
  btime.tv_sec = 0;
  btime.tv_nsec = 0xFF * 100;

  uint16_t len = ExtraField::editNtfsExtraField(9, data, mtime, atime, btime);
  assert(len == sizeof(expected));
  for (unsigned int i = 0; i < len; ++i) {
    assert(data[i] == expected[i]);
  }

  struct timespec mtime2, atime2, btime2;
  assert(ExtraField::parseNtfsExtraField(len, data, mtime2, atime2, btime2));

  assert(mtime.tv_sec == mtime2.tv_sec);
  assert(mtime.tv_nsec == mtime2.tv_nsec);
  assert(atime.tv_sec == atime2.tv_sec);
  assert(atime.tv_nsec == atime2.tv_nsec);
  assert(btime.tv_sec == btime2.tv_sec);
  assert(btime.tv_nsec == btime2.tv_nsec);
}

int main(int, char**) {
  test_getShort();
  test_getLong();
  test_getLong64();

  timestamp_mtime_atime_present_local();
  timestamp_mtime_cretime_present_local();

  timestamp_bad();

  timestamp_create_central();
  timestamp_create_local();
  timestamp_create_local_cretime();

  unix_pkware_regular();
  unix_pkware_device();
  unix_pkware_link();

  unix_infozip1();
  unix_infozip2();
  unix_infozip_new();

  infozip_unix_new_create();

  pkware_create_regular();
  pkware_create_device();

  ntfs_extra_field_parse();
  ntfs_extra_field_create();
  ntfs_extra_field_edit_replace();
  ntfs_extra_field_edit_add();
  ntfs_extra_field_edit_incomplete_reserved();
  ntfs_extra_field_edit_incomplete_tag();

  return EXIT_SUCCESS;
}

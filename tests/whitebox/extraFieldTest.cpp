#include "../config.h"

#include <zip.h>
#include <assert.h>
#include <stdlib.h>
#include <cstring>
#include <cerrno>

#include "common.h"

#define private public

#include "extraField.h"

void test_getShort() {
    zip_uint8_t d[] = { 0x01, 0x02, 0xF1, 0xF2};
    const zip_uint8_t *data = d;

    assert(ExtraField::getShort(data) == 0x0201);
    assert(ExtraField::getShort(data) == 0xF2F1);
}

void test_getLong() {
    zip_uint8_t d[] = { 0x01, 0x02, 0x03, 0x04, 0xF1, 0xF2, 0xF3, 0xF4};
    const zip_uint8_t *data = d;

    assert(ExtraField::getLong(data) == 0x04030201);
    assert(ExtraField::getLong(data) == 0xF4F3F2F1);
}

void test_getLong64() {
    zip_uint8_t d[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8};
    const zip_uint8_t *data = d;

    assert(ExtraField::getLong64(data) == 0x0807060504030201);
    assert(ExtraField::getLong64(data) == 0xF8F7F6F5F4F3F2F1);
}

/**
 * LOCAL extra field with both mtime and atime present in flags
 */
void timestamp_mtime_atime_present_local () {
    const zip_uint8_t data[] = {1 | 2, 0xD4, 0x6F, 0xCE, 0x51, 0x72, 0xE3, 0xC7, 0x52};

    bool has_atime, has_mtime, has_cretime;
    time_t atime, mtime, cretime;
    assert(ExtraField::parseExtTimeStamp(sizeof(data), data, has_mtime, mtime, has_atime, atime, has_cretime, cretime));
    assert(has_mtime);
    assert(has_atime);
    assert(!has_cretime);
    assert(mtime == 0x51CE6FD4);
    assert(atime == 0X52C7E372);
}

/**
 * LOCAL extra field with both mtime and creation time present in flags
 */
void timestamp_mtime_cretime_present_local () {
    const zip_uint8_t data[] = {1 | 4, 0xD4, 0x6F, 0xCE, 0x51, 0x72, 0xE3, 0xC7, 0x52};

    bool has_atime, has_mtime, has_cretime;
    time_t atime, mtime, cretime;
    assert(ExtraField::parseExtTimeStamp(sizeof(data), data, has_mtime, mtime, has_atime, atime, has_cretime, cretime));
    assert(has_mtime);
    assert(!has_atime);
    assert(has_cretime);
    assert(mtime == 0x51CE6FD4);
    assert(cretime == 0x52C7E372);
}

/**
 * Bad timestamp
 */
void timestamp_bad () {
    const zip_uint8_t data[] = {1 | 2 | 4, 0x72, 0xE3, 0xC7, 0x52};

    bool has_atime, has_mtime, has_cretime;
    time_t atime, mtime, cretime;
    assert(!ExtraField::parseExtTimeStamp(sizeof(data), data, has_mtime, mtime, has_atime, atime, has_cretime, cretime));
}

/**
 * create timestamp extra field for CENTRAL directory
 */
void timestamp_create_central () {
    zip_uint16_t len;
    const zip_uint8_t *data;
    const zip_uint8_t expected[] = {1 | 2, 4, 3, 2, 1};

    data = ExtraField::createExtTimeStamp(ZIP_FL_CENTRAL, 0x01020304, 0x05060708, false, 0, len);
    assert(data != NULL);
    assert(len == sizeof(expected));
    
    for (int i = 0; i < len; ++i) {
        assert(data[i] == expected[i]);
    }
}

/**
 * create LOCAL timestamp extra field
 */
void timestamp_create_local () {
    zip_uint16_t len;
    const zip_uint8_t *data;
    const zip_uint8_t expected[] = {1 | 2, 4, 3, 2, 1, 8, 7, 6, 5};

    data = ExtraField::createExtTimeStamp(ZIP_FL_LOCAL, 0x01020304, 0x05060708, false, 0, len);
    assert(data != NULL);
    assert(len == sizeof(expected));
    
    for (int i = 0; i < len; ++i) {
        assert(data[i] == expected[i]);
    }
}

/**
 * create LOCAL timestamp extra field with creation time defined
 */
void timestamp_create_local_cretime () {
    zip_uint16_t len;
    const zip_uint8_t *data;
    const zip_uint8_t expected[] = {1 | 2 | 4, 4, 3, 2, 1, 8, 7, 6, 5, 0xC, 0xB, 0xA, 0x9};

    data = ExtraField::createExtTimeStamp(ZIP_FL_LOCAL, 0x01020304, 0x05060708, true, 0x090A0B0C, len);
    assert(data != NULL);
    assert(len == sizeof(expected));
    
    for (int i = 0; i < len; ++i) {
        assert(data[i] == expected[i]);
    }
}

/**
 * Parse PKWARE Unix Extra Field
 */
void simple_unix_pkware() {
    const zip_uint8_t data[] = {
        0xD4, 0x6F, 0xCE, 0x51,
        0x72, 0xE3, 0xC7, 0x52,
        0x02, 0x01,
        0x04, 0x03
    };

    bool has_atime, has_mtime;
    time_t atime, mtime;
    uid_t uid;
    gid_t gid;
    assert(ExtraField::parseSimpleUnixField(0x000D, sizeof(data), data,
                uid, gid, has_mtime, mtime, has_atime, atime));
    assert(has_atime);
    assert(has_mtime);
    assert(atime == 0x51CE6FD4);
    assert(mtime == 0x52C7E372);
    assert(uid == 0x0102);
    assert(gid == 0x0304);
}

/**
 * Parse Info-ZIP Unix Extra Field (type2)
 */
void simple_unix_infozip2() {
    const zip_uint8_t data[] = {
        0x02, 0x01,
        0x04, 0x03
    };

    bool has_atime, has_mtime;
    time_t atime, mtime;
    uid_t uid;
    gid_t gid;
    assert(ExtraField::parseSimpleUnixField(0x7855, sizeof(data), data,
                uid, gid, has_mtime, mtime, has_atime, atime));
    assert(!has_atime);
    assert(!has_mtime);
    assert(uid == 0x0102);
    assert(gid == 0x0304);
}

/**
 * Parse Info-ZIP New Unix Extra Field
 */
void simple_unix_infozip_new() {
    const zip_uint8_t data[] = {
        1,
        4, 0x04, 0x03, 0x02, 0x01,
        4, 0x08, 0x07, 0x06, 0x05
    };

    bool has_atime, has_mtime;
    time_t atime, mtime;
    uid_t uid;
    gid_t gid;
    assert(ExtraField::parseSimpleUnixField(0x7875, sizeof(data), data,
                uid, gid, has_mtime, mtime, has_atime, atime));
    assert(!has_atime);
    assert(!has_mtime);
    assert(uid == 0x01020304);
    assert(gid == 0x05060708);
}

/**
 * Create Info-ZIP New Unix Extra Field
 */
void infozip_unix_new_create () {
    zip_uint16_t len;
    const zip_uint8_t *data;
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

    data = ExtraField::createInfoZipNewUnixField (0x0102, 0x0304, len);
    assert(data != NULL);
    assert(len == sizeof(expected));
    
    for (unsigned int i = 0; i < sizeof(expected); ++i) {
        assert(data[i] == expected[i]);
    }
}

/**
 * Parse NTFS Extra Field
 */

void ntfs_extra_field_parse() {
    const zip_uint8_t data[] = {
        0x00, 0x00, 0x00, 0x00, // reserved
        0x01, 0x00, // tag 1
        0x18, 0x00, // size
        0x00, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01, // ctime
        0x00, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01, // atime
        0x00, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01, // btime
        0xEF, 0xDC, // unknown tag
        0x03, 0x00, // size
        0x01, 0x02, 0x03, // unhandled data
        0x01, 0x00, // tag 1 (again)
        0x18, 0x00, // size
        0x1B, 0xFA, 0x1F, 0x5E, 0xF3, 0x21, 0xD5, 0x01, // ctime
        0x87, 0xCB, 0xA9, 0x32, 0x33, 0x8E, 0xC9, 0x01, // atime
        0xFF, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01  // btime
    };

    struct timespec mtime, atime, btime;
    assert(ExtraField::parseNtfsExtraField(sizeof(data), data,
                mtime, atime, btime));

    assert(mtime.tv_sec  == 1560435721);
    assert(mtime.tv_nsec == 722114700);
    assert(atime.tv_sec  == 1234567890);
    assert(atime.tv_nsec == 123456700);
    assert(btime.tv_sec  == 0);
    assert(btime.tv_nsec == 0xFF * 100);
}

/**
 * Create NTFS Extra Field
 */
void ntfs_extra_field_create() {
    zip_uint16_t len = 0;
    const zip_uint8_t *data;
    struct timespec mtime, atime, btime;
    zip_uint8_t expected[] = {
        0x00, 0x00, 0x00, 0x00, // reserved
        0x01, 0x00, // tag 1
        0x18, 0x00, // size
        0xFF, 0x60, 0x4A, 0x5E, 0xF3, 0x21, 0xD5, 0x01, // mtime
        0x87, 0xCB, 0xA9, 0x32, 0x33, 0x8E, 0xC9, 0x01, // atime
        0xFF, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01  // btime
    };

    mtime.tv_sec  = 1560435721;
    mtime.tv_nsec = 999999900;
    atime.tv_sec  = 1234567890;
    atime.tv_nsec = 123456700;
    btime.tv_sec  = 0;
    btime.tv_nsec = 0xFF * 100;

    data = ExtraField::createNtfsExtraField(ZIP_FL_LOCAL, mtime, atime, btime, len);
    assert(data != NULL);
    assert(len == sizeof(expected));
    for (unsigned int i = 0; i < len; ++i) {
        assert(data[i] == expected[i]);
    }

    struct timespec mtime2, atime2, btime2;
    assert(ExtraField::parseNtfsExtraField(len, data,
                mtime2, atime2, btime2));

    assert(mtime.tv_sec  == mtime2.tv_sec);
    assert(mtime.tv_nsec == mtime2.tv_nsec);
    assert(atime.tv_sec  == atime2.tv_sec);
    assert(atime.tv_nsec == atime2.tv_nsec);
    assert(btime.tv_sec  == btime2.tv_sec);
    assert(btime.tv_nsec == btime2.tv_nsec);
}

int main(int, char **) {
    test_getShort();
    test_getLong();
    test_getLong64();

    timestamp_mtime_atime_present_local();
    timestamp_mtime_cretime_present_local();

    timestamp_bad();

    timestamp_create_central();
    timestamp_create_local();
    timestamp_create_local_cretime();

    simple_unix_pkware();
    simple_unix_infozip2();
    simple_unix_infozip_new();

    infozip_unix_new_create();

    ntfs_extra_field_parse();
    ntfs_extra_field_create();

    return EXIT_SUCCESS;
}


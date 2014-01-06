#include "../config.h"

#include <zip.h>
#include <assert.h>
#include <stdlib.h>
#include <cstring>
#include <cerrno>

#include "common.h"

#include "extraField.h"

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

int main(int, char **) {
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

    return EXIT_SUCCESS;
}


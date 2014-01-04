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

    bool has_atime, has_mtime;
    time_t atime, mtime;
    assert(ExtraField::parseExtTimeStamp(sizeof(data), data, has_mtime, mtime, has_atime, atime));
    assert(has_mtime);
    assert(has_atime);
    assert(mtime == 0x51CE6FD4);
    assert(atime == 0X52C7E372);
}

/**
 * LOCAL extra field with both mtime and creation time present in flags
 */
void timestamp_mtime_cretime_present_local () {
    const zip_uint8_t data[] = {1 | 4, 0xD4, 0x6F, 0xCE, 0x51, 0x72, 0xE3, 0xC7, 0x52};

    bool has_atime, has_mtime;
    time_t atime, mtime;
    assert(ExtraField::parseExtTimeStamp(sizeof(data), data, has_mtime, mtime, has_atime, atime));
    assert(has_mtime);
    assert(!has_atime);
    assert(mtime == 0x51CE6FD4);
}

/**
 * Bad timestamp
 */
void timestamp_bad () {
    const zip_uint8_t data[] = {1 | 2 | 4, 0x72, 0xE3, 0xC7, 0x52};

    bool has_atime, has_mtime;
    time_t atime, mtime;
    assert(!ExtraField::parseExtTimeStamp(sizeof(data), data, has_mtime, mtime, has_atime, atime));
}

/**
 * create timestamp extra field for CENTRAL directory
 */
void timestamp_create_central () {
    zip_uint16_t len;
    const zip_uint8_t *data;
    const zip_uint8_t expected[] = {1 | 2, 4, 3, 2, 1};

    data = ExtraField::createExtTimeStamp(ZIP_FL_CENTRAL, 0x01020304, 0x05060708, len);
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

    data = ExtraField::createExtTimeStamp(ZIP_FL_LOCAL, 0x01020304, 0x05060708, len);
    assert(data != NULL);
    assert(len == sizeof(expected));
    
    for (int i = 0; i < len; ++i) {
        assert(data[i] == expected[i]);
    }
}

int main(int, char **) {
    timestamp_mtime_atime_present_local();
    timestamp_mtime_cretime_present_local();

    timestamp_bad();

    timestamp_create_central();
    timestamp_create_local();

    return EXIT_SUCCESS;
}


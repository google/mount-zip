#include <cassert>
#include <cstdlib>
#include <memory>

#include <syslog.h>
#include <zip.h>

#include "fileNode.h"

void zip_stat_init(struct zip_stat *sb) {
    memset(sb, 0, sizeof(struct zip_stat));
}

void initTest() {
    // hide almost all messages
    setlogmask(LOG_MASK(LOG_EMERG));
}

uint16_t extrafld_type;
bool input_exist;
uint16_t input_len, expected_len;
const uint8_t *input_data, *expected_data;

zip_int16_t zip_file_extra_fields_count(struct zip *, zip_uint64_t, zip_flags_t) {
    return 1;
}

zip_int16_t zip_file_extra_fields_count_by_id(struct zip *, zip_uint64_t, zip_uint16_t, zip_flags_t) {
    return (input_exist) ? 1 : 0;
}

const zip_uint8_t *zip_file_extra_field_get(struct zip *, zip_uint64_t, zip_uint16_t,
        zip_uint16_t *idp, zip_uint16_t *lenp, zip_flags_t) {
    *idp = extrafld_type;
    *lenp = input_len;
    return input_data;
}

const zip_uint8_t *zip_file_extra_field_get_by_id(struct zip *, zip_uint64_t, zip_uint16_t, zip_uint16_t, zip_uint16_t *, zip_flags_t) {
    assert(false);
    return NULL;
}

int zip_file_extra_field_delete(struct zip *, zip_uint64_t, zip_uint16_t, zip_flags_t) {
    return 0;
}

int zip_file_extra_field_set(struct zip *, zip_uint64_t, zip_uint16_t id, zip_uint16_t,
        const zip_uint8_t *data, zip_uint16_t len, zip_flags_t) {
    if (id == extrafld_type) {
        assert(len == expected_len);
        for (unsigned int i = 0; i < len; ++i) {
            assert(data[i] == expected_data[i]);
        }
    }
    return 0;
}

int zip_file_get_external_attributes(struct zip *, zip_uint64_t, zip_flags_t, zip_uint8_t *opsysPtr, zip_uint32_t *attrPtr) {
    *opsysPtr = ZIP_OPSYS_UNIX;
    *attrPtr = 0;
    return 0;
}

int zip_file_set_external_attributes(struct zip *, zip_uint64_t, zip_flags_t, zip_uint8_t, zip_uint32_t) {
    return 0;
}

int zip_stat_index(struct zip *, zip_uint64_t, zip_flags_t, struct zip_stat *zs) {
    zs->valid = ZIP_STAT_NAME | ZIP_STAT_INDEX | ZIP_STAT_SIZE | ZIP_STAT_MTIME;
    zs->name = "test";
    zs->index = 0;
    zs->size = 0;
    zs->mtime = 1234567890; // Fri Feb 13 23:31:30 GMT 2009
    return 0;
}

struct zip_file *zip_fopen_index(struct zip *, zip_uint64_t, zip_flags_t) {
    assert(false);
    return NULL;
}

zip_int64_t zip_fread(struct zip_file *, void *, zip_uint64_t) {
    assert(false);
    return 0;
}

int zip_fclose(struct zip_file *) {
    assert(false);
    return 0;
}

zip_int64_t zip_file_add(struct zip *, const char *, struct zip_source *, zip_flags_t) {
    assert(false);
    return 0;
}

int zip_file_replace(struct zip *, zip_uint64_t, struct zip_source *, zip_flags_t) {
    return 0;
}

struct zip_source *zip_source_function(struct zip *, zip_source_callback, void *) {
    assert(false);
    return NULL;
}

void zip_source_free(struct zip_source *) {
    assert(false);
}

const char *zip_get_name(struct zip *, zip_uint64_t, zip_flags_t) {
    assert(false);
    return NULL;
}

const char *zip_file_strerror(struct zip_file *) {
    assert(false);
    return NULL;
}

const char *zip_strerror(struct zip *) {
    assert(false);
    return NULL;
}

const char *zip_get_archive_comment(zip_t *, int *, zip_flags_t) {
    return NULL;
}

int zip_set_archive_comment(zip_t *, const char *, zip_uint16_t) {
    assert(false);
    return 0;
}

const char *zip_file_get_comment(zip_t *, zip_uint64_t, zip_uint32_t *, zip_flags_t) {
    return NULL;
}

int zip_file_set_comment(zip_t *, zip_uint64_t, const char *, zip_uint16_t, zip_flags_t) {
    assert(false);
    return 0;
}

void ntfs_extra_field_create() {
    const zip_uint8_t expected[] = {
        0x00, 0x00, 0x00, 0x00, // reserved
        0x01, 0x00, // tag 1
        0x18, 0x00, // size
        0xFF, 0x60, 0x4A, 0x5E, 0xF3, 0x21, 0xD5, 0x01, // mtime
        0x87, 0xCB, 0xA9, 0x32, 0x33, 0x8E, 0xC9, 0x01, // atime
        0x00, 0xF5, 0x96, 0x32, 0x33, 0x8E, 0xC9, 0x01  // btime
    };

    extrafld_type = 0x000A;
    input_exist = false;
    expected_data = expected;
    expected_len = sizeof(expected);

    std::unique_ptr<FileNode> n (FileNode::createNodeForZipEntry((zip*)1, "test", 0, S_IFREG | 0666));
    struct timespec atime, mtime;
    mtime.tv_sec  = 1560435721;
    mtime.tv_nsec = 999999900;
    atime.tv_sec  = 1234567890;
    atime.tv_nsec = 123456700;
    n->setTimes(atime, mtime);

    n->saveMetadata(true);
}

void ntfs_extra_field_edit() {
    const uint8_t data[] = {
        0x12, 0x34, 0x56, 0x78, // reserved
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
    const zip_uint8_t expected[] = {
        0x12, 0x34, 0x56, 0x78, // reserved
        0xEF, 0xDC, // unknown tag
        0x03, 0x00, // size
        0x01, 0x02, 0x03, // unhandled data
        0x01, 0x00, // tag 1
        0x18, 0x00, // size
        0xFF, 0x60, 0x4A, 0x5E, 0xF3, 0x21, 0xD5, 0x01, // mtime
        0x87, 0xCB, 0xA9, 0x32, 0x33, 0x8E, 0xC9, 0x01, // atime
        0xFF, 0x80, 0x3E, 0xD5, 0xDE, 0xB1, 0x9D, 0x01  // btime
    };

    extrafld_type = 0x000A;
    input_exist = true;
    input_data = data;
    input_len = sizeof(data);
    expected_data = expected;
    expected_len = sizeof(expected);

    std::unique_ptr<FileNode> n (FileNode::createNodeForZipEntry((zip*)1, "test", 0, S_IFREG | 0666));
    struct timespec atime, mtime;
    mtime.tv_sec  = 1560435721;
    mtime.tv_nsec = 999999900;
    atime.tv_sec  = 1234567890;
    atime.tv_nsec = 123456700;
    n->setTimes(atime, mtime);

    n->saveMetadata(true);
}

// no extra field - create

int main(int, char **) {
    initTest();

    ntfs_extra_field_create();
    ntfs_extra_field_edit();

    return EXIT_SUCCESS;
}

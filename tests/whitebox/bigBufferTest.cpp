#include "../config.h"

#include <zip.h>
#include <assert.h>
#include <stdlib.h>
#include <cstring>

#define private public

#include "bigBuffer.h"
#include "common.h"

// libzip stub structures
struct zip {};
struct zip_file {};
struct zip_source {};

// libzip stub functions

struct zip *zip_open(const char *, int, int *errorp) {
    *errorp = 0;
    return NULL;
}

int zip_error_to_str(char *buf, size_t len, int, int) {
    return strncpy(buf, "Expected error", len) - buf;
}

// only stubs

int zip_add(struct zip *, const char *, struct zip_source *) {
    assert(false);
    return 0;
}

int zip_add_dir(struct zip *, const char *) {
    assert(false);
    return 0;
}

int zip_close(struct zip *) {
    assert(false);
    return 0;
}

int zip_delete(struct zip *, int) {
    assert(false);
    return 0;
}

int zip_fclose(struct zip_file *) {
    assert(false);
    return 0;
}

struct zip_file *zip_fopen_index(struct zip *, int, int) {
    assert(false);
    return NULL;
}

ssize_t zip_fread(struct zip_file *, void *, size_t) {
    assert(false);
    return 0;
}

int zip_get_num_files(struct zip *) {
    assert(false);
    return 0;
}

int zip_rename(struct zip *, int, const char *) {
    assert(false);
    return 0;
}

int zip_replace(struct zip *, int, struct zip_source *) {
    assert(false);
    return 0;
}

void zip_source_free(struct zip_source *) {
    assert(false);
}

struct zip_source *zip_source_function(struct zip *, zip_source_callback, void *) {
    assert(false);
    return NULL;
}

int zip_stat_index(struct zip *, int, int, struct zip_stat *) {
    assert(false);
    return 0;
}

const char *zip_strerror(struct zip *) {
    assert(false);
    return NULL;
}

// test functions

int main(int, char **) {
    initTest();

    // static functions test
    assert(BigBuffer::chunksCount(0) == 0);
    assert(BigBuffer::chunksCount(1) == 1);
    assert(BigBuffer::chunksCount(BigBuffer::chunkSize) == 1);
    assert(BigBuffer::chunksCount(BigBuffer::chunkSize - 1) == 1);
    assert(BigBuffer::chunksCount(BigBuffer::chunkSize + 1) == 2);
    assert(BigBuffer::chunksCount(BigBuffer::chunkSize * 2 - 1) == 2);

    assert(BigBuffer::chunkNumber(0) == 0);
    assert(BigBuffer::chunkNumber(1) == 0);
    assert(BigBuffer::chunkNumber(BigBuffer::chunkSize) == 1);
    assert(BigBuffer::chunkNumber(BigBuffer::chunkSize - 1) == 0);
    assert(BigBuffer::chunkNumber(BigBuffer::chunkSize + 1) == 1);
    assert(BigBuffer::chunkNumber(BigBuffer::chunkSize * 2 - 1) == 1);

    assert(BigBuffer::chunkOffset(0) == 0);
    assert(BigBuffer::chunkOffset(1) == 1);
    assert(BigBuffer::chunkOffset(BigBuffer::chunkSize) == 0);
    assert(BigBuffer::chunkOffset(BigBuffer::chunkSize - 1) == BigBuffer::chunkSize - 1);
    assert(BigBuffer::chunkOffset(BigBuffer::chunkSize + 1) == 1);
    assert(BigBuffer::chunkOffset(BigBuffer::chunkSize * 2 - 1) == BigBuffer::chunkSize - 1);

    // create-delete
    {
        BigBuffer bb;
        assert(bb.len == 0);
    }

    // truncation
    {
        BigBuffer bb;

        bb.truncate(22);
        assert(bb.len == 22);

        bb.truncate(2);
        assert(bb.len == 2);

        bb.truncate(BigBuffer::chunkSize);
        assert(bb.len == BigBuffer::chunkSize);

        bb.truncate(BigBuffer::chunkSize + 1);
        assert(bb.len == BigBuffer::chunkSize + 1);

        bb.truncate(0);
        assert(bb.len == 0);
    }

    // read
    {
        char buf[0xff];
        char empty[0xff];
        memset(empty, 0, 0xff);
        int nr;
        BigBuffer bb;

        nr = bb.read(buf, 100, 0);
        assert(nr == 0);

        nr = bb.read(buf, 100, 100);
        assert(nr == 0);

        bb.truncate(10);
        nr = bb.read(buf, 10, 0);
        assert(nr == 10);
        assert(memcmp(buf, empty, nr) == 0);

        bb.truncate(BigBuffer::chunkSize);
        nr = bb.read(buf, 10, BigBuffer::chunkSize - 5);
        assert(nr == 5);
        assert(memcmp(buf, empty, nr) == 0);
    }

    // read (size > chunkSize)
    {
        int n = BigBuffer::chunkSize * 3 + 15;
        char buf[n];
        char empty[n];
        memset(empty, 0, n);
        int nr;
        BigBuffer bb;

        nr = bb.read(buf, n, 0);
        assert(nr == 0);

        nr = bb.read(buf, n, 100);
        assert(nr == 0);

        bb.truncate(10);
        nr = bb.read(buf, 10, 0);
        assert(nr == 10);
        assert(memcmp(buf, empty, nr) == 0);

        bb.truncate(BigBuffer::chunkSize);
        nr = bb.read(buf, n, BigBuffer::chunkSize - 5);
        assert(nr == 5);
        assert(memcmp(buf, empty, nr) == 0);

        bb.truncate(BigBuffer::chunkSize * 2 - 12);
        nr = bb.read(buf, n, 1);
        assert(nr == BigBuffer::chunkSize * 2 - 12 - 1);
        assert(memcmp(buf, empty, nr) == 0);

        bb.truncate(BigBuffer::chunkSize * 10);
        nr = bb.read(buf, n, 1);
        assert(nr == n);
        assert(memcmp(buf, empty, nr) == 0);
    }

    return EXIT_SUCCESS;
}


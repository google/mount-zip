#include "../config.h"

#include <zip.h>
#include <assert.h>
#include <stdlib.h>
#include <cstring>

// Public Morozoff design pattern :)
#define private public

#include "bigBuffer.h"
#include "common.h"

// libzip stub structures
struct zip {};
struct zip_file {};
struct zip_source {};

// libzip stub functions

struct zip *zip_open(const char *, int, int *) {
    assert(false);
    return NULL;
}

int zip_error_to_str(char *, size_t, int, int) {
    assert(false);
    return 0;
}

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

////////////////////////////////////////////////////////////////////////////
// TESTS
////////////////////////////////////////////////////////////////////////////

void chunkLocators() {
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
}

void createDelete() {
    BigBuffer bb;
    assert(bb.len == 0);
}

void truncate() {
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

void readFile() {
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
void readFileOverChunkSize() {
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

// read data created by truncate
void truncateRead() {
    char buf[BigBuffer::chunkSize];
    char empty[BigBuffer::chunkSize];
    memset(empty, 0, BigBuffer::chunkSize);
    BigBuffer b;
    b.truncate(BigBuffer::chunkSize);
    assert(b.len == BigBuffer::chunkSize);
    int nr = b.read(buf, BigBuffer::chunkSize, 0);
    assert((unsigned)nr == BigBuffer::chunkSize);
    assert(memcmp(buf, empty, BigBuffer::chunkSize) == 0);
}

// writing to file
void writeFile() {
    char buf[0xff];
    char buf2[0xff];
    int nr, nw;
    BigBuffer bb;

    nw = bb.write(buf, 0, 0);
    assert(nw == 0);
    assert(bb.len == 0);

    memset(buf, 1, 10);
    memset(buf+10, 2, 10);
    nw = bb.write(buf, 20, 0);
    assert(nw == 20);
    assert(bb.len == 20);
    nr = bb.read(buf2, 30, 0);
    assert(nr == 20);
    assert(memcmp(buf, buf2, 20) == 0);

    bb.truncate(0);
    nw = bb.write(buf, 20, 0);
    assert(nw == 20);
    assert(bb.len == 20);
    nr = bb.read(buf2, 20, 10);
    assert(nr == 10);
    assert(memcmp(buf + 10, buf2, 10) == 0);
}

// read data from file expanded by write
void readExpanded() {
    int n = BigBuffer::chunkSize * 2;
    char buf[n];
    char expected[n];
    memset(expected, 0, n);
    BigBuffer b;

    memset(buf, 'a', 10);
    memset(expected, 'a', 10);
    b.write(buf, 10, 0);
    assert(b.len == 10);

    memset(buf, 'z', 10);
    memset(expected + BigBuffer::chunkSize + 10, 'z', 10);
    b.write(buf, 10, BigBuffer::chunkSize + 10);
    assert(b.len == BigBuffer::chunkSize + 20);

    int nr = b.read(buf, n, 0);
    assert((unsigned)nr == BigBuffer::chunkSize + 20);
    assert(memcmp(buf, expected, nr) == 0);
}

// Test zip user function callback with empty file
void zipUserFunctionCallBackEmpty() {
    BigBuffer bb;
    struct BigBuffer::CallBackStruct *cbs = new BigBuffer::CallBackStruct();
    cbs->buf = &bb;
    cbs->mtime = 12345;

    struct zip_stat stat;
    assert(BigBuffer::zipUserFunctionCallback(cbs, &stat, 0, ZIP_SOURCE_STAT)
            == sizeof(struct zip_stat));
    assert(stat.size == 0);
    assert(stat.mtime == 12345);

    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_OPEN)
            == 0);
    char buf[0xff];
    assert(BigBuffer::zipUserFunctionCallback(cbs, buf, 0xff, ZIP_SOURCE_READ)
            == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_CLOSE)
            == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_FREE)
            == 0);
}

// Test zip user function callback with non-empty file
void zipUserFunctionCallBackNonEmpty() {
    int n = BigBuffer::chunkSize*2;
    char buf[n];
    memset(buf, 'f', n);

    BigBuffer bb;
    bb.write(buf, n, 0);

    struct BigBuffer::CallBackStruct *cbs = new BigBuffer::CallBackStruct();
    cbs->buf = &bb;
    cbs->mtime = 0;

    struct zip_stat stat;
    assert(BigBuffer::zipUserFunctionCallback(cbs, &stat, 0, ZIP_SOURCE_STAT)
            == sizeof(struct zip_stat));
    assert(stat.size == n);
    assert(stat.mtime == 0);

    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_OPEN)
            == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, buf, BigBuffer::chunkSize,
                ZIP_SOURCE_READ) == BigBuffer::chunkSize);
    assert(BigBuffer::zipUserFunctionCallback(cbs, buf, BigBuffer::chunkSize,
                ZIP_SOURCE_READ) == BigBuffer::chunkSize);
    assert(BigBuffer::zipUserFunctionCallback(cbs, buf, BigBuffer::chunkSize,
                ZIP_SOURCE_READ) == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_CLOSE)
            == 0);
    assert(BigBuffer::zipUserFunctionCallback(cbs, NULL, 0, ZIP_SOURCE_FREE)
            == 0);
}

int main(int, char **) {
    initTest();

    chunkLocators();
    createDelete();
    truncate();
    readFile();
    readFileOverChunkSize();
    truncateRead();
    writeFile();
    readExpanded();
    zipUserFunctionCallBackEmpty();
    zipUserFunctionCallBackNonEmpty();
    //TODO: read from zip, write to zip

    return EXIT_SUCCESS;
}


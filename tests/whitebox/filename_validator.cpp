#include "../config.h"

#include <fuse.h>
#include <zip.h>
#include <assert.h>
#include <stdlib.h>
#include <string>
#include <stdexcept>

// Public Morozoff design pattern :)
#define private public

#include "fuseZipData.h"
#include "common.h"

// FUSE stub functions

struct fuse_context *fuse_get_context(void) {
    return NULL;
}

// libzip stub structures
struct zip {};
struct zip_file {};
struct zip_source {};

// libzip stub functions
// only stubs

struct zip *zip_open(const char *, int, int *) {
    assert(false);
    return NULL;
}

int zip_error_to_str(char *, zip_uint64_t, int, int) {
    assert(false);
    return 0;
}

const char *zip_get_name(struct zip *, zip_uint64_t, zip_flags_t) {
    assert(false);
    return NULL;
}

zip_int64_t zip_file_add(struct zip *, const char *, struct zip_source *, zip_flags_t) {
    assert(false);
    return 0;
}

zip_int64_t zip_dir_add(struct zip *, const char *, zip_flags_t) {
    assert(false);
    return 0;
}

int zip_close(struct zip *) {
    assert(false);
    return 0;
}

int zip_delete(struct zip *, zip_uint64_t) {
    assert(false);
    return 0;
}

int zip_fclose(struct zip_file *) {
    assert(false);
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

zip_int64_t zip_get_num_entries(struct zip *, zip_flags_t) {
    assert(false);
    return 0;
}

int zip_file_rename(struct zip *, zip_uint64_t, const char *, zip_flags_t) {
    assert(false);
    return 0;
}

int zip_file_replace(struct zip *, zip_uint64_t, struct zip_source *, zip_flags_t) {
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

int zip_stat_index(struct zip *, zip_uint64_t, zip_flags_t, struct zip_stat *) {
    assert(false);
    return 0;
}

const char *zip_strerror(struct zip *) {
    assert(false);
    return NULL;
}

const char *zip_file_strerror(struct zip_file *) {
    assert(false);
    return NULL;
}

void checkException(const char *fname, const char *prefix) {
    bool thrown = false;
    try {
        FuseZipData::validateFileName(fname);
    }
    catch (const std::runtime_error &e) {
        thrown = true;
        assert(strncmp(e.what(), prefix, strlen(prefix)) == 0);
    }
    assert(thrown);
}

int main(int, char **) {
    initTest();

    FuseZipData::validateFileName("normal.name");
    FuseZipData::validateFileName("path/to/normal.name");

    checkException("", "empty file name");
    checkException("/", "absolute paths are not supported");
    checkException("moo//moo", "bad file name (two slashes): ");
    checkException("/absolute/path", "absolute paths are not supported");

    checkException(".", "paths relative to parent directory are not supported");
    checkException("abc/./cde", "paths relative to parent directory are not supported");
    checkException("abc/.", "paths relative to parent directory are not supported");
    FuseZipData::validateFileName(".hidden");
    FuseZipData::validateFileName("path/to/.hidden");
    FuseZipData::validateFileName("path/to/.hidden/dir");

    checkException("..", "paths relative to parent directory are not supported");
    checkException("abc/../cde", "paths relative to parent directory are not supported");
    checkException("abc/..", "paths relative to parent directory are not supported");
    FuseZipData::validateFileName("..superhidden");
    FuseZipData::validateFileName("path/to/..superhidden");
    FuseZipData::validateFileName("path/to/..superhidden/dir");

    return EXIT_SUCCESS;
}


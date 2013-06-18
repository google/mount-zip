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

void checkValidationException(const char *fname, const char *prefix) {
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

void checkConvertException(const char *fname, const char *prefix, bool readonly, bool needPrefix) {
    bool thrown = false;
    try {
        std::string converted;
        FuseZipData::convertFileName(fname, readonly, needPrefix, converted);
    }
    catch (const std::runtime_error &e) {
        thrown = true;
        assert(strncmp(e.what(), prefix, strlen(prefix)) == 0);
    }
    assert(thrown);
}

void checkConversion(const char *fname, bool readonly, bool needPrefix, const char *expected) {
    std::string res;
    FuseZipData::convertFileName(fname, readonly, needPrefix, res);
    assert(res == expected);
}

int main(int, char **) {
    initTest();

    // validator
    FuseZipData::validateFileName("normal.name");
    FuseZipData::validateFileName("path/to/normal.name");

    FuseZipData::validateFileName(".hidden");
    FuseZipData::validateFileName("path/to/.hidden");
    FuseZipData::validateFileName("path/to/.hidden/dir");

    FuseZipData::validateFileName("..superhidden");
    FuseZipData::validateFileName("path/to/..superhidden");
    FuseZipData::validateFileName("path/to/..superhidden/dir");

    checkValidationException("", "empty file name");
    checkValidationException("moo//moo", "bad file name (two slashes): ");

    // converter
    checkConversion("normal.name", true, false, "normal.name");
    checkConversion("normal.name", true, true, "CUR/normal.name");
    checkConversion("path/to/normal.name", true, false, "path/to/normal.name");
    checkConversion("path/to/normal.name", true, true, "CUR/path/to/normal.name");

    checkConvertException(".", "bad file name: ", true, false);
    checkConvertException("./", "bad file name: ", true, false);
    checkConvertException("abc/./cde", "bad file name: ", true, false);
    checkConvertException("abc/.", "bad file name: ", true, false);

    checkConversion(".hidden", false, false, ".hidden");
    checkConversion("path/to/.hidden", false, false, "path/to/.hidden");
    checkConversion("path/to/.hidden/dir", false, false, "path/to/.hidden/dir");

    checkConvertException(".", "bad file name: .", false, true);
    checkConvertException(".", "bad file name: .", true, true);
    checkConvertException("/.", "bad file name: /.", true, true);
    checkConvertException("./", "bad file name: ./", false, false);
    checkConvertException("./", "bad file name: ./", true, false);

    checkConvertException("..", "bad file name: ..", false, true);
    checkConvertException("../", "paths relative to parent directory are not supported", false, true);
    checkConversion("../", true, true, "UP/");
    checkConversion("../../../", true, true, "UPUPUP/");

    checkConvertException("/..", "bad file name: /..", true, true);
    checkConvertException("/../blah", "bad file name: /../blah", true, true);

    checkConversion("../abc", true, true, "UP/abc");
    checkConversion("../../../abc", true, true, "UPUPUP/abc");

    checkConvertException("abc/../cde", "bad file name: ", false, false);
    checkConvertException("abc/../cde", "bad file name: ", true, true);
    checkConvertException("abc/..", "bad file name: ", false, false);
    checkConvertException("abc/..", "bad file name: ", true, true);
    checkConvertException("../abc/..", "bad file name: ", true, true);

    checkConvertException("/", "absolute paths are not supported in read-write mode", false, false);
    checkConvertException("/rootname", "absolute paths are not supported in read-write mode", false, false);

    checkConversion("/", true, true, "ROOT/");
    checkConversion("/rootname", true, true, "ROOT/rootname");
    checkConversion("/path/name", true, true, "ROOT/path/name");

    return EXIT_SUCCESS;
}


#include <fuse.h>
#include <zip.h>
#include <assert.h>
#include <stdlib.h>

#include "fuse-zip.h"
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

int main(int, char **argv) {
    initTest();

    FuseZipData *data = initFuseZip(argv[0], "test.zip");
    assert(data == NULL);

    return EXIT_SUCCESS;
}


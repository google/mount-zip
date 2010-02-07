#include <zip.h>
#include <syslog.h>

#include <cstring>

void zip_stat_init(struct zip_stat *sb) {
    memset(sb, 0, sizeof(struct zip_stat));
}

void initTest() {
    // hide almost all messages
    setlogmask(LOG_MASK(LOG_EMERG));
}

// Unused functions. Not called inside main program.

void zip_error_clear(struct zip *) {
    assert(false);
}

void zip_error_get(struct zip *, int *, int *) {
    assert(false);
}

int zip_error_get_sys_type(int) {
    assert(false);
    return 0;
}

void zip_file_error_clear(struct zip_file *) {
    assert(false);
}

void zip_file_error_get(struct zip_file *, int *, int *) {
    assert(false);
}

const char *zip_file_strerror(struct zip_file *) {
    assert(false);
    return NULL;
}

struct zip_file *zip_fopen(struct zip *, const char *, int) {
    assert(false);
    return NULL;
}

const char *zip_get_archive_comment(struct zip *, int *, int) {
    assert(false);
    return NULL;
}

int zip_get_archive_flag(struct zip *, int, int) {
    assert(false);
    return 0;
}

const char *zip_get_file_comment(struct zip *, int, int *, int) {
    assert(false);
    return NULL;
}

const char *zip_get_name(struct zip *, int, int) {
    assert(false);
    return NULL;
}

int zip_name_locate(struct zip *, const char *, int) {
    assert(false);
    return 0;
}

int zip_set_archive_comment(struct zip *, const char *, int) {
    assert(false);
    return 0;
}

int zip_set_archive_flag(struct zip *, int, int) {
    assert(false);
    return 0;
}

int zip_set_file_comment(struct zip *, int, const char *, int) {
    assert(false);
    return 0;
}

struct zip_source *zip_source_buffer(struct zip *, const void *, off_t, int) {
    assert(false);
    return NULL;
}

struct zip_source *zip_source_file(struct zip *, const char *, off_t, off_t) {
    assert(false);
    return NULL;
}

struct zip_source *zip_source_filep(struct zip *, FILE *, off_t, off_t) {
    assert(false);
    return NULL;
}

struct zip_source *zip_source_zip(struct zip *, struct zip *, int, int, off_t, off_t) {
    assert(false);
    return NULL;
}

int zip_stat(struct zip *, const char *, int, struct zip_stat *) {
    assert(false);
    return 0;
}

int zip_unchange(struct zip *, int) {
    assert(false);
    return 0;
}

int zip_unchange_all(struct zip *) {
    assert(false);
    return 0;
}

int zip_unchange_archive(struct zip *) {
    assert(false);
    return 0;
}


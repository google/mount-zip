////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2013-2021 by Alexander Galanin                          //
//  al@galanin.nnov.ru                                                    //
//  http://galanin.nnov.ru/~al                                            //
//                                                                        //
//  This program is free software: you can redistribute it and/or modify  //
//  it under the terms of the GNU General Public License as published by  //
//  the Free Software Foundation, either version 3 of the License, or     //
//  (at your option) any later version.                                   //
//                                                                        //
//  This program is distributed in the hope that it will be useful,       //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//  GNU General Public License for more details.                          //
//                                                                        //
//  You should have received a copy of the GNU General Public License     //
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.//
////////////////////////////////////////////////////////////////////////////

#include "../config.h"

#include <fuse.h>
#include <zip.h>
#include <assert.h>
#include <stdlib.h>
#include <string>
#include <stdexcept>

#include "fuse-zip.h"
#include "fuseZipData.h"
#include "common.h"

// FUSE stub functions

struct fuse_context *fuse_get_context(void) {
    return NULL;
}

// libzip stub structures
struct zip {
    std::string filename;
    zip_int64_t count;
};
struct zip_file {};
struct zip_source {};

// libzip stub functions

struct zip *zip_open(const char *, int, int *errorp) {
    *errorp = 0;
    return NULL;
}

int zip_error_to_str(char *buf, zip_uint64_t len, int, int) {
    return strncpy(buf, "Expected error", len) - buf;
}

zip_int64_t zip_get_num_entries(struct zip *z, zip_flags_t) {
    return z->count;
}

const char *zip_get_name(struct zip *z, zip_uint64_t, zip_flags_t) {
    return z->filename.c_str();
}

int zip_stat_index(struct zip *z, zip_uint64_t index, zip_flags_t,
        struct zip_stat *zs) {
    zs->valid = ZIP_STAT_NAME | ZIP_STAT_INDEX | ZIP_STAT_SIZE |
        ZIP_STAT_COMP_SIZE | ZIP_STAT_MTIME | ZIP_STAT_CRC |
        ZIP_STAT_COMP_METHOD | ZIP_STAT_ENCRYPTION_METHOD | ZIP_STAT_FLAGS;
    zs->name = z->filename.c_str();
    zs->index = index;
    zs->size = 0;
    zs->comp_size = 0;
    zs->mtime = 0;
    zs->crc = 0;
    zs->comp_method = ZIP_CM_STORE;
    zs->encryption_method = ZIP_EM_NONE;
    zs->flags = 0;
    return 0;
}

int zip_close(struct zip *) {
    return 0;
}


// only stubs

zip_int64_t zip_file_add(struct zip *, const char *, struct zip_source *, zip_flags_t) {
    assert(false);
    return 0;
}

zip_int64_t zip_dir_add(struct zip *, const char *, zip_flags_t) {
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

const char *zip_strerror(struct zip *) {
    assert(false);
    return NULL;
}

const char *zip_file_strerror(struct zip_file *) {
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

// test functions
void duplicateFileNames() {
    struct zip z;
    z.filename = "same_file.name";
    z.count = 2;
    FuseZipData zd("test.zip", &z, "/tmp", false);
    bool thrown = false;
    try {
        zd.build_tree(false);
    }
    catch (const std::runtime_error &e) {
        thrown = true;
        assert(strcmp(e.what(), "duplicate file names") == 0);
    }
    assert(thrown);
}

void relativePathsReadWrite() {
    struct zip z;
    z.filename = "../file.name";
    z.count = 1;
    FuseZipData zd("test.zip", &z, "/tmp", false);
    bool thrown = false;
    try {
        zd.build_tree(false);
    }
    catch (const std::runtime_error &e) {
        thrown = true;
        assert(strcmp(e.what(), "paths relative to parent directory are not supported in read-write mode") == 0);
    }
    assert(thrown);
}

void absolutePathsReadWrite() {
    struct zip z;
    z.filename = "/file.name";
    z.count = 1;
    FuseZipData zd("test.zip", &z, "/tmp", false);
    bool thrown = false;
    try {
        zd.build_tree(false);
    }
    catch (const std::runtime_error &e) {
        thrown = true;
        assert(strcmp(e.what(), "absolute paths are not supported in read-write mode") == 0);
    }
    assert(thrown);
}

void relativePathsReadOnly() {
    struct zip z;
    z.filename = "../file.name";
    z.count = 1;
    FuseZipData zd("test.zip", &z, "/tmp", false);
    zd.build_tree(true);
}

void absolutePathsReadOnly() {
    struct zip z;
    z.filename = "/file.name";
    z.count = 1;
    FuseZipData zd("test.zip", &z, "/tmp", false);
    zd.build_tree(true);
}

int main(int, char **) {
    initTest();

    duplicateFileNames();
    relativePathsReadOnly();
    absolutePathsReadOnly();
    relativePathsReadWrite();
    absolutePathsReadWrite();

    return EXIT_SUCCESS;
}


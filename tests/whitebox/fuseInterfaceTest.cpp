#include "../config.h"

#include <zip.h>
#include <fuse.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <memory>

// Public Morozoff design pattern :)
#define private public
#define protected public

#include "fuse-zip.h"
#include "fileNode.h"
#include "fuseZipData.h"
#include "common.h"

using namespace std;

// libzip stubs

struct zip {
};
struct zip_file {
};
struct zip_source {
};

zip *zip_open(const char *, int, int *) {
    return new zip;
}

int zip_close(zip_t *zip) {
    delete zip;
    return 0;
}

int zip_delete(zip_t *, zip_uint64_t) {
    assert(false);
    return 0;
}

int zip_stat_index(struct zip *, zip_uint64_t, zip_flags_t, struct zip_stat *) {
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

int zip_fclose(struct zip_file *) {
    assert(false);
    return 0;
}

zip_int64_t zip_file_add(struct zip *, const char *, struct zip_source *, zip_flags_t) {
    return 0;
}

zip_int64_t zip_dir_add(zip_t *, const char *, zip_flags_t) {
    assert(false);
    return 0;
}

int zip_file_rename(zip_t *, zip_uint64_t, const char *, zip_flags_t) {
    assert(false);
    return 0;
}

int zip_file_replace(struct zip *, zip_uint64_t, struct zip_source *, zip_flags_t) {
    assert(false);
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

zip_int64_t zip_get_num_entries(zip_t *, zip_flags_t) {
    return 0;
}

void zip_error_init_with_code(zip_error_t *, int) {
    assert(false);
}

const char *zip_error_strerror(zip_error_t *) {
    assert(false);
    return NULL;
}

void zip_error_fini(zip_error_t *) {
    assert(false);
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

// FUSE stubs

fuse_context context;

struct fuse_context *fuse_get_context(void) {
    return &context;
}

// tests

/**
 * Test fisezip_create()
 */
void test_create () {
    FuseZipData *data = initFuseZip("testprogram", "test.zip", false, false);
    context.private_data = data;
    fuse_conn_info conn;
    void *initResult = fusezip_init(&conn);
    assert(initResult == data);

    fuse_file_info fi;

    // empty path
    assert(fusezip_create("", 0, &fi) == -EACCES);
    // create new
    assert(fusezip_create("/test", 0, &fi) == 0);
    assert(reinterpret_cast<void*>(fi.fh) != NULL);
    assert(std::string("test") == reinterpret_cast<FileNode*>(fi.fh)->name);
    // create twice
    assert(fusezip_create("/test", 0, &fi) == -EEXIST);

    // unlink created file
    assert(fusezip_unlink("/test") == 0);

    fusezip_destroy(data);
}

/**
 * Test fisezip_mknod()
 */
void test_mknod () {
    FuseZipData *data = initFuseZip("testprogram", "test.zip", false, false);
    context.private_data = data;
    fuse_conn_info conn;
    void *initResult = fusezip_init(&conn);
    assert(initResult == data);

    // empty path
    assert(fusezip_mknod("", 0, 0) == -EACCES);
    {
        // character device
        dev_t dev = makedev(4, 63);
        assert(fusezip_mknod("/char", S_IFCHR, dev) == 0);
        FileNode *node = data->find("char");
        assert(node != NULL);
        assert(S_ISCHR(node->mode()));
        assert(node->device() == dev);

        assert(fusezip_unlink("/char") == 0);
    }
    {
        // block device
        dev_t dev = makedev(8, 255);
        assert(fusezip_mknod("/block", S_IFBLK, dev) == 0);
        FileNode *node = data->find("block");
        assert(node != NULL);
        assert(S_ISBLK(node->mode()));
        assert(node->device() == dev);

        assert(fusezip_unlink("/block") == 0);
    }
    {
        // FIFO
        assert(fusezip_mknod("/fifo", S_IFIFO, 0) == 0);
        FileNode *node = data->find("fifo");
        assert(node != NULL);
        assert(S_ISFIFO(node->mode()));
        assert(node->size() == 0);

        assert(fusezip_unlink("/fifo") == 0);
    }
    {
        // socket
        assert(fusezip_mknod("/socket", S_IFSOCK, 0) == 0);
        FileNode *node = data->find("socket");
        assert(node != NULL);
        assert(S_ISSOCK(node->mode()));

        assert(fusezip_unlink("/socket") == 0);
    }

    {
        // create new
        assert(fusezip_mknod("/test", 0, 0) == 0);
        FileNode *node = data->find("test");
        assert(node != NULL);
        assert(std::string("test") == node->name);
        // create twice
        assert(fusezip_mknod("/test", 0, 0) == -EEXIST);

        // unlink created file
        assert(fusezip_unlink("/test") == 0);
    }

    fusezip_destroy(data);
}

int main(int, char **) {
    test_create();
    test_mknod();

    return EXIT_SUCCESS;
}


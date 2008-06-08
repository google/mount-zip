////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008 by Alexander Galanin                               //
//  gaa.nnov@mail.ru                                                      //
//                                                                        //
//  This program is free software; you can redistribute it and/or modify  //
//  it under the terms of the GNU Library General Public License as       //
//  published by the Free Software Foundation; either version 3 of the    //
//  License, or (at your option) any later version.                       //
//                                                                        //
//  This program is distributed in the hope that it will be useful,       //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//  GNU General Public License for more details.                          //
//                                                                        //
//  You should have received a copy of the GNU Library General Public     //
//  License along with this program; if not, write to the                 //
//  Free Software Foundation, Inc.,                                       //
//  51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA               //
////////////////////////////////////////////////////////////////////////////

#define FUSE_USE_VERSION 26
#define PROGRAM "fuse-zip"
#define ERROR_STR_BUF_LEN 0x100

#include <fuse.h>
#include <zip.h>
#include <errno.h>

#include <queue>

#include "types.h"
#include "fileNode.h"
#include "fuseZipData.h"

using namespace std;

static void *fusezip_init(struct fuse_conn_info *conn) {
    (void) conn;
    return fuse_get_context()->private_data;
}

static void fusezip_destroy(void *data) {
    FuseZipData *d = (FuseZipData*)data;
    // Saving changed data
    for (filemap_t::const_iterator i = d->files.begin(); i != d->files.end(); ++i) {
        if (i->second->changed != 0) {
            i->second->save();
        }
    }
    delete d;
}

inline FuseZipData *get_data() {
    return (FuseZipData*)fuse_get_context()->private_data;
}

FileNode *get_file_node(const char *fname) {
    FuseZipData *data = get_data();
    filemap_t::iterator i = data->files.find(fname);
    if (i == data->files.end()) {
        return NULL;
    } else {
        return i->second;
    }
}

inline struct zip *get_zip() {
    return get_data()->m_zip;
}

static int fusezip_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    if (*path == '\0') {
        return -ENOENT;
    }
    FileNode *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    struct zip_stat &zstat = node->stat;
    if (node->is_dir) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2 + node->childs.size();
    } else {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
    }
    stbuf->st_ino = node->id;
    stbuf->st_size = zstat.size;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = zstat.mtime;

    return 0;
}

static int fusezip_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    if (*path == '\0') {
        return -ENOENT;
    }
    FileNode *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    for (nodelist_t::const_iterator i = node->childs.begin(); i != node->childs.end(); ++i) {
        filler(buf, (*i)->name, NULL, 0);
    }

    return 0;
}

static int fusezip_statfs(const char *path, struct statvfs *buf) {
    (void) path;

    buf->f_bsize = 1;
    //TODO: set this field to archive size
    buf->f_blocks = 0;

    //TODO: may be set to amount of free space on file system?
    buf->f_bfree = 0;
    buf->f_bavail = 0;
    buf->f_ffree = 0;

    buf->f_files = get_data()->files.size() - 1;
    buf->f_namemax = 255;

    return 0;
}

static int fusezip_open(const char *path, struct fuse_file_info *fi) {
    if (*path == '\0') {
        return -ENOENT;
    }
    FileNode *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    if (node->is_dir) {
        return -EISDIR;
    }
    fi->fh = (uint64_t)node;

    return node->open();
}

static int fusezip_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) mode;

    if (*path == '\0') {
        return -EACCES;
    }
    FileNode *node = get_file_node(path + 1);
    if (node != NULL) {
        return -EEXIST;
    }
    node = new FileNode(get_data(), path + 1);
    if (!node) {
        return -ENOMEM;
    }
    fi->fh = (uint64_t)node;

    return node->open();
}

static int fusezip_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) path;

    return ((FileNode*)fi->fh)->read(buf, size, offset);
}

int fusezip_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) path;

    return ((FileNode*)fi->fh)->write(buf, size, offset);
}

static int fusezip_release (const char *path, struct fuse_file_info *fi) {
    (void) path;

    return ((FileNode*)fi->fh)->close();
}

int remove_node(FileNode *node) {
    node->detach();
    int id = node->id;
    delete node;
    if (id >= 0) {
        return (zip_delete (get_zip(), id) == 0)? 0 : -ENOENT;
    } else {
        return 0;
    }
}

static int fusezip_unlink(const char *path) {
    if (*path == '\0') {
        return -ENOENT;
    }
    FileNode *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    if (node->is_dir) {
        return -EISDIR;
    }
    return remove_node(node);
}

static int fusezip_rmdir(const char *path) {
    if (*path == '\0') {
        return -ENOENT;
    }
    FileNode *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    if (!node->is_dir) {
        return -ENOTDIR;
    }
    if (!node->childs.empty()) {
        return -ENOTEMPTY;
    }
    return remove_node(node);
}

static int fusezip_mkdir(const char *path, mode_t mode) {
    (void) mode;
    if (*path == '\0') {
        return -ENOENT;
    }
    int idx = zip_add_dir(get_zip(), path + 1);
    if (idx < 0) {
        return -ENOMEM;
    }
    FileNode *node = new FileNode(get_data(), path + 1, idx);
    if (!node) {
        return -ENOMEM;
    }
    node->is_dir = true;
    return 0;
}

static int fusezip_rename(const char *path, const char *new_path) {
    if (*path == '\0') {
        return -ENOENT;
    }
    FileNode *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    if (*new_path == '\0') {
        return -EINVAL;
    }
    FileNode *new_node = get_file_node(new_path + 1);
    if (new_node != NULL) {
        remove_node(new_node);
    }

    int len = strlen(new_path);
    char *new_name;
    if (!node->is_dir) {
        len--;
    }
    new_name = (char*)malloc(len + 1);
    if (new_path == NULL) {
        return -ENOMEM;
    }
    strcpy(new_name, new_path + 1);
    if (node->is_dir) {
        new_name[len - 1] = '/';
        new_name[len] = '\0';
    }

    struct zip *z = get_zip();
    // Renaming directory and its content recursively
    if (node->is_dir) {
        queue<FileNode*> q;
        q.push(node);
        while (!q.empty()) {
            FileNode *n = q.front();
            q.pop();
            for (nodelist_t::const_iterator i = n->childs.begin(); i != n->childs.end(); ++i) {
                FileNode *nn = *i;
                q.push(nn);
                char *name = (char*)malloc(len + strlen(nn->name) + 1);
                if (name == NULL) {
                    //TODO: check that we are have enough memory before entering this loop
                    return -ENOMEM;
                }
                strcpy(name, new_name);
                strcpy(name + len, nn->name);
                nn->rename_wo_reparenting(name);
                zip_rename(z, nn->id, name);
            }
        }
    }
    zip_rename(z, node->id, new_name);
    // Must be called after loop because new_name will be truncated
    node->rename(new_name);
    return 0;
}

void print_usage() {
    printf("USAGE: %s <zip-file> [fusermount options]\n", PROGRAM);
}

int main(int argc, char *argv[]) {
    if (sizeof(void*) > sizeof(uint64_t)) {
        fprintf(stderr,"%s: This program cannot be run on your system because of FUSE design limitation\n", PROGRAM);
        return EXIT_FAILURE;
    }
    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    int err;
    struct zip *zip_file;
    if ((zip_file = zip_open(argv[1], ZIP_CHECKCONS | ZIP_CREATE, &err)) == NULL) {
        char err_str[ERROR_STR_BUF_LEN];
        zip_error_to_str(err_str, ERROR_STR_BUF_LEN, err, errno);
        fprintf(stderr, "%s: cannot open zip archive %s: %s\n", PROGRAM, argv[1], err_str);
        return EXIT_FAILURE;
    }
    try {
        FuseZipData *data = new FuseZipData(zip_file);
    }
    catch (std::bad_alloc) {
      fprintf(stderr, "%s: no enough memory\n", PROGRAM);
      return EXIT_FAILURE;
    }
    catch (std::exception) {
        fprintf(stderr, "%s: ZIP file corrupted\n", PROGRAM);
        return EXIT_FAILURE;
    }

    static struct fuse_operations fusezip_oper;
    fusezip_oper.init       =   fusezip_init;
    fusezip_oper.destroy    =   fusezip_destroy;
    fusezip_oper.readdir    =   fusezip_readdir;
    fusezip_oper.getattr    =   fusezip_getattr;
    fusezip_oper.statfs     =   fusezip_statfs;
    fusezip_oper.open       =   fusezip_open;
    fusezip_oper.read       =   fusezip_read;
    fusezip_oper.write      =   fusezip_write;
    fusezip_oper.release    =   fusezip_release;
    fusezip_oper.unlink     =   fusezip_unlink;
    fusezip_oper.rmdir      =   fusezip_rmdir;
    fusezip_oper.mkdir      =   fusezip_mkdir;
    fusezip_oper.rename     =   fusezip_rename;
    fusezip_oper.create     =   fusezip_create;

// We cannot use fuse_main to initialize FUSE because libzip are have problems with thread safety.
// return fuse_main(argc - 1, argv + 1, &fusezip_oper, zip_file);

    struct fuse *fuse;
    char *mountpoint;
    int multithreaded;
    int res;

    fuse = fuse_setup(argc - 1, argv + 1, &fusezip_oper, sizeof(fusezip_oper), &mountpoint, &multithreaded, data);
    if (fuse == NULL) {
        return EXIT_FAILURE;
    }
    res = fuse_loop(fuse);
    fuse_teardown(fuse, mountpoint);
    return (res == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

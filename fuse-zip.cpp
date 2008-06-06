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
#define ROOT_NODE_INDEX (-1)

#include <fuse.h>
#include <zip.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include <map>
#include <list>
#include <queue>

#include "types.h"
#include "fileNode.h"
#include "fileHandler.h"
#include "roHandler.h"

using namespace std;

class fusezip_data {
public:
    filemap_t files;
    struct zip *m_zip;

    fusezip_data(struct zip *z) {
        m_zip = z;
        build_tree();
    }

    ~fusezip_data() {
        zip_close(m_zip);
        //TODO: handle error code of zip_close

        for (filemap_t::iterator i = files.begin(); i != files.end(); ++i) {
            delete i->second;
        }
    }

private:
    void build_tree() {
        FileNode *root_node = new FileNode(files, ROOT_NODE_INDEX, "");
        root_node->is_dir = true;

        int n = zip_get_num_files(m_zip);
        for (int i = 0; i < n; ++i) {
            FileNode *node = new FileNode(files, i, zip_get_name(m_zip, i, 0));
            (void) node;
        }
    }
};

static void *fusezip_init(struct fuse_conn_info *conn) {
    return fuse_get_context()->private_data;
}

static void fusezip_destroy(void *data) {
    delete (fusezip_data*)data;
}

inline fusezip_data *get_data() {
    return (fusezip_data*)fuse_get_context()->private_data;
}

FileNode *get_file_node(const char *fname) {
    fusezip_data *data = get_data();
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
    struct zip_stat zstat;
    if (node->id != ROOT_NODE_INDEX && zip_stat_index(get_zip(), node->id, 0, &zstat) != 0) {
        int err;
        zip_error_get(get_zip(), NULL, &err);
        return err;
    }
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

    //TODO: change in rw version
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
    //TODO: change in rw version
    if ((fi->flags & (O_WRONLY | O_RDWR)) != 0) {
        return -EROFS;
    }
   
    FileNode *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    if (node->is_dir) {
        return -EISDIR;
    }
    if (node->open_count == INT_MAX) {
        return -EMFILE;
    }
    if (node->open_count++) {
        // Reusing existing file handle
        fi->fh = (uint64_t)node->fh;
        return 0;
    }
    struct zip_file *f = zip_fopen_index(get_zip(), node->id, fi->flags);
    if (f == NULL) {
        int err;
        zip_error_get(get_zip(), NULL, &err);
        return err;
    }
    FileHandler *fh = new RoHandler(get_zip(), f, node);
    fi->fh = (uint64_t)fh;

    return 0;
}

static int fusezip_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    FileHandler *fh = (FileHandler*)fi->fh;
    return fh->read(buf, size, offset, fi);
}

int fusezip_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    FileHandler *fh = (FileHandler*)fi->fh;
    return fh->write(buf, size, offset, fi);
}

static int fusezip_release (const char *path, struct fuse_file_info *fi) {
    struct FileHandler *fh = (FileHandler*)fi->fh;
    int res = fh->close();
    delete fh;
    return res;
}

int remove_node(FileNode *node) {
    node->detach(get_data()->files);

    int id = node->id;
    delete node;
    if (zip_delete (get_zip(), id) != 0) {
        return -ENOENT;
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
        int err;
        zip_error_get(get_zip(), NULL, &err);
        return err;
    }
    FileNode *node = new FileNode(get_data()->files, idx, path + 1);
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
                strcpy(name, new_name);
                strcpy(name + len, nn->name);
                nn->rename_wo_reparenting(name);
                int res = zip_rename(z, nn->id, name);
                assert(res == 0);
            }
        }
    }
    zip_rename(z, node->id, new_name);
    // Must be called after loop because new_name will be truncated
    node->rename(get_data()->files, new_name);
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
    fusezip_data *data = new fusezip_data(zip_file);

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


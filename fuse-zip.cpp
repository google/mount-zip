////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008 by Alexander Galanin                               //
//  gaa.nnov@mail.ru                                                      //
//                                                                        //
//  This program is free software; you can redistribute it and/or modify  //
//  it under the terms of the GNU Library General Public License as       //
//  published by the Free Software Foundation; either version 2 of the    //
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
#define SKIP_BUFFER_LENGTH (1024*1024)

#include <fuse.h>
#include <zip.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include <map>
#include <list>

using namespace std;

struct ltstr {
    bool operator() (const char* s1, const char* s2) const {
        return strcmp(s1, s2) < 0;
    }
};

struct file_handle;

class fusezip_node {
public:
    fusezip_node(int id, char *full_name) {
        this->id = id;
        this->full_name = full_name;
        this->name = full_name;
        this->is_dir = false;
        this->open_count = 0;
    }

    ~fusezip_node() {
        free(full_name);
    }

    char *name, *full_name;
    bool is_dir;
    int id;
    list<fusezip_node*> childs;
    int open_count;
    file_handle *fh;
};

typedef map <const char*, fusezip_node*, ltstr> filemap_t; 

class fusezip_data {
private:
    inline void insert(fusezip_node *node) {
        files[node->full_name] = node;
    }
public:
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

    //TODO: move tree building into main()
    void build_tree() {
        fusezip_node *root_node = new fusezip_node(-1, strdup(""));
        root_node->is_dir = true;
        insert(root_node);

        int n = zip_get_num_files(m_zip);
        for (int i = 0; i < n; ++i) {
            char *fullname = strdup(zip_get_name(m_zip, i, 0));
            if (fullname == NULL) {
                //TODO: may be throw error?
                continue;
            }

            fusezip_node *node = new fusezip_node(i, fullname);
            char *lsl = fullname;
            while (*lsl++) {}
            lsl--;
            while (lsl > fullname && *lsl != '/') {
                lsl--;
            }
            // If the last symbol in file name is '/' then it is a directory
            if (*(lsl+1) == '\0') {
                // It will produce two \0s at the end of file name. I think that it is not a problem
                *lsl = '\0';
                node->is_dir = true;
                while (lsl > fullname && *lsl != '/') {
                    lsl--;
                }
            }

            // Adding new child to parent node. For items without '/' in fname it will be root_node.
            char c = *lsl;
            *lsl = '\0';
            filemap_t::iterator parent = files.find(fullname);
            assert(parent != files.end());
            parent->second->childs.push_back(node);
            *lsl = c;
            
            // Setting short name of node
            if (c == '/') {
                lsl++;
            }
            node->name = lsl;

            insert(node);
        }
    }

    filemap_t files;
    struct zip *m_zip;
};

struct file_handle {
    struct zip_file *zf;
    off_t pos;
    fusezip_node *node;
};

static void *fusezip_init(struct fuse_conn_info *conn) {
    struct fuse_context *context = fuse_get_context();
    struct zip *z = (struct zip*)context->private_data;

    fusezip_data *data = new fusezip_data(z);
    return data;
}

static void fusezip_destroy(void *v_data) {
    fusezip_data *data = (fusezip_data*)v_data;
    delete data;
}

inline fusezip_data *get_data() {
    return (fusezip_data*)fuse_get_context()->private_data;
}

fusezip_node *get_file_node(const char *fname) {
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
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));
    if (*path == '\0') {
        return -ENOENT;
    }
    fusezip_node *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    struct zip_stat zstat;
    if (node->id != -1 && zip_stat_index(get_zip(), node->id, 0, &zstat) != 0) {
        int err;
        zip_error_get(get_zip(), NULL, &err);
        return err;
    }
    if (node->is_dir) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2 + node->childs.size();
    } else {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
    }
    stbuf->st_ino = node->id;
    stbuf->st_size = zstat.size;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = zstat.mtime;

    return res;
}

static int fusezip_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    if (*path == '\0') {
        return -ENOENT;
    }
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    
    fusezip_node *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    for (list<fusezip_node*>::const_iterator i = node->childs.begin(); i != node->childs.end(); ++i) {
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
   
    fusezip_node *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    if (node->open_count == INT_MAX) {
        return -EMFILE;
    }
    if (node->open_count++) {
        fi->fh = (uint64_t)node->fh;
        return 0;
    }
    struct zip_file *f = zip_fopen_index(get_zip(), node->id, fi->flags);
    if (f == NULL) {
        int err;
        zip_error_get(get_zip(), NULL, &err);
        return err;
    }
    struct file_handle *fh = new file_handle();
    fh->zf = f;
    fh->pos = 0;
    fi->fh = (uint64_t)fh;
    fh->node = node;

    return 0;
}

static int fusezip_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct file_handle *fh = (file_handle*)fi->fh;
    struct zip_file *f = fh->zf;

    if (fh->pos > offset) {
        // It is terrible, but there are no other way to rewind file :(
        int err;
        err = zip_fclose(f);
        if (err != 0) {
            return -EIO;
        }
        struct zip_file *f = zip_fopen(get_zip(), path + 1, fi->flags);
        if (f == NULL) {
            int err;
            zip_error_get(get_zip(), NULL, &err);
            return err;
        }
        fh->zf = f;
        fh->pos = 0;
    }
    // skipping to offset ...
    ssize_t count = offset - fh->pos;
    while (count > 0) {
        static char bb[SKIP_BUFFER_LENGTH];
        ssize_t r = SKIP_BUFFER_LENGTH;
        if (r > count) {
            r = count;
        }
        ssize_t rr = zip_fread(f, bb, r);
        if (rr < 0) {
            return -EIO;
        }
        count -= rr;
    }

    ssize_t nr = zip_fread(f, buf, size);
    fh->pos += nr;
    return nr;
}

static int fusezip_release (const char *path, struct fuse_file_info *fi) {
    struct file_handle *fh = (file_handle*)fi->fh;
    struct zip_file *f = fh->zf;
    //TODO: handle error
    zip_fclose(f);
    fh->node->open_count--;
    delete fh;
    return 0;
}

void print_usage() {
    printf("USAGE: %s <zip-file> [fusermount options]\n", PROGRAM);
}

int main(int argc, char *argv[]) {
    //TODO: think about workaround
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
    //TODO: add ZIP_CREATE in rw version
    if ((zip_file = zip_open(argv[1], ZIP_CHECKCONS, &err)) == NULL) {
        char err_str[ERROR_STR_BUF_LEN];
        zip_error_to_str(err_str, ERROR_STR_BUF_LEN, err, errno);
        fprintf(stderr, "%s: cannot open zip archive %s: %s\n", PROGRAM, argv[1], err_str);
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
    fusezip_oper.release    =   fusezip_release;

// We cannot use fuse_main to initialize FUSE because libzip are have problems with thread safety.
// return fuse_main(argc - 1, argv + 1, &fusezip_oper, zip_file);

    struct fuse *fuse;
    char *mountpoint;
    int multithreaded;
    int res;

    fuse = fuse_setup(argc - 1, argv + 1, &fusezip_oper, sizeof(fusezip_oper), &mountpoint, &multithreaded, zip_file);
    if (fuse == NULL) {
        return 1;
    }

    res = fuse_loop(fuse);

    fuse_teardown(fuse, mountpoint);
    if (res == -1) {
        return 1;
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2010 by Alexander Galanin                          //
//  al@galanin.nnov.ru                                                    //
//  http://galanin.nnov.ru/~al                                            //
//                                                                        //
//  This program is free software; you can redistribute it and/or modify  //
//  it under the terms of the GNU Lesser General Public License as        //
//  published by the Free Software Foundation; either version 3 of the    //
//  License, or (at your option) any later version.                       //
//                                                                        //
//  This program is distributed in the hope that it will be useful,       //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//  GNU General Public License for more details.                          //
//                                                                        //
//  You should have received a copy of the GNU Lesser General Public      //
//  License along with this program; if not, write to the                 //
//  Free Software Foundation, Inc.,                                       //
//  51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA               //
////////////////////////////////////////////////////////////////////////////

#define FUSE_USE_VERSION 27
#define PROGRAM "fuse-zip"
#define VERSION "0.2.11"
#define ERROR_STR_BUF_LEN 0x100
#define STANDARD_BLOCK_SIZE (512)

#define KEY_HELP (0)
#define KEY_VERSION (1)

#include <fuse.h>
#include <fuse_opt.h>
#include <zip.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <sys/xattr.h>
#include <sys/types.h>
#include <sys/statvfs.h>

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <queue>

#include "types.h"
#include "fileNode.h"
#include "fuseZipData.h"
#include "libZipWrapper.h"

using namespace std;

/**
 * Initialize filesystem
 *
 * Report current working dir and archive file name to syslog.
 *
 * @return filesystem-private data
 */
static void *fusezip_init(struct fuse_conn_info *conn) {
    (void) conn;
    FuseZipData *data = (FuseZipData*)fuse_get_context()->private_data;
    syslog(LOG_INFO, "Mounting file system on %s (cwd=%s)", data->m_archiveName, data->m_cwd);
    return data;
}

/**
 * Destroy filesystem
 *
 * Save all modified data back to ZIP archive and report to syslog about completion.
 * Note that filesystem unmounted before this method finishes
 * (see http://code.google.com/p/fuse-zip/issues/detail?id=7).
 */
static void fusezip_destroy(void *data) {
    FuseZipData *d = (FuseZipData*)data;
    // Saving changed data
    for (filemap_t::const_iterator i = d->files.begin(); i != d->files.end(); ++i) {
        if (i->second->isChanged()) {
            int res = i->second->save();
            if (res != 0) {
                syslog(LOG_ERR, "Error while saving file %s in ZIP archive: %d", i->second->full_name, res);
            }
        }
    }
    delete d;
    syslog(LOG_INFO, "File system unmounted");
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
    struct zip_stat_64 zstat;
    zip_stat_assign_64_to_default(&zstat, &node->stat);
    if (node->is_dir) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2 + node->childs.size();
    } else {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
    }
    stbuf->st_blksize = STANDARD_BLOCK_SIZE;
    stbuf->st_ino = node->id;
    stbuf->st_blocks = (node->size() + STANDARD_BLOCK_SIZE - 1) / STANDARD_BLOCK_SIZE;
    stbuf->st_size = node->size();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = zstat.mtime;
    stbuf->st_uid = geteuid();
    stbuf->st_gid = getegid();

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

    // Getting amount of free space in directory with archive
    struct statvfs st;
    int err;
    if ((err = statvfs(get_data()->m_cwd,&st)) != 0) {
        return -err;
    }
    buf->f_bavail = buf->f_bfree = st.f_frsize * st.f_bavail;

    buf->f_bsize = 1;
    //TODO: may be append archive size?
    buf->f_blocks = buf->f_bavail + 0;

    buf->f_ffree = 0;
    buf->f_favail = 0;

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

    try {
        return node->open();
    }
    catch (std::bad_alloc) {
        return -ENOMEM;
    }
    catch (std::exception) {
        return -EIO;
    }
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

static int fusezip_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi) {
    (void) path;

    return ((FileNode*)fi->fh)->truncate(offset);
}

static int fusezip_truncate(const char *path, off_t offset) {
    if (*path == '\0') {
        return -EACCES;
    }
    FileNode *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    if (node->is_dir) {
        return -EISDIR;
    }
    int res;
    if ((res = node->open()) != 0) {
        return res;
    }
    if ((res = node->truncate(offset)) != 0) {
        return res;
    }
    return node->close();
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

    try {
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
    catch (...) {
        return -EIO;
    }
}

static int fusezip_utimens(const char *path, const struct timespec tv[2]) {
    if (*path == '\0') {
        return -ENOENT;
    }
    FileNode *node = get_file_node(path + 1);
    if (node == NULL) {
        return -ENOENT;
    }
    node->stat.mtime = tv[1].tv_sec;
    return 0;
}

#if ( __FreeBSD__ >= 10 )
static int fusezip_setxattr(const char *, const char *, const char *, size_t, int, uint32_t) {
#else
static int fusezip_setxattr(const char *, const char *, const char *, size_t, int) {
#endif
    return -ENOTSUP;
}

#if ( __FreeBSD__ >= 10 )
static int fusezip_getxattr(const char *, const char *, char *, size_t, uint32_t) {
#else
static int fusezip_getxattr(const char *, const char *, char *, size_t) {
#endif
    return -ENOTSUP;
}

static int fusezip_listxattr(const char *, char *, size_t) {
    return -ENOTSUP;
}

static int fusezip_removexattr(const char *, const char *) {
    return -ENOTSUP;
}

static int fusezip_chmod(const char *, mode_t) {
    return 0;
}

static int fusezip_chown(const char *, uid_t, gid_t) {
    return 0;
}

static int fusezip_flush(const char *, struct fuse_file_info *) {
    return 0;
}

static int fusezip_fsync(const char *, int, struct fuse_file_info *) {
    return 0;
}

static int fusezip_fsyncdir(const char *, int, struct fuse_file_info *) {
    return 0;
}

static int fusezip_opendir(const char *, struct fuse_file_info *) {
  return 0;
}

static int fusezip_releasedir(const char *, struct fuse_file_info *) {
    return 0;
}

static int fusezip_access(const char *, int) {
    return 0;
}

/**
 * Print usage information
 */
void print_usage() {
    fprintf(stderr, "usage: %s [options] <zip-file> <mountpoint>\n\n", PROGRAM);
    fprintf(stderr,
            "general options:\n"
            "    -o opt,[opt...]        mount options\n"
            "    -h   --help            print help\n"
            "    -V   --version         print version\n"
            "    -r   -o ro             open archive in read-only mode\n"
            "    -f                     don't detach from terminal\n"
            "    -d                     turn on debugging, also implies -f\n"
            "\n");
}

/**
 * Print version information (fuse-zip and FUSE library)
 */
void print_version() {
    fprintf(stderr, "%s version: %s\n", PROGRAM, VERSION);
}


/**
 * Initialize libzip and fuse-zip structures.
 *
 * @param fileName  ZIP file name
 * @return NULL if an error occured, otherwise pointer to FuseZipData structure.
 */
FuseZipData *initFuseZip(const char * fileName) {
    FuseZipData *data = NULL;
    int err;
    struct zip *zip_file;

    openlog(PROGRAM, LOG_PID, LOG_USER);

    if ((zip_file = zip_open(fileName, ZIP_CHECKCONS | ZIP_CREATE, &err)) == NULL) {
        char err_str[ERROR_STR_BUF_LEN];
        zip_error_to_str(err_str, ERROR_STR_BUF_LEN, err, errno);
        fprintf(stderr, "%s: cannot open zip archive %s: %s\n", PROGRAM, fileName, err_str);
        return data;
    }

    try {
        // current working directory
        char *cwd = (char*)malloc(PATH_MAX + 1);
        if (getcwd(cwd, PATH_MAX) == NULL) {
            perror(NULL);
            return data;
        }

        data = new FuseZipData(fileName, zip_file, cwd);
        if (data == NULL) {
            throw std::bad_alloc();
        }
    }
    catch (std::bad_alloc) {
        fprintf(stderr, "%s: no enough memory\n", PROGRAM);
    }
    catch (std::exception) {
        fprintf(stderr, "%s: ZIP file corrupted\n", PROGRAM);
    }
    return data;
}

/**
 * Parameters for command-line argument processing function
 */
struct fusezip_param {
    // help shown
    bool help;
    // version information shown
    bool version;
    // number of string arguments
    int strArgCount;
    // zip file name
    const char *fileName;
};

/**
 * Function to process arguments (called from fuse_opt_parse).
 *
 * @param data  Pointer to fusezip_param structure
 * @param arg is the whole argument or option
 * @param key determines why the processing function was called
 * @param outargs the current output argument list
 * @return -1 on error, 0 if arg is to be discarded, 1 if arg should be kept
 */
static int process_arg(void *data, const char *arg, int key, struct fuse_args *outargs) {
    struct fusezip_param *param = (fusezip_param*)data;

    (void)outargs;

    // 'magic' fuse_opt_proc return codes
    const static int KEEP = 1;
    const static int DISCARD = 0;
    const static int ERROR = -1;

    switch (key) {
        case KEY_HELP: {
            print_usage();
            param->help = true;
            return DISCARD;
        }

        case KEY_VERSION: {
            print_version();
            param->version = true;
            return KEEP;
        }

        case FUSE_OPT_KEY_NONOPT: {
            ++param->strArgCount;
            switch (param->strArgCount) {
                case 1: {
                    // zip file name
                    param->fileName = arg;
                    return DISCARD;
                }
                case 2: {
                    // mountpoint
                    // keep it and then pass to FUSE initializer
                    return KEEP;
                }
                default:
                    fprintf(stderr, "%s: only two arguments allowed: filename and mountpoint\n", PROGRAM);
                    return ERROR;
            }
        }

        default: {
            return KEEP;
        }
    }
}

static const struct fuse_opt fusezip_opts[] = {
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    {NULL, 0, 0}
};

int main(int argc, char *argv[]) {
    if (sizeof(void*) > sizeof(uint64_t)) {
        fprintf(stderr,"%s: This program cannot be run on your system because of FUSE design limitation\n", PROGRAM);
        return EXIT_FAILURE;
    }
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    FuseZipData *data = NULL;
    struct fusezip_param param;
    param.help = false;
    param.version = false;
    param.strArgCount = 0;
    param.fileName = NULL;

    if (fuse_opt_parse(&args, &param, fusezip_opts, process_arg)) {
        return EXIT_FAILURE;
    }

    // if all work is done inside options parsing...
    if (param.help) {
        return EXIT_SUCCESS;
    }
    
    // pass version switch to HELP library to see it's version
    if (!param.version) {
        // no file name passed
        if (param.fileName == NULL) {
            print_usage();
            return EXIT_FAILURE;
        }

        if ((data = initFuseZip(param.fileName)) == NULL) {
            return EXIT_FAILURE;
        }
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
    fusezip_oper.chmod      =   fusezip_chmod;
    fusezip_oper.chown      =   fusezip_chown;
    fusezip_oper.flush      =   fusezip_flush;
    fusezip_oper.fsync      =   fusezip_fsync;
    fusezip_oper.fsyncdir   =   fusezip_fsyncdir;
    fusezip_oper.opendir    =   fusezip_opendir;
    fusezip_oper.releasedir =   fusezip_releasedir;
    fusezip_oper.access     =   fusezip_access;
    fusezip_oper.utimens    =   fusezip_utimens;
    fusezip_oper.ftruncate  =   fusezip_ftruncate;
    fusezip_oper.truncate   =   fusezip_truncate;
    fusezip_oper.setxattr   =   fusezip_setxattr;
    fusezip_oper.getxattr   =   fusezip_getxattr;
    fusezip_oper.listxattr  =   fusezip_listxattr;
    fusezip_oper.removexattr=   fusezip_removexattr;

#if FUSE_VERSION >= 28
    // don't allow NULL path
    fusezip_oper.flag_nullpath_ok = 0;
#endif

    struct fuse *fuse;
    char *mountpoint;
    // this flag ignored because libzip does not supports multithreading
    int multithreaded;
    int res;

    fuse = fuse_setup(args.argc, args.argv, &fusezip_oper, sizeof(fusezip_oper), &mountpoint, &multithreaded, data);
    fuse_opt_free_args(&args);
    if (fuse == NULL) {
        return EXIT_FAILURE;
    }
    res = fuse_loop(fuse);
    fuse_teardown(fuse, mountpoint);
    return (res == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


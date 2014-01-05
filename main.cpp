////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2014 by Alexander Galanin                          //
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

#define KEY_HELP (0)
#define KEY_VERSION (1)
#define KEY_RO (2)

#include "config.h"

#include <fuse.h>
#include <fuse_opt.h>
#include <limits.h>
#include <syslog.h>

#include <cerrno>

#include "fuse-zip.h"
#include "fuseZipData.h"

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
    // read-only flag
    bool readonly;
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

        case KEY_RO: {
            param->readonly = true;
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
    FUSE_OPT_KEY("-r",          KEY_RO),
    FUSE_OPT_KEY("ro",          KEY_RO),
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
    param.readonly = false;
    param.strArgCount = 0;
    param.fileName = NULL;

    if (fuse_opt_parse(&args, &param, fusezip_opts, process_arg)) {
        fuse_opt_free_args(&args);
        return EXIT_FAILURE;
    }

    // if all work is done inside options parsing...
    if (param.help) {
        fuse_opt_free_args(&args);
        return EXIT_SUCCESS;
    }
    
    // pass version switch to HELP library to see it's version
    if (!param.version) {
        // no file name passed
        if (param.fileName == NULL) {
            print_usage();
            fuse_opt_free_args(&args);
            return EXIT_FAILURE;
        }

        openlog(PROGRAM, LOG_PID, LOG_USER);
        if ((data = initFuseZip(PROGRAM, param.fileName, param.readonly))
                == NULL) {
            fuse_opt_free_args(&args);
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
    fusezip_oper.readlink   =   fusezip_readlink;
    fusezip_oper.symlink    =   fusezip_symlink;

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
        delete data;
        return EXIT_FAILURE;
    }
    res = fuse_loop(fuse);
    fuse_teardown(fuse, mountpoint);
    return (res == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


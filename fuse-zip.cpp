//TODO: insert license header

#define FUSE_USE_VERSION 26
#define PROGRAM "fuse-zip"
#define ERROR_STR_BUF_LEN 0x100

#include <fuse.h>
#include <zip.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

struct fusezip_rootnode {
    struct zip *zip_file;
};

static void *fusezip_init(struct fuse_conn_info *conn) {
    struct fuse_context *context = fuse_get_context();
    struct zip *zip_file = (struct zip*)context->private_data;

    struct fusezip_rootnode *data = (struct fusezip_rootnode*)malloc(sizeof(struct fusezip_rootnode));
    data->zip_file = zip_file;
    return data;
}

static void fusezip_destroy(void *data) {
    struct fuse_context *context = fuse_get_context();
    struct fusezip_rootnode *root_node = (struct fusezip_rootnode*)context->private_data;

    zip_close(root_node->zip_file);
    //TODO: handle error code of zip_close
    free(root_node);
}

void print_usage() {
    printf("USAGE: %s <zip-file> [fusermount options]\n", PROGRAM);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }
    
    int err;
    struct zip *zip_file;
    if ((zip_file = zip_open(argv[1], ZIP_CREATE | ZIP_CHECKCONS, &err)) == NULL) {
        char err_str[ERROR_STR_BUF_LEN];
        zip_error_to_str(err_str, ERROR_STR_BUF_LEN, err, errno);
        fprintf(stderr, "%s: cannot open zip archive %s: %s\n", PROGRAM, argv[1], err_str);
        return EXIT_FAILURE;
    }

    static struct fuse_operations fusezip_oper;
    fusezip_oper.init       =   fusezip_init;
    fusezip_oper.destroy    =   fusezip_destroy;
    return fuse_main(argc - 1, argv + 1, &fusezip_oper, zip_file);
}


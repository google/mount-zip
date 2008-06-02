//TODO: insert license header

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <zip.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static void *fusezip_init(struct fuse_conn_info *conn) {
    void *data=NULL;
    //TODO
    return data;
}

static void fusezip_destroy(void *data) {
    //TODO
}

static struct fuse_operations fuzezip_oper = {
    .init	=   fusezip_init,
    .destroy	=   fusezip_destroy
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &fuzezip_oper, NULL);
}


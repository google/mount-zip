#define SKIP_BUFFER_LENGTH (1024*1024)

#include <errno.h>
#include <fuse.h>

#include "roHandler.h"

RoHandler::RoHandler(struct zip *z, struct zip_file *zf, FileNode *_node): FileHandler(z, zf), node(_node) {
    node->fh = this;
}

RoHandler::~RoHandler() {
}

int RoHandler::read(char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (pos > offset) {
        // It is terrible, but there are no other way to rewind file :(
        int err;
        err = zip_fclose(zf);
        if (err != 0) {
            return -EIO;
        }
        zf = zip_fopen(z, node->full_name, fi->flags);
        if (zf == NULL) {
            int err;
            zip_error_get(z, NULL, &err);
            return err;
        }
        pos = 0;
    }
    // skipping to offset ...
    ssize_t count = offset - pos;
    while (count > 0) {
        static char bb[SKIP_BUFFER_LENGTH];
        ssize_t r = SKIP_BUFFER_LENGTH;
        if (r > count) {
            r = count;
        }
        ssize_t rr = zip_fread(zf, bb, r);
        if (rr < 0) {
            return -EIO;
        }
        count -= rr;
    }

    ssize_t nr = zip_fread(zf, buf, size);
    pos += nr;
    return nr;
}

int RoHandler::write(const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Not implemented here
    return -1;
}

int RoHandler::close() {
    if (--node->open_count) {
        if (zip_fclose(zf) != 0) {
            return -EIO;
        }
    }
    return 0;
}


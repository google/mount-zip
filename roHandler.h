#ifndef RO_HANDLER_H
#define RO_HANDLER_H 1

#include "fileHandler.h"
#include "fileNode.h"

class RoHandler: public FileHandler {
private:
    FileNode *node;
public:
    RoHandler(struct zip *z, struct zip_file *zf, FileNode *_node);
    virtual ~RoHandler();

    int read(char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
    int write(const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
    int close();
};

#endif


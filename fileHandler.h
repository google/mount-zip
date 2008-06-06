#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H 1

#include <unistd.h>
#include <zip.h>

class FileHandler {
protected:
    struct zip *z;
    struct zip_file *zf;
    off_t pos;
public:
    FileHandler(struct zip *_z, struct zip_file *_zf);
    virtual ~FileHandler();

    virtual int read(char *buf, size_t size, off_t offset, struct fuse_file_info *fi) = 0;
    virtual int write(const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) = 0;
    virtual int close() = 0;
};

#endif


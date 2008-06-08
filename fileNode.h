#ifndef FILE_NODE_H
#define FILE_NODE_H

#include "types.h"
#include "bigBuffer.h"

class FileNode {
private:
    BigBuffer *buffer;
    FuseZipData *data;
    bool saving;

    void parse_name(char *fname);
    void attach();
public:
    FileNode(FuseZipData *_data, const char *fname, int id = -2);
    ~FileNode();

    void detach();
    void rename(char *fname);
    void rename_wo_reparenting(char *new_name);

    int open();
    int read(char *buf, size_t size, off_t offset) const;
    int write(const char *buf, size_t size, off_t offset);
    int close();
    int save();

    char *name, *full_name;
    bool is_dir;
    int id;
    nodelist_t childs;
    int open_count;
    FileNode *parent;

    bool changed;
    struct zip_stat stat;
};
#endif


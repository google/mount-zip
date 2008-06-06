#ifndef FILE_NODE_H
#define FILE_NODE_H

#include "types.h"

class FileNode {
private:
    void parse_name(char *fname);
    void attach(filemap_t &files);
public:
    FileNode(filemap_t &files, int id, const char *fname);
    ~FileNode();

    void detach(filemap_t &files);
    void rename(filemap_t &filemap, char *fname);
    void rename_wo_reparenting(char *new_name);

    char *name, *full_name;
    bool is_dir;
    int id;
    nodelist_t childs;
    int open_count;
    FileHandler *fh;
    FileNode *parent;
};
#endif


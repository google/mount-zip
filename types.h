#ifndef FUSEZIP_TYPES_H
#define FUSEZIP_TYPES_H

#include <list>
#include <map>

class FileNode;
class FileHandler;
class FuseZipData;

struct ltstr {
    bool operator() (const char* s1, const char* s2) const {
        return strcmp(s1, s2) < 0;
    }
};

//TODO: replace with set or hash_set
typedef std::list <FileNode*> nodelist_t;
typedef std::map <const char*, FileNode*, ltstr> filemap_t;

#endif


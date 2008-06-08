#ifndef FUSEZIP_DATA
#define FUSEZIP_DATA

#include "types.h"
#include "fileNode.h"

class FuseZipData {
private:
    void build_tree();
public:
    filemap_t files;
    struct zip *m_zip;

    FuseZipData(struct zip *z);
    ~FuseZipData();
};

#endif

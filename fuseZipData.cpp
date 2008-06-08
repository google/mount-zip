#define ROOT_NODE_INDEX (-1)

#include <zip.h>

#include "fuseZipData.h"

FuseZipData::FuseZipData(struct zip *z) {
    m_zip = z;
    build_tree();
}

FuseZipData::~FuseZipData() {
    zip_close(m_zip);
    //TODO: handle error code of zip_close

    for (filemap_t::iterator i = files.begin(); i != files.end(); ++i) {
        delete i->second;
    }
}

void FuseZipData::build_tree() {
    FileNode *root_node = new FileNode(this, "", ROOT_NODE_INDEX);
    root_node->is_dir = true;

    int n = zip_get_num_files(m_zip);
    for (int i = 0; i < n; ++i) {
        FileNode *node = new FileNode(this, zip_get_name(m_zip, i, 0), i);
        (void) node;
    }
}


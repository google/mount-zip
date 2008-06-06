#include <assert.h>

#include "fileNode.h"

FileNode::FileNode(filemap_t &files, int id, const char *fname) {
    this->id = id;
    this->is_dir = false;
    this->open_count = 0;
    parse_name(strdup(fname));
    attach(files);
}

FileNode::~FileNode() {
    free(full_name);
}

void FileNode::parse_name(char *fname) {
    assert(fname != NULL);
    this->full_name = fname;
    if (*fname == '\0') {
        // in case of root directory of a virtual filesystem
        this->name = this->full_name;
        this->is_dir = true;
    } else {
        char *lsl = full_name;
        while (*lsl++) {}
        lsl--;
        while (lsl > full_name && *lsl != '/') {
            lsl--;
        }
        // If the last symbol in file name is '/' then it is a directory
        if (*(lsl+1) == '\0') {
            // It will produce two \0s at the end of file name. I think that it is not a problem
            *lsl = '\0';
            this->is_dir = true;
            while (lsl > full_name && *lsl != '/') {
                lsl--;
            }
        }
        // Setting short name of node
        if (*lsl == '/') {
            lsl++;
        }
        this->name = lsl;
    }
}

void FileNode::attach(filemap_t &files) {
    if (*full_name != '\0') {
        // Adding new child to parent node. For items without '/' in fname it will be root_node.
        char *lsl = name;
        if (lsl > full_name) {
            lsl--;
        }
        char c = *lsl;
        *lsl = '\0';
        // Searching for parent node...
        filemap_t::iterator parent = files.find(this->full_name);
        assert(parent != files.end());
        parent->second->childs.push_back(this);
        this->parent = parent->second;
        *lsl = c;
    }
    files[this->full_name] = this;
}

void FileNode::detach(filemap_t &files) {
    files.erase(full_name);
    parent->childs.remove(this);
}

void FileNode::rename(filemap_t &filemap, char *fname) {
    detach(filemap);
    free(full_name);
    parse_name(fname);
    attach(filemap);
}

void FileNode::rename_wo_reparenting(char *new_name) {
    free(full_name);
    parse_name(new_name);
}

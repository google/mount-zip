////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2010 by Alexander Galanin                          //
//  al@galanin.nnov.ru                                                    //
//  http://galanin.nnov.ru/~al                                            //
//                                                                        //
//  This program is free software; you can redistribute it and/or modify  //
//  it under the terms of the GNU Lesser General Public License as        //
//  published by the Free Software Foundation; either version 3 of the    //
//  License, or (at your option) any later version.                       //
//                                                                        //
//  This program is distributed in the hope that it will be useful,       //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//  GNU General Public License for more details.                          //
//                                                                        //
//  You should have received a copy of the GNU Lesser General Public      //
//  License along with this program; if not, write to the                 //
//  Free Software Foundation, Inc.,                                       //
//  51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA               //
////////////////////////////////////////////////////////////////////////////

#include <cerrno>
#include <climits>
#include <ctime>
#include <cstdlib>
#include <cstring>

#include "fileNode.h"
#include "fuseZipData.h"

const int FileNode::ROOT_NODE_INDEX = -1;
const int FileNode::NEW_NODE_INDEX = -2;

FileNode::FileNode(FuseZipData *_data, const char *fname, int id): data(_data) {
    this->id = id;
    this->is_dir = false;
    this->open_count = 0;
    if (id == NEW_NODE_INDEX) {
        state = NEW;
        buffer = new BigBuffer();
        if (!buffer) {
            throw std::bad_alloc();
        }
        zip_stat_init(&stat);
        stat.mtime = time(NULL);
        stat.size = 0;
    } else {
        state = CLOSED;
        if (id != ROOT_NODE_INDEX) {
            zip_stat_index(data->m_zip, this->id, 0, &stat);
        } else {
            zip_stat_init(&stat);
            stat.mtime = time(NULL);
            stat.size = 0;
        }
    }
    char *t = strdup(fname);
    if (t == NULL) {
        throw std::bad_alloc();
    }
    parse_name(t);
    try {
        attach();
    }
    catch (...) {
        free(full_name);
        throw;
    }
}

FileNode::~FileNode() {
    free(full_name);
    if (state == OPENED || state == CHANGED || state == NEW) {
        delete buffer;
    }
}

void FileNode::parse_name(char *fname) {
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
        if (*lsl == '/' && *(lsl+1) == '\0') {
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

void FileNode::attach() {
    filemap_t::const_iterator other_iter =
        this->data->files.find(this->full_name);
    // If Node with the same name already exists...
    if (other_iter != this->data->files.end()) {
        FileNode *other = other_iter->second;
        if (other->id != FileNode::NEW_NODE_INDEX) {
            throw std::exception();
        }
        // ... update existing node information
        other->id = this->id;
        other->stat = this->stat;
        other->state = this->state;
        throw AlreadyExists();
    }
    if (*this->full_name != '\0') {
        // Adding new child to parent node. For items without '/' in fname it will be root_node.
        char *lsl = this->name;
        if (lsl > this->full_name) {
            lsl--;
        }
        char c = *lsl;
        *lsl = '\0';
        // Searching for parent node...
        filemap_t::const_iterator parent_iter = data->files.find(this->full_name);
        if (parent_iter == data->files.end()) {
            // Relative paths are not supported
            if (strcmp(this->full_name, ".") == 0 || strcmp(this->full_name, "..") == 0) {
                throw std::exception();
            }
            FileNode *p = new FileNode(data, this->full_name);
            p->is_dir = true;
            this->parent = p;
        } else {
            this->parent = parent_iter->second;
        }
        this->parent->childs.push_back(this);
        *lsl = c;
    }
    this->data->files[this->full_name] = this;
}

void FileNode::detach() {
    data->files.erase(full_name);
    parent->childs.remove(this);
}

void FileNode::rename(char *fname) {
    detach();
    free(full_name);
    parse_name(fname);
    attach();
}

void FileNode::rename_wo_reparenting(char *new_name) {
    data->files.erase(full_name);
    free(full_name);
    parse_name(new_name);
    data->files[new_name] = this;
}

int FileNode::open() {
    if (state == NEW) {
        return 0;
    }
    if (state == OPENED) {
        if (open_count == INT_MAX) {
            return -EMFILE;
        } else {
            ++open_count;
        }
    }
    if (state == CLOSED) {
        open_count = 1;
        try {
            buffer = new BigBuffer(data->m_zip, id, stat.size);
            state = OPENED;
        }
        catch (std::bad_alloc) {
            return -ENOMEM;
        }
        catch (std::exception) {
            return -EIO;
        }
    }
    return 0;
}

int FileNode::read(char *buf, size_t size, offset_t offset) const {
    return buffer->read(buf, size, offset);
}

int FileNode::write(const char *buf, size_t size, offset_t offset) {
    if (state == OPENED) {
        state = CHANGED;
    }
    return buffer->write(buf, size, offset);
}

int FileNode::close() {
    stat.size = buffer->len;
    if (state == OPENED && --open_count == 0) {
        delete buffer;
        state = CLOSED;
    }
    if (state == CHANGED) {
        stat.mtime = time(NULL);
    }
    return 0;
}

int FileNode::save() {
    return buffer->saveToZip(stat.mtime, data->m_zip, full_name,
            state == NEW, id);
}

int FileNode::truncate(offset_t offset) {
    if (state != CLOSED) {
        if (state != NEW) {
            state = CHANGED;
        }
        try {
            buffer->truncate(offset);
            return 0;
        }
        catch (const std::bad_alloc &) {
            return EIO;
        }
    } else {
        return EBADF;
    }
}

offset_t FileNode::size() const {
    if (state == NEW || state == OPENED || state == CHANGED) {
        return buffer->len;
    } else {
        return stat.size;
    }
}


////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2013 by Alexander Galanin                          //
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
#include <stdexcept>
#include <syslog.h>
#include <cassert>

#include "fileNode.h"
#include "fuseZipData.h"

const zip_int64_t FileNode::ROOT_NODE_INDEX = -1;
const zip_int64_t FileNode::NEW_NODE_INDEX = -2;

FileNode::FileNode(FuseZipData *_data, const char *fname, zip_int64_t id):
        data(_data), full_name(fname) {
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
            // check that all used fields are valid
            zip_uint64_t needValid = ZIP_STAT_NAME | ZIP_STAT_INDEX |
                ZIP_STAT_SIZE | ZIP_STAT_MTIME;
            assert((stat.valid & needValid) == needValid);
        } else {
            zip_stat_init(&stat);
            stat.mtime = time(NULL);
            stat.size = 0;
        }
    }
    parse_name();
    attach();
}

FileNode::~FileNode() {
    if (state == OPENED || state == CHANGED || state == NEW) {
        delete buffer;
    }
}

void FileNode::parse_name() {
    if (full_name.empty()) {
        // in case of root directory of a virtual filesystem
        this->name = full_name.c_str();
        this->is_dir = true;
    } else {
        const char *lsl = full_name.c_str();
        while (*lsl++) {}
        lsl--;
        while (lsl > full_name.c_str() && *lsl != '/') {
            lsl--;
        }
        // If the last symbol in file name is '/' then it is a directory
        if (*lsl == '/' && *(lsl+1) == '\0') {
            // It will produce two \0s at the end of file name. I think that
            // it is not a problem
            full_name[full_name.size() - 1] = 0;
            this->is_dir = true;
            while (lsl > full_name.c_str() && *lsl != '/') {
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
        this->data->files.find(full_name.c_str());
    // If Node with the same name already exists...
    if (other_iter != this->data->files.end()) {
        FileNode *other = other_iter->second;
        if (other->id != FileNode::NEW_NODE_INDEX) {
            syslog(LOG_ERR, "duplicated file name: %s",
                    full_name.c_str());
            throw std::runtime_error("duplicate file names");
        }
        // ... update existing node information
        other->id = this->id;
        other->stat = this->stat;
        other->state = this->state;
        throw AlreadyExists();
    }
    if (!full_name.empty()) {
        // Adding new child to parent node. For items without '/' in fname it will be root_node.
        const char *lsl = this->name;
        if (lsl > full_name.c_str()) {
            lsl--;
        }
        char c = *lsl;
        *(const_cast <char*> (lsl)) = '\0';
        // Searching for parent node...
        filemap_t::const_iterator parent_iter = data->files.find(
                full_name.c_str());
        if (parent_iter == data->files.end()) {
            FileNode *p = new FileNode(data, full_name.c_str());
            p->is_dir = true;
            this->parent = p;
        } else {
            this->parent = parent_iter->second;
        }
        this->parent->childs.push_back(this);
        *(const_cast <char*> (lsl)) = c;
    }
    this->data->files[full_name.c_str()] = this;
}

void FileNode::detach() {
    data->files.erase(full_name.c_str());
    parent->childs.remove(this);
}

void FileNode::rename(char *fname) {
    detach();
    full_name = fname;
    parse_name();
    attach();
}

void FileNode::rename_wo_reparenting(char *new_name) {
    data->files.erase(full_name.c_str());
    full_name = new_name;
    parse_name();
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

int FileNode::read(char *buf, size_t size, zip_uint64_t offset) const {
    return buffer->read(buf, size, offset);
}

int FileNode::write(const char *buf, size_t size, zip_uint64_t offset) {
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
    return buffer->saveToZip(stat.mtime, data->m_zip, full_name.c_str(),
            state == NEW, id);
}

int FileNode::truncate(zip_uint64_t offset) {
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

zip_uint64_t FileNode::size() const {
    if (state == NEW || state == OPENED || state == CHANGED) {
        return buffer->len;
    } else {
        return stat.size;
    }
}


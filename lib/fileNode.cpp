////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2014 by Alexander Galanin                          //
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
#include "extraField.h"

const zip_int64_t FileNode::ROOT_NODE_INDEX = -1;
const zip_int64_t FileNode::NEW_NODE_INDEX = -2;

// ZIP extra fields
const int FileNode::EXT_TIMESTAMP = 0x5455;

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
        has_cretime = true;
        mtime = atime = ctime = cretime = time(NULL);
        m_size = 0;
    } else {
        has_cretime = false;
        state = CLOSED;
        if (id != ROOT_NODE_INDEX) {
            struct zip_stat stat;
            zip_stat_index(data->m_zip, this->id, 0, &stat);
            // check that all used fields are valid
            zip_uint64_t needValid = ZIP_STAT_NAME | ZIP_STAT_INDEX |
                ZIP_STAT_SIZE | ZIP_STAT_MTIME;
            assert((stat.valid & needValid) == needValid);
            mtime = atime = ctime = stat.mtime;
            m_size = stat.size;
            processExtraFields();
        } else {
            mtime = atime = ctime = time(NULL);
            m_size = 0;
        }
    }
    parse_name();
    attach();
    if (id == NEW_NODE_INDEX && parent != NULL) {
        parent->ctime = time(NULL);
    }
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
        other->m_size = this->m_size;
        other->mtime = this->mtime;
        other->atime = this->atime;
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
    } else {
        parent = NULL;
    }
    this->data->files[full_name.c_str()] = this;
}

void FileNode::detach() {
    data->files.erase(full_name.c_str());
    parent->childs.remove(this);
}

void FileNode::rename(const char *fname) {
    FileNode *oldParent = parent;
    detach();
    full_name = fname;
    parse_name();
    attach();
    if (parent != oldParent) {
        if (oldParent != NULL) {
            oldParent->ctime = time(NULL);
        }
        if (parent != NULL) {
            parent->ctime = time(NULL);
        }
    }
}

void FileNode::rename_wo_reparenting(const char *new_name) {
    data->files.erase(full_name.c_str());
    full_name = new_name;
    parse_name();
    data->files[full_name.c_str()] = this;
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
            buffer = new BigBuffer(data->m_zip, id, m_size);
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

int FileNode::read(char *buf, size_t sz, zip_uint64_t offset) {
    atime = time(NULL);
    return buffer->read(buf, sz, offset);
}

int FileNode::write(const char *buf, size_t sz, zip_uint64_t offset) {
    if (state == OPENED) {
        state = CHANGED;
    }
    mtime = time(NULL);
    return buffer->write(buf, sz, offset);
}

int FileNode::close() {
    m_size = buffer->len;
    if (state == OPENED && --open_count == 0) {
        delete buffer;
        state = CLOSED;
    }
    return 0;
}

int FileNode::save() {
    // index is modified if state == NEW
    int res = buffer->saveToZip(mtime, data->m_zip, full_name.c_str(),
            state == NEW, id);
    if (res == 0) {
        return updateExtraFields();
    } else {
        return res;
    }
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
        mtime = time(NULL);
    } else {
        return EBADF;
    }
}

zip_uint64_t FileNode::size() const {
    if (state == NEW || state == OPENED || state == CHANGED) {
        return buffer->len;
    } else {
        return m_size;
    }
}

/**
 * Get timestamp information from extra fields
 */
void FileNode::processExtraFields () {
    zip_int16_t count;

    // 5455: Extended Timestamp Extra Field
    count = zip_file_extra_fields_count_by_id (data->m_zip, id,
            EXT_TIMESTAMP, ZIP_FL_LOCAL);
    for (zip_int16_t i = 0; i < count; ++i) {
        zip_uint16_t len;
        const zip_uint8_t *field = zip_file_extra_field_get_by_id
            (data->m_zip, id, EXT_TIMESTAMP, i, &len, ZIP_FL_LOCAL);
        bool has_mtime, has_atime, has_cretime;
        time_t mt, at, cret;
        if (ExtraField::parseExtTimeStamp(len, field, has_mtime, mt,
                    has_atime, at, has_cretime, cret)) {
            if (has_mtime) {
                mtime = mt;
            }
            if (has_atime) {
                atime = at;
            }
            if (has_cretime) {
                cretime = cret;
                this->has_cretime = true;
            }
        }
    }
}

/**
 * Save timestamp into extra fields
 * @return 0 on success
 */
int FileNode::updateExtraFields () {
    static zip_flags_t locations[] = {ZIP_FL_CENTRAL, ZIP_FL_LOCAL};
    zip_int16_t count;
    const zip_uint8_t *field;
    zip_uint16_t len;

    for (unsigned int loc = 0; loc < sizeof(locations); ++loc) {
        // remove old extra fields
        count = zip_file_extra_fields_count (data->m_zip, id,
                locations[loc]);
        for (zip_int16_t i = count; i >= 0; --i) {
            zip_uint16_t type;
            const zip_uint8_t *ptr = zip_file_extra_field_get (data->m_zip,
                    id, i, &type, NULL, locations[loc]);
            if (ptr != NULL && type == EXT_TIMESTAMP) {
                zip_file_extra_field_delete (data->m_zip, id, i,
                        locations[loc]);
            }
        }
        // add timestamps
        field = ExtraField::createExtTimeStamp (locations[loc], mtime,
                atime, has_cretime, cretime, len);
        int res = zip_file_extra_field_set (data->m_zip, id, EXT_TIMESTAMP,
                ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
        if (res != 0) {
            return res;
        }
    }
    return 0;
}


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

FileNode::FileNode(FuseZipData *_data, const char *fname, zip_int64_t _id) {
    data = _data;
    metadataChanged = false;
    full_name = fname;
    id = _id;
    m_uid = 0;
    m_gid = 0;
}

FileNode *FileNode::createFile (FuseZipData *data, const char *fname, 
        mode_t mode) {
    FileNode *n = new FileNode(data, fname, NEW_NODE_INDEX);
    if (n == NULL) {
        return NULL;
    }
    n->state = NEW;
    n->is_dir = false;
    n->buffer = new BigBuffer();
    if (!n->buffer) {
        delete n;
        return NULL;
    }
    n->has_cretime = true;
    n->m_mtime = n->m_atime = n->m_ctime = n->cretime = time(NULL);

    n->parse_name();
    n->attach();
    n->parent->setCTime(n->m_mtime);
    n->m_mode = mode;

    return n;
}

FileNode *FileNode::createSymlink(FuseZipData *data, const char *fname) {
    FileNode *n = new FileNode(data, fname, NEW_NODE_INDEX);
    if (n == NULL) {
        return NULL;
    }
    n->state = NEW;
    n->is_dir = false;
    n->buffer = new BigBuffer();
    if (!n->buffer) {
        delete n;
        return NULL;
    }
    n->has_cretime = true;
    n->m_mtime = n->m_atime = n->m_ctime = n->cretime = time(NULL);

    n->parse_name();
    n->attach();
    n->parent->setCTime(n->m_mtime);
    n->m_mode = S_IFLNK | 0777;

    return n;
}

/**
 * Create intermediate directory to build full tree
 */
FileNode *FileNode::createIntermediateDir(FuseZipData *data,
        const char *fname) {
    FileNode *n = new FileNode(data, fname, NEW_NODE_INDEX);
    if (n == NULL) {
        return NULL;
    }
    n->state = NEW_DIR;
    n->is_dir = true;
    n->has_cretime = true;
    n->m_mtime = n->m_atime = n->m_ctime = n->cretime = time(NULL);
    n->m_size = 0;
    n->m_mode = S_IFDIR | 0775;

    n->parse_name();
    n->attach();

    return n;
}

FileNode *FileNode::createDir(FuseZipData *data, const char *fname,
        zip_int64_t id, mode_t mode) {
    FileNode *n = createNodeForZipEntry(data, fname, id);
    if (n == NULL) {
        return NULL;
    }
    n->has_cretime = true;
    n->parent->setCTime (n->cretime = n->m_mtime);
    // FUSE does not pass S_IFDIR bit here
    n->m_mode = S_IFDIR | mode;
    n->is_dir = true;
    return n;
}

FileNode *FileNode::createRootNode(FuseZipData *data) {
    FileNode *n = new FileNode(data, "", ROOT_NODE_INDEX);
    if (n == NULL) {
        return NULL;
    }
    n->is_dir = true;
    n->state = NEW_DIR;
    n->m_mtime = n->m_atime = n->m_ctime = n->cretime = time(NULL);
    n->has_cretime = true;
    n->m_size = 0;
    n->name = n->full_name.c_str();
    n->parent = NULL;
    n->m_mode = S_IFDIR | 0775;
    data->files[n->full_name.c_str()] = n;
    return n;
}

FileNode *FileNode::createNodeForZipEntry(FuseZipData *data,
        const char *fname, zip_int64_t id) {
    FileNode *n = new FileNode(data, fname, id);
    if (n == NULL) {
        return NULL;
    }
    n->is_dir = false;
    n->open_count = 0;
    n->state = CLOSED;

    struct zip_stat stat;
    zip_stat_index(data->m_zip, id, 0, &stat);
    // check that all used fields are valid
    zip_uint64_t needValid = ZIP_STAT_NAME | ZIP_STAT_INDEX |
        ZIP_STAT_SIZE | ZIP_STAT_MTIME;
    // required fields are always valid for existing items or newly added
    // directories (see zip_stat_index.c from libzip)
    assert((stat.valid & needValid) == needValid);
    n->m_mtime = n->m_atime = n->m_ctime = stat.mtime;
    n->has_cretime = false;
    n->m_size = stat.size;

    n->parse_name();
    //TODO: remove after existing node search redesign
    try {
        n->attach();
    }
    catch (...) {
        delete n;
        throw;
    }

    n->processExternalAttributes();
    n->processExtraFields();
    return n;
}

FileNode::~FileNode() {
    if (state == OPENED || state == CHANGED || state == NEW) {
        delete buffer;
    }
}

/**
 * Get short name of a file. If last char is '/' then node is a directory
 */
void FileNode::parse_name() {
    assert(!full_name.empty());

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

/**
 * Attach to parent node. Parent nodes are created if not yet exist.
 */
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
        other->m_mtime = this->m_mtime;
        other->m_atime = this->m_atime;
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
            //TODO: search for existing node by name
            FileNode *p = createIntermediateDir(data, full_name.c_str());
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
        time_t now = time(NULL);
        if (oldParent != NULL) {
            oldParent->setCTime (now);
        }
        if (parent != NULL) {
            parent->setCTime (now);
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
    m_atime = time(NULL);
    return buffer->read(buf, sz, offset);
}

int FileNode::write(const char *buf, size_t sz, zip_uint64_t offset) {
    if (state == OPENED) {
        state = CHANGED;
    }
    m_mtime = time(NULL);
    metadataChanged = true;
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
    return buffer->saveToZip(m_mtime, data->m_zip, full_name.c_str(),
            state == NEW, id);
}

int FileNode::saveMetadata() const {
    assert(id >= 0);
    return updateExtraFields() && updateExternalAttributes();
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
        m_mtime = time(NULL);
        metadataChanged = true;
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
 * Get file mode from external attributes.
 */
void FileNode::processExternalAttributes () {
    zip_uint8_t opsys;
    zip_uint32_t attr;
    assert(id >= 0);
    zip_file_get_external_attributes(data->m_zip, id, 0, &opsys, &attr);
    switch (opsys) {
        case ZIP_OPSYS_UNIX: {
            m_mode = attr >> 16;
            // force is_dir value
            if (is_dir) {
                m_mode = (m_mode & ~S_IFMT) | S_IFDIR;
            } else {
                m_mode = m_mode & ~S_IFDIR;
            }
            break;
        }
        case ZIP_OPSYS_DOS:
        case ZIP_OPSYS_WINDOWS_NTFS:
        case ZIP_OPSYS_MVS: {
            /*
             * Both WINDOWS_NTFS and OPSYS_MVS used here because of
             * difference in constant assignment by PKWARE and Info-ZIP
             */
            m_mode = S_IRUSR | S_IRGRP | S_IROTH;
            // http://msdn.microsoft.com/en-us/library/windows/desktop/gg258117%28v=vs.85%29.aspx
            // http://en.wikipedia.org/wiki/File_Allocation_Table#attributes
            // FILE_ATTRIBUTE_READONLY
            if ((attr & 1) == 0) {
                m_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
            }
            // directory
            if (is_dir) {
                m_mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
            } else {
                m_mode |= S_IFREG;
            }

            break;
        }
        default: {
            if (is_dir) {
                m_mode = S_IFDIR | 0775;
            } else {
                m_mode = S_IFREG | 0664;
            }
        }
    }
}

/**
 * Get timestamp information from extra fields.
 * Get owner and group information.
 */
void FileNode::processExtraFields () {
    zip_int16_t count;
    // times from timestamp have precedence
    bool mtimeFromTimestamp = false, atimeFromTimestamp = false;
    // UIDs and GIDs from UNIX extra fields with bigger type IDs have
    // precedence
    int lastProcessedUnixField = 0;

    count = zip_file_extra_fields_count (data->m_zip, id, ZIP_FL_LOCAL);
    for (zip_int16_t i = 0; i < count; ++i) {
        bool has_mtime, has_atime, has_cretime;
        time_t mt, at, cret;
        zip_uint16_t type, len;
        const zip_uint8_t *field = zip_file_extra_field_get (data->m_zip,
                id, i, &type, &len, ZIP_FL_LOCAL);

        switch (type) {
            case FZ_EF_TIMESTAMP: {
                if (ExtraField::parseExtTimeStamp (len, field, has_mtime, mt,
                            has_atime, at, has_cretime, cret)) {
                    if (has_mtime) {
                        m_mtime = mt;
                        mtimeFromTimestamp = true;
                    }
                    if (has_atime) {
                        m_atime = at;
                        atimeFromTimestamp = true;
                    }
                    if (has_cretime) {
                        cretime = cret;
                        this->has_cretime = true;
                    }
                }
                break;
            }
            case FZ_EF_PKWARE_UNIX:
            case FZ_EF_INFOZIP_UNIX1:
            case FZ_EF_INFOZIP_UNIX2:
            case FZ_EF_INFOZIP_UNIXN: {
                uid_t uid;
                gid_t gid;
                if (ExtraField::parseSimpleUnixField (type, len, field,
                            uid, gid, has_mtime, mt, has_atime, at)) {
                    if (type >= lastProcessedUnixField) {
                        m_uid = uid;
                        m_gid = gid;
                        lastProcessedUnixField = type;
                    }
                    if (has_mtime && !mtimeFromTimestamp) {
                        m_mtime = mt;
                    }
                    if (has_atime && !atimeFromTimestamp) {
                        m_atime = at;
                    }
                }
                break;
            }
        }
    }
}

/**
 * Save timestamp into extra fields
 * @return 0 on success
 */
int FileNode::updateExtraFields () const {
    static zip_flags_t locations[] = {ZIP_FL_CENTRAL, ZIP_FL_LOCAL};

    for (unsigned int loc = 0; loc < sizeof(locations); ++loc) {
        // remove old extra fields
        zip_int16_t count = zip_file_extra_fields_count (data->m_zip, id,
                locations[loc]);
        const zip_uint8_t *field;
        for (zip_int16_t i = count; i >= 0; --i) {
            zip_uint16_t type;
            field = zip_file_extra_field_get (data->m_zip, id, i, &type,
                    NULL, locations[loc]);
            // FZ_EF_PKWARE_UNIX not removed because can contain extra data
            // that currently not handled by fuse-zip
            if (field != NULL && (type == FZ_EF_TIMESTAMP ||
                        type == FZ_EF_INFOZIP_UNIX1 ||
                        type == FZ_EF_INFOZIP_UNIX2 ||
                        type == FZ_EF_INFOZIP_UNIXN)) {
                zip_file_extra_field_delete (data->m_zip, id, i,
                        locations[loc]);
            }
        }
        // add new extra fields
        zip_uint16_t len;
        int res;
        // add timestamps
        field = ExtraField::createExtTimeStamp (locations[loc], m_mtime,
                m_atime, has_cretime, cretime, len);
        res = zip_file_extra_field_set (data->m_zip, id, FZ_EF_TIMESTAMP,
                ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
        if (res != 0) {
            return res;
        }
        // add UNIX owner info
        field = ExtraField::createInfoZipNewUnixField (m_uid, m_gid, len);
        res = zip_file_extra_field_set (data->m_zip, id, FZ_EF_INFOZIP_UNIXN,
                ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
        if (res != 0) {
            return res;
        }
    }
    return 0;
}

void FileNode::chmod (mode_t mode) {
    m_mode = (m_mode & S_IFMT) | mode;
    m_ctime = time(NULL);
    metadataChanged = true;
}

void FileNode::setUid (uid_t uid) {
    m_uid = uid;
    metadataChanged = true;
}

void FileNode::setGid (gid_t gid) {
    m_gid = gid;
    metadataChanged = true;
}

/**
 * Save OS type and permissions into external attributes
 * @return libzip error code (ZIP_ER_MEMORY or ZIP_ER_RDONLY)
 */
int FileNode::updateExternalAttributes() const {
    assert(id >= 0);
    // save UNIX attributes in high word
    mode_t mode = m_mode << 16;

    // save DOS attributes in low byte
    // http://msdn.microsoft.com/en-us/library/windows/desktop/gg258117%28v=vs.85%29.aspx
    // http://en.wikipedia.org/wiki/File_Allocation_Table#attributes
    if (is_dir) {
        // FILE_ATTRIBUTE_DIRECTORY
        mode |= 0x10;
    }
    if (name[0] == '.') {
        // FILE_ATTRIBUTE_HIDDEN
        mode |= 2;
    }
    if (!(mode & S_IWUSR)) {
        // FILE_ATTRIBUTE_READONLY
        mode |= 1;
    }
    return zip_file_set_external_attributes (data->m_zip, id, 0,
            ZIP_OPSYS_UNIX, mode);
}

void FileNode::setTimes (time_t atime, time_t mtime) {
    m_atime = atime;
    m_mtime = mtime;
    metadataChanged = true;
}

void FileNode::setCTime (time_t ctime) {
    m_ctime = ctime;
    metadataChanged = true;
}

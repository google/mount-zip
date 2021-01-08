////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2019 by Alexander Galanin                          //
//  al@galanin.nnov.ru                                                    //
//  http://galanin.nnov.ru/~al                                            //
//                                                                        //
//  This program is free software: you can redistribute it and/or modify  //
//  it under the terms of the GNU General Public License as published by  //
//  the Free Software Foundation, either version 3 of the License, or     //
//  (at your option) any later version.                                   //
//                                                                        //
//  This program is distributed in the hope that it will be useful,       //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//  GNU General Public License for more details.                          //
//                                                                        //
//  You should have received a copy of the GNU General Public License     //
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.//
////////////////////////////////////////////////////////////////////////////

#define __STDC_LIMIT_MACROS

#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <stdexcept>

#include <syslog.h>

#include "fileNode.h"
#include "extraField.h"

const zip_int64_t FileNode::ROOT_NODE_INDEX = -1;
const zip_int64_t FileNode::NEW_NODE_INDEX = -2;
const zip_int64_t FileNode::TMP_DIR_INDEX = -2;

FileNode::FileNode(struct zip *zip_, const char *fname, zip_int64_t id,
        std::shared_ptr<DataNode> data) {
    zip = zip_;
    full_name = fname;
    _id = id;
    _data = std::move(data);
    m_comment = NULL;
    m_commentLen = 0;
    m_commentChanged = true;
}

FileNode *FileNode::createFile (struct zip *zip, const char *fname, 
        uid_t owner, gid_t group, mode_t mode, dev_t dev) {
    auto data = DataNode::createNew(mode, owner, group, dev);
    FileNode *n = new FileNode(zip, fname, NEW_NODE_INDEX, std::move(data));
    if (n == NULL) {
        return NULL;
    }
    n->parse_name();

    return n;
}

FileNode *FileNode::createSymlink(struct zip *zip, const char *fname) {
    auto data = DataNode::createNew(S_IFLNK | 0777, 0, 0, 0);
    FileNode *n = new FileNode(zip, fname, NEW_NODE_INDEX, std::move(data));
    if (n == NULL) {
        return NULL;
    }
    n->parse_name();

    return n;
}

/**
 * Create intermediate directory to build full tree
 */
FileNode *FileNode::createIntermediateDir(struct zip *zip,
        const char *fname) {
    auto data = DataNode::createTmpDir(S_IFDIR | 0755, 0, 0, 0);
    FileNode *n = new FileNode(zip, fname, TMP_DIR_INDEX, std::move(data));
    if (n == NULL) {
        return NULL;
    }
    n->m_commentChanged = false;
    n->parse_name();

    return n;
}

FileNode *FileNode::createDir(struct zip *zip, const char *fname,
        zip_int64_t id, uid_t owner, gid_t group, mode_t mode) {
    assert(id >= 0);
    // FUSE does not pass S_IFDIR bit here
    auto data = DataNode::createNew(S_IFDIR | mode, owner, group, 0);
    FileNode *n = new FileNode(zip, fname, id, std::move(data));
    if (n == NULL) {
        return NULL;
    }
    n->m_commentChanged = false;
    n->parse_name();

    return n;
}

FileNode *FileNode::createRootNode(struct zip *zip) {
    auto data = DataNode::createNew(S_IFDIR | 0755, 0, 0, 0);
    FileNode *n = new FileNode(zip, "", ROOT_NODE_INDEX, std::move(data));
    if (n == NULL) {
        return NULL;
    }
    n->name = n->full_name.c_str();

    int len = 0;
    n->m_comment = zip_get_archive_comment(zip, &len, ZIP_FL_ENC_RAW);
    n->m_commentChanged = false;
    if (len < 0)
        n->m_comment = NULL;
    else
        // RAW comment length can't exceed 16 bits
        n->m_commentLen = static_cast<uint16_t>(len);
    if (n->m_comment != NULL && n->m_comment[0] == '\0')
        n->m_comment = NULL;
    if (n->m_comment == NULL)
        n->m_commentLen = 0;

    return n;
}

FileNode *FileNode::createNodeForZipEntry(struct zip *zip,
        const char *fname, zip_int64_t id, mode_t mode) {
    assert(id >= 0);
    auto data = DataNode::createExisting(zip, static_cast<zip_uint64_t>(id), mode);
    FileNode *n = new FileNode(zip, fname, id, data);
    if (n == NULL) {
        return NULL;
    }

    n->parse_name();
    n->readComment();

    return n;
}

FileNode *FileNode::createHardlink(struct zip *zip,
            const char *fname, zip_int64_t id, FileNode *target) {
    assert(id >= 0);
    FileNode *n = new FileNode(zip, fname, id, target->_data);
    if (n == NULL) {
        return NULL;
    }

    n->parse_name();
    n->readComment();

    return n;
}

FileNode::~FileNode() {
    if (m_commentChanged && m_comment != NULL)
        delete [] m_comment;
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

void FileNode::readComment() {
    uint32_t len = 0;
    m_comment = zip_file_get_comment(zip, id(), &len, ZIP_FL_ENC_RAW);
    m_commentChanged = false;
    // RAW comment length can't exceed 16 bits
    m_commentLen = static_cast<uint16_t>(len);
    if (m_comment != NULL && m_comment[0] == '\0')
        m_comment = NULL;
    if (m_comment == NULL)
        m_commentLen = 0;
}

void FileNode::appendChild (FileNode *child) {
    childs.push_back (child);
}

void FileNode::detachChild (FileNode *child) {
    childs.remove (child);
}

void FileNode::rename(const char *new_name) {
    full_name = new_name;
    _data->touchCTime();
    parse_name();
}

int FileNode::open() {
    return _data->open(zip);
}

int FileNode::read(char *buf, size_t sz, size_t offset) {
    return _data->read(buf, sz, offset);
}

int FileNode::write(const char *buf, size_t sz, size_t offset) {
    return _data->write(buf, sz, offset);
}

int FileNode::close() {
    return _data->close();
}

int FileNode::save() {
    assert (!is_dir());
    // index is modified if state == NEW
    return _data->save(zip, full_name.c_str(), _id);
}

int FileNode::saveMetadata(bool force_precise_time) const {
    assert(zip != NULL);
    assert(_id >= 0);

    int res = updateExtraFields(force_precise_time);
    if (res != 0)
        return res;
    return updateExternalAttributes();
}

int FileNode::saveComment() const {
    if (_id == ROOT_NODE_INDEX)
        return zip_set_archive_comment(zip, m_comment, m_commentLen);
    else
        return zip_file_set_comment(zip, id(), m_comment, m_commentLen, 0);
}

int FileNode::truncate(size_t offset) {
    assert(_data);
    return _data->truncate(offset);
}

zip_uint64_t FileNode::size() const {
    return _data->size();
}

bool FileNode::is_dir() const {
    return S_ISDIR(_data->mode());
}

/**
 * Save timestamp, owner and group info into extra fields
 * @return 0 on success
 */
int FileNode::updateExtraFields (bool force_precise_time) const {
    static zip_flags_t locations[] = {ZIP_FL_CENTRAL, ZIP_FL_LOCAL};

    assert(_id >= 0);
    assert(zip != NULL);

    mode_t mode = _data->mode();
    timespec mtime = _data->mtime();
    timespec atime = _data->atime();
    timespec btime = _data->btime();
    bool has_btime = _data->has_btime();

    for (unsigned int loc = 0; loc < sizeof(locations) / sizeof(locations[0]); ++loc) {
        std::unique_ptr<zip_uint8_t[]> ntfs_field;
        zip_uint16_t ntfs_field_len = 0;

        // remove old extra fields
        zip_int16_t count = zip_file_extra_fields_count (zip, id(),
                locations[loc]);
        const zip_uint8_t *field;
        for (zip_int16_t i = count; i >= 0; --i) {
            zip_uint16_t type;
            zip_uint16_t len;
            field = zip_file_extra_field_get (zip, id(), static_cast<zip_uint16_t>(i), &type,
                    &len, locations[loc]);
            if (field == NULL)
                continue;
            if (type == FZ_EF_TIMESTAMP ||
                        type == FZ_EF_PKWARE_UNIX   ||
                        type == FZ_EF_INFOZIP_UNIX1 ||
                        type == FZ_EF_INFOZIP_UNIX2 ||
                        type == FZ_EF_INFOZIP_UNIXN) {
                zip_file_extra_field_delete (zip, id(), static_cast<zip_uint16_t>(i),
                        locations[loc]);
            }
            if (type == FZ_EF_NTFS) {
                // back up previous field content
                ntfs_field_len = len;
                ntfs_field.reset(new zip_uint8_t[len + FZ_EF_NTFS_TIMESTAMP_LENGTH]);
                memcpy(ntfs_field.get(), field, len);

                zip_file_extra_field_delete (zip, id(), static_cast<zip_uint16_t>(i),
                        locations[loc]);
            }
        }
        // add new extra fields
        zip_uint16_t len;
        int res;
        // add special device or FIFO information
        if (S_ISBLK(mode) || S_ISCHR(mode) || S_ISFIFO(mode)) {
            field = ExtraField::createPkWareUnixField(mtime.tv_sec, atime.tv_sec, mode,
                    _data->uid(), _data->gid(), _data->device(), len);
            res = zip_file_extra_field_set(zip, id(), FZ_EF_PKWARE_UNIX,
                    ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
            if (res != 0) {
                return res;
            }

            // PKZIP 14 for Linux doesn't extract device nodes if extra fields
            // other than PKWARE UNIX are present in the central directory
            if ((S_ISBLK(mode) || S_ISCHR(mode)) && locations[loc] == ZIP_FL_CENTRAL)
                continue;
        }

        // add timestamps
        field = ExtraField::createExtTimeStamp (locations[loc], mtime.tv_sec,
                atime.tv_sec, has_btime, btime.tv_sec, len);
        res = zip_file_extra_field_set (zip, id(), FZ_EF_TIMESTAMP,
                ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
        if (res != 0) {
            return res;
        }
        // PKZIP 14 for Linux doesn't extract symlinks if NTFS extra field
        // is present in the central directory
        if ((has_btime || force_precise_time) &&
            (locations[loc] == ZIP_FL_LOCAL || !S_ISLNK(mode)))
        {
            // add high-precision timestamps
            if (ntfs_field) {
                len = ExtraField::editNtfsExtraField(ntfs_field_len, ntfs_field.get(),
                        mtime, atime, btime);
                field = ntfs_field.get();
            } else {
                field = ExtraField::createNtfsExtraField (mtime, atime, btime, len);
            }
            res = zip_file_extra_field_set (zip, id(), FZ_EF_NTFS,
                    ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
            if (res != 0) {
                return res;
            }
        }
        // add UNIX owner info
        field = ExtraField::createInfoZipNewUnixField (_data->uid(), _data->gid(), len);
        res = zip_file_extra_field_set (zip, id(), FZ_EF_INFOZIP_UNIXN,
                ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
        if (res != 0) {
            return res;
        }
    }
    return 0;
}

void FileNode::chmod (mode_t mode) {
    _data->chmod(mode);
}

void FileNode::setUid (uid_t uid) {
    _data->setUid(uid);
}

void FileNode::setGid (gid_t gid) {
    _data->setGid(gid);
}

/**
 * Save OS type and permissions into external attributes
 * @return 0 on success or libzip error code (ZIP_ER_MEMORY or ZIP_ER_RDONLY)
 */
int FileNode::updateExternalAttributes() const {
    assert(_id >= 0);
    assert (zip != NULL);
    // save UNIX attributes in high word
    mode_t mode = _data->mode() << 16;

    // save DOS attributes in low byte
    // http://msdn.microsoft.com/en-us/library/windows/desktop/gg258117%28v=vs.85%29.aspx
    // http://en.wikipedia.org/wiki/File_Allocation_Table#attributes
    if (is_dir()) {
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
    return zip_file_set_external_attributes (zip, id(), 0,
            ZIP_OPSYS_UNIX, mode);
}

void FileNode::setTimes (const timespec &atime, const timespec &mtime) {
    _data->setTimes(atime, mtime);
}

void FileNode::setCTime (const timespec &ctime) {
    _data->setCTime(ctime);
}

bool FileNode::setComment(const char *value, uint16_t length) {
    char *newComment = NULL;
    if (value != NULL) {
        newComment = new char[length];
        if (newComment == NULL)
            return false;
        memcpy(newComment, value, length);
    }

    if (m_commentChanged && m_comment != NULL)
        delete [] m_comment;
    m_comment = newComment;
    m_commentLen = length;
    m_commentChanged = true;

    return true;
}

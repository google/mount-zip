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

FileNode::FileNode(struct zip *zip_, const char *fname, zip_int64_t id_) {
    zip = zip_;
    metadataChanged = false;
    full_name = fname;
    _id = id_;
    m_size = 0;
    m_uid = 0;
    m_gid = 0;
    m_comment = NULL;
    m_commentLen = 0;
    m_commentChanged = true;
    m_device = 0;
}

FileNode *FileNode::createFile (struct zip *zip, const char *fname, 
        uid_t owner, gid_t group, mode_t mode, dev_t dev) {
    FileNode *n = new FileNode(zip, fname, NEW_NODE_INDEX);
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
    n->m_mtime = n->m_atime = n->m_ctime = n->m_cretime = currentTime();

    n->parse_name();
    n->m_mode = mode;
    n->m_uid = owner;
    n->m_gid = group;
    n->m_device = dev;

    return n;
}

FileNode *FileNode::createSymlink(struct zip *zip, const char *fname) {
    FileNode *n = new FileNode(zip, fname, NEW_NODE_INDEX);
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
    n->m_mtime = n->m_atime = n->m_ctime = n->m_cretime = currentTime();

    n->parse_name();
    n->m_mode = S_IFLNK | 0777;

    return n;
}

/**
 * Create intermediate directory to build full tree
 */
FileNode *FileNode::createIntermediateDir(struct zip *zip,
        const char *fname) {
    FileNode *n = new FileNode(zip, fname, NEW_NODE_INDEX);
    if (n == NULL) {
        return NULL;
    }
    n->state = NEW_DIR;
    n->is_dir = true;
    n->has_cretime = true;
    n->m_mtime = n->m_atime = n->m_ctime = n->m_cretime = currentTime();
    n->m_size = 0;
    n->m_mode = S_IFDIR | 0775;
    n->m_commentChanged = false;

    n->parse_name();

    return n;
}

FileNode *FileNode::createDir(struct zip *zip, const char *fname,
        zip_int64_t id, uid_t owner, gid_t group, mode_t mode) {
    assert(id >= 0);
    FileNode *n = createNodeForZipEntry(zip, fname, id);
    if (n == NULL) {
        return NULL;
    }
    n->state = CLOSED;
    n->has_cretime = true;
    n->m_cretime = n->m_mtime;
    // FUSE does not pass S_IFDIR bit here
    n->m_mode = S_IFDIR | mode;
    n->m_uid = owner;
    n->m_gid = group;
    n->is_dir = true;
    return n;
}

FileNode *FileNode::createRootNode(struct zip *zip) {
    FileNode *n = new FileNode(zip, "", ROOT_NODE_INDEX);
    if (n == NULL) {
        return NULL;
    }
    n->is_dir = true;
    n->state = NEW_DIR;
    n->m_mtime = n->m_atime = n->m_ctime = n->m_cretime = currentTime();
    n->has_cretime = true;
    n->m_size = 0;
    n->name = n->full_name.c_str();
    n->m_mode = S_IFDIR | 0775;

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
        const char *fname, zip_int64_t id) {
    assert(id >= 0);
    FileNode *n = new FileNode(zip, fname, id);
    if (n == NULL) {
        return NULL;
    }
    n->is_dir = false;
    n->open_count = 0;
    n->state = CLOSED;

    struct zip_stat stat;
    zip_stat_index(zip, n->id(), 0, &stat);
    // check that all used fields are valid
    zip_uint64_t needValid = ZIP_STAT_NAME | ZIP_STAT_INDEX |
        ZIP_STAT_SIZE | ZIP_STAT_MTIME;
    // required fields are always valid for existing items or newly added
    // directories (see zip_stat_index.c from libzip)
    assert((stat.valid & needValid) == needValid);
    n->m_mtime.tv_sec = n->m_atime.tv_sec = n->m_ctime.tv_sec = n->m_cretime.tv_sec = stat.mtime;
    n->m_mtime.tv_nsec = n->m_atime.tv_nsec = n->m_ctime.tv_nsec = n->m_cretime.tv_nsec = 0;
    n->has_cretime = false;
    n->m_size = stat.size;

    n->parse_name();

    bool hasPkWareField;
    n->processExternalAttributes();
    n->processExtraFields(hasPkWareField);

    // InfoZIP may produce FIFO-marked node with content, PkZip - can't.
    if (S_ISFIFO(n->m_mode)) {
        unsigned int type = S_IFREG;
        if (n->m_size == 0 && hasPkWareField)
            type = S_IFIFO;
        n->m_mode = (n->m_mode & static_cast<unsigned>(~S_IFMT)) | type;
    }

    uint32_t len = 0;
    n->m_comment = zip_file_get_comment(zip, n->id(), &len, ZIP_FL_ENC_RAW);
    n->m_commentChanged = false;
    // RAW comment length can't exceed 16 bits
    n->m_commentLen = static_cast<uint16_t>(len);
    if (n->m_comment != NULL && n->m_comment[0] == '\0')
        n->m_comment = NULL;
    if (n->m_comment == NULL)
        n->m_commentLen = 0;

    return n;
}

FileNode::~FileNode() {
    if (state == OPENED || state == CHANGED || state == NEW || state == VIRTUAL_SYMLINK) {
        delete buffer;
    }
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

void FileNode::appendChild (FileNode *child) {
    childs.push_back (child);
}

void FileNode::detachChild (FileNode *child) {
    childs.remove (child);
}

void FileNode::rename(const char *new_name) {
    full_name = new_name;
    m_ctime = currentTime();
    parse_name();
}

int FileNode::open() {
    if (state == NEW || state == VIRTUAL_SYMLINK) {
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
            assert (zip != NULL);
            if (m_size > std::numeric_limits<size_t>::max()) {
                syslog(LOG_WARNING, "file %s is too big for your system :)", full_name.c_str());
                return -ENOMEM;
            }
            buffer = new BigBuffer(zip, id(), static_cast<size_t>(m_size));
            state = OPENED;
        }
        catch (std::bad_alloc&) {
            return -ENOMEM;
        }
        catch (std::exception&) {
            return -EIO;
        }
    }
    return 0;
}

int FileNode::read(char *buf, size_t sz, size_t offset) {
    m_atime = currentTime();
    return buffer->read(buf, sz, offset);
}

int FileNode::write(const char *buf, size_t sz, size_t offset) {
    assert(state != VIRTUAL_SYMLINK);
    if (state == OPENED) {
        state = CHANGED;
    }
    m_mtime = currentTime();
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
    assert (!is_dir);
    // index is modified if state == NEW
    assert(zip != NULL);
    return buffer->saveToZip(m_mtime.tv_sec, zip, full_name.c_str(),
            state == NEW, _id);
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
    assert(state != VIRTUAL_SYMLINK);
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
        m_mtime = currentTime();
        metadataChanged = true;
    } else {
        return EBADF;
    }
}

zip_uint64_t FileNode::size() const {
    if (state == NEW || state == OPENED || state == CHANGED || state == VIRTUAL_SYMLINK) {
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
    assert(_id >= 0);
    assert (zip != NULL);
    zip_file_get_external_attributes(zip, id(), 0, &opsys, &attr);

    mode_t unix_mode = attr >> 16;
    /*
     * PKWARE describes "OS made by" now (since 1998) as follows:
     * The upper byte indicates the compatibility of the file attribute
     * information.  If the external file attributes are compatible with MS-DOS
     * and can be read by PKZIP for DOS version 2.04g then this value will be
     * zero.
     */
    if (opsys == ZIP_OPSYS_DOS && (unix_mode & S_IFMT) != 0)
        opsys = ZIP_OPSYS_UNIX;
    switch (opsys) {
        case ZIP_OPSYS_UNIX: {
            m_mode = unix_mode;
            // force is_dir value
            if (is_dir) {
                m_mode = (m_mode & static_cast<unsigned>(~S_IFMT)) | S_IFDIR;
            } else if ((m_mode & S_IFMT) == S_IFDIR) {
                m_mode = (m_mode & static_cast<unsigned>(~S_IFMT)) | S_IFREG;
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
            m_mode = 0444;
            // http://msdn.microsoft.com/en-us/library/windows/desktop/gg258117%28v=vs.85%29.aspx
            // http://en.wikipedia.org/wiki/File_Allocation_Table#attributes
            // FILE_ATTRIBUTE_READONLY
            if ((attr & 1) == 0) {
                m_mode |= 0220;
            }
            // directory
            if (is_dir) {
                m_mode |= S_IFDIR | 0111;
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
void FileNode::processExtraFields (bool &hasPkWareField) {
    zip_int16_t count;
    // times from timestamp have precedence to UNIX field times
    bool mtimeFromTimestamp = false, atimeFromTimestamp = false;
    // high-precision timestamps have the highest precedence
    bool highPrecisionTime = false;
    // UIDs and GIDs from UNIX extra fields with bigger type IDs have
    // precedence
    int lastProcessedUnixField = 0;

    hasPkWareField = false;

    assert(_id >= 0);
    assert (zip != NULL);

    // read central directory
    count = zip_file_extra_fields_count (zip, id(), ZIP_FL_CENTRAL);
    if (count > 0) {
        for (zip_uint16_t i = 0; i < static_cast<zip_uint16_t>(count); ++i) {
            struct timespec mt, at, cret;
            zip_uint16_t type, len;
            const zip_uint8_t *field = zip_file_extra_field_get (zip,
                    id(), i, &type, &len, ZIP_FL_CENTRAL);

            switch (type) {
                case FZ_EF_PKWARE_UNIX:
                    hasPkWareField = true;
                    processPkWareUnixField(type, len, field,
                            mtimeFromTimestamp, atimeFromTimestamp, highPrecisionTime,
                            lastProcessedUnixField);
                    break;
                case FZ_EF_NTFS:
                    if (ExtraField::parseNtfsExtraField(len, field, mt, at, cret)) {
                        m_mtime = mt;
                        m_atime = at;
                        m_cretime = cret;
                        has_cretime = true;
                        highPrecisionTime = true;
                    }
                    break;
            }
        }
    }

    // read local headers
    count = zip_file_extra_fields_count (zip, id(), ZIP_FL_LOCAL);
    if (count < 0)
        return;
    for (zip_uint16_t i = 0; i < static_cast<zip_uint16_t>(count); ++i) {
        bool has_mt, has_at, has_cret;
        time_t mt, at, cret;
        zip_uint16_t type, len;
        const zip_uint8_t *field = zip_file_extra_field_get (zip,
                id(), i, &type, &len, ZIP_FL_LOCAL);

        switch (type) {
            case FZ_EF_TIMESTAMP: {
                if (ExtraField::parseExtTimeStamp (len, field, has_mt, mt,
                            has_at, at, has_cret, cret)) {
                    if (has_mt && !highPrecisionTime) {
                        m_mtime.tv_sec = mt;
                        m_mtime.tv_nsec = 0;
                        mtimeFromTimestamp = true;
                    }
                    if (has_at && !highPrecisionTime) {
                        m_atime.tv_sec = at;
                        m_atime.tv_nsec = 0;
                        atimeFromTimestamp = true;
                    }
                    if (has_cret && !highPrecisionTime) {
                        m_cretime.tv_sec = cret;
                        m_cretime.tv_nsec = 0;
                        has_cretime = true;
                    }
                }
                break;
            }
            case FZ_EF_PKWARE_UNIX:
                hasPkWareField = true;
                processPkWareUnixField(type, len, field,
                        mtimeFromTimestamp, atimeFromTimestamp, highPrecisionTime,
                        lastProcessedUnixField);
                break;
            case FZ_EF_INFOZIP_UNIX1:
            {
                bool has_uid_gid;
                uid_t uid;
                gid_t gid;
                if (ExtraField::parseSimpleUnixField (type, len, field,
                            has_uid_gid, uid, gid, mt, at)) {
                    if (has_uid_gid && type >= lastProcessedUnixField) {
                        m_uid = uid;
                        m_gid = gid;
                        lastProcessedUnixField = type;
                    }
                    if (!mtimeFromTimestamp && !highPrecisionTime) {
                        m_mtime.tv_sec = mt;
                        m_mtime.tv_nsec = 0;
                    }
                    if (!atimeFromTimestamp && !highPrecisionTime) {
                        m_atime.tv_sec = at;
                        m_atime.tv_nsec = 0;
                    }
                }
                break;
            }
            case FZ_EF_INFOZIP_UNIX2:
            case FZ_EF_INFOZIP_UNIXN: {
                uid_t uid;
                gid_t gid;
                if (ExtraField::parseUnixUidGidField (type, len, field, uid, gid)) {
                    if (type >= lastProcessedUnixField) {
                        m_uid = uid;
                        m_gid = gid;
                        lastProcessedUnixField = type;
                    }
                }
                break;
            }
            case FZ_EF_NTFS: {
                struct timespec mts, ats, bts;
                if (ExtraField::parseNtfsExtraField(len, field, mts, ats, bts)) {
                    m_mtime = mts;
                    m_atime = ats;
                    m_cretime = bts;
                    has_cretime = true;
                    highPrecisionTime = true;
                }
                break;
            }
        }
    }
}

void FileNode::processPkWareUnixField(zip_uint16_t type, zip_uint16_t len, const zip_uint8_t *field,
        bool mtimeFromTimestamp, bool atimeFromTimestamp, bool highPrecisionTime,
        int &lastProcessedUnixField) {
    time_t mt, at;
    uid_t uid;
    gid_t gid;
    dev_t dev;
    const char *link;
    uint16_t link_len;
    if (!ExtraField::parsePkWareUnixField(len, field, m_mode, mt, at,
                uid, gid, dev, link, link_len))
        return;

    if (type >= lastProcessedUnixField) {
        m_uid = uid;
        m_gid = gid;
        lastProcessedUnixField = type;
    }
    if (!mtimeFromTimestamp && !highPrecisionTime) {
        m_mtime.tv_sec = mt;
        m_mtime.tv_nsec = 0;
    }
    if (!atimeFromTimestamp && !highPrecisionTime) {
        m_atime.tv_sec = at;
        m_atime.tv_nsec = 0;
    }
    m_device = dev;
    // use PKWARE link target only if link target in Info-ZIP format is not
    // specified (empty file content)
    if (S_ISLNK(m_mode) && m_size == 0 && link_len > 0) {
        assert(state == CLOSED || state == VIRTUAL_SYMLINK);
        if (state == VIRTUAL_SYMLINK)
        {
            state = CLOSED;
            delete buffer;
        }
        buffer = new BigBuffer();
        if (!buffer)
            return;
        assert(link != NULL);
        buffer->write(link, link_len, 0);
        state = VIRTUAL_SYMLINK;
    }
    // TODO: hardlinks
}

/**
 * Save timestamp, owner and group info into extra fields
 * @return 0 on success
 */
int FileNode::updateExtraFields (bool force_precise_time) const {
    static zip_flags_t locations[] = {ZIP_FL_CENTRAL, ZIP_FL_LOCAL};

    assert(_id >= 0);
    assert (zip != NULL);
    for (unsigned int loc = 0; loc < sizeof(locations) / sizeof(locations[0]); ++loc) {
        std::unique_ptr<zip_uint8_t[]> ntfs_field;
        zip_uint16_t ntfs_field_len;

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
        if (S_ISBLK(m_mode) || S_ISCHR(m_mode) || S_ISFIFO(m_mode)) {
            field = ExtraField::createPkWareUnixField(m_mtime.tv_sec, m_atime.tv_sec, m_mode,
                    m_uid, m_gid, m_device, len);
            res = zip_file_extra_field_set(zip, id(), FZ_EF_PKWARE_UNIX,
                    ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
            if (res != 0) {
                return res;
            }

            // PKZIP 14 for Linux doesn't extract device nodes if extra fields
            // other than PKWARE UNIX are present in the central directory
            if ((S_ISBLK(m_mode) || S_ISCHR(m_mode)) && locations[loc] == ZIP_FL_CENTRAL)
                continue;
        }

        // add timestamps
        field = ExtraField::createExtTimeStamp (locations[loc], m_mtime.tv_sec,
                m_atime.tv_sec, has_cretime, m_cretime.tv_sec, len);
        res = zip_file_extra_field_set (zip, id(), FZ_EF_TIMESTAMP,
                ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
        if (res != 0) {
            return res;
        }
        // PKZIP 14 for Linux doesn't extract symlinks if NTFS extra field
        // is present in the central directory
        if ((has_cretime || force_precise_time) &&
            (locations[loc] == ZIP_FL_LOCAL || !S_ISLNK(m_mode)))
        {
            // add high-precision timestamps
            if (ntfs_field) {
                len = ExtraField::editNtfsExtraField(ntfs_field_len, ntfs_field.get(),
                        m_mtime, m_atime, m_cretime);
                field = ntfs_field.get();
            } else {
                field = ExtraField::createNtfsExtraField (m_mtime, m_atime, m_cretime, len);
            }
            res = zip_file_extra_field_set (zip, id(), FZ_EF_NTFS,
                    ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
            if (res != 0) {
                return res;
            }
        }
        // add UNIX owner info
        field = ExtraField::createInfoZipNewUnixField (m_uid, m_gid, len);
        res = zip_file_extra_field_set (zip, id(), FZ_EF_INFOZIP_UNIXN,
                ZIP_EXTRA_FIELD_NEW, field, len, locations[loc]);
        if (res != 0) {
            return res;
        }
    }
    return 0;
}

void FileNode::chmod (mode_t mode) {
    m_mode = (m_mode & S_IFMT) | mode;
    m_ctime = currentTime();
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
 * @return 0 on success or libzip error code (ZIP_ER_MEMORY or ZIP_ER_RDONLY)
 */
int FileNode::updateExternalAttributes() const {
    assert(_id >= 0);
    assert (zip != NULL);
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
    return zip_file_set_external_attributes (zip, id(), 0,
            ZIP_OPSYS_UNIX, mode);
}

void FileNode::setTimes (const timespec &atime, const timespec &mtime) {
    struct timespec now;
    if (atime.tv_nsec == UTIME_NOW || mtime.tv_nsec == UTIME_NOW)
        now = currentTime();
    if (atime.tv_nsec == UTIME_NOW)
        m_atime = now;
    else if (atime.tv_nsec != UTIME_OMIT)
        m_atime = atime;
    if (mtime.tv_nsec == UTIME_NOW)
        m_mtime = now;
    else if (mtime.tv_nsec != UTIME_OMIT)
        m_mtime = mtime;
    metadataChanged = true;
}

void FileNode::setCTime (const timespec &ctime) {
    m_ctime = ctime;
    metadataChanged = true;
}

struct timespec FileNode::currentTime() {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        ts.tv_sec = time(NULL);
        ts.tv_nsec = 0;
    }
    return ts;
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

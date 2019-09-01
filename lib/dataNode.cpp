////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2019 by Alexander Galanin                               //
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
#include <memory>
#include <stdexcept>

#include <syslog.h>

#include "dataNode.h"
#include "extraField.h"
#include "util.h"

DataNode::DataNode(zip_uint64_t id, mode_t mode, uid_t uid, gid_t gid, dev_t dev) {
    _id = id;
    _open_count = 0;
    _size = 0;

    _mode = mode;
    _uid = uid;
    _gid = gid;
    _device = dev;
}

std::shared_ptr<DataNode> DataNode::createNew(mode_t mode, uid_t uid, gid_t gid, dev_t dev) {
    std::shared_ptr<DataNode> n(new DataNode(FAKE_ID, mode, uid, gid, dev));

    n->_state = NodeState::NEW;
    n->_buffer.reset(new BigBuffer());

    n->_has_btime = true;
    n->_metadataChanged = true;
    n->_mtime = n->_atime = n->_ctime = n->_btime = currentTime();

    return n;
}

std::shared_ptr<DataNode> DataNode::createTmpDir(mode_t mode, uid_t uid, gid_t gid, dev_t dev) {
    std::shared_ptr<DataNode> n(new DataNode(FAKE_ID, mode, uid, gid, dev));

    n->_state = NodeState::NEW;
    n->_buffer.reset(new BigBuffer());

    n->_has_btime = true;
    n->_metadataChanged = false;
    n->_mtime = n->_atime = n->_ctime = n->_btime = currentTime();

    return n;
}

std::shared_ptr<DataNode> DataNode::createExisting(struct zip *zip, zip_uint64_t id, mode_t mode) {
    std::shared_ptr<DataNode> n(new DataNode(id, mode, 0, 0, 0));

    n->_state = NodeState::CLOSED;
    n->_metadataChanged = false;

    struct zip_stat stat;
    zip_stat_index(zip, id, 0, &stat);
    // check that all used fields are valid
    zip_uint64_t needValid = ZIP_STAT_NAME | ZIP_STAT_INDEX |
        ZIP_STAT_SIZE | ZIP_STAT_MTIME;
    // required fields are always valid for existing items or newly added
    // directories (see zip_stat_index.c from libzip)
    assert((stat.valid & needValid) == needValid);
    n->_mtime.tv_sec = n->_atime.tv_sec = n->_ctime.tv_sec = n->_btime.tv_sec = stat.mtime;
    n->_mtime.tv_nsec = n->_atime.tv_nsec = n->_ctime.tv_nsec = n->_btime.tv_nsec = 0;
    n->_has_btime = false;
    n->_size = stat.size;

    bool hasPkWareField;
    n->processExtraFields(zip, hasPkWareField);

    // InfoZIP may produce FIFO-marked node with content, PkZip - can't.
    if (S_ISFIFO(n->_mode)) {
        unsigned int type = S_IFREG;
        if (n->_size == 0 && hasPkWareField)
            type = S_IFIFO;
        n->_mode = (n->_mode & static_cast<unsigned>(~S_IFMT)) | type;
    }

    return n;
}

int DataNode::open(struct zip *zip) {
    if (_state == NodeState::NEW || _state == NodeState::VIRTUAL_SYMLINK) {
        return 0;
    }
    if (_state == NodeState::OPENED) {
        if (_open_count == INT_MAX) {
            return -EMFILE;
        } else {
            ++_open_count;
        }
    }
    if (_state == NodeState::CLOSED) {
        _open_count = 1;
        try {
            assert(zip != NULL);
            if (_size > std::numeric_limits<size_t>::max()) {
                return -ENOMEM;
            }
            assert(_id != FAKE_ID);
            _buffer.reset(new BigBuffer(zip, _id, static_cast<size_t>(_size)));
            _state = NodeState::OPENED;
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

int DataNode::read(char *buf, size_t sz, size_t offset) {
    _atime = currentTime();
    return _buffer->read(buf, sz, offset);
}

int DataNode::write(const char *buf, size_t sz, size_t offset) {
    assert(_state != NodeState::VIRTUAL_SYMLINK);
    if (_state == NodeState::OPENED) {
        _state = NodeState::CHANGED;
    }
    _mtime = currentTime();
    _metadataChanged = true;
    return _buffer->write(buf, sz, offset);
}

int DataNode::close() {
    _size = _buffer->len;
    if (_state == NodeState::OPENED && --_open_count == 0) {
        _buffer.reset();
        _state = NodeState::CLOSED;
    }
    return 0;
}

int DataNode::save(struct zip *zip, const char *full_name, zip_int64_t &index) {
    assert(zip != NULL);
    assert(full_name != NULL);
    return _buffer->saveToZip(_mtime.tv_sec, zip, full_name,
            _state == NodeState::NEW, index);
}

//int DataNode::saveMetadata(bool force_precise_time) const {
//    assert(zip != NULL);
//    assert(_id >= 0);
//
//    int res = updateExtraFields(force_precise_time);
//    if (res != 0)
//        return res;
//    return updateExternalAttributes();
//}

int DataNode::truncate(size_t offset) {
    assert(_state != NodeState::VIRTUAL_SYMLINK);
    if (_state != NodeState::CLOSED) {
        if (_state != NodeState::NEW) {
            _state = NodeState::CHANGED;
        }
        try {
            _buffer->truncate(offset);
            return 0;
        }
        catch (const std::bad_alloc &) {
            return EIO;
        }
        _mtime = currentTime();
        _metadataChanged = true;
    } else {
        return EBADF;
    }
}

zip_uint64_t DataNode::size() const {
    if (_state == NodeState::NEW || _state == NodeState::OPENED || _state == NodeState::CHANGED ||
            _state == NodeState::VIRTUAL_SYMLINK) {
        return _buffer->len;
    } else {
        return _size;
    }
}

/**
 * Get timestamp information from extra fields.
 * Get owner and group information.
 */
void DataNode::processExtraFields(struct zip *zip, bool &hasPkWareField) {
    zip_int16_t count;
    // times from timestamp have precedence to UNIX field times
    bool mtimeFromTimestamp = false, atimeFromTimestamp = false;
    // high-precision timestamps have the highest precedence
    bool highPrecisionTime = false;
    // UIDs and GIDs from UNIX extra fields with bigger type IDs have
    // precedence
    int lastProcessedUnixField = 0;

    hasPkWareField = false;

    assert(zip != NULL);

    // read central directory
    count = zip_file_extra_fields_count (zip, _id, ZIP_FL_CENTRAL);
    if (count > 0) {
        for (zip_uint16_t i = 0; i < static_cast<zip_uint16_t>(count); ++i) {
            struct timespec mt, at, cret;
            zip_uint16_t type, len;
            const zip_uint8_t *field = zip_file_extra_field_get (zip,
                    _id, i, &type, &len, ZIP_FL_CENTRAL);

            switch (type) {
                case FZ_EF_PKWARE_UNIX:
                    hasPkWareField = true;
                    processPkWareUnixField(type, len, field,
                            mtimeFromTimestamp, atimeFromTimestamp, highPrecisionTime,
                            lastProcessedUnixField);
                    break;
                case FZ_EF_NTFS:
                    if (ExtraField::parseNtfsExtraField(len, field, mt, at, cret)) {
                        _mtime = mt;
                        _atime = at;
                        _btime = cret;
                        _has_btime = true;
                        highPrecisionTime = true;
                    }
                    break;
            }
        }
    }

    // read local headers
    count = zip_file_extra_fields_count (zip, _id, ZIP_FL_LOCAL);
    if (count < 0)
        return;
    for (zip_uint16_t i = 0; i < static_cast<zip_uint16_t>(count); ++i) {
        bool has_mt, has_at, has_cret;
        time_t mt, at, cret;
        zip_uint16_t type, len;
        const zip_uint8_t *field = zip_file_extra_field_get (zip,
                _id, i, &type, &len, ZIP_FL_LOCAL);

        switch (type) {
            case FZ_EF_TIMESTAMP: {
                if (ExtraField::parseExtTimeStamp (len, field, has_mt, mt,
                            has_at, at, has_cret, cret)) {
                    if (has_mt && !highPrecisionTime) {
                        _mtime.tv_sec = mt;
                        _mtime.tv_nsec = 0;
                        mtimeFromTimestamp = true;
                    }
                    if (has_at && !highPrecisionTime) {
                        _atime.tv_sec = at;
                        _atime.tv_nsec = 0;
                        atimeFromTimestamp = true;
                    }
                    if (has_cret && !highPrecisionTime) {
                        _btime.tv_sec = cret;
                        _btime.tv_nsec = 0;
                        _has_btime = true;
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
                        _uid = uid;
                        _gid = gid;
                        lastProcessedUnixField = type;
                    }
                    if (!mtimeFromTimestamp && !highPrecisionTime) {
                        _mtime.tv_sec = mt;
                        _mtime.tv_nsec = 0;
                    }
                    if (!atimeFromTimestamp && !highPrecisionTime) {
                        _atime.tv_sec = at;
                        _atime.tv_nsec = 0;
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
                        _uid = uid;
                        _gid = gid;
                        lastProcessedUnixField = type;
                    }
                }
                break;
            }
            case FZ_EF_NTFS: {
                struct timespec mts, ats, bts;
                if (ExtraField::parseNtfsExtraField(len, field, mts, ats, bts)) {
                    _mtime = mts;
                    _atime = ats;
                    _btime = bts;
                    _has_btime = true;
                    highPrecisionTime = true;
                }
                break;
            }
        }
    }
}

void DataNode::processPkWareUnixField(zip_uint16_t type, zip_uint16_t len, const zip_uint8_t *field,
        bool mtimeFromTimestamp, bool atimeFromTimestamp, bool highPrecisionTime,
        int &lastProcessedUnixField) {
    time_t mt, at;
    uid_t uid;
    gid_t gid;
    dev_t dev;
    const char *link;
    uint16_t link_len;
    if (!ExtraField::parsePkWareUnixField(len, field, _mode, mt, at,
                uid, gid, dev, link, link_len))
        return;

    if (type >= lastProcessedUnixField) {
        _uid = uid;
        _gid = gid;
        lastProcessedUnixField = type;
    }
    if (!mtimeFromTimestamp && !highPrecisionTime) {
        _mtime.tv_sec = mt;
        _mtime.tv_nsec = 0;
    }
    if (!atimeFromTimestamp && !highPrecisionTime) {
        _atime.tv_sec = at;
        _atime.tv_nsec = 0;
    }
    _device = dev;
    // use PKWARE link target only if link target in Info-ZIP format is not
    // specified (empty file content)
    if (S_ISLNK(_mode) && _size == 0 && link_len > 0) {
        assert(_state == NodeState::CLOSED || _state == NodeState::VIRTUAL_SYMLINK);
        if (_state == NodeState::VIRTUAL_SYMLINK)
        {
            _state = NodeState::CLOSED;
            _buffer.reset();
        }
        _buffer.reset(new BigBuffer());
        if (!_buffer)
            return;
        assert(link != NULL);
        _buffer->write(link, link_len, 0);
        _state = NodeState::VIRTUAL_SYMLINK;
    }
    // hardlinks are handled in FuseZipData::build_tree
}

void DataNode::chmod (mode_t mode) {
    _mode = (_mode & S_IFMT) | mode;
    _ctime = currentTime();
    _metadataChanged = true;
}

void DataNode::setUid (uid_t uid) {
    _uid = uid;
    _metadataChanged = true;
}

void DataNode::setGid (gid_t gid) {
    _gid = gid;
    _metadataChanged = true;
}

void DataNode::setTimes (const timespec &atime, const timespec &mtime) {
    struct timespec now;
    if (atime.tv_nsec == UTIME_NOW || mtime.tv_nsec == UTIME_NOW)
        now = currentTime();
    if (atime.tv_nsec == UTIME_NOW)
        _atime = now;
    else if (atime.tv_nsec != UTIME_OMIT)
        _atime = atime;
    if (mtime.tv_nsec == UTIME_NOW)
        _mtime = now;
    else if (mtime.tv_nsec != UTIME_OMIT)
        _mtime = mtime;
    _metadataChanged = true;
}

void DataNode::setCTime (const timespec &ctime) {
    _ctime = ctime;
    _metadataChanged = true;
}

void DataNode::touchCTime() {
    _ctime = currentTime();
}

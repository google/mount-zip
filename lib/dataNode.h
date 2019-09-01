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

#ifndef DATA_NODE_H
#define DATA_NODE_H

#include <limits>
#include <memory>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include "types.h"
#include "bigBuffer.h"

class DataNode {
private:
    DataNode (const DataNode &) = delete;
    DataNode &operator= (const DataNode &) = delete;

    static constexpr zip_uint64_t FAKE_ID { std::numeric_limits<zip_uint64_t>::max() };

    enum class NodeState {
        CLOSED,
        OPENED,
        VIRTUAL_SYMLINK,
        CHANGED,
        NEW
    };

    zip_uint64_t _id;
    std::unique_ptr<BigBuffer> _buffer;
    int _open_count;
    NodeState _state;

    zip_uint64_t _size;
    bool _has_btime, _metadataChanged;
    mode_t _mode;
    dev_t _device;
    struct timespec _mtime, _atime, _ctime, _btime;
    uid_t _uid;
    gid_t _gid;

    DataNode(zip_uint64_t id, mode_t mode, uid_t uid, gid_t gid, dev_t dev);

    void processExtraFields(struct zip *zip, bool &hasPkWareField);
    void processPkWareUnixField(zip_uint16_t type, zip_uint16_t len, const zip_uint8_t *field,
            bool mtimeFromTimestamp, bool atimeFromTimestamp, bool highPrecisionTime,
            int &lastProcessedUnixField);
    int updateExtraFields(bool force_precise_time) const;
    int updateExternalAttributes() const;

public:
    static std::shared_ptr<DataNode> createNew(mode_t mode, uid_t uid, gid_t gid, dev_t dev);
    static std::shared_ptr<DataNode> createTmpDir(mode_t mode, uid_t uid, gid_t gid, dev_t dev);
    static std::shared_ptr<DataNode> createExisting(struct zip *zip, zip_uint64_t id, mode_t mode);

    int open(struct zip *zip);
    int read(char *buf, size_t size, size_t offset);
    int write(const char *buf, size_t size, size_t offset);
    int close();

    /**
     * Invoke zip_file_add() or zip_file_replace() for file to save it.
     * Should be called only if item is needed to ba saved into zip file.
     *
     * @param zip zip structure pointer
     * @param full_name full file name
     * @param index file node index (updated if state is NEW)
     * @return 0 if success, != 0 on error
     */
    int save(struct zip *zip, const char *full_name, zip_int64_t &index);

    /**
     * Save file metadata to ZIP
     * @param force_precise_time force creation of NTFS extra field
     * @return libzip error code or 0 on success
     */
    int saveMetadata (bool force_precise_time) const;

    /**
     * Truncate file.
     *
     * @return
     *      0       If successful
     *      EBADF   If file is currently closed
     *      EIO     If insufficient memory available (because ENOMEM not
     *              listed in truncate() error codes)
     */
    int truncate(size_t offset);

    inline bool isChanged() const {
        return _state == NodeState::CHANGED
            || _state == NodeState::NEW
            || (_state == NodeState::VIRTUAL_SYMLINK && _metadataChanged);
    }

    inline bool isMetadataChanged() const {
        return _metadataChanged;
    }

    /**
     * Change file mode
     */
    void chmod (mode_t mode);
    inline mode_t mode() const {
        return _mode;
    }

    inline dev_t device() const {
        return _device;
    }

    /**
     * set atime and mtime
     */
    void setTimes (const timespec &atime, const timespec &mtime);

    void setCTime (const timespec &ctime);

    void touchCTime();

    inline struct timespec atime() const {
        return _atime;
    }
    inline struct timespec ctime() const {
        return _ctime;
    }
    inline struct timespec mtime() const {
        return _mtime;
    }
    inline bool has_btime() const { return _has_btime; }
    inline struct timespec btime() const {
        return _btime;
    }

    /**
     * owner and group
     */
    void setUid (uid_t uid);
    void setGid (gid_t gid);
    inline uid_t uid () const {
        return _uid;
    }
    inline gid_t gid () const {
        return _gid;
    }

    zip_uint64_t size() const;
};
#endif


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

#ifndef FILE_NODE_H
#define FILE_NODE_H

#include <memory>
#include <string>

#include <unistd.h>
#include <sys/stat.h>

#include "types.h"
#include "bigBuffer.h"
#include "dataNode.h"

class FileNode {
friend class FuseZipData;
private:
    // must not be defined
    FileNode (const FileNode &);
    FileNode &operator= (const FileNode &);

    zip_int64_t _id;
    std::shared_ptr<DataNode> _data;

    struct zip *zip;

    const char *m_comment;
    uint16_t m_commentLen;
    bool m_commentChanged;

    void parse_name();
    void readComment();
    int updateExtraFields(bool force_precise_time) const;
    int updateExternalAttributes() const;

    static const zip_int64_t ROOT_NODE_INDEX, NEW_NODE_INDEX, TMP_DIR_INDEX;
    FileNode(struct zip *zip, const char *fname, zip_int64_t id, std::shared_ptr<DataNode> data);

protected:
    static FileNode *createIntermediateDir(struct zip *zip, const char *fname);

public:
    /**
     * Create new regular file
     */
    static FileNode *createFile(struct zip *zip, const char *fname,
            uid_t owner, gid_t group, mode_t mode, dev_t dev = 0);
    /**
     * Create new symbolic link
     */
    static FileNode *createSymlink(struct zip *zip, const char *fname);
    /**
     * Create new directory for ZIP file entry
     */
    static FileNode *createDir(struct zip *zip, const char *fname,
            zip_int64_t id, uid_t owner, gid_t group, mode_t mode);
    /**
     * Create root pseudo-node for file system
     */
    static FileNode *createRootNode(struct zip *zip);
    /**
     * Create node for existing ZIP file entry
     */
    static FileNode *createNodeForZipEntry(struct zip *zip,
            const char *fname, zip_int64_t id, mode_t mode);
    /**
     * Create hardlink to another node
     */
    static FileNode *createHardlink(struct zip *zip,
            const char *fname, zip_int64_t id, FileNode *target);
    ~FileNode();

    /**
     * return a pointer to data structure
     */
    DataNode *data() {
        return _data.get();
    }
    
    /**
     * add child node to list
     */
    void appendChild (FileNode *child);

    /**
     * remove child node from list
     */
    void detachChild (FileNode *child);

    /**
     * Rename file without reparenting
     */
    void rename (const char *new_name);

    int open();
    int read(char *buf, size_t size, size_t offset);
    int write(const char *buf, size_t size, size_t offset);
    int close();

    /**
     * Invoke zip_file_add() or zip_file_replace() for file to save it.
     * Should be called only if item is needed to ba saved into zip file.
     *
     * @return 0 if success, != 0 on error
     */
    int save();

    /**
     * Save file metadata to ZIP
     * @param force_precise_time force creation of NTFS extra field
     * @return libzip error code or 0 on success
     */
    int saveMetadata (bool force_precise_time) const;

    /**
     * Save file or archive comment into ZIP
     * @return libzip error code or 0 on success
     */
    int saveComment() const;

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
        return _data->isChanged();
    }

    inline bool isMetadataChanged() const {
        return _data->isMetadataChanged();
    }

    inline bool isTemporaryDir() const {
        return _id == TMP_DIR_INDEX;
    }

    /**
     * Change file mode
     */
    void chmod (mode_t mode);
    inline mode_t mode() const {
        return _data->mode();
    }

    inline dev_t device() const {
        return _data->device();
    }

    /**
     * set atime and mtime
     */
    void setTimes (const timespec &atime, const timespec &mtime);

    void setCTime (const timespec &ctime);

    inline struct timespec atime() const {
        return _data->atime();
    }
    inline struct timespec ctime() const {
        return _data->ctime();
    }
    inline struct timespec mtime() const {
        return _data->mtime();
    }

    /**
     * Get parent name
     */
    //TODO: rewrite without memory allocation
    inline std::string getParentName () const {
        if (name > full_name.c_str()) {
            return std::string (full_name, 0, static_cast<size_t>(name - full_name.c_str() - 1));
        } else {
            return "";
        }
    }

    /**
     * owner and group
     */
    void setUid (uid_t uid);
    void setGid (gid_t gid);
    inline uid_t uid () const {
        return _data->uid();
    }
    inline gid_t gid () const {
        return _data->gid();
    }

    zip_uint64_t size() const;

    bool present_in_zip() const { return _id >= 0; }
    zip_uint64_t id() const { return static_cast<zip_uint64_t>(_id); }

    void set_id(zip_int64_t id_) {
        _id = id_;
        // called only from FuseZipData::save, so we're don't worry about 'status' variable value
    }

    bool is_dir() const;

    bool hasComment() const { return m_comment != NULL; }
    bool isCommentChanged() const { return m_commentChanged; }
    bool setComment(const char *value, uint16_t length);
    const char *getComment() const { return m_comment; }
    uint16_t getCommentLength() const { return m_commentLen; }

    const char *name;
    std::string full_name;
    nodelist_t childs;
    FileNode *parent;
};
#endif


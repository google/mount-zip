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

#ifndef FILE_NODE_H
#define FILE_NODE_H

#include <string>
#include <unistd.h>
#include <sys/stat.h>

#include "types.h"
#include "bigBuffer.h"

class FileNode {
private:
    enum nodeState {
        CLOSED,
        OPENED,
        CHANGED,
        NEW,
        NEW_DIR
    };
    static const int EXT_TIMESTAMP;

    BigBuffer *buffer;
    FuseZipData *data;
    int open_count;
    nodeState state;

    void parse_name();
    void attach();
    void processExtraFields();
    void processExternalAttributes();
    int updateExtraFields();
    int updateExternalAttributes();

    static const zip_int64_t ROOT_NODE_INDEX, NEW_NODE_INDEX;
    FileNode(FuseZipData *_data, const char *fname, zip_int64_t id);

    static FileNode *createIntermediateDir(FuseZipData *data, const char *fname);
public:
    /**
     * Create new regular file
     */
    static FileNode *createFile(FuseZipData *data, const char *fname,
            mode_t mode);
    /**
     * Create new directory for ZIP file entry
     */
    static FileNode *createDir(FuseZipData *data, const char *fname,
            zip_int64_t id, mode_t mode);
    /**
     * Create root pseudo-node for file system
     */
    static FileNode *createRootNode(FuseZipData *data);
    /**
     * Create node for existing ZIP file entry
     */
    static FileNode *createNodeForZipEntry(FuseZipData *data,
            const char *fname, zip_int64_t id);
    ~FileNode();

    void detach();
    void rename(const char *fname);

    /**
     * Rename file without reparenting.
     *
     * 1. Remove file item from tree
     * 2. Parse new name
     * 3. Create link to new name in tree
     */
    void rename_wo_reparenting(const char *new_name);

    int open();
    int read(char *buf, size_t size, zip_uint64_t offset);
    int write(const char *buf, size_t size, zip_uint64_t offset);
    int close();

    /**
     * Invoke zip_add() or zip_replace() for file to save it.
     * Should be called only if item is needed to ba saved into zip file.
     *
     * @return 0 if success, != 0 on error
     */
    int save();

    /**
     * Truncate file.
     *
     * @return
     *      0       If successful
     *      EBADF   If file is currently closed
     *      EIO     If insufficient memory available (because ENOMEM not
     *              listed in truncate() error codes)
     */
    int truncate(zip_uint64_t offset);

    inline bool isChanged() const {
        return state == CHANGED || state == NEW;
    }

    /**
     * Change file mode
     */
    void chmod (mode_t mode);

    zip_uint64_t size() const;

    const char *name;
    std::string full_name;
    bool is_dir;
    zip_int64_t id;
    nodelist_t childs;
    FileNode *parent;

    bool has_cretime;
    time_t mtime, atime, ctime, cretime;
    zip_uint64_t m_size;
    mode_t mode;

    class AlreadyExists {
    };
};
#endif


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

#ifndef BIG_BUFFER_H
#define BIG_BUFFER_H

#include <zip.h>
#include <unistd.h>

#include <vector>

#include "types.h"

class BigBuffer {
private:
    //TODO: use >> and <<
    static const unsigned int chunkSize = 4*1024; //4 Kilobytes

    class ChunkWrapper;

    typedef std::vector<ChunkWrapper> chunks_t;

    struct CallBackStruct {
        size_t pos;
        const BigBuffer *buf;
        const FileNode *fileNode;
    };

    chunks_t chunks;

    static ssize_t zipUserFunctionCallback(void *state, void *data, size_t len, enum zip_source_cmd cmd);

    /**
     * Return number of chunks needed to keep 'offset' bytes.
     */
    inline unsigned int chunksCount(offset_t offset) const {
        return (offset + chunkSize - 1) / chunkSize;
    }

    /**
     * Return number of chunk where 'offset'-th byte is located.
     */
    inline unsigned int chunkNumber(offset_t offset) const {
        return offset / chunkSize;
    }

    /**
     * Return offset inside chunk to 'offset'-th byte.
     */
    inline int chunkOffset(offset_t offset) const {
        return offset % chunkSize;
    }

public:
    offset_t len;

    BigBuffer();
    BigBuffer(struct zip *z, int nodeId, ssize_t length);
    ~BigBuffer();

    int read(char *buf, size_t size, offset_t offset) const;
    int write(const char *buf, size_t size, offset_t offset);
    int saveToZip(const FileNode *fileNode, struct zip *z, const char *fname, bool newFile, int index);

    /**
     * Truncate buffer at position offset.
     * 1. Free chunks after offset
     * 2. Resize chunks vector to a new size
     * 3. Fill data block that made readable by resize with zeroes
     *
     * @throws
     *      std::bad_alloc  If insufficient memory available
     */
    void truncate(offset_t offset);
};

#endif


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
        time_t mtime;
    };

    chunks_t chunks;

    /**
     * Callback for zip_source_function.
     * ZIP_SOURCE_CLOSE is not needed to be handled, ZIP_SOURCE_ERROR is
     * never called because read() always successfull.
     * See zip_source_function(3) for details.
     */
    static ssize_t zipUserFunctionCallback(void *state, void *data, size_t len, enum zip_source_cmd cmd);

    /**
     * Return number of chunks needed to keep 'offset' bytes.
     */
    inline static unsigned int chunksCount(zip_uint64_t offset) {
        return (offset + chunkSize - 1) / chunkSize;
    }

    /**
     * Return number of chunk where 'offset'-th byte is located.
     */
    inline static unsigned int chunkNumber(zip_uint64_t offset) {
        return offset / chunkSize;
    }

    /**
     * Return offset inside chunk to 'offset'-th byte.
     */
    inline static int chunkOffset(zip_uint64_t offset) {
        return offset % chunkSize;
    }

public:
    zip_uint64_t len;

    /**
     * Create new file buffer without mapping to file in a zip archive
     */
    BigBuffer();

    /**
     * Read file data from file inside zip archive
     *
     * @param z         Zip file
     * @param nodeId    Node index inside zip file
     * @param length    File length
     * @throws 
     *      std::exception  On file read error
     *      std::bad_alloc  On memory insufficiency
     */
    BigBuffer(struct zip *z, int nodeId, ssize_t length);

    ~BigBuffer();

    /**
     * Dispatch read requests to chunks of a file and write result to
     * resulting buffer.
     * Reading after end of file is not allowed, so 'size' is decreased to
     * fit file boundaries.
     *
     * @param buf       destination buffer
     * @param size      requested bytes count
     * @param offset    offset to start reading from
     * @return always 0
     */
    int read(char *buf, size_t size, zip_uint64_t offset) const;

    /**
     * Dispatch write request to chunks of a file and grow 'chunks' vector if
     * necessary.
     * If 'offset' is after file end, tail of last chunk cleared before growing.
     *
     * @param buf       Source buffer
     * @param size      Number of bytes to be written
     * @param offset    Offset in file to start writing from
     * @return number of bytes written
     * @throws
     *      std::bad_alloc  If there are no memory for buffer
     */
    int write(const char *buf, size_t size, zip_uint64_t offset);

    /**
     * Create (or replace) file element in zip file. Class instance should
     * not be destroyed until zip_close() is called.
     *
     * @param mtime     File modification time
     * @param z         ZIP archive structure
     * @param fname     File name
     * @param newFile   Is file not yet created?
     * @param index     File index in ZIP archive
     * @return
     *      0       If successfull
     *      -ENOMEM If there are no memory
     */
    int saveToZip(time_t mtime, struct zip *z, const char *fname,
            bool newFile, zip_int64_t index);

    /**
     * Truncate buffer at position offset.
     * 1. Free chunks after offset
     * 2. Resize chunks vector to a new size
     * 3. Fill data block that made readable by resize with zeroes
     *
     * @throws
     *      std::bad_alloc  If insufficient memory available
     */
    void truncate(zip_uint64_t offset);
};

#endif


////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2013 by Alexander Galanin                          //
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
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <syslog.h>

#include "bigBuffer.h"

/**
 * Class that keep chunk of file data.
 */
class BigBuffer::ChunkWrapper {
private:
    /**
     * Pointer that keeps data for chunk. Can be NULL.
     */
    char *m_ptr;

public:
    /**
     * By default internal buffer is NULL, so this can be used for creating
     * sparse files.
     */
    ChunkWrapper(): m_ptr(NULL) {
    }

    /**
     * Take ownership on internal pointer from 'other' object.
     */
    ChunkWrapper(const ChunkWrapper &other) {
        m_ptr = other.m_ptr;
        const_cast<ChunkWrapper*>(&other)->m_ptr = NULL;
    }

    /**
     * Free pointer if allocated.
     */
    ~ChunkWrapper() {
        if (m_ptr != NULL) {
            free(m_ptr);
        }
    }

    /**
     * Take ownership on internal pointer from 'other' object.
     */
    ChunkWrapper &operator=(const ChunkWrapper &other) {
        if (&other != this) {
            m_ptr = other.m_ptr;
            const_cast<ChunkWrapper*>(&other)->m_ptr = NULL;
        }
        return *this;
    }

    /**
     * Return pointer to internal storage and initialize it if needed.
     * @throws
     *      std::bad_alloc  If memory can not be allocated
     */
    char *ptr(bool init = false) {
        if (init && m_ptr == NULL) {
            m_ptr = (char *)malloc(chunkSize);
            if (m_ptr == NULL) {
                throw std::bad_alloc();
            }
        }
        return m_ptr;
    }

    /**
     * Fill 'dest' with internal buffer content.
     * If m_ptr is NULL, destination bytes is zeroed.
     *
     * @param dest      Destination buffer.
     * @param offset    Offset in internal buffer to start reading from.
     * @param count     Number of bytes to be read.
     *
     * @return  Number of bytes actually read. It can differ with 'count'
     *      if offset+count>chunkSize.
     */
    size_t read(char *dest, zip_uint64_t offset, size_t count) const {
        if (offset + count > chunkSize) {
            count = chunkSize - offset;
        }
        if (m_ptr != NULL) {
            memcpy(dest, m_ptr + offset, count);
        } else {
            memset(dest, 0, count);
        }
        return count;
    }

    /**
     * Fill internal buffer with bytes from 'src'.
     * If m_ptr is NULL, memory for buffer is malloc()-ed and then head of
     * allocated space is zeroed. After that byte copying is performed.
     *
     * @param src       Source buffer.
     * @param offset    Offset in internal buffer to start writting from.
     * @param count     Number of bytes to be written.
     *
     * @return  Number of bytes actually written. It can differ with
     *      'count' if offset+count>chunkSize.
     * @throws
     *      std::bad_alloc  If there are no memory for buffer
     */
    size_t write(const char *src, zip_uint64_t offset, size_t count) {
        if (offset + count > chunkSize) {
            count = chunkSize - offset;
        }
        if (m_ptr == NULL) {
            m_ptr = (char *)malloc(chunkSize);
            if (m_ptr == NULL) {
                throw std::bad_alloc();
            }
            if (offset > 0) {
                memset(m_ptr, 0, offset);
            }
        }
        memcpy(m_ptr + offset, src, count);
        return count;
    }

    /**
     * Clear tail of internal buffer with zeroes starting from 'offset'.
     */
    void clearTail(zip_uint64_t offset) {
        if (m_ptr != NULL && offset < chunkSize) {
            memset(m_ptr + offset, 0, chunkSize - offset);
        }
    }

};

BigBuffer::BigBuffer(): len(0) {
}

BigBuffer::BigBuffer(struct zip *z, zip_uint64_t nodeId, zip_uint64_t length):
        len(length) {
    struct zip_file *zf = zip_fopen_index(z, nodeId, 0);
    if (zf == NULL) {
        syslog(LOG_WARNING, "%s", zip_strerror(z));
        throw std::runtime_error(zip_strerror(z));
    }
    unsigned int ccount = chunksCount(length);
    chunks.resize(ccount, ChunkWrapper());
    unsigned int chunk = 0;
    int nr;
    while (length > 0) {
        zip_uint64_t readSize = chunkSize;
        if (readSize > length) {
            readSize = length;
        }
        nr = zip_fread(zf, chunks[chunk].ptr(true), readSize);
        if (nr < 0) {
            std::string err = zip_file_strerror(zf);
            syslog(LOG_WARNING, "%s", err.c_str());
            zip_fclose(zf);
            throw std::runtime_error(err);
        }
        ++chunk;
        length -= nr;
        if ((nr == 0 || chunk == ccount) && length != 0) {
            // Allocated memory are exhausted, but there are unread bytes (or
            // file is longer that given length). Possibly CRC error.
            zip_fclose(zf);
            syslog(LOG_WARNING, "length of file %s differ from data length",
                    zip_get_name(z, nodeId, ZIP_FL_ENC_RAW));
            throw std::runtime_error("data length differ");
        }
    }
    if (zip_fclose(zf)) {
        syslog(LOG_WARNING, "%s", zip_strerror(z));
        throw std::runtime_error(zip_strerror(z));
    }
}

BigBuffer::~BigBuffer() {
}

int BigBuffer::read(char *buf, size_t size, zip_uint64_t offset) const {
    if (offset > len) {
        return 0;
    }
    int chunk = chunkNumber(offset);
    int pos = chunkOffset(offset);
    if (size > unsigned(len - offset)) {
        size = len - offset;
    }
    int nread = size;
    while (size > 0) {
        size_t r = chunks[chunk].read(buf, pos, size);

        size -= r;
        buf += r;
        ++chunk;
        pos = 0;
    }
    return nread;
}

int BigBuffer::write(const char *buf, size_t size, zip_uint64_t offset) {
    int chunk = chunkNumber(offset);
    int pos = chunkOffset(offset);
    int nwritten = size;

    if (offset > len) {
        if (len > 0) {
            chunks[chunkNumber(len)].clearTail(chunkOffset(len));
        }
        len = size + offset;
    } else if (size > unsigned(len - offset)) {
        len = size + offset;
    }
    chunks.resize(chunksCount(len));
    while (size > 0) {
        size_t w = chunks[chunk].write(buf, pos, size);

        size -= w;
        buf += w;
        ++ chunk;
        pos = 0;
    }
    return nwritten;
}

void BigBuffer::truncate(zip_uint64_t offset) {
    chunks.resize(chunksCount(offset));

    if (offset > len && len > 0) {
        // Fill end of last non-empty chunk with zeroes
        chunks[chunkNumber(len)].clearTail(chunkOffset(len));
    }

    len = offset;
}

zip_int64_t BigBuffer::zipUserFunctionCallback(void *state, void *data,
        zip_uint64_t len, enum zip_source_cmd cmd) {
    CallBackStruct *b = (CallBackStruct*)state;
    switch (cmd) {
        case ZIP_SOURCE_OPEN: {
            b->pos = 0;
            return 0;
        }
        case ZIP_SOURCE_READ: {
            int r = b->buf->read((char*)data, len, b->pos);
            b->pos += r;
            return r;
        }
        case ZIP_SOURCE_STAT: {
            struct zip_stat *st = (struct zip_stat*)data;
            zip_stat_init(st);
            st->size = b->buf->len;
            st->mtime = b->mtime;
            return sizeof(struct zip_stat);
        }
        case ZIP_SOURCE_FREE: {
            delete b;
            return 0;
        }
        default: {
            return 0;
        }
    }
}

int BigBuffer::saveToZip(time_t mtime, struct zip *z, const char *fname,
        bool newFile, zip_int64_t index) {
    struct zip_source *s;
    struct CallBackStruct *cbs = new CallBackStruct();
    cbs->buf = this;
    cbs->mtime = mtime;
    if ((s=zip_source_function(z, zipUserFunctionCallback, cbs)) == NULL) {
        delete cbs;
        return -ENOMEM;
    }
    if ((newFile && zip_file_add(z, fname, s, ZIP_FL_ENC_UTF_8) < 0)
            || (!newFile && zip_file_replace(z, index, s, ZIP_FL_ENC_UTF_8) < 0)) {
        delete cbs;
        zip_source_free(s);
        return -ENOMEM;
    }
    return 0;
}

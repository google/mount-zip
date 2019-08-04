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

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#include <limits.h>
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
    size_t read(char *dest, unsigned int offset, size_t count) const {
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
    size_t write(const char *src, unsigned int offset, size_t count) {
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
    void clearTail(unsigned int offset) {
        if (m_ptr != NULL && offset < chunkSize) {
            memset(m_ptr + offset, 0, chunkSize - offset);
        }
    }

};

BigBuffer::BigBuffer(): len(0) {
}

BigBuffer::BigBuffer(struct zip *z, zip_uint64_t nodeId, size_t length):
        len(length) {
    struct zip_file *zf = zip_fopen_index(z, nodeId, 0);
    if (zf == NULL) {
        syslog(LOG_WARNING, "%s", zip_strerror(z));
        throw std::runtime_error(zip_strerror(z));
    }
    size_t ccount = chunksCount(length);
    chunks.resize(ccount, ChunkWrapper());
    size_t chunk = 0;
    while (length > 0) {
        size_t readSize = chunkSize;
        if (readSize > length) {
            readSize = length;
        }
        zip_int64_t nr = zip_fread(zf, chunks[chunk].ptr(true), readSize);
        if (nr < 0) {
            std::string err = zip_file_strerror(zf);
            syslog(LOG_WARNING, "%s", err.c_str());
            zip_fclose(zf);
            throw std::runtime_error(err);
        }
        ++chunk;
        length -= static_cast<size_t>(nr);
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

int BigBuffer::read(char *buf, size_t size, size_t offset) const {
    if (offset > len) {
        return 0;
    }
    size_t chunk = chunkNumber(offset);
    unsigned int pos = chunkOffset(offset);
    if (size > len - offset) {
        size = len - offset;
    }
    if (size > INT_MAX)
        size = INT_MAX;
    int nread = static_cast<int>(size);
    while (size > 0) {
        size_t r = chunks[chunk].read(buf, pos, size);

        size -= r;
        buf += r;
        ++chunk;
        pos = 0;
    }
    return nread;
}

int BigBuffer::write(const char *buf, size_t size, size_t offset) {
    size_t chunk = chunkNumber(offset);
    unsigned int pos = chunkOffset(offset);
    if (size > INT_MAX)
        size = INT_MAX;
    int nwritten = static_cast<int>(size);

    if (offset > len) {
        if (chunkNumber(len) < chunksCount(len)) {
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

void BigBuffer::truncate(size_t offset) {
    chunks.resize(chunksCount(offset));

    if (offset > len && chunkNumber(len) < chunksCount(len)) {
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
            size_t rlen = std::numeric_limits<size_t>::max();
            if (len < rlen)
                rlen = static_cast<size_t>(len);
            int r = b->buf->read((char*)data, rlen, b->pos);
            b->pos += static_cast<unsigned int>(r);
            return r;
        }
        case ZIP_SOURCE_STAT: {
            struct zip_stat *st = (struct zip_stat*)data;
            zip_stat_init(st);
            st->valid = ZIP_STAT_SIZE | ZIP_STAT_MTIME;
            st->size = b->buf->len;
            st->mtime = b->mtime;
            return sizeof(struct zip_stat);
        }
        case ZIP_SOURCE_FREE: {
            delete b;
            return 0;
        }
        case ZIP_SOURCE_CLOSE:
            return 0;
        case ZIP_SOURCE_ERROR: {
            // This code should not be called in normal case because none of
            // implemented functions raises error flag.
            int *errs = static_cast<int *>(data);
            errs[0] = ZIP_ER_OPNOTSUPP;
            errs[1] = EINVAL;
            return 2 * sizeof(int);
        }
        case ZIP_SOURCE_SUPPORTS:
            return ZIP_SOURCE_SUPPORTS_READABLE;
        default:
            // indicate unsupported operation
            return -1;
    }
}

int BigBuffer::saveToZip(time_t mtime, struct zip *z, const char *fname,
        bool newFile, zip_int64_t &index) {
    struct zip_source *s;
    struct CallBackStruct *cbs = new CallBackStruct();
    cbs->buf = this;
    cbs->mtime = mtime;
    if ((s=zip_source_function(z, zipUserFunctionCallback, cbs)) == NULL) {
        delete cbs;
        return -ENOMEM;
    }
    if (newFile) {
        zip_int64_t nid = zip_file_add(z, fname, s, ZIP_FL_ENC_GUESS);
        if (nid < 0) {
            delete cbs;
            zip_source_free(s);
            return -ENOMEM;
        } else {
            // indices are actually in range [0..2^63-1]
            index = nid;
        }
    } else {
        assert(index >= 0);
        if (zip_file_replace(z, static_cast<zip_uint64_t>(index), s, ZIP_FL_ENC_GUESS) < 0) {
            delete cbs;
            zip_source_free(s);
            return -ENOMEM;
        }
    }
    return 0;
}

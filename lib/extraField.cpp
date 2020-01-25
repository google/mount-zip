////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2014-2020 by Alexander Galanin                          //
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

#include "extraField.h"

#include <sys/stat.h>

#if ! __APPLE__
#   include <sys/sysmacros.h>
#endif // !__APPLE__

#include <cassert>
#include <cstring>

static const uint64_t NTFS_TO_UNIX_OFFSET = ((uint64_t)(369 * 365 + 89) * 24 * 3600 * 10000000);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define le_16(x) (x)
#define le_32(x) (x)
#define le_64(x) (x)

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

uint16_t le_16(uint16_t x) { return __builtin_bswap16(x); }
uint32_t le_32(uint32_t x) { return __builtin_bswap32(x); }
uint64_t le_64(uint64_t x) { return __builtin_bswap64(x); }

#else
#    error "bad byte order"
#endif

uint64_t
ExtraField::getLong64 (const zip_uint8_t *&data) {
#pragma pack(push,1)
    struct tmp {
        uint64_t value;
    };
#pragma pack(pop)
    uint64_t res = reinterpret_cast<const tmp*>(data)->value;
    data += 8;
    return le_64(res);
}

unsigned long
ExtraField::getLong (const zip_uint8_t *&data) {
#pragma pack(push,1)
    struct tmp {
        uint32_t value;
    };
#pragma pack(pop)
    uint32_t res = reinterpret_cast<const tmp*>(data)->value;
    data += 4;
    return le_32(res);
}

unsigned short 
ExtraField::getShort (const zip_uint8_t *&data) {
#pragma pack(push,1)
    struct tmp {
        uint16_t value;
    };
#pragma pack(pop)
    uint16_t res = reinterpret_cast<const tmp*>(data)->value;
    data += 2;
    return le_16(res);
}

bool
ExtraField::parseExtTimeStamp (zip_uint16_t len, const zip_uint8_t *data,
        bool &hasMTime, time_t &mtime, bool &hasATime, time_t &atime,
        bool &hasCreTime, time_t &cretime) {
    if (len < 1) {
        return false;
    }
    const zip_uint8_t *end = data + len;
    unsigned char flags = *data++;
    
    hasMTime = flags & 1;
    hasATime = flags & 2;
    hasCreTime = flags & 4;

    if (hasMTime) {
        if (data + 4 > end) {
            return false;
        }
        mtime = static_cast<time_t>(getLong(data));
    }
    if (hasATime) {
        if (data + 4 > end) {
            return false;
        }
        atime = static_cast<time_t>(getLong(data));
    }
    // only check that data format is correct
    if (hasCreTime) {
        if (data + 4 > end) {
            return false;
        }
        cretime = static_cast<time_t>(getLong(data));
    }

    return true;
}

const zip_uint8_t *
ExtraField::createExtTimeStamp (zip_flags_t location,
        time_t mtime, time_t atime, bool set_cretime, time_t cretime,
        zip_uint16_t &len) {
    assert(location == ZIP_FL_LOCAL || location == ZIP_FL_CENTRAL);

    // one byte for flags and three 4-byte ints for mtime, atime and cretime
    static zip_uint8_t data [1 + 4 * 3];
    len = 0;

    // mtime and atime
    zip_uint8_t flags = 1 | 2;
    if (set_cretime) {
        flags |= 4;
    }
    data[len++] = flags;

    for (int i = 0; i < 4; ++i) {
        data[len++] = static_cast<unsigned char>(mtime);
        mtime >>= 8;
    }
    // The central-header extra field contains the modification time only,
    // or no timestamp at all.
    if (location == ZIP_FL_LOCAL) {
        for (int i = 0; i < 4; ++i) {
            data[len++] = static_cast<unsigned char>(atime);
            atime >>= 8;
        }
        if (set_cretime) {
            for (int i = 0; i < 4; ++i) {
                data[len++] = static_cast<unsigned char>(cretime);
                cretime >>= 8;
            }
        }
    }

    return data;
}

bool
ExtraField::parseSimpleUnixField (zip_uint16_t type, zip_uint16_t len,
        const zip_uint8_t *data,
        bool &hasUidGid, uid_t &uid, gid_t &gid,
        time_t &mtime, time_t &atime) {
    const zip_uint8_t *end = data + len;
    switch (type) {
        case FZ_EF_PKWARE_UNIX:
        case FZ_EF_INFOZIP_UNIX1:
            hasUidGid = false; 
            if (data + 8 > end) {
                return false;
            }
            atime = static_cast<time_t>(getLong(data));
            mtime = static_cast<time_t>(getLong(data));
            if (data + 4 > end)
                return true;
            hasUidGid = true;
            uid = getShort (data);
            gid = getShort (data);
            break;
        default:
            return false;
    }
    return true;
}

bool
ExtraField::parseUnixUidGidField (zip_uint16_t type, zip_uint16_t len,
        const zip_uint8_t *data, uid_t &uid, gid_t &gid) {
    const zip_uint8_t *end = data + len;
    switch (type) {
        case FZ_EF_INFOZIP_UNIX2:
            if (data + 4 > end) {
                return false;
            }
            uid = getShort (data);
            gid = getShort (data);
            break;
        case FZ_EF_INFOZIP_UNIXN: {
            const zip_uint8_t *p;
            // version
            if (len < 1) {
                return false;
            }
            if (*data++ != 1) {
                // unsupported version
                return false;
            }
            // UID
            if (data + 1 > end) {
                return false;
            }
            int lenUid = *data++;
            if (data + lenUid > end) {
                return false;
            }
            p = data + lenUid;
            uid = 0;
            int overflowBytes = lenUid - static_cast<int>(sizeof(uid_t));
            while (--p >= data) {
                if (overflowBytes > 0 && *p != 0) {
                    // UID overflow
                    return false;
                }
                uid = (uid << 8) + *p;
                overflowBytes--;
            }
            data += lenUid;
            // GID
            if (data + 1 > end) {
                return false;
            }
            int lenGid = *data++;
            if (data + lenGid > end) {
                return false;
            }
            p = data + lenGid;
            gid = 0;
            overflowBytes = lenGid - static_cast<int>(sizeof(gid_t));
            while (--p >= data) {
                if (overflowBytes > 0 && *p != 0) {
                    // GID overflow
                    return false;
                }
                gid = (gid << 8) + *p;
                overflowBytes--;
            }

            break;
        }
        default:
            return false;
    }
    return true;
}

#pragma pack(push,1)
struct PkWareUnixExtraField
{
    uint32_t atime;
    uint32_t mtime;
    uint16_t uid;
    uint16_t gid;
    struct {
        uint32_t major;
        uint32_t minor;
    } dev;
};
#pragma pack(pop)

bool
ExtraField::parsePkWareUnixField(zip_uint16_t len, const zip_uint8_t *data, mode_t mode,
        time_t &mtime, time_t &atime, uid_t &uid, gid_t &gid, dev_t &dev,
        const char *&link_target, zip_uint16_t &link_target_len) {
    const PkWareUnixExtraField *f = reinterpret_cast<const PkWareUnixExtraField*>(data);

    if (len < 12) {
        return false;
    }
    atime = static_cast<time_t>(le_32(f->atime));
    mtime = static_cast<time_t>(le_32(f->mtime));
    uid   = le_16(f->uid);
    gid   = le_16(f->gid);

    // variable data field
    dev = 0;
    link_target = NULL;
    link_target_len = 0;
    if (S_ISBLK(mode) || S_ISCHR(mode)) {
        if (len < 20)
            return false;
        
        unsigned int maj, min;
        maj = static_cast<unsigned int>(le_32(f->dev.major));
        min = static_cast<unsigned int>(le_32(f->dev.minor));

        dev = makedev(maj, min);
        link_target = NULL;
        link_target_len = 0;
    } else {
        link_target = reinterpret_cast<const char*>(data + 12);
        link_target_len = static_cast<zip_uint16_t>(len - 12);
    }

    return true;
}

const zip_uint8_t *
ExtraField::createPkWareUnixField (time_t mtime, time_t atime,
        mode_t mode, uid_t uid, gid_t gid, dev_t dev,
        zip_uint16_t &len) {
    static PkWareUnixExtraField data;
    data.mtime = le_32(static_cast<uint32_t>(mtime));
    data.atime = le_32(static_cast<uint32_t>(atime));
    data.uid   = le_16(static_cast<uint16_t>(uid));
    data.gid   = le_16(static_cast<uint16_t>(gid));
    data.dev.major = le_32(major(dev));
    data.dev.minor = le_32(minor(dev));
    if (S_ISBLK(mode) || S_ISCHR(mode))
        len = 20;
    else
        len = 12;
    return reinterpret_cast<zip_uint8_t*>(&data);
}

inline static timespec ntfs2timespec(uint64_t t) {
    timespec ts;
    t -= NTFS_TO_UNIX_OFFSET;
    ts.tv_sec  = static_cast<time_t>(t / 10000000);
    ts.tv_nsec = static_cast<long int>(t % 10000000) * 100;
    return ts;
}

bool
ExtraField::parseNtfsExtraField (zip_uint16_t len, const zip_uint8_t *data,
        struct timespec &mtime, struct timespec &atime, struct timespec &cretime)
{
    bool hasTimes = false;
    const zip_uint8_t *end = data + len;
    data += 4; // skip 'Reserved' field

    while (data + 4 < end) {
        uint16_t tag = getShort(data);
        uint16_t size = getShort(data);
        if (data + size > end)
            return false;

        if (tag == 0x0001) {
            if (size < 24)
                return false;

            mtime   = ntfs2timespec(getLong64(data));
            atime   = ntfs2timespec(getLong64(data));
            cretime = ntfs2timespec(getLong64(data));

            hasTimes = true;
        } else {
            data += size;
        }
    }

    return hasTimes;
}

const zip_uint8_t *
ExtraField::createInfoZipNewUnixField (uid_t uid, gid_t gid,
        zip_uint16_t &len) {
    const int uidLen = sizeof(uid_t), gidLen = sizeof(gid_t);
    static zip_uint8_t data [3 + uidLen + gidLen];

    len = 0;
    // version
    data[len++] = 1;
    // UID
    data[len++] = uidLen;
    for (int i = 0; i < uidLen; ++i) {
        data[len++] = uid & 0xFF;
        uid >>= 8;
    }
    // GID
    data[len++] = gidLen;
    for (int i = 0; i < gidLen; ++i) {
        data[len++] = gid & 0xFF;
        gid >>= 8;
    }

    return data;
}

#pragma pack(push,1)
struct NtfsExtraFieldFull
{
    uint32_t reserved;
    uint16_t tag;
    uint16_t size;
    uint64_t mtime;
    uint64_t atime;
    uint64_t btime;
};

struct NtfsExtraFieldTag
{
    uint16_t tag;
    uint16_t size;

    uint64_t mtime;
    uint64_t atime;
    uint64_t btime;
};
#pragma pack(pop)

inline static uint64_t timespec2ntfs(const timespec &ts) {
    return static_cast<uint64_t>(ts.tv_sec) * 10000000
        + static_cast<uint64_t>(ts.tv_nsec) / 100
        + NTFS_TO_UNIX_OFFSET;
}

const zip_uint8_t *
ExtraField::createNtfsExtraField (const timespec &mtime,
        const timespec &atime, const timespec &btime, zip_uint16_t &len) {
    len = sizeof(NtfsExtraFieldFull);
    static NtfsExtraFieldFull data;

    data.reserved = 0;
    data.tag = le_16(0x0001);
    data.size = le_16(24);

    data.mtime = le_64(timespec2ntfs(mtime));
    data.atime = le_64(timespec2ntfs(atime));
    data.btime = le_64(timespec2ntfs(btime));

    return reinterpret_cast<zip_uint8_t*>(&data);
}

zip_uint16_t
ExtraField::editNtfsExtraField (zip_uint16_t len, zip_uint8_t *data,
        const timespec &mtime, const timespec &atime, const timespec &btime) {
    zip_uint8_t *orig = data;
    zip_uint8_t *dest = data;
    const zip_uint8_t *end = data + len;
    if (data + 4 > end) {
        // incomplete 'reserved' field - re-create
        NtfsExtraFieldFull *out = reinterpret_cast<NtfsExtraFieldFull*>(dest);
        out->reserved = 0;
        out->tag = le_16(0x0001);
        out->size = le_16(24);

        out->mtime = le_64(timespec2ntfs(mtime));
        out->atime = le_64(timespec2ntfs(atime));
        out->btime = le_64(timespec2ntfs(btime));

        return static_cast<zip_uint16_t>(sizeof(*out));
    }
    // skip 'Reserved' field
    data += 4;
    dest += 4;

    while (data + 4 < end) {
        // use only header fields
        NtfsExtraFieldTag *in = reinterpret_cast<NtfsExtraFieldTag*>(data);
        zip_uint16_t tag = le_16(in->tag);
        zip_uint16_t size = le_16(in->size);
        if (data + 4 + size > end)
            break;

        if (tag != 0x0001) {
            // copy tag content
            memmove(dest, data, 4U + size);
            dest += 4 + size;
        }
        data += 4 + size;
    }
    // fill tag 0001
    NtfsExtraFieldTag *out = reinterpret_cast<NtfsExtraFieldTag*>(dest);
    out->tag = le_16(0x0001);
    out->size = le_16(24);

    out->mtime = le_64(timespec2ntfs(mtime));
    out->atime = le_64(timespec2ntfs(atime));
    out->btime = le_64(timespec2ntfs(btime));

    dest += sizeof(*out);

    return static_cast<zip_uint16_t>(dest - orig);
}

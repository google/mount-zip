////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2014-2019 by Alexander Galanin                          //
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

#include <cassert>

unsigned long
ExtraField::getLong (const zip_uint8_t *&data) {
    unsigned long t = *data++;
    t += static_cast<unsigned long>(*data++) << 8;
    t += static_cast<unsigned long>(*data++) << 16;
    t += static_cast<unsigned long>(*data++) << 24;
    return t;
}

unsigned short 
ExtraField::getShort (const zip_uint8_t *&data) {
    unsigned short t = *data++;
    t = static_cast<unsigned short>(t + (*data++ << 8));
    return t;
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
        const zip_uint8_t *data, uid_t &uid, gid_t &gid,
        bool &hasMTime, time_t &mtime, bool &hasATime, time_t &atime) {
    const zip_uint8_t *end = data + len;
    switch (type) {
        case FZ_EF_PKWARE_UNIX:
        case FZ_EF_INFOZIP_UNIX1:
            hasMTime = hasATime = true; 
            if (data + 12 > end) {
                return false;
            }
            atime = static_cast<time_t>(getLong(data));
            mtime = static_cast<time_t>(getLong(data));
            uid = getShort (data);
            gid = getShort (data);
            break;
        case FZ_EF_INFOZIP_UNIX2:
            hasMTime = hasATime = false; 
            if (data + 4 > end) {
                return false;
            }
            uid = getShort (data);
            gid = getShort (data);
            break;
        case FZ_EF_INFOZIP_UNIXN: {
            const zip_uint8_t *p;
            hasMTime = hasATime = false; 
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
            int overflowBytes = static_cast<int>(sizeof(uid_t)) - lenUid;
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
            overflowBytes = static_cast<int>(sizeof(gid_t)) - lenGid;
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


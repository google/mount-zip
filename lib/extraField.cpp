////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2014 by Alexander Galanin                          //
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

#include "extraField.h"

unsigned long
ExtraField::getLong (const zip_uint8_t *&data) {
    unsigned long t = *data++;
    t += *data++ << 8;
    t += *data++ << 16;
    t += *data++ << 24;
    return t;
}

unsigned short 
ExtraField::getShort (const zip_uint8_t *&data) {
    unsigned short t = *data++;
    t += *data++ << 8;
    return t;
}

bool
ExtraField::parseExtTimeStamp (zip_uint16_t len, const zip_uint8_t *data,
        bool &hasMTime, time_t &mtime, bool &hasATime, time_t &atime,
        bool &hasCreTime, time_t &cretime) {
    if (len < 1) {
        return false;
    }
    unsigned char flags = *data++;
    
    hasMTime = flags & 1;
    hasATime = flags & 2;
    hasCreTime = flags & 4;

    const zip_uint8_t *end = data + len;
    if (hasMTime) {
        if (data + 4 > end) {
            return false;
        }
        mtime = (time_t)getLong(data);
    }
    if (hasATime) {
        if (data + 4 > end) {
            return false;
        }
        atime = (time_t)getLong(data);
    }
    // only check that data format is correct
    if (hasCreTime) {
        if (data + 4 > end) {
            return false;
        }
        cretime = (time_t)getLong(data);
    }

    return true;
}

const zip_uint8_t *
ExtraField::createExtTimeStamp (zip_flags_t location,
        time_t mtime, time_t atime, bool set_cretime, time_t cretime,
        zip_uint16_t &len) {
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
        data[len++] = mtime & 0xFF;
        mtime >>= 8;
    }
    // The central-header extra field contains the modification time only,
    // or no timestamp at all.
    if (location == ZIP_FL_LOCAL) {
        for (int i = 0; i < 4; ++i) {
            data[len++] = atime & 0xFF;
            atime >>= 8;
        }
        if (set_cretime) {
            for (int i = 0; i < 4; ++i) {
                data[len++] = cretime & 0xFF;
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
        case 0x000D:
            // PKWARE Unix Extra Field
        case 0x5855:
            // Info-ZIP Unix Extra Field (type 1)
            hasMTime = hasATime = true; 
            if (data + 12 > end) {
                return false;
            }
            atime = getLong (data);
            mtime = getLong (data);
            uid = getShort (data);
            gid = getShort (data);
            break;
        case 0x7855:
            // Info-ZIP Unix Extra Field (type 2)
            hasMTime = hasATime = false; 
            if (data + 4 > end) {
                return false;
            }
            uid = getShort (data);
            gid = getShort (data);
            break;
        case 0x7875: {
            // Info-ZIP New Unix Extra Field
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
            while (--p >= data) {
                uid = (uid << 8) + *p;
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
            while (--p >= data) {
                gid = (gid << 8) + *p;
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


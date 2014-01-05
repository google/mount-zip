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
        signed long t = *data++;
        t += *data++ << 8;
        t += *data++ << 16;
        t += *data++ << 24;
        mtime = (time_t)t;
    }
    if (hasATime) {
        if (data + 4 > end) {
            return false;
        }
        signed long t = *data++;
        t += *data++ << 8;
        t += *data++ << 16;
        t += *data++ << 24;
        atime = (time_t)t;
    }
    // only check that data format is correct
    if (hasCreTime) {
        if (data + 4 > end) {
            return false;
        }
        signed long t = *data++;
        t += *data++ << 8;
        t += *data++ << 16;
        t += *data++ << 24;
        cretime = (time_t)t;
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


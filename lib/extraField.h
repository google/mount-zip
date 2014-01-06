////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2014 by Alexander Galanin                                //
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

#ifndef EXTRA_FIELD_H
#define EXTRA_FIELD_H

#include <zip.h>

struct ExtraField {

/**
 * Parse 'Extended Timestamp' LOCAL extra field (0x5455) to get mtime,
 * atime and creation time values.
 * @param len (IN) field length in bytes
 * @param data (IN) field data
 * @param hasMTime (OUT) mtime presence
 * @param mtime (OUT) file modification time if present
 * @param hasATime (OUT) atime presence
 * @param atime (OUT) file access time if present
 * @param hasCreTime (OUT) creation time presence
 * @param cretime (OUT) file creation time if present
 * @return successful completion flag
 */
static bool parseExtTimeStamp (zip_uint16_t len, const zip_uint8_t *data,
        bool &hasMTime, time_t &mtime, bool &hasATime, time_t &atime,
        bool &hasCreTime, time_t &cretime);

/**
 * Create 'Extended Timestamp' extra field (0x5455) from mtime and atime.
 * Creation time field is filled only if defined.
 * @param location location of timestamp field (ZIP_FL_CENTRAL or
 * ZIP_FL_LOCAL for central directory and local extra field respectively)
 * @param mtime modification time
 * @param atime access time
 * @param set_cretime true if creation time is defined
 * @param cretime creation time
 * @param len (OUT) data length
 * @return pointer to timestamp data (must not be free()-d)
 */
static const zip_uint8_t *createExtTimeStamp (zip_flags_t location,
        time_t mtime, time_t atime, bool set_cretime, time_t cretime,
        zip_uint16_t &len);

/**
 * Parse simple UNIX LOCAL extra field to extract UID/GID and (maybe)
 * timestamps:
 *  000D    PKWARE Unix Extra Field
 *  5855    Info-ZIP Unix Extra Field (type 1)
 *  7855    Info-ZIP Unix Extra Field (type 2)
 *  7875    Info-ZIP New Unix Extra Field
 * Variable part of 000D are currently ignored
 *
 * @param type extended field type ID
 * @param len field length in bytes
 * @param data field data
 * @param uid (OUT) UID
 * @param gid (OUT) GID
 * @param hasMTime (OUT) mtime presence
 * @param mtime (OUT) file modification time if present
 * @param hasATime (OUT) atime presence
 * @param atime (OUT) file access time if present
 * @return successful completion flag
 */
static bool parseSimpleUnixField (zip_uint16_t type, zip_uint16_t len,
        const zip_uint8_t *data, uid_t &uid, gid_t &gid,
        bool &hasMTime, time_t &mtime, bool &hasATime, time_t &atime);

/**
 * Create Info-ZIP New Unix extra field (0x7875)
 * @param uid UID
 * @param gid GID
 * @param len (OUT) data length
 * @return pointer to timestamp data (must not be free()-d)
 */
static const zip_uint8_t *createInfoZipNewUnixField (uid_t uid, gid_t gid,
        zip_uint16_t &len);

private:
/**
 * Get Intel low-byte/high-byte order 32-bit number from data.
 * Pointer is moved to next byte after parsed data.
 */
static unsigned long getLong (const zip_uint8_t *&data);

/**
 * Get Intel low-byte/high-byte order 16-bit number from data.
 * Pointer is moved to next byte after parsed data.
 */
static unsigned short getShort (const zip_uint8_t *&data);

};

#endif


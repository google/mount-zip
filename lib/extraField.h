////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 204 by Alexander Galanin                                //
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
 * Parse 'Extended Timestamp' extra field (0x5455) to get mtime and
 * atime field values.
 * @param len (IN) field length in bytes
 * @param data (IN) field data
 * @param hasMTime (OUT) mtime presence
 * @param mtime (OUT) file modification time if present
 * @param hasATime (OUT) atime presence
 * @param atime (OUT) file access time if present
 * @return successful completion flag
 */
static bool parseExtTimeStamp (zip_uint16_t len, const zip_uint8_t *data,
        bool &hasMTime, time_t &mtime, bool &hasATime, time_t &atime);

/**
 * Create 'Extended Timestamp' extra field (0x5455) from mtime and atime.
 * Creation time field left empty.
 * @param location location of timestamp field (ZIP_FL_CENTRAL or
 * ZIP_FL_LOCAL for central directory and local extra field respectively)
 * @param mtime modification time
 * @param atime access time
 * @param len (OUT) data length
 * @return pointer to timestamp data (must not be free()-d)
 */
static const zip_uint8_t *createExtTimeStamp (zip_flags_t location,
        time_t mtime, time_t atime, zip_uint16_t &len);

};
#endif


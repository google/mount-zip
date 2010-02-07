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

#ifndef LIBZIPWRAPPER_H
#define LIBZIPWRAPPER_H

//
// This is compatibility layer to support libzip compiled without large file
// support on 32-bit system.
//
// If on your system libzip compiled with -D_FILE_OFFSET_BITS=64 then you must
// add -D_FILE_OFFSET_BITS=64 to CXXFLAGS when running make.
//
// make CXXFLAGS="$CXXFLAGS -D_FILE_OFFSET_BITS=64" clean all
//

#include <unistd.h>
#include <sys/types.h>
#include <ctime>

#include "types.h"

struct zip_stat_64 {
    const char *name;                   /* name of the file */
    int index;                          /* index within archive */
    unsigned int crc;                   /* crc of file data */
    time_t mtime;                       /* modification time */
//modified type
    offset_t size;                       /* size of file (uncompressed) */
//modified type
    offset_t comp_size;                  /* size of file (compressed) */
    unsigned short comp_method;         /* compression method used */
    unsigned short encryption_method;   /* encryption method used */
};

void zip_stat_assign_64_to_default(struct zip_stat_64 *dest, const struct zip_stat *src);

#endif


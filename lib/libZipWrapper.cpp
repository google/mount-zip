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

#include <zip.h>

#include "libZipWrapper.h"

void zip_stat_assign_64_to_default(struct zip_stat_64 *dest, const struct zip_stat *src) {
    dest->name = src->name;
    dest->index = src->index;
    dest->crc = src->crc;
    dest->mtime = src->mtime;
    dest->comp_method = src->comp_method;
    dest->encryption_method = src->encryption_method;

    dest->size = offset_t(src->size);
    dest->comp_size = offset_t(src->comp_size);
}


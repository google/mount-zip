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

#ifndef FUSEZIP_TYPES_H
#define FUSEZIP_TYPES_H

#include <unistd.h>

#include <cstring>
#include <cstdlib>
#include <list>
#include <map>

class FileNode;
class FuseZipData;

struct ltstr {
    bool operator() (const char* s1, const char* s2) const {
        return strcmp(s1, s2) < 0;
    }
};

typedef std::list <FileNode*> nodelist_t;
typedef std::map <const char*, FileNode*, ltstr> filemap_t;

#endif


////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2017 by Alexander Galanin                          //
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
    // This function compares two strings until last non-slash and non-zero
    // character.
    // This is a workaround for FUSE subdir module that appends '/' to the end
    // of new root path.
    bool operator() (const char* s1, const char* s2) const {
        const char *e1, *e2;
        char cmp1, cmp2;
        // set e1 and e2 to the last character in the string that is not a slash
        for (e1 = s1 + strlen(s1) - 1; e1 > s1 && *e1 == '/'; --e1);
        for (e2 = s2 + strlen(s2) - 1; e2 > s2 && *e2 == '/'; --e2);
        // compare strings until e1 and e2
        for (;s1 <= e1 && s2 <= e2 && *s1 == *s2; s1++, s2++);
        cmp1 = (s1 <= e1) ? *s1 : '\0';
        cmp2 = (s2 <= e2) ? *s2 : '\0';
        return cmp1 < cmp2;
    }
};

typedef std::list <FileNode*> nodelist_t;
typedef std::map <const char*, FileNode*, ltstr> filemap_t;

#endif


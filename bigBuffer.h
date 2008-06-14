////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008 by Alexander Galanin                               //
//  gaa.nnov@mail.ru                                                      //
//                                                                        //
//  This program is free software; you can redistribute it and/or modify  //
//  it under the terms of the GNU Library General Public License as       //
//  published by the Free Software Foundation; either version 3 of the    //
//  License, or (at your option) any later version.                       //
//                                                                        //
//  This program is distributed in the hope that it will be useful,       //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//  GNU General Public License for more details.                          //
//                                                                        //
//  You should have received a copy of the GNU Library General Public     //
//  License along with this program; if not, write to the                 //
//  Free Software Foundation, Inc.,                                       //
//  51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA               //
////////////////////////////////////////////////////////////////////////////

#ifndef BIG_BUFFER_H
#define BIG_BUFFER_H

#include <zip.h>
#include <unistd.h>

#include <vector>

class BigBuffer {
private:
    typedef std::vector<char*> chunks_t;

    static const int chunkSize = 4*1024; //4 Kilobytes

    chunks_t chunks;

    static ssize_t zipUserFunctionCallback(void *state, void *data, size_t len, enum zip_source_cmd cmd);
public:
    off64_t len;

    BigBuffer();
    BigBuffer(struct zip *z, int nodeId, ssize_t length);
    ~BigBuffer();

    int read(char *buf, size_t size, off64_t offset) const;
    int write(const char *buf, size_t size, off64_t offset);
    int saveToZip(struct zip *z, const char *fname, bool newFile, int index);
    int truncate(off64_t offset);
};

#endif


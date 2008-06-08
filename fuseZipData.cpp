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

#define ROOT_NODE_INDEX (-1)

#include <zip.h>

#include "fuseZipData.h"

FuseZipData::FuseZipData(struct zip *z) {
    m_zip = z;
    build_tree();
}

FuseZipData::~FuseZipData() {
    zip_close(m_zip);
    //TODO: handle error code of zip_close

    for (filemap_t::iterator i = files.begin(); i != files.end(); ++i) {
        delete i->second;
    }
}

void FuseZipData::build_tree() {
    FileNode *root_node = new FileNode(this, "", ROOT_NODE_INDEX);
    root_node->is_dir = true;

    int n = zip_get_num_files(m_zip);
    for (int i = 0; i < n; ++i) {
        FileNode *node = new FileNode(this, zip_get_name(m_zip, i, 0), i);
        (void) node;
    }
}


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

#ifndef FUSEZIP_DATA
#define FUSEZIP_DATA

#include <string>

#include "types.h"
#include "fileNode.h"

class FuseZipData {
public:
    filemap_t files;
    struct zip *m_zip;
    const char *m_archiveName;
    std::string m_cwd;

    /**
     * Keep archiveName and cwd in class fields and build file tree from z.
     *
     * 'cwd' and 'z' free()-ed in destructor.
     * 'archiveName' should be managed externally.
     */
    FuseZipData(const char *archiveName, struct zip *z, const char *cwd);
    ~FuseZipData();

    /**
     * Detach node from tree, and delete associated entry in zip file if
     * present.
     *
     * @param node Node to remove
     * @return Error code or 0 is successful
     */
    int removeNode(FileNode *node) const;

    /**
     * Build tree of zip file entries from ZIP file
     */
    void build_tree();
};

#endif


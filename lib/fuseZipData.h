////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2014 by Alexander Galanin                          //
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
private:
    /**
     * Check that file name is non-empty and does not contain duplicate
     * slashes
     * @throws std::runtime_exception if name is invalid
     */
    static void validateFileName(const char *fname);

    /**
     * In read-only mode convert file names to replace .. to UP and / to
     * ROOT to make absolute and relative paths accessible via file system.
     * In read-write mode throw an error if path is absolute or relative to
     * parent directory.
     * @param fname file name
     * @param readonly read-only flag
     * @param needPrefix prepend CUR directory prefix to "normal" file
     * names to not mix them with parent-relative or absolute
     * @param converted result string
     * @throws std::runtime_exception if name is invalid
     */
    static void convertFileName(const char *fname, bool readonly,
            bool needPrefix, std::string &converted);

    FileNode *m_root;
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
    void build_tree(bool readonly);

    /**
     * Return pointer to root filesystem node
     */
    inline FileNode *rootNode () const {
        return m_root;
    }
};

#endif


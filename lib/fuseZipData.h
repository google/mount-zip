////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2019 by Alexander Galanin                          //
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

    /**
     * Find node parent by its name
     */
    FileNode *findParent (const FileNode *node) const;

    /**
     * Create node parents (if not yet exist) and connect node to
     * @throws std::bad_alloc
     * @throws std::runtime_error - if parent is not directory
     */
    void connectNodeToTree (FileNode *node);

    /**
     * Get ZIP file entry UNIX mode and PkWare hardlink flag from external attributes field.
     */
    mode_t getEntryAttributes(zip_uint64_t id, const char *name, bool &isHardlink);

    /**
     * create and attach file node
     */
    void attachNode(zip_int64_t id, const char *name, mode_t mode, bool readonly,
            bool needPrefix, filemap_t &origNames);

    /**
     * create and attach hardlink node
     */
    bool attachHardlink(zip_int64_t id, const char *name, mode_t mode, bool readonly,
            bool needPrefix, filemap_t &origNames);

    FileNode *m_root;
    filemap_t files;
public:
    struct zip *m_zip;
    const char *m_archiveName;
    std::string m_cwd;
    const bool m_force_precise_time;

    /**
     * Keep archiveName and cwd in class fields and build file tree from z.
     *
     * 'cwd' and 'z' free()-ed in destructor.
     * 'archiveName' should be managed externally.
     */
    FuseZipData(const char *archiveName, struct zip *z, const char *cwd, bool force_precise_time);
    ~FuseZipData();

    /**
     * Detach node from tree, and delete associated entry in zip file if
     * present.
     *
     * @param node Node to remove
     * @return Error code or 0 is successful
     */
    int removeNode(FileNode *node);

    /**
     * Build tree of zip file entries from ZIP file
     */
    void build_tree(bool readonly);

    /**
     * Insert new node into tree by adding it to parent's childs list and
     * specifying node parent field.
     */
    void insertNode (FileNode *node);

    /**
     * Detach node from old parent, rename, attach to new parent.
     * @param node
     * @param newName new name
     * @param reparent if false, node detaching is not performed
     */
    void renameNode (FileNode *node, const char *newName, bool reparent);

    /**
     * search for node
     * @return node or NULL
     */
    FileNode *find (const char *fname) const;

    /**
     * Return number of files in tree
     */
    size_t numFiles () const {
        return files.size() - 1;
    }

    /**
     * Save archive
     */
    void save ();
};

#endif


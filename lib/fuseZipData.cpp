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

#include <zip.h>
#include <syslog.h>
#include <cerrno>

#include "fuseZipData.h"

FuseZipData::FuseZipData(const char *archiveName, struct zip *z, const char *cwd): m_zip(z), m_archiveName(archiveName), m_cwd(cwd)  {
}

FuseZipData::~FuseZipData() {
    if (chdir(m_cwd.c_str()) != 0) {
        syslog(LOG_ERR, "Unable to chdir() to archive directory %s. Trying to save file into /tmp",
                m_cwd.c_str());
        if (chdir(getenv("TMP")) != 0) {
            chdir("/tmp");
        }
    }
    int res = zip_close(m_zip);
    if (res != 0) {
        syslog(LOG_ERR, "Error while closing archive: %s", zip_strerror(m_zip));
    }
    for (filemap_t::iterator i = files.begin(); i != files.end(); ++i) {
        delete i->second;
    }
}

void FuseZipData::build_tree() {
    FileNode *root_node = new FileNode(this, "", FileNode::ROOT_NODE_INDEX);
    root_node->is_dir = true;

    zip_int64_t n = zip_get_num_entries(m_zip, 0);
    for (zip_int64_t i = 0; i < n; ++i) {
        const char *name = zip_get_name(m_zip, i, ZIP_FL_ENC_RAW);
        try {
            FileNode *node = new FileNode(this, name, i);
            (void) node;
        }
        catch (const FileNode::AlreadyExists &e) {
            // Only need to skip node creation
        }
    }
}

int FuseZipData::removeNode(FileNode *node) const {
    node->detach();
    zip_int64_t id = node->id;
    delete node;
    if (id >= 0) {
        return (zip_delete (m_zip, id) == 0)? 0 : ENOENT;
    } else {
        return 0;
    }
}


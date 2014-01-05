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

#include <zip.h>
#include <syslog.h>
#include <cerrno>
#include <cassert>
#include <stdexcept>

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

void FuseZipData::build_tree(bool readonly) {
    m_root = FileNode::createRootNode(this);
    zip_int64_t n = zip_get_num_entries(m_zip, 0);
    // search for absolute or parent-relative paths
    bool needPrefix = false;
    if (readonly) {
        for (zip_int64_t i = 0; i < n; ++i) {
            const char *name = zip_get_name(m_zip, i, ZIP_FL_ENC_RAW);
            if ((name[0] == '/') || (strncmp(name, "../", 3) == 0)) {
                needPrefix = true;
            }
        }
    }
    // add zip entries into tree
    for (zip_int64_t i = 0; i < n; ++i) {
        const char *name = zip_get_name(m_zip, i, ZIP_FL_ENC_RAW);
        std::string converted;
        convertFileName(name, readonly, needPrefix, converted);
        try {
            FileNode *node = FileNode::createNodeForZipEntry(this,
                    converted.c_str(), i);
            (void) node;
        }
        catch (const FileNode::AlreadyExists &e) {
            // Only need to skip node creation
        }
    }
}

int FuseZipData::removeNode(FileNode *node) const {
    node->detach();
    if (node->parent != NULL) {
        node->parent->setCTime (time(NULL));
    }
    zip_int64_t id = node->id;
    delete node;
    if (id >= 0) {
        return (zip_delete (m_zip, id) == 0)? 0 : ENOENT;
    } else {
        return 0;
    }
}

void FuseZipData::validateFileName(const char *fname) {
    if (fname[0] == 0) {
        throw std::runtime_error("empty file name");
    }
    if (strstr(fname, "//") != NULL) {
        throw std::runtime_error(std::string("bad file name (two slashes): ") + fname);
    }
}

void FuseZipData::convertFileName(const char *fname, bool readonly,
        bool needPrefix, std::string &converted) {
    const char *UP_PREFIX = "UP";
    const char *CUR_PREFIX = "CUR";
    const char *ROOT_PREFIX = "ROOT";

    validateFileName(fname);

    assert(fname[0] != 0);
    const char *orig = fname;
    bool parentRelative = false;
    converted.reserve(strlen(fname) + strlen(ROOT_PREFIX) + 1);
    converted = "";
    // add prefix
    if (fname[0] == '/') {
        if (!readonly) {
            throw std::runtime_error("absolute paths are not supported in read-write mode");
        } else {
            assert(needPrefix);
            converted.append(ROOT_PREFIX);
            ++fname;
        }
    } else {
        while (strncmp(fname, "../", 3) == 0) {
            if (!readonly) {
                throw std::runtime_error("paths relative to parent directory are not supported in read-write mode");
            }
            assert(needPrefix);
            converted.append(UP_PREFIX);
            fname += strlen("../");
            parentRelative = true;
        }
        if (needPrefix && !parentRelative) {
            converted.append(CUR_PREFIX);
        }
    }
    if (needPrefix) {
        converted.append("/");
    }
    if (fname[0] == 0) {
        return;
    }
    assert(fname[0] != '/');

    const char *start = fname, *cur;
    while (start[0] != 0 && (cur = strchr(start + 1, '/')) != NULL) {
        if ((cur - start == 1 && start[0] == '.') ||
            (cur - start == 2 && start[0] == '.' && start[1] == '.')) {
            throw std::runtime_error(std::string("bad file name: ") + orig);
        }
        converted.append(start, cur - start + 1);
        start = cur + 1;
    }
    // end of string is reached
    if (strcmp(start, ".") == 0 || strcmp(start, "..") == 0) {
        throw std::runtime_error(std::string("bad file name: ") + orig);
    }
    converted.append(start);
}


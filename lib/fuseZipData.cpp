////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2020 by Alexander Galanin                          //
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

#include <zip.h>
#include <syslog.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <stdexcept>

#include "fuseZipData.h"
#include "extraField.h"
#include "util.h"

#define FZ_ATTR_HARDLINK (0x800)

FuseZipData::FuseZipData(const char *archiveName, struct zip *z, const char *cwd,
        bool force_precise_time):
    m_zip(z), m_archiveName(archiveName), m_cwd(cwd), m_force_precise_time(force_precise_time) {
}

FuseZipData::~FuseZipData() {
    if (chdir(m_cwd.c_str()) != 0) {
        syslog(LOG_ERR, "Unable to chdir() to archive directory %s: %s. Trying to save file into $TMP or /tmp...",
                m_cwd.c_str(), strerror(errno));
        const char *tmpDir = getenv("TMP");
        if (tmpDir == NULL || chdir(tmpDir) != 0) {
            if (tmpDir != NULL) {
                syslog(LOG_WARNING, "Unable to chdir() to %s: %s.", tmpDir, strerror(errno));
            }
            if (chdir("/tmp") != 0) {
                syslog(LOG_ERR, "Unable to chdir() to /tmp: %s!", strerror(errno));
            }
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
    m_root = FileNode::createRootNode(m_zip);
    if (m_root == NULL) {
        throw std::bad_alloc();
    }
    m_root->parent = NULL;
    files[m_root->full_name.c_str()] = m_root;
    zip_int64_t n = zip_get_num_entries(m_zip, 0);
    // search for absolute or parent-relative paths
    bool needPrefix = false;
    if (readonly) {
        for (zip_int64_t i = 0; i < n; ++i) {
            const char *name = zip_get_name(m_zip, static_cast<zip_uint64_t>(i), ZIP_FL_ENC_RAW);
            if ((name[0] == '/') || (strncmp(name, "../", 3) == 0)) {
                needPrefix = true;
            }
        }
    }
    // add zip entries for all items except hardlinks
    filemap_t origNames;
    for (zip_int64_t i = 0; i < n; ++i) {
        zip_uint64_t id = static_cast<zip_uint64_t>(i);
        bool isHardlink;
        const char *name = zip_get_name(m_zip, id, ZIP_FL_ENC_RAW);
        mode_t mode = getEntryAttributes(id, name, isHardlink);
        
        if (isHardlink)
            continue;

        attachNode(i, name, mode, readonly, needPrefix, origNames);
    }
    // add hardlinks
    for (zip_int64_t i = 0; i < n; ++i) {
        zip_uint64_t id = static_cast<zip_uint64_t>(i);
        bool isHardlink;
        const char *name = zip_get_name(m_zip, id, ZIP_FL_ENC_RAW);
        mode_t mode = getEntryAttributes(id, name, isHardlink);

        if (!isHardlink)
            continue;

        bool notHLink = !attachHardlink(i, name, mode, readonly, needPrefix, origNames);
        if (notHLink)
            attachNode(i, name, mode, readonly, needPrefix, origNames);
        else if (!readonly)
            throw std::runtime_error("hard links are supported only in read-only mode");
    }
    // Connect nodes to tree. Missing intermediate nodes created on demand.
    for (filemap_t::const_iterator i = files.begin(); i != files.end(); ++i)
    {
        FileNode *node = i->second;
        if (node != m_root) {
            connectNodeToTree (node);
        }
    }
}

void FuseZipData::connectNodeToTree (FileNode *node) {
    FileNode *parent = findParent(node);
    if (parent == NULL) {
        parent = FileNode::createIntermediateDir (m_zip,
                node->getParentName().c_str());
        if (parent == NULL) {
            throw std::bad_alloc();
        }
        files[parent->full_name.c_str()] = parent;
        connectNodeToTree (parent);
    } else if (!parent->is_dir()) {
        throw std::runtime_error ("bad archive structure");
    }
    // connecting to parent
    node->parent = parent;
    parent->appendChild (node);
}

mode_t FuseZipData::getEntryAttributes(zip_uint64_t id, const char *name, bool &isHardlink) {
    bool is_dir = false;
    size_t len = strlen(name);
    if (len > 0)
        is_dir = name[len - 1] == '/';

    zip_uint8_t opsys;
    zip_uint32_t attr;
    zip_file_get_external_attributes(m_zip, id, 0, &opsys, &attr);

    mode_t unix_mode = attr >> 16;
    mode_t mode;
    isHardlink = false;
    /*
     * PKWARE describes "OS made by" now (since 1998) as follows:
     * The upper byte indicates the compatibility of the file attribute
     * information. If the external file attributes are compatible with MS-DOS
     * and can be read by PKZIP for DOS version 2.04g then this value will be
     * zero.
     */
    if (opsys == ZIP_OPSYS_DOS && (unix_mode & S_IFMT) != 0)
        opsys = ZIP_OPSYS_UNIX;
    switch (opsys) {
        case ZIP_OPSYS_UNIX: {
            mode = unix_mode;
            // force is_dir value
            if (is_dir) {
                mode = (mode & static_cast<unsigned>(~S_IFMT)) | S_IFDIR;
            } else {
                if ((mode & S_IFMT) == S_IFDIR) {
                    mode = (mode & static_cast<unsigned>(~S_IFMT)) | S_IFREG;
                }
                if ((mode & S_IFMT) == 0) {
                    // treat unknown file types as regular
                    mode = (mode & static_cast<unsigned>(~S_IFMT)) | S_IFREG;
                }
            }
            isHardlink = (attr & FZ_ATTR_HARDLINK) != 0;
            if (isHardlink) {
                switch (mode & S_IFMT) {
                    case S_IFLNK:
                        // PkZip saves hardlink to symlink as a symlink to an original symlink target
                        isHardlink = true;
                        break;
                    case S_IFREG:
                        // normal hardlink
                        isHardlink = true;
                        break;
                    case S_IFSOCK:
                    case S_IFIFO:
                        // create hardlink if destination file exists
                        isHardlink = true;
                        break;
                    case S_IFBLK:
                    case S_IFCHR:
                    case S_IFDIR:
                        // always ignore hardlink flag for devices and dirs
                        isHardlink = false;
                        break;
                    default:
                        // don't hardlink unknown file types
                        isHardlink = false;
                }
            }
            break;
        }
        case ZIP_OPSYS_DOS:
        case ZIP_OPSYS_WINDOWS_NTFS:
        case ZIP_OPSYS_MVS: {
            /*
             * Both WINDOWS_NTFS and OPSYS_MVS used here because of
             * difference in constant assignment by PKWARE and Info-ZIP
             */
            mode = 0444;
            // http://msdn.microsoft.com/en-us/library/windows/desktop/gg258117%28v=vs.85%29.aspx
            // http://en.wikipedia.org/wiki/File_Allocation_Table#attributes
            // FILE_ATTRIBUTE_READONLY
            if ((attr & 1) == 0) {
                mode |= 0220;
            }
            // directory
            if (is_dir) {
                mode |= S_IFDIR | 0111;
            } else {
                mode |= S_IFREG;
            }

            break;
        }
        default: {
            if (is_dir) {
                mode = S_IFDIR | 0775;
            } else {
                mode = S_IFREG | 0664;
            }
        }
    }

    return mode;
}

void FuseZipData::attachNode(zip_int64_t id, const char *name, mode_t mode, bool readonly,
            bool needPrefix, filemap_t &origNames)
{
    std::string converted;
    convertFileName(name, readonly, needPrefix, converted);
    const char *cname = converted.c_str();
    if (files.find(cname) != files.end()) {
        syslog(LOG_ERR, "duplicated file name: %s", cname);
        throw std::runtime_error("duplicate file names");
    }
    FileNode *node = FileNode::createNodeForZipEntry(m_zip, cname, id, mode);
    if (node == NULL) {
        throw std::bad_alloc();
    }
    files[node->full_name.c_str()] = node;
    origNames[name] = node;
}

bool FuseZipData::attachHardlink(zip_int64_t sid, const char *name, mode_t mode, bool readonly,
            bool needPrefix, filemap_t &origNames)
{
    const zip_uint8_t *field;
    zip_uint16_t len;
    zip_uint64_t id = static_cast<zip_uint64_t>(sid);
    field = zip_file_extra_field_get_by_id(m_zip, id, FZ_EF_PKWARE_UNIX, 0, &len, ZIP_FL_CENTRAL);
    if (!field)
        field = zip_file_extra_field_get_by_id(m_zip, id, FZ_EF_PKWARE_UNIX, 0, &len, ZIP_FL_LOCAL);
    if (!field) {
        // ignoring hardlink without PKWARE UNIX field
        syslog(LOG_INFO, "%s: PKWARE UNIX field is absent for hardlink\n", name);
        return false;
    }

    time_t mt, at;
    uid_t uid;
    gid_t gid;
    dev_t dev;
    const char *link;
    uint16_t link_len;
    if (!ExtraField::parsePkWareUnixField(len, field, mode, mt, at,
                uid, gid, dev, link, link_len))
    {
        syslog(LOG_WARNING, "%s: unable to parse PKWARE UNIX field\n", name);
        return false;
    }

    if (link_len == 0 || !link)
    {
        syslog(LOG_ERR, "%s: hard link target is empty\n", name);
        return true;
    }

    std::string linkStr(link, link_len);

    auto it = origNames.find(linkStr.c_str());
    if (it == origNames.end())
    {
        syslog(LOG_ERR, "%s: unable to find link target %s\n", name, linkStr.c_str());
        return true;
    }

    if ((it->second->mode() & S_IFMT) != (mode & S_IFMT))
    {
        // PkZip saves hard-link flag for symlinks with inode link count > 1.
        if (S_ISLNK(mode))
        {
            return false;
        }
        else
        {
            syslog(LOG_ERR, "%s: file format differs with link target %s\n", name, linkStr.c_str());
            return true;
        }
    }

    std::string converted;
    convertFileName(name, readonly, needPrefix, converted);
    const char *cname = converted.c_str();
    if (files.find(cname) != files.end()) {
        syslog(LOG_ERR, "duplicated file name: %s", cname);
        throw std::runtime_error("duplicate file names");
    }
    FileNode *node = FileNode::createHardlink(m_zip, cname, sid, it->second);
    if (node == NULL) {
        throw std::bad_alloc();
    }
    files[node->full_name.c_str()] = node;
    origNames[name] = node;

    return true;
}

int FuseZipData::removeNode(FileNode *node) {
    assert(node != NULL);
    assert(node->parent != NULL);
    node->parent->detachChild (node);
    node->parent->setCTime (currentTime());
    files.erase(node->full_name.c_str());

    bool present = node->present_in_zip();
    zip_uint64_t id = node->id();
    delete node;
    if (present) {
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
    static const char *UP_PREFIX = "UP";
    static const char *CUR_PREFIX = "CUR";
    static const char *ROOT_PREFIX = "ROOT";

    validateFileName(fname);

    assert(fname[0] != 0);
    const char *orig = fname;
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
        bool parentRelative = false;
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
        converted.append(start, static_cast<size_t>(cur - start + 1));
        start = cur + 1;
    }
    // end of string is reached
    if (strcmp(start, ".") == 0 || strcmp(start, "..") == 0) {
        throw std::runtime_error(std::string("bad file name: ") + orig);
    }
    converted.append(start);
}

FileNode *FuseZipData::findParent (const FileNode *node) const {
    std::string name = node->getParentName();
    return find(name.c_str());
}

void FuseZipData::insertNode (FileNode *node) {
    FileNode *parent = findParent (node);
    assert (parent != NULL);
    parent->appendChild (node);
    node->parent = parent;
    parent->setCTime (node->ctime());
    assert (files.find(node->full_name.c_str()) == files.end());
    files[node->full_name.c_str()] = node;
}

void FuseZipData::renameNode (FileNode *node, const char *newName, bool
        reparent) {
    assert(node != NULL);
    assert(newName != NULL);
    FileNode *parent1 = node->parent, *parent2;
    assert (parent1 != NULL);
    if (reparent) {
        parent1->detachChild (node);
    }

    files.erase(node->full_name.c_str());
    node->rename(newName);
    files[node->full_name.c_str()] = node;

    if (reparent) {
        parent2 = findParent(node);
        assert (parent2 != NULL);
        parent2->appendChild (node);
        node->parent = parent2;
    }

    if (reparent && parent1 != parent2) {
        struct timespec now = currentTime();
        parent1->setCTime (now);
        parent2->setCTime (now);
    }
}

FileNode *FuseZipData::find (const char *fname) const {
    filemap_t::const_iterator i = files.find(fname);
    if (i == files.end()) {
        return NULL;
    } else {
        return i->second;
    }
}

void FuseZipData::save () {
    for (filemap_t::const_iterator i = files.begin(); i != files.end(); ++i) {
        FileNode *node = i->second;
        if (node == m_root) {
            if (node->isCommentChanged()) {
                int res = node->saveComment();
                if (res != 0) {
                    syslog(LOG_ERR, "Error while saving archive comment: %d", res);
                }
            }
            continue;
        }
        assert(node != NULL);
        bool saveMetadata = node->isMetadataChanged();
        if (node->isChanged() && !node->is_dir()) {
            saveMetadata = true;
            int res = node->save();
            if (res != 0) {
                saveMetadata = false;
                syslog(LOG_ERR, "Error while saving file %s in ZIP archive: %d",
                        node->full_name.c_str(), res);
            }
        }
        if (saveMetadata) {
            if (node->isTemporaryDir()) {
                // persist temporary directory
                zip_int64_t idx = zip_dir_add(m_zip,
                        node->full_name.c_str(), ZIP_FL_ENC_GUESS);
                if (idx < 0) {
                    syslog(LOG_ERR, "Unable to save directory %s in ZIP archive",
                        node->full_name.c_str());
                    continue;
                }
                node->set_id(idx);
            }
            int res = node->saveMetadata(m_force_precise_time);
            if (res != 0) {
                syslog(LOG_ERR, "Error while saving metadata for file %s in ZIP archive: %d",
                        node->full_name.c_str(), res);
            }
        }
        if (node->isCommentChanged()) {
            int res = node->saveComment();
            if (res != 0) {
                syslog(LOG_ERR, "Error while saving comment for file %s in ZIP archive: %d",
                        node->full_name.c_str(), res);
            }
        }
    }
}


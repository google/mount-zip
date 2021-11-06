////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2014-2021 by Alexander Galanin                          //
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

#include "../config.h"

#include <zip.h>
#include <assert.h>
#include <stdlib.h>
#include <cstring>
#include <cerrno>
#include <memory>

// Public Morozoff design pattern :)
#define private public
#define protected public

#include "fileNode.h"
#include "common.h"

using namespace std;

// libzip stubs

struct zip {
};
struct zip_file {
};
struct zip_source {
};

int zip_stat_index(struct zip *, zip_uint64_t, zip_flags_t, struct zip_stat *) {
    assert(false);
    return 0;
}

struct zip_file *zip_fopen_index(struct zip *, zip_uint64_t, zip_flags_t) {
    assert(false);
    return NULL;
}

zip_int64_t zip_fread(struct zip_file *, void *, zip_uint64_t) {
    assert(false);
    return 0;
}

int zip_fclose(struct zip_file *) {
    assert(false);
    return 0;
}

zip_int64_t zip_file_add(struct zip *, const char *, struct zip_source *, zip_flags_t) {
    assert(false);
    return 0;
}

int zip_file_replace(struct zip *, zip_uint64_t, struct zip_source *, zip_flags_t) {
    assert(false);
    return 0;
}

struct zip_source *zip_source_function(struct zip *, zip_source_callback, void *) {
    assert(false);
    return NULL;
}

void zip_source_free(struct zip_source *) {
    assert(false);
}

const char *zip_get_name(struct zip *, zip_uint64_t, zip_flags_t) {
    assert(false);
    return NULL;
}

const char *zip_file_strerror(struct zip_file *) {
    assert(false);
    return NULL;
}

const char *zip_strerror(struct zip *) {
    assert(false);
    return NULL;
}

const char *zip_get_archive_comment(zip_t *, int *, zip_flags_t) {
    return NULL;
}

int zip_set_archive_comment(zip_t *, const char *, zip_uint16_t) {
    assert(false);
    return 0;
}

const char *zip_file_get_comment(zip_t *, zip_uint64_t, zip_uint32_t *, zip_flags_t) {
    return NULL;
}

int zip_file_set_comment(zip_t *, zip_uint64_t, const char *, zip_uint16_t, zip_flags_t) {
    assert(false);
    return 0;
}

// tests

/**
 * Test parse_name()
 */
void parseNameTest () {
    struct zip zip;
    unique_ptr<FileNode> n (FileNode::createRootNode(&zip));

    n->full_name = "test";
    n->parse_name ();
    assert (n->name == n->full_name.c_str());

    n->full_name = "dir/test";
    n->parse_name ();
    assert (n->name == n->full_name.c_str() + strlen("dir/"));

    n->full_name = "dir/dir2/dir3/test";
    n->parse_name ();
    assert (n->name == n->full_name.c_str() + strlen("dir/dir2/dir3/"));

    n->full_name = "subdir/";
    n->parse_name ();
    assert (n->name == n->full_name.c_str());

    n->full_name = "dir/subdir/";
    n->parse_name ();
    assert (n->name == n->full_name.c_str() + strlen("dir/"));

    n->full_name = "dir/dir2/dir3/subdir/";
    n->parse_name ();
    assert (n->name == n->full_name.c_str() + strlen("dir/dir2/dir3/"));
}

/**
 * Test getParentName()
 */
void parentNameTest () {
    // files
    {
        unique_ptr<FileNode> n (FileNode::createFile(NULL, "test", 0, 0, 0666));
        assert (n->getParentName() == "");
    }
    {
        unique_ptr<FileNode> n (FileNode::createFile(NULL, "dir/file", 0, 0, 0666));
        assert (n->getParentName() == "dir");
    }
    {
        unique_ptr<FileNode> n (FileNode::createFile(NULL, "dir/dir2/file", 0, 0, 0666));
        assert (n->getParentName() == "dir/dir2");
    }
    // directories
    {
        unique_ptr<FileNode> n (FileNode::createIntermediateDir(NULL, "dir/subdir/"));
        assert (n->getParentName() == "dir");
    }
}

int main(int, char **) {
    parseNameTest ();
    parentNameTest ();

    return EXIT_SUCCESS;
}


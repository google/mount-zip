// Copyright 2021 Google LLC
// Copyright 2010-2021 Alexander Galanin <al@galanin.nnov.ru>
// http://galanin.nnov.ru/~al
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "../config.h"

#include <cassert>
#include <iostream>

#include <fuse.h>
#include <stdlib.h>
#include <zip.h>

#include "common.h"
#include "tree.h"

// FUSE stub functions

struct fuse_context* fuse_get_context(void) {
  return NULL;
}

// libzip stub structures
struct zip {};
struct zip_file {};
struct zip_source {};

// libzip stub functions

struct zip* zip_open(const char*, int, int* errorp) {
  *errorp = 0;
  return NULL;
}

void zip_error_init_with_code(zip_error_t*, int) {}

const char* zip_error_strerror(zip_error_t*) {
  return "Expected error";
}

void zip_error_fini(zip_error_t*) {}

// only stubs

const char* zip_get_name(struct zip*, zip_uint64_t, zip_flags_t) {
  assert(false);
  return NULL;
}

zip_int64_t zip_file_add(struct zip*,
                         const char*,
                         struct zip_source*,
                         zip_flags_t) {
  assert(false);
  return 0;
}

zip_int64_t zip_dir_add(struct zip*, const char*, zip_flags_t) {
  assert(false);
  return 0;
}

int zip_close(struct zip*) {
  assert(false);
  return 0;
}

int zip_delete(struct zip*, zip_uint64_t) {
  assert(false);
  return 0;
}

int zip_fclose(struct zip_file*) {
  assert(false);
  return 0;
}

struct zip_file* zip_fopen_index(struct zip*, zip_uint64_t, zip_flags_t) {
  assert(false);
  return NULL;
}

zip_int64_t zip_fread(struct zip_file*, void*, zip_uint64_t) {
  assert(false);
  return 0;
}

zip_int64_t zip_get_num_entries(struct zip*, zip_flags_t) {
  assert(false);
  return 0;
}

int zip_file_rename(struct zip*, zip_uint64_t, const char*, zip_flags_t) {
  assert(false);
  return 0;
}

int zip_file_replace(struct zip*,
                     zip_uint64_t,
                     struct zip_source*,
                     zip_flags_t) {
  assert(false);
  return 0;
}

void zip_source_free(struct zip_source*) {
  assert(false);
}

struct zip_source* zip_source_function(struct zip*,
                                       zip_source_callback,
                                       void*) {
  assert(false);
  return NULL;
}

int zip_stat_index(struct zip*, zip_uint64_t, zip_flags_t, struct zip_stat*) {
  assert(false);
  return 0;
}

const char* zip_strerror(struct zip*) {
  assert(false);
  return NULL;
}

const char* zip_file_strerror(struct zip_file*) {
  assert(false);
  return NULL;
}

const char* zip_get_archive_comment(zip_t*, int*, zip_flags_t) {
  assert(false);
  return NULL;
}

int zip_set_archive_comment(zip_t*, const char*, zip_uint16_t) {
  assert(false);
  return 0;
}

const char* zip_file_get_comment(zip_t*,
                                 zip_uint64_t,
                                 zip_uint32_t*,
                                 zip_flags_t) {
  assert(false);
  return NULL;
}

int zip_file_set_comment(zip_t*,
                         zip_uint64_t,
                         const char*,
                         zip_uint16_t,
                         zip_flags_t) {
  assert(false);
  return 0;
}

// test functions

int main() {
  initTest();

  bool thrown = false;
  try {
    std::unique_ptr<Tree> data = Tree::Init("test.zip");
  } catch (const std::runtime_error& e) {
    assert(
        e.what() ==
        std::string_view("Cannot open ZIP archive 'test.zip': Expected error"));
    thrown = true;
  }

  assert(thrown);

  return EXIT_SUCCESS;
}

// Copyright 2021 Google LLC
// Copyright 2019-2021 Alexander Galanin <al@galanin.nnov.ru>
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

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>

#include <zip.h>

struct context {
  zip_uint64_t offset;
  long long size;
};

zip_int64_t callback(void* user,
                     void* data,
                     zip_uint64_t len,
                     enum zip_source_cmd cmd) {
  struct context* ctx = (struct context*)user;
  switch (cmd) {
    case ZIP_SOURCE_OPEN:
      ctx->offset = 0;
      return 0;
    case ZIP_SOURCE_READ: {
      size_t nr = std::numeric_limits<size_t>::max();
      if (len < nr)
        nr = static_cast<size_t>(len);
      if (ctx->size - ctx->offset < nr)
        nr = static_cast<size_t>(ctx->size - ctx->offset);
      memset(data, 0, nr);
      ctx->offset += nr;
      return nr;
    }
    case ZIP_SOURCE_STAT: {
      struct zip_stat* st = (struct zip_stat*)data;
      zip_stat_init(st);
      st->valid = ZIP_STAT_SIZE | ZIP_STAT_MTIME;
      st->size = ctx->size;
      st->mtime = time(NULL);
      return sizeof(struct zip_stat);
    }
    case ZIP_SOURCE_FREE:
      return 0;
    case ZIP_SOURCE_CLOSE:
      return 0;
    case ZIP_SOURCE_ERROR: {
      int* errs = static_cast<int*>(data);
      errs[0] = ZIP_ER_OPNOTSUPP;
      errs[1] = EINVAL;
      return 2 * sizeof(int);
    }
    case ZIP_SOURCE_SUPPORTS:
      return ZIP_SOURCE_SUPPORTS_READABLE;
    default:
      // indicate unsupported operation
      return -1;
  }
}

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <output-zip-file> <size>\n", argv[0]);
    return EXIT_FAILURE;
  }

  struct context ctx;
  ctx.size = atoll(argv[2]);
  printf("creating %s of size %llx bytes\n", argv[1], ctx.size);

  int err;
  struct zip* z = zip_open(argv[1], ZIP_CREATE | ZIP_TRUNCATE, &err);

  struct zip_source* s = zip_source_function(z, callback, &ctx);
  zip_add(z, "content", s);

  zip_close(z);
  return 0;
}

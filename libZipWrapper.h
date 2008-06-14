#ifndef LIBZIPWRAPPER_H
#define LIBZIPWRAPPER_H

//
// This is compatibility layer to support libzip compiled without large file
// support on 32-bit system.
//
// If on your system libzip compiled with -D_FILE_OFFSET_BITS=64 then you must
// add -D_FILE_OFFSET_BITS=64 to CXXFLAGS when running make.
//
// make CXXFLAGS="$CXXFLAGS -D_FILE_OFFSET_BITS=64" clean all
//

#include <unistd.h>
#include <sys/types.h>
#include <ctime>

#include "types.h"

struct zip_stat_64 {
    const char *name;                   /* name of the file */
    int index;                          /* index within archive */
    unsigned int crc;                   /* crc of file data */
    time_t mtime;                       /* modification time */
//modified type
    offset_t size;                       /* size of file (uncompressed) */
//modified type
    offset_t comp_size;                  /* size of file (compressed) */
    unsigned short comp_method;         /* compression method used */
    unsigned short encryption_method;   /* encryption method used */
};

void zip_stat_assign_64_to_default(struct zip_stat_64 *dest, const struct zip_stat *src);

#endif


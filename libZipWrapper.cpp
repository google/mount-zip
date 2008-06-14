#include <zip.h>

#include "libZipWrapper.h"

void zip_stat_assign_64_to_default(struct zip_stat_64 *dest, const struct zip_stat *src) {
    dest->name = src->name;
    dest->index = src->index;
    dest->crc = src->crc;
    dest->mtime = src->mtime;
    dest->comp_method = src->comp_method;
    dest->encryption_method = src->encryption_method;

    dest->size = off64_t(src->size);
    dest->comp_size = off64_t(src->comp_size);
}


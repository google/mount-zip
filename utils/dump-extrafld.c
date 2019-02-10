#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>

#include "lib/extraField.h"

void print_time (const char *label, time_t time) {
    char str[1024];
    time_t t = (time_t)time;
    struct tm *tmp;
    tmp = localtime(&t);
    if (tmp == NULL) {
        perror("localtime");
        exit(EXIT_FAILURE);
    }
    if (strftime(str, sizeof(str), "%a, %d %b %Y %T %z", tmp) == 0) {
        fprintf(stderr, "strftime returned 0");
        exit(EXIT_FAILURE);
    }

    printf("      %s: %s\n", label, str);
}

void dump_extrafld(zip_uint16_t id, zip_uint16_t len, const zip_uint8_t *field, bool central) {
    const zip_uint8_t *end = field + len;
    for (const zip_uint8_t *f = field; f < end; ++f) {
        printf("0x%02X, ", *f);
    }
    printf("\n");
    switch (id) {
        case FZ_EF_TIMESTAMP: {
            bool has_mtime, has_atime, has_cretime;
            time_t mtime, atime, cretime;
            ExtraField::parseExtTimeStamp (len, field, has_mtime, mtime, has_atime, atime, has_cretime, cretime);
            printf("    extended timestamp\n");
            unsigned char flags = *field;
            printf("      flags %d: mod %d acc %d cre %d\n", flags, has_mtime, has_atime, has_cretime);
            if (has_mtime) {
                print_time("mtime", mtime);
            }
            if (!central) {
                if (has_atime) {
                    print_time("atime", atime);
                }
                if (has_cretime) {
                    print_time("cretime", cretime);
                }
            }
            break;
        }

        case FZ_EF_PKWARE_UNIX:
        case FZ_EF_INFOZIP_UNIX1:
        case FZ_EF_INFOZIP_UNIX2:
        {
            switch (id)
            {
                case FZ_EF_PKWARE_UNIX:
                    printf("    PKWare Unix\n");
                    break;
                case FZ_EF_INFOZIP_UNIX1:
                    printf("    Info-ZIP Unix v1\n");
                    break;
                case FZ_EF_INFOZIP_UNIX2:
                    printf("    Info-ZIP Unix v2\n");
                    break;
            }
            if (central)
                break;
            bool has_mtime, has_atime;
            time_t mtime, atime;
            uid_t uid;
            gid_t gid;
            bool res = ExtraField::parseSimpleUnixField(id, len, field, uid, gid, has_mtime, mtime, has_atime, atime);
            if (!res) {
                printf("      parse failed\n");
                break;
            }
            printf("      UID %u\n", uid);
            printf("      GID %u\n", gid);
            if (has_atime) {
                print_time("atime", atime);
            }
            if (has_mtime) {
                print_time("mtime", mtime);
            }
            break;
        }

        case FZ_EF_INFOZIP_UNIXN: {
            printf("    Info-ZIP Unix (new)\n");
            if (len < 2) break;
            unsigned char version = *field++;
            printf("      version %d\n", version);
            int l = *field++, shift = 0;
            printf("      len(UID) %d\n", l);
            if (field + l > end) break;
            unsigned long long uid = 0, gid = 0;
            while (l-- > 0) {
                uid = uid + (*field++ << shift);
                shift += 8;
            }
            printf("      UID %llu\n", uid);
            l = *field++;
            shift = 0;
            printf("      len(GID) %d\n", l);
            if (field + l > end) break;
            while (l-- > 0) {
                gid = gid + (*field++ << shift);
                shift += 8;
            }
            printf("      GID %llu\n", gid);
            break;
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <zip-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int err;
    struct zip *z = zip_open(argv[1], 0, &err);
    if (z == NULL) {
        fprintf(stderr, "file open error\n");
        return EXIT_FAILURE;
    }

    for (zip_int64_t i = 0; i < zip_get_num_entries(z, 0); ++i) {
        zip_uint8_t opsys;
        zip_uint32_t attr;
        zip_file_get_external_attributes(z, i, 0, &opsys, &attr);
        const char *opsys_s;
        switch (opsys)
        {
            case ZIP_OPSYS_UNIX:
                opsys_s = "UNIX";
                break;
            case ZIP_OPSYS_DOS:
                opsys_s = "DOS";
                break;
            case ZIP_OPSYS_WINDOWS_NTFS:
                opsys_s = "WINDOWS NTFS";
                break;
            case ZIP_OPSYS_MVS:
                opsys_s = "MVS";
                break;
            default:
                opsys_s = "unknown";
        }
        printf("%s\t(opsys %s (%d), mode1 0%06lo, mode2 0x%04X):\n",
                zip_get_name(z, i, ZIP_FL_ENC_STRICT), opsys_s, opsys, (long)attr >> 16, attr & 0xffff);
        for (zip_int16_t j = 0; j < zip_file_extra_fields_count(z, i, ZIP_FL_CENTRAL); ++j) {
            zip_uint16_t id, len;
            const zip_uint8_t *field = zip_file_extra_field_get(z, i, j, &id, &len, ZIP_FL_CENTRAL);
            printf("  0x%04X len=%d central: ", id, len);
            dump_extrafld(id, len, field, true);
        }
        for (zip_int16_t j = 0; j < zip_file_extra_fields_count(z, i, ZIP_FL_LOCAL); ++j) {
            zip_uint16_t id, len;
            const zip_uint8_t *field = zip_file_extra_field_get(z, i, j, &id, &len, ZIP_FL_LOCAL);
            printf("  0x%04X len=%d local: ", id, len);
            dump_extrafld(id, len, field, false);
        }
    }

    zip_close(z);
    return EXIT_SUCCESS;
}

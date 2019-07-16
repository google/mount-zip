#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>

#include "lib/extraField.h"

void print_time (const char *label, struct timespec &time) {
    char str1[512], str2[16];
    time_t t = (time_t)time.tv_sec;
    struct tm *tmp;
    tmp = localtime(&t);
    if (tmp == NULL) {
        perror("localtime");
        exit(EXIT_FAILURE);
    }
    if (strftime(str1, sizeof(str1), "%Y-%m-%d %H:%M:%S", tmp) == 0) {
        fprintf(stderr, "strftime returned 0");
        exit(EXIT_FAILURE);
    }
    if (strftime(str2, sizeof(str2), "%z", tmp) == 0) {
        fprintf(stderr, "strftime returned 0");
        exit(EXIT_FAILURE);
    }

    printf("      %s%s.%09lu %s\n", label, str1, static_cast<long unsigned>(time.tv_nsec), str2);
}

void print_time (const char *label, time_t time) {
    struct timespec ts;
    ts.tv_sec = time;
    ts.tv_nsec = 0;
    print_time(label, ts);
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
                print_time("mtime:   ", mtime);
            }
            if (!central) {
                if (has_atime) {
                    print_time("atime:   ", atime);
                }
                if (has_cretime) {
                    print_time("cretime: ", cretime);
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
                print_time("atime:   ", atime);
            }
            if (has_mtime) {
                print_time("mtime:   ", mtime);
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

        case FZ_EF_NTFS:
        {
            printf("    NTFS Extra Field\n");
            struct timespec mtime, atime, cretime;
            bool res = ExtraField::parseNtfsExtraField(len, field, mtime, atime, cretime);
            if (!res) {
                printf("      parse failed or no timestamp data\n");
                break;
            }
            print_time("mtime:   ", mtime);
            print_time("atime:   ", atime);
            print_time("cretime: ", cretime);
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
                zip_get_name(z, i, ZIP_FL_ENC_STRICT), opsys_s, opsys, (unsigned long)attr >> 16, attr & 0xffff);


        zip_stat_t stat;
        if (zip_stat_index(z, i, 0, &stat) != 0)
        {
            fprintf(stderr, "zip_stat_index failed\n");
            return EXIT_FAILURE;
        }
        printf("  zip_stat:\n");
        if (stat.valid & ZIP_STAT_SIZE)
            printf("      orig.size:  %llu\n", (long long unsigned int)stat.size);
        if (stat.valid & ZIP_STAT_COMP_SIZE)
            printf("      comp.size:  %llu\n", (long long unsigned int)stat.comp_size);
        if (stat.valid & ZIP_STAT_MTIME)
            print_time("stat.mtime: ", stat.mtime);
        if (stat.valid & ZIP_STAT_CRC)
            printf("      CRC:        0x%08lX\n", (long unsigned int)stat.crc);
        if (stat.valid & ZIP_STAT_COMP_METHOD)
        {
            const char *method;
            switch (stat.comp_method)
            {
                case ZIP_CM_STORE:
                    method = "stored (uncompressed)";
                    break;
                case ZIP_CM_SHRINK:
                    method = "shrunk";
                    break;
                case ZIP_CM_REDUCE_1:
                    method = "reduced with factor 1";
                    break;
                case ZIP_CM_REDUCE_2:
                    method = "reduced with factor 2";
                    break;
                case ZIP_CM_REDUCE_3:
                    method = "reduced with factor 3";
                    break;
                case ZIP_CM_REDUCE_4:
                    method = "reduced with factor 4";
                    break;
                case ZIP_CM_IMPLODE:
                    method = "imploded";
                    break;
                case 7:
                    method = "tokenizing compression";
                    break;
                case ZIP_CM_DEFLATE:
                    method = "deflated";
                    break;
                case ZIP_CM_DEFLATE64:
                    method = "deflate64";
                    break;
                case ZIP_CM_PKWARE_IMPLODE:
                    method = "PKWARE imploding";
                    break;
                case ZIP_CM_BZIP2:
                    method = "BZIP2";
                    break;
                case ZIP_CM_LZMA:
                    method = "LZMA (EFS)";
                    break;
                case ZIP_CM_TERSE:
                    method = "IBM TERSE (new)";
                    break;
                case ZIP_CM_LZ77:
                    method = "IBM LZ77 z Architecture (PFS)";
                    break;
                case ZIP_CM_XZ:
                    method = "XZ";
                    break;
                case ZIP_CM_JPEG:
                    method = "JPEG";
                    break;
                case ZIP_CM_WAVPACK:
                    method = "WavPack";
                    break;
                case ZIP_CM_PPMD:
                    method = "PPMd version I, Rev 1";
                    break;
                case 11:
                case 13:
                case 15:
                case 16:
                case 17:
                    method = "Reserved by PKWARE";
                    break;
                default:
                    method = "UNKNOWN";
            }
            printf("      compressed: %s (%u)\n", method, stat.comp_method);
        }
        if (stat.valid & ZIP_STAT_ENCRYPTION_METHOD)
        {
            const char *method;
            switch (stat.encryption_method)
            {
                case 0:
                    method = "none";
                    break;
                case 0x6601: 
                    method = "DES";
                    break;
                case 0x6602: 
                    method = "RC2 (version needed to extract < 5.2)";
                    break;
                case 0x6603: 
                    method = "3DES 168";
                    break;
                case 0x6609: 
                    method = "3DES 112";
                    break;
                case 0x660E: 
                    method = "AES 128 ";
                    break;
                case 0x660F: 
                    method = "AES 192 ";
                    break;
                case 0x6610: 
                    method = "AES 256 ";
                    break;
                case 0x6702: 
                    method = "RC2 (version needed to extract >= 5.2)";
                    break;
                case 0x6720: 
                    method = "Blowfish";
                    break;
                case 0x6721: 
                    method = "Twofish";
                    break;
                case 0x6801: 
                    method = "RC4";
                    break;
                default:
                    method = "UNKNOWN";
            }
            printf("      encrypted:  %s (0x%04X)\n", method, stat.encryption_method);
        }

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

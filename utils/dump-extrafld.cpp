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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zip.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "lib/extra_field.h"

void print_time(const char* label, struct timespec& time) {
  char str1[512], str2[16];
  time_t t = (time_t)time.tv_sec;
  struct tm* tmp;
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

  printf("      %s%s.%09lu %s\n", label, str1,
         static_cast<long unsigned>(time.tv_nsec), str2);
}

void print_time(const char* label, time_t time) {
  struct timespec ts;
  ts.tv_sec = time;
  ts.tv_nsec = 0;
  print_time(label, ts);
}

void dump_extrafld(zip_uint16_t id,
                   zip_uint16_t len,
                   const zip_uint8_t* field,
                   bool central,
                   mode_t mode) {
  const zip_uint8_t* end = field + len;
  for (const zip_uint8_t* f = field; f < end; ++f) {
    printf("0x%02X, ", *f);
  }
  printf("\n");
  switch (id) {
    case FZ_EF_TIMESTAMP: {
      bool has_mtime, has_atime, has_cretime;
      time_t mtime, atime, cretime;
      ExtraField::parseExtTimeStamp(len, field, has_mtime, mtime, has_atime,
                                    atime, has_cretime, cretime);
      printf("    extended timestamp\n");
      unsigned char flags = *field;
      printf("      flags %d: mod %d acc %d cre %d\n", flags, has_mtime,
             has_atime, has_cretime);
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

    case FZ_EF_PKWARE_UNIX: {
      printf("    PKWare Unix\n");
      time_t atime, mtime;
      uid_t uid;
      gid_t gid;
      dev_t dev;
      const char* link;
      uint16_t link_len;
      bool res = ExtraField::parsePkWareUnixField(
          len, field, mode, mtime, atime, uid, gid, dev, link, link_len);
      if (!res) {
        printf("      parse failed\n");
        break;
      }
      print_time("atime:   ", atime);
      print_time("mtime:   ", mtime);
      printf("      UID %u\n", uid);
      printf("      GID %u\n", gid);
      if (S_ISBLK(mode) || S_ISCHR(mode)) {
        printf("      device: %u, %u\n", major(dev), minor(dev));
      }
      if (link_len > 0) {
        printf("      link target: %.*s\n", link_len, link);
      }
      break;
    }

    case FZ_EF_INFOZIP_UNIX1: {
      printf("    Info-ZIP Unix v1\n");
      bool has_uid_gid;
      time_t mtime, atime;
      uid_t uid;
      gid_t gid;
      bool res = ExtraField::parseSimpleUnixField(id, len, field, has_uid_gid,
                                                  uid, gid, mtime, atime);
      if (!res) {
        printf("      parse failed\n");
        break;
      }
      if (has_uid_gid) {
        printf("      UID %u\n", uid);
        printf("      GID %u\n", gid);
      }
      print_time("atime:   ", atime);
      print_time("mtime:   ", mtime);
      break;
    }

    case FZ_EF_INFOZIP_UNIX2: {
      printf("    Info-ZIP Unix v2\n");
      uid_t uid;
      gid_t gid;
      if (ExtraField::parseUnixUidGidField(id, len, field, uid, gid)) {
        printf("      UID %u\n", uid);
        printf("      GID %u\n", gid);
      }
      break;
    }

    case FZ_EF_INFOZIP_UNIXN: {
      printf("    Info-ZIP Unix (new)\n");
      if (len < 2)
        break;
      unsigned char version = *field++;
      printf("      version %d\n", version);
      int l = *field++, shift = 0;
      printf("      len(UID) %d\n", l);
      if (field + l > end)
        break;
      unsigned long long uid = 0, gid = 0;
      while (l-- > 0) {
        uid = uid + (*field++ << shift);
        shift += 8;
      }
      printf("      UID %llu\n", uid);
      l = *field++;
      shift = 0;
      printf("      len(GID) %d\n", l);
      if (field + l > end)
        break;
      while (l-- > 0) {
        gid = gid + (*field++ << shift);
        shift += 8;
      }
      printf("      GID %llu\n", gid);
      break;
    }

    case FZ_EF_NTFS: {
      printf("    NTFS Extra Field\n");
      struct timespec mtime, atime, cretime;
      bool res =
          ExtraField::parseNtfsExtraField(len, field, mtime, atime, cretime);
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

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <zip-file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int err;
  struct zip* z = zip_open(argv[1], 0, &err);
  if (z == NULL) {
    fprintf(stderr, "file open error\n");
    return EXIT_FAILURE;
  }

  {
    int len = 0;
    const char* comment = zip_get_archive_comment(z, &len, ZIP_FL_ENC_RAW);
    if (comment != NULL && len > 0)
      printf("archive comment: %*s\n", len, comment);
  }

  for (zip_int64_t i = 0; i < zip_get_num_entries(z, 0); ++i) {
    zip_uint8_t opsys;
    zip_uint32_t attr;
    zip_file_get_external_attributes(z, i, 0, &opsys, &attr);
    const char* opsys_s;
    switch (opsys) {
      case ZIP_OPSYS_UNIX:
        opsys_s = "UNIX";
        break;
      case ZIP_OPSYS_DOS:
        opsys_s = "DOS compatible";
        break;
      case ZIP_OPSYS_WINDOWS_NTFS:
        opsys_s = "Windows NTFS";
        break;
      case ZIP_OPSYS_MVS:
        opsys_s = "MVS (PKWARE) or Windows NTFS (Info-Zip)";
        break;
      default:
        opsys_s = "unknown";
    }
    unsigned int unix_mode = attr >> 16;
    printf("%s\t(opsys: %s (%d), mode1: 0%06o, mode2: 0x%04X):\n",
           zip_get_name(z, i, ZIP_FL_ENC_STRICT), opsys_s, opsys, unix_mode,
           attr & 0xffff);

    /*
     * PKWARE describes "OS made by" now (since 1998) as follows:
     * The upper byte indicates the compatibility of the file attribute
     * information.  If the external file attributes are compatible with
     * MS-DOS and can be read by PKZIP for DOS version 2.04g then this
     * value will be zero.
     */
    if (opsys == ZIP_OPSYS_DOS && (unix_mode & S_IFMT) != 0)
      opsys = ZIP_OPSYS_UNIX;
    switch (opsys) {
      case ZIP_OPSYS_UNIX: {
        unsigned int mode = unix_mode;
        printf("  type: ");
        switch (mode & S_IFMT) {
          case S_IFSOCK:
            printf("socket");
            break;
          case S_IFLNK:
            printf("symlink");
            break;
          case S_IFREG:
            printf("regular file");
            break;
          case S_IFBLK:
            printf("block device");
            break;
          case S_IFDIR:
            printf("directory");
            break;
          case S_IFCHR:
            printf("character device");
            break;
          case S_IFIFO:
            printf("FIFO");
            break;
          default:
            printf("unknown (0x%0X)", mode & S_IFMT);
        }
        printf("\n");
        printf("  mode: %03o ", mode);
        printf("%c%c%c ", (mode & S_IRUSR) ? 'r' : '-',
               (mode & S_IWUSR) ? 'w' : '-',
               (mode & S_IXUSR) ? ((mode & S_ISUID) ? 's' : 'x')
                                : ((mode & S_ISUID) ? 'S' : '-'));
        printf("%c%c%c ", (mode & S_IRGRP) ? 'r' : '-',
               (mode & S_IWGRP) ? 'w' : '-',
               (mode & S_IXGRP) ? ((mode & S_ISGID) ? 's' : 'x')
                                : ((mode & S_ISGID) ? 'S' : '-'));
        printf("%c%c%c ", (mode & S_IROTH) ? 'r' : '-',
               (mode & S_IWOTH) ? 'w' : '-',
               (mode & S_IXOTH) ? ((mode & S_ISVTX) ? 't' : 'x')
                                : ((mode & S_ISVTX) ? 'T' : '-'));
        printf("\n");
        break;
      }
      case ZIP_OPSYS_DOS:
      case ZIP_OPSYS_WINDOWS_NTFS:
      case ZIP_OPSYS_MVS: {
        printf("  attributes: ");
        printf("%c%c%c%c", (attr & 0x01) ? 'R' : '-', (attr & 0x20) ? 'A' : '-',
               (attr & 0x02) ? 'H' : '-', (attr & 0x04) ? 'S' : '-');
        printf("\n");
        break;
      }
    }

    {
      uint32_t len = 0;
      const char* comment = zip_file_get_comment(z, i, &len, ZIP_FL_ENC_RAW);
      if (comment != NULL && len > 0) {
        int len_int =
            static_cast<int>(len);  // file comment length is actually 16-bit
        printf("  comment: %*s\n", len_int, comment);
      }
    }

    zip_stat_t stat;
    if (zip_stat_index(z, i, 0, &stat) != 0) {
      fprintf(stderr, "zip_stat_index failed\n");
      return EXIT_FAILURE;
    }
    printf("  zip_stat:\n");
    if (stat.valid & ZIP_STAT_SIZE)
      printf("      orig.size:  %llu\n", (long long unsigned int)stat.size);
    if (stat.valid & ZIP_STAT_COMP_SIZE)
      printf("      comp.size:  %llu\n",
             (long long unsigned int)stat.comp_size);
    if (stat.valid & ZIP_STAT_MTIME)
      print_time("stat.mtime: ", stat.mtime);
    if (stat.valid & ZIP_STAT_CRC)
      printf("      CRC:        0x%08lX\n", (long unsigned int)stat.crc);
    if (stat.valid & ZIP_STAT_COMP_METHOD) {
      const char* method;
      switch (stat.comp_method) {
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
    if (stat.valid & ZIP_STAT_ENCRYPTION_METHOD) {
      const char* method;
      switch (stat.encryption_method) {
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

    for (zip_int16_t j = 0;
         j < zip_file_extra_fields_count(z, i, ZIP_FL_CENTRAL); ++j) {
      zip_uint16_t id, len;
      const zip_uint8_t* field =
          zip_file_extra_field_get(z, i, j, &id, &len, ZIP_FL_CENTRAL);
      printf("  0x%04X len=%d central: ", id, len);
      dump_extrafld(id, len, field, true, static_cast<mode_t>(unix_mode));
    }
    for (zip_int16_t j = 0; j < zip_file_extra_fields_count(z, i, ZIP_FL_LOCAL);
         ++j) {
      zip_uint16_t id, len;
      const zip_uint8_t* field =
          zip_file_extra_field_get(z, i, j, &id, &len, ZIP_FL_LOCAL);
      printf("  0x%04X len=%d local: ", id, len);
      dump_extrafld(id, len, field, false, static_cast<mode_t>(unix_mode));
    }
  }

  zip_close(z);
  return EXIT_SUCCESS;
}

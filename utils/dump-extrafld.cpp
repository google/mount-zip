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

void print_time(const char* label, const timespec& time) {
  if (time.tv_sec == -1) {
    return;
  }
  struct tm tmp;
  if (!localtime_r(&time.tv_sec, &tmp)) {
    perror("localtime");
    exit(EXIT_FAILURE);
  }

  char str1[512];
  if (strftime(str1, sizeof(str1), "%Y-%m-%d %H:%M:%S", &tmp) == 0) {
    fprintf(stderr, "strftime returned 0");
    exit(EXIT_FAILURE);
  }

  char str2[16];
  if (strftime(str2, sizeof(str2), "%z", &tmp) == 0) {
    fprintf(stderr, "strftime returned 0");
    exit(EXIT_FAILURE);
  }

  printf("      %s%s.%09lu %s\n", label, str1,
         static_cast<long unsigned>(time.tv_nsec), str2);
}

void print_time(const char* label, time_t time) {
  print_time(label, timespec{.tv_sec = time, .tv_nsec = 0});
}

void dump_extrafld(FieldId id, Bytes b, mode_t mode) {
  for (const std::byte c : b) {
    printf("\\x%02X", static_cast<unsigned int>(c));
  }
  printf("\n");
  switch (id) {
    case FieldId::UNIX_TIMESTAMP: {
      printf("    UNIX timestamp\n");
      ExtraFields f;
      if (!f.Parse(id, b)) {
        printf("      Cannot parse\n");
        break;
      }

      int flags = static_cast<int>(b.front());
      printf("      flags: %d\n", flags);
      print_time("mtime: ", f.mtime);
      print_time("atime: ", f.atime);
      print_time("ctime: ", f.ctime);

      break;
    }

    case FieldId::PKWARE_UNIX: {
      printf("    PKWare Unix\n");
      ExtraFields f;
      if (!f.Parse(id, b, mode)) {
        printf("      Cannot parse\n");
        break;
      }

      print_time("mtime: ", f.mtime);
      print_time("atime: ", f.atime);
      print_time("ctime: ", f.ctime);
      if (f.uid != -1) {
        printf("      UID:   %u\n", f.uid);
      }
      if (f.gid != -1) {
        printf("      GID:   %u\n", f.gid);
      }
      if (f.dev != -1) {
        printf("      device: %u, %u\n", major(f.dev), minor(f.dev));
      }
      if (!f.link_target.empty()) {
        printf("      link:   %.*s\n",
               static_cast<int>(f.link_target.size()), f.link_target.data());
      }
      break;
    }

    case FieldId::INFOZIP_UNIX_1: {
      printf("    Info-ZIP Unix v1\n");
      ExtraFields f;
      if (!f.Parse(id, b)) {
        printf("      Cannot parse\n");
        break;
      }

      if (f.uid != -1) {
        printf("      UID %u\n", f.uid);
      }

      if (f.gid != -1) {
        printf("      GID %u\n", f.gid);
      }

      print_time("mtime: ", f.mtime);
      print_time("atime: ", f.atime);
      print_time("ctime: ", f.ctime);
      break;
    }

    case FieldId::INFOZIP_UNIX_2: {
      printf("    Info-ZIP Unix v2\n");
      ExtraFields f;
      if (!f.Parse(id, b)) {
        printf("      Cannot parse\n");
        break;
      }

      printf("      UID %u\n", f.uid);
      printf("      GID %u\n", f.gid);
      break;
    }

    case FieldId::INFOZIP_UNIX_3: {
      printf("    Info-ZIP Unix (new)\n");
      if (b.size() < 2)
        break;
      int version = static_cast<int>(b.front());
      b = b.subspan(1);
      printf("      version: %d\n", version);
      int l = static_cast<int>(b.front());
      b = b.subspan(1);
      int shift = 0;
      if (b.size() < l)
        break;
      unsigned long long uid = 0, gid = 0;
      while (l-- > 0) {
        uid += static_cast<int>(b.front()) << shift;
        b = b.subspan(1);
        shift += 8;
      }
      printf("      UID:     %llu\n", uid);
      l = static_cast<int>(b.front());
      b = b.subspan(1);
      shift = 0;
      if (b.size() < l)
        break;
      while (l-- > 0) {
        gid += static_cast<int>(b.front()) << shift;
        b = b.subspan(1);
        shift += 8;
      }
      printf("      GID:     %llu\n", gid);
      break;
    }

    case FieldId::NTFS_TIMESTAMP: {
      printf("    NTFS timestamp\n");
      ExtraFields f;
      if (!f.Parse(id, b)) {
        printf("      Cannot parse\n");
        break;
      }

      print_time("mtime: ", f.mtime);
      print_time("atime: ", f.atime);
      print_time("ctime: ", f.ctime);
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

  zip_int64_t const n = zip_get_num_entries(z, 0);
  for (zip_int64_t i = 0; i < n; ++i) {
    zip_uint8_t opsys;
    zip_uint32_t attr;
    zip_file_get_external_attributes(z, i, 0, &opsys, &attr);
    const char* opsys_s;
    switch (opsys) {
      case ZIP_OPSYS_UNIX:
        opsys_s = "UNIX";
        break;
      case ZIP_OPSYS_DOS:
        opsys_s = "DOS";
        break;
      case ZIP_OPSYS_WINDOWS_NTFS:
        opsys_s = "Windows NTFS";
        break;
      case ZIP_OPSYS_MVS:
        opsys_s = "MVS (PKWARE) or Windows NTFS (Info-Zip)";
        break;
      default:
        opsys_s = "Unknown";
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
            printf("Socket");
            break;
          case S_IFLNK:
            printf("Symlink");
            break;
          case S_IFREG:
            printf("Regular file");
            break;
          case S_IFBLK:
            printf("Block device");
            break;
          case S_IFDIR:
            printf("Directory");
            break;
          case S_IFCHR:
            printf("Character device");
            break;
          case S_IFIFO:
            printf("FIFO");
            break;
          default:
            printf("Unknown (0x%0X)", mode & S_IFMT);
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
      if (comment != nullptr && len > 0) {
        printf("  comment: %*s\n", static_cast<int>(len), comment);
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
      print_time("mtime:      ", stat.mtime);
    if (stat.valid & ZIP_STAT_CRC)
      printf("      CRC:        0x%08lX\n", (long unsigned int)stat.crc);
    if (stat.valid & ZIP_STAT_COMP_METHOD) {
      const char* method;
      switch (stat.comp_method) {
        case ZIP_CM_STORE:
          method = "Stored (uncompressed)";
          break;
        case ZIP_CM_SHRINK:
          method = "Shrunk";
          break;
        case ZIP_CM_REDUCE_1:
          method = "Reduced with factor 1";
          break;
        case ZIP_CM_REDUCE_2:
          method = "Reduced with factor 2";
          break;
        case ZIP_CM_REDUCE_3:
          method = "Reduced with factor 3";
          break;
        case ZIP_CM_REDUCE_4:
          method = "Reduced with factor 4";
          break;
        case ZIP_CM_IMPLODE:
          method = "Imploded";
          break;
        case 7:
          method = "Tokenizing compression";
          break;
        case ZIP_CM_DEFLATE:
          method = "Deflated";
          break;
        case ZIP_CM_DEFLATE64:
          method = "Deflate64";
          break;
        case ZIP_CM_PKWARE_IMPLODE:
          method = "PKWARE Imploding";
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
          method = "Unknown";
      }
      printf("      compressed: %s (%u)\n", method, stat.comp_method);
    }

    if (stat.valid & ZIP_STAT_ENCRYPTION_METHOD) {
      const char* method;
      switch (stat.encryption_method) {
        case ZIP_EM_NONE:
          method = "None";
          break;
        case ZIP_EM_TRAD_PKWARE:
          method = "PKWARE ZipCrypto";
          break;
        case ZIP_EM_AES_128:
          method = "AES 128 ";
          break;
        case ZIP_EM_AES_192:
          method = "AES 192 ";
          break;
        case ZIP_EM_AES_256:
          method = "AES 256 ";
          break;
        case 0x6601:
          method = "DES";
          break;
        case 0x6609:
          method = "3DES 112";
          break;
        case 0x6603:
          method = "3DES 168";
          break;
        case 0x660E:
          method = "PKZIP AES 128 ";
          break;
        case 0x660F:
          method = "PKZIP AES 192 ";
          break;
        case 0x6610:
          method = "PKZIP AES 256 ";
          break;
        case 0x6602:
          method = "RC2 (version needed to extract < 5.2)";
          break;
        case 0x6702:
          method = "RC2 (version needed to extract >= 5.2)";
          break;
        case 0x6801:
          method = "RC4";
          break;
        case 0x6720:
          method = "Blowfish";
          break;
        case 0x6721:
          method = "Twofish";
          break;
        default:
          method = "Unknown";
      }
      printf("      encrypted:  %s (0x%04X)\n", method, stat.encryption_method);
    }

    {
      int const n = zip_file_extra_fields_count(z, i, ZIP_FL_CENTRAL);
      for (int j = 0; j < n; ++j) {
        zip_uint16_t id, len;
        const auto* const field =
            zip_file_extra_field_get(z, i, j, &id, &len, ZIP_FL_CENTRAL);
        printf("  Central x%04X len=%d ", id, len);
        dump_extrafld(FieldId(id), Bytes(field, len), static_cast<mode_t>(unix_mode));
      }
    }

    {
      int const n = zip_file_extra_fields_count(z, i, ZIP_FL_LOCAL);
      for (int j = 0; j < n; ++j) {
        zip_uint16_t id, len;
        const auto* const field =
            zip_file_extra_field_get(z, i, j, &id, &len, ZIP_FL_LOCAL);
        printf("  Local x%04X len=%d ", id, len);
        dump_extrafld(FieldId(id), Bytes(field, len), static_cast<mode_t>(unix_mode));
      }
    }
  }

  zip_close(z);
  return EXIT_SUCCESS;
}

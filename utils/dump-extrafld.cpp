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

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>
#include <memory>
#include <print>

#include <zip.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "lib/extra_field.h"

void PrintTime(const char* label, const timespec& time) {
  if (time.tv_sec == -1) {
    return;
  }
  struct tm tmp;
  if (!localtime_r(&time.tv_sec, &tmp)) {
    perror("localtime");
    return;
  }

  char str1[512];
  if (strftime(str1, sizeof(str1), "%Y-%m-%d %H:%M:%S", &tmp) == 0) {
    std::println(stderr, "strftime returned 0");
    return;
  }

  char str2[16];
  if (strftime(str2, sizeof(str2), "%z", &tmp) == 0) {
    std::println(stderr, "strftime returned 0");
    return;
  }

  std::println("{}{}.{:09} {}", label, str1, time.tv_nsec, str2);
}

void PrintTime(const char* label, time_t time) {
  PrintTime(label, timespec{.tv_sec = time, .tv_nsec = 0});
}

void PrintExtraFields(FieldId id, bool local, Bytes b, mode_t mode) {
  std::print("    ");
  switch (id) {
#define PRINT(s)    \
  case FieldId::s:  \
    std::print(#s); \
    break;
    PRINT(UNIX_TIMESTAMP)
    PRINT(NTFS_TIMESTAMP)
    PRINT(PKWARE_UNIX)
    PRINT(INFOZIP_UNIX_1)
    PRINT(INFOZIP_UNIX_2)
    PRINT(INFOZIP_UNIX_3)
#undef PRINT
    default:
      std::print("Unknown (x{:04X})", static_cast<unsigned int>(id));
  }

  if (local) {
    std::print(" local");
  } else {
    std::print(" central");
  }

  std::print(" len={} ", b.size());
  for (const std::byte c : b) {
    std::print("\\x{:02X}", static_cast<unsigned int>(c));
  }
  std::println();

  ExtraFields f;
  if (!f.Parse(id, b, mode)) {
    std::println("      Cannot parse");
    return;
  }
  PrintTime("      mtime:  ", f.mtime);
  PrintTime("      atime:  ", f.atime);
  PrintTime("      ctime:  ", f.ctime);
  if (f.uid != -1) {
    std::println("      UID:    {}", f.uid);
  }
  if (f.gid != -1) {
    std::println("      GID:    {}", f.gid);
  }
  if (f.dev != -1) {
    std::println("      device: {}, {}", major(f.dev), minor(f.dev));
  }
  if (!f.link_target.empty()) {
    std::println("      link:   {}", f.link_target);
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::println(stderr, "usage: {} <zip-file>", argv[0]);
    return EXIT_FAILURE;
  }

  while (*++argv) {
    struct CloseZip {
      void operator()(zip_t* z) const { zip_close(z); }
    };

    using Zip = std::unique_ptr<zip_t, CloseZip>;
    int err;
    Zip z(zip_open(*argv, 0, &err));
    if (!z) {
      std::println("Cannot open {}", *argv);
      continue;
    }

    std::println("{}", *argv);

    {
      int len = 0;
      const char* const comment =
          zip_get_archive_comment(z.get(), &len, ZIP_FL_ENC_RAW);
      if (comment != nullptr && len > 0)
        std::println("  comment: {}", std::string_view(comment, len));
    }

    zip_int64_t const n = zip_get_num_entries(z.get(), 0);
    for (zip_int64_t i = 0; i < n; ++i) {
      std::println("  {}", zip_get_name(z.get(), i, ZIP_FL_ENC_STRICT));

      zip_uint8_t opsys;
      zip_uint32_t attr;
      zip_file_get_external_attributes(z.get(), i, 0, &opsys, &attr);

      std::print("    op sys:     ");
      switch (opsys) {
        case ZIP_OPSYS_UNIX:
          std::println("UNIX");
          break;
        case ZIP_OPSYS_DOS:
          std::println("DOS");
          break;
        case ZIP_OPSYS_WINDOWS_NTFS:
          std::println("Windows NTFS");
          break;
        case ZIP_OPSYS_MVS:
          std::println("MVS (PKWARE) or Windows NTFS (Info-Zip)");
          break;
        default:
          std::println("Unknown ({})", opsys);
      }

      mode_t const mode = attr >> 16;

      /*
       * PKWARE describes "OS made by" now (since 1998) as follows:
       * The upper byte indicates the compatibility of the file attribute
       * information.  If the external file attributes are compatible with
       * MS-DOS and can be read by PKZIP for DOS version 2.04g then this
       * value will be zero.
       */
      if (opsys == ZIP_OPSYS_DOS && (mode & S_IFMT) != 0) {
        opsys = ZIP_OPSYS_UNIX;
      }

      switch (opsys) {
        case ZIP_OPSYS_UNIX:
          std::print("    type:       ");
          switch (mode & S_IFMT) {
            case S_IFBLK:
              std::println("Block Device");
              break;
            case S_IFCHR:
              std::println("Character Device");
              break;
            case S_IFDIR:
              std::println("Directory");
              break;
            case S_IFIFO:
              std::println("FIFO");
              break;
            case S_IFREG:
              std::println("File");
              break;
            case S_IFSOCK:
              std::println("Socket");
              break;
            case S_IFLNK:
              std::println("Symlink");
              break;
            default:
              std::println("Unknown (x{:0X})", mode & S_IFMT);
          }

          std::println(
              "    mode:       {:03o} {}{}{} {}{}{} {}{}{}", mode,
              (mode & S_IRUSR) ? 'r' : '-', (mode & S_IWUSR) ? 'w' : '-',
              (mode & S_IXUSR) ? ((mode & S_ISUID) ? 's' : 'x')
                               : ((mode & S_ISUID) ? 'S' : '-'),
              (mode & S_IRGRP) ? 'r' : '-', (mode & S_IWGRP) ? 'w' : '-',
              (mode & S_IXGRP) ? ((mode & S_ISGID) ? 's' : 'x')
                               : ((mode & S_ISGID) ? 'S' : '-'),
              (mode & S_IROTH) ? 'r' : '-', (mode & S_IWOTH) ? 'w' : '-',
              (mode & S_IXOTH) ? ((mode & S_ISVTX) ? 't' : 'x')
                               : ((mode & S_ISVTX) ? 'T' : '-'));
          break;

        case ZIP_OPSYS_DOS:
        case ZIP_OPSYS_WINDOWS_NTFS:
        case ZIP_OPSYS_MVS:
          std::println("    attributes: {}{}{}{}", (attr & 0x01) ? 'R' : '-',
                       (attr & 0x20) ? 'A' : '-', (attr & 0x02) ? 'H' : '-',
                       (attr & 0x04) ? 'S' : '-');
          break;
      }

      {
        uint32_t len = 0;
        const char* const comment =
            zip_file_get_comment(z.get(), i, &len, ZIP_FL_ENC_RAW);
        if (comment && len > 0) {
          std::println("    comment:    {}", std::string_view(comment, len));
        }
      }

      zip_stat_t stat;
      if (zip_stat_index(z.get(), i, 0, &stat) != 0) {
        std::println("zip_stat_index failed");
        continue;
      }
      if (stat.valid & ZIP_STAT_SIZE) {
        std::println("    orig.size:  {}", stat.size);
      }
      if (stat.valid & ZIP_STAT_COMP_SIZE) {
        std::println("    comp.size:  {}", stat.comp_size);
      }
      if (stat.valid & ZIP_STAT_MTIME) {
        PrintTime("    mtime:      ", stat.mtime);
      }
      if (stat.valid & ZIP_STAT_CRC) {
        std::println("    CRC:        {:08X}", stat.crc);
      }
      if (stat.valid & ZIP_STAT_COMP_METHOD) {
        std::print("    compressed: ");
        switch (stat.comp_method) {
          case ZIP_CM_STORE:
            std::println("No (stored uncompressed)");
            break;
          case ZIP_CM_SHRINK:
            std::println("Shrink");
            break;
          case ZIP_CM_REDUCE_1:
            std::println("Reduce with factor 1");
            break;
          case ZIP_CM_REDUCE_2:
            std::println("Reduce with factor 2");
            break;
          case ZIP_CM_REDUCE_3:
            std::println("Reduce with factor 3");
            break;
          case ZIP_CM_REDUCE_4:
            std::println("Reduce with factor 4");
            break;
          case ZIP_CM_IMPLODE:
            std::println("Implode");
            break;
          case 7:
            std::println("Tokenizing compression");
            break;
          case ZIP_CM_DEFLATE:
            std::println("Deflate");
            break;
          case ZIP_CM_DEFLATE64:
            std::println("Deflate64");
            break;
          case ZIP_CM_PKWARE_IMPLODE:
            std::println("PKWARE Implode");
            break;
          case ZIP_CM_BZIP2:
            std::println("BZIP2");
            break;
          case ZIP_CM_LZMA:
            std::println("LZMA (EFS)");
            break;
          case ZIP_CM_TERSE:
            std::println("IBM TERSE (new)");
            break;
          case ZIP_CM_LZ77:
            std::println("IBM LZ77 z Architecture (PFS)");
            break;
          case ZIP_CM_XZ:
            std::println("XZ");
            break;
          case ZIP_CM_JPEG:
            std::println("JPEG");
            break;
          case ZIP_CM_WAVPACK:
            std::println("WavPack");
            break;
          case ZIP_CM_PPMD:
            std::println("PPMd version I, Rev 1");
            break;
          default:
            std::println("Unknown ({})", stat.comp_method);
        }
      }

      if (stat.valid & ZIP_STAT_ENCRYPTION_METHOD) {
        std::print("    encrypted:  ");
        switch (stat.encryption_method) {
          case ZIP_EM_NONE:
            std::println("No");
            break;
          case ZIP_EM_TRAD_PKWARE:
            std::println("PKWARE ZipCrypto");
            break;
          case ZIP_EM_AES_128:
            std::println("AES 128");
            break;
          case ZIP_EM_AES_192:
            std::println("AES 192");
            break;
          case ZIP_EM_AES_256:
            std::println("AES 256");
            break;
          case 0x6601:
            std::println("DES");
            break;
          case 0x6609:
            std::println("3DES 112");
            break;
          case 0x6603:
            std::println("3DES 168");
            break;
          case 0x660E:
            std::println("PKZIP AES 128");
            break;
          case 0x660F:
            std::println("PKZIP AES 192");
            break;
          case 0x6610:
            std::println("PKZIP AES 256");
            break;
          case 0x6602:
            std::println("RC2 (version needed to extract < 5.2)");
            break;
          case 0x6702:
            std::println("RC2 (version needed to extract >= 5.2)");
            break;
          case 0x6801:
            std::println("RC4");
            break;
          case 0x6720:
            std::println("Blowfish");
            break;
          case 0x6721:
            std::println("Twofish");
            break;
          default:
            std::println("Unknown (x{:04x})", stat.encryption_method);
        }
      }

      for (bool const local : {false, true}) {
        zip_flags_t const flags = local ? ZIP_FL_LOCAL : ZIP_FL_CENTRAL;
        int const n = zip_file_extra_fields_count(z.get(), i, flags);
        for (int j = 0; j < n; ++j) {
          zip_uint16_t id, len;
          const auto* const data =
              zip_file_extra_field_get(z.get(), i, j, &id, &len, flags);
          PrintExtraFields(FieldId(id), local, Bytes(data, len), mode);
        }
      }
    }
  }

  return EXIT_SUCCESS;
}

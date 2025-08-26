// Copyright 2021 Google LLC
// Copyright 2014-2019 Alexander Galanin <al@galanin.nnov.ru>
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

#ifndef EXTRA_FIELD_H
#define EXTRA_FIELD_H

#include <ctime>
#include <span>

#include <sys/types.h>

// ZIP extra fields
enum FieldId {
  FZ_EF_TIMESTAMP = 0x5455,
  FZ_EF_NTFS = 0x000A,
  FZ_EF_PKWARE_UNIX = 0x000D,
  FZ_EF_INFOZIP_UNIX1 = 0x5855,
  FZ_EF_INFOZIP_UNIX2 = 0x7855,
  FZ_EF_INFOZIP_UNIXN = 0x7875,
};

struct Bytes : std::span<const std::byte> {
  using Base = std::span<const std::byte>;
  using Base::operator=;
  using Base::Base;

  template <typename T, size_t N>
  Bytes(const T (&x)[N]) : Base(std::as_bytes(std::span(x))) {}

  template <typename T>
  Bytes(const T* const p, size_t n) : Base(std::as_bytes(std::span(p, n))) {}
};

// 'Extended Timestamp' LOCAL extra field (0x5455).
struct ExtTimeStamp {
  time_t mtime = 0;
  time_t atime = 0;
  time_t ctime = 0;

  bool Parse(Bytes b);
};

// Info-ZIP UNIX extra field (5855) with timestamps and (maybe) UID/GID.
struct SimpleUnixField {
  time_t mtime = 0;
  time_t atime = 0;
  uid_t uid = -1;
  gid_t gid = -1;

  bool Parse(FieldId id, Bytes b);
};

struct ExtraField {
  /**
   * Parse Info-ZIP UNIX extra field (5855) to extract UID/GID and (maybe)
   * timestamps.
   *
   * @param type extended field type ID
   * @param b field data
   * @param hasUidGid (OUT) UID and GID are present
   * @param uid (OUT) UID
   * @param gid (OUT) GID
   * @param mtime (OUT) file modification time if present
   * @param atime (OUT) file access time if present
   * @return successful completion flag
   */
  static bool parseSimpleUnixField(int type,
                                   Bytes b,
                                   bool& hasUid,
                                   uid_t& uid,
                                   gid_t& gid,
                                   time_t& mtime,
                                   time_t& atime);

  /**
   * Parse UNIX extra field to extract UID/GID:
   *  7855    Info-ZIP Unix Extra Field (type 2)
   *  7875    Info-ZIP New Unix Extra Field
   *
   * @param type extended field type ID
   * @param b field data
   * @param uid (OUT) UID
   * @param gid (OUT) GID
   * @return successful completion flag
   */
  static bool parseUnixUidGidField(int type, Bytes b, uid_t& uid, gid_t& gid);

  /**
   * Parse PKWARE Unix Extra Field (000D). If file is a device (character or
   * block) then device inor-major numbers are extracted into 'dev' parameter.
   * For symbolic links and regular files link target pointer is extracted into
   * 'link_target' field and 'link_target_len' variables.
   *
   * @param type extended field type ID
   * @param b field data
   * @param mode UNIX file mode
   * @param mtime (OUT) file modification time if present
   * @param atime (OUT) file access time if present
   * @param uid (OUT) UID
   * @param gid (OUT) GID
   * @param dev (OUT) device major/minor numbers
   * @param link_target (OUT) pointer to a first byte of hard/symbolic link
   * target
   * @param link_target_len (OUT) length of hard/symbolic link target
   * @return successful completion flag
   */
  static bool parsePkWareUnixField(Bytes b,
                                   mode_t mode,
                                   time_t& mtime,
                                   time_t& atime,
                                   uid_t& uid,
                                   gid_t& gid,
                                   dev_t& dev,
                                   const char*& link_target,
                                   size_t& link_target_len);

  /**
   * Parse NTFS Extra FIeld
   *
   * @param b field data
   * @param mtime (OUT) file modification time if present
   * @param atime (OUT) file access time if present
   * @param ctime (OUT) file creation time if present
   * @return successful completion flag
   */
  static bool parseNtfsExtraField(Bytes b,
                                  timespec& mtime,
                                  timespec& atime,
                                  timespec& ctime);
};

#endif

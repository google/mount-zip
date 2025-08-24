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

#include <cstdint>

#include <zip.h>

// ZIP extra fields
#define FZ_EF_TIMESTAMP (0x5455)
#define FZ_EF_NTFS (0x000A)
#define FZ_EF_PKWARE_UNIX (0x000D)
#define FZ_EF_INFOZIP_UNIX1 (0x5855)
#define FZ_EF_INFOZIP_UNIX2 (0x7855)
#define FZ_EF_INFOZIP_UNIXN (0x7875)

#define FZ_EF_NTFS_TIMESTAMP_LENGTH (28U)

struct ExtraField {
  /**
   * Parse 'Extended Timestamp' LOCAL extra field (0x5455) to get mtime,
   * atime and creation time values.
   * @param len (IN) field length in bytes
   * @param data (IN) field data
   * @param hasMTime (OUT) mtime presence
   * @param mtime (OUT) file modification time if present
   * @param hasATime (OUT) atime presence
   * @param atime (OUT) file access time if present
   * @param hasCreTime (OUT) creation time presence
   * @param cretime (OUT) file creation time if present
   * @return successful completion flag
   */
  static bool parseExtTimeStamp(zip_uint16_t len,
                                const zip_uint8_t* data,
                                bool& hasMTime,
                                time_t& mtime,
                                bool& hasATime,
                                time_t& atime,
                                bool& hasCreTime,
                                time_t& cretime);

  /**
   * Parse Info-ZIP UNIX extra field (5855) to extract UID/GID and (maybe)
   * timestamps.
   *
   * @param type extended field type ID
   * @param len field length in bytes
   * @param data field data
   * @param hasUidGid (OUT) UID and GID are present
   * @param uid (OUT) UID
   * @param gid (OUT) GID
   * @param mtime (OUT) file modification time if present
   * @param atime (OUT) file access time if present
   * @return successful completion flag
   */
  static bool parseSimpleUnixField(zip_uint16_t type,
                                   zip_uint16_t len,
                                   const zip_uint8_t* data,
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
   * @param len field length in bytes
   * @param data field data
   * @param uid (OUT) UID
   * @param gid (OUT) GID
   * @return successful completion flag
   */
  static bool parseUnixUidGidField(zip_uint16_t type,
                                   zip_uint16_t len,
                                   const zip_uint8_t* data,
                                   uid_t& uid,
                                   gid_t& gid);

  /**
   * Parse PKWARE Unix Extra Field (000D). If file is a device (character or
   * block) then device inor-major numbers are extracted into 'dev' parameter.
   * For symbolic links and regular files link target pointer is extracted into
   * 'link_target' field and 'link_target_len' variables.
   *
   * @param type extended field type ID
   * @param len field length in bytes
   * @param data field data
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
  static bool parsePkWareUnixField(zip_uint16_t len,
                                   const zip_uint8_t* data,
                                   mode_t mode,
                                   time_t& mtime,
                                   time_t& atime,
                                   uid_t& uid,
                                   gid_t& gid,
                                   dev_t& dev,
                                   const char*& link_target,
                                   zip_uint16_t& link_target_len);

  /**
   * Parse NTFS Extra FIeld
   *
   * @param len field length in bytes
   * @param data field data
   * @param mtime (OUT) file modification time if present
   * @param atime (OUT) file access time if present
   * @param cretime (OUT) file creation time if present
   * @return successful completion flag
   */
  static bool parseNtfsExtraField(zip_uint16_t len,
                                  const zip_uint8_t* data,
                                  struct timespec& mtime,
                                  struct timespec& atime,
                                  struct timespec& cretime);
};

#endif

# ABOUT #

fuse-zip is a FUSE file system to navigate, extract, create and modify ZIP and
ZIP64 archives based on libzip implemented in C++.

With fuse-zip you really can work with ZIP archives as real directories.
Unlike KIO or Gnome VFS, it can be used in any application without
modifications.

* File system features:
    * Read-only and read-write modes.
    * Transparent packing and unpacking.
    * File types:
        * regular files, sockets, symbolic links, block and character devices, FIFOs;
        * hard links (read-only mode).
    * Create/edit/rename/delete files and directories.
    * Read/modify/save file access modes and owner/group information.
    * Sparse files.
    * Relative and absolute paths (read-only mode).
    * File name encoding conversion on the fly (using iconv module).
    * Access to file and archive comments via extended attribute "user.comment".
    * File creation/modification/access/change times with nanosecond resolution.
    * Time stamp precision:
        * at least 1 s resolution for all files;
        * 100 ns resolution for all created files or files with NTFS extra field (see also force\_precise\_time option);
        * Note: high-resolution time stamps for symbolic links, block and character devices are saved in local directory only.
* ZIP file format features:
    * ZIP64: support for more than 65535 files in archive, support for files bigger than 4 Gb.
    * Compression methods: store (no compression), deflate, bzip2.
    * Time fields: creation, modify and access time.
    * UNIX permissions: variable-length UID and GID, file access modes.
    * DOS file permissions.
    * File and archive comments.
* Supported ZIP format extensions:
    * 000A PKWARE NTFS Extra Field - high-precision timestamps;
    * 000D PKWARE UNIX Extra Field:
        * regular files, sockets, symbolic links, block and character devices, FIFOs;
        * hard links (read-only mode);
    * 5455 extended timestamp;
    * 5855 Info-ZIP UNIX extra field (type 1);
    * 7855 Info-ZIP Unix Extra Field (type 2);
    * 7875 Info-ZIP New Unix Extra Field - variable-length UIDs and GIDs.

Since version 0.3.0 fuse-zip has support for absolute and parent-relative paths
in file names, but only in read-only mode (-r command line switch). Absolute
paths are displayed under "ROOT" directory, every ".." in path replaced by "UP"
in directory name and "normal" files are placed under "CUR" directory.

File/archive comments are supported since version 0.7.0. To read/modify file
comment use extended attribute "user.comment". Archive comment is accessible
via mount point's extended attribute.

High-precision time stamps (with 100 ns resolution) are supported since version
0.7.0. By default high precision timestamp is saved to archive only for new
files or files with known creation time. You can force high-precision time
saving by specifying '-o force\_precise\_time' option. In this case creation time
will be equal to a first known file modification time.

Release 0.7.0 adds read and write support for all UNIX special file types:
symbolic links, block and character devices, sockets and FIFOs. Hard links are
supported in read-only mode. Note that block/character devices, sockets and FIFOs
are not supported by Info-Zip tools such as unzip.

Unlike other FUSE filesystems, _only_ fuse-zip provides write support to ZIP
archives. Also, fuse-zip is faster than all known implementations on large
archives with many files.

You can download fuse-zip at https://bitbucket.org/agalanin/fuse-zip

Repository mirror: http://galanin.nnov.ru/hg/fuse-zip

# AUTHOR #

Alexander Galanin

  * E-mail:     al@galanin.nnov.ru
  * Home page:  http://galanin.nnov.ru/~al/

# LICENSE #

fuse-zip are licensed under GNU GPL v3 or later.

# USAGE #

```
$ mkdir /tmp/zipArchive
$ fuse-zip foobar.zip /tmp/zipArchive
(do something with the mounted file system)
$ fusermount -u /tmp/zipArchive
```

If ZIP file does not exists, it will be created after filesystem unmounting.

Be patient. Wait for fuse-zip process finish after unmounting especially on
a big archives.

If you want to specify character set conversion for file names in archive,
use the following fusermount options:

    -omodules=iconv,from_code=$charset1,to_code=$charset2

Those Russian who uses archives from the "other OS" should use CP866 as
'charset1' and locale charset as 'charset2'.

See FUSE documentation for details.

Look at /var/log/user.log in case of any errors.

# PERFORMANCE #

On a small archives fuse-zip have the same performance with commonly used
virtual filesystems like KIO, Gnome GVFS, mc vfs, unpackfs, avfs, fuse-j-zip.
But on large archives with many file (like zipped Linux kernel sources)
fuse-zip have the greatest speed.
You can download test suite from the web-site and make sure that it is true.

# PERMISSIONS #

Support for UNIX file permissions and owner information has been added in
version 0.4. Note that access check will not be performed unless
'-o default\_permissions' mount option is given.

# HINTS #

* Added/changed files resides into memory until you unmount file system.
* Adding/unpacking very big files(more than one half of available memory) may
  cause your system swapping. It is a good idea to use zip/unzip in that case
  :)
* After adding/modifying files in archive it will be repacked at filesystem
  unmount. Hence, your file system must have enough space to keep temporary
  files.
* Wait until fuse-zip process is finished after unmount before using archive
  file.

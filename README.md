---
title: MOUNT-ZIP
section: 1
header: User Manual
footer: mount-zip 1.17
date: August 2026
---
# ⚠️ REPOSITORY ARCHIVED & TRANSITIONED

**This repository is no longer actively maintained by Google and has been officially archived.**

The primary maintainer of `mount-zip` is continuing active development,
maintenance, and community support under a new name in his personal repository:

👉 **[github.com/fdegros/zipmount](https://github.com/fdegros/zipmount)**

## What does this mean for you?

* **Code & History:** The historical releases, issues, and code remain here for
  reference.
* **New Name & Active Home:** Active development is continuing at
  [fdegros/zipmount](https://github.com/fdegros/zipmount).
* **New Features & Bug Fixes:** All new development, dependencies uprevs, and
  security hardening will occur exclusively in the new repository.
* **Contributions:** Please submit all future bug reports, feature requests, and
  pull requests to the new
  [fdegros/zipmount](https://github.com/fdegros/zipmount) home.

Thank you to everyone who has starred, used, or contributed to this project
under the Google organization! We encourage you to head over, star, and follow
the new repository to keep up with active updates.

# NAME

**mount-zip** - Mount ZIP archives as FUSE file systems.

# SYNOPSIS

*   **mount-zip** [*options*] *zip-file*
*   **mount-zip** [*options*] *zip-file* *mount-point*
*   **mount-zip** [*options*] *zip-file-1* *zip-file-2* ... *mount-point*

# DESCRIPTION

**mount-zip** mounts one or several ZIP archives as a read-only
[FUSE file system](https://en.wikipedia.org/wiki/Filesystem_in_Userspace). It
starts quickly, uses little memory, decodes encrypted files, and provides
on-the-go decompression and caching for maximum efficiency.

**mount-zip** automatically creates the target mount point if needed. If no
mount point is specified, **mount-zip** creates a mount point in the current
working directory.

# OPTIONS

**-\-help** or **-h**
:   Print help.

**-\-version** or **-V**
:   Print program version.

**-o quiet** or **-q**
:   Print fewer log messages.

**-o verbose** or **-v**
:   Print more detailed log messages.

**-o redact**
:   Redact file names from log messages.

**-o force**
:   Continue even if the given password is wrong or missing, or if the
    encryption or compression method is unsupported.

**-o precache**
:   Preemptively decompress and cache the whole ZIP archive(s).

**-o cache=DIR**
:   Use a different cache directory (default is `$TMPDIR` or `/tmp`).

**-o memcache**
:   Cache the decompressed data in memory.

**-o nocache**
:   Do not cache the decompressed data.

**-o encoding=CHARSET**
:   Original encoding of file names.

**-o nomerge**
:   Do not merge multiple ZIP archives on top of each other. Instead, create a
    subdirectory for each ZIP archive inside the mount point.

**-o notrim**
:   Do not trim the base of the tree. Keep all the intermediate directories as
    specified in the ZIP archive(s).

**-o nodirs**
:   No directories. Flatten the archive structure by presenting all its files in
    its root directory. This might make sense if you're not interested in the
    archive tree structure, but only in its file data, e.g. for malware scanning.

**-o nospecials**
:   Hide special files (FIFOs, sockets, devices).

**-o nosymlinks**
:   Hide symbolic links.

**-o nohardlinks**
:   Hide hard links.

**-o dmask=M**
:   Directory permission mask in octal (default 0022).

**-o fmask=M**
:   File permission mask in octal (default 0022).

**-o uid=N**
:   Set the user ID of all the items in the mounted archive (default is current
    user).

**-o gid=N**
:   Set the group ID of all the items in the mounted archive (default is current
    group).

**-o enforce_permissions**
:   Use the user ID, group ID and permissions stored with each item in the
    archive.

**-f**
:   Foreground mode.

**-d**
:   Foreground mode with debug output.

# USAGE EXAMPLES

Mount a ZIP archive:

```
$ mount-zip foobar.zip mnt
```

The mounted ZIP archive can be explored and read using any application:

```
$ tree mnt
mnt
└── foo

0 directories, 1 file

$ cat mnt/foo
bar
```

When finished, unmount the file system:

```
$ umount mnt
```

# FEATURES

*   Read-only view
*   Instant mounting, even with big ZIP archives
*   Compression methods: deflate, bzip2
*   Encryption methods: AES and legacy ZIP encryption
*   Asks for decryption password if necessary
*   Detects file name encoding
*   Converts file names to Unicode UTF-8
*   Deduplicates files in case of name collisions
*   Unpacks files when reading them (on-the-go decompression)
*   Supports all file types, including named sockets, FIFOs, block and character
    devices, symbolic links and hard links
*   Supports Unix access modes and DOS file permissions
*   Supports owner and group information (UID and GID)
*   Supports relative and absolute paths
*   Supports high-precision timestamps
*   Works on 32-bit and 64-bit devices
*   Supports ZIP64 extensions, even on 32-bit devices:
    *   Supports ZIP archives containing more than 65,535 files
    *   Supports ZIP archives and files bigger than 4 GB
*   Supports the following ZIP format extensions:
    *   000A PKWARE NTFS high-precision timestamps
    *   000D PKWARE Unix file type
    *   5455 Unix timestamps
    *   5855 Info-ZIP Unix extra fields (type 1)
    *   7855 Info-ZIP Unix extra fields (type 2)
    *   7875 Info-ZIP Unix extra fields (type 3): UID and GID

# FILE NAME ENCODING

**mount-zip** is fully Unicode compliant. It converts the file names stored in
the ZIP archive from their original encoding to UTF-8.

In order to interpret these file names correctly, **mount-zip** needs to
determine their original encoding. By default **mount-zip** tries to guess this
encoding using the detection feature provided by the ICU library. It can
automatically recognize the following encodings:

*   UTF-8
*   CP437
*   Shift JIS
*   Big5
*   EUC-JP
*   EUC-KR
*   GB18030
*   ISO-2022-CN
*   ISO-2022-JP
*   ISO-2022-KR
*   KOI8-R

For example, when mounting a ZIP containing a Shift JIS-encoded file name, the
encoding is correctly detected:

```
$ mount-zip sjis-filename.zip mnt

$ tree mnt
mnt
└── 新しいテキスト ドキュメント.txt

0 directories, 1 file
```

This system is not foolproof, and doesn't recognize a number of popular
encodings. For example, when mounting a ZIP containing file names encoded in
CP866, they are interpreted as CP437 and rendered as
[Mojibake](https://en.wikipedia.org/wiki/Mojibake):

```
$ mount-zip cp866.zip mnt

$ tree mnt
mnt
├── äáΓá
└── ÆÑ¬ßΓ«óδ⌐ ñ«¬π¼Ñ¡Γ.txt

0 directories, 2 files
```

In this case, the user needs to explicitly specify the original file name
encoding using the `-o encoding` mount option:

```
$ mount-zip -o encoding=cp866 cp866.zip mnt

$ tree mnt
mnt
├── Дата
└── Текстовый документ.txt

0 directories, 2 files
```

# NAME DEDUPLICATION

In case of name collision, **mount-zip** adds a number to deduplicate the
conflicting file name:

```
$ unzip -l file-dir-same-name.zip
  Length      Date    Time    Name
---------  ---------- -----   ----
       25  2021-10-29 14:22   pet/cat
       21  2021-10-29 14:22   pet
       30  2021-10-29 14:22   pet/cat/fish
        0  2021-10-29 14:22   pet/cat/fish/
       26  2021-10-29 14:22   pet/cat
       22  2021-10-29 14:22   pet
       31  2021-10-29 14:22   pet/cat/fish
---------                     -------
      155                     7 files

$ mount-zip file-dir-same-name.zip mnt

$ tree -F mnt
mnt
├── pet/
│   ├── cat/
│   │   ├── fish/
│   │   ├── fish (1)
│   │   └── fish (2)
│   ├── cat (1)
│   └── cat (2)
├── pet (1)
└── pet (2)

3 directories, 6 files
```

Directories are never renamed. If a file is colliding with a directory, the file
will be the one getting renamed.

# ENCRYPTED ARCHIVES

**mount-zip** supports encrypted ZIP archives. It understands the legacy ZIP
encryption scheme, as well as the more recent AES encryption schemes (AES-128,
AES-192 and AES-256).

When **mount-zip** finds an encrypted file while mounting a ZIP archive, it asks
for a password. If the given password does not decrypt the file, then
**mount-zip** refuses to mount the ZIP archive and returns an error:

```
$ unzip -l different-encryptions.zip
Archive:  different-encryptions.zip
  Length      Date    Time    Name
---------  ---------- -----   ----
       23  2020-08-28 15:22   ClearText.txt
       32  2020-08-28 15:23   Encrypted AES-128.txt
       32  2020-08-28 15:23   Encrypted AES-192.txt
       32  2020-08-28 15:23   Encrypted AES-256.txt
       34  2020-08-28 15:23   Encrypted ZipCrypto.txt
---------                     -------
      153                     5 files

$ mount-zip different-encryptions.zip mnt
Need password for File [1] '/Encrypted AES-128.txt'
Password > Got it!
Use the -o force option to mount an encrypted ZIP with a wrong password
Cannot open File [1] '/Encrypted AES-128.txt': Wrong password provided
```

Providing the correct password allows **mount-zip** to mount the ZIP archive and
decode the files:

```
$ mount-zip different-encryptions.zip mnt
Need password for File [1] '/Encrypted AES-128.txt'
Password > Got it!
Password is Ok

$ tree mnt
mnt
├── ClearText.txt
├── Encrypted AES-128.txt
├── Encrypted AES-192.txt
├── Encrypted AES-256.txt
└── Encrypted ZipCrypto.txt

0 directories, 5 files

$ md5sum mnt/*
7a542815e2c51837b3d8a8b2ebf36490  mnt/ClearText.txt
07c4edd2a55c9d5614457a21fb40aa56  mnt/Encrypted AES-128.txt
e48d57930ef96ff2ad45867202d3250d  mnt/Encrypted AES-192.txt
ca5e064a0835d186f2f6326f88a7078f  mnt/Encrypted AES-256.txt
275e8c5aed7e7ce2f32dd1e5e9ee4a5b  mnt/Encrypted ZipCrypto.txt

$ cat mnt/*
This is not encrypted.
This is encrypted with AES-128.
This is encrypted with AES-192.
This is encrypted with AES-256.
This is encrypted with ZipCrypto.
```

You can force **mount-zip** to mount an encrypted ZIP even without providing the
right password by using the `-o force` option:

```
$ mount-zip -o force different-encryptions.zip mnt
Need password for File [1] '/Encrypted AES-128.txt'
Password > Got it!
Continuing despite wrong password because of -o force option
```

In this case, the files can be listed, but trying to open an encrypted file for
which the given password doesn't work results in an I/O error:

```
$ tree mnt
mnt
├── ClearText.txt
├── Encrypted AES-128.txt
├── Encrypted AES-192.txt
├── Encrypted AES-256.txt
└── Encrypted ZipCrypto.txt

0 directories, 5 files

$ md5sum mnt/*
7a542815e2c51837b3d8a8b2ebf36490  mnt/ClearText.txt
md5sum: 'mnt/Encrypted AES-128.txt': Input/Output error
md5sum: 'mnt/Encrypted AES-192.txt': Input/Output error
md5sum: 'mnt/Encrypted AES-256.txt': Input/Output error
md5sum: 'mnt/Encrypted ZipCrypto.txt': Input/Output error

$ cat mnt/*
This is not encrypted.
cat: 'mnt/Encrypted AES-128.txt': Input/Output error
cat: 'mnt/Encrypted AES-192.txt': Input/Output error
cat: 'mnt/Encrypted AES-256.txt': Input/Output error
cat: 'mnt/Encrypted ZipCrypto.txt': Input/Output error
```

For security reasons, **mount-zip** doesn't allow the password to be specified
on the command line. However, it is possible to pipe the password to
**mount-zip**'s standard input:

```
$ echo password | mount-zip different-encryptions.zip mnt
Need password for File [1] '/Encrypted AES-128.txt'
Password is Ok
```

# FILE TYPES

## Symbolic links

**mount-zip** shows symbolic links recorded in the ZIP archive:

```
$ mount-zip symlink.zip mnt

$ tree mnt
mnt
├── date
└── symlink -> ../tmp/date
```

Note that symbolic links can refer to files located outside the mounted ZIP
archive. In some circumstances, these links could pose a security risk.

Symbolic links can be suppressed with the `-o nosymlinks` option:

```
$ mount-zip -o nosymlinks symlink.zip mnt
Skipped Symlink [1] '/symlink'

2021-10-28 20:05:01 laptop ~/mount-zip/tests/blackbox/data (intrusive)
$ tree mnt
mnt
└── date

0 directories, 1 file
```

## Special Files

**mount-zip** shows special files (sockets, FIFOs or pipes, character and block
devices) recorded in the ZIP archive:

```
$ mount-zip -o enforce_permissions pkware-specials.zip mnt

$ ls -n mnt
brw-rw---- 1    0    6 8, 1 Aug  3  2019 block
crw--w---- 1    0    5 4, 0 Aug  3  2019 char
prw-r--r-- 1 1000 1000    0 Aug 15  2019 fifo
-rw-r--r-- 3 1000 1000   32 Aug  9  2019 regular
srw------- 1 1000 1000    0 Aug  3  2019 socket
lrwxrwxrwx 1 1000 1000    7 Aug  3  2019 symlink -> regular
lrwxrwxrwx 1 1000 1000    7 Aug 25  2019 symlink2 -> regular
-rw-r--r-- 3 1000 1000   32 Aug  9  2019 z-hardlink1
-rw-r--r-- 3 1000 1000   32 Aug  9  2019 z-hardlink2
brw-rw---- 1    0    6 8, 1 Aug  3  2019 z-hardlink-block
crw--w---- 1    0    5 4, 0 Aug  3  2019 z-hardlink-char
prw-r--r-- 1 1000 1000    0 Aug 15  2019 z-hardlink-fifo
srw------- 1 1000 1000    0 Aug  3  2019 z-hardlink-socket
lrwxrwxrwx 1 1000 1000    7 Aug  3  2019 z-hardlink-symlink -> regular
```

Special files can be suppressed with the `-o nospecials` option.

## Hard Links

**mount-zip** shows hard links recorded in the ZIP archive.

In this example, the three file entries `0regular`, `hlink1` and `hlink2` point
to the same inode number (2) and their reference count is 3:

```
$ mount-zip hlink-chain.zip mnt

$ ls -ni mnt
2 -rw-r--r-- 3 0 0 10 Aug 14  2019 0regular
2 -rw-r--r-- 3 0 0 10 Aug 14  2019 hlink1
2 -rw-r--r-- 3 0 0 10 Aug 14  2019 hlink2

$ md5sum mnt/*
e09c80c42fda55f9d992e59ca6b3307d  mnt/0regular
e09c80c42fda55f9d992e59ca6b3307d  mnt/hlink1
e09c80c42fda55f9d992e59ca6b3307d  mnt/hlink2
```

Some tools can use the inode number to detect duplicated hard links. In this
example, `du` only counts the size of the inode (2) once, even though there are
three file entries pointing to it, and only reports 10 bytes instead of 30
bytes:

```
$ du -b mnt
10      mnt
```

Duplicated hard links can be suppressed with the `-o nohardlinks` option.

# CACHING

**mount-zip** only does the minimum amount of work required to serve the
requested data. It offers several caching strategies and storage options to
balance performance, mount time, and resource usage.

## Caching Strategies

The following strategies are mutually exclusive. If none is specified, lazy
decompression is used by default.

### Lazy Decompression (Default)

By default, **mount-zip** only decompresses enough data to serve the reading
application. Accessing the beginning of a big compressed file is therefore
instantaneous. **mount-zip** will start caching a file if it detects that this
file is getting read in a non-sequential way.

### Pre-emptive Caching (`-o precache`)

The `-o precache` option instructs **mount-zip** to preemptively decompress and
cache the whole ZIP archive(s) at mount time. The cost of decompression is
incurred upfront, ensuring that any subsequent access to the served data is fast
and supports efficient random access.

### No Caching (`-o nocache`)

The `-o nocache` option disables long-term caching. **mount-zip** will use a
small rolling buffer in memory to serve data. This minimizes disk and memory
usage but can be significantly slower for non-sequential access patterns or if
the same files are read repeatedly, as data may need to be re-decompressed.

## Storage Options

### Disk Caching (Default)

By default, decompressed data is cached in an anonymous temporary file (in
`$TMPDIR` or `/tmp`). This cache can use a significant amount of disk space but
is automatically deleted when the ZIP is unmounted. The cache directory can be
changed with the `-o cache=DIR` option.

### Memory Caching (`-o memcache`)

The `-o memcache` option instructs **mount-zip** to store the cache in RAM
instead of a temporary file. This provides the highest performance but can
consume a large amount of memory. It can be used with both lazy and pre-emptive
caching strategies.

# ADVANCED OPTIONS

## Handling Errors (`-o force`)

The `-o force` option allows **mount-zip** to continue mounting a ZIP archive
even if some errors are encountered. This includes:

*   Wrong or missing decryption passwords.
*   Unsupported compression or encryption methods.

In these cases, files that cannot be correctly processed will still be listed in
the directory tree, but attempting to read them will result in an Input/Output
error (EIO).

## Permissions and Ownership

By default, **mount-zip** presents all files as being owned by the current user
and group, with standard read permissions.

### User and Group IDs (`-o uid=N`, `-o gid=N`)

You can explicitly set the user and group of all items in the mounted archive
using the `-o uid` and `-o gid` options with the desired numerical IDs.

### Using Archive Permissions (`-o enforce_permissions`)

The `-o enforce_permissions` option instructs **mount-zip** to use the exact
UID, GID, and Unix permission bits stored within the archive for each item. This
includes support for special bits such as **SUID**, **SGID**, and **SVTX**
(sticky bit). Note that this may result in access denied errors if you are not
the owner of the files inside the archive.

### Permission Masks (`-o dmask=M`, `-o fmask=M`)

You can apply an octal permission mask to directories (`dmask`) and files
(`fmask`). For example, `-o fmask=077` would remove all permissions for group
and others from files.

# PERFORMANCE

**mount-zip** works well with large archives containing many files. For example
on my laptop, a ZIP archive containing more than 70,000 files is mounted in half
a second:

```
$ ls -lh linux-5.14.15.zip
-rw-r--r-- 1 fdegros primarygroup 231M Oct 28 15:48 linux-5.14.15.zip

$ time mount-zip linux-5.14.15.zip mnt

real    0m0.561s
user    0m0.344s
sys     0m0.212s

$ tree mnt
mnt
└── linux-5.14.15
    ├── arch
...

4817 directories, 72539 files

$ du -sh mnt
1.1G    mnt
```

The full contents of this mounted ZIP, totaling 1.1 GB, can be extracted with
`cp -R` in 14 seconds:

```
$ time cp -R mnt out

real    0m13.810s
user    0m0.605s
sys     0m5.356s
```

For comparison, `unzip` extracts the contents of the same ZIP in 8.5 seconds:

```
$ time unzip -q -d out linux-5.14.15.zip

real    0m8.411s
user    0m6.067s
sys     0m2.270s
```

Mounting an 8-GB ZIP containing only a few files is instantaneous:

```
$ ls -lh bru.zip
-rw-r----- 1 fdegros primarygroup 7.9G Sep  2 22:37 bru.zip

$ time mount-zip bru.zip mnt

real    0m0.033s
user    0m0.018s
sys     0m0.011s

$ tree -h mnt
mnt
├── [2.0M]  bios
├── [ 25G]  disk
└── [ 64M]  tools

0 directories, 3 files
```

Decompressing and reading the 25-GB file from this mounted ZIP takes less than
two minutes:

```
$ dd if=mnt/disk of=/dev/null status=progress
26843545600 bytes (27 GB, 25 GiB) copied, 104.586 s, 257 MB/s
```

There is no lag when opening and reading the file, and only a moderate amount of
memory is used. The file is getting lazily decompressed by **mount-zip** as it
is getting read by the `dd` program.

# LOG MESSAGES

**mount-zip** records log messages to the system logger (**syslog**). These
messages can help troubleshoot issues, such as I/O errors or archives that
refuse to mount.

To read **mount-zip**'s log messages on most Linux systems:

```
$ journalctl -t mount-zip
```

Alternatively, you can run **mount-zip** in the foreground with the `-f`
option to see log messages directly on your terminal.

By default, **mount-zip** writes **INFO** and **ERROR** messages. You can
decrease the logging level to just **ERROR** messages with the `-o quiet`
option, or increase it to include **DEBUG** messages with the `-o verbose`
option.

```
$ mount-zip -f -o verbose foobar.zip mnt
Indexing 'foobar.zip'...
Allocating 16 buckets
Detected encoding UTF-8 with 15% confidence
Indexed 'foobar.zip' in 0 ms
Mounted 'foobar.zip' on 'mnt' in 2 ms
Reader 1: Opened File [0]
Reader 1: Closed
Unmounting 'foobar.zip' from 'mnt'...
Unmounted 'foobar.zip' in 0 ms
```

To prevent file names from being recorded in **mount-zip**'s log messages, use
the `-o redact` option:

```
$ mount-zip -f -o verbose -o redact bad-crc.zip mnt
Indexing (redacted)...
Allocating 16 buckets
Indexed (redacted) in 0 ms
Mounted (redacted) on (redacted) in 2 ms
Reader 1: Opened File [0]
Cannot read (redacted): Cannot read file: CRC error
Reader 1: Closed
Unmounting (redacted) from (redacted)...
Unmounted (redacted) in 0 ms
```

# RETURN VALUE

**mount-zip** returns distinct error codes for different error conditions
related to the ZIP archives themselves:

**0**
:   Success.

**1**
:   Generic error code for: missing argument, unknown option, unknown file name
    encoding, mount point cannot be created, mount point is not empty, etc.

**11**
:   An archive is a multipart ZIP.

**15**
:   A ZIP archive cannot be read.

**19**
:   A ZIP archive cannot be found.

**21**
:   A ZIP archive cannot be opened.

**23**
:   Decompression error. This is probably the sign of a wrong password. Use
    `-o force` to bypass the password verification.

**26**
:   Unsupported compression method. Use `-o force` to bypass the compression
    method verification.

**29**
:   An archive is not recognized as a valid ZIP.

**31**
:   A ZIP archive has an inconsistent structure.

**34**
:   Unsupported encryption method. Use `-o force` to bypass the encryption
    method verification.

**36**
:   Password needed. A ZIP archive contains an encrypted file, but no password
    was provided. Use `-o force` to bypass the password verification.

**37**
:   Wrong password. A ZIP archive contains an encrypted file, and the provided
    password does not decrypt it. Use `-o force` to bypass the password
    verification.

**45**
:   Possibly truncated or corrupted ZIP archive, as detected by **libzip** 1.11
    or higher.

# PROJECT HISTORY

**mount-zip** started as a fork of **fuse-zip**.

The original **fuse-zip** project was created in 2008 by
[Alexander Galanin](http://galanin.nnov.ru/~al/) and is available on
[Bitbucket](https://bitbucket.org/agalanin/fuse-zip).

The **mount-zip** project was then forked from **fuse-zip** in 2021 and further
developed by [François Degros](https://github.com/fdegros). The ability to write
and modify ZIP archives has been removed, but a number of optimizations and
features have been added:

Feature                       | **mount-zip** | **fuse-archive** | **fuse-zip**
:---------------------------- | :-----------: | :--------------: | :---------:
Read-Write Mode               | ❌             | ❌                | ✅
Read-Only Mode                | ✅             | ✅                | ✅
Format Support            | ZIP           | Wide             | ZIP
GPG Encryption            | ❌             | ✅                | ❌
Native ZIP Encryption     | ✅             | ✅                | ✅
Lazy Decompression        | ✅             | ✅                | ❌
Default Caching           | Lazy          | Pre-emptive      | N/A
Memory Caching            | ✅             | ✅                | ✅
Temp File Caching         | ✅             | ✅                | ❌
Handles Huge Files        | ✅             | ✅                | ❌
Sparse File Detection     | ❌             | ✅                | ❌
Precision Timestamps      | ✅             | ✅                | ✅
Several Archives          | ✅             | ✅                | ❌
Automatic Mount Point     | ✅             | ✅                | ❌
FUSE 3 Support            | ✅             | ✅                | ❌
Distinct Error Codes      | ✅             | ✅                | ❌

# AUTHORS

*   [François Degros](https://github.com/fdegros)
*   [Alexander Galanin](http://galanin.nnov.ru/~al/)

# LICENSE

**mount-zip** is released under the GNU General Public License Version 3 or
later.

# SEE ALSO

fuse-archive(1), fuse-zip(1), fusermount(1), fuse(8), umount(8)

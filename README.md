---
title: MOUNT-ZIP
section: 1
header: User Manual
footer: mount-zip 1.0
date: August 2022
---
# NAME

**mount-zip** - Mount a ZIP archive as a FUSE filesystem.

# SYNOPSIS

**mount-zip** [*options*] *zip-file* [*mount-point*]

# DESCRIPTION

**mount-zip** is a tool allowing to open, explore and extract ZIP archives.

**mount-zip** mounts a ZIP archive as a read-only
[FUSE file system](https://en.wikipedia.org/wiki/Filesystem_in_Userspace), which
can then be explored and read by any application.

**mount-zip** aspires to be an excellent ZIP mounter. It starts quickly, uses
little memory, decodes encrypted files, and provides on-the-go decompression and
caching for maximum efficiency.

The mount point should be an empty directory. If the mount point doesn't exist
yet, **mount-zip** creates it first. If no mount point is provided,
**mount-zip** creates one in the same directory as the ZIP archive.

# OPTIONS

**-\-help** **-h**
:   print help

**-\-version**
:   print version

**-\-quiet** **-q**
:   print fewer log messages

**-\-verbose**
:   print more log messages

**-\-redact**
:   redact file names from log messages

**-\-force**
:   mount ZIP even if password is wrong or missing, or if the encryption or
    compression method is unsupported

**-\-encoding=CHARSET**
:   original encoding of file names

**-\-cache=DIR**
:   cache directory (default is `/tmp`)

**-\-nocache**
:   no caching of uncompressed data

**-o nospecials**
:   hide special files (FIFOs, sockets, devices)

**-o nosymlinks**
:   hide symbolic links

**-o nohardlinks**
:   hide hard links

# USAGE

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
$ fusermount -u mnt
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
*   Supports UNIX access modes and DOS file permissions
*   Supports owner and group information (UID and GID)
*   Supports relative and absolute paths
*   Supports high precision time stamps
*   Works on 32-bit and 64-bit devices
*   Supports ZIP64 extensions, even on 32-bit devices:
    *   Supports ZIP archives containing more than 65,535 files
    *   Supports ZIP archives and files bigger than 4 GB
*   Supports ZIP format extensions:
    *   000A PKWARE NTFS Extra Field: High-precision timestamps
    *   000D PKWARE UNIX Extra Field: File type
    *   5455 Extended Timestamp
    *   5855 Info-ZIP Unix Extra Field (type 1)
    *   7855 Info-ZIP Unix Extra Field (type 2)
    *   7875 Info-ZIP New Unix Extra Field: Variable-length UIDs and GIDs

## File Name Encoding

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

## Name Deduplication

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

Directories are never renamed. If a file name is colliding with a directory
name, the file is the one getting renamed.

## Encrypted Archives

**mount-zip** supports encrypted ZIP archives. It understand both the legacy ZIP
encryption scheme, and the more recent AES encryption schemes.

When **mount-zip** finds an encrypted file while mounting a ZIP archive, it asks
for a password. If the given password doesn't allow to decrypt the file, then
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
Use the --force option to mount an encrypted ZIP with a wrong password
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
right password by using the `--force` option:

```
$ mount-zip --force different-encryptions.zip mnt
Need password for File [1] '/Encrypted AES-128.txt'
Password > Got it!
Continuing despite wrong password because of --force option
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
md5sum: 'mnt/Encrypted AES-128.txt': Input/output error
md5sum: 'mnt/Encrypted AES-192.txt': Input/output error
md5sum: 'mnt/Encrypted AES-256.txt': Input/output error
md5sum: 'mnt/Encrypted ZipCrypto.txt': Input/output error

$ cat mnt/*
This is not encrypted.
cat: 'mnt/Encrypted AES-128.txt': Input/output error
cat: 'mnt/Encrypted AES-192.txt': Input/output error
cat: 'mnt/Encrypted AES-256.txt': Input/output error
cat: 'mnt/Encrypted ZipCrypto.txt': Input/output error
```

For security reasons, **mount-zip** doesn't allow to specify the password on the
command line. However, it is possible to pipe the password to **mount-zip**'s
standard input:

```
$ echo password | mount-zip different-encryptions.zip mnt
Need password for File [1] '/Encrypted AES-128.txt'
Password is Ok
```

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
$ mount-zip pkware-specials.zip mnt

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

Special files can be suppressed with the `-o nospecials` option:

```
$ mount-zip -o nospecials pkware-specials.zip mnt
Skipped Block Device [0] '/block'
Skipped Character Device [1] '/char'
Skipped Pipe [2] '/fifo'
Skipped Socket [4] '/socket'
Skipped Block Device [7] '/z-hardlink-block'
Skipped Character Device [8] '/z-hardlink-char'
Skipped Pipe [9] '/z-hardlink-fifo'
Skipped Socket [10] '/z-hardlink-socket'

$ ls -n mnt
-rw-r--r-- 3 1000 1000 32 Aug  9  2019 regular
lrwxrwxrwx 1 1000 1000  7 Aug  3  2019 symlink -> regular
lrwxrwxrwx 1 1000 1000  7 Aug 25  2019 symlink2 -> regular
-rw-r--r-- 3 1000 1000 32 Aug  9  2019 z-hardlink1
-rw-r--r-- 3 1000 1000 32 Aug  9  2019 z-hardlink2
lrwxrwxrwx 1 1000 1000  7 Aug  3  2019 z-hardlink-symlink -> regular
```

## Hard Links

**mount-zip** shows hard links recorded in the ZIP archive.

In this example, the three file entries `0regular`, `hlink1` and `hlink2` point
to the same inode number (2) and their reference count is 3:

```
$ mount-zip -o use_ino hlink-chain.zip mnt

$ ls -ni mnt
2 -rw-r----- 3 0 0 10 Aug 14  2019 0regular
2 -rw-r----- 3 0 0 10 Aug 14  2019 hlink1
2 -rw-r----- 3 0 0 10 Aug 14  2019 hlink2

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

Duplicated hard links can be suppressed with the `-o nohardlinks` option:

```
$ mount-zip -o nohardlinks hlink-chain.zip mnt
Skipped File [1]: Hardlinks are ignored
Skipped File [2]: Hardlinks are ignored

$ ls -ni mnt
2 -rw-r----- 1 0 0 10 Aug 14  2019 0regular
```

## File Permissions

**mount-zip** shows the Unix file permissions and ownership (UIDs and GIDs) as
recorded in the ZIP archive:

```
$ mount-zip unix-perm.zip mnt

$ ls -n mnt
-rw-r----- 1 1000 1000 0 Jan  5  2014 640
-rw-r---w- 1 1000 1000 0 Jan  5  2014 642
-rw-rw-rw- 1 1000 1000 0 Jan  5  2014 666
-rwsrwsr-x 1 1000 1000 0 Jan  5  2014 6775
-rwxrwxrwx 1 1000 1000 0 Jan  5  2014 777
```

Note that these access permissions are not enforced by default. In this example,
I am able to read the file `640` even though I don't own it and I don't have the
read permission:

```
$ md5sum mnt/*
d41d8cd98f00b204e9800998ecf8427e  mnt/640
d41d8cd98f00b204e9800998ecf8427e  mnt/642
d41d8cd98f00b204e9800998ecf8427e  mnt/666
d41d8cd98f00b204e9800998ecf8427e  mnt/6775
d41d8cd98f00b204e9800998ecf8427e  mnt/777
```

To enforce the access permission check, use the `-o default_permissions` mount
option:

```
$ mount-zip -o default_permissions unix-perm.zip mnt

$ md5sum mnt/*
md5sum: mnt/640: Permission denied
md5sum: mnt/642: Permission denied
d41d8cd98f00b204e9800998ecf8427e  mnt/666
d41d8cd98f00b204e9800998ecf8427e  mnt/6775
d41d8cd98f00b204e9800998ecf8427e  mnt/777
```

## Absolute and Parent-Relative Paths

**mount-zip** supports absolute and parent-relative paths in file names.
Absolute paths are displayed under the `ROOT` directory. For parent-relative
paths, every `..` is replaced by `UP`. Finally, ordinary relative paths are
placed under the `CUR` directory:

```
$ unzip -l mixed-paths.zip
 Length      Date    Time   Name
--------  ---------- -----  ----
      49  2021-11-02 13:55  normal.txt
      29  2021-11-02 13:55  ../up-1.txt
      30  2021-11-02 13:55  ../../up-2.txt
      40  2021-11-02 13:55  /top.txt
      45  2021-11-02 13:55  /../over-the-top.txt
--------                    -------
     193                    5 files

$ mount-zip mixed-paths.zip mnt
mount-zip[2886935]: Bad file name: '/../over-the-top.txt'
mount-zip[2886935]: Skipped File [4]: Cannot normalize path

$ tree mnt
mnt
├── CUR
│   └── normal.txt
├── ROOT
│   └── top.txt
├── UP
│   └── up-1.txt
└── UPUP
    └── up-2.txt

4 directories, 4 files
```

## Smart Caching

**mount-zip** only does the minimum amount of work required to serve the
requested data. When reading a compressed file, **mount-zip** only decompresses
enough data to serve the reading application. This is called *lazy* or
*on-the-go* decompression.

Accessing the beginning of a big compressed file is therefore instantaneous:

```
$ mount-zip 'Big One.zip' mnt

$ ls -lh mnt/
-rw-rw-r-- 1 root root 6.4G Mar 26  2020 'Big One.txt'

$ time head -4 'mnt/Big One.txt'
We're going on a bear hunt.
We're going to catch a big one.
What a beautiful day!
We're not scared.

real    0m0.030s
user    0m0.015s
sys     0m0.014s
```

**mount-zip** generally avoids caching decompressed data. If you read a
compressed file several times, it is getting decompressed each time:

```
$ dd if='mnt/Big One.txt' of=/dev/null status=progress
6777995272 bytes (6.8 GB, 6.3 GiB) copied, 24.9395 s, 272 MB/s

$ dd if='mnt/Big One.txt' of=/dev/null status=progress
6777995272 bytes (6.8 GB, 6.3 GiB) copied, 24.961 s, 272 MB/s
```

But **mount-zip** will start caching a file if it detects that this file is
getting read in a non-sequential way (ie the reading application starts jumping
to different positions of the file).

For example, `tail` jumps to the end of the file. The first time this happens,
**mount-zip** decompresses the whole file and caches the decompressed data (in
about 13 seconds in this instance):

```
$ time tail -1 'mnt/Big One.txt'
The End

real    0m12.631s
user    0m0.024s
sys     0m0.656s
```

A subsequent call to `tail` is instantaneous, because **mount-zip** has now
cached the decompressed data:

```
$ time tail -1 'mnt/Big One.txt'
The End

real    0m0.032s
user    0m0.018s
sys     0m0.018s
```

Decompressed data is cached in a temporary file located in the cache directory
(`/tmp` by default). The cache directory can be changed with the `--cache=DIR`
option. The cache file is only created if necessary, and automatically deleted
when the ZIP is unmounted.

If **mount-zip** cannot create the cache file, or if it was passed the
`--nocache` option, it will do its best using a small rolling buffer in memory.
However, some data access patterns might then result in poor performance,
especially if **mount-zip** has to repeatedly extract the same file.

# PERFORMANCE

On small archives **mount-zip** has the same performance as commonly used
virtual filesystems such as KIO, Gnome GVFS, mc vfs, unpackfs, avfs and
fuse-j-zip. But on large archives containing many files, **mount-zip** is pretty
quick.

For example on my laptop, a ZIP archive containing more than 70,000 files is
mounted in half a second:

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

The full contents of this mounted ZIP, totalling 1.1 GB, can be extracted with
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

**mount-zip** records log messages into `/var/log/user.log`. They can help
troubleshooting issues, especially if you are facing I/O errors when reading
files from the mounted ZIP.

To read **mount-zip**'s log messages:

```
$ grep mount-zip /var/log/user.log | less -S
```

To follow **mount-zip**'s log messages as they are being written:

```
$ tail -F /var/log/user.log | grep mount-zip
```

By default, **mount-zip** writes INFO and ERROR messages. You can decrease the
logging level to just ERROR messages with the `--quiet` option. Or you can
increase the logging level to include DEBUG messages with the `--verbose`
option:

```
$ mount-zip -f --verbose foobar.zip mnt
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
the `--redact` option:

```
$ mount-zip -f --verbose --redact bad-crc.zip mnt
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
related the ZIP archive itself:

**0**
:   Success.

**1**
:   Generic error code for: missing argument, unknown option, unknown file name
    encoding, mount point cannot be created, mount point is not empty, etc.

**11**
:   The archive is a multipart ZIP.

**15**
:   **mount-zip** cannot read the ZIP archive.

**19**
:   **mount-zip** cannot find the ZIP archive.

**21**
:   **mount-zip** cannot open the ZIP archive.

**23**
:   Zlib data error. This is probably the sign of a wrong password. Use
    `--force` to bypass the password verification.

**26**
:   Unsupported compression method. Use `--force` to bypass the compression
    method verification.

**29**
:   The archive is not recognized as a valid ZIP.

**31**
:   The ZIP archive has an inconsistent structure.

**34**
:   Unsupported encryption method. Use `--force` to bypass the encryption method
    verification.

**36**
:   Needs password. The ZIP archive contains an encrypted file, but no password
    was provided. Use `--force` to bypass the password verification.

**37**
:   Wrong password. The ZIP archive contains an encrypted file, and the provided
    password does not allow to decrypt it. Use `--force` to bypass the password
    verification.

# PROJECT HISTORY

**mount-zip** started as a fork of **fuse-zip**.

The original **fuse-zip** project was created in 2008 by
[Alexander Galanin](http://galanin.nnov.ru/~al/) and is available on
[Bitbucket](https://bitbucket.org/agalanin/fuse-zip).

The **mount-zip** project was then forked from **fuse-zip** in 2021 and further
developed by [François Degros](https://github.com/fdegros). The ability to write
and modify ZIP archives has been removed, but a number of optimisations and
features have been added:

Feature                      | mount-zip | fuse-zip
:--------------------------- | :-------: | :------:
Read-Write Mode              | ❌         | ✅
Read-Only Mode               | ✅         | ✅
Shows Symbolic Links         | ✅         | ✅
Shows Hard Links             | ✅         | ✅
Shows Special Files          | ✅         | ✅
Shows Precise Timestamps     | ✅         | ✅
Allows Random Access         | ✅         | ✅
Decompresses Lazily          | ✅         | ❌
Decrypts Encrypted Files     | ✅         | ❌
Detects Name Encoding        | ✅         | ❌
Deduplicates Names           | ✅         | ❌
Reads Huge Files             | ✅         | ❌
Smart Caching                | ✅         | ❌
Can Hide Symlinks            | ✅         | ❌
Can Hide Hard Links          | ✅         | ❌
Can Hide Special Files       | ✅         | ❌
Can Redact Log Messages      | ✅         | ❌
Returns Distinct Error Codes | ✅         | ❌

# AUTHORS

*   [François Degros](https://github.com/fdegros)
*   [Alexander Galanin](http://galanin.nnov.ru/~al/)

# LICENSE

**mount-zip** is released under the GNU General Public License Version 3 or
later.

# SEE ALSO

fusermount(1), fuse(8), umount(8)

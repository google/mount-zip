#!/usr/bin/python3

# Copyright 2021 Google LLC
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import hashlib
import logging
import os
import pprint
import random
import stat
import subprocess
import sys
import tempfile


# Computes the MD5 hash of the given file.
# Returns the MD5 hash as an hexadecimal string.
# Throws OSError if the file cannot be read.
def md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        while chunk := f.read(4096):
            h.update(chunk)
    return h.hexdigest()


# Walks the given directory.
# Returns a dict representing all the files and directories.
def GetTree(root, use_md5=True):
    result = {}

    def scan(dir):
        for entry in os.scandir(dir):
            path = entry.path
            st = entry.stat(follow_symlinks=False)
            mode = st.st_mode
            line = {
                'ino': st.st_ino,
                'mode': stat.filemode(mode),
                'nlink': st.st_nlink,
                'uid': st.st_uid,
                'gid': st.st_gid,
                'atime': st.st_atime_ns,
                'mtime': st.st_mtime_ns,
                'ctime': st.st_ctime_ns,
            }
            result[os.path.relpath(path, root)] = line
            if stat.S_ISREG(mode):
                line['size'] = st.st_size
                try:
                    if use_md5: line['md5'] = md5(path)
                except OSError as e:
                    line['errno'] = e.errno
                continue
            if stat.S_ISDIR(mode):
                scan(path)
                continue
            if stat.S_ISLNK(mode):
                line['target'] = os.readlink(path)
                continue
            if stat.S_ISBLK(mode) or stat.S_ISCHR(mode):
                line['rdev'] = st.st_rdev
                continue

    scan(root)
    return result


# Total number of errors.
error_count = 0


# Logs the given error.
def LogError(msg):
    logging.error(msg)
    global error_count
    error_count += 1


# Compares got_tree with want_tree. If strict is True, checks that got_tree
# doesn't contain any extra entries that aren't in want_tree.
def CheckTree(got_tree, want_tree, strict=False):
    for name, want_entry in want_tree.items():
        try:
            got_entry = got_tree.pop(name)
            for key, want_value in want_entry.items():
                if key in ('atime', 'ctime', 'mtime'):
                    continue  # For the time being
                got_value = got_entry.get(key)
                if got_value != want_value:
                    LogError(
                        f'Mismatch for {name!r}[{key}] got: {got_value!r}, want: {want_value!r}'
                    )
        except KeyError:
            LogError(f'Missing entry {name!r}')

    if strict and got_tree:
        LogError(f'Found {len(got_tree)} unexpected entries: {got_tree}')


# Directory of this test program.
script_dir = os.path.dirname(os.path.realpath(__file__))

# Directory containing the ZIP archives to mount.
data_dir = os.path.join(script_dir, 'data')

# Path of the FUSE mounter.
mount_program = os.path.join(script_dir, '..', '..', 'mount-zip')


# Mounts the given ZIP archive, walks the mounted ZIP and unmounts.
# Returns a dict representing the mounted ZIP.
# Throws subprocess.CalledProcessError if the ZIP cannot be mounted.
def MountZipAndGetTree(zip_name, options=[], password='', use_md5=True):
    with tempfile.TemporaryDirectory() as mount_point:
        zip_path = os.path.join(script_dir, 'data', zip_name)
        logging.debug(f'Mounting {zip_path!r} on {mount_point!r}...')
        subprocess.run([mount_program, *options, zip_path, mount_point],
                       check=True,
                       capture_output=True,
                       input=password,
                       encoding='UTF-8')
        try:
            logging.debug(f'Mounted ZIP {zip_path!r} on {mount_point!r}')
            return GetTree(mount_point, use_md5=use_md5)
        finally:
            logging.debug(f'Unmounting {zip_path!r} from {mount_point!r}...')
            subprocess.run(['fusermount', '-u', '-z', mount_point], check=True)
            logging.debug(f'Unmounted {zip_path!r} from {mount_point!r}')


# Mounts the given ZIP archive, checks the mounted ZIP tree and unmounts.
# Logs an error if the ZIP cannot be mounted.
def MountZipAndCheckTree(zip_name,
                         want_tree,
                         options=[],
                         password='',
                         strict=True,
                         use_md5=True):
    logging.info(f'Checking {zip_name!r}...')
    try:
        got_tree = MountZipAndGetTree(zip_name,
                                      options=options,
                                      password=password,
                                      use_md5=use_md5)
        CheckTree(got_tree, want_tree, strict=strict)
    except subprocess.CalledProcessError as e:
        LogError(f'Cannot test {zip_name}: {e.stderr}')


# Try to mount the given ZIP archive, and expects an error.
# Logs an error if the ZIP can be mounted, or if the returned error code doesn't match.
def CheckZipMountingError(zip_name, want_error_code, options=[], password=''):
    logging.info(f'Checking {zip_name!r}...')
    try:
        got_tree = MountZipAndGetTree(zip_name,
                                      options=options,
                                      password=password)
        LogError(f'Want error, Got tree: {got_tree}')
    except subprocess.CalledProcessError as e:
        if (e.returncode != want_error_code):
            LogError(
                f'Want error: {want_error_code}, Got error: {e.returncode} in {e}'
            )


def GenerateReferenceData():
    for zip_name in os.listdir(os.path.join(script_dir, 'data')):
        all_zips[zip_name] = MountZipAndGetTree(zip_name, password='password')

    pprint.pprint(all_zips, compact=True, sort_dicts=False)


# Tests most of the ZIP files in data_dir using default mounting options.
def TestZipWithDefaultOptions():
    want_trees = {
        'absolute-path.zip': {
            'ROOT': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'ROOT/rootname.ext': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1371478408000000000,
                'mtime': 1371478408000000000,
                'ctime': 1371478408000000000,
                'size': 22,
                'md5': '8b4cc90d421780e7674e2a25db33b770'
            }
        },
        'bad-archive.zip': {
            'bash.txt': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1265257350000000000,
                'mtime': 1265257351000000000,
                'ctime': 1265228552000000000,
                'size': 282292,
                'errno': 5
            }
        },
        'bad-crc.zip': {
            'bash.txt': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1265262326000000000,
                'mtime': 1265257351000000000,
                'ctime': 1265228552000000000,
                'size': 282292,
                'errno': 5
            }
        },
        'bzip2.zip': {
            'bzip2.txt': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1635811418000000000,
                'mtime': 1635811418000000000,
                'ctime': 1635811418000000000,
                'size': 36,
                'md5': 'f28623c0be8da636e6c53ead06ce0731'
            }
        },
        'comment-utf8.zip': {
            'dir': {
                'mode': 'drwxrwxr-x',
                'nlink': 2,
                'uid': 1000,
                'gid': 1000,
                'atime': 1563727175000000000,
                'mtime': 1563727169000000000,
                'ctime': 1563701970000000000
            },
            'dir/empty_comment.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1563727148000000000,
                'mtime': 1563721364000000000,
                'ctime': 1563696164000000000,
                'size': 14,
                'md5': '3453ff2f4d15a71d21e859829f0da9fc'
            },
            'dir/with_comment.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1563727148000000000,
                'mtime': 1563720300000000000,
                'ctime': 1563695100000000000,
                'size': 13,
                'md5': '557fcaa19da1b3e2cd6ce8e546a13f46'
            },
            'dir/without_comment.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1563727235000000000,
                'mtime': 1563720306000000000,
                'ctime': 1563695106000000000,
                'size': 16,
                'md5': '4cdf349a6dc1516f9642ebaf278af394'
            }
        },
        'comment.zip': {
            'dir': {
                'mode': 'drwxrwxr-x',
                'nlink': 2,
                'uid': 1000,
                'gid': 1000,
                'atime': 1563727175000000000,
                'mtime': 1563727169000000000,
                'ctime': 1563701970000000000
            },
            'dir/empty_comment.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1563727148000000000,
                'mtime': 1563721364000000000,
                'ctime': 1563696164000000000,
                'size': 14,
                'md5': '3453ff2f4d15a71d21e859829f0da9fc'
            },
            'dir/with_comment.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1563727148000000000,
                'mtime': 1563720300000000000,
                'ctime': 1563695100000000000,
                'size': 13,
                'md5': '557fcaa19da1b3e2cd6ce8e546a13f46'
            },
            'dir/without_comment.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1563727148000000000,
                'mtime': 1563720306000000000,
                'ctime': 1563695106000000000,
                'size': 16,
                'md5': '4cdf349a6dc1516f9642ebaf278af394'
            }
        },
        'dos-perm.zip': {
            'dir': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'dir/hidden.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1388826134000000000,
                'mtime': 1388826134000000000,
                'ctime': 1388826134000000000,
                'size': 11,
                'md5': 'fa29ea74a635e3be468256f9007b1bb6'
            },
            'dir/normal.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1388826120000000000,
                'mtime': 1388826120000000000,
                'ctime': 1388826120000000000,
                'size': 11,
                'md5': '050256c2ac1d77494b9fddaa933f5eda'
            },
            'dir/readonly.txt': {
                'mode': '-r--r--r--',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1388826146000000000,
                'mtime': 1388826146000000000,
                'ctime': 1388826146000000000,
                'size': 14,
                'md5': '6f651ad751dadd4e76c8f46b6fae0c48'
            }
        },
        'empty.zip': {},
        'extrafld.zip': {
            'README': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1388831602000000000,
                'mtime': 1372483540000000000,
                'ctime': 1372461940000000000,
                'size': 2551,
                'md5': '27d03e3bb5ed8add7917810c3ba68836'
            }
        },
        'fifo.zip': {
            '-': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1564403758000000000,
                'mtime': 1564403758000000000,
                'ctime': 1564403758000000000,
                'size': 14,
                'md5': '42e16527aee5fe43ffa78a89d36a244c'
            },
            'fifo': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1564428868000000000,
                'mtime': 1564428868000000000,
                'ctime': 1564403668000000000,
                'size': 13,
                'md5': 'd36dcce71b10bb7dd348ab6efb1c4aab'
            }
        },
        'file-dir-same-name.zip': {
            'pet': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'pet/cat': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'pet/cat/fish': {
                'mode': 'drwxrwxr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1635479490000000000,
                'mtime': 1635479490000000000,
                'ctime': 1635479490000000000
            },
            'pet/cat/fish (1)': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1635479490000000000,
                'mtime': 1635479490000000000,
                'ctime': 1635479490000000000,
                'size': 30,
                'md5': '47661a04dfd111f95ec5b02c4a1dab05'
            },
            'pet/cat/fish (2)': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1635479490000000000,
                'mtime': 1635479490000000000,
                'ctime': 1635479490000000000,
                'size': 31,
                'md5': '4969fad4edba3582a114f17000583344'
            },
            'pet/cat (1)': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1635479490000000000,
                'mtime': 1635479490000000000,
                'ctime': 1635479490000000000,
                'size': 25,
                'md5': '3a3cd8d02b1a2a232e2684258f38c882'
            },
            'pet/cat (2)': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1635479490000000000,
                'mtime': 1635479490000000000,
                'ctime': 1635479490000000000,
                'size': 26,
                'md5': '07691c6ecc5be713b8d1224446f20970'
            },
            'pet (1)': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1635479490000000000,
                'mtime': 1635479490000000000,
                'ctime': 1635479490000000000,
                'size': 21,
                'md5': '185743d56c5a917f02e2193a96effb25'
            },
            'pet (2)': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1635479490000000000,
                'mtime': 1635479490000000000,
                'ctime': 1635479490000000000,
                'size': 22,
                'md5': 'ec5ffb0de216fb1e9578bc17a169399a'
            }
        },
        'foobar.zip': {
            'foo': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1565484162000000000,
                'mtime': 1565484162000000000,
                'ctime': 1565458962000000000,
                'size': 4,
                'md5': 'c157a79031e1c40f85931829bc5fc552'
            }
        },
        'hlink-before-target.zip': {
            '0hlink': {
                'mode': '-rw-r-----',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565781818000000000,
                'mtime': 1565781818000000000,
                'ctime': 1565781818000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            },
            '1regular': {
                'mode': '-rw-r-----',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565781818000000000,
                'mtime': 1565781818000000000,
                'ctime': 1565781818000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            }
        },
        'hlink-chain.zip': {
            '0regular': {
                'mode': '-rw-r-----',
                'nlink': 3,
                'uid': 0,
                'gid': 0,
                'atime': 1565781818000000000,
                'mtime': 1565781818000000000,
                'ctime': 1565781818000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            },
            'hlink1': {
                'mode': '-rw-r-----',
                'nlink': 3,
                'uid': 0,
                'gid': 0,
                'atime': 1565781818000000000,
                'mtime': 1565781818000000000,
                'ctime': 1565781818000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            },
            'hlink2': {
                'mode': '-rw-r-----',
                'nlink': 3,
                'uid': 0,
                'gid': 0,
                'atime': 1565781818000000000,
                'mtime': 1565781818000000000,
                'ctime': 1565781818000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            }
        },
        'hlink-different-types.zip': {
            'dir': {
                'mode': 'drwxrwxrwx',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565781818000000000,
                'mtime': 1565781818000000000,
                'ctime': 1565781818000000000
            },
            'dir/regular': {
                'mode': '-rw-r-----',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1565781818000000000,
                'mtime': 1565781818000000000,
                'ctime': 1565781818000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            },
            'hlink': {
                'mode': '-rwxrwxrwx',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            }
        },
        'hlink-dir.zip': {
            'dir': {
                'mode': 'drwxrwxrwx',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1567323446000000000,
                'mtime': 1567323446000000000,
                'ctime': 1567323446000000000
            },
            'dir/regular': {
                'mode': '-rw-r-----',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1567323446000000000,
                'mtime': 1567323446000000000,
                'ctime': 1567323446000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            },
            'hlink': {
                'mode': 'drwxrwxrwx',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1567348646000000000,
                'mtime': 1567348646000000000,
                'ctime': 1567323446000000000
            }
        },
        'hlink-recursive-one.zip': {
            'hlink': {
                'mode': '-rw-r-----',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            }
        },
        'hlink-recursive-two.zip': {
            'hlink1': {
                'mode': '-rw-r-----',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            },
            'hlink2': {
                'mode': '-rw-r-----',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            }
        },
        'hlink-relative.zip': {
            'UP': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'UP/0regular': {
                'mode': '-rw-r-----',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1567323446000000000,
                'mtime': 1567323446000000000,
                'ctime': 1567323446000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            },
            'UP/hlink': {
                'mode': '-rw-r-----',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1567323446000000000,
                'mtime': 1567323446000000000,
                'ctime': 1567323446000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            }
        },
        'hlink-special.zip': {
            '0block': {
                'mode': 'brw-r--r--',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000,
                'rdev': 2303
            },
            '0fifo': {
                'mode': 'prw-r--r--',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000
            },
            '0socket': {
                'mode': 'srw-r--r--',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000
            },
            'hlink-block': {
                'mode': 'brw-r--r--',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000,
                'rdev': 2303
            },
            'hlink-fifo': {
                'mode': 'prw-r--r--',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000
            },
            'hlink-socket': {
                'mode': 'srw-r--r--',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000
            }
        },
        'hlink-symlink.zip': {
            '0regular': {
                'mode': '-rw-r-----',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1565781818000000000,
                'mtime': 1565781818000000000,
                'ctime': 1565781818000000000,
                'size': 10,
                'md5': 'e09c80c42fda55f9d992e59ca6b3307d'
            },
            '1symlink': {
                'mode': 'lrwxrwxrwx',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000,
                'target': '0regular'
            },
            'hlink': {
                'mode': 'lrwxrwxrwx',
                'nlink': 2,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000,
                'target': '0regular'
            }
        },
        'hlink-without-target.zip': {
            'hlink1': {
                'mode': '-rw-r-----',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1565807019000000000,
                'mtime': 1565807019000000000,
                'ctime': 1565781818000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            }
        },
        'issue-43.zip': {
            'README': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1425892392000000000,
                'mtime': 1265521877000000000,
                'ctime': 1265493078000000000,
                'size': 760,
                'md5': 'f196d610d1cdf9191b4440863e8d31ab'
            },
            'a': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'a/b': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'a/b/c': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'a/b/c/d': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'a/b/c/d/e': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'a/b/c/d/e/f': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'a/b/c/d/e/f/g': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1425893690000000000,
                'mtime': 1425893690000000000,
                'ctime': 1425864890000000000,
                'size': 32,
                'md5': '21977dc7948b88fdefd50f77afc9ac7b'
            },
            'a/b/c/d/e/f/g2': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1425893719000000000,
                'mtime': 1425893719000000000,
                'ctime': 1425864920000000000,
                'size': 32,
                'md5': '74ad09827032bc0be935c23c53f8ee29'
            }
        },
        'lzma.zip': {
            'lzma.txt': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1635812732000000000,
                'mtime': 1635812732000000000,
                'ctime': 1635812732000000000,
                'size': 35,
                'md5': 'afcc577cec75357c61cc360e3bca0ac6'
            }
        },
        'mixed-paths.zip': {
            'CUR/Empty': {
                'ino': 18,
                'mode': 'drwxrwxr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'CUR/Square [].txt': {
                'ino': 16,
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 30,
                'md5': 'a3c0f4c806fd09aed3820f4cd04a5c17'
            },
            'CUR/Angle <>.txt': {
                'ino': 15,
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 29,
                'md5': '3275b321cca4963642cf083bbcc0cf2d'
            },
            'CUR/Backslash (\\).txt': {
                'ino': 14,
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 23,
                'md5': '1c5360ba1d4f1698941d2ef7ccc722d8'
            },
            'CUR/Double quote (").txt': {
                'ino': 13,
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 26,
                'md5': 'b4d1ac0f736d863e0cead53f96c7d314'
            },
            "CUR/Quote (').txt": {
                'ino': 12,
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 19,
                'md5': '7d27cb02785c9c7a939889104544de82'
            },
            'CUR/Question (?).txt': {
                'ino': 11,
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 27,
                'md5': '6449d2e0a6ac10b9d763347208e4a347'
            },
            'CUR/Star (*).txt': {
                'ino': 10,
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 18,
                'md5': 'fd26a5799726c278ede65ab2298a513a'
            },
            'ROOT': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'ROOT/Empty': {
                'ino': 20,
                'mode': 'drwxrwxr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'ROOT/top.txt': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 40,
                'md5': '153d090308be772d55ab57269cc03377'
            },
            'UPUP': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'UPUP/up-2.txt': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 30,
                'md5': 'd0fa0ae6a603cb14fdecc0f2ec45ca71'
            },
            'UP': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'UP/up-1.txt': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 29,
                'md5': '4d7835d420f48238d0e95d3271f31273'
            },
            'CUR': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'CUR/normal.txt': {
                'mode': '-rw-------',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'size': 49,
                'md5': 'd1f99c5bd5af5898c9ef8829dfbbcfd6'
            }
        },
        'no-owner-info.zip': {
            'README': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1265493078000000000,
                'mtime': 1265493078000000000,
                'ctime': 1265493078000000000,
                'size': 760,
                'md5': 'f196d610d1cdf9191b4440863e8d31ab'
            }
        },
        'not-full-path-deep.zip': {
            'sim': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'sim/salabim': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'sim/salabim/rahat': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'sim/salabim/rahat/lukum': {
                'mode': '-rw-rw-rw-',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1389126376000000000,
                'mtime': 1389126376000000000,
                'ctime': 1389126376000000000,
                'size': 10,
                'md5': '16c52c6e8326c071da771e66dc6e9e57'
            },
            'sim/salabim/rahat-lukum': {
                'mode': '-rw-rw-rw-',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1389126376000000000,
                'mtime': 1389126376000000000,
                'ctime': 1389126376000000000,
                'size': 10,
                'md5': '38b18761d3d0c217371967a98d545c2e'
            }
        },
        'not-full-path.zip': {
            'bebebe': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1291639841000000000,
                'mtime': 1291639841000000000,
                'ctime': 1291611042000000000,
                'size': 5,
                'md5': '655dba24211af27d85c3cc4a910cc2ef'
            },
            'foo': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'foo/bar': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1291630409000000000,
                'mtime': 1291630409000000000,
                'ctime': 1291601610000000000,
                'size': 10,
                'md5': '10a28a91b53776aaf0800d68eb260eb6'
            }
        },
        'ntfs-extrafld.zip': {
            'test.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1560435723770100400,
                'mtime': 1560435721722114700,
                'ctime': 1560417720000000000,
                'size': 2600,
                'errno': 5
            }
        },
        'parent-relative-paths.zip': {
            'UP': {
                'mode': 'drwxr-xr-x',
                'nlink': 3,
                'uid': 0,
                'gid': 0
            },
            'UP/other': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'UP/other/LICENSE': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1371459156000000000,
                'mtime': 1371459156000000000,
                'ctime': 1371437556000000000,
                'size': 7639,
                'md5': '6a6a8e020838b23406c81b19c1d46df6'
            },
            'UPUP': {
                'mode': 'drwxr-xr-x',
                'nlink': 2,
                'uid': 0,
                'gid': 0
            },
            'UPUP/INSTALL': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1371459066000000000,
                'mtime': 1371459066000000000,
                'ctime': 1371437466000000000,
                'size': 454,
                'md5': '2ffc1c72359e389608ec4de36c6d1fac'
            }
        },
        'pkware-specials.zip': {
            'block': {
                'mode': 'brw-rw----',
                'nlink': 1,
                'uid': 0,
                'gid': 6,
                'atime': 1564833480000000000,
                'mtime': 1564833480000000000,
                'ctime': 1564808280000000000,
                'rdev': 2049
            },
            'char': {
                'mode': 'crw--w----',
                'nlink': 1,
                'uid': 0,
                'gid': 5,
                'atime': 1564833480000000000,
                'mtime': 1564833480000000000,
                'ctime': 1564808280000000000,
                'rdev': 1024
            },
            'fifo': {
                'mode': 'prw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1565809123000000000,
                'mtime': 1565809123000000000,
                'ctime': 1565783922000000000
            },
            'regular': {
                'mode': '-rw-r--r--',
                'nlink': 3,
                'uid': 1000,
                'gid': 1000,
                'atime': 1565807437000000000,
                'mtime': 1565290018000000000,
                'ctime': 1565264818000000000,
                'size': 32,
                'md5': '456e611a5420b7dd09bae143a7b2deb0'
            },
            'socket': {
                'mode': 'srw-------',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1564834444000000000,
                'mtime': 1564834444000000000,
                'ctime': 1564809244000000000
            },
            'symlink': {
                'mode': 'lrwxrwxrwx',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1566731330000000000,
                'mtime': 1564834729000000000,
                'ctime': 1564809528000000000,
                'target': 'regular'
            },
            'symlink2': {
                'mode': 'lrwxrwxrwx',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1566731384000000000,
                'mtime': 1566731354000000000,
                'ctime': 1566706154000000000,
                'target': 'regular'
            },
            'z-hardlink-block': {
                'mode': 'brw-rw----',
                'nlink': 1,
                'uid': 0,
                'gid': 6,
                'atime': 1564833480000000000,
                'mtime': 1564833480000000000,
                'ctime': 1564808280000000000,
                'rdev': 2049
            },
            'z-hardlink-char': {
                'mode': 'crw--w----',
                'nlink': 1,
                'uid': 0,
                'gid': 5,
                'atime': 1564833480000000000,
                'mtime': 1564833480000000000,
                'ctime': 1564808280000000000,
                'rdev': 1024
            },
            'z-hardlink-fifo': {
                'mode': 'prw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1565809123000000000,
                'mtime': 1565809123000000000,
                'ctime': 1565783922000000000
            },
            'z-hardlink-socket': {
                'mode': 'srw-------',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1564834444000000000,
                'mtime': 1564834444000000000,
                'ctime': 1564809244000000000
            },
            'z-hardlink-symlink': {
                'mode': 'lrwxrwxrwx',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1566731330000000000,
                'mtime': 1564834729000000000,
                'ctime': 1564809528000000000,
                'target': 'regular'
            },
            'z-hardlink1': {
                'mode': '-rw-r--r--',
                'nlink': 3,
                'uid': 1000,
                'gid': 1000,
                'atime': 1565807437000000000,
                'mtime': 1565290018000000000,
                'ctime': 1565264818000000000,
                'size': 32,
                'md5': '456e611a5420b7dd09bae143a7b2deb0'
            },
            'z-hardlink2': {
                'mode': '-rw-r--r--',
                'nlink': 3,
                'uid': 1000,
                'gid': 1000,
                'atime': 1565807437000000000,
                'mtime': 1565290018000000000,
                'ctime': 1565264818000000000,
                'size': 32,
                'md5': '456e611a5420b7dd09bae143a7b2deb0'
            }
        },
        'pkware-symlink.zip': {
            'regular': {
                'mode': '-rw-r--r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1566730531000000000,
                'mtime': 1566730512000000000,
                'ctime': 1566705312000000000,
                'size': 33,
                'md5': '4404716d8a90c37fdc18d88b70d09fa3'
            },
            'symlink': {
                'mode': 'lrwxrwxrwx',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1566730690000000000,
                'mtime': 1566730517000000000,
                'ctime': 1566705316000000000,
                'target': 'regular'
            }
        },
        'sjis-filename.zip': {
            '新しいテキスト ドキュメント.txt': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 0,
                'gid': 0,
                'atime': 1601539972000000000,
                'mtime': 1601539972000000000,
                'ctime': 1601539972000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            }
        },
        'symlink.zip': {
            'date': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1388943675000000000,
                'mtime': 1388943675000000000,
                'ctime': 1388918476000000000,
                'size': 35,
                'md5': 'e84bea37a02d9285935368412725b442'
            },
            'symlink': {
                'mode': 'lrwxrwxrwx',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1388943662000000000,
                'mtime': 1388943650000000000,
                'ctime': 1388918450000000000,
                'target': '../tmp/date'
            }
        },
        'unix-perm.zip': {
            '640': {
                'mode': '-rw-r-----',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1388890728000000000,
                'mtime': 1388890728000000000,
                'ctime': 1388865528000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            },
            '642': {
                'mode': '-rw-r---w-',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1388890755000000000,
                'mtime': 1388890755000000000,
                'ctime': 1388865556000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            },
            '666': {
                'mode': '-rw-rw-rw-',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1388890728000000000,
                'mtime': 1388890728000000000,
                'ctime': 1388865528000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            },
            '6775': {
                'mode': '-rwsrwsr-x',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1388890915000000000,
                'mtime': 1388890915000000000,
                'ctime': 1388865716000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            },
            '777': {
                'mode': '-rwxrwxrwx',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1388890728000000000,
                'mtime': 1388890728000000000,
                'ctime': 1388865528000000000,
                'size': 0,
                'md5': 'd41d8cd98f00b204e9800998ecf8427e'
            }
        },
        'with-and-without-precise-time.zip': {
            'unmodified': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1564327465000000000,
                'mtime': 1564327465000000000,
                'ctime': 1564302266000000000,
                'size': 7,
                'md5': '88e6b9694d2fe3e0a47a898110ed44b6'
            },
            'with-precise': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1532741025123456700,
                'mtime': 1532741025123456700,
                'ctime': 1564301732000000000,
                'size': 4,
                'md5': '814fa5ca98406a903e22b43d9b610105'
            },
            'without-precise': {
                'mode': '-rw-rw-r--',
                'nlink': 1,
                'uid': 1000,
                'gid': 1000,
                'atime': 1532741025000000000,
                'mtime': 1532741025000000000,
                'ctime': 1532715826000000000,
                'size': 4,
                'md5': '814fa5ca98406a903e22b43d9b610105'
            }
        }
    }

    for zip_name, want_tree in want_trees.items():
        MountZipAndCheckTree(zip_name, want_tree, options = ['--force'])


# Tests the ZIP with lots of files.
def TestZipWithManyFiles():
    # Only check a few files: the first one, the last one, and one in the middle.
    want_tree = {
        '1': {
            'mode': '-rw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1371243195000000000,
            'mtime': 1371243195000000000,
            'ctime': 1371221596000000000,
            'size': 0,
        },
        '30000': {
            'mode': '-rw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1371243200000000000,
            'mtime': 1371243200000000000,
            'ctime': 1371221600000000000,
            'size': 0,
        },
        '65536': {
            'mode': '-rw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1371243206000000000,
            'mtime': 1371243206000000000,
            'ctime': 1371221606000000000,
            'size': 0,
        }
    }

    MountZipAndCheckTree('65536-files.zip',
                         want_tree,
                         strict=False,
                         use_md5=False)

    want_tree = {
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file': {
            'size': 0,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (1)': {
            'size': 0,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (2)': {
            'size': 18,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (3)': {
            'size': 0,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (4)': {
            'size': 19,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (5)': {
            'size': 0,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (50000)': {
            'size': 0,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (99999)': {
            'size': 0,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (100000)': {
            'size': 0,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (100001)': {
            'size': 0,
        },
        'a/b/c/d/e/f/g/h/i/j/There are many versions of this file (100002)': {
            'size': 8,
        },
    }
    MountZipAndCheckTree('collisions.zip',
                         want_tree,
                         strict=False,
                         use_md5=False)


# Tests that a big file can be accessed in random order.
def TestBigZip():
    zip_name = 'big.zip'
    logging.info(f'Checking {zip_name!r}...')
    with tempfile.TemporaryDirectory() as mount_point:
        zip_path = os.path.join(script_dir, 'data', zip_name)
        logging.debug(f'Mounting {zip_path!r} on {mount_point!r}...')
        subprocess.run([mount_program, zip_path, mount_point],
                       check=True,
                       capture_output=True,
                       input='',
                       encoding='UTF-8')
        try:
            logging.debug(f'Mounted ZIP {zip_path!r} on {mount_point!r}')
            tree = GetTree(mount_point, use_md5=False)
            fd = os.open(os.path.join(mount_point, 'big.txt'), os.O_RDONLY)
            try:
                random.seed()
                n = 100000000
                for j in [random.randrange(n)
                          for i in range(100)] + [n - 1, 0, n - 1]:
                    logging.debug(f'Getting line {j}...')
                    want_line = b'%08d The quick brown fox jumps over the lazy dog.\n' % j
                    got_line = os.pread(fd, len(want_line), j * len(want_line))
                    if (got_line != want_line):
                        LogError(
                            f'Want line: {want_line!r}, Got line: {got_line!r}'
                        )
                got_line = os.pread(fd, 100, j * len(want_line))
                if (got_line != want_line):
                    LogError(
                        f'Want line: {want_line!r}, Got line: {got_line!r}')
                got_line = os.pread(fd, 100, n * len(want_line))
                if (got_line):
                    LogError(f'Want empty line, Got line: {got_line!r}')
            finally:
                os.close(fd)
        finally:
            logging.debug(f'Unmounting {zip_path!r} from {mount_point!r}...')
            subprocess.run(['fusermount', '-u', '-z', mount_point], check=True)
            logging.debug(f'Unmounted {zip_path!r} from {mount_point!r}')

# Tests that a big file can be accessed in somewhat random order even with no
# cache file.
def TestBigZipNoCache():
    zip_name = 'big.zip'
    logging.info(f'Checking {zip_name!r}...')
    with tempfile.TemporaryDirectory() as mount_point:
        zip_path = os.path.join(script_dir, 'data', zip_name)
        logging.debug(f'Mounting {zip_path!r} on {mount_point!r}...')
        subprocess.run([mount_program, '--nocache', zip_path, mount_point],
                       check=True,
                       capture_output=True,
                       input='',
                       encoding='UTF-8')
        try:
            logging.debug(f'Mounted ZIP {zip_path!r} on {mount_point!r}')
            tree = GetTree(mount_point, use_md5=False)
            fd = os.open(os.path.join(mount_point, 'big.txt'), os.O_RDONLY)
            try:
                random.seed()
                n = 100000000
                for j in sorted([random.randrange(n) for i in range(50)]) + [n - 1, 0] + sorted([random.randrange(n) for i in range(50)]) + [n - 1, 0]:
                    logging.debug(f'Getting line {j}...')
                    want_line = b'%08d The quick brown fox jumps over the lazy dog.\n' % j
                    got_line = os.pread(fd, len(want_line), j * len(want_line))
                    if (got_line != want_line):
                        LogError(
                            f'Want line: {want_line!r}, Got line: {got_line!r}'
                        )
            finally:
                os.close(fd)
        finally:
            logging.debug(f'Unmounting {zip_path!r} from {mount_point!r}...')
            subprocess.run(['fusermount', '-u', '-z', mount_point], check=True)
            logging.debug(f'Unmounted {zip_path!r} from {mount_point!r}')


# Tests encrypted ZIP.
def TestEncryptedZip():
    zip_name = 'different-encryptions.zip'

    # With correct password.
    want_tree = {
        'Encrypted ZipCrypto.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598594095000000000,
            'mtime': 1598592187000000000,
            'ctime': 1598592188000000000,
            'size': 34,
            'md5': '275e8c5aed7e7ce2f32dd1e5e9ee4a5b'
        },
        'Encrypted AES-256.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598594134000000000,
            'mtime': 1598592213000000000,
            'ctime': 1598592214000000000,
            'size': 32,
            'md5': 'ca5e064a0835d186f2f6326f88a7078f'
        },
        'Encrypted AES-192.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598594124000000000,
            'mtime': 1598592206000000000,
            'ctime': 1598592206000000000,
            'size': 32,
            'md5': 'e48d57930ef96ff2ad45867202d3250d'
        },
        'Encrypted AES-128.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598594117000000000,
            'mtime': 1598592200000000000,
            'ctime': 1598592200000000000,
            'size': 32,
            'md5': '07c4edd2a55c9d5614457a21fb40aa56'
        },
        'ClearText.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598592142000000000,
            'mtime': 1598592138000000000,
            'ctime': 1598592138000000000,
            'size': 23,
            'md5': '7a542815e2c51837b3d8a8b2ebf36490'
        }
    }

    MountZipAndCheckTree(zip_name, want_tree, password='password')
    MountZipAndCheckTree(zip_name, want_tree, password='password\n')
    MountZipAndCheckTree(zip_name,
                         want_tree,
                         password='password\nThis line is ignored...\n')

    # With wrong or no password.
    CheckZipMountingError(zip_name, 37, password='wrong password')
    CheckZipMountingError(zip_name, 36)

    want_tree = {
        'Encrypted ZipCrypto.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598594095000000000,
            'mtime': 1598592187000000000,
            'ctime': 1598592188000000000,
            'size': 34,
            'errno': 5
        },
        'Encrypted AES-256.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598594134000000000,
            'mtime': 1598592213000000000,
            'ctime': 1598592214000000000,
            'size': 32,
            'errno': 5
        },
        'Encrypted AES-192.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598594124000000000,
            'mtime': 1598592206000000000,
            'ctime': 1598592206000000000,
            'size': 32,
            'errno': 5
        },
        'Encrypted AES-128.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598594117000000000,
            'mtime': 1598592200000000000,
            'ctime': 1598592200000000000,
            'size': 32,
            'errno': 5
        },
        'ClearText.txt': {
            'mode': '-rw-r-----',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1598592142000000000,
            'mtime': 1598592138000000000,
            'ctime': 1598592138000000000,
            'size': 23,
            'md5': '7a542815e2c51837b3d8a8b2ebf36490'
        }
    }

    MountZipAndCheckTree(zip_name,
                         want_tree,
                         password='wrong password',
                         options=['--force'])
    MountZipAndCheckTree(zip_name, want_tree, options=['--force'])


# Tests mounting ZIP with explicit file name encoding.
def TestZipFileNameEncoding():
    want_tree = {
        'Дата': {
            'mode': '-rw-rw-r--',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1265363324000000000,
            'mtime': 1265363324000000000,
            'ctime': 1265363324000000000,
            'size': 5,
            'md5': 'a9564ebc3289b7a14551baf8ad5ec60a'
        },
        'Текстовый документ.txt': {
            'mode': '-rw-rw-r--',
            'nlink': 1,
            'uid': 0,
            'gid': 0,
            'atime': 1265362564000000000,
            'mtime': 1265362564000000000,
            'ctime': 1265362564000000000,
            'size': 8,
            'md5': 'f75b8179e4bbe7e2b4a074dcef62de95'
        }
    }
    MountZipAndCheckTree('cp866.zip',
                         want_tree,
                         options=['-o', 'encoding=cp866'])

    want_tree = {
        '±≥≤⌠⌡÷≈°∙·√ⁿ²■\xa0\xa0': {
            'size': 0
        },
        'ßΓπΣσμτΦΘΩδ∞φε∩≡': {
            'size': 0
        },
        '╤╥╙╘╒╓╫╪┘┌█▄▌▐▀α': {
            'size': 0
        },
        '┴┬├─┼╞╟╚╔╩╦╠═╬╧╨': {
            'size': 0
        },
        '▒▓│┤╡╢╖╕╣║╗╝╜╛┐└': {
            'size': 0
        },
        'íóúñÑªº¿⌐¬½¼¡«»░': {
            'size': 0
        },
        'æÆôöòûùÿÖÜ¢£¥₧ƒá': {
            'size': 0
        },
        'üéâäàåçêëèïîìÄÅÉ': {
            'size': 0
        },
        'abcdefghijklmnop': {
            'size': 0
        },
        'QRSTUVWXYZ[\\]^_`': {
            'size': 0
        },
        'ABCDEFGHIJKLMNOP': {
            'size': 0
        },
        '123456789:;<=>?@': {
            'size': 0
        },
        '!"#$%&\'()*+,-.': {},
        '!"#$%&\'()*+,-./0': {
            'size': 0
        }
    }
    MountZipAndCheckTree('cp437.zip',
                         want_tree,
                         options=['-o', 'encoding=cp437'])

    del want_tree['ßΓπΣσμτΦΘΩδ∞φε∩≡']
    want_tree.update({
        'ßΓπΣσµτΦΘΩδ∞φε∩≡': {
            'size': 0
        },
        'qrstuvwxyz{|}~⌂Ç': {
            'size': 0
        },
        '◄↕‼¶§▬↨↑↓→←∟↔▲▼ ': {
            'size': 0
        },
        '☺☻♥♦♣♠•◘○◙♂♀♪♫☼►': {
            'size': 0
        }
    })
    MountZipAndCheckTree('cp437.zip',
                         want_tree,
                         options=['-o', 'encoding=libzip'])
    MountZipAndCheckTree('cp437.zip',
                         want_tree,
                         options=['-o', 'encoding=wrong'])


# Tests the nosymlinks, nohardlinks and nospecials mount options.
def TestZipWithSpecialFiles():
    zip_name = 'pkware-specials.zip'

    want_tree = {
        'z-hardlink2': {
            'mode': '-rw-r--r--',
            'nlink': 3,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        },
        'z-hardlink1': {
            'mode': '-rw-r--r--',
            'nlink': 3,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        },
        'z-hardlink-symlink': {
            'mode': 'lrwxrwxrwx',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1566731330000000000,
            'mtime': 1564834729000000000,
            'ctime': 1564809528000000000,
            'target': 'regular'
        },
        'symlink': {
            'mode': 'lrwxrwxrwx',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1566731330000000000,
            'mtime': 1564834729000000000,
            'ctime': 1564809528000000000,
            'target': 'regular'
        },
        'z-hardlink-socket': {
            'mode': 'srw-------',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1564834444000000000,
            'mtime': 1564834444000000000,
            'ctime': 1564809244000000000
        },
        'z-hardlink-fifo': {
            'mode': 'prw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565809123000000000,
            'mtime': 1565809123000000000,
            'ctime': 1565783922000000000
        },
        'z-hardlink-char': {
            'mode': 'crw--w----',
            'nlink': 1,
            'uid': 0,
            'gid': 5,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 1024
        },
        'z-hardlink-block': {
            'mode': 'brw-rw----',
            'nlink': 1,
            'uid': 0,
            'gid': 6,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 2049
        },
        'symlink2': {
            'mode': 'lrwxrwxrwx',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1566731384000000000,
            'mtime': 1566731354000000000,
            'ctime': 1566706154000000000,
            'target': 'regular'
        },
        'socket': {
            'mode': 'srw-------',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1564834444000000000,
            'mtime': 1564834444000000000,
            'ctime': 1564809244000000000
        },
        'regular': {
            'mode': '-rw-r--r--',
            'nlink': 3,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        },
        'fifo': {
            'mode': 'prw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565809123000000000,
            'mtime': 1565809123000000000,
            'ctime': 1565783922000000000
        },
        'char': {
            'mode': 'crw--w----',
            'nlink': 1,
            'uid': 0,
            'gid': 5,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 1024
        },
        'block': {
            'mode': 'brw-rw----',
            'nlink': 1,
            'uid': 0,
            'gid': 6,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 2049
        }
    }

    MountZipAndCheckTree(zip_name, want_tree)

    # Check that the inode numbers of hardlinks match
    got_tree = MountZipAndGetTree(zip_name)
    want_ino = got_tree['regular']['ino']
    if not want_ino > 0:
        LogError(f'Want positive ino, Got: {want_ino}')

    for link_name in ['z-hardlink1', 'z-hardlink2']:
        got_ino = got_tree[link_name]['ino']
        if got_ino != want_ino:
            LogError(f'Want ino: {want_ino}, Got: {got_ino}')

    # Test -o nosymlinks
    want_tree = {
        'z-hardlink2': {
            'mode': '-rw-r--r--',
            'nlink': 3,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        },
        'z-hardlink1': {
            'mode': '-rw-r--r--',
            'nlink': 3,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        },
        'z-hardlink-socket': {
            'mode': 'srw-------',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1564834444000000000,
            'mtime': 1564834444000000000,
            'ctime': 1564809244000000000
        },
        'z-hardlink-fifo': {
            'mode': 'prw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565809123000000000,
            'mtime': 1565809123000000000,
            'ctime': 1565783922000000000
        },
        'z-hardlink-char': {
            'mode': 'crw--w----',
            'nlink': 1,
            'uid': 0,
            'gid': 5,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 1024
        },
        'z-hardlink-block': {
            'mode': 'brw-rw----',
            'nlink': 1,
            'uid': 0,
            'gid': 6,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 2049
        },
        'socket': {
            'mode': 'srw-------',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1564834444000000000,
            'mtime': 1564834444000000000,
            'ctime': 1564809244000000000
        },
        'regular': {
            'mode': '-rw-r--r--',
            'nlink': 3,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        },
        'fifo': {
            'mode': 'prw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565809123000000000,
            'mtime': 1565809123000000000,
            'ctime': 1565783922000000000
        },
        'char': {
            'mode': 'crw--w----',
            'nlink': 1,
            'uid': 0,
            'gid': 5,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 1024
        },
        'block': {
            'mode': 'brw-rw----',
            'nlink': 1,
            'uid': 0,
            'gid': 6,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 2049
        }
    }

    MountZipAndCheckTree(zip_name, want_tree, options=['-o', 'nosymlinks'])

    # Test -o nohardlinks
    want_tree = {
        'z-hardlink-socket': {
            'mode': 'srw-------',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1564834444000000000,
            'mtime': 1564834444000000000,
            'ctime': 1564809244000000000
        },
        'z-hardlink-fifo': {
            'mode': 'prw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565809123000000000,
            'mtime': 1565809123000000000,
            'ctime': 1565783922000000000
        },
        'z-hardlink-char': {
            'mode': 'crw--w----',
            'nlink': 1,
            'uid': 0,
            'gid': 5,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 1024
        },
        'z-hardlink-block': {
            'mode': 'brw-rw----',
            'nlink': 1,
            'uid': 0,
            'gid': 6,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 2049
        },
        'symlink2': {
            'mode': 'lrwxrwxrwx',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1566731384000000000,
            'mtime': 1566731354000000000,
            'ctime': 1566706154000000000,
            'target': 'regular'
        },
        'socket': {
            'mode': 'srw-------',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1564834444000000000,
            'mtime': 1564834444000000000,
            'ctime': 1564809244000000000
        },
        'regular': {
            'mode': '-rw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        },
        'fifo': {
            'mode': 'prw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565809123000000000,
            'mtime': 1565809123000000000,
            'ctime': 1565783922000000000
        },
        'char': {
            'mode': 'crw--w----',
            'nlink': 1,
            'uid': 0,
            'gid': 5,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 1024
        },
        'block': {
            'mode': 'brw-rw----',
            'nlink': 1,
            'uid': 0,
            'gid': 6,
            'atime': 1564833480000000000,
            'mtime': 1564833480000000000,
            'ctime': 1564808280000000000,
            'rdev': 2049
        }
    }

    MountZipAndCheckTree(zip_name, want_tree, options=['-o', 'nohardlinks'])

    # Test -o nospecials
    want_tree = {
        'z-hardlink2': {
            'mode': '-rw-r--r--',
            'nlink': 3,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        },
        'z-hardlink1': {
            'mode': '-rw-r--r--',
            'nlink': 3,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        },
        'z-hardlink-symlink': {
            'mode': 'lrwxrwxrwx',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1566731330000000000,
            'mtime': 1564834729000000000,
            'ctime': 1564809528000000000,
            'target': 'regular'
        },
        'symlink': {
            'mode': 'lrwxrwxrwx',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1566731330000000000,
            'mtime': 1564834729000000000,
            'ctime': 1564809528000000000,
            'target': 'regular'
        },
        'symlink2': {
            'mode': 'lrwxrwxrwx',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1566731384000000000,
            'mtime': 1566731354000000000,
            'ctime': 1566706154000000000,
            'target': 'regular'
        },
        'regular': {
            'mode': '-rw-r--r--',
            'nlink': 3,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        }
    }

    MountZipAndCheckTree(zip_name, want_tree, options=['-o', 'nospecials'])

    # Tests -o nosymlinks nohardlinks and nospecials together
    want_tree = {
        'regular': {
            'mode': '-rw-r--r--',
            'nlink': 1,
            'uid': 1000,
            'gid': 1000,
            'atime': 1565807437000000000,
            'mtime': 1565290018000000000,
            'ctime': 1565264818000000000,
            'size': 32,
            'md5': '456e611a5420b7dd09bae143a7b2deb0'
        }
    }

    MountZipAndCheckTree(zip_name,
                         want_tree,
                         options=['-o', 'nosymlinks,nohardlinks,nospecials'])


# Tests invalid and absent ZIP archives.
def TestInvalidZip():
    CheckZipMountingError('', 38)
    CheckZipMountingError('absent.zip', 19)
    CheckZipMountingError('invalid.zip', 29)
    with tempfile.NamedTemporaryFile() as f:
        os.chmod(f.name, 0)
        CheckZipMountingError(f.name, 21)


logging.getLogger().setLevel('INFO')

TestZipWithDefaultOptions()
TestZipFileNameEncoding()
TestZipWithSpecialFiles()
TestEncryptedZip()
TestInvalidZip()
TestZipWithManyFiles()
TestBigZip()
TestBigZipNoCache()

if error_count:
    LogError(f'There were {error_count} errors')
    sys.exit(1)
else:
    logging.info('All tests passed Ok')

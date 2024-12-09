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

  def scan(path, st):
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
        if use_md5:
          line['md5'] = md5(path)
      except OSError as e:
        line['errno'] = e.errno
    elif stat.S_ISLNK(mode):
      line['target'] = os.readlink(path)
    elif stat.S_ISBLK(mode) or stat.S_ISCHR(mode):
      line['rdev'] = st.st_rdev
    elif stat.S_ISDIR(mode):
      for entry in os.scandir(path):
        scan(entry.path, entry.stat(follow_symlinks=False))

  scan(root, os.stat(root, follow_symlinks=False))
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
              f'Mismatch for {name!r}[{key}] got: {got_value!r}, want:'
              f' {want_value!r}'
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
# Returns a pair where:
# - member 0 is a dict representing the mounted ZIP.
# - member 1 is the result of os.statvfs
#
# Throws subprocess.CalledProcessError if the ZIP cannot be mounted.
def MountZipAndGetTree(zip_name, options=[], password='', use_md5=True):
  with tempfile.TemporaryDirectory() as mount_point:
    zip_path = os.path.join(script_dir, 'data', zip_name)
    logging.debug(f'Mounting {zip_path!r} on {mount_point!r}...')
    subprocess.run(
        [mount_program, *options, zip_path, mount_point],
        check=True,
        capture_output=True,
        input=password,
        encoding='UTF-8',
    )
    try:
      logging.debug(f'Mounted ZIP {zip_path!r} on {mount_point!r}')
      return GetTree(mount_point, use_md5=use_md5), os.statvfs(mount_point)
    finally:
      logging.debug(f'Unmounting {zip_path!r} from {mount_point!r}...')
      subprocess.run(['fusermount', '-u', '-z', mount_point], check=True)
      logging.debug(f'Unmounted {zip_path!r} from {mount_point!r}')


# Mounts the given ZIP archive, checks the mounted ZIP tree and unmounts.
# Logs an error if the ZIP cannot be mounted.
def MountZipAndCheckTree(
    zip_name,
    want_tree,
    want_blocks=None,
    want_inodes=None,
    options=[],
    password='',
    strict=True,
    use_md5=True,
):
  s = f'Test {zip_name!r}'
  if options:
    s += f', options = {" ".join(options)!r}'
  if password:
    s += f', password = {password!r}'
  logging.info(s)
  try:
    got_tree, st = MountZipAndGetTree(
        zip_name, options=options, password=password, use_md5=use_md5
    )

    want_block_size = 512
    if st.f_bsize != want_block_size:
      LogError(
          f'Mismatch for st.f_bsize: got: {st.f_bsize}, want: {want_block_size}'
      )
    if st.f_frsize != want_block_size:
      LogError(
          'Mismatch for st.f_frsize: '
          f'got: {st.f_frsize}, want: {want_block_size}'
      )

    want_name_max = 255
    if st.f_namemax != want_name_max:
      LogError(
          'Mismatch for st.f_namemax: '
          f'got: {st.f_namemax}, want: {want_name_max}'
      )

    if want_blocks is not None and st.f_blocks != want_blocks:
      LogError(
          f'Mismatch for st.f_blocks: got: {st.f_blocks}, want: {want_blocks}'
      )

    if want_inodes is not None and st.f_files != want_inodes:
      LogError(
          f'Mismatch for st.f_files: got: {st.f_files}, want: {want_inodes}'
      )

    CheckTree(got_tree, want_tree, strict=strict)
  except subprocess.CalledProcessError as e:
    LogError(f'Cannot test {zip_name}: {e.stderr}')


# Try to mount the given ZIP archive, and expects an error.
# Logs an error if the ZIP can be mounted, or if the returned error code doesn't match.
def CheckZipMountingError(zip_name, want_error_code, options=[], password=''):
  s = f'Test {zip_name!r}'
  if options:
    s += f', options = {" ".join(options)!r}'
  if password:
    s += f', password = {password!r}'
  logging.info(s)
  try:
    got_tree, _ = MountZipAndGetTree(
        zip_name, options=options, password=password
    )
    LogError(f'Want error, Got tree: {got_tree}')
  except subprocess.CalledProcessError as e:
    if e.returncode != want_error_code:
      LogError(
          f'Want error: {want_error_code}, Got error: {e.returncode} in {e}'
      )


def GenerateReferenceData():
  for zip_name in os.listdir(os.path.join(script_dir, 'data')):
    tree, _ = MountZipAndGetTree(zip_name, password='password')
    all_zips[zip_name] = tree

  pprint.pprint(all_zips, compact=True, sort_dicts=False)


# Tests most of the ZIP files in data_dir using default mounting options.
def TestZipWithDefaultOptions():
  want_trees = {
      'absolute-path.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'ROOT': {'nlink': 2},
          'ROOT/rootname.ext': {
              'nlink': 1,
              'atime': 1371478408000000000,
              'mtime': 1371478408000000000,
              'ctime': 1371478408000000000,
              'size': 22,
              'md5': '8b4cc90d421780e7674e2a25db33b770',
          },
      },
      'bad-archive.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'bash.txt': {
              'nlink': 1,
              'atime': 1265257350000000000,
              'mtime': 1265257351000000000,
              'ctime': 1265228552000000000,
              'size': 282292,
              'errno': 5,
          }
      },
      'bad-crc.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'bash.txt': {
              'nlink': 1,
              'atime': 1265262326000000000,
              'mtime': 1265257351000000000,
              'ctime': 1265228552000000000,
              'size': 282292,
              'errno': 5,
          }
      },
      'bzip2.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'bzip2.txt': {
              'nlink': 1,
              'atime': 1635811418000000000,
              'mtime': 1635811418000000000,
              'ctime': 1635811418000000000,
              'size': 36,
              'md5': 'f28623c0be8da636e6c53ead06ce0731',
          }
      },
      'comment-utf8.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'dir': {
              'nlink': 2,
              'atime': 1563727175000000000,
              'mtime': 1563727169000000000,
              'ctime': 1563701970000000000,
          },
          'dir/empty_comment.txt': {
              'nlink': 1,
              'atime': 1563727148000000000,
              'mtime': 1563721364000000000,
              'ctime': 1563696164000000000,
              'size': 14,
              'md5': '3453ff2f4d15a71d21e859829f0da9fc',
          },
          'dir/with_comment.txt': {
              'nlink': 1,
              'atime': 1563727148000000000,
              'mtime': 1563720300000000000,
              'ctime': 1563695100000000000,
              'size': 13,
              'md5': '557fcaa19da1b3e2cd6ce8e546a13f46',
          },
          'dir/without_comment.txt': {
              'nlink': 1,
              'atime': 1563727235000000000,
              'mtime': 1563720306000000000,
              'ctime': 1563695106000000000,
              'size': 16,
              'md5': '4cdf349a6dc1516f9642ebaf278af394',
          },
      },
      'comment.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'dir': {
              'nlink': 2,
              'atime': 1563727175000000000,
              'mtime': 1563727169000000000,
              'ctime': 1563701970000000000,
          },
          'dir/empty_comment.txt': {
              'nlink': 1,
              'atime': 1563727148000000000,
              'mtime': 1563721364000000000,
              'ctime': 1563696164000000000,
              'size': 14,
              'md5': '3453ff2f4d15a71d21e859829f0da9fc',
          },
          'dir/with_comment.txt': {
              'nlink': 1,
              'atime': 1563727148000000000,
              'mtime': 1563720300000000000,
              'ctime': 1563695100000000000,
              'size': 13,
              'md5': '557fcaa19da1b3e2cd6ce8e546a13f46',
          },
          'dir/without_comment.txt': {
              'nlink': 1,
              'atime': 1563727148000000000,
              'mtime': 1563720306000000000,
              'ctime': 1563695106000000000,
              'size': 16,
              'md5': '4cdf349a6dc1516f9642ebaf278af394',
          },
      },
      'dos-perm.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'dir': {'mode': 'drwxr-xr-x', 'nlink': 2},
          'dir/hidden.txt': {
              'nlink': 1,
              'atime': 1388826134000000000,
              'mtime': 1388826134000000000,
              'ctime': 1388826134000000000,
              'size': 11,
              'md5': 'fa29ea74a635e3be468256f9007b1bb6',
          },
          'dir/normal.txt': {
              'nlink': 1,
              'atime': 1388826120000000000,
              'mtime': 1388826120000000000,
              'ctime': 1388826120000000000,
              'size': 11,
              'md5': '050256c2ac1d77494b9fddaa933f5eda',
          },
          'dir/readonly.txt': {
              'nlink': 1,
              'atime': 1388826146000000000,
              'mtime': 1388826146000000000,
              'ctime': 1388826146000000000,
              'size': 14,
              'md5': '6f651ad751dadd4e76c8f46b6fae0c48',
          },
      },
      'empty.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
      },
      'extrafld.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'README': {
              'nlink': 1,
              'atime': 1388831602000000000,
              'mtime': 1372483540000000000,
              'ctime': 1372461940000000000,
              'size': 2551,
              'md5': '27d03e3bb5ed8add7917810c3ba68836',
          }
      },
      'fifo.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          '-': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1564403758000000000,
              'mtime': 1564403758000000000,
              'ctime': 1564403758000000000,
              'size': 14,
              'md5': '42e16527aee5fe43ffa78a89d36a244c',
          },
          'fifo': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1564428868000000000,
              'mtime': 1564428868000000000,
              'ctime': 1564403668000000000,
              'size': 13,
              'md5': 'd36dcce71b10bb7dd348ab6efb1c4aab',
          },
      },
      'file-dir-same-name.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'pet': {'nlink': 3},
          'pet/cat': {'nlink': 3},
          'pet/cat/fish': {
              'nlink': 2,
              'atime': 1635479490000000000,
              'mtime': 1635479490000000000,
              'ctime': 1635479490000000000,
          },
          'pet/cat/fish (1)': {
              'nlink': 1,
              'atime': 1635479490000000000,
              'mtime': 1635479490000000000,
              'ctime': 1635479490000000000,
              'size': 30,
              'md5': '47661a04dfd111f95ec5b02c4a1dab05',
          },
          'pet/cat/fish (2)': {
              'nlink': 1,
              'atime': 1635479490000000000,
              'mtime': 1635479490000000000,
              'ctime': 1635479490000000000,
              'size': 31,
              'md5': '4969fad4edba3582a114f17000583344',
          },
          'pet/cat (1)': {
              'nlink': 1,
              'atime': 1635479490000000000,
              'mtime': 1635479490000000000,
              'ctime': 1635479490000000000,
              'size': 25,
              'md5': '3a3cd8d02b1a2a232e2684258f38c882',
          },
          'pet/cat (2)': {
              'nlink': 1,
              'atime': 1635479490000000000,
              'mtime': 1635479490000000000,
              'ctime': 1635479490000000000,
              'size': 26,
              'md5': '07691c6ecc5be713b8d1224446f20970',
          },
          'pet (1)': {
              'nlink': 1,
              'atime': 1635479490000000000,
              'mtime': 1635479490000000000,
              'ctime': 1635479490000000000,
              'size': 21,
              'md5': '185743d56c5a917f02e2193a96effb25',
          },
          'pet (2)': {
              'nlink': 1,
              'atime': 1635479490000000000,
              'mtime': 1635479490000000000,
              'ctime': 1635479490000000000,
              'size': 22,
              'md5': 'ec5ffb0de216fb1e9578bc17a169399a',
          },
      },
      'foobar.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'foo': {
              'nlink': 1,
              'atime': 1565484162000000000,
              'mtime': 1565484162000000000,
              'ctime': 1565458962000000000,
              'size': 4,
              'md5': 'c157a79031e1c40f85931829bc5fc552',
          }
      },
      'hlink-before-target.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          '0hlink': {
              'nlink': 2,
              'atime': 1565781818000000000,
              'mtime': 1565781818000000000,
              'ctime': 1565781818000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
          '1regular': {
              'nlink': 2,
              'atime': 1565781818000000000,
              'mtime': 1565781818000000000,
              'ctime': 1565781818000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
      },
      'hlink-chain.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          '0regular': {
              'nlink': 3,
              'atime': 1565781818000000000,
              'mtime': 1565781818000000000,
              'ctime': 1565781818000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
          'hlink1': {
              'nlink': 3,
              'atime': 1565781818000000000,
              'mtime': 1565781818000000000,
              'ctime': 1565781818000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
          'hlink2': {
              'nlink': 3,
              'atime': 1565781818000000000,
              'mtime': 1565781818000000000,
              'ctime': 1565781818000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
      },
      'hlink-different-types.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'dir': {
              'nlink': 2,
              'atime': 1565781818000000000,
              'mtime': 1565781818000000000,
              'ctime': 1565781818000000000,
          },
          'dir/regular': {
              'nlink': 1,
              'atime': 1565781818000000000,
              'mtime': 1565781818000000000,
              'ctime': 1565781818000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
          'hlink': {
              'nlink': 1,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          },
      },
      'hlink-dir.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 4},
          'dir': {
              'nlink': 2,
              'atime': 1567323446000000000,
              'mtime': 1567323446000000000,
              'ctime': 1567323446000000000,
          },
          'dir/regular': {
              'nlink': 1,
              'atime': 1567323446000000000,
              'mtime': 1567323446000000000,
              'ctime': 1567323446000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
          'hlink': {
              'nlink': 2,
              'atime': 1567348646000000000,
              'mtime': 1567348646000000000,
              'ctime': 1567323446000000000,
          },
      },
      'hlink-recursive-one.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'hlink': {
              'nlink': 1,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          }
      },
      'hlink-recursive-two.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'hlink1': {
              'nlink': 2,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          },
          'hlink2': {
              'nlink': 2,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          },
      },
      'hlink-relative.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'UP': {'nlink': 2},
          'UP/0regular': {
              'nlink': 2,
              'atime': 1567323446000000000,
              'mtime': 1567323446000000000,
              'ctime': 1567323446000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
          'UP/hlink': {
              'nlink': 2,
              'atime': 1567323446000000000,
              'mtime': 1567323446000000000,
              'ctime': 1567323446000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
      },
      'hlink-special.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          '0block': {
              'mode': 'brw-r--r--',
              'nlink': 1,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
              'rdev': 2303,
          },
          '0fifo': {
              'mode': 'prw-r--r--',
              'nlink': 2,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
          },
          '0socket': {
              'mode': 'srw-r--r--',
              'nlink': 2,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
          },
          'hlink-block': {
              'mode': 'brw-r--r--',
              'nlink': 1,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
              'rdev': 2303,
          },
          'hlink-fifo': {
              'mode': 'prw-r--r--',
              'nlink': 2,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
          },
          'hlink-socket': {
              'mode': 'srw-r--r--',
              'nlink': 2,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
          },
      },
      'hlink-symlink.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          '0regular': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1565781818000000000,
              'mtime': 1565781818000000000,
              'ctime': 1565781818000000000,
              'size': 10,
              'md5': 'e09c80c42fda55f9d992e59ca6b3307d',
          },
          '1symlink': {
              'mode': 'lrwxr-xr-x',
              'nlink': 2,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
              'target': '0regular',
          },
          'hlink': {
              'mode': 'lrwxr-xr-x',
              'nlink': 2,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
              'target': '0regular',
          },
      },
      'hlink-without-target.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'hlink1': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1565807019000000000,
              'mtime': 1565807019000000000,
              'ctime': 1565781818000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          }
      },
      'issue-43.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'README': {
              'nlink': 1,
              'atime': 1425892392000000000,
              'mtime': 1265521877000000000,
              'ctime': 1265493078000000000,
              'size': 760,
              'md5': 'f196d610d1cdf9191b4440863e8d31ab',
          },
          'a': {'nlink': 3},
          'a/b': {'nlink': 3},
          'a/b/c': {'nlink': 3},
          'a/b/c/d': {'nlink': 3},
          'a/b/c/d/e': {'nlink': 3},
          'a/b/c/d/e/f': {'nlink': 2},
          'a/b/c/d/e/f/g': {
              'nlink': 1,
              'atime': 1425893690000000000,
              'mtime': 1425893690000000000,
              'ctime': 1425864890000000000,
              'size': 32,
              'md5': '21977dc7948b88fdefd50f77afc9ac7b',
          },
          'a/b/c/d/e/f/g2': {
              'nlink': 1,
              'atime': 1425893719000000000,
              'mtime': 1425893719000000000,
              'ctime': 1425864920000000000,
              'size': 32,
              'md5': '74ad09827032bc0be935c23c53f8ee29',
          },
      },
      'lzma.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'lzma.txt': {
              'nlink': 1,
              'atime': 1635812732000000000,
              'mtime': 1635812732000000000,
              'ctime': 1635812732000000000,
              'size': 35,
              # 'md5': 'afcc577cec75357c61cc360e3bca0ac6',
          }
      },
      'mixed-paths.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 7},
          'CUR': {},
          "CUR/Quote ' (1)": {'md5': '3ca0f2a7d572f3ad256fcd13e39bd8da'},
          "CUR/Quote ' (2)": {'md5': '60e20f9b84bf0fe96e7d226341aaf72d'},
          "CUR/Quote '": {'md5': '71e103fda7cffdb58b4aa44fa540efce'},
          'CUR/  (1)': {'md5': 'fc520ede1760d4e64770b48ba8f859fb'},
          'CUR/  (2)': {'md5': '9d0c61de9c0cdc3aec1221d3b00f6af1'},
          'CUR/ ': {'md5': '4ecf69a9cb2cce4469fbea4cab35277d'},
          'CUR/ ←Space (1)': {'md5': '42f00634c4115c5f67d9be6a1cfa414c'},
          'CUR/ ←Space (2)': {'md5': 'a4506a1c46eaab6a62de9d5ddb80666b'},
          'CUR/ ←Space': {'md5': '9f407c9493c1ccc2458ab776b0e3efcf'},
          'CUR/$HOME (1)': {'md5': 'c41dc9371b2be069bd8e013c428ace13'},
          'CUR/$HOME (2)': {'md5': 'e40b01b5ff1cc66197d3428919a18da4'},
          'CUR/$HOME': {'md5': 'aedeaeef1d21c8bb6afe8c1252f7d3c8'},
          'CUR/%TMP% (1)': {'md5': '8ecc4766c79654383ed3e1ec4cefd5ce'},
          'CUR/%TMP% (2)': {'md5': 'edb312c5d6d077b0a27f5bf1e7117206'},
          'CUR/%TMP%': {'md5': 'a0e10b52aca6cc8b673d6eeb72f2f95f'},
          'CUR/- (1)': {'md5': '704efc6cc347fcdf01585e2ee91c80fa'},
          'CUR/- (2)': {'md5': '6c561d06c7890a70215c3f253cb6bb02'},
          'CUR/-': {'md5': '0015ecf55d7da11c150a666e0583c9fa'},
          'CUR/... (1)': {'md5': '24b8fb13b27ae0e9d0da9cc411c279c6'},
          'CUR/... (2)': {'md5': '05e8526f15d1bee44e8e0f526fb93f3a'},
          'CUR/... (3)': {'md5': 'd554ecc8f6a782ab311a81f38b095ef5'},
          'CUR/... (4)': {'md5': '7a51680c3febd5563773fe9a8090ee73'},
          'CUR/... (5)': {'md5': '66a818b0b78bd22bc33f330de5101c00'},
          'CUR/...': {'md5': 'b246079eb0c241c0ad50a30873701b7d'},
          'CUR/.... (1)': {'md5': 'bf4189b1f38be5379b3c8e330e14ef00'},
          'CUR/.... (2)': {'md5': 'c0ddf361e9cce0a3d1ce734023c99852'},
          'CUR/....': {'md5': '51cd1872e35401de1969f1c53c32332a'},
          'CUR/' + '.' * 251 + ' (1)': {'md5': 'd73c3af2bb73f18416eae424d2bb1226'},
          'CUR/' + '.' * 251 + ' (2)': {'md5': 'ab6a815c651781866cf22f96699954cb'},
          'CUR/' + '.' * 251 + ' (3)': {'md5': '35eef262524368b8c99d28d1b9018629'},
          'CUR/' + '.' * 251 + ' (4)': {'md5': '804600f4aa79357fe896a10773bf78fa'},
          'CUR/' + '.' * 251 + ' (5)': {'md5': '707b9ac9459e6dda9da9abd0957a21bc'},
          'CUR/' + '.' * 255: {'md5': 'cc3b7dadd6280c4586748318e2ee6b1e'},
          'CUR/...Three (1)': {'md5': '205a4fa1968f1a05d90a5810d7daea5a'},
          'CUR/...Three (2)': {'md5': 'fed163fdc049f5ba0347e27de9e3b75e'},
          'CUR/...Three (3)': {'md5': 'be8c7e5c8149543bfb0b899a63dfd3a5'},
          'CUR/...Three (4)': {'md5': 'a32371c48bdd02833ececf98ab629ff1'},
          'CUR/...Three (5)': {'md5': '2cdb1981a75f35b3e2c7b35ad04aa461'},
          'CUR/...Three': {'md5': 'ea9951c389c3bcd6355df418e6338d86'},
          'CUR/..Two (1)': {'md5': 'e2bafe9b9dab8502c8dd91a7cd304aca'},
          'CUR/..Two (2)': {'md5': '6372fc10e660a6bf413b9fe3e52cf6df'},
          'CUR/..Two': {'md5': '3676540e9ef52c8fec43c1705e547270'},
          'CUR/.One (1)': {'md5': '695b92c19227b154a9ad4c7454e60954'},
          'CUR/.One (2)': {'md5': '69ef0885b6ec5b8434bf67767b061924'},
          'CUR/.One': {'md5': '39c89ab7f825d93e11273cee816983d1'},
          'CUR/.foo (1).txt': {'md5': '8ff4eea96f318c3cbe8e6a713c8ad8af'},
          'CUR/.foo (2).txt': {'md5': '1c758452430d22a7bb54798873ea854f'},
          'CUR/.foo (3).txt': {'md5': '88a416d7c9817c188596d3dcb553c9ca'},
          'CUR/.foo (4).txt': {'md5': '568de72385d3ccc1d21fa2ccf4a1517f'},
          'CUR/.foo (5).txt': {'md5': '634a6c97ee76c4032c06f0769f0601b6'},
          'CUR/.foo.txt': {'md5': '52a1b1c7d65f4d9cdd41425f861150f7'},
          'CUR/AUX (1)': {'md5': '7fdc6b9a4f48109c19455764fad5f7a0'},
          'CUR/AUX (2)': {'md5': '6b02202ccdafa51eae1f6425d608709c'},
          'CUR/AUX': {'md5': '3f69a27bc959159d6911d1d2f44ecfb5'},
          'CUR/Ampersand & (1)': {'md5': '7ddd78b542607296a9a5e93745b82297'},
          'CUR/Ampersand & (2)': {'md5': '31184f350acc30d15ffac762acde7304'},
          'CUR/Ampersand &': {'md5': '255622e1d13e054c5b0e9365c9a723bf'},
          'CUR/Angle <> (1)': {'md5': '36fbea036273dc1917a3e7e3b573dd22'},
          'CUR/Angle <> (2)': {'md5': 'c98dc6a537865f0896db87452784280b'},
          'CUR/Angle <>': {'md5': '1c5cbe4d86c73de115eb48de5cf0eeea'},
          'CUR/At @ (1)': {'md5': 'dccf5a36da117a1b9ea52f6aa1d46dca'},
          'CUR/At @ (2)': {'md5': '40d05f890dcda5552b7b87f1b1223b1a'},
          'CUR/At @': {'md5': 'cabcc9ad22fd30b2fe3220785474d9d8'},
          'CUR/Backslash \\ (1)': {'md5': 'd916ebecb74db316f76c43cdc53d813a'},
          'CUR/Backslash \\ (2)': {'md5': '2eb14342696819b71240ee370bb11258'},
          'CUR/Backslash \\': {'md5': 'e88e6eb289064bc8cabd711310342d83'},
          'CUR/Backspace ◘ (1)': {'md5': 'a14e44a642e9037c3c298348093ec380'},
          'CUR/Backspace ◘ (2)': {'md5': 'a52f6c6706cfe008497efe714eb2a5ff'},
          'CUR/Backspace ◘': {'md5': '5d2071685575754babe374981552164b'},
          'CUR/Backtick ` (1)': {'md5': 'b92aae294d43c3e081397788bcdeda77'},
          'CUR/Backtick ` (2)': {'md5': '714d2283f6384ffabcf1c403ad0ebb3e'},
          'CUR/Backtick `': {'md5': '44a52fddfbaf7c0c213c20192744afd5'},
          'CUR/Bell • (1)': {'md5': 'd42111a8d9d715d6ca13f870c03bb136'},
          'CUR/Bell • (2)': {'md5': 'a9e9d87ea96b4f4e8797820c7ac19df1'},
          'CUR/Bell •': {'md5': 'aafdd252197512170d856086452836a9'},
          'CUR/C:\\Temp\\File (1)': {'md5': 'ca11bb68c069615a4b9b6eecedae436b'},
          'CUR/C:\\Temp\\File (2)': {'md5': 'de6cf0f9e21e500d452ca1731148a774'},
          'CUR/C:\\Temp\\File': {'md5': '09e89cca300f6ad14c9c614fb95c33b0'},
          'CUR/CASE (1)': {'md5': '00b78d79abf97077fbf025f3355fddb2'},
          'CUR/CASE (2)': {'md5': '53e44ae7ecdff196ff18881fae2a4c31'},
          'CUR/CASE': {'md5': 'e92836e4b17a39306f51d13ead0a09e4'},
          'CUR/CLOCK$ (1)': {'md5': '4a89d1ec340b7984fce1828f32b17a1b'},
          'CUR/CLOCK$ (2)': {'md5': 'a10ee44c95468f79c4d76acbc431f2d9'},
          'CUR/CLOCK$': {'md5': '306be585c84e6ed5aab5070e9846121d'},
          'CUR/COM1 (1)': {'md5': '7126112f4c33e7530fffcf7278a5e719'},
          'CUR/COM1 (2)': {'md5': '619a4330a4dd8433f2b27ceb9e2c575c'},
          'CUR/COM1': {'md5': 'a613354228a8154a5c980526c82d7efe'},
          'CUR/COM9 (1)': {'md5': '7c904dcc4bb875a99e403d6a41d97c2a'},
          'CUR/COM9 (2)': {'md5': 'c0a1e5c28a2359f61cc4531df14d0892'},
          'CUR/COM9': {'md5': '413d4b59327743bb2d8ce59f41fd1f41'},
          'CUR/CON (1)': {'md5': 'a6a5ee67a986dc6270311f631c2d403d'},
          'CUR/CON (2)': {'md5': '66673cda9a9e98f3ab3f7194ead33958'},
          'CUR/CON': {'md5': '407f3ff633ac1822974ce5e1a07ac9e5'},
          'CUR/Café (1)': {'md5': 'e9e59d2978d1ffcfc11274fcedea35d6'},
          'CUR/Café (2)': {'md5': '4f51f74e22dd4e3a77f965d77daffb4b'},
          'CUR/Café': {'md5': 'b7ce2be1dfb8cf535bccf1036284900c'},
          'CUR/Café (1)': {'md5': '637f113e67522f774879f6f63f3093da'},
          'CUR/Café (2)': {'md5': 'f60233605389c3fc8ba841d202860c38'},
          'CUR/Café': {'md5': '080f684e7afffcc022c53359c1b6d968'},
          'CUR/Caret ^ (1)': {'md5': '6ecbe85c819de33f1a1566fc6a49b148'},
          'CUR/Caret ^ (2)': {'md5': '4037bf95d8b2a026056c6be3cb88f16d'},
          'CUR/Caret ^': {'md5': '327e8a9bae15530ada822c20e6e875f2'},
          'CUR/Carriage Return \r (1)': {'md5': '1657ca6d389450c66dcb3737ade632d4'},
          'CUR/Carriage Return \r (2)': {'md5': 'f5f79fbe6bd369bb344d2acb1500f3a0'},
          'CUR/Carriage Return \r': {'md5': '5b5600d7515e86d01364bc6f066cfc14'},
          'CUR/Case (1)': {'md5': 'b33984146a2bb8c8fc337362e91d1911'},
          'CUR/Case (2)': {'md5': '2653ca3273626a97a5b9155b83511e44'},
          'CUR/Case': {'md5': 'f39791e31c562ce31a325a188c405d02'},
          'CUR/Colon : (1)': {'md5': '662fcf045861ca1e9be6466f34f23846'},
          'CUR/Colon : (2)': {'md5': '30b9479836d7aab4428db84a4435de2b'},
          'CUR/Colon :': {'md5': 'bad98d8795bd453e675ae45cf511cb6f'},
          'CUR/Comma , (1)': {'md5': '741313ce46594c276d4dbf8c50a3c242'},
          'CUR/Comma , (2)': {'md5': 'dbe2d9051ca9c7ad197acdd39644c151'},
          'CUR/Comma ,': {'md5': '8e4e2a35ea3db7e1f0c71e9e260e3f2b'},
          'CUR/Curly {} (1)': {'md5': '5e75818b995fac62fd10046e918a6d68'},
          'CUR/Curly {} (2)': {'md5': '2526b1e0fdb6a56ef9c7104b0432295a'},
          'CUR/Curly {}': {'md5': '7f80122391b3c4f8af113d08784576bb'},
          'CUR/Dash - (1)': {'md5': 'c2fe85e07ed29f1907466647d8e7de73'},
          'CUR/Dash - (2)': {'md5': '394758376fd6bece3d8c523911d4802f'},
          'CUR/Dash -': {'md5': '367fcecd09ee039d04346ca9483f15b0'},
          'CUR/Delete \x7f (1)': {'md5': '63f936c1f9b6679f6448d6b9dd6907e9'},
          'CUR/Delete \x7f (2)': {'md5': '1e5f71a3210f3572ad57cd0f8db7c773'},
          'CUR/Delete \x7f': {'md5': '3ffe534d559861937f43e74252183f7d'},
          'CUR/Dollar $ (1)': {'md5': '785fcb195fd44a2d958ab601533aaa93'},
          'CUR/Dollar $ (2)': {'md5': 'ad033aafabafeeece964f7558f5e0110'},
          'CUR/Dollar $': {'md5': '8e567128bd3120c6b504f0ea1c078591'},
          'CUR/Dot . (1)': {'md5': 'd6bbeadd3e2949c9a97863faf7941fd1'},
          'CUR/Dot . (2)': {'md5': 'ba3e606d9d97dcaa1903c77b4caa62a7'},
          'CUR/Dot .': {'md5': 'f312d5c330e833d409ad05fe206b3099'},
          'CUR/Double quote " (1)': {'md5': '843b0fa6d0bd93d58b5a1a0960c4be2f'},
          'CUR/Double quote " (2)': {'md5': '94912b944529fe9ac74f1d17ffd685ed'},
          'CUR/Double quote "': {'md5': '7b20185b51dbce9398dfbe5b3c5c2f44'},
          'CUR/Empty': {},
          'CUR/Equal = (1)': {'md5': '83379a04a3ee4566eb3605bc3f5a4ab4'},
          'CUR/Equal = (2)': {'md5': '7891ccb015f7c6daec40bbd16c8075ec'},
          'CUR/Equal =': {'md5': '69b9497f9f34aba975699c833d037666'},
          'CUR/Escape ← (1)': {'md5': '9e3c4cefd408009286d5b418ae9988db'},
          'CUR/Escape ← (2)': {'md5': '8e7878beac19e997da1ef12bd481cb44'},
          'CUR/Escape ←': {'md5': '1c1ee3412c62f438d4403816482ed486'},
          'CUR/Euro € (1)': {'md5': '54dd622a6f10eed2cd59ed2a6f594ef2'},
          'CUR/Euro € (2)': {'md5': '3ee86efa36d8cf102288ad2bf936dfa8'},
          'CUR/Euro €': {'md5': '8e79df94fcaf890f6433ecf48152919d'},
          'CUR/Exclamation ! (1)': {'md5': '10e762da30d7793ee14e7bf0c154078b'},
          'CUR/Exclamation ! (2)': {'md5': '22520ad75ab8d3b28e90fd7a4fc2bfc8'},
          'CUR/Exclamation !': {'md5': '255abe4fe8bbb0b4f8411913673388fe'},
          'CUR/FileOrDir (1)': {'md5': '616efa472d51b7f5aacab007d9a635be'},
          'CUR/FileOrDir (2)': {'md5': 'be82b39076b03bcf3828ab09b1c34582'},
          'CUR/FileOrDir (3)': {'md5': 'e503e8e89925339aefb47443581ba4bc'},
          'CUR/FileOrDir': {},
          'CUR/Hash # (1)': {'md5': '37715cd1852064027118eb0e466d1172'},
          'CUR/Hash # (2)': {'md5': '38730629648b8780fe1f4f90738eb6a1'},
          'CUR/Hash #': {'md5': '2c33603f6b59836dc9dd61bbd6f47b6d'},
          'CUR/LPT1 (1)': {'md5': '695771298fcc161d5375e7ef65fe0cbf'},
          'CUR/LPT1 (2)': {'md5': '4c6478198627fe5d5d8ca588782502ea'},
          'CUR/LPT1': {'md5': 'e9aa40253c2cda7319f60e127b7a5d2b'},
          'CUR/LPT9 (1)': {'md5': '338ad2a4d01b83966a2d93b84991079c'},
          'CUR/LPT9 (2)': {'md5': 'a5ee3dd960f9c0457cc778afdc1bc45e'},
          'CUR/LPT9': {'md5': 'b5908fed9f25a430cc17f41058517fd7'},
          'CUR/Line Feed \n (1)': {'md5': '3e1c022c4be1b6d982289cbd6aeb9eba'},
          'CUR/Line Feed \n (2)': {'md5': 'fc8dfff4cc4757c29ab931d9e7f954e9'},
          'CUR/Line Feed \n': {'md5': '72af886a4aed8ad6885a7f786ec5b661'},
          'CUR/NUL (1)': {'md5': '95c40e86277b9e90a040c3b302d7562c'},
          'CUR/NUL (2)': {'md5': 'd6516225315fb534b075c396016ca039'},
          'CUR/NUL': {'md5': '485ca989764c13cf55f8ab3d839cfd1e'},
          'CUR/One. (1)': {'md5': 'a85a42df93c1b1365a6a593e07a3f80a'},
          'CUR/One. (2)': {'md5': 'a05d70a56ff44a1fa4b42a55d5a29c19'},
          'CUR/One.': {'md5': 'ab394e10e5ef36efedc9e415c2c3cb42'},
          'CUR/PRN (1)': {'md5': '193b412ab1a91011b5ea7accb3c146c2'},
          'CUR/PRN (2)': {'md5': 'dc5ef433ccf07082e793a71e17dd2b1f'},
          'CUR/PRN': {'md5': 'c6f96d5f3a7313541646d5d8b951dc0d'},
          'CUR/Percent % (1)': {'md5': 'c8dd7a81eaf6d6c8e817657aa45063ef'},
          'CUR/Percent % (2)': {'md5': '5dca4785fee3d3cf8b7a090555409e1a'},
          'CUR/Percent %': {'md5': 'bc402953f32f33d3e8c17360557bd294'},
          'CUR/Pipe | (1)': {'md5': '8f2eaf2f601cfc28087f0826c0d0415d'},
          'CUR/Pipe | (2)': {'md5': '715f63380171ee77dd2057cff284edd7'},
          'CUR/Pipe |': {'md5': '6dfa11c10b119a6f0bea267804707f59'},
          'CUR/Plus + (1)': {'md5': '00a706dba456a2da0c8175498c1c2e0a'},
          'CUR/Plus + (2)': {'md5': '9dfada89dcbd35eb95e6d6882f4eb79b'},
          'CUR/Plus +': {'md5': '2517d92c6a28e82e53086558f599f7a3'},
          'CUR/Question ? (1)': {'md5': '2b04a6c5990e28e981052c2a1b487891'},
          'CUR/Question ? (2)': {'md5': '96ae8325f90df4cfb401b3d40c65417d'},
          'CUR/Question ?': {'md5': '4762c2deeccd2243cbe2c750f6027608'},
          'CUR/Round () (1)': {'md5': '66a98a72d65a794b288469d1b9f5a9c7'},
          'CUR/Round () (2)': {'md5': '347ea63daa56787225c2575a277fefcf'},
          'CUR/Round ()': {'md5': 'ea2053ca1e8235aec047f2debca62161'},
          'CUR/Semicolon ; (1)': {'md5': '3ee36aec082f0d0c9f89145d4d3081f8'},
          'CUR/Semicolon ; (2)': {'md5': '7977cacb02ea2bbb75517dde267e6904'},
          'CUR/Semicolon ;': {'md5': '789f0142366e24b2ce19c7248b3c1103'},
          'CUR/Smile 🙂 (1)': {'md5': 'bafa29d272d040544572ab3b4e5cc497'},
          'CUR/Smile 🙂 (2)': {'md5': '581599352cd9dda6a541481952d06048'},
          'CUR/Smile 🙂': {'md5': 'a870c6b6877e97c950d18d95505a9769'},
          'CUR/Space→  (1)': {'md5': '2c8a1745d3f0add39eb277b539fdfeaa'},
          'CUR/Space→  (2)': {'md5': 'e272aa59f06c6b9f3af33c941e6b3c5a'},
          'CUR/Space→ ': {'md5': '20f9a99e85968869900467f65298c1ba'},
          'CUR/Square [] (1)': {'md5': 'bc066cc3c9934b31a337260efc99d1df'},
          'CUR/Square [] (2)': {'md5': 'fc77bf99b696d888e4ca937f8cf5097a'},
          'CUR/Square []': {'md5': '47a86abc13e9703ca7457ea9e29b83b1'},
          'CUR/Star * (1)': {'md5': '2a9362d8d04ce694c85e4d054fa72763'},
          'CUR/Star * (2)': {'md5': '341c4e1e9785c4658250c1a37e9fd04f'},
          'CUR/Star *': {'md5': '1e1d1d592ee97949db5fa6d1db01a06f'},
          'CUR/String Terminator \x9c (1)': {'md5': 'ba7f76d662af39dfbaeb3899c46978ef'},
          'CUR/String Terminator \x9c (2)': {'md5': '33abcf5fd9bff50dc5f97592a133d1d2'},
          'CUR/String Terminator \x9c': {'md5': 'ad901030332576dfb289e848d7ef5721'},
          'CUR/Tab \t (1)': {'md5': '76635ca0e84ce5af2d08804b81fe33e0'},
          'CUR/Tab \t (2)': {'md5': '3f92d47d30f56412f37b81aaf302d5eb'},
          'CUR/Tab \t': {'md5': 'f546955680b666f1f4ed2cddb173d142'},
          'CUR/Three... (1)': {'md5': '79d37b7eedc1aa7d1d54fdf906ea32f4'},
          'CUR/Three... (2)': {'md5': '17715f0b716cf0061fce3c6ac99fa035'},
          'CUR/Three... (3)': {'md5': '2cc2c8026d87a4a2ee0175d3977bd9db'},
          'CUR/Three... (4)': {'md5': 'cc8a892db5bbb6e00783911a977e49e2'},
          'CUR/Three... (5)': {'md5': 'd79667406b09137e02359a22570cc69f'},
          'CUR/Three...': {'md5': 'fd7443b6ef5da1fe8229a64ac462fbb9'},
          'CUR/Tilde ~ (1)': {'md5': 'b9170048f08dafa6d314ff685cf56396'},
          'CUR/Tilde ~ (2)': {'md5': '44a6e1123c523c089a1fb2f622a346d9'},
          'CUR/Tilde ~': {'md5': 'd644fb4311c7b1c581ed1cbca0913ff3'},
          'CUR/Two.. (1)': {'md5': '561eaf19d31ba9f2f3a93f4a1cf740cd'},
          'CUR/Two.. (2)': {'md5': '3bfdba5a9317da55733a3aa3db8788c8'},
          'CUR/Two..': {'md5': '8b25e8cb6343c03d3dceeb5287c4414f'},
          'CUR/Underscore _ (1)': {'md5': 'c23f32b919508169a496a093839f0e04'},
          'CUR/Underscore _ (2)': {'md5': 'cdf441502a50204b943e0a8f943e0668'},
          'CUR/Underscore _': {'md5': '3ef0593f0a008dd757bfc49dc75f3f9a'},
          'CUR/\\ (1)': {'md5': '12f8de3a25b32ebf31d24faf6e3e1a24'},
          'CUR/\\ (2)': {'md5': 'a3d218116d9c340c9844a9dfe97ddf27'},
          'CUR/\\': {'md5': 'd8c23e9e78fc36a8a07a74fd7af66417'},
          'CUR/\\\\server\\share\\file (1)': {'md5': 'bcfef6ccd938358b6db770eb1d3de17f'},
          'CUR/\\\\server\\share\\file (2)': {'md5': '422825a9e933d11cf4c2f5c0f03e8224'},
          'CUR/\\\\server\\share\\file': {'md5': '6bf0dd24273206e58f6dae18c4e0c5d6'},
          'CUR/abcd' + '🙂' * 61 + ' (1)': {'md5': 'c573d71784261e3c388d489c915b879a'},
          'CUR/abcd' + '🙂' * 61 + ' (2)': {'md5': '5f19970d9d6df57bd84b506e6d81807e'},
          'CUR/abcd' + '🙂' * 62: {'md5': '51ee3f362dab5fafdb377657487fa09c'},
          'CUR/abc' + '🙂' * 62 + ' (1)': {'md5': '05937261559a83256ddb4d44480bb5c4'},
          'CUR/abc' + '🙂' * 62 + ' (2)': {'md5': '6ac381136adf1f350e667f4753e65f63'},
          'CUR/abc' + '🙂' * 63: {'md5': '4d222a84ac3bd28f6efd125337e125f6'},
          'CUR/ab' + '🙂' * 62 + ' (1)': {'md5': '4db72400bf44ff1bf81231513083701d'},
          'CUR/ab' + '🙂' * 62 + ' (2)': {'md5': 'f7b12f040637a6dae4263bc2817b56eb'},
          'CUR/ab' + '🙂' * 63: {'md5': 'fd0d1895da329d89e2396f8300c4f61f'},
          'CUR/a' + '🙂' * 62 + ' (1)': {'md5': '6ca3d8755b658c8c0ffe1c1d43b61b2a'},
          'CUR/a' + '🙂' * 62 + ' (2)': {'md5': '56e595f226384b9413361d435b5f5e44'},
          'CUR/a' + '🙂' * 63: {'md5': 'e1397fa63e2d64195fcedad9348182e8'},
          'CUR/case (1)': {'md5': 'cf53c713e71b4765ebbe560c8e826868'},
          'CUR/case (2)': {'md5': '0f94a61da6aaa86267cd006c6812582a'},
          'CUR/case': {'md5': '2c6d2601bfa42243878ef4ddd9940b42'},
          'CUR/foo (1).tar.gz': {'md5': '108c35f79486c56c344d0f064e4a511a'},
          'CUR/foo (2).tar.gz': {'md5': '387ccc3f6b2333031dc50e733f7827b2'},
          'CUR/foo (3).tar.gz': {'md5': '711ab4fd640632037360f79cda1fdc2b'},
          'CUR/foo (4).tar.gz': {'md5': '82fd4edb638dae95d133c2c870bc09eb'},
          'CUR/foo (5).tar.gz': {'md5': '0e6b54d7b2997381121a0b943e8243b4'},
          'CUR/foo.a b (1)': {'md5': 'b9210e8eda5412bf89a20cdac2e0a104'},
          'CUR/foo.a b (2)': {'md5': 'aa32c2818b3e50eda6b8fa566d0fe927'},
          'CUR/foo.a b (3)': {'md5': '30ff74a98043c1d2c015210865d456d6'},
          'CUR/foo.a b (4)': {'md5': '5e9a95548b5b6b53061ec81719bedd8e'},
          'CUR/foo.a b (5)': {'md5': 'b10267c805c7c45a8c5a09c4f36377ea'},
          'CUR/foo.a b': {'md5': '786e84bde86ba0eb1606b4df422a791d'},
          'CUR/foo.tar.gz': {'md5': '06ab92061c8deeabf83b9aefcd0d59b0'},
          'CUR/u': {},
          'CUR/u/v': {},
          'CUR/u/v/w': {},
          'CUR/u/v/w/x': {},
          'CUR/u/v/w/x/y': {},
          'CUR/u/v/w/x/y/z (1)': {'md5': '92ca5594530ebbe26df838c6e789a669'},
          'CUR/u/v/w/x/y/z (2)': {'md5': '5780324871ddfae6538bd0d427e1c731'},
          'CUR/u/v/w/x/y/z': {'md5': '7913f1d77a8a8d35cb62da4e8cba336a'},
          'CUR/~ (1)': {'md5': 'db7971e041320de89f83b3caa3c11c7e'},
          'CUR/~ (2)': {'md5': '115d04c23c77c6490bd9aebe9cfde881'},
          'CUR/~': {'md5': '5dc8143d7f881a02622daaeabf714329'},
          'CUR/' + '🏳\u200d🌈' * 22 + ' (1)': {'md5': 'e15a50f8eb3fccdc26132fa06fa1205f'},
          'CUR/' + '🏳\u200d🌈' * 22 + ' (2)': {'md5': 'e5918d1b19a95d6a0724bca2b9e74878'},
          'CUR/' + '🏳\u200d🌈' * 23: {'md5': '8c29470e347f44ec20cd684c0f192945'},
          'CUR/' + '🙂' * 62 + ' (1)': {'md5': '0ca3c0471e4176f8e223d69ffb67e847'},
          'CUR/' + '🙂' * 62 + ' (2)': {'md5': '6c191a9c421da46230b4367e4b8e08d9'},
          'CUR/' + '🙂' * 63: {'md5': '9ba840cf76707d55db222c1b90cedbfb'},
          'ROOT': {},
          'ROOT/At The Top (1)': {'md5': 'ba54e1aca97bad73f835c5d1c858417c'},
          'ROOT/At The Top (2)': {'md5': '2d56b90a936b4f2b7f9ba2e4ef1fbd83'},
          'ROOT/At The Top': {'md5': '9ef02862743242d23ce6ed223c38b707'},
          'ROOT/Empty': {},
          'ROOT/dev': {},
          'ROOT/dev/null (1)': {'md5': '0542f8d179a0a5812cca12f9d1a20768'},
          'ROOT/dev/null (2)': {'md5': '146840ea79bf74bd86d1e73c3a166d4b'},
          'ROOT/dev/null': {'md5': '2a62812a0e6f22b55507ef85c0e3e3e4'},
          'UP': {},
          'UP/One Level Up (1)': {'md5': '5cfff7eb216252fd9edd35ad58f65807'},
          'UP/One Level Up (2)': {'md5': '8926cc7e8073e1079320f2e0b4b2a05c'},
          'UP/One Level Up': {'md5': 'd530362d8793bd2213702f7a8b9eb391'},
          'UPUP': {},
          'UPUP/Two Levels Up (1)': {'md5': 'c1c08ba600c42750bb25007bd93fcd37'},
          'UPUP/Two Levels Up (2)': {'md5': '35bdc6589118dee115df941fd9775282'},
          'UPUP/Two Levels Up': {'md5': 'fefd04175ab55cbf25f4e59a62b44c2a'},
          'UPUPUP': {},
          'UPUPUP/Three Levels Up (1)': {'md5': '5d7122fa28bb1886d90cdbaee7b8b630'},
          'UPUPUP/Three Levels Up (2)': {'md5': '69baf719bc3af25f12c86a2c146ab491'},
          'UPUPUP/Three Levels Up': {'md5': '77798d1b2b8f820dbf742a6416d2fd51'},
      },
      'no-owner-info.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'README': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1265493078000000000,
              'mtime': 1265493078000000000,
              'ctime': 1265493078000000000,
              'size': 760,
              'md5': 'f196d610d1cdf9191b4440863e8d31ab',
          }
      },
      'not-full-path-deep.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'sim': {'nlink': 3},
          'sim/salabim': {'nlink': 3},
          'sim/salabim/rahat': {
              'nlink': 2,
          },
          'sim/salabim/rahat/lukum': {
              'nlink': 1,
              'atime': 1389126376000000000,
              'mtime': 1389126376000000000,
              'ctime': 1389126376000000000,
              'size': 10,
              'md5': '16c52c6e8326c071da771e66dc6e9e57',
          },
          'sim/salabim/rahat-lukum': {
              'nlink': 1,
              'atime': 1389126376000000000,
              'mtime': 1389126376000000000,
              'ctime': 1389126376000000000,
              'size': 10,
              'md5': '38b18761d3d0c217371967a98d545c2e',
          },
      },
      'not-full-path.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
          'bebebe': {
              'nlink': 1,
              'atime': 1291639841000000000,
              'mtime': 1291639841000000000,
              'ctime': 1291611042000000000,
              'size': 5,
              'md5': '655dba24211af27d85c3cc4a910cc2ef',
          },
          'foo': {'nlink': 2},
          'foo/bar': {
              'nlink': 1,
              'atime': 1291630409000000000,
              'mtime': 1291630409000000000,
              'ctime': 1291601610000000000,
              'size': 10,
              'md5': '10a28a91b53776aaf0800d68eb260eb6',
          },
      },
      'ntfs-extrafld.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'test.txt': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1560435723770100400,
              'mtime': 1560435721722114700,
              'ctime': 1560417720000000000,
              'size': 2600,
              'errno': 5,
          }
      },
      'parent-relative-paths.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 4},
          'UP': {'nlink': 3},
          'UP/other': {'nlink': 2},
          'UP/other/LICENSE': {
              'nlink': 1,
              'atime': 1371459156000000000,
              'mtime': 1371459156000000000,
              'ctime': 1371437556000000000,
              'size': 7639,
              'md5': '6a6a8e020838b23406c81b19c1d46df6',
          },
          'UPUP': {'nlink': 2},
          'UPUP/INSTALL': {
              'nlink': 1,
              'atime': 1371459066000000000,
              'mtime': 1371459066000000000,
              'ctime': 1371437466000000000,
              'size': 454,
              'md5': '2ffc1c72359e389608ec4de36c6d1fac',
          },
      },
      'pkware-specials.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'block': {
              'mode': 'brw-r--r--',
              'nlink': 1,
              'atime': 1564833480000000000,
              'mtime': 1564833480000000000,
              'ctime': 1564808280000000000,
              'rdev': 2049,
          },
          'char': {
              'mode': 'crw-r--r--',
              'nlink': 1,
              'atime': 1564833480000000000,
              'mtime': 1564833480000000000,
              'ctime': 1564808280000000000,
              'rdev': 1024,
          },
          'fifo': {
              'mode': 'prw-r--r--',
              'nlink': 1,
              'atime': 1565809123000000000,
              'mtime': 1565809123000000000,
              'ctime': 1565783922000000000,
          },
          'regular': {
              'mode': '-rw-r--r--',
              'nlink': 3,
              'atime': 1565807437000000000,
              'mtime': 1565290018000000000,
              'ctime': 1565264818000000000,
              'size': 32,
              'md5': '456e611a5420b7dd09bae143a7b2deb0',
          },
          'socket': {
              'mode': 'srw-r--r--',
              'nlink': 1,
              'atime': 1564834444000000000,
              'mtime': 1564834444000000000,
              'ctime': 1564809244000000000,
          },
          'symlink': {
              'mode': 'lrwxr-xr-x',
              'nlink': 1,
              'atime': 1566731330000000000,
              'mtime': 1564834729000000000,
              'ctime': 1564809528000000000,
              'target': 'regular',
          },
          'symlink2': {
              'mode': 'lrwxr-xr-x',
              'nlink': 1,
              'atime': 1566731384000000000,
              'mtime': 1566731354000000000,
              'ctime': 1566706154000000000,
              'target': 'regular',
          },
          'z-hardlink-block': {
              'mode': 'brw-r--r--',
              'nlink': 1,
              'atime': 1564833480000000000,
              'mtime': 1564833480000000000,
              'ctime': 1564808280000000000,
              'rdev': 2049,
          },
          'z-hardlink-char': {
              'mode': 'crw-r--r--',
              'nlink': 1,
              'atime': 1564833480000000000,
              'mtime': 1564833480000000000,
              'ctime': 1564808280000000000,
              'rdev': 1024,
          },
          'z-hardlink-fifo': {
              'mode': 'prw-r--r--',
              'nlink': 1,
              'atime': 1565809123000000000,
              'mtime': 1565809123000000000,
              'ctime': 1565783922000000000,
          },
          'z-hardlink-socket': {
              'mode': 'srw-r--r--',
              'nlink': 1,
              'atime': 1564834444000000000,
              'mtime': 1564834444000000000,
              'ctime': 1564809244000000000,
          },
          'z-hardlink-symlink': {
              'mode': 'lrwxr-xr-x',
              'nlink': 1,
              'atime': 1566731330000000000,
              'mtime': 1564834729000000000,
              'ctime': 1564809528000000000,
              'target': 'regular',
          },
          'z-hardlink1': {
              'mode': '-rw-r--r--',
              'nlink': 3,
              'atime': 1565807437000000000,
              'mtime': 1565290018000000000,
              'ctime': 1565264818000000000,
              'size': 32,
              'md5': '456e611a5420b7dd09bae143a7b2deb0',
          },
          'z-hardlink2': {
              'mode': '-rw-r--r--',
              'nlink': 3,
              'atime': 1565807437000000000,
              'mtime': 1565290018000000000,
              'ctime': 1565264818000000000,
              'size': 32,
              'md5': '456e611a5420b7dd09bae143a7b2deb0',
          },
      },
      'pkware-symlink.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'regular': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1566730531000000000,
              'mtime': 1566730512000000000,
              'ctime': 1566705312000000000,
              'size': 33,
              'md5': '4404716d8a90c37fdc18d88b70d09fa3',
          },
          'symlink': {
              'mode': 'lrwxr-xr-x',
              'nlink': 1,
              'atime': 1566730690000000000,
              'mtime': 1566730517000000000,
              'ctime': 1566705316000000000,
              'target': 'regular',
          },
      },
      'sjis-filename.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          '新しいテキスト ドキュメント.txt': {
              'nlink': 1,
              'atime': 1601539972000000000,
              'mtime': 1601539972000000000,
              'ctime': 1601539972000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          }
      },
      'symlink.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'date': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1388943675000000000,
              'mtime': 1388943675000000000,
              'ctime': 1388918476000000000,
              'size': 35,
              'md5': 'e84bea37a02d9285935368412725b442',
          },
          'symlink': {
              'mode': 'lrwxr-xr-x',
              'nlink': 1,
              'atime': 1388943662000000000,
              'mtime': 1388943650000000000,
              'ctime': 1388918450000000000,
              'target': '../tmp/date',
          },
      },
      'unix-perm.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          '640': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1388890728000000000,
              'mtime': 1388890728000000000,
              'ctime': 1388865528000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          },
          '642': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1388890755000000000,
              'mtime': 1388890755000000000,
              'ctime': 1388865556000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          },
          '666': {
              'mode': '-rw-r--r--',
              'nlink': 1,
              'atime': 1388890728000000000,
              'mtime': 1388890728000000000,
              'ctime': 1388865528000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          },
          '6775': {
              'mode': '-rwxr-xr-x',
              'nlink': 1,
              'atime': 1388890915000000000,
              'mtime': 1388890915000000000,
              'ctime': 1388865716000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          },
          '777': {
              'mode': '-rwxr-xr-x',
              'nlink': 1,
              'atime': 1388890728000000000,
              'mtime': 1388890728000000000,
              'ctime': 1388865528000000000,
              'size': 0,
              'md5': 'd41d8cd98f00b204e9800998ecf8427e',
          },
      },
      'with-and-without-precise-time.zip': {
          '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
          'unmodified': {
              'nlink': 1,
              'atime': 1564327465000000000,
              'mtime': 1564327465000000000,
              'ctime': 1564302266000000000,
              'size': 7,
              'md5': '88e6b9694d2fe3e0a47a898110ed44b6',
          },
          'with-precise': {
              'nlink': 1,
              'atime': 1532741025123456700,
              'mtime': 1532741025123456700,
              'ctime': 1564301732000000000,
              'size': 4,
              'md5': '814fa5ca98406a903e22b43d9b610105',
          },
          'without-precise': {
              'nlink': 1,
              'atime': 1532741025000000000,
              'mtime': 1532741025000000000,
              'ctime': 1532715826000000000,
              'size': 4,
              'md5': '814fa5ca98406a903e22b43d9b610105',
          },
      },
  }

  for zip_name, want_tree in want_trees.items():
    MountZipAndCheckTree(zip_name, want_tree, options=['--force'])


# Tests the ZIP with lots of files.
def TestZipWithManyFiles():
  # Only check a few files: the first one, the last one, and one in the middle.
  want_tree = {
      '1': {
          'mode': '-rw-r--r--',
          'nlink': 1,
          'atime': 1371243195000000000,
          'mtime': 1371243195000000000,
          'ctime': 1371221596000000000,
          'size': 0,
      },
      '30000': {
          'mode': '-rw-r--r--',
          'nlink': 1,
          'atime': 1371243200000000000,
          'mtime': 1371243200000000000,
          'ctime': 1371221600000000000,
          'size': 0,
      },
      '65536': {
          'mode': '-rw-r--r--',
          'nlink': 1,
          'atime': 1371243206000000000,
          'mtime': 1371243206000000000,
          'ctime': 1371221606000000000,
          'size': 0,
      },
  }

  MountZipAndCheckTree(
      '65536-files.zip',
      want_tree,
      want_blocks=65537,
      want_inodes=65537,
      strict=False,
      use_md5=False,
  )

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

  MountZipAndCheckTree(
      'collisions.zip',
      want_tree,
      want_blocks=100007,
      want_inodes=100014,
      strict=False,
      use_md5=False,
  )


# Tests that a big file can be accessed in random order.
def TestBigZip(options=[]):
  zip_name = 'big.zip'
  s = f'Test {zip_name!r}'
  if options:
    s += f', options = {" ".join(options)!r}'
  logging.info(s)
  with tempfile.TemporaryDirectory() as mount_point:
    zip_path = os.path.join(script_dir, 'data', zip_name)
    logging.debug(f'Mounting {zip_path!r} on {mount_point!r}...')
    subprocess.run(
        [mount_program] + options + [zip_path, mount_point],
        check=True,
        capture_output=True,
        input='',
        encoding='UTF-8',
    )
    try:
      logging.debug(f'Mounted ZIP {zip_path!r} on {mount_point!r}')

      st = os.statvfs(mount_point)

      want_blocks = 10546877
      if st.f_blocks != want_blocks:
        LogError(
            f'Mismatch for st.f_blocks: got: {st.f_blocks}, want: {want_blocks}'
        )

      want_inodes = 2
      if st.f_files != want_inodes:
        LogError(
            f'Mismatch for st.f_files: got: {st.f_files}, want: {want_inodes}'
        )

      tree = GetTree(mount_point, use_md5=False)
      fd = os.open(os.path.join(mount_point, 'big.txt'), os.O_RDONLY)
      try:
        random.seed()
        n = 100000000
        for j in [random.randrange(n) for i in range(100)] + [n - 1, 0, n - 1]:
          logging.debug(f'Getting line {j}...')
          want_line = b'%08d The quick brown fox jumps over the lazy dog.\n' % j
          got_line = os.pread(fd, len(want_line), j * len(want_line))
          if got_line != want_line:
            LogError(f'Want line: {want_line!r}, Got line: {got_line!r}')
        got_line = os.pread(fd, 100, j * len(want_line))
        if got_line != want_line:
          LogError(f'Want line: {want_line!r}, Got line: {got_line!r}')
        got_line = os.pread(fd, 100, n * len(want_line))
        if got_line:
          LogError(f'Want empty line, Got line: {got_line!r}')
      finally:
        os.close(fd)
    finally:
      logging.debug(f'Unmounting {zip_path!r} from {mount_point!r}...')
      subprocess.run(['fusermount', '-u', '-z', mount_point], check=True)
      logging.debug(f'Unmounted {zip_path!r} from {mount_point!r}')


# Tests that a big file can be accessed in somewhat random order even with no
# cache file.
def TestBigZipNoCache(options=['--nocache']):
  zip_name = 'big.zip'
  s = f'Test {zip_name!r}'
  if options:
    s += f', options = {" ".join(options)!r}'
  logging.info(s)
  with tempfile.TemporaryDirectory() as mount_point:
    zip_path = os.path.join(script_dir, 'data', zip_name)
    logging.debug(f'Mounting {zip_path!r} on {mount_point!r}...')
    subprocess.run(
        [mount_program] + options + [zip_path, mount_point],
        check=True,
        capture_output=True,
        input='',
        encoding='UTF-8',
    )
    try:
      logging.debug(f'Mounted ZIP {zip_path!r} on {mount_point!r}')
      tree = GetTree(mount_point, use_md5=False)
      fd = os.open(os.path.join(mount_point, 'big.txt'), os.O_RDONLY)
      try:
        random.seed()
        n = 100000000
        for j in (
            sorted([random.randrange(n) for i in range(50)])
            + [n - 1, 0]
            + sorted([random.randrange(n) for i in range(50)])
            + [n - 1, 0]
        ):
          logging.debug(f'Getting line {j}...')
          want_line = b'%08d The quick brown fox jumps over the lazy dog.\n' % j
          got_line = os.pread(fd, len(want_line), j * len(want_line))
          if got_line != want_line:
            LogError(f'Want line: {want_line!r}, Got line: {got_line!r}')
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
      '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
      'Encrypted ZipCrypto.txt': {
          'nlink': 1,
          'atime': 1598594095000000000,
          'mtime': 1598592187000000000,
          'ctime': 1598592188000000000,
          'size': 34,
          'md5': '275e8c5aed7e7ce2f32dd1e5e9ee4a5b',
      },
      'Encrypted AES-256.txt': {
          'nlink': 1,
          'atime': 1598594134000000000,
          'mtime': 1598592213000000000,
          'ctime': 1598592214000000000,
          'size': 32,
          'md5': 'ca5e064a0835d186f2f6326f88a7078f',
      },
      'Encrypted AES-192.txt': {
          'nlink': 1,
          'atime': 1598594124000000000,
          'mtime': 1598592206000000000,
          'ctime': 1598592206000000000,
          'size': 32,
          'md5': 'e48d57930ef96ff2ad45867202d3250d',
      },
      'Encrypted AES-128.txt': {
          'nlink': 1,
          'atime': 1598594117000000000,
          'mtime': 1598592200000000000,
          'ctime': 1598592200000000000,
          'size': 32,
          'md5': '07c4edd2a55c9d5614457a21fb40aa56',
      },
      'ClearText.txt': {
          'nlink': 1,
          'atime': 1598592142000000000,
          'mtime': 1598592138000000000,
          'ctime': 1598592138000000000,
          'size': 23,
          'md5': '7a542815e2c51837b3d8a8b2ebf36490',
      },
  }

  MountZipAndCheckTree(
      zip_name, want_tree, want_blocks=11, want_inodes=6, password='password'
  )

  MountZipAndCheckTree(
      zip_name, want_tree, want_blocks=11, want_inodes=6, password='password\n'
  )

  MountZipAndCheckTree(
      zip_name,
      want_tree,
      want_blocks=11,
      want_inodes=6,
      password='password\nThis line is ignored...\n',
  )

  # With wrong or no password.
  CheckZipMountingError(zip_name, 37, password='wrong password')
  CheckZipMountingError(zip_name, 36)

  want_tree = {
      '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
      'Encrypted ZipCrypto.txt': {
          'nlink': 1,
          'atime': 1598594095000000000,
          'mtime': 1598592187000000000,
          'ctime': 1598592188000000000,
          'size': 34,
          'errno': 5,
      },
      'Encrypted AES-256.txt': {
          'nlink': 1,
          'atime': 1598594134000000000,
          'mtime': 1598592213000000000,
          'ctime': 1598592214000000000,
          'size': 32,
          'errno': 5,
      },
      'Encrypted AES-192.txt': {
          'nlink': 1,
          'atime': 1598594124000000000,
          'mtime': 1598592206000000000,
          'ctime': 1598592206000000000,
          'size': 32,
          'errno': 5,
      },
      'Encrypted AES-128.txt': {
          'nlink': 1,
          'atime': 1598594117000000000,
          'mtime': 1598592200000000000,
          'ctime': 1598592200000000000,
          'size': 32,
          'errno': 5,
      },
      'ClearText.txt': {
          'nlink': 1,
          'atime': 1598592142000000000,
          'mtime': 1598592138000000000,
          'ctime': 1598592138000000000,
          'size': 23,
          'md5': '7a542815e2c51837b3d8a8b2ebf36490',
      },
  }

  MountZipAndCheckTree(
      zip_name,
      want_tree,
      want_blocks=11,
      want_inodes=6,
      password='wrong password',
      options=['--force'],
  )

  MountZipAndCheckTree(
      zip_name, want_tree, want_blocks=11, want_inodes=6, options=['--force']
  )


# Tests mounting ZIP with explicit file name encoding.
def TestZipFileNameEncoding():
  want_tree = {
      '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
      'Дата': {
          'nlink': 1,
          'atime': 1265363324000000000,
          'mtime': 1265363324000000000,
          'ctime': 1265363324000000000,
          'size': 5,
          'md5': 'a9564ebc3289b7a14551baf8ad5ec60a',
      },
      'Текстовый документ.txt': {
          'nlink': 1,
          'atime': 1265362564000000000,
          'mtime': 1265362564000000000,
          'ctime': 1265362564000000000,
          'size': 8,
          'md5': 'f75b8179e4bbe7e2b4a074dcef62de95',
      },
  }
  MountZipAndCheckTree('cp866.zip', want_tree, options=['-o', 'encoding=cp866'])

  want_tree = {
      '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 3},
      '±≥≤⌠⌡÷≈°∙·√ⁿ²■\xa0\xa0': {'size': 0},
      'ßΓπΣσμτΦΘΩδ∞φε∩≡': {'size': 0},
      '╤╥╙╘╒╓╫╪┘┌█▄▌▐▀α': {'size': 0},
      '┴┬├─┼╞╟╚╔╩╦╠═╬╧╨': {'size': 0},
      '▒▓│┤╡╢╖╕╣║╗╝╜╛┐└': {'size': 0},
      'íóúñÑªº¿⌐¬½¼¡«»░': {'size': 0},
      'æÆôöòûùÿÖÜ¢£¥₧ƒá': {'size': 0},
      'üéâäàåçêëèïîìÄÅÉ': {'size': 0},
      'abcdefghijklmnop': {'size': 0},
      'QRSTUVWXYZ[\\]^_`': {'size': 0},
      'ABCDEFGHIJKLMNOP': {'size': 0},
      '123456789:;<=>?@': {'size': 0},
      '!"#$%&\'()*+,-.': {},
      '!"#$%&\'()*+,-./0': {'size': 0},
      'qrstuvwxyz{|}~\x1aÇ': {'size': 0},
      '\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1c\x1b\x7f\x1d\x1e\x1f ': {
          'size': 0
      },
      '\x01\x02\x03\x04\x05\x06\x07\x08\t\n\x0b\x0c\r\x0e\x0f\x10': {'size': 0},
  }
  MountZipAndCheckTree('cp437.zip', want_tree, options=['-o', 'encoding=cp437'])

  del want_tree['ßΓπΣσμτΦΘΩδ∞φε∩≡']
  del want_tree['qrstuvwxyz{|}~\x1aÇ']
  del want_tree['\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1c\x1b\x7f\x1d\x1e\x1f ']
  del want_tree['\x01\x02\x03\x04\x05\x06\x07\x08\t\n\x0b\x0c\r\x0e\x0f\x10']

  want_tree.update({
      'ßΓπΣσµτΦΘΩδ∞φε∩≡': {'size': 0},
      'qrstuvwxyz{|}~⌂Ç': {'size': 0},
      '◄↕‼¶§▬↨↑↓→←∟↔▲▼ ': {'size': 0},
      '☺☻♥♦♣♠•◘○◙♂♀♪♫☼►': {'size': 0},
  })
  MountZipAndCheckTree(
      'cp437.zip', want_tree, options=['-o', 'encoding=libzip']
  )
  MountZipAndCheckTree('cp437.zip', want_tree, options=['-o', 'encoding=wrong'])


# Tests the nosymlinks, nohardlinks and nospecials mount options.
def TestZipWithSpecialFiles():
  zip_name = 'pkware-specials.zip'

  want_tree = {
      '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
      'z-hardlink2': {
          'mode': '-rw-r--r--',
          'nlink': 3,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
      'z-hardlink1': {
          'mode': '-rw-r--r--',
          'nlink': 3,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
      'z-hardlink-symlink': {
          'mode': 'lrwxr-xr-x',
          'nlink': 1,
          'atime': 1566731330000000000,
          'mtime': 1564834729000000000,
          'ctime': 1564809528000000000,
          'target': 'regular',
      },
      'symlink': {
          'mode': 'lrwxr-xr-x',
          'nlink': 1,
          'atime': 1566731330000000000,
          'mtime': 1564834729000000000,
          'ctime': 1564809528000000000,
          'target': 'regular',
      },
      'z-hardlink-socket': {
          'mode': 'srw-r--r--',
          'nlink': 1,
          'atime': 1564834444000000000,
          'mtime': 1564834444000000000,
          'ctime': 1564809244000000000,
      },
      'z-hardlink-fifo': {
          'mode': 'prw-r--r--',
          'nlink': 1,
          'atime': 1565809123000000000,
          'mtime': 1565809123000000000,
          'ctime': 1565783922000000000,
      },
      'z-hardlink-char': {
          'mode': 'crw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 1024,
      },
      'z-hardlink-block': {
          'mode': 'brw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 2049,
      },
      'symlink2': {
          'mode': 'lrwxr-xr-x',
          'nlink': 1,
          'atime': 1566731384000000000,
          'mtime': 1566731354000000000,
          'ctime': 1566706154000000000,
          'target': 'regular',
      },
      'socket': {
          'mode': 'srw-r--r--',
          'nlink': 1,
          'atime': 1564834444000000000,
          'mtime': 1564834444000000000,
          'ctime': 1564809244000000000,
      },
      'regular': {
          'mode': '-rw-r--r--',
          'nlink': 3,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
      'fifo': {
          'mode': 'prw-r--r--',
          'nlink': 1,
          'atime': 1565809123000000000,
          'mtime': 1565809123000000000,
          'ctime': 1565783922000000000,
      },
      'char': {
          'mode': 'crw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 1024,
      },
      'block': {
          'mode': 'brw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 2049,
      },
  }

  MountZipAndCheckTree(zip_name, want_tree, want_blocks=17, want_inodes=15)

  # Check that the inode numbers of hardlinks match
  got_tree, _ = MountZipAndGetTree(zip_name)
  want_ino = got_tree['regular']['ino']
  if not want_ino > 0:
    LogError(f'Want positive ino, Got: {want_ino}')

  for link_name in ['z-hardlink1', 'z-hardlink2']:
    got_ino = got_tree[link_name]['ino']
    if got_ino != want_ino:
      LogError(f'Want ino: {want_ino}, Got: {got_ino}')

  # Test -o nosymlinks
  want_tree = {
      '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
      'z-hardlink2': {
          'mode': '-rw-r--r--',
          'nlink': 3,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
      'z-hardlink1': {
          'mode': '-rw-r--r--',
          'nlink': 3,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
      'z-hardlink-socket': {
          'mode': 'srw-r--r--',
          'nlink': 1,
          'atime': 1564834444000000000,
          'mtime': 1564834444000000000,
          'ctime': 1564809244000000000,
      },
      'z-hardlink-fifo': {
          'mode': 'prw-r--r--',
          'nlink': 1,
          'atime': 1565809123000000000,
          'mtime': 1565809123000000000,
          'ctime': 1565783922000000000,
      },
      'z-hardlink-char': {
          'mode': 'crw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 1024,
      },
      'z-hardlink-block': {
          'mode': 'brw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 2049,
      },
      'socket': {
          'mode': 'srw-r--r--',
          'nlink': 1,
          'atime': 1564834444000000000,
          'mtime': 1564834444000000000,
          'ctime': 1564809244000000000,
      },
      'regular': {
          'mode': '-rw-r--r--',
          'nlink': 3,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
      'fifo': {
          'mode': 'prw-r--r--',
          'nlink': 1,
          'atime': 1565809123000000000,
          'mtime': 1565809123000000000,
          'ctime': 1565783922000000000,
      },
      'char': {
          'mode': 'crw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 1024,
      },
      'block': {
          'mode': 'brw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 2049,
      },
  }

  MountZipAndCheckTree(
      zip_name,
      want_tree,
      want_blocks=13,
      want_inodes=12,
      options=['-o', 'nosymlinks'],
  )

  # Test -o nohardlinks
  want_tree = {
      '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
      'z-hardlink-socket': {
          'mode': 'srw-r--r--',
          'nlink': 1,
          'atime': 1564834444000000000,
          'mtime': 1564834444000000000,
          'ctime': 1564809244000000000,
      },
      'z-hardlink-fifo': {
          'mode': 'prw-r--r--',
          'nlink': 1,
          'atime': 1565809123000000000,
          'mtime': 1565809123000000000,
          'ctime': 1565783922000000000,
      },
      'z-hardlink-char': {
          'mode': 'crw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 1024,
      },
      'z-hardlink-block': {
          'mode': 'brw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 2049,
      },
      'symlink2': {
          'mode': 'lrwxr-xr-x',
          'nlink': 1,
          'atime': 1566731384000000000,
          'mtime': 1566731354000000000,
          'ctime': 1566706154000000000,
          'target': 'regular',
      },
      'socket': {
          'mode': 'srw-r--r--',
          'nlink': 1,
          'atime': 1564834444000000000,
          'mtime': 1564834444000000000,
          'ctime': 1564809244000000000,
      },
      'regular': {
          'mode': '-rw-r--r--',
          'nlink': 1,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
      'fifo': {
          'mode': 'prw-r--r--',
          'nlink': 1,
          'atime': 1565809123000000000,
          'mtime': 1565809123000000000,
          'ctime': 1565783922000000000,
      },
      'char': {
          'mode': 'crw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 1024,
      },
      'block': {
          'mode': 'brw-r--r--',
          'nlink': 1,
          'atime': 1564833480000000000,
          'mtime': 1564833480000000000,
          'ctime': 1564808280000000000,
          'rdev': 2049,
      },
  }

  MountZipAndCheckTree(
      zip_name,
      want_tree,
      want_blocks=13,
      want_inodes=11,
      options=['-o', 'nohardlinks'],
  )

  # Test -o nospecials
  want_tree = {
      '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
      'z-hardlink2': {
          'mode': '-rw-r--r--',
          'nlink': 3,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
      'z-hardlink1': {
          'mode': '-rw-r--r--',
          'nlink': 3,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
      'z-hardlink-symlink': {
          'mode': 'lrwxr-xr-x',
          'nlink': 1,
          'atime': 1566731330000000000,
          'mtime': 1564834729000000000,
          'ctime': 1564809528000000000,
          'target': 'regular',
      },
      'symlink': {
          'mode': 'lrwxr-xr-x',
          'nlink': 1,
          'atime': 1566731330000000000,
          'mtime': 1564834729000000000,
          'ctime': 1564809528000000000,
          'target': 'regular',
      },
      'symlink2': {
          'mode': 'lrwxr-xr-x',
          'nlink': 1,
          'atime': 1566731384000000000,
          'mtime': 1566731354000000000,
          'ctime': 1566706154000000000,
          'target': 'regular',
      },
      'regular': {
          'mode': '-rw-r--r--',
          'nlink': 3,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      },
  }

  MountZipAndCheckTree(
      zip_name,
      want_tree,
      want_blocks=9,
      want_inodes=7,
      options=['-o', 'nospecials'],
  )

  # Tests -o nosymlinks nohardlinks and nospecials together
  want_tree = {
      '.': {'ino': 1, 'mode': 'drwxr-xr-x', 'nlink': 2},
      'regular': {
          'mode': '-rw-r--r--',
          'nlink': 1,
          'atime': 1565807437000000000,
          'mtime': 1565290018000000000,
          'ctime': 1565264818000000000,
          'size': 32,
          'md5': '456e611a5420b7dd09bae143a7b2deb0',
      }
  }

  MountZipAndCheckTree(
      zip_name,
      want_tree,
      want_blocks=3,
      want_inodes=2,
      options=['-o', 'nosymlinks,nohardlinks,nospecials'],
  )


# Tests invalid and absent ZIP archives.
def TestInvalidZip():
  CheckZipMountingError('', 38)
  CheckZipMountingError('absent.zip', 19)
  CheckZipMountingError('invalid.zip', 29)
  with tempfile.NamedTemporaryFile() as f:
    os.chmod(f.name, 0)
    CheckZipMountingError(f.name, 21 if os.getuid() != 0 else 29)


logging.getLogger().setLevel('INFO')

TestZipWithDefaultOptions()
TestZipFileNameEncoding()
TestZipWithSpecialFiles()
TestEncryptedZip()
TestInvalidZip()
TestZipWithManyFiles()
TestBigZip()
TestBigZip(options=['--precache'])
TestBigZipNoCache()

if error_count:
  LogError(f'FAIL: There were {error_count} errors')
  sys.exit(1)
else:
  logging.info('PASS: All tests passed')

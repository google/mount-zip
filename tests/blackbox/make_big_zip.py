#!/bin/python3

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

from zipfile import ZipFile, ZIP_DEFLATED
import os.path
import os

dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'data')
tmp = os.path.join(dir, 'big.zip~')

try:
    with ZipFile(tmp, 'w', compression=ZIP_DEFLATED, allowZip64=True) as z:
        with z.open('big.txt', mode='w', force_zip64=True) as f:
            for i in range(100):
                print('\rWriting big.zip... %3d %%' % i, end='', flush=True)
                for j in range(1000000):
                    f.write(
                        b'%02d%06d The quick brown fox jumps over the lazy dog.\n'
                        % (i, j))

    print('\r\033[2KDone', flush=True)
    os.replace(tmp, os.path.join(dir, 'big.zip'))
except:
    os.remove(tmp)

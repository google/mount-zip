#!/bin/python3

# Copyright 2026 Google LLC
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

import os
import os.path
from zipfile import ZipFile

dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'data')
tmp = os.path.join(dir, 'deep.zip~')

try:
  with ZipFile(tmp, 'w') as z:
    n = 30000
    while n > 0:
        with z.open('a/' * n + 'pwn.txt', mode='w', force_zip64=True) as f:
            f.write(b'At depth of %d\n' % n)
        n = n * 3 // 4

  os.replace(tmp, os.path.join(dir, 'deep.zip'))
except:
  os.remove(tmp)

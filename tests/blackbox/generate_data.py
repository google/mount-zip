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

import os.path
from zipfile import ZIP_BZIP2, ZIP_DEFLATED, ZIP_LZMA, ZipFile

dir = os.path.dirname(os.path.realpath(__file__))


def MakeZipWithManySmallFiles():
  with ZipFile(
      os.path.join(dir, 'data', '100000-files.zip'), 'w', allowZip64=True
  ) as z:
    for i in range(100000):
      z.writestr('%06d.txt' % i, 'This is file %06d.\n' % i)


def MakeOtherZips():
  with ZipFile(os.path.join(dir, 'data', 'file-dir-same-name.zip'), 'w') as z:
    z.writestr('pet/cat', 'This is my first pet cat\n')
    z.writestr('pet', 'This is my first pet\n')
    z.writestr('pet/cat/fish', 'This is my first pet cat fish\n')
    z.writestr('pet/cat/fish/', '')
    z.writestr('pet/cat', 'This is my second pet cat\n')
    z.writestr('pet', 'This is my second pet\n')
    z.writestr('pet/cat/fish', 'This is my second pet cat fish\n')

  with ZipFile(
      os.path.join(dir, 'data', 'lzma.zip'), 'w', compression=ZIP_LZMA
  ) as z:
    z.writestr('lzma.txt', 'This file is compressed with LZMA.\n')

  with ZipFile(
      os.path.join(dir, 'data', 'bzip2.zip'), 'w', compression=ZIP_BZIP2
  ) as z:
    z.writestr('bzip2.txt', 'This file is compressed with BZIP2.\n')

  with ZipFile(os.path.join(dir, 'data', 'empty.zip'), 'w') as z:
    pass

  with ZipFile(os.path.join(dir, 'data', 'mixed-paths.zip'), 'w') as z:
    z.writestr(
        'normal.txt', 'This file is in the default "current" directory.\n'
    )
    z.writestr('../up-1.txt', 'This file is one level "up".\n')
    z.writestr('../../up-2.txt', 'This file is two levels "up".\n')
    z.writestr('/top.txt', 'This file is in the top root directory.\n')
    z.writestr(
        '/../over-the-top.txt', 'This file is "above" the top root directory.\n'
    )
    z.writestr('.', 'This should not be a valid path.\n')
    z.writestr('/.', 'This should not be a valid path.\n')
    z.writestr('a/.', 'This should not be a valid path.\n')
    z.writestr('a/./', 'This should not be a valid path.\n')
    z.writestr('a/./b', 'This should not be a valid path.\n')
    z.writestr('a/..', 'This should not be a valid path.\n')
    z.writestr('a/../', 'This should not be a valid path.\n')
    z.writestr('a/../b', 'This should not be a valid path.\n')
    z.writestr('And (&).txt', 'This is an ampersand &\n')
    z.writestr('Angle <>.txt', 'These are angle brackets <>\n')
    z.writestr('At (@).txt', 'This is an at sign @\n')
    z.writestr('Backslash (\\).txt', 'This is a backslash \\\n')
    z.writestr('Backtick (`).txt', 'This is a backtick `\n')
    z.writestr('Bar (|).txt', 'This is a bar |\n')
    z.writestr('Caret (^).txt', 'This is a caret ^\n')
    z.writestr('Carriage return (\r).txt', 'This is a carriage return\r\n')
    z.writestr('Colon (:).txt', 'This is a colon :\n')
    z.writestr('Comma (,).txt', 'This is a comma ,\n')
    z.writestr('Curly {}.txt', 'These are curly braces {}\n')
    z.writestr('Dollar ($).txt', 'This is a dollar sign $\n')
    z.writestr('Double (").txt', 'This is a double quote "\n')
    z.writestr('Escape (\x1b).txt', 'This is an escape character \x1b\n')
    z.writestr('Hash (#).txt', 'This is a hash sign #\n')
    z.writestr('Newline (\n).txt', 'This is a newline\n')
    z.writestr('Percent (%).txt', 'This is a percent sign %\n')
    z.writestr('Plus (+).txt', 'This is a plus sign +\n')
    z.writestr('Question (?).txt', 'This is a question mark ?\n')
    z.writestr("Quote (').txt", "This is a single quote '\n")
    z.writestr('Semicolon (;).txt', 'This is a semicolon ;\n')
    z.writestr('Square [].txt', 'These are square brackets []\n')
    z.writestr('Star (*).txt', 'This is a star *\n')
    z.writestr('Tab (\t).txt', 'This is a tab \t\n')
    z.writestr('Tilde (~).txt', 'This is a tilde ~\n')
    z.writestr('Empty/', 'This is an empty directory.\n')
    z.writestr('/Empty/', 'This is an empty directory.\n')

  with ZipFile(os.path.join(dir, 'data', 'long-names.zip'), 'w') as z:
    z.writestr('Short name.txt', 'This is a short long name.\n')
    z.writestr(
        '255 ' + 'z' * (255 - 8) + '.txt',
        'This file name has 255 characters.\n',
    )
    z.writestr(
        '256 ' + 'z' * (256 - 8) + '.txt',
        'This file name has 256 characters.\n',
    )
    z.writestr(
        '511 ' + 'z' * (511 - 8) + '.txt',
        'This file name has 511 characters.\n',
    )
    z.writestr(
        '1023 ' + 'z' * (1023 - 9) + '.txt',
        'This file name has 1023 characters.\n',
    )
    z.writestr(
        '1024 ' + 'z' * (1024 - 9) + '.txt',
        'This file name has 1024 characters.\n',
    )
    z.writestr(
        '1025 ' + 'z' * (1025 - 9) + '.txt',
        'This file name has 1025 characters.\n',
    )
    z.writestr(
        ('a' * 255 + '/') * 16 + 'z' * 255, 'This is a very long path.\n'
    )


MakeZipWithManySmallFiles()
MakeOtherZips()

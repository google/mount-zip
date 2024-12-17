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
  with ZipFile(os.path.join(dir, 'data', 'relative-to-current.zip'), 'w') as z:
    z.writestr('./', '')
    z.writestr('./hi.txt', 'This is ./hi.txt\n')

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
    for v in ['first', 'second', 'third']:
      for s in [
          '../One Level Up',
          '../../Two Levels Up',
          '../../../Three Levels Up',
          '/At The Top',
          '/../Over The Top',
          '/',
          './',
          '../',
          '.',
          '..',
          '...',
          '... (1)',
          '....',
          '.' * 255,
          '.' * 256,
          '/.',
          '/./',
          'a/.',
          'a/./',
          'a/./b',
          'a/..',
          'a/../',
          'a/../b',
          '.One',
          '..Two',
          '...Three',
          '...Three (1)',
          'One.',
          'Two..',
          'Three...',
          'Three... (1)',
          'foo.a b',
          'foo.a b (1)',
          '.foo.txt',
          'foo.tar.gz',
          'foo (1).tar.gz',
          '.foo (1).txt',
          'Tab \t',
          'Star *',
          'Dot .',
          'Ampersand &',
          'Hash #',
          'Dollar $',
          'Euro €',
          'Pipe |',
          'Smile 🙂',
          'Tilde ~',
          'Colon :',
          'Semicolon ;',
          'Percent %',
          'Caret ^',
          'At @',
          'Comma ,',
          'Exclamation !',
          'Dash -',
          'Plus +',
          'Equal =',
          'Underscore _',
          'Question ?',
          'Backtick `',
          "Quote '",
          'Double quote "',
          'Backslash \\',
          '\\',
          'C:\\Temp\\File',
          '\\\\server\\share\\file',
          'u/v//w///x//y/z',
          ' ',
          '~',
          '%TMP%',
          '$HOME',
          '-',
          'Space→ ',
          ' ←Space',
          'Angle <>',
          'Square []',
          'Round ()',
          'Curly {}',
          'Delete \x7F',
          'Escape \x1B',
          'Backspace \x08',
          'Line Feed \n',
          'Carriage Return \r',
          'Bell \a',
          'String Terminator \u009C',
          'Empty/',
          '/Empty/',
          'FileOrDir',
          'FileOrDir/',
          'Case',
          'case',
          'CASE',
          'Café',
          'Cafe\u0301',
          'NUL',
          'CON',
          'PRN',
          'AUX',
          'COM1',
          'COM9',
          'LPT1',
          'LPT9',
          'CLOCK$',
          '/dev/null',
          '🙂' * 63,
          'a' + '🙂' * 63,
          'ab' + '🙂' * 63,
          'abc' + '🙂' * 63,
          'abcd' + '🙂' * 63,
          '\U0001F3F3\u200D\U0001F308' * 23,
      ]:
        z.writestr(s, f'This is the {v} version of {s!r}')

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

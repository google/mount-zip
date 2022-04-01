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

from zipfile import ZipFile, ZIP_DEFLATED, ZIP_BZIP2, ZIP_LZMA
import os.path

dir = os.path.dirname(os.path.realpath(__file__))


def MakeZipWithManySmallFiles():
    with ZipFile(os.path.join(dir, 'data', '100000-files.zip'),
                 'w',
                 allowZip64=True) as z:
        for i in range(100000):
            z.writestr('%06d.txt' % i, 'This is file %06d.\n' % i)


def MakeOtherZips():
    with ZipFile(os.path.join(dir, 'data', 'file-name-same-name-as-dir.zip'),
                 'w') as z:
        z.writestr('repeated/', '')
        z.writestr('repeated', 'First file')

    with ZipFile(os.path.join(dir, 'data', 'empty-dir-same-name-as-file.zip'),
                 'w') as z:
        z.writestr('repeated', 'First file')
        z.writestr('repeated/', '')

    with ZipFile(os.path.join(dir, 'data', 'parent-dir-same-name-as-file.zip'),
                 'w') as z:
        z.writestr('repeated', 'First file')
        z.writestr('repeated/second', 'Second file')

    with ZipFile(os.path.join(dir, 'data', 'repeated-file-name.zip'),
                 'w') as z:
        z.writestr('repeated', 'First file')
        z.writestr('repeated', 'Second file')

    with ZipFile(os.path.join(dir, 'data', 'repeated-dir-name.zip'), 'w') as z:
        z.writestr('repeated/', '')
        z.writestr('repeated/', '')

    with ZipFile(os.path.join(dir, 'data', 'mixed-case.zip'), 'w') as z:
        z.writestr('Case', 'Mixed case 111')
        z.writestr('case', 'Lower case 22')
        z.writestr('CASE', 'Upper case 3')

    with ZipFile(os.path.join(dir, 'data', 'file-dir-same-name.zip'),
                 'w') as z:
        z.writestr('pet/cat', 'This is my first pet cat\n')
        z.writestr('pet', 'This is my first pet\n')
        z.writestr('pet/cat/fish', 'This is my first pet cat fish\n')
        z.writestr('pet/cat/fish/', '')
        z.writestr('pet/cat', 'This is my second pet cat\n')
        z.writestr('pet', 'This is my second pet\n')
        z.writestr('pet/cat/fish', 'This is my second pet cat fish\n')

    with ZipFile(os.path.join(dir, 'data', 'windows-specials.zip'), 'w') as z:
        for s in [
                'First',
                'NUL',
                'NUL ',
                'NUL.',
                'NUL .',
                'NUL.txt',
                'NUL.tar.gz',
                'NUL..txt',
                'NUL...txt',
                'NUL .txt',
                'NUL  .txt',
                'NUL  ..txt',
                'Nul.txt',
                'nul.very long extension',
                ' NUL.txt',
                'a/NUL',
                'CON',
                'PRN',
                'AUX',
                'COM1',
                'COM2',
                'COM3',
                'COM4',
                'COM5',
                'COM6',
                'COM7',
                'COM8',
                'COM9',
                'LPT1',
                'LPT2',
                'LPT3',
                'LPT4',
                'LPT5',
                'LPT6',
                'LPT7',
                'LPT8',
                'LPT9',
                'CLOCK$',
                'Last',
        ]:
            z.writestr(s, 'This is: ' + s)

    with ZipFile(os.path.join(dir, 'data', 'lzma.zip'),
                 'w',
                 compression=ZIP_LZMA) as z:
        z.writestr('lzma.txt', 'This file is compressed with LZMA.\n')

    with ZipFile(os.path.join(dir, 'data', 'bzip2.zip'),
                 'w',
                 compression=ZIP_BZIP2) as z:
        z.writestr('bzip2.txt', 'This file is compressed with BZIP2.\n')

    with ZipFile(os.path.join(dir, 'data', 'empty.zip'), 'w') as z:
        pass

    with ZipFile(os.path.join(dir, 'data', 'mixed-paths.zip'), 'w') as z:
        for s in [
                'First',
                '../One Level Up',
                '../../Two Levels Up',
                '/At The Top',
                '/../Over The Top',
                '/',
                '../',
                '.',
                '..',
                '...',
                '....',
                '/.',
                'a/.',
                'a/./',
                'a/./b',
                'a/..',
                'a/../',
                'a/../b',
                '.One',
                '..Two',
                '...Three',
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
                'Quote \'',
                'Double quote \"',
                'Backslash1→\\',
                '\\←Backslash2',
                'Backslash3→\\←Backslash4',
                'C:',
                'C:\\',
                'C:\\Temp',
                'C:\\Temp\\',
                'C:\\Temp\\File',
                '\\\\server\\share\\file',
                'u/v//w///x//y/z',
                ' ',
                '~',
                '%TMP',
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
                'FileOrDir/File',
                'Case',
                'case',
                'CASE',
                'NUL',
                'NUL ',
                'NUL.',
                'NUL .',
                'NUL.txt',
                'NUL.tar.gz',
                'NUL..txt',
                'NUL...txt',
                'NUL .txt',
                'NUL  .txt',
                'NUL  ..txt',
                'nul.very long extension',
                ' NUL.txt',
                'c/NUL',
                'CON',
                'PRN',
                'AUX',
                'COM1',
                'COM2',
                'COM3',
                'COM4',
                'COM5',
                'COM6',
                'COM7',
                'COM8',
                'COM9',
                'LPT1',
                'LPT2',
                'LPT3',
                'LPT4',
                'LPT5',
                'LPT6',
                'LPT7',
                'LPT8',
                'LPT9',
                'CLOCK$',
                '/dev/null',
                'Last',
        ]:
            z.writestr(s, 'This is: ' + s)

    with ZipFile(os.path.join(dir, 'data', 'long-names.zip'), 'w') as z:
        z.writestr('Short name.txt', 'This is a short long name.\n')
        z.writestr('255 ' + 'z' * (255 - 8) + '.txt',
                   'This file name has 255 characters.\n')
        z.writestr('256 ' + 'z' * (256 - 8) + '.txt',
                   'This file name has 256 characters.\n')
        z.writestr('511 ' + 'z' * (511 - 8) + '.txt',
                   'This file name has 511 characters.\n')
        z.writestr('1023 ' + 'z' * (1023 - 9) + '.txt',
                   'This file name has 1023 characters.\n')
        z.writestr('1024 ' + 'z' * (1024 - 9) + '.txt',
                   'This file name has 1024 characters.\n')
        z.writestr('1025 ' + 'z' * (1025 - 9) + '.txt',
                   'This file name has 1025 characters.\n')
        z.writestr(('a' * 255 + '/') * 16 + 'z' * 255,
                   'This is a very long path.\n')


MakeZipWithManySmallFiles()
MakeOtherZips()

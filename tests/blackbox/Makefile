# Copyright 2021 Google LLC
# Copyright 2010-2021 Alexander Galanin <al@galanin.nnov.ru>
# http://galanin.nnov.ru/~al
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

all: check

check: data/big.zip data/collisions.zip
	python3 test.py

check-fast:
	python3 test.py --fast

data/big.zip: make_big_zip.py
	python3 make_big_zip.py

data/collisions.zip: make_collisions.py
	python3 make_collisions.py

rebuild:
	make -C ../../lib all
	make -C ../.. all

force-rebuild:
	make -C ../.. clean all

.PHONY: all check check-fast rebuild force-rebuild

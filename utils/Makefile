# Copyright 2021 Google LLC
# Copyright 2019-2021 Alexander Galanin <al@galanin.nnov.ru>
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

PKG_CONFIG ?= pkg-config
LIBS = -L../lib -lmountzip $(shell $(PKG_CONFIG) libzip --libs)
CXXFLAGS += $(shell $(PKG_CONFIG) libzip --cflags)
CXXFLAGS += -Wall -Wextra -Wno-nullability-extension -Wno-sign-compare -Wno-missing-field-initializers -pedantic -std=c++23
CXXFLAGS += -D_FILE_OFFSET_BITS=64 

ifeq ($(DEBUG), 1)
CXXFLAGS += -O0 -g
else
CXXFLAGS += -O2 -DNDEBUG
endif

all: dump-extrafld create-content-size

dump-extrafld: dump-extrafld.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $< -I.. $(LDFLAGS) $(LIBS) -o $@

create-content-size: create-content-size.cpp
	$(CXX) $(CXXFLAGS) $< -I.. $(LDFLAGS) $(LIBS) -o $@

clean:
	rm -f dump-extrafld create-content-size

.PHONY: all clean

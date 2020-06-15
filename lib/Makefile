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

DEST=libfusezip.a
PKG_CONFIG ?= pkg-config
PC_DEPS = fuse libzip icu-uc icu-i18n
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
COMMON_CXXFLAGS = -Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers -pedantic -std=c++20
CXXFLAGS = -g -O0 $(COMMON_CXXFLAGS)
RELEASE_CXXFLAGS = -O2 $(COMMON_CXXFLAGS) -DNDEBUG
SOURCES=$(sort $(wildcard *.cc))
OBJECTS=$(SOURCES:.cc=.o)
CLEANFILES=$(OBJECTS) $(DEST)

all: $(DEST)

release:
	$(MAKE) CXXFLAGS="$(RELEASE_CXXFLAGS)" all

$(DEST): $(OBJECTS)
	$(AR) -cr $@ $(OBJECTS)

.cc.o:
	$(CXX) -c $(CXXFLAGS) $(PC_CFLAGS) $< -o $@

clean:
	rm -f $(DEST) $(OBJECTS)

.PHONY: all clean


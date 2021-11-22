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

DEST = libfusezip.a
PKG_CONFIG ?= pkg-config
DEPS = fuse libzip icu-uc icu-i18n
CXXFLAGS := $(shell $(PKG_CONFIG) --cflags $(DEPS))
CXXFLAGS += -Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers -pedantic -std=c++20
ifeq ($(DEBUG), 1)
CXXFLAGS += -O0 -g
else
CXXFLAGS += -O2 -DNDEBUG
endif
SOURCES = $(sort $(wildcard *.cc))
OBJECTS = $(SOURCES:.cc=.o)
CLEANFILES = $(OBJECTS) $(DEST)

all: $(DEST)

debug:
	$(MAKE) DEBUG=1 all

$(DEST): $(OBJECTS)
	$(AR) -cr $@ $(OBJECTS)

.cc.o:
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f $(DEST) $(OBJECTS)

.PHONY: all clean debug

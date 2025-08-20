# Copyright 2021 Google LLC
# Copyright 2008-2021 Alexander Galanin <al@galanin.nnov.ru>
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

DEST = mount-zip
PREFIX = $(DESTDIR)/usr
BINDIR = $(PREFIX)/bin
PKG_CONFIG ?= pkg-config

FUSE_MAJOR_VERSION ?= 3
ifeq ($(FUSE_MAJOR_VERSION), 3)
DEPS = fuse3
CXXFLAGS += -DFUSE_USE_VERSION=30
else ifeq ($(FUSE_MAJOR_VERSION), 2)
DEPS = fuse
CXXFLAGS += -DFUSE_USE_VERSION=26
endif

DEPS += libzip icu-uc icu-i18n
LDFLAGS += -Llib -lmountzip
LDFLAGS += $(shell $(PKG_CONFIG) --libs $(DEPS))
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags $(DEPS))
CXXFLAGS += -Wall -Wextra -Wno-nullability-extension -Wno-sign-compare -Wno-missing-field-initializers -pedantic -std=c++20
CXXFLAGS += -D_FILE_OFFSET_BITS=64 

ifeq ($(DEBUG), 1)
CXXFLAGS += -O0 -g
else
CXXFLAGS += -O2 -DNDEBUG
endif

LIB = lib/libmountzip.a
SOURCES = $(DEST).cc
OBJECTS = $(SOURCES:.cc=.o)
MAN = $(DEST).1
MANDIR = $(PREFIX)/share/man/man1
INSTALL = install

all: $(DEST)

doc: $(MAN)
	man -l $(MAN)

$(DEST): $(OBJECTS) $(LIB)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

.cc.o:
	$(CXX) -Ilib -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@ -MMD -MP -MF $(@:.o=.d)

-include $(SOURCES:.cc=.d)

$(LIB):
	$(MAKE) -C lib

clean:
	rm -f $(DEST) *.o *.d
	$(MAKE) -C tests clean
	$(MAKE) -C lib clean

$(MAN): README.md
	pandoc $< -s -t man -o $@

install: $(DEST)
	$(INSTALL) -D "$(DEST)" "$(BINDIR)/$(DEST)"
	$(INSTALL) -D -m 644 $(MAN) "$(MANDIR)/$(MAN)"

install-strip: $(DEST)
	$(INSTALL) -D -s "$(DEST)" "$(BINDIR)/$(DEST)"
	$(INSTALL) -D -m 644 $(MAN) "$(MANDIR)/$(MAN)"

uninstall:
	rm "$(BINDIR)/$(DEST)" "$(MANDIR)/$(MAN)"

debug:
	$(MAKE) DEBUG=1 all

check: $(DEST)
	$(MAKE) -C tests check

check-fast: $(DEST)
	$(MAKE) -C tests check-fast

.PHONY: all doc debug clean check-fast install uninstall check $(LIB)

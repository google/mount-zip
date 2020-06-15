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

DEST=mount-zip
prefix=/usr
exec_prefix=$(prefix)
bindir=$(exec_prefix)/bin
PKG_CONFIG ?= pkg-config
PC_DEPS = fuse libzip icu-uc icu-i18n
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
LIBS := -Llib -lfusezip $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
COMMON_CXXFLAGS = -Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers -pedantic -std=c++20
CXXFLAGS = -g -O0 $(COMMON_CXXFLAGS)
RELEASE_CXXFLAGS = -O2 $(COMMON_CXXFLAGS) -DNDEBUG
LIB=lib/libfusezip.a
SOURCES=main.cc
OBJECTS=$(SOURCES:.cc=.o)
MAN=$(DEST).1
MANDIR=$(prefix)/share/man/man1
CLEANFILES=$(OBJECTS) $(DEST) $(MAN)
INSTALL=install -D
INSTALL_PROGRAM=$(INSTALL)
INSTALL_DATA=$(INSTALL) -m 644

all: $(DEST)

doc: $(MAN)

$(DEST): $(OBJECTS) $(LIB)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LIBS) \
	    -o $@

main.o: main.cc
	$(CXX) -c $(CXXFLAGS) $(PC_CFLAGS) $< \
	    -Ilib \
	    -o $@

$(LIB):
	$(MAKE) -C lib

lib-clean:
	$(MAKE) -C lib clean

clean: lib-clean all-clean check-clean

all-clean:
	rm -f $(CLEANFILES)

$(MAN): README.md
	pandoc $< -s -t man -o $@

install: all doc
	$(INSTALL_PROGRAM) "$(DEST)" "$(DESTDIR)$(bindir)/$(DEST)"
	$(INSTALL_DATA) $(MAN) "$(DESTDIR)$(MANDIR)/$(MAN)"

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

uninstall:
	rm "$(DESTDIR)$(bindir)/$(DEST)" "$(DESTDIR)$(MANDIR)/$(MAN)"

release:
	$(MAKE) CXXFLAGS="$(RELEASE_CXXFLAGS)" all doc

check: $(DEST)
	$(MAKE) -C tests

.PHONY: all doc release clean all-clean lib-clean check-clean install uninstall check $(LIB)


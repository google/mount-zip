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
prefix = /usr
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
PKG_CONFIG ?= pkg-config
DEPS = fuse libzip icu-uc icu-i18n
LIBS := -Llib -lfusezip $(shell $(PKG_CONFIG) --libs $(DEPS))
LIBS += -Llib -lfusezip
CXXFLAGS := $(shell $(PKG_CONFIG) --cflags $(DEPS))
CXXFLAGS += -Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers -pedantic -std=c++20
ifeq ($(DEBUG), 1)
CXXFLAGS += -O0 -g
else
CXXFLAGS += -O2 -DNDEBUG
endif
LIB = lib/libfusezip.a
SOURCES = main.cc
OBJECTS = $(SOURCES:.cc=.o)
MAN = $(DEST).1
MANDIR = $(prefix)/share/man/man1
CLEANFILES = $(OBJECTS) $(DEST) $(MAN)
INSTALL = install -D
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

all: $(DEST)

doc: $(MAN)
	man -l $(MAN)

$(DEST): $(OBJECTS) $(LIB)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@

main.o: main.cc
	$(CXX) -Ilib -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(LIB):
	$(MAKE) -C lib

lib-clean:
	$(MAKE) -C lib clean

clean: lib-clean all-clean check-clean

all-clean:
	rm -f $(CLEANFILES)

$(MAN): README.md
	pandoc $< -s -t man -o $@

install: $(DEST) $(MAN)
	$(INSTALL_PROGRAM) "$(DEST)" "$(DESTDIR)$(bindir)/$(DEST)"
	$(INSTALL_DATA) $(MAN) "$(DESTDIR)$(MANDIR)/$(MAN)"

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

uninstall:
	rm "$(DESTDIR)$(bindir)/$(DEST)" "$(DESTDIR)$(MANDIR)/$(MAN)"

debug:
	$(MAKE) DEBUG=1 all

check: debug
	$(MAKE) -C tests

.PHONY: all doc debug clean all-clean lib-clean check-clean install uninstall check $(LIB)

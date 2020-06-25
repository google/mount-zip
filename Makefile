DEST=fuse-zip
prefix=/usr/local
exec_prefix=$(prefix)
bindir=$(exec_prefix)/bin
datarootdir=$(prefix)/share
docdir=$(datarootdir)/doc/$(DEST)
mandir=$(datarootdir)/man
man1dir=$(mandir)/man1
manext=.1
LIBS=-Llib -lfusezip $(shell $(PKG_CONFIG) fuse --libs) $(shell $(PKG_CONFIG) libzip --libs)
LIB=lib/libfusezip.a
CXXFLAGS=-g -O0 -Wall -Wextra -Wconversion -Wsign-conversion -Wlogical-op -Wshadow -pedantic -Werror -std=c++11
RELEASE_CXXFLAGS=-O2 -Wall -Wextra -Wconversion -Wsign-conversion -Wlogical-op -Wshadow -pedantic -Werror -std=c++11
PKG_CONFIG?=pkg-config
FUSEFLAGS=$(shell $(PKG_CONFIG) fuse --cflags)
ZIPFLAGS=$(shell $(PKG_CONFIG) libzip --cflags)
SOURCES=main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
MANSRC=fuse-zip.1
MAN=fuse-zip$(manext).gz
CLEANFILES=$(OBJECTS) $(MAN)
DOCFILES=README.md changelog
INSTALL=install
INSTALL_PROGRAM=$(INSTALL)
INSTALL_DATA=$(INSTALL) -m 644

all: $(DEST)

doc: $(MAN)

doc-clean: man-clean

$(DEST): $(OBJECTS) $(LIB)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LIBS) \
	    -o $@

main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(ZIPFLAGS) $< \
	    -Ilib \
	    -o $@

$(LIB):
	$(MAKE) -C lib

lib-clean:
	$(MAKE) -C lib clean

distclean: clean doc-clean
	rm -f $(DEST)

clean: lib-clean all-clean check-clean tarball-clean

all-clean:
	rm -f $(CLEANFILES)

$(MAN): $(MANSRC)
	gzip -c9 $< > $@

man-clean:
	rm -f $(MANSRC).gz

install: all doc
	mkdir -p "$(DESTDIR)$(bindir)/"
	$(INSTALL_PROGRAM) "$(DEST)" "$(DESTDIR)$(bindir)/"
	mkdir -p "$(DESTDIR)$(docdir)/"
	$(INSTALL_DATA) $(DOCFILES) "$(DESTDIR)$(docdir)/"
	mkdir -p "$(DESTDIR)$(man1dir)/"
	$(INSTALL_DATA) $(MAN) "$(DESTDIR)$(man1dir)/"

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

uninstall:
	rm "$(DESTDIR)$(bindir)/$(DEST)"
	rm -r "$(DESTDIR)$(docdir)"
	rm "$(DESTDIR)$(man1dir)/$(MAN)"

dist:
	./makeArchives.sh

tarball-clean:
	rm -f fuse-zip-*.tar.gz fuse-zip-tests-*.tar.gz

release:
	$(MAKE) CXXFLAGS="$(RELEASE_CXXFLAGS)" all doc

check: $(DEST)
	$(MAKE) -C tests

check-clean:
	$(MAKE) -C tests clean

valgrind:
	$(MAKE) -C tests valgrind

.PHONY: all release doc clean all-clean lib-clean doc-clean check-clean tarball-clean distclean install uninstall dist check valgrind $(LIB)


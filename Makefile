DEST=fuse-zip
prefix=/usr/local
exec_prefix=$(prefix)
bindir=$(exec_prefix)/bin
datarootdir=$(prefix)/share
docdir=$(datarootdir)/doc/$(DEST)
mandir=$(datarootdir)/man
man1dir=$(mandir)/man1
manext=.1
LIBS=-Llib -lfusezip $(shell pkg-config fuse --libs) $(shell pkg-config libzip --libs)
LIB=lib/libfusezip.a
CXXFLAGS=-g -O0 -Wall -Wextra
RELEASE_CXXFLAGS=-O2 -Wall -Wextra
FUSEFLAGS=$(shell pkg-config fuse --cflags)
ZIPFLAGS=$(shell pkg-config libzip --cflags)
SOURCES=main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
MANSRC=fuse-zip.1
MAN=fuse-zip$(manext).gz
CLEANFILES=$(OBJECTS) $(MAN)
DOCFILES=README changelog
INSTALL=install
INSTALL_PROGRAM=$(INSTALL)
INSTALL_DATA=$(INSTALL) -m 644

all: $(DEST)

doc: $(MAN)

doc-clean: man-clean

$(DEST): $(OBJECTS) $(LIB)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LIBS) \
	    -o $@

# main.cpp must be compiled separately with FUSEFLAGS
main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(ZIPFLAGS) $< \
	    -Ilib \
	    -o $@

$(LIB):
	make -C lib

lib-clean:
	make -C lib clean

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
	$(INSTALL_PROGRAM) "$(DEST)" "$(DESTDIR)$(bindir)/"
	mkdir -p "$(DESTDIR)$(docdir)/"
	$(INSTALL_DATA) $(DOCFILES) "$(DESTDIR)$(docdir)/"
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
	make CXXFLAGS="$(RELEASE_CXXFLAGS)" all doc

check: $(DEST)
	make -C tests

check-clean:
	make -C tests clean

valgrind:
	make -C tests valgrind

.PHONY: all release doc clean all-clean lib-clean doc-clean check-clean tarball-clean distclean install uninstall dist check valgrind $(LIB)


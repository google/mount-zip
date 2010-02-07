DEST=fuse-zip
LIBS=$(shell pkg-config fuse --libs) $(shell pkg-config libzip --libs) -Llib -lfusezip
CXXFLAGS:=$(CXXFLAGS) -Wall -Wextra
FUSEFLAGS=$(shell pkg-config fuse --cflags)
SOURCES=main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
MANSRC=fuse-zip.1
MAN=fuse-zip.1.gz
CLEANFILES=$(OBJECTS) $(MAN)
DOCFILES=README changelog
INSTALLPREFIX=/usr

all: $(DEST)

doc: $(MAN)

doc-clean: man-clean

$(DEST): $(OBJECTS) lib
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) \
	    -o $@

# main.cpp must be compiled separately with FUSEFLAGS
main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $< \
	    -Ilib \
	    -o $@

lib:
	make -C lib

lib-clean:
	make -C lib clean

distclean: clean doc-clean
	rm -f $(DEST)

clean: lib-clean all-clean test-clean tarball-clean

all-clean:
	rm -f $(CLEANFILES)

$(MAN): $(MANSRC)
	gzip -c9 $< > $@

man-clean:
	rm -f $(MANSRC).gz

install: all doc
	mkdir -p "$(INSTALLPREFIX)/bin"
	install -m 755 -s "$(DEST)" "$(INSTALLPREFIX)/bin"
	mkdir -p "$(INSTALLPREFIX)/share/doc/$(DEST)"
	cp $(DOCFILES) "$(INSTALLPREFIX)/share/doc/$(DEST)"
	mkdir -p "$(INSTALLPREFIX)/share/man/man1"
	cp $(MAN) "$(INSTALLPREFIX)/share/man/man1"

uninstall:
	rm "$(INSTALLPREFIX)/bin/$(DEST)"
	rm -r "$(INSTALLPREFIX)/share/doc/$(DEST)"
	rm "$(INSTALLPREFIX)/share/man/man1/$(MAN)"

tarball:
	./makeArchives.sh

tarball-clean:
	rm -f fuse-zip-*.tar.gz fuse-zip-tests-*.tar.gz

debug:
	make CXXFLAGS="-g $(CXXFLAGS)"

test: $(DEST)
	make -C tests

test-clean:
	make -C tests clean

valgrind: clean debug
	make -C tests valgrind

.PHONY: all lib doc clean all-clean lib-clean doc-clean test-clean tarball-clean distclean install uninstall tarball test valgrind


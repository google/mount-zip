DEST=fuse-zip
LIBS=$(shell pkg-config fuse --libs) $(shell pkg-config libzip --libs)
CXXFLAGS:=$(CXXFLAGS) -Wall -Wextra
FUSEFLAGS=$(shell pkg-config fuse --cflags)
ZIPFLAGS=$(shell pkg-config libzip --cflags)
SOURCES=fuse-zip.cpp fileNode.cpp bigBuffer.cpp fuseZipData.cpp libZipWrapper.cpp
OBJECTS=$(SOURCES:.cpp=.o)
CLEANFILES=$(OBJECTS)
DOCFILES=LICENSE README INSTALL changelog
INSTALLPREFIX=/usr

all: $(DEST)

$(DEST): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

#fuse-zip.cpp must be compiled separately with FUSEFLAGS
fuse-zip.o: fuse-zip.cpp
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(ZIPFLAGS) $< -o $@

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(ZIPFLAGS) $< -o $@

distclean: clean
	rm -f $(DEST)

clean:
	rm -f $(CLEANFILES)

install: $(DEST)
	mkdir -p "$(INSTALLPREFIX)/bin"
	install -m 755 -s -t "$(INSTALLPREFIX)/usr/bin" "$(DEST)"
	mkdir -p "$(INSTALLPREFIX)/share/doc/$(DEST)"
	cp $(DOCFILES) "$(INSTALLPREFIX)/share/doc/$(DEST)"
	mkdir -p "$(INSTALLPREFIX)/share/man/man1"
	cp fuse-zip.1.gz "$(INSTALLPREFIX)/share/man/man1"

uninstall:
	rm "$(INSTALLPREFIX)/bin/$(DEST)"
	rm -r "$(INSTALLPREFIX)/share/doc/$(DEST)"
	rm "$(INSTALLPREFIX)/share/man/man1/fuse-zip.1.gz"


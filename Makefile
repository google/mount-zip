DEST=fuse-zip
LIBS=$(shell pkg-config fuse --libs) $(shell pkg-config libzip --libs)
CXXFLAGS=-Wall -Wextra $(shell pkg-config fuse --cflags) $(shell pkg-config libzip --cflags)
SOURCES=fuse-zip.cpp fileNode.cpp bigBuffer.cpp fuseZipData.cpp
OBJECTS=$(SOURCES:.cpp=.o)
CLEANFILES=$(DEST) $(OBJECTS)
INSTALLPREFIX=

all: $(SOURCES) $(DEST)

$(DEST): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f $(CLEANFILES)

install: $(DEST)
	mkdir -p $(INSTALLPREFIX)/usr/bin
	install -m 755 -s -t $(INSTALLPREFIX)/usr/bin $(DEST)

uninstall:
	rm $(INSTALLPREFIX)/usr/bin/$(DEST)


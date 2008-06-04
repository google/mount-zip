DEST=fuse-zip
CLEANFILES=$(DEST)
LIBS=`pkg-config fuse --cflags --libs` `pkg-config libzip --cflags --libs`
WARN=-Wall
INSTALLPREFIX=

$(DEST): fuse-zip.cpp
	$(CXX) $(CXXFLAGS) $(WARN) $(LIBS) fuse-zip.cpp -o $(DEST)

clean:
	rm -f $(CLEANFILES)

install: $(DEST)
	install -m 755 -s -t $(INSTALLPREFIX)/usr/bin $(DEST)

uninstall:
	rm $(INSTALLPREFIX)/usr/bin/$(DEST)


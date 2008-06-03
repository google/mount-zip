DEST=fuse-zip
CLEANFILES=$(DEST)
LIBS=`pkg-config fuse --cflags --libs` `pkg-config libzip --cflags --libs`
WARN=-Wall

$(DEST): fuse-zip.cpp
	$(CXX) $(CXXFLAGS) $(WARN) $(LIBS) fuse-zip.cpp -o $(DEST)

clean:
	rm -f $(CLEANFILES)


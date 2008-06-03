DEST=fuse-zip
CLEANFILES=$(DEST)
LIBS=`pkg-config fuse --cflags --libs` `pkg-config libzip --cflags --libs` -lzip
WARN=-Wall

$(DEST): fuse-zip.cpp
	$(CXX) $(CXXFLAGS) $(WARN) $(LIBS) fuse-zip.cpp -o $(DEST)

clean:
	rm -f $(CLEANFILES)

test: $(DEST)
	mkdir -p testDir
	./$(DEST) test.zip testDir
	ls testDir || true
	cat testDir/hello || true
	fusermount -u testDir
	rmdir testDir


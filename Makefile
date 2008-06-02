DEST=fuze-zip
CLEANFILES=$(DEST)
LIBS=`pkg-config fuse --cflags --libs` `pkg-config libzip --cflags --libs` -lzip

$(DEST): fuse-zip.c
	$(CC) $(CCFLAGS) -Wall $(LIBS) fuse-zip.c -o $(DEST)

clean:
	rm -f $(CLEANFILES)

test: $(DEST)
	mkdir -p testDir
	./$(DEST) testDir
	ls testDir
	cat testDir/hello
	fusermount -u testDir
	rmdir testDir


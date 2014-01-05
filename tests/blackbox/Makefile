rebuild:
	make -C ../../lib all
	make -C ../.. all

force-rebuild:
	make -C ../.. clean all

all:
	./fuse-zip.test

valgrind:
	./fuse-zip.test -valgrind

.PHONY: all rebuild force-rebuild

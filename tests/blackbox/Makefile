all:
	./fuse-zip.test
rebuild:
	make -C ../../lib all
	make -C ../.. all

force-rebuild:
	make -C ../.. clean all

valgrind:
	./fuse-zip.test -valgrind

.PHONY: all valgrind rebuild force-rebuild

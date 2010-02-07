all:
	make -C blackbox
	make -C whitebox test

valgrind:
	make -C blackbox valgrind
	make -C whitebox valgrind

.PHONY: all valgrind


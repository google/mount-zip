all:
	make -C blackbox all
	make -C whitebox test

valgrind:
	make -C blackbox valgrind
	make -C whitebox valgrind

clean:
	make -C whitebox clean

.PHONY: all valgrind clean


all:
	./all.tcl | tee test.log
	if grep -q 'Files with failing tests' test.log; then exit 1; fi

valgrind:
	VALGRIND=1 ./all.tcl | tee test.log
	if grep -q 'Files with failing tests' test.log; then exit 1; fi

.PHONY: all

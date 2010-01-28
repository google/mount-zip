all:
	./all.tcl | tee test.log
	if grep -q 'Files with failing tests' test.log; then exit 1; fi

.PHONY: all

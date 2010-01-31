SUITES=blackbox whitebox

all: $(SUITES)

valgrind:
	make -C blackbox valgrind

$(SUITES):
	make -C $@

.PHONY: all $(SUITES)


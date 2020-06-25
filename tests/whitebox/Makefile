CXXFLAGS=-g -O2 -Wall -Wextra -Werror -std=c++11
PKG_CONFIG?=pkg-config
FUSEFLAGS=$(shell $(PKG_CONFIG) fuse --cflags)
ZIPFLAGS=$(shell $(PKG_CONFIG) libzip --cflags)
VALGRIND=valgrind -q --leak-check=full --track-origins=yes --error-exitcode=33
LIB=../../lib/libfusezip.a

SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
DEST=$(OBJECTS:.o=.x)
TESTS=$(DEST:.x=.test)
VALGRIND_TESTS=$(DEST:.x=.valgrind)

all: $(DEST)

$(DEST): %.x: %.o $(LIB)
	$(CXX) $(LDFLAGS) $< \
	    -L../../lib -lfusezip \
	    -o $@

$(OBJECTS): %.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(ZIPFLAGS) \
	    -I../../lib \
	    $< -o $@

$(LIB):
	make -C ../../lib

lib-clean:
	make -C ../../lib clean

clean: lib-clean test-clean
	rm -f *.o $(DEST) $(OBJECTS)

distclean: clean
	make -C ../../lib clean

list:
	@echo $(TESTS)

test: $(TESTS)

test-clean:
	rm -f *.x.core vgcore.*

$(TESTS): %.test: %.x
	./$<

$(VALGRIND_TESTS): %.valgrind: %.x
	$(VALGRIND) ./$<

valgrind: $(VALGRIND_TESTS)

.PHONY: all clean distclean test test-clean valgrind lib-clean $(LIB)


CXXFLAGS:=$(CXXFLAGS) -g -Wall -Wextra
FUSEFLAGS=$(shell pkg-config fuse --cflags)
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
DEST=$(OBJECTS:.o=.x)
TESTS=$(DEST:.x=.test)

all: $(DEST)

$(DEST): %.x: %.o lib
	$(CXX) $(LDFLAGS) $< \
	    -L../../lib -lfusezip \
	    -o $@

$(OBJECTS): %.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(ZIPFLAGS) \
	    -I../../lib \
	    $< -o $@

lib:
	make -C ../../lib CXXFLAGS="$(CXXFLAGS)"

clean: test-clean
	rm -f *.o $(DEST) $(OBJECTS)

distclean: clean
	make -C ../../lib clean

list:
	@echo $(TESTS)

test: $(TESTS)

test-clean:
	rm -f *.x.core vgcore.*

$(TESTS): %.test: %.x
	$(WRAPPER) ./$<

valgrind: all
	make WRAPPER="valgrind -q --leak-check=full --track-origins=yes --error-exitcode=33" test

.PHONY: all clean distclean test test-clean valgrind lib 


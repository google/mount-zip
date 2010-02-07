CXXFLAGS:=$(CXXFLAGS) -g -Wall -Wextra
FUSEFLAGS=$(shell pkg-config fuse --cflags)
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
DEST=$(OBJECTS:.o=.x)
TESTS=$(DEST:.x=.test)

all: $(DEST)

$(DEST): %.x: %.o
	$(CXX) $(LDFLAGS) $< \
	    -L../../lib -lfusezip \
	    -o $@

$(OBJECTS): %.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(ZIPFLAGS) \
	    -I../../lib \
	    $< -o $@

clean:
	rm -f *.o $(DEST) $(OBJECTS)

list:
	@echo $(TESTS)

test: $(TESTS)

$(TESTS): %.test: %.x
	$(WRAPPER) ./$<

valgrind: all
	make WRAPPER="valgrind --leak-check=full --track-origins=yes --error-exitcode=33" test

.PHONY: all clean test valgrind


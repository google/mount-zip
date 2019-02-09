DEST=libfusezip.a
LIBS=$(shell pkg-config fuse --libs) $(shell pkg-config libzip --libs)
CXXFLAGS=-g -O0 -Wall -Wextra -Wconversion -Wsign-conversion -Wlogical-op -Wshadow -pedantic -Werror
RELEASE_CXXFLAGS=-O2 -Wall -Wextra -Wconversion -Wsign-conversion -Wlogical-op -Wshadow -pedantic -Werror
FUSEFLAGS=$(shell pkg-config fuse --cflags)
ZIPFLAGS=$(shell pkg-config libzip --cflags)
SOURCES=$(sort $(wildcard *.cpp))
OBJECTS=$(SOURCES:.cpp=.o)
CLEANFILES=$(OBJECTS) $(DEST)

all: $(DEST)

release:
	$(MAKE) CXXFLAGS="$(RELEASE_CXXFLAGS)" all

$(DEST): $(OBJECTS)
	$(AR) -cr $@ $(OBJECTS)

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(ZIPFLAGS) $< -o $@

clean:
	rm -f $(DEST) $(OBJECTS)

.PHONY: all clean


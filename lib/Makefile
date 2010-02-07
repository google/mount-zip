DEST=libfusezip.a
LIBS=$(shell pkg-config fuse --libs) $(shell pkg-config libzip --libs)
CXXFLAGS:=$(CXXFLAGS) -Wall -Wextra
FUSEFLAGS=$(shell pkg-config fuse --cflags)
ZIPFLAGS=$(shell pkg-config libzip --cflags)
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)
CLEANFILES=$(OBJECTS) $(DEST)

all: $(DEST)

$(DEST): $(OBJECTS)
	$(AR) -cvq $@ $(OBJECTS)

# fuse-zip.cpp must be compiled separately with FUSEFLAGS
fuse-zip.o: fuse-zip.cpp
	$(CXX) -c $(CXXFLAGS) $(FUSEFLAGS) $(ZIPFLAGS) $< -o $@

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(ZIPFLAGS) $< -o $@

clean:
	rm -f $(DEST) $(OBJECTS)

.PHONY: all clean


LIBS=-L../lib -lfusezip $(shell pkg-config libzip --libs)
CXXFLAGS=-g -O0 -Wall -Wextra -Wlogical-op -Wshadow -pedantic -Werror
ZIPFLAGS=$(shell pkg-config libzip --cflags)

all: dump-extrafld

dump-extrafld.o: dump-extrafld.c
	$(CXX) -c $(CXXFLAGS) $(ZIPFLAGS) $< \
	    -I.. \
	    -o $@

dump-extrafld: dump-extrafld.o $(LIB)
	$(CXX) dump-extrafld.o $(LDFLAGS) $(LIBS) \
	    -o $@

.PHONY: all

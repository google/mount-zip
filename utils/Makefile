PKG_CONFIG?=pkg-config
LIBS=-L../lib -lfusezip $(shell $(PKG_CONFIG) libzip --libs)
CXXFLAGS=-g -O0 -Wall -Wextra -Wlogical-op -Wshadow -pedantic -Werror
ZIPFLAGS=$(shell $(PKG_CONFIG) libzip --cflags)

all: dump-extrafld create-content-size

dump-extrafld: dump-extrafld.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $(ZIPFLAGS) $< \
	    -I.. \
	    $(LDFLAGS) $(LIBS) \
	    -o $@

create-content-size: create-content-size.cpp
	$(CXX) $(CXXFLAGS) $(ZIPFLAGS) $< \
	    -I.. \
	    $(LDFLAGS) $(LIBS) \
	    -o $@

clean:
	rm -f dump-extrafld create-content-size

.PHONY: all clean

# Copyright 2021 Google LLC
# Copyright 2010-2021 Alexander Galanin <al@galanin.nnov.ru>
# http://galanin.nnov.ru/~al
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

CXXFLAGS=-g -O2 -Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers -std=c++20
PKG_CONFIG ?= pkg-config
PC_DEPS = fuse libzip icu-uc icu-i18n
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
LIBS := -L../../lib -lfusezip $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
VALGRIND=valgrind -q --leak-check=full --track-origins=yes --error-exitcode=33
LIB=libfusezip.a

SOURCES=$(wildcard *.cc)
OBJECTS=$(SOURCES:.cc=.o)
DEST=$(OBJECTS:.o=.x)
TESTS=$(DEST:.x=.test)
VALGRIND_TESTS=$(DEST:.x=.valgrind)

all: $(DEST)

$(DEST): %.x: %.o $(LIB)
	$(CXX) $(LDFLAGS) $< $(LIBS) -o $@

$(OBJECTS): %.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(PC_CFLAGS) \
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


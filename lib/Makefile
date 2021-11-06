############################################################################
##  Copyright (C) 2010-2021 by Alexander Galanin                          ##
##  al@galanin.nnov.ru                                                    ##
##  http://galanin.nnov.ru/~al                                            ##
##                                                                        ##
##  Contributors:                                                         ##
##    François Degros <fdegros@chromium.org>, 2020                        ##
##    Reiner Herrmann <reiner@reiner-h.de>, 2016                          ##
##                                                                        ##
##  This program is free software: you can redistribute it and/or modify  ##
##  it under the terms of the GNU General Public License as published by  ##
##  the Free Software Foundation, either version 3 of the License, or     ##
##  (at your option) any later version.                                   ##
##                                                                        ##
##  This program is distributed in the hope that it will be useful,       ##
##  but WITHOUT ANY WARRANTY; without even the implied warranty of        ##
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         ##
##  GNU General Public License for more details.                          ##
##                                                                        ##
##  You should have received a copy of the GNU General Public License     ##
##  along with this program.  If not, see <https://www.gnu.org/licenses/>.##
############################################################################

DEST=libfusezip.a
PKG_CONFIG?=pkg-config
LIBS=$(shell $(PKG_CONFIG) fuse --libs) $(shell $(PKG_CONFIG) libzip --libs)
CXXFLAGS=-g -O0 -Wall -Wextra -Wconversion -Wsign-conversion -Wlogical-op -Wshadow -pedantic -Werror -std=c++11
RELEASE_CXXFLAGS=-O2 -Wall -Wextra -Wconversion -Wsign-conversion -Wlogical-op -Wshadow -pedantic -Werror -std=c++11
FUSEFLAGS=$(shell $(PKG_CONFIG) fuse --cflags)
ZIPFLAGS=$(shell $(PKG_CONFIG) libzip --cflags)
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


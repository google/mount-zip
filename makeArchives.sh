#!/bin/sh

dir=`mktemp -d`
pwd=`pwd`

# make program tarball
id="fuse-zip-`basename "$pwd"`"
tmp="$dir/$id"

mkdir "$tmp"
cp -t "$tmp" *.cpp *.h Makefile INSTALL LICENSE README changelog fuse-zip.1.gz

rm -f "$id.tar.gz"
cd "$dir"
tar -cvzf "$pwd/$id.tar.gz" "$id"
cd "$pwd"

# make tests tarball
id="fuse-zip-tests-r`svn info tests | grep Revision | cut -d \  -f 2`"
tmp="$dir/$id"

mkdir "$tmp"
mkdir "$tmp/kio_copy"
cp -t "$tmp" tests/README tests/run-tests.tcl tests/unpackfs.config
cp -t "$tmp/kio_copy" tests/kio_copy/kio_copy.pro tests/kio_copy/main.cpp

rm -rf "$id.tar.gz"
cd "$dir"
tar -cvzf "$pwd/$id.tar.gz" "$id"
cd "$pwd"

rm -rf "$dir"


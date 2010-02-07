#!/bin/sh

if [ $# != 0 ]
then
    echo "usage: $0"
    echo "  generate archives for product"
    exit 1
fi

version=`grep '#define VERSION' config.h | sed 's/^[^"]*"//;s/".*$//'`
if [ "$version" = "" ]
then
    echo "Unable to determine version"
    exit 1
fi

dir=`mktemp -d`
pwd=`pwd`

# make program tarball
id="fuse-zip-$version"
tmp="$dir/$id"

hg archive -X performance\* -t tgz $id.tar.gz

# make tests tarball
id="fuse-zip-tests-r`hg log performance_tests/ | head -n 1 | cut -d : -f 2 | sed 's/ //g'`"
tmp="$dir/$id"

mkdir "$tmp"
mkdir "$tmp/kio_copy"
cp -t "$tmp" performance_tests/README performance_tests/run-tests.tcl performance_tests/unpackfs.config
cp -t "$tmp/kio_copy" performance_tests/kio_copy/kio_copy.pro performance_tests/kio_copy/main.cpp

rm -rf "$id.tar.gz"
cd "$dir"
tar -cvzf "$pwd/$id.tar.gz" "$id"
cd "$pwd"

rm -rf "$dir"


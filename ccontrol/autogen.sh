#!/bin/sh
set -x
# autoconf needs build-aux directory
mkdir -p build-aux || exit 1
mkdir -p m4 || exit 1
sh version.sh
# libtoolize generates m4 dir
libtoolize || exit 1
aclocal -I m4 || exit 1
autoheader || exit 1
automake --add-missing --copy || exit 1
autoconf || exit 1

#!/bin/sh
# load kernel module
set -e -u
path=$srcdir/../src/module
cd $path
./module_load mem=1M
cd -
./random 12
cd $path
./module_unload
cd -

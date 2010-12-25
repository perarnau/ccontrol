#!/bin/sh
# load kernel module
set -e -u
path=$srcdir/../src/module
cd $path
./module_load mem=5000
cd -
./random 4000
cd $path
./module_unload
cd -

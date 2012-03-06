#!/bin/sh
# load kernel module
set -e -u
path=$srcdir/../src/utils
$path/ccontrol load -m 1M
./random 12
$path/ccontrol unload

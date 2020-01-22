#!/bin/bash

set -e

# Releaser runs this script for both the Linux build and the Mac build. If we ever need them
# to be different, we can instead provide linux-build.sh and mac-build.sh

mkdir build
cd build
cmake -D REDIS_STORE=ON -D SKIP_DATABASE_TESTS=ON ..

make

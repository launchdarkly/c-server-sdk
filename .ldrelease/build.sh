#!/bin/bash

set -e

# Releaser runs this script for both the Linux build and the Mac build. If we ever need them
# to be different, we can instead provide linux-build.sh and mac-build.sh

mkdir -p build-static && cd build-static
mkdir -p release
cmake -D SKIP_DATABASE_TESTS=ON -D CMAKE_INSTALL_PREFIX=./release ..
cmake --build .
cmake --build . --target install
cd ..

mkdir -p build-dynamic && cd build-dynamic
mkdir -p release
cmake -D BUILD_TESTING=OFF -D BUILD_SHARED_LIBS=ON -D CMAKE_INSTALL_PREFIX=./release ..
cmake --build .
cmake --build . --target install
cd ..

mkdir -p build-redis-static && cd build-redis-static
mkdir -p release
cmake -D SKIP_BASE_INSTALL=ON  -D REDIS_STORE=ON -D BUILD_TESTING=OFF -D CMAKE_INSTALL_PREFIX=./release ..
cmake --build .
cmake --build . --target install
cd ..

mkdir -p build-redis-dynamic && cd build-redis-dynamic
mkdir -p release
cmake -D SKIP_BASE_INSTALL=ON -D REDIS_STORE=ON -D BUILD_TESTING=OFF -D BUILD_SHARED_LIBS=ON -D CMAKE_INSTALL_PREFIX=./release ..
cmake --build .
cmake --build . --target install
cd ..

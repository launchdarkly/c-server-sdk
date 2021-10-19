#!/bin/bash

set -e

# Releaser runs this script for both the Linux build and the Mac build. If we ever need them
# to be different, we can instead provide linux-build.sh and mac-build.sh

# LD_LIBRARY_FILE_PREFIX is set in the environment variables for each build job in
# .ldrelease/config.yml

base=$(pwd)

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

# Copy the binary archives to the artifacts directory; Releaser will find them
# there and attach them to the release (this also makes the artifacts available
# in dry-run mode)

cd "${base}/build-static/release"
zip -r "${LD_RELEASE_ARTIFACTS_DIR}/${LD_LIBRARY_FILE_PREFIX}-static.zip" *
tar -cf "${LD_RELEASE_ARTIFACTS_DIR}/${LD_LIBRARY_FILE_PREFIX}-static.tar" *

cd "${base}/build-dynamic/release"
zip -r "${LD_RELEASE_ARTIFACTS_DIR}/${LD_LIBRARY_FILE_PREFIX}-dynamic.zip" *
tar -cf "${LD_RELEASE_ARTIFACTS_DIR}/${LD_LIBRARY_FILE_PREFIX}-dynamic.tar" *

cd "${base}/build-redis-static/release"
zip -r "${LD_RELEASE_ARTIFACTS_DIR}/${LD_LIBRARY_FILE_PREFIX}-redis-static.zip" *
tar -cf "${LD_RELEASE_ARTIFACTS_DIR}/${LD_LIBRARY_FILE_PREFIX}-redis-static.tar" *

cd "${base}/build-redis-dynamic/release"
zip -r "${LD_RELEASE_ARTIFACTS_DIR}/${LD_LIBRARY_FILE_PREFIX}-redis-dynamic.zip" *
tar -cf "${LD_RELEASE_ARTIFACTS_DIR}/${LD_LIBRARY_FILE_PREFIX}-redis-dynamic.tar" *

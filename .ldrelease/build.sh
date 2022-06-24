#!/bin/bash

set -e

# Releaser runs this script for both the Linux build and the Mac build. If we ever need them
# to be different, we can instead provide linux-build.sh and mac-build.sh

# LD_LIBRARY_FILE_PREFIX is set in the environment variables for each build job in
# .ldrelease/config.yml

# Redefine pushd/popd to not output the current directory stack; cleans up the file-manifest output below.
pushd() { builtin pushd "$1" > /dev/null; }
popd() { builtin popd > /dev/null; }

# Hash the output of 'find' on a given directory, allowing for instant comparison with previous builds.
print_directory_hash() {
  pushd "$1"
  file_names=$(find . | sort)
  hash=$(shasum -a 256 <<< "$file_names")
  echo "FILE_NAMES_HASH[$1] = #${hash:0:4}"
  # shellcheck disable=SC2016
  find . \( -name '*.so' -o -name '*.dll' -o -name '*.dylib' -o -name '*.a' -o -name '*.lib' \) -exec "$SHELL" -c '
        for file in "$@" ; do
            sum=$(shasum -a 256 < $file)
            echo "     ARTIFACT_HASH[$file] = #${sum:0:4}"
            size=$(wc -c < $file | tr -s " ")
            echo "     ARTIFACT_SIZE[$file] = $size"
        done' find-sh {} +
  echo ""
  popd
}

print_directory_contents() {
  pushd "$1"
  find . | sort
  popd
}

# Prints the files in a given directory, along with a hash of that output for easy comparison.
print_file_manifest() {
  echo ""
  echo "========== FILE MANIFEST =========="
  for dir in "$@"
  do
      echo "FILES[$dir]:"
      print_directory_contents "$dir/release"
      echo ""
  done
  echo "========== INTEGRITY =============="
  for dir in "$@"
  do
      print_directory_hash "$dir/release"
  done
  echo "========== END MANIFEST ==========="
}

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

cd "${base}"

print_file_manifest \
    "build-static" \
    "build-dynamic" \
    "build-redis-static" \
    "build-redis-dynamic"

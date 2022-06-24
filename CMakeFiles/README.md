## 3rd Party Dependency Strategy

Dependency management for the `c-server-sdk` is complicated by a few factors:
- The codebase for the SDK is written in C89 style
- Many libraries that could be of use are written in C99
- Libraries may use a variety of build systems, be hosted in a variety of places, using a variety of protocols.

Therefore, a flexible approach is necessary. 

## History

The `third-party` directory was previously host to a collection of single-purpose C libraries, with patches
applied directly to source.

Additional system-level packages were found using cmake's `find_package()`, while other libraries were pulled in via 
cmake's `FetchContent`.

As of the commit that introduced this README, the dependencies living in `third-party` have been removed 
in favor of `FetchContent`. The historical patches made directly to source have been extracted into diffs that can be applied
on top of the downloaded sources. 


## Strategy 1: find_package()

See: `FindXXX.cmake`


The `CURL` and `PCRE` packages are found on the host system via cmake Find Modules.

`FindCURL` is [distributed with cmake](https://cmake.org/cmake/help/latest/manual/cmake-modules.7.html),
whereas `FindPCRE` is distributed with this SDK.

## Strategy 2: FetchContent

See: `XXX.cmake`

The SDK utilizes a collection of focused 3rd party C libraries. Some of these need to be patched
to be useful for the SDK.

These libraries are downloaded via cmake's `FetchContent` capability, which was introduced
in cmake 3.11. 

The `FetchContent_MakeAvailable` was introduced in cmake 3.14. This command is convenient for use with
projects supporting cmake, as it exposes those projects' targets, 
significantly reducing `CMakeLists.txt` boilerplate. 

Most of the libraries used by the SDK do not support cmake, so the `XXX.cmake` files
in this repo need to setup the targets manually. If the upstream repos provided such targets, it would be 
possible to remove that boilerplate in favor of `FetchContent_MakeAvailable`.

Therefore, cmake 3.11 is sufficient for the time being.

For reference:
- Ubuntu 18.04 ships with cmake 3.10
- Ubuntu 20.04 ships with cmake 3.16

Recent cmake versions can be obtained directly from [Kitware](https://cmake.org/download/).

## Updating dependencies

To upgrade one of the dependencies here, update the `GIT_TAG` hash. 

Note: use a hash, not a tag - tags are not immutable.

Problem: cmake errors when changing the `GIT_TAG`.

Solution: remove build directory and rerun cmake. Because the `UPDATE_COMMAND` is set to `""`, cmake
won't be able to update the dependency when you modify the tag, and instead it should be fetched again. See below.

## Patching dependencies 

The libraries are patched using diffs found in the `patches` directory.

These patches were extracted from the vendored version of these libraries by comparison with the upstream
upstream repos.

If the upstream repo changes due to an update of `GIT_TAG`, the patch may need to be regenerated.

The `PATCH_COMMAND` step in `FetchContent_Declare` will fail if applied more than once to the downloaded sources
(since the patch has already been applied.)
This command was intended by cmake authors to be used for patching artifacts, rather than git repos, so
this usage is inherently fragile. 

To suppress the errors that would otherwise be caused by applying the patches multiple times
whenever cmake reruns, the `UPDATE_COMMAND` is set to `""` - meaning disabled. This in turn should disable 
the patch step from running again after its initial run.

A better solution would be to maintain dedicated forks of each repository, such that `FetchContent` can obtain
the source directly without patching.

There are various bugs/race-conditions in the interaction of `FetchContent` and `ExternalProject` (implementation of `FetchContent`)
which may cause spurious failures in CI. It remains to be seen how much of a problem this will be, and it may be solved
by upgrading the cmake version on all hosts.

## Release artifacts

The SDK is released with shared and static libraries for all supported platforms.
For example, on macOS:
- `libldserverapi.a` (static library)
- `libldserverapi.dylib` (shared library)

Care should be taken to minimize the number of artifacts that are needed to include the 
SDK in an application, for ease of use.

In CMake, that might mean:
- Using `target_link_libraries` to "link" static library dependencies into the shared library release artifact.
  - This way, only a single shared library is needed to use the SDK.
  - Alternatively, if the dependency is popular or has variants that a user may want to customize, then
  it might be beneficial to use dynamic linkage so it can be swapped out.
- Using `$<TARGET_OBJECT:targetname>` to propagate a static library dependencies into the static library release artifact.
  - Without this, it would be necessary for users to include all static library artifacts for the build to succeed.



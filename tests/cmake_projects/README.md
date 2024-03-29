## CMake project tests overview

This directory contains tests for various integration techniques that users of the
C Server SDK may employ. 

Each test takes the form of a minimal CMake project with a `CMakeLists.txt` and `main.c`.
An additional `CMakeLists.txt` sets up the test properties.

Structure:
```
some_test_directory
    project                 # Contains the CMake project under test.
        - main.c            # Minimal code that invokes LaunchDarkly SDK.
        - CMakeLists.txt    # CMake configuration that builds the project executable.
    - CMakeLists.txt        # CMake configuration that sets up the CTest machinery for this test.
```

*Important note about `main.c`*:

The optimizer employed by whatever toolchain is building the project might omit function definitions in the SDK
during static linking, if those functions are proven to be unused.

The code in `main.c` should not have any branches that allow this to happen 
(such as a check for an SDK key, like in the hello demo projects.)

This could obscure linker errors if the release artifacts are generated incorrectly. 

## CMake test setup

The toplevel `CMakeLists.txt` in each subdirectory is responsible for setting up
the actual CTest tests that configure and build the projects.

Note, the logic described below is encapsulated in two macros defined in `DeclareProjectTest.cmake`, so that
that new tests don't need to copy boilerplate. 

Test creation is generally done in two phases:
1) Make a test that configures the project (simulating `cmake .. [options]`)
2) Make a test that builds the project (simulating `cmake --build .`)

The tests are ordered via `set_tests_properties` to ensure the configure test
runs before the build test, as would be expected. 

The test creation logic harbors additional complexity because these tests are executed
in CI on multiple types of executors (Windows/Mac/Linux) in various configurations.

In particular, some environment variables must be forwarded to each test project CMake configuration.
These include `C` and `CXX` variables, which are explicitly set/overridden in the `clang11` CI build.
Without setting these, the test would fail to build with the same compilers as the SDK.

Additionally, certain variables must be forwarded to each test project CMake configuration.

| Variable                          | Explanation                                                                                                              |
|-----------------------------------|--------------------------------------------------------------------------------------------------------------------------|
| `CURL_LIBRARY`/`CURL_INCLUDE_DIR` | Windows build uses CURL downloaded at runtime, not system CURL.                                                          |
| `PCRE_LIBRARY`/`PCRE_INCLUDE_DIR` | Windows build uses PCRE downloaded at runtime, not system PCRE.                                                          |
| `CMAKE_GENERATOR_PLATFORM`        | Windows build explicitly specifies x64 build, whereas the default project build would be x86. Linker errors would ensue. |

The creation logic uses a series of CMake generator expressions (`$<...>`) to forward the variables 
in the table above from the main SDK project (which `add_subdirectory`'d each test) to the test projects. 

Simply specifying the variables directly using `-DVARIABLE=${VARIABLE}` as is normally done on the command line
wouldn't work correctly. If the variable is empty in the SDK project (like when a system package can be used), 
then passing that empty string to the test project would cause the eventual `find_package(CURL/PCRE)` to fail, as passing 
those variables implies the user wants to override the find scripts. 

The generator expressions omit the `-D` entirely if the original variable is empty, otherwise they add it.

## Tests
### cmake_projects/test_add_subdirectory
Checks that a project can include the SDK as a sub-project, via `add_subdirectory`.
This would be a likely use-case when the repo is a submodule of another project.

### cmake_projects/test_find_package
Checks that a project can include the SDK via `find_package(ldserverapi)`. 
This would be a likely use-case if the SDK was installed on the system by the user. 

**NOTE:** Requires SDK to be installed.

### cmake_projects/test_find_package_cpp
Checks that a C++ project can include the SDK via `find_package(ldserverapi)`.
Also checks that C++ bindings can be included without compilation issues.

**NOTE:** Requires SDK to be installed.

### cmake_projects/test_find_package_compatible_version
Checks that a project can include the SDK via `find_package(ldserverapi [version])`.
This would be a likely use-case if the user depends on a particular version of the SDK,
rather than accepting any version.

**NOTE:** Requires SDK to be installed.

### cmake_projects/test_find_package_incompatible_version
Checks that a project will *fail* to configure if `find_package(ldserverapi [version])`
is invoked with a version that isn't present on the system. The test uses a fictional
version `10.0.0`.

**NOTE:** Requires SDK to be installed.


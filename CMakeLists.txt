# 3.11 is required for basic FetchContent support.
cmake_minimum_required(VERSION 3.11)

# The launchdarkly/api.h header contains the SDK's version.
# See .ldrelease/update-version.sh for how it is updated during the release process.
# It is used here in order to set the project's package version.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/launchdarkly/api.h" FILE_API_H)
string(REGEX MATCH "LD_SDK_VERSION \"(.*)\"\n\n" _ ${FILE_API_H})

if(NOT DEFINED CMAKE_MATCH_1)
    message(FATAL_ERROR "LaunchDarkly - Unable to determine SDK version from <launchdarkly/api.h>")
endif()

project(ldserverapi
    VERSION ${CMAKE_MATCH_1}
    DESCRIPTION "LaunchDarkly C Server SDK"
)

message(STATUS "LaunchDarkly C Server SDK ${PROJECT_VERSION}")

# Contains various Find files, code coverage, 3rd party library FetchContent scripts,
# and the project's Package Configuration script.
set(CMAKE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Download 3rd party libraries.
include(FetchContent)
include(${CMAKE_FILES}/uthash.cmake)
include(${CMAKE_FILES}/hexify.cmake)
include(${CMAKE_FILES}/semver.cmake)
include(${CMAKE_FILES}/sha1.cmake)
include(${CMAKE_FILES}/timestamp.cmake)

# Provide user configurable options.
include(CMakeDependentOption)
option(BUILD_TESTING "Enable C++ unit tests." ON)
option(BUILD_TEST_SERVICE "Build the contract test service. Requires C++" OFF)
option(REDIS_STORE "Build optional redis store support" OFF)
option(COVERAGE "Add support for generating coverage reports" OFF)
option(SKIP_DATABASE_TESTS "Do not test external store integrations" OFF)
option(SKIP_BASE_INSTALL "Do not install the base library on install" OFF)

# Add an option for enabling the "CMake Project Tests" (see tests/cmake_projects README).
# These tests require testing to be enabled (BUILD_TESTING), but aren't unit tests, so are disabled by default.
cmake_dependent_option(ENABLE_CMAKE_PROJECT_TESTS
        "Test integration of SDK into other CMake projects" OFF
        "BUILD_TESTING" OFF
)

# Emit variables for informational/debugging purposes.
message(STATUS "LaunchDarkly - BUILD_TESTING: ${BUILD_TESTING}")
message(STATUS "LaunchDarkly - BUILD_TEST_SERVICE: ${BUILD_TEST_SERVICE}")
message(STATUS "LaunchDarkly - BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
message(STATUS "LaunchDarkly - REDIS_STORE: ${REDIS_STORE}")
message(STATUS "LaunchDarkly - COVERAGE: ${COVERAGE}")
message(STATUS "LaunchDarkly - SKIP_DATABASE_TESTS: ${SKIP_DATABASE_TESTS}")
message(STATUS "LaunchDarkly - SKIP_BASE_INSTALL: ${SKIP_BASE_INSTALL}")
message(STATUS "LaunchDarkly - ENABLE_CMAKE_PROJECT_TESTS: ${ENABLE_CMAKE_PROJECT_TESTS}")

# The SDK uses two system packages, CURL and PCRE.
# Use the CMAKE_FILES directory to find these packages first - necessary because FindPCRE is provided
# with the SDK (while FindCURL is provided by CMake itself.)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_FILES}")
find_package(CURL REQUIRED)
find_package(PCRE REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Build the unit test binary, and add all of the unit tests.
include(CTest)
if(BUILD_TESTING)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    configure_file(external/googletest/CMakeLists.txt.in
            ${CMAKE_BINARY_DIR}/googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )


    add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
            ${CMAKE_BINARY_DIR}/googletest-build EXCLUDE_FROM_ALL)

    enable_testing()

    # Adds unit tests, and if enabled, cmake project tests.
    add_subdirectory(tests)
endif()

# Enable code coverage.
if (COVERAGE)
    include(${CMAKE_FILES}/CodeCoverage.cmake)
    append_coverage_compiler_flags()
    setup_target_for_coverage_gcovr_html(NAME coverage)
endif()

set(LD_INCLUDE_PATHS
    "src"
    "c-sdk-common/include"
    "c-sdk-common/src"
    ${CURL_INCLUDE_DIR}
    ${PCRE_INCLUDE_DIR}
)

file(GLOB SOURCES "src/*" "src/integrations/*" "third-party/src/*" "c-sdk-common/src/*")

if(NOT DEFINED MSVC)
    list(REMOVE_ITEM SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/src/strptime.c"
    )
endif()

# Define the main ldserverapi target.
# The 3rd party libs are included as object archives, so that static library users can include one
# artifact, rather than the ldserverapi + these dependencies individually.
add_library(ldserverapi
    $<TARGET_OBJECTS:hexify>
    $<TARGET_OBJECTS:semver>
    $<TARGET_OBJECTS:sha1>
    $<TARGET_OBJECTS:timestamp>
    ${SOURCES}
)

if(REDIS_STORE)
    add_subdirectory(stores/redis)
endif()

# Allow linking against the target ldserverapi::ldserverapi when using find_package, or when
# using add_subdirectory.
add_library(ldserverapi::ldserverapi ALIAS ldserverapi)

target_link_libraries(ldserverapi
    PRIVATE
        uthash
        hexify
        semver
        sha1
        timestamp
        Threads::Threads
        ${CURL_LIBRARIES}
        ${PCRE_LIBRARIES}
)

if(NOT WIN32 AND NOT APPLE)
    # Required for fmod function definition, which is implemented in libm (the c math library.)
    # This is part of the c standard library on macOS, and apparently not required on Windows.
    target_link_libraries(ldserverapi PRIVATE m)
endif()

target_include_directories(ldserverapi
        PUBLIC
            # During the build phase, the SDK requires headers from the shared c-sdk-common subtree and the
            # normal include directory.
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/c-sdk-common/include>
            # Installed headers should all be lumped into a single include directory.
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${LD_INCLUDE_PATHS}
)

# Setup compiler definitions.
target_compile_definitions(ldserverapi
    PRIVATE -D LAUNCHDARKLY_CONCURRENCY_ABORT
            -D LAUNCHDARKLY_USE_ASSERT
            -D LAUNCHDARKLY_DEFENSIVE
)

if(MSVC)
    target_compile_definitions(ldserverapi
        PRIVATE -D CURL_STATICLIB
                -D PCRE_STATIC
                -D _CRT_SECURE_NO_WARNINGS
    )
else()
    target_compile_definitions(ldserverapi
        PRIVATE -D __USE_XOPEN
                -D _GNU_SOURCE
    )

    target_compile_options(ldserverapi
        PRIVATE -fno-omit-frame-pointer
                -pedantic
                -Wall
                -Wextra
                -Werror
                -Wstrict-prototypes
                -Wmissing-prototypes
                -Wmissing-declarations
                -std=c89
    )
endif(MSVC)

if(BUILD_SHARED_LIBS)
    set_property(TARGET ldserverapi PROPERTY C_VISIBILITY_PRESET hidden)
endif(BUILD_SHARED_LIBS)

if(NOT SKIP_BASE_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # All of these variables are defined relative to the build tree.
    # Generally, the scheme used here is that files created in the build tree will be copied over to the install tree.
    # This makes it simpler to support the add_subdirectory/find_package workflows in a unified manner.
    set(LDSERVERSDK_CMAKE_CONFIG_DIR  "${CMAKE_CURRENT_BINARY_DIR}")

    # Path of the (to be created) cmake version configuration.
    set(LDSERVERSDK_CMAKE_VERSION_CONFIG_FILE "${LDSERVERSDK_CMAKE_CONFIG_DIR}/${PROJECT_NAME}ConfigVersion.cmake")

    # Path of the (to be copied/renamed) cmake package configuration.
    # This file already exists in the project's cmake directory, but is copied here so the install steps are uniform.
    set(LDSERVERSDK_CMAKE_PROJECT_CONFIG_FILE "${LDSERVERSDK_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Config.cmake")

    # Path of the (to be created) cmake targets configuration.
    set(LDSERVERSDK_CMAKE_PROJECT_TARGETS_FILE "${LDSERVERSDK_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Targets.cmake")

    # Arbitrary name of the targets file, which serves to contain the ldserverapi target.
    # This file allows other projects to import the target for convenient usage.
    set(LDSERVERSDK_EXPORT_NAME "${PROJECT_NAME}-targets")

    # Generate a version config for the SDK.
    write_basic_package_version_file(${LDSERVERSDK_CMAKE_VERSION_CONFIG_FILE}
            VERSION ${PACKAGE_VERSION}
            COMPATIBILITY SameMajorVersion
    )

    # Copy over the cmake project config to the build tree. If this scheme is later changed to generate the config
    # using a template, rather than including the source directly in the repo as is currently the case,
    # the installation won't need to change.
    configure_file(${CMAKE_FILES}/${PROJECT_NAME}Config.cmake
           ${LDSERVERSDK_CMAKE_PROJECT_CONFIG_FILE}
            COPYONLY
    )

    # Instead of directly installing the library to a destination, associate it to an "export".
    # The name of the export is arbitrary; it just needs to be consistent.
    install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${LDSERVERSDK_EXPORT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    # Take the export and generate a new CMake file containing the targets contained within.
    # This lives in the build tree, and MUST NOT BE INSTALLED, since paths are relative to the build tree,
    # not the install tree.
    export(EXPORT ${LDSERVERSDK_EXPORT_NAME}
            FILE ${LDSERVERSDK_CMAKE_PROJECT_TARGETS_FILE} # <-- build tree
            NAMESPACE ldserverapi::
    )

    # Do the same as the above, but for the install tree. This file will take the name
    # <export-name>.cmake. This results in two different export files - one for the build tree (above)
    # and one for the install tree (below.)
    install(EXPORT ${LDSERVERSDK_EXPORT_NAME}
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ldserverapi
        NAMESPACE ldserverapi::
        FILE ${PROJECT_NAME}Targets.cmake
    )

    # Install SDK include directory.
    install(
        DIRECTORY              include/
        DESTINATION            ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h*"
    )

    # Install c-sdk-common include directory.
    install(
        DIRECTORY              c-sdk-common/include/
        DESTINATION            ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h*"
    )

    # Install the custom FindPCRE module, the project config, and the version config, all to the
    # install tree's cmake/ldserverapi directory.
    install(
        FILES
            ${CMAKE_FILES}/FindPCRE.cmake
            ${LDSERVERSDK_CMAKE_PROJECT_CONFIG_FILE}
            ${LDSERVERSDK_CMAKE_VERSION_CONFIG_FILE}
        DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/ldserverapi
    )
endif()

# test targets ----------------------------------------------------------------
if(BUILD_TESTING)
    file(GLOB TEST_UTILS_SRC "test-utils/src/*" "c-sdk-common/test-utils/src/*")

    add_library(test-utils STATIC ${TEST_UTILS_SRC})

    target_link_libraries(test-utils PRIVATE ldserverapi )

    target_include_directories(test-utils
        PUBLIC
               "test-utils/include"
               "c-sdk-common/test-utils/include"
                $<TARGET_PROPERTY:ldserverapi,INCLUDE_DIRECTORIES> # Some of the internal headers of ldserverapi
                # include 3rd party dependencies, which are not part of the public interface of the SDK. Therefore
                # any test code that imports these internal headers, for testing purposes, will need access to those
                # 3rd party includes. The INCLUDE_DIRECTORIES property of target ldserverapi includes both the public
                # and private headers, so propagate them here.
    )

    target_compile_definitions(test-utils
        PRIVATE -D LAUNCHDARKLY_USE_ASSERT
                -D LAUNCHDARKLY_CONCURRENCY_ABORT
    )
endif()


if (BUILD_TEST_SERVICE)
    add_subdirectory(contract-tests)
endif()

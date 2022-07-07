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

include(FetchContent)

include(CMakeFiles/uthash.cmake)
include(CMakeFiles/hexify.cmake)
include(CMakeFiles/semver.cmake)
include(CMakeFiles/sha1.cmake)
include(CMakeFiles/timestamp.cmake)


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
    add_subdirectory(tests)
endif()


option(REDIS_STORE "Build optional redis store support" OFF)
option(COVERAGE "Add support for generating coverage reports" OFF)
option(SKIP_DATABASE_TESTS "Do not test external store integrations" OFF)
option(SKIP_BASE_INSTALL "Do not install the base library on install" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeFiles")

if (COVERAGE)
    include(CMakeFiles/CodeCoverage.cmake)
    append_coverage_compiler_flags()
    setup_target_for_coverage_gcovr_html(NAME coverage)
endif (COVERAGE)


find_package(CURL REQUIRED)
find_package(PCRE REQUIRED)

if(NOT DEFINED MSVC)
    set(LD_LIBRARIES pthread m)
endif()

set(LD_INCLUDE_PATHS
    "src"
    "c-sdk-common/include"
    "c-sdk-common/src"
    ${CURL_INCLUDE_DIR}
    ${PCRE_INCLUDE_DIR}
)

if(APPLE)
    set(LD_INCLUDE_PATHS ${LD_INCLUDE_PATHS} "/usr/local/include")
endif(APPLE)

if (REDIS_STORE)
    add_subdirectory(stores/redis)
endif (REDIS_STORE)

file(GLOB SOURCES "src/*" "src/integrations/*" "third-party/src/*" "c-sdk-common/src/*")

if(NOT DEFINED MSVC)
    list(REMOVE_ITEM SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/src/strptime.c"
    )
endif()

set(LD_LIBRARIES ${LD_LIBRARIES} ${CURL_LIBRARIES} ${PCRE_LIBRARIES})

# ldserverapi target -----------------------------------------------------------

add_library(ldserverapi
    $<TARGET_OBJECTS:hexify>
    $<TARGET_OBJECTS:semver>
    $<TARGET_OBJECTS:sha1>
    $<TARGET_OBJECTS:timestamp>
    ${SOURCES}
)

target_link_libraries(ldserverapi
    PRIVATE
        uthash
        hexify
        semver
        sha1
        timestamp
        ${LD_LIBRARIES}
)

target_include_directories(ldserverapi
    PUBLIC  "include"
            "c-sdk-common/include"
    PRIVATE ${LD_INCLUDE_PATHS}
)

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
    INSTALL(
        TARGETS     ldserverapi
        DESTINATION lib
    )

    INSTALL(
        DIRECTORY              ${PROJECT_SOURCE_DIR}/include/
        DESTINATION            include
        FILES_MATCHING PATTERN "*.h*"
    )

    INSTALL(
        DIRECTORY              ${PROJECT_SOURCE_DIR}/c-sdk-common/include/
        DESTINATION            include
        FILES_MATCHING PATTERN "*.h*"
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

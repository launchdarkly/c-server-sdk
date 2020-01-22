cmake_minimum_required(VERSION 2.8.8)

option(REDIS_STORE "Build optional redis store support" OFF)
option(COVERAGE "Add support for generating coverage reports" OFF)
option(SKIP_DATABASE_TESTS "Do not test external store integrations" OFF)

enable_testing() # CTEST_OUTPUT_ON_FAILURE=1 make test

if (COVERAGE)
    include(CMakeFiles/CodeCoverage.cmake)
    append_coverage_compiler_flags()
    setup_target_for_coverage_gcovr_html(NAME coverage)
endif (COVERAGE)

if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -MD")
    add_definitions(-D CURL_STATICLIB -D PCRE_STATIC)
    set(CURL_LIBRARIES ../curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/lib/libcurl_a)
    set(CURL_INCLUDE_DIR curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/include)
    set(PCRE_LIBRARIES ../pcre-8.43/build/Debug/pcred)
    set(PCRE_INCLUDE_DIR pcre-8.43/build)
else()
    find_package(CURL REQUIRED)
    add_definitions(-D __USE_XOPEN -D _GNU_SOURCE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -fno-omit-frame-pointer -pedantic -Wall -Wextra")
    set(PCRE_LIBRARIES pcre)
    set(LD_LIBRARIES pthread m)
endif(MSVC)

set(LD_INCLUDE_PATHS "src" "include" "third-party/include" ${CURL_INCLUDE_DIR} ${PCRE_INCLUDE_DIR})

if(APPLE)
    set(LD_INCLUDE_PATHS ${LD_INCLUDE_PATHS} "/usr/local/include")
endif(APPLE)

if (REDIS_STORE)
    add_subdirectory(stores/redis)
endif (REDIS_STORE)

include_directories(${LD_INCLUDE_PATHS})
file(GLOB SOURCES "src/*" "third-party/src/*")

set(LD_LIBRARIES ${LD_LIBRARIES} ${CURL_LIBRARIES} ${PCRE_LIBRARIES})

add_library(ldserverapi STATIC ${SOURCES})
target_link_libraries(ldserverapi PUBLIC ${LD_LIBRARIES})

add_library(ldserverapidynamic SHARED ${SOURCES})
set_property(TARGET ldserverapidynamic PROPERTY C_VISIBILITY_PRESET hidden)
target_link_libraries(ldserverapidynamic PUBLIC ${LD_LIBRARIES})

file(GLOB TESTS "tests/test-*")
foreach(testsource ${TESTS})
    get_filename_component(testsourceleaf ${testsource} NAME)
    string(REPLACE ".c" "" testexe ${testsourceleaf})
    add_executable(${testexe} ${testsource})
    target_link_libraries(${testexe} ldserverapi)
    add_test(NAME ${testexe} COMMAND ${CMAKE_BINARY_DIR}/${testexe})
endforeach(testsource)

cmake_minimum_required(VERSION 2.8)

enable_testing() # CTEST_OUTPUT_ON_FAILURE=1 make test

if(MSVC)
    set(CMAKE_C_FLAGS "-D CURL_STATICLIB -D PCRE_STATIC -MD")
    set(CURL_LIBRARIES ../curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/lib/libcurl_a)
    set(CURL_INCLUDE_DIR curl-7.59.0/builds/libcurl-vc-x64-release-static-ipv6-sspi-winssl/include)
    set(PCRE_LIBRARIES ../pcre-8.43/build/Debug/pcred)
    set(PCRE_INCLUDE_DIR pcre-8.43/build)
else()
    find_package(CURL REQUIRED)
    set(CMAKE_C_FLAGS "-D__USE_XOPEN -D_GNU_SOURCE -g3 -fno-omit-frame-pointer")
    set(PCRE_LIBRARIES pcre)
    set(LD_LIBRARIES pthread m)
endif(MSVC)

include_directories("include" "third-party/include" ${CURL_INCLUDE_DIR} ${PCRE_INCLUDE_DIR})
file(GLOB SOURCES "src/*" "third-party/src/*")

set(LD_LIBRARIES ${LD_LIBRARIES} ${CURL_LIBRARIES} ${PCRE_LIBRARIES})

add_library(ldserverapi STATIC ${SOURCES})
target_link_libraries(ldserverapi ${LD_LIBRARIES})

add_library(ldserverapidynamic SHARED ${SOURCES})
target_link_libraries(ldserverapidynamic ${LD_LIBRARIES})

file(GLOB TESTS "tests/*")
foreach(testsource ${TESTS})
    get_filename_component(testsourceleaf ${testsource} NAME)
    string(REPLACE ".c" "" testexe ${testsourceleaf})
    add_executable(${testexe} ${testsource})
    target_link_libraries(${testexe} ldserverapi)
    add_test(NAME ${testexe} COMMAND ${CMAKE_BINARY_DIR}/${testexe})
endforeach(testsource)

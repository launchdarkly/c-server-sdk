cmake_minimum_required(VERSION 2.8)
find_package(CURL REQUIRED)

enable_testing() # CTEST_OUTPUT_ON_FAILURE=1 make test

set(CMAKE_C_FLAGS "-D__USE_XOPEN -D_GNU_SOURCE -Wall -Wextra -g3 -fno-omit-frame-pointer -Wno-pointer-to-int-cast -Wdeclaration-after-statement")

include_directories("include" "third-party/include" ${CURL_INCLUDE_DIR})
file(GLOB SOURCES "src/*" "third-party/src/*")

add_library(ldserverapi STATIC ${SOURCES})
target_link_libraries(ldserverapi ${CURL_LIBRARIES} pthread pcre m)

add_library(ldserverapidynamic SHARED ${SOURCES})
target_link_libraries(ldserverapidynamic ${CURL_LIBRARIES} pthread pcre m)

file(GLOB TESTS "tests/*")
foreach(testsource ${TESTS})
    get_filename_component(testsourceleaf ${testsource} NAME)
    string(REPLACE ".c" "" testexe ${testsourceleaf})
    add_executable(${testexe} ${testsource})
    target_link_libraries(${testexe} ldserverapi)
    add_test(NAME ${testexe} COMMAND ${CMAKE_BINARY_DIR}/${testexe})
endforeach(testsource)

cmake_minimum_required(VERSION 2.8)
find_package(CURL REQUIRED)

enable_testing() # CTEST_OUTPUT_ON_FAILURE=1 make test

set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -g3 -fno-omit-frame-pointer -Wno-pointer-to-int-cast -no-pie -Wdeclaration-after-statement")

include_directories("include" "third-party/include" ${CURL_INCLUDE_DIR})
file(GLOB SOURCES "src/*" "third-party/src/*")

add_library(sdk ${SOURCES})
target_link_libraries(sdk ${CURL_LIBRARIES} pthread)

add_executable(main "src/main.c")
target_link_libraries(main sdk)

file(GLOB TESTS "tests/*")
foreach(testsource ${TESTS})
    get_filename_component(testsourceleaf ${testsource} NAME)
    string(REPLACE ".c" "" testexe ${testsourceleaf})
    add_executable(${testexe} ${testsource})
    target_link_libraries(${testexe} sdk)
    add_test(NAME ${testexe} COMMAND ${CMAKE_BINARY_DIR}/${testexe})
endforeach(testsource)

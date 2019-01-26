cmake_minimum_required(VERSION 2.8)
find_package(CURL REQUIRED)
enable_testing()

set(CMAKE_C_FLAGS "-std=c89 -pedantic -Wall -Wextra -Werror -g3 -fsanitize=address -fno-omit-frame-pointer")

include_directories("include" "third-party/include" ${CURL_INCLUDE_DIR})
file(GLOB SOURCES "src/*" "third-party/src/*")
add_library(sdk ${SOURCES})

add_executable(main "src/main.c")
target_link_libraries(main ${CURL_LIBRARIES} sdk)

file(GLOB TESTS "tests/*")
foreach(testsource ${TESTS})
    get_filename_component(testsourceleaf ${testsource} NAME)
    string(REPLACE ".c" "" testexe ${testsourceleaf})
    add_executable(${testexe} ${testsource})
    target_link_libraries(${testexe} ${CURL_LIBRARIES} sdk)
    add_test(NAME ${testexe} COMMAND ${CMAKE_BINARY_DIR}/${testexe})
endforeach(testsource)

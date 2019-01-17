cmake_minimum_required(VERSION 2.8)

find_package(CURL REQUIRED)

set(CMAKE_C_FLAGS "-std=c89 -pedantic -Wall -Wextra -Werror -g3 -fsanitize=address -fno-omit-frame-pointer")

include_directories("include" "third-party/include" ${CURL_INCLUDE_DIR})

file(GLOB SOURCES "src/*" "third-party/src/*")

add_executable(main ${SOURCES})

target_link_libraries(main ${CURL_LIBRARIES})

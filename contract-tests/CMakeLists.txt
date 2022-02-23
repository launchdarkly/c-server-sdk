cmake_minimum_required(VERSION 3.10)

project(ContractTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include httplib, which is fetched from a remote git repository.
configure_file(lib/external/httplib/CMakeLists.txt.in
        ${CMAKE_BINARY_DIR}/httplib-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/httplib-download )
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/httplib-download )
add_subdirectory(${CMAKE_BINARY_DIR}/httplib-src
        ${CMAKE_BINARY_DIR}/httplib-build EXCLUDE_FROM_ALL)

add_subdirectory(lib/external/json)

# Include internal libraries. Exposes 'service' target.
add_subdirectory(lib/internal)

add_executable(testservice main.cpp)

target_link_libraries(testservice PRIVATE service ldserverapi)


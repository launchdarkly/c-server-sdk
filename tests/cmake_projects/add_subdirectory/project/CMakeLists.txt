cmake_minimum_required(VERSION 3.11)

project(AddSubdirectoryTest)

add_subdirectory(
    # Source directory where the SDK's CMakeLists.txt is located.
    ${LDSERVERAPI_SOURCE_DIR}
    # Binary directory must be specified when using an out-of-tree source.
    ${CMAKE_CURRENT_BINARY_DIR}/ldserverapi
)

add_executable(add_subdirectory_app main.c)
target_link_libraries(add_subdirectory_app ldserverapi)

add_executable(add_subdirectory_app_alias main.c)
target_link_libraries(add_subdirectory_app_alias ldserverapi::ldserverapi)



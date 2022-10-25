cmake_minimum_required(VERSION 3.11)

project(UseImportTest)

find_package(ldserverapi REQUIRED)

add_executable(use_import_app main.c)
target_link_libraries(use_import_app ldserverapi::ldserverapi)

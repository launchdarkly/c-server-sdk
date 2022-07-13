cmake_minimum_required(VERSION 3.11)

project(UseImportTest)

find_package(ldserverapi 2.7.0 REQUIRED )

add_executable(package_version_app main.c)
target_link_libraries(package_version_app ldserverapi::ldserverapi)

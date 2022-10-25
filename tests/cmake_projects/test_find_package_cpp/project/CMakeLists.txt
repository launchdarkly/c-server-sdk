cmake_minimum_required(VERSION 3.11)

project(UseImportFromCPPTest)

find_package(ldserverapi REQUIRED)

add_executable(use_import_cpp_app main.cpp)
target_link_libraries(use_import_cpp_app ldserverapi::ldserverapi)
target_compile_features(use_import_cpp_app PRIVATE cxx_std_17)

get_filename_component(ldserverapi_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(CMakeFindDependencyMacro)

find_dependency(Threads REQUIRED)
find_dependency(CURL REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${ldserverapi_CMAKE_DIR})
find_dependency(PCRE REQUIRED)
list(REMOVE_AT CMAKE_MODULE_PATH -1)

include("${CMAKE_CURRENT_LIST_DIR}/ldserverapiTargets.cmake")

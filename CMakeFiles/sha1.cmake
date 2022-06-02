FetchContent_Declare(sha1
    GIT_REPOSITORY https://github.com/clibs/sha1
    GIT_TAG "fa1d96ec293d2968791603548125e3274bd6b472"
    CONFIGURE_COMMAND ""
)

FetchContent_GetProperties(sha1)
if(NOT sha1_POPULATED)
    FetchContent_Populate(sha1)
endif()

add_library(sha1 STATIC ${sha1_SOURCE_DIR}/sha1.c)
target_include_directories(sha1 PUBLIC ${sha1_SOURCE_DIR})

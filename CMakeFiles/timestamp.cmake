FetchContent_Declare(timestamp
    GIT_REPOSITORY https://github.com/chansen/c-timestamp
    GIT_TAG "b205c407ae6680d23d74359ac00444b80989792f"
    CONFIGURE_COMMAND ""
    UPDATE_COMMAND "" # patching will fail if applied more than once; disable updates.
    PATCH_COMMAND git apply "${PROJECT_SOURCE_DIR}/patches/timestamp.patch"
)

FetchContent_GetProperties(timestamp)
if(NOT timestamp_POPULATED)
    FetchContent_Populate(timestamp)
endif()

add_library(timestamp STATIC
    ${timestamp_SOURCE_DIR}/timestamp_compare.c
    ${timestamp_SOURCE_DIR}/timestamp_parse.c
)
target_include_directories(timestamp PUBLIC ${timestamp_SOURCE_DIR})



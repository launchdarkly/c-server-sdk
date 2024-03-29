FetchContent_Declare(hexify
    GIT_REPOSITORY https://github.com/pepaslabs/hexify.c
    GIT_TAG "f823bd619f73584a75829cc1e44a532f5e09336e"
    CONFIGURE_COMMAND ""
    UPDATE_COMMAND "" # patching will fail if applied more than once; disable updates.
    PATCH_COMMAND git apply "${PROJECT_SOURCE_DIR}/patches/hexify.patch"
)

FetchContent_GetProperties(hexify)
if(NOT hexify_POPULATED)
    FetchContent_Populate(hexify)
endif()

add_library(hexify OBJECT ${hexify_SOURCE_DIR}/hexify.c)

if(BUILD_SHARED_LIBS)
    set_target_properties(hexify PROPERTIES
        POSITION_INDEPENDENT_CODE 1
        C_VISIBILITY_PRESET hidden
    )
endif()

target_include_directories(hexify PUBLIC
        $<BUILD_INTERFACE:${hexify_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/hexify>
)
install(
        TARGETS hexify
        EXPORT ${PROJECT_NAME}-targets
)

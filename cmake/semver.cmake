FetchContent_Declare(semver
    GIT_REPOSITORY https://github.com/h2non/semver.c
    GIT_TAG "bd1db234a68f305ed10268bd023df1ad672061d7"
    CONFIGURE_COMMAND ""
    UPDATE_COMMAND "" # patching will fail if applied more than once; disable updates.
    PATCH_COMMAND git apply "${PROJECT_SOURCE_DIR}/patches/semver.patch"
)

FetchContent_GetProperties(semver)
if(NOT semver_POPULATED)
    FetchContent_Populate(semver)
endif()

add_library(semver OBJECT ${semver_SOURCE_DIR}/semver.c)

if(BUILD_SHARED_LIBS)
    set_property(TARGET semver PROPERTY POSITION_INDEPENDENT_CODE 1)
endif()

target_include_directories(semver PUBLIC
        $<BUILD_INTERFACE:${semver_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/semver>
)
install(
        TARGETS semver
        EXPORT ${PROJECT_NAME}-targets
)

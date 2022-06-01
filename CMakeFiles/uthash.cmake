FetchContent_Declare(uthash
    URL https://github.com/troydhanson/uthash/archive/v2.3.0.tar.gz
    URL_HASH SHA256=e10382ab75518bad8319eb922ad04f907cb20cccb451a3aa980c9d005e661acc
    CONFIGURE_COMMAND ""
)

FetchContent_GetProperties(uthash)
if(NOT uthash_POPULATED)
    FetchContent_Populate(uthash)
endif()

add_library(uthash INTERFACE)
target_include_directories(uthash INTERFACE ${uthash_SOURCE_DIR}/src)



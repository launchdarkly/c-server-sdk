cmake_minimum_required(VERSION 2.8.8)

project(ldserverapi)

option(REDIS_STORE "Build optional redis store support" OFF)
option(COVERAGE "Add support for generating coverage reports" OFF)
option(SKIP_DATABASE_TESTS "Do not test external store integrations" OFF)
option(SKIP_BASE_INSTALL "Do not install the base library on install" OFF)

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeFiles")

if (COVERAGE)
    include(CMakeFiles/CodeCoverage.cmake)
    append_coverage_compiler_flags()
    setup_target_for_coverage_gcovr_html(NAME coverage)
endif (COVERAGE)

find_package(CURL REQUIRED)
find_package(PCRE REQUIRED)

if(NOT DEFINED MSVC)
    set(LD_LIBRARIES pthread m)
endif()

set(LD_INCLUDE_PATHS
    "src"
    "third-party/include"
    "c-sdk-common/include"
    "c-sdk-common/src"
    ${CURL_INCLUDE_DIR}
    ${PCRE_INCLUDE_DIR}
)

if(APPLE)
    set(LD_INCLUDE_PATHS ${LD_INCLUDE_PATHS} "/usr/local/include")
endif(APPLE)

if (REDIS_STORE)
    add_subdirectory(stores/redis)
endif (REDIS_STORE)

file(GLOB SOURCES "src/*" "third-party/src/*" "c-sdk-common/src/*")

if(NOT DEFINED MSVC)
    list(REMOVE_ITEM SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/third-party/src/strptime.c"
    )
endif()

set(LD_LIBRARIES ${LD_LIBRARIES} ${CURL_LIBRARIES} ${PCRE_LIBRARIES})

# ldserverapi target -----------------------------------------------------------

add_library(ldserverapi ${SOURCES})

target_link_libraries(ldserverapi PRIVATE ${LD_LIBRARIES})

target_include_directories(ldserverapi
    PUBLIC "include"
    PRIVATE ${LD_INCLUDE_PATHS}
)

target_compile_definitions(ldserverapi
    PRIVATE -D LAUNCHDARKLY_CONCURRENCY_ABORT
            -D LAUNCHDARKLY_USE_ASSERT
            -D LAUNCHDARKLY_DEFENSIVE
)

if(MSVC)
    target_compile_definitions(ldserverapi
        PRIVATE -D CURL_STATICLIB
                -D PCRE_STATIC
                -D _CRT_SECURE_NO_WARNINGS
    )
else()
    target_compile_definitions(ldserverapi
        PRIVATE -D __USE_XOPEN
                -D _GNU_SOURCE
    )

    target_compile_options(ldserverapi
        PRIVATE -fno-omit-frame-pointer
                -pedantic
                -Wall
                -Wextra
                -Wstrict-prototypes
                -Wmissing-prototypes
                -Wmissing-declarations
    )
endif(MSVC)

if(BUILD_SHARED_LIBS)
    set_property(TARGET ldserverapi PROPERTY C_VISIBILITY_PRESET hidden)
endif(BUILD_SHARED_LIBS)

if(NOT SKIP_BASE_INSTALL)
    INSTALL(
        TARGETS     ldserverapi
        DESTINATION lib
    )

    INSTALL(
        DIRECTORY              ${PROJECT_SOURCE_DIR}/include/
        DESTINATION            include
        FILES_MATCHING PATTERN "*.h*"
    )

    INSTALL(
        DIRECTORY              ${PROJECT_SOURCE_DIR}/c-sdk-common/include/
        DESTINATION            include
        FILES_MATCHING PATTERN "*.h*"
    )
endif()

# test targets ----------------------------------------------------------------

if(BUILD_TESTING)
    file(GLOB TEST_UTILS_SRC "test-utils/src/*" "c-sdk-common/test-utils/src/*")

    add_library(test-utils STATIC ${TEST_UTILS_SRC})

    target_link_libraries(test-utils ldserverapi)

    target_include_directories(test-utils
        PUBLIC ${LD_INCLUDE_PATHS}
               "test-utils/include"
               "c-sdk-common/test-utils/include"
    )

    target_compile_definitions(test-utils
        PRIVATE -D LAUNCHDARKLY_USE_ASSERT
                -D LAUNCHDARKLY_CONCURRENCY_ABORT
    )

    file(GLOB TESTS "tests/test-*")
    foreach(testsource ${TESTS})
        get_filename_component(testsourceleaf ${testsource} NAME)
        string(REPLACE ".c" "" testexe ${testsourceleaf})
        add_executable(${testexe} ${testsource})
        target_link_libraries(${testexe} ldserverapi test-utils)
        target_include_directories(${testexe} PRIVATE ${LD_INCLUDE_PATHS})
        add_test(NAME ${testexe} COMMAND ${CMAKE_BINARY_DIR}/${testexe})
        target_compile_definitions(${testexe}
            PRIVATE -D LAUNCHDARKLY_USE_ASSERT
                    -D LAUNCHDARKLY_CONCURRENCY_ABORT
        )
    endforeach(testsource)
endif()

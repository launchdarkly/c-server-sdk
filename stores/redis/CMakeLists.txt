cmake_minimum_required(VERSION 3.10)

project(ldserverapi-redis)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeFiles")

# ldserverapi-redis targets ----------------------------------------------------

find_package(hiredis REQUIRED)
include(CTest)

add_library(ldserverapi-redis
    "src/redis.c"
    "../../c-sdk-common/src/concurrency.c"
    "../../c-sdk-common/src/utility.c"
    "../../src/store.c"
)

target_link_libraries(ldserverapi-redis
    PRIVATE
        ldserverapi
        uthash
        ${HIREDIS_LIBRARIES}
)

target_include_directories(ldserverapi-redis
    PUBLIC  "include"
    PRIVATE "../../include"
            "../../src"
            "../../tests"
            "../../c-sdk-common/include"
            "../../c-sdk-common/src"
            ${HIREDIS_INCLUDE_DIRS}
)

INSTALL(
    TARGETS     ldserverapi-redis
    DESTINATION lib
)

INSTALL(
    DIRECTORY              ${PROJECT_SOURCE_DIR}/include/
    DESTINATION            include
    FILES_MATCHING PATTERN "*.h*"
)

# test targets ----------------------------------------------------------------

if(BUILD_TESTING)
    if (NOT SKIP_DATABASE_TESTS)
        include(GoogleTest)

        add_definitions(-DTEST_REDIS)
        add_executable(test-store-redis ../../tests/test-stores.cpp ../../tests/commonfixture.cpp)

        target_link_libraries(test-store-redis
            ldserverapi
            ldserverapi-redis
            test-utils
        )

        target_link_libraries("test-store-redis" gtest_main)

        gtest_discover_tests(test-store-redis)
        target_compile_definitions(test-store-redis
            PRIVATE -D LAUNCHDARKLY_USE_ASSERT
        )

        target_include_directories(test-store-redis
            PRIVATE "include"
                    "../../include"
                    "../../src"
                    "../../tests"
                    "../../c-sdk-common/include"
                    "../../tests"
        )
    endif (NOT SKIP_DATABASE_TESTS)
endif()

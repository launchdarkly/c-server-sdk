cmake_minimum_required(VERSION 2.8.8)

project(ldserverapi-redis)

# ldserverapi-redis targets ----------------------------------------------------

add_library(ldserverapi-redis src/redis.c)

target_link_libraries(ldserverapi-redis PRIVATE hiredis)

target_include_directories(ldserverapi-redis
    PUBLIC  "include"
    PRIVATE "../../include"
            "../../src"
            "../../tests"
            "../../third-party/include"
)

# test targets ----------------------------------------------------------------

if (NOT SKIP_DATABASE_TESTS)
    add_executable(test-store-redis test-redis.c)

    target_link_libraries(test-store-redis ldserverapi ldserverapi-redis)

    add_test(test-store-redis test-store-redis)

    target_compile_definitions(test-store-redis
        PRIVATE -D LAUNCHDARKLY_USE_ASSERT
    )

    target_include_directories(test-store-redis
        PRIVATE "include"
                "../../include"
                "../../src"
                "../../tests"
                "../../third-party/include"
    )
endif (NOT SKIP_DATABASE_TESTS)

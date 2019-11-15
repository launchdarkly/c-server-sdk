cmake_minimum_required(VERSION 2.8.8)

project(ldserverapi-redis)

include_directories("include" "../../include" "../../src" "../../tests" "../../third-party/include")

add_library(ldserverapi-redis STATIC src/redis.c)

target_link_libraries(ldserverapi-redis PUBLIC hiredis)

add_executable(test-store-redis test-redis.c)
target_link_libraries(test-store-redis ldserverapi ldserverapi-redis)
add_test(test-store-redis test-store-redis)

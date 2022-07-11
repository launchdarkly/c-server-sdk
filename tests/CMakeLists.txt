cmake_minimum_required(VERSION 3.10)
include(GoogleTest)

include_directories("${PROJECT_SOURCE_DIR}/include")

file(GLOB tests "${PROJECT_SOURCE_DIR}/tests/*.cpp")
list(REMOVE_ITEM tests "${PROJECT_SOURCE_DIR}/tests/test-main.cpp")
list(REMOVE_ITEM tests "${PROJECT_SOURCE_DIR}/tests/commonfixture.cpp")
file(COPY "${PROJECT_SOURCE_DIR}/tests/datafiles" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable("google_tests"
        ${sources}
        ${tests}
        "${PROJECT_SOURCE_DIR}/tests/commonfixture.cpp"
        "${PROJECT_SOURCE_DIR}/tests/test-main.cpp")
target_link_libraries(google_tests ldserverapi test-utils)
target_include_directories(google_tests PRIVATE ${LD_INCLUDE_PATHS})
target_link_libraries("google_tests" gtest_main)


gtest_discover_tests(google_tests)


target_compile_definitions(google_tests
        PRIVATE
        -D LAUNCHDARKLY_USE_ASSERT
        -D LAUNCHDARKLY_CONCURRENCY_ABORT
)

# These tests check that the SDK can be used in other cmake projects,
# via add_subdirectory or find_package.
if (ENABLE_CMAKE_PROJECT_TESTS)
    add_subdirectory(cmake_projects)
endif()

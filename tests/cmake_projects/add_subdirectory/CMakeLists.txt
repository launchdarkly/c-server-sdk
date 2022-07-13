set(test_prefix add_subdirectory)

add_test(
    NAME ${test_prefix}_configure
    COMMAND
        ${CMAKE_COMMAND}
            # Since project/CMakeLists.txt is going to call add_subdirectory(), it needs to know where
            # the SDK's project is (which is actually a couple directories above this particular file; not normally the case.)
            # The variable name is arbitrary.
            -DLDSERVERAPI_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            # Do not setup all of the SDK's testing machinery, which would normally happen when calling add_subdirectory.
            -DBUILD_TESTING=OFF
            # Forward variables from the SDK project to the test project, if set.
            $<$<BOOL:${CMAKE_GENERATOR_PLATFORM}>:-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}>
            $<$<BOOL:${CURL_LIBRARY}>:-DCURL_LIBRARY=${CURL_LIBRARY}>
            $<$<BOOL:${CURL_INCLUDE_DIR}>:-DCURL_INCLUDE_DIR=${CURL_INCLUDE_DIR}>
            $<$<BOOL:${PCRE_LIBRARY}>:-DPCRE_LIBRARY=${PCRE_LIBRARY}>
            $<$<BOOL:${PCRE_INCLUDE_DIR}>:-DPCRE_INCLUDE_DIR=${PCRE_INCLUDE_DIR}>
        ${CMAKE_CURRENT_SOURCE_DIR}/project
)

# Setup a 'test' to perform the cmake build step.
add_test(
    NAME ${test_prefix}_build
    COMMAND ${CMAKE_COMMAND} --build .
)

set_tests_properties(${test_prefix}_configure
    PROPERTIES
        FIXTURES_SETUP ${test_prefix}
        # Forward along the CC and CXX environment variables, because clang11 CI build uses them.
        ENVIRONMENT "CC=${CMAKE_C_COMPILER};CXX=${CMAKE_CXX_COMPILER}"
)

set_tests_properties(${test_prefix}_build
    PROPERTIES
        FIXTURES_REQUIRED ${test_prefix}
)

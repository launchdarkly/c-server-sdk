# This test assumes that the SDK has been installed at CMAKE_INSTALL_PREFIX.
set(test_prefix find_package_compatible_version)

add_test(
    NAME ${test_prefix}_configure
    COMMAND
        ${CMAKE_COMMAND}
            # Since project/CMakeLists.txt uses find_package(), it needs to know where to find
            # ldserverapiConfig.cmake. That can be found where the SDK is installed, which is CMAKE_INSTALL_PREFIX.
            -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
            # Forward variables from the SDK project to the test project, if set.
            $<$<BOOL:${CMAKE_GENERATOR_PLATFORM}>:-DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}>
            $<$<BOOL:${CURL_LIBRARY}>:-DCURL_LIBRARY=${CURL_LIBRARY}>
            $<$<BOOL:${CURL_INCLUDE_DIR}>:-DCURL_INCLUDE_DIR=${CURL_INCLUDE_DIR}>
            $<$<BOOL:${PCRE_LIBRARY}>:-DPCRE_LIBRARY=${PCRE_LIBRARY}>
            $<$<BOOL:${PCRE_INCLUDE_DIR}>:-DPCRE_INCLUDE_DIR=${PCRE_INCLUDE_DIR}>
        ${CMAKE_CURRENT_SOURCE_DIR}/project
)

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

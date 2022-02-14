add_library(http-helpers
    src/http_helpers.cpp
    include/helpers/http_helpers.hpp
)
target_link_libraries(http-helpers PUBLIC httplib)
target_include_directories(http-helpers PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")


add_library(definitions
    src/definitions.cpp
    include/definitions/definitions.hpp
)
target_link_libraries(definitions PUBLIC json)
target_include_directories(definitions PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(service
    src/service.cpp
    src/client_entity.cpp
    include/service/service.hpp
    include/service/client_entity.hpp
)
target_link_libraries(service
    PUBLIC
        httplib
    PRIVATE
        http-helpers
        definitions
        json
        ldserverapi
)
target_include_directories(service PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

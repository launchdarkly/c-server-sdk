# Note: nlohmann-json is embedded directly in source, rather than downloaded like httplib, because
# the git repo is quite large.
#
# This can be mitigated by using FetchContent on a release, or by downloading from another repo that tracks
# nlohmann-json but is smaller in size, as mentioned in the nlohmann-json README.
#
# Since neither of these are ideal (especially since FetchContent isn't supported until cmake 3.11),
# it is embedded directly and therefore must be updated manually.

add_library(json INTERFACE)
target_include_directories(json INTERFACE "include")

# LaunchDarkly Server-Side SDK for C/C++

[![CircleCI](https://circleci.com/gh/launchdarkly/c-server-sdk.svg?style=svg)](https://circleci.com/gh/launchdarkly/c-server-sdk)

The LaunchDarkly Server-Side SDK for C/C++ is designed primarily for use in multi-user systems such as web servers and applications. It follows the server-side LaunchDarkly model for multi-user contexts. It is not intended for use in desktop and embedded systems applications.

For using LaunchDarkly in _client-side_ C/C++ applications, refer to our [client-side C/C++ SDK](https://github.com/launchdarkly/c-client-sdk).

## LaunchDarkly overview

[LaunchDarkly](https://www.launchdarkly.com) is a feature management platform that serves over 100 billion feature flags daily to help teams build better software, faster. [Get started](https://docs.launchdarkly.com/home/getting-started) using LaunchDarkly today!

[![Twitter Follow](https://img.shields.io/twitter/follow/launchdarkly.svg?style=social&label=Follow&maxAge=2592000)](https://twitter.com/intent/follow?screen_name=launchdarkly)

## Compatibility

This version of the LaunchDarkly SDK is compatible with POSIX environments (Linux, OS X, BSD) and Windows.

## Getting started

Download a release archive from the [GitHub Releases](https://github.com/launchdarkly/c-server-sdk/releases) for use in your project. Refer to the [SDK documentation](https://docs.launchdarkly.com/sdk/server-side/c-c--#getting-started) for complete instructions on getting started with using the SDK.

## CMake Compatibility

| Component      | Minimum CMake version required |
|----------------|--------------------------------|
| SDK            | 3.11                           |
| Contract Tests | 3.14                           |


## CMake Usage

CMake is a highly flexible build-system generator. The SDK strives to support
two common use-cases for integrating a CMake-based project into a main application.

### Adding the SDK as a sub-project with add_subdirectory()

First, obtain the SDK's source code and place the directory somewhere within your project.
For example:
```bash
cd your-project
git clone https://github.com/launchdarkly/c-server-sdk.git
```

Add the source directory within your project's `CMakeLists.txt`:
```bash
add_subdirectory(c-server-sdk)
```
The SDK's `ldserverapi::ldserverapi` target should be available for linking.
For example:
```cmake
target_link_libraries(yourproject ldserverapi::ldserverapi)
```

### Installing & finding the SDK with find_package() 
Instead of including the SDK's project directly in another project, the SDK can instead be
installed to a location on the build machine.

The installed files can be referenced by another CMake project.
For example:
```bash
git clone https://github.com/launchdarkly/c-server-sdk.git
cd c-server-sdk

# If no install prefix is provided, standard GNU install directories are used.
cmake .. -DCMAKE_INSTALL_PREFIX=/path/where/sdk/should/be/installed
cmake --build . --target install
```
In your project's `CMakeLists.txt`:
```cmake
# Replace with latest version
find_package(ldserverapi 2.7.1 REQUIRED)

target_link_libraries(yourproject ldserverapi::ldserverapi)
```
## Dependencies 
| Component      | Package Name        | Location   | Method       | Minimum Version | Patched                      |
|----------------|---------------------|------------|--------------|-----------------|------------------------------|
| SDK            | PCRE                | Build host | FindPCRE     |                 | N                            |
| SDK            | CURL                | Build host | FindCURL     |                 | N                            |
| SDK            | pepaslabs/hexify    | Github     | FetchContent | Pegged @ f823b  | [Y](patches/hexify.patch)    |
| SDK            | h2non/semver        | Github     | FetchContent | Pegged @ bd1db  | [Y](patches/semver.patch)    |
| SDK            | clibs/sha1          | Github     | FetchContent | Pegged @ fa1d9  | N                            |
| SDK            | chansen/timestamp   | Github     | FetchContent | Pegged @ b205c  | [Y](patches/timestamp.patch) |
| SDK            | troydhanson/uthash  | Github     | FetchContent | v2.3.0          | N                            |
| SDK            | NetBSD strptime.c   | Vendored   | N/A          | N/A             | Unknown                      |
| SDK with Redis | hiredis             | Build host | FindHiredis  |                 | N                            |
| Contract Tests | yhirose/cpp-httplib | Github     | FetchContent | v0.10.2         | N                            |
| Contract Tests | nlohmann/json       | Github     | FetchContent | v3.10.5         | N                            |


## Unit tests

The project **uses C++** for unit testing. 
To exclude the unit tests from the SDK build, disable via CMake option:
```
cmake .. -DBUILD_TESTING=OFF <other options...>
```

## Redis store integration
To enable the Redis store integration, the `hiredis` package must be present on the build host.
Enable support via cmake option:
```
cmake .. -DREDIS_STORE=ON <other options...>
```
The SDK library artifacts will now contain Redis support, and an additional
[header](stores/redis/include/launchdarkly/store/redis.h) will be made available
in the build/install trees.

## Learn more

Read our [documentation](https://docs.launchdarkly.com) for in-depth instructions on configuring and using LaunchDarkly. You can also head straight to the [complete reference guide for this SDK](https://docs.launchdarkly.com/docs/c-server-sdk-reference).

## Testing

We run integration tests for all our SDKs using a centralized test harness. This approach gives us the ability to test for consistency across SDKs, as well as test networking behavior in a long-running application. These tests cover each method in the SDK, and verify that event sending, flag evaluation, stream reconnection, and other aspects of the SDK all behave correctly.

## Contributing

We encourage pull requests and other contributions from the community. Check out our [contributing guidelines](CONTRIBUTING.md) for instructions on how to contribute to this SDK.

## About LaunchDarkly

* LaunchDarkly is a continuous delivery platform that provides feature flags as a service and allows developers to iterate quickly and safely. We allow you to easily flag your features and manage them from the LaunchDarkly dashboard.  With LaunchDarkly, you can:
    * Roll out a new feature to a subset of your users (like a group of users who opt-in to a beta tester group), gathering feedback and bug reports from real-world use cases.
    * Gradually roll out a feature to an increasing percentage of users, and track the effect that the feature has on key metrics (for instance, how likely is a user to complete a purchase if they have feature A versus feature B?).
    * Turn off a feature that you realize is causing performance problems in production, without needing to re-deploy, or even restart the application with a changed configuration file.
    * Grant access to certain features based on user attributes, like payment plan (eg: users on the ‘gold’ plan get access to more features than users in the ‘silver’ plan). Disable parts of your application to facilitate maintenance, without taking everything offline.
* LaunchDarkly provides feature flag SDKs for a wide variety of languages and technologies. Read [our documentation](https://docs.launchdarkly.com/sdk) for a complete list.
* Explore LaunchDarkly
    * [launchdarkly.com](https://www.launchdarkly.com/ "LaunchDarkly Main Website") for more information
    * [docs.launchdarkly.com](https://docs.launchdarkly.com/  "LaunchDarkly Documentation") for our documentation and SDK reference guides
    * [apidocs.launchdarkly.com](https://apidocs.launchdarkly.com/  "LaunchDarkly API Documentation") for our API documentation
    * [blog.launchdarkly.com](https://blog.launchdarkly.com/  "LaunchDarkly Blog Documentation") for the latest product updates

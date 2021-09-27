# Contributing to the LaunchDarkly Server-Side SDK for C/C++

LaunchDarkly has published an [SDK contributor's guide](https://docs.launchdarkly.com/sdk/concepts/contributors-guide) that provides a detailed explanation of how our SDKs work. See below for additional information on how to contribute to this SDK.

## Submitting bug reports and feature requests

The LaunchDarkly SDK team monitors the [issue tracker](https://github.com/launchdarkly/c-server-sdk/issues) in the SDK repository. Bug reports and feature requests specific to this SDK should be filed in this issue tracker. The SDK team will respond to all newly filed issues within two business days.

## Submitting pull requests

We encourage pull requests and other contributions from the community. Before submitting pull requests, ensure that all temporary or unintended code is removed. Don't worry about adding reviewers to the pull request; the LaunchDarkly SDK team will add themselves. The SDK team will acknowledge all pull requests within two business days.

## Build instructions

### Prerequisites (POSIX)

This SDK is built with [CMake](https://cmake.org/). The specific commands run and tools utilized by CMake depend on the platform in use; refer to the SDK's [CI configuration](.circleci/config.yml) to learn more about the commands and prerequisite libraries such as `libcurl` and `pcre`.

### Prerequisites (Windows)

Building the SDK requires that the Visual Studio C compiler be installed. The SDK also requires `libcurl` and `libpcre`.

You can obtain the `libcurl` dependency at [curl.haxx.se](https://curl.haxx.se/download/curl-7.59.0.zip). Extract this archive into the SDK source directory. To build `libcurl` run:

```bash
cd curl-7.59.0/winbuild
nmake /f Makefile.vc mode=static
```

You can obtain the `libpcre` dependency at [ftp.pcre.org](https://ftp.pcre.org/pub/pcre/pcre-8.43.zip). Extract this archive into the SDK source directory. To build `libpcre` run:

```bash
cd pcre-8.43
mkdir build
cd build
cmake -G "Visual Studio 15 2017 Win64" ..
cmake --build .
```

You may need to modify the CMAKE generator (`-G` flag) for your specific environment. For Visual Studio 2019, the equivalent is `cmake -G "Visual Studio 16 2019" -A x64`.

The Visual Studio command prompt can be configured for multiple environments. To ensure that you are using your intended tool chain you can launch an environment with: `call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\Tools\VsDevCmd.bat" -host_arch=amd64 -arch=amd64`, where `arch` is your target, `host_arch` is the platform you are building on, and the path is your path to `VsDevCmd.bat`. You will need to modify the above command for your specific setup.

### Building

After fulfilling the platform-specific prerequisites for your environment, build the SDK by entering the SDK source directory and running:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

To build with Redis support use `cmake -D REDIS_STORE="true" ..` instead.

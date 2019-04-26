LaunchDarkly SDK for C / C++ (server-side)
===================================

*Warning:* This software is *beta* software and should not be used in a production environment until the general availability release.

The LaunchDarkly C / C++ (server-side) SDK is designed primarily for use in multi-user systems such as web servers and applications. It follows the server-side LaunchDarkly model for multi-user contexts. It is not intended for use in desktop and embedded systems applications.

For using LaunchDarkly in C / C++ client-side applications, refer to our [C / C++ client-side SDK](https://github.com/launchdarkly/c-client).

Quick setup
-------------------

Unlike other LaunchDarkly SDKs, the C SDK has no installation steps. To get started, clone [this repository](https://github.com/launchdarkly/c-client-server-side) or download a release archive from the [GitHub Releases](https://github.com/launchdarkly/c-client-server-side/releases) page. You can use the `CMakeLists.txt` in this repository as a starting point for integrating this SDK into your application.

After reading the platform specific notes for your environment build the SDK by entering the source directory and running:

```
mkdir build
cd build
cmake ..
cmake --build .
```

You may need to modify the CMAKE generator (`-G` flag) for your specific environment.

## Ubuntu specific

You can get the required dependencies on Ubuntu Linux with:

```
sudo apt-get update && sudo apt-get install build-essential libcurl4-openssl-dev libpcre3-dev
```

Other Linux distributions are supported but will have different packaging requirements.

## Windows specific

### environment

Visual Studio command prompt can be configured for multiple environments. To ensure that you are using your intended tool chain you can launch an environment with: `call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\Tools\VsDevCmd.bat" -host_arch=amd64 -arch=amd64`, where `arch` is your target, `host_arch` is the platform you are building on, and the path is your path to `VsDevCmd.bat`. You will need to modify the above command for your specific setup.

### libcurl

You can obtain the libcurl dependency at [curl.haxx.se](https://curl.haxx.se/download/curl-7.59.0.zip). Extract this archive into the SDK source directory. To build libcurl run:

```
cd curl-7.59.0/winbuild
nmake /f Makefile.vc mode=static
```

### libpcre

You can obtain the libpcre dependency at [ftp.pcre.org](https://ftp.pcre.org/pub/pcre/pcre-8.43.zip). Extract this archive into the SDK source directory. To build libpcre run:

```
cd pcre-8.43
mkdir build
cd build
cmake -G "Visual Studio 15 2017 Win64" ..
cmake --build .
```

You may need to modify the CMAKE generator (`-G` flag) for your specific environment.

## OSX specific

You can get the required dependencies on OSX with:

```
brew install cmake pcre
```

Getting started
---------------

Once integrated, you can follow these steps to initialize a client instance:

1. Include the LaunchDarkly SDK headers:

```C
#include <launchdarkly/api.h>
```

2. Create a new LDClient instance and user with your SDK key:

```C
unsigned int maxwaitmilliseconds = 10 * 1000;
struct LDConfig *config = LDConfigNew("YOUR_CLIENT_SIDE_KEY");
struct LDUser *user = LDUserNew("YOUR_USER_KEY");
struct LDClient *client = LDClientInit(config, maxwaitmilliseconds);
```

In most cases, you should create a single `LDClient` instance for the lifecycle of your program (a singleton pattern can be helpful here). When finished with the client (or prior to program exit), you should close the client:

```C
LDClientClose(client);
```

Your first feature flag
-----------------------

1. Create a new feature flag on your [dashboard](https://app.launchdarkly.com)
2. In your application code, use the feature's key to check whether the flag is on for each user:

```C
show_feature = LDBoolVariation(client, user, "your.flag.key", false, NULL);
if (show_feature) {
    // application code to show the feature
} else {
    // the code to run if the feature is off
}
```

You'll also want to ensure that the client is initialized before checking the flag:

```C
initialized = LDClientIsInitialized(client);
```

Learn more
-----------

Check out our [documentation](http://docs.launchdarkly.com) for in-depth instructions on configuring and using LaunchDarkly. You can also head straight to the [complete reference guide for this SDK](http://docs.launchdarkly.com/v2.0/docs/c-server-side-sdk-reference).

Testing
-------

We run integration tests for all our SDKs using a centralized test harness. This approach gives us the ability to test for consistency across SDKs, as well as test networking behavior in a long-running application. These tests cover each method in the SDK, and verify that event sending, flag evaluation, stream reconnection, and other aspects of the SDK all behave correctly.

Contributing
------------

We encourage pull-requests and other contributions from the community. We've also published an [SDK contributor's guide](http://docs.launchdarkly.com/v2.0/docs/sdk-contributors-guide) that provides a detailed explanation of how our SDKs work.

About LaunchDarkly
-----------

* LaunchDarkly is a continuous delivery platform that provides feature flags as a service and allows developers to iterate quickly and safely. We allow you to easily flag your features and manage them from the LaunchDarkly dashboard.  With LaunchDarkly, you can:
    * Roll out a new feature to a subset of your users (like a group of users who opt-in to a beta tester group), gathering feedback and bug reports from real-world use cases.
    * Gradually roll out a feature to an increasing percentage of users, and track the effect that the feature has on key metrics (for instance, how likely is a user to complete a purchase if they have feature A versus feature B?).
    * Turn off a feature that you realize is causing performance problems in production, without needing to re-deploy, or even restart the application with a changed configuration file.
    * Grant access to certain features based on user attributes, like payment plan (eg: users on the `gold` plan get access to more features than users in the `silver` plan). Disable parts of your application to facilitate maintenance, without taking everything offline.
* Explore LaunchDarkly
    * [www.launchdarkly.com](http://www.launchdarkly.com/ "LaunchDarkly Main Website") for more information
    * [docs.launchdarkly.com](http://docs.launchdarkly.com/  "LaunchDarkly Documentation") for our documentation and SDKs
    * [apidocs.launchdarkly.com](http://apidocs.launchdarkly.com/  "LaunchDarkly API Documentation") for our API documentation
    * [blog.launchdarkly.com](http://blog.launchdarkly.com/  "LaunchDarkly Blog Documentation") for the latest product updates

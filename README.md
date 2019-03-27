LaunchDarkly SDK for C / C++
===================================

*Warning:* This software is *beta* software and should not be used in a production environment until the general availability release.

Quick setup (POSIX)
-------------------

The C / C++ SDK requires a POSIX environment, and assumes that `libcurl`, `libpthread`, `libpcre`, and `libgmp` are installed.

Unlike other LaunchDarkly SDKs, the C SDK has no installation steps. To get started, clone [this repository](https://github.com/launchdarkly/c-client-server-side) or download a release archive from the [GitHub Releases](https://github.com/launchdarkly/c-client-server-side/releases) page. You can use the `Makefile` in this repository as a starting point for integrating this SDK into your application.

You can get the required dependencies on Ubuntu Linux with:

```
sudo apt-get update && sudo apt-get install build-essential libcurl4-openssl-dev libpcre3-dev libgmp-dev
```

Getting started
---------------

Once integrated, you can follow these steps to initialize a client instance:

1. Include the LaunchDarkly SDK headers:

```C
#include "ldapi.h"
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
show_feature = LDBoolVariation(client, user, "your.flag.key", false);
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

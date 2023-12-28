# Change log

All notable changes to the LaunchDarkly C server-side SDK will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org).

## [2.9.3] - 2023-12-28
### Fixed:
- Patched clibs/sha1 dependency to rename SHA1 symbol, avoiding naming conflict with commonly used libcrypto SHA1 symbol.

## [2.9.2] - 2023-08-29
### Fixed:
- Fixed cause of potential crash when using test data feature. Thanks, @cochicde!

## [2.9.1] - 2023-05-05
### Fixed:
- Fixed an issue which was preventing the redis persistent store from initializing correctly.

## [2.9.0] - 2023-01-27
### Changed:
- Changed the internal abstractions used for feature stores and updated the implementations to improve performance and reduce memory usage.

### Fixed:
- Fixed an issue preventing the proper generation of debug events.

## [2.8.7] - 2022-10-25
### Changed:
- Testing: refactored cmake project tests to remove unnecessary boilerplate.
- Bumped c-sdk-common dependency, introducing experimental APIs. Such APIs are subject to change or removal, and carry no guarantees.

## [2.8.6] - 2022-10-12
### Fixed:
- Fixed flag evaluation to report error when flag data is malformed (variation out of bounds).
- Fixed use-after-free in situation where a prerequisite flag has a NULL off-variation.
- Fixed `LDAllFlagsState` handling of malformed flags, and fixed JSON serialization to omit details properly.

## [2.8.5] - 2022-09-01
### Fixed:
- Fixed support for CMake versions 3.11 through 3.13, which require an explicit LIBRARY DESTINATION path when installing the SDK's target.

## [2.8.4] - 2022-09-01
### Fixed:
- Fixed ldserverapi shared library symbol visibility: undocumented symbols previously exposed by 3rd party dependencies are now hidden.

## [2.8.3] - 2022-08-09
### Fixed:
- Fixed an issue where the SDK was erroneously logging errors when evaluating flags which have an `offVariation` of `null` or when the `offVariation` was not specified.

## [2.8.2] - 2022-08-04
### Fixed:
- Fixed bug in cmake configuration causing build failures on some platforms due to omission of position-independent code flag.

## [2.8.1] - 2022-07-28
### Fixed:
- Fixed build error on platforms where char is unsigned by default.

## [2.8.0] - 2022-07-12
### Added:
- Added CMake minimum version table to README.
- Added 3rd-party dependency table to README.

### Changed:
- Updated CMake project configuration to support find_package. 
- After installation, it should now be possible to call `find_package(ldserverapi [version] REQUIRED)` followed by `target_link_libraries(app ldserverapi::ldserverapi)`.
- Note: find_package support does not extend to the Redis integration at this time. 
- Please see the README for updated instructions.

## [2.7.2] - 2022-07-07
### Added:
- Added cmake PROJECT_VERSION variable which reflects the SDK's version.

### Fixed:
- Fixed behavior of patch event handling to properly ignore unrecognized paths.

## [2.7.1] - 2022-06-24
### Fixed:
- Fixed generation of ldserverapi static library release artifacts, which would cause undefined symbol errors during linking.

## [2.7.0] - 2022-06-02
### Added:
- Added section to README.md with basic CMake integration instructions.

### Changed:
- The SDK previously vendored & patched a set of 3rd-party dependencies. These dependencies are now obtained via CMake's `FetchContent` API, similar to `googletest`. This required a CMake version bump from `3.10` to `3.11`.


### Removed:
- Removed most 3rd-party dependencies in `third-party` subdirectory.
- Removed unused `base64` dependency.

## [2.6.2] - 2022-05-26
### Fixed:
- Null user attributes should never result in a rule match.
- `debugEventsUntilDate` property of flags obtained via `LDAllFlagsState` API no longer overflows due to millisecond-precision timestamping.

## [2.6.1] - 2022-05-06
### Changed:
- Updated language in changelog related to Relay Proxy.
- Updated c-sdk-common dependency.

### Fixed:
- Users with empty keys should not generate identify events.
- Removed redundant index event that was generated after an identify event.
- Fixed handling of per-user private attributes in SDK contract tests.

## [2.6.0] - 2022-04-20
### Added:
- Added `integrations/test_data.h`. Test Data is a new way to inject feature flag data programmatically into the SDK for testing — either with fixed values for each flag, or with targets and/or rules that can return different values for different users. 

- Added `integrations/file_data.h`. File Data is a new way to inject feature flag data via an external resource.

### Fixed:
- Fixed bug in `LDAllFlagsState` where invalid flags were included in the output.
- Fixed `LDConfigSetSendEvents` to properly disable analytic events when set to false. As part of this change, public functions `LDClientTrack`, `LDClientTrackMetric`, `LDClientAlias`, and `LDClientIdentify` now return true when analytic events are disabled.
- Fixed JSON serialization of time values to remove fractional millisecond component.

## [2.5.0] - 2022-02-23
### Added:
- Added `LDVersion()`, equivalent to `LD_SDK_VERSION`. Useful when wrapping the SDK with a higher level language that cannot access the `LD_SDK_VERSION` symbol. 
- Added `LDAllFlagsState` API for fetching flag values and details. This API is commonly used to [bootstrap](https://docs.launchdarkly.com/sdk/features/bootstrapping) the client-side Javascript SDK.

### Deprecated:
- Deprecated `LDAllFlags` in favor of `LDAllFlagsState`. Please invoke `LDAllFlagsState()` followed by `LDAllFlagsStateToValuesMap()` for equivalent behavior. 

### Changed:
- Changed CircleCI macOS executors to Gen2 resource class.
- Swapped CircleCI `helgrind` test for `drd` because of false positives within curl dependency.

### Fixed
- Fixed `LDJSONVariation` to allow for returning a flag of any JSON-representable type (previously would return an error if the expected flag type was not object or array.)
- Fixed all `LDVariation` variants to return `hasVariation = false` within `LDDetails` when caller requests wrong flag type.
- Fixed handling of base/stream URIs set by `LDConfigSet[Base/Stream]URI` with an unintentional trailing slash.
- Fixed double-free bug within `LDStoreInitialized`.

## [2.4.5] - 2022-01-20
### Changed:
- Updated CI with dedicated valgrind & helgrind jobs
- Added internal-only JSON helpers.

### Fixed:
- Various grammar & spelling issues

## [2.4.4] - 2021-12-14
### Changed:
- Removed unused internal headers
- Fixed comment spelling issues
- Updated spelling in an error log message (`checkPrequisites failed` -> `checkPrerequisites failed`)

### Fixed:
- Fixed Windows CI build by updating PCRE download URL to new host

## [2.4.3] - 2021-10-19
### Changed:
- Updated the build process to produce release artifacts for Windows.
- Updated unit tests to use GoogleTest.

## [2.4.2] - 2021-09-17
### Fixed:
- In cases where there was a malformed feature flag `LDAllFlags` would return null instead of all of the valid flags.
- When a malformed feature flag was evaluated events would not be generated for that flag.

## [2.4.1] - 2021-07-21
### Fixed:
- Refactored parsing logic to provide better error messages and be more reliable.


## [2.4.0] - 2021-06-20
### Added:
- The SDK now supports the ability to control the proportion of traffic allocation to an experiment. This works in conjunction with a new platform feature now available to early access customers.


## [2.3.4] - 2021-06-14
### Fixed:
- Removed stray imports of stdbool.h


## [2.3.3] - 2021-06-09
### Fixed:
- Removed stray imports of stdbool.h


## [2.3.2] - 2021-05-26
### Fixed:
- Corrected rollout bucketing behavior when bucketing by a user attribute that does not exist.
- Fixed handling of flag rollout bucketing by a specific user attribute.


## [2.3.1] - 2021-05-12
### Changed:
- Now building with -Werror on Linux and macOS
- Refactored to reduce code duplication between c-client and c-server
- Improved c89 compatibility
- Added clang-format

### Fixed:
- A race condition inside the in memory cache when utilizing an external data store. Thanks @lcapaldo
- An instance of using the incorrect enum `LD_ERROR` instead of `LD_LOG_ERROR` with `LD_LOG`
- Resolved many build warnings (that do not impact semantics)

## [2.3.0] - 2021-01-28
### Added:
- Added the `LDClientAlias` function. This can be used to associate two user objects for analytics purposes with an alias event.


## [2.2.1] - 2021-01-14
### Added:
- Vendored Findhiredis to ease building the Redis integration
- Added code coverage reporting functionality to CMake
- Increased unit test coverage

### Fixed:
- Added ability to target secondary key
- Internal assertions that should of been API assertions
- A memory leak related to every delivery
- Empty summary events being sent when other events were also being sent
- A leak of custom attributes if the attributes were replaced after already being set
- Fixed inverted defensive check in LDClientIsOffline
- Fixed stream read timeout handling

## [2.2.0] - 2020-11-24
### Added:
- LDBasicLoggerThreadSafeInitialize used to setup LDBasicLoggerThreadSafe
- LDBasicLoggerThreadSafe a thread safe alternative to LDBasicLogger
- LDBasicLoggerThreadSafeShutdown to cleanup LDBasicLoggerThreadSafe resources

### Changed:
- OSX artifacts are now generated with Xcode 9.4.1

### Fixed:
- All helgrind concurrency warnings

### Deprecated:
- Marked LDBasicLogger as deprecated


## [2.1.4] - 2020-07-01
### Fixed:
- Added extra public headers to the `ldserversdk` target to fix Redis usage when embedding the cmake project
- Fixed generation of full fidelity evaluation events for the LaunchDarkly experimentation feature

## [2.1.3] - 2020-06-25
### Changed:
- Set the &#34;Accept&#34; header to &#34;text/event-stream&#34; for streaming connections.

## [2.1.2] - 2020-06-19
### Fixed:
- `usleep(3)` setting `errno` to `EINTR` is no longer considered a fatal error.


## [2.1.1] - 2020-06-18
### Fixed:
- Cap usage of usleep(3) at 999999 usec which is required on certain platforms
- When an error occurs with usleep(3) print errno and not the status code

## [2.1.0] - 2020-05-12
### Added:
- LDConfigSetWrapperInfo which is used to inform the LaunchDarkly backend of an SDK wrappers name and version. Currently used in our Lua server-side SDK.

## [2.0.0] - 2020-04-30
Version `2.0.0` brings many changes, however they are primarily focused on edge cases and internal details. For most customers, this release will be a drop-in replacement and will not require any changes in your SDK usage. When using the SDK, you should either install the SDK configured for your desired location with `cmake --build . --target install` or use it as a CMake sub-project with the `ldserverapi` target. The internal organization of files is not part of the API and may change in future releases.

### Added:
- Added the capability to remove all assertions at compile time. This is controlled with the compile time definition `LAUNCHDARKLY_USE_ASSERT`.

### Fixed:
- Fixed handling of large time values to fix overflow issues on certain systems: For example 32 bit builds and Windows.
- Waiting until timeout on streaming mode initialization.
- Made CMake more hygienic with target based compilation options.
- Fixed many miscellaneous warnings. Now the SDK is much closer to ANSI compatibility.
- Disabled certain unneeded warnings on Windows.

### Changed:
- By default all API operations now use defensive checks instead of assertions by default. This can be controlled based on preference with the compile time definition `LAUNCHDARKLY_DEFENSIVE`.
- The `LDStringVariation` and `LDJSONVariation` functions now accept `NULL` as fallback values.
- Binary releases are now archives which contain both the headers, and binary, for each platform. These can be extracted and used directly.
- All `bool` values in the public API now use `LDBoolean`. This value is a number containing either zero or one. It is currently a `char`, however this should be considered an opaque detail. This is part of ANSI C compatibility progress.
- Switched to usage of `PTHREAD_MUTEX_ERRORCHECK` on POSIX. This is controlled with the compile time definition `LAUNCHDARKLY_CONCURRENCY_UNSAFE`.
- Optimized internals to reduce locking, and switch certain reader/writer locks to mutexes, based on usage pattern.
- Detangled headers reducing complexity.
- Added a mock HTTP server used for integration testing.
- Added stricter compilation options than the exiting `-Wextra`.
- Greatly simplified internal event generation logic.
- Increased error checking around many system operations.


## [1.2.1] - 2020-03-31
### Fixed:
- Standardized polling retry behavior. It now simply waits until the next poll interval.
- Standardized event delivery retry behavior. It now retries delivery after one second, a single time.
- Standardized streaming retry behavior. It now correctly follows exponential back-off, changed how status codes are interpreted, and resets back-off after 60 seconds of being successfully connected.

## [1.2.0] - 2020-01-22
### Added:
- Added support for utilizing external feature stores. See the new `launchdarkly/store.h` file for details on implementing a store. You can configure usage of a specific store with `LDConfigSetFeatureStoreBackend`.
- Added support for Redis as an external feature store. To build add `-D REDIS_STORE=ON` to your cmake build. This will require the hiredis library to be installed on your system.
- Added LaunchDarkly daemon mode configurable with `LDConfigSetUseLDD` for use with the Relay Proxy. To learn more, read [Using daemon mode](https://docs.launchdarkly.com/home/relay-proxy/using#using-daemon-mode).
- Added a code coverage reporting system. To build with code coverage add `-D COVERAGE=ON` to your cmake build. Run the tests, then build the `coverage` target. This will generate an HTML report.
### Fixed:
- The SDK now specifies a uniquely identifiable request header when sending events to LaunchDarkly to ensure that events are only processed once, even if the SDK sends them two times due to a failed initial attempt.
- Change how time is handled on OSX to support old OSX versions.

## [1.1.0] - 2019-09-26
### Added
- Added `LDClientTrackMetric` which is an extended version of `LDClientTrack` but with an extra associated metric value.
### Fixed
- Corrected `LDReasonToJSON` encoding of the `RULE_MATCH` case.

## [1.0.2] - 2019-07-26
### Fixed
- Undefined behavior associated with branching on an uninitialized variable when parsing server time headers

## [1.0.1] - 2019-07-11
### Fixed
- Updated CMAKE header search to include `/usr/local/include` for OSX Mojave

## [1.0.0] - 2019-07-05
### Added
- LRU caching to prevent users being indexed multiple times
### Fixed
- The server HTTP response time parsing logic

## [1.0.0-beta.4] - 2019-06-11
### Fixed
- Memory leak when evaluating flags with prerequisites

## [1.0.0-beta.3] - 2019-05-17
### Added
- Completed Doxygen coverage
### Changed
- The header files have been reorganized. Now include "launchdarkly/api.h".

## [1.0.0-beta.2] - 2019-04-05
### Added
- Windows support
- Networking retry timing jitter
- Analytics `debugEventsUntilDate` support

## [1.0.0-beta.1] - 2019-03-27
### Added
- Initial public API
- OSX / Linux builds

### Missing
- Windows support
- Networking retry timing jitter
- Analytics `debugEventsUntilDate` support

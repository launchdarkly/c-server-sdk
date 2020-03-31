# Change log

All notable changes to the LaunchDarkly C server-side SDK will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org).

## [1.2.1] - 2020-03-31
### Fixed:
- Standardized polling retry behavior. It now simply waits until the next poll interval.
- Standardized event delivery retry behavior. It now retries delivery after one second, a single time.
- Standardized streaming retry behavior. It now correctly follows exponential back-off, changed how status codes are interpreted, and resets back-off after 60 seconds of being successfully connected.

## [1.2.0] - 2020-01-22
### Added:
- Added support for utilizing external feature stores. See the new `launchdarkly/store.h` file for details on implementing a store. You can configure usage of a specific store with `LDConfigSetFeatureStoreBackend`.
- Added support for Redis as an external feature store. To build add `-D REDIS_STORE=ON` to your cmake build. This will require the hiredis library to be installed on your system.
- Added LaunchDarkly daemon mode configurable with `LDConfigSetUseLDD`.
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

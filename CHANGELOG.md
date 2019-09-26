# Change log

All notable changes to the LaunchDarkly C server-side SDK will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org).

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

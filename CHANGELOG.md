# Change log

All notable changes to the LaunchDarkly C server-side SDK will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org).

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

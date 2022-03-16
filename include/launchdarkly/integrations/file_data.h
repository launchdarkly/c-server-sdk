/*!
 * @file integrations/file_data.h
 * @brief Public API for file data source
 *
 * @details
 * Integration between the LaunchDarkly SDK and file data.
 *
 * The file data source allows you to use local files as a source of feature flag state. This would
 * typically be used in a test environment, to operate using a predetermined feature flag state
 * without an actual LaunchDarkly connection. See LDFileDataInit() for details.
 *
 */

#pragma once

#include <stdio.h>
#include <stdarg.h>

#include <launchdarkly/boolean.h>
#include <launchdarkly/export.h>
#include <launchdarkly/data_source.h>

/**
 * @brief Creates an {@link LDDataSource} which you can use to configure the client with a file data source.
 *
 * @param[in] fileCount The number of filename arguments that are going to be passed in.
 * @param[in] filenames The filenames of the files to load flags from
 * @return a data source configuration object
 *
 * @details
 * This allows you to use local files as a source of feature flag state,
 * instead of using an actual LaunchDarkly connection.
 *
 * This function can be called with fileCount file names and will load them all into the
 * data store.
 * @code{.c}
 *     struct LDClient *client;
 *     struct LDConfig *config = makeMyConfig();
       const char *filenames[2] = {"../tests/datafiles/flag-only.json", "../tests/datafiles/flag-with-duplicate-key.json"};
 *
 *     LDConfigSetDataSource(config, LDFileDataInit(2, filenames));
 *
 *     if(!(client = LDClientInit(config, 10))) {
 *         LDConfigFree(config);
 *         return;
 *     }
 * @endcode
 *
 * This will cause the client <i>not</i> to connect to LaunchDarkly to get feature flags. The
 * client may still make network connections to send analytics events, unless you have disabled
 * this with LDConfigSetSendEvents().
 *
 * Flag data files should be encoded using JSON. They contain an object with three possible
 * properties:
 * @li @c flags Feature flag definitions.
 * @li @c flagVersions Simplified feature flags that contain only a value.
 * @li @c segments User segment definitions.
 *
 * The format of the data in @c flags and @c segments is defined by the LaunchDarkly application
 * and is subject to change. Rather than trying to construct these objects yourself, it is simpler
 * to request existing flags directly from the LaunchDarkly server in JSON format, and use this
 * output as the starting point for your file. In Linux you would do this:
 * @code{.sh}
 * curl -H "Authorization: {your sdk key}" https://app.launchdarkly.com/sdk/latest-all
 * @endcode
 *
 * The output will look something like this (but with many more properties):
 * @code{.json}
 * {
 *     "flags": {
 *         "flag-key-1": {
 *             "key": "flag-key-1",
 *             "on": true,
 *             "variations": [ "a", "b" ]
 *         },
 *         "flag-key-2": {
 *             "key": "flag-key-2",
 *             "on": true,
 *             "variations": [ "c", "d" ]
 *         }
 *     },
 *     "segments": {
 *         "segment-key-1": {
 *             "key": "segment-key-1",
 *             "includes": [ "user-key-1" ]
 *         }
 *     }
 * }
 * @endcode
 *
 * Data in this format allows the SDK to exactly duplicate all the kinds of flag behavior supported
 * by LaunchDarkly. However, in many cases you will not need this complexity, but will just want to
 * set specific flag keys to specific values. For that, you can use a much simpler format:
 *
 * @code{.json}
 * {
 *     "flagValues": {
 *         "my-string-flag-key": "value-1",
 *         "my-boolean-flag-key": true,
 *         "my-integer-flag-key": 3
 *     }
 * }
 * @endcode
 *
 * It is also possible to specify both @c flags and @c flagValues, if you want some flags
 * to have simple values and others to have complex behavior. However, it is an error to use the
 * same flag key or segment key more than once, either in a single file or across multiple files.
 *
 * If the data source encounters a duplicate key it will ignore it and use the previously loaded flag.
 *
 */
LD_EXPORT(struct LDDataSource *)
LDFileDataInit(int fileCount, const char **filenames);

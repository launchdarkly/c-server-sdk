/*!
 * @file client.h
 * @brief Public API Interface for Client operations
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <launchdarkly/config.h>
#include <launchdarkly/user.h>
#include <launchdarkly/json.h>
#include <launchdarkly/export.h>

/**
 * @brief Initialize a new client, and connect to LaunchDarkly.
 * @param[in] config The client configuration. Ownership of `config` is
 * transferred.
 * @param[in] maxwaitmilli How long to wait for flags to download.
 * If the timeout is reached a non fully initialized client will be returned.
 * @return A fresh client
 */
LD_EXPORT(struct LDClient *) LDClientInit(struct LDConfig *const config,
    const unsigned int maxwaitmilli);

/**
 * @brief Shuts down the LaunchDarkly client. This will block until all
 * resources have been freed. It is not safe to use the client during or after
 * this operation. It is safe to initialize another client after this operation
 * is completed.
 * @param[in] client The client to use.
 * @return Void.
 */
LD_EXPORT(void) LDClientClose(struct LDClient *const client);

/**
 * @brief Check if a client has been fully initialized. This may be useful
 * if the initialization timeout was reached in `LDClientInit`.
 * @param[in] client The client to use. May not be `NULL` (assert).
 * @return true if fully initialized
 */
LD_EXPORT(bool) LDClientIsInitialized(struct LDClient *const client);

/**
 * @brief Reports that a user has performed an event. Custom data can be
 * attached to the event as JSON.
 * @param[in] client The client to use. May not be `NULL` (assert).
 * @param[in] key The name of the event
 * @param[in] user The user to generate the event for. Ownership is not
 * transferred. May not be `NULL` (assert).
 * @param[in] data The JSON to attach to the event. Ownership of `data` is
 * transferred.
 * @return true if the event was queued, false on error
 */
LD_EXPORT(bool) LDClientTrack(struct LDClient *const client,
    const char *const key, const struct LDUser *const user,
    struct LDJSON *const data);

/**
 * @brief Reports that a user has performed an event. Custom data, and a metric
 * can be attached to the event as JSON.
 * @param[in] client The client to use. May not be `NULL` (assert).
 * @param[in] key The name of the event
 * @param[in] user The user to generate the event for. Ownership is not
 * transferred. May not be `NULL` (assert).
 * @param[in] data The JSON to attach to the event. Ownership of `data` is
 * transferred.
 * @param[in] metric A metric to be assocated with the event
 * @return true if the event was queued, false on error
 */
LD_EXPORT(bool) LDClientTrackMetric(struct LDClient *const client,
    const char *const key, const struct LDUser *const user,
    struct LDJSON *const data, const double metric);

/**
 * @brief Generates an identify event for a user.
 * @param[in] client The client to use. May not be `NULL` (assert).
 * @param[in] user The user to generate the event for. Ownership is not
 * transferred. May not be `NULL` (assert).
 * @return true if the event was queued, false on error
 */
LD_EXPORT(bool) LDClientIdentify(struct LDClient *const client,
    const struct LDUser *const user);

/**
 * @brief Whether the LaunchDarkly client is in offline mode.
 * @param[in] client The client to use. May not be `NULL` (assert).
 * @return true if offline.
 */
LD_EXPORT(bool) LDClientIsOffline(struct LDClient *const client);

/**
 * @brief Immediately flushes queued events.
 * @param[in] client The client to use. May not be `NULL` (assert).
 * @return Void.
 */
LD_EXPORT(void) LDClientFlush(struct LDClient *const client);

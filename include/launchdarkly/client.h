/*!
 * @file client.h
 * @brief Public API Interface for Client operations
 */

#pragma once

#include <launchdarkly/boolean.h>
#include <launchdarkly/config.h>
#include <launchdarkly/export.h>
#include <launchdarkly/json.h>
#include <launchdarkly/user.h>

/**
 * @struct LDClient
 * @brief An opaque client object.
 */
struct LDClient;

/**
 * @brief Initialize a new client, and connect to LaunchDarkly.
 * @param[in] config The client configuration. Ownership of `config` is
 * transferred.
 * @param[in] maxwaitmilli How long to wait for flags to download.
 * If the timeout is reached a non fully initialized client will be returned.
 * @return A fresh client.
 */
LD_EXPORT(struct LDClient *)
LDClientInit(struct LDConfig *const config, const unsigned int maxwaitmilli);

/**
 * @brief Shuts down the LaunchDarkly client. This will block until all
 * resources have been freed. It is not safe to use the client during or after
 * this operation. It is safe to initialize another client after this operation
 * is completed.
 * @param[in] client The client to use. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean) LDClientClose(struct LDClient *const client);

/**
 * @brief Check if a client has been fully initialized. This may be useful
 * if the initialization timeout was reached in `LDClientInit`.
 * @param[in] client The client to use. May not be `NULL`.
 * @return True if fully initialized, False if not or on error.
 */
LD_EXPORT(LDBoolean) LDClientIsInitialized(struct LDClient *const client);

/**
 * @brief Reports that a user has performed an event. Custom data can be
 * attached to the event as JSON.
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] key The name of the event. May not be `NULL`.
 * @param[in] user The user to generate the event for. Ownership is not
 * transferred. May not be `NULL`.
 * @param[in] data The JSON to attach to the event. Ownership of `data` is
 * transferred. May be `NULL`.
 * @return True if the event was queued, False on error.
 */
LD_EXPORT(LDBoolean)
LDClientTrack(
    struct LDClient *const     client,
    const char *const          key,
    const struct LDUser *const user,
    struct LDJSON *const       data);

/**
 * @brief Reports that a user has performed an event. Custom data, and a metric
 * can be attached to the event as JSON.
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] key The name of the event. May not be `NULL`.
 * @param[in] user The user to generate the event for. Ownership is not
 * transferred. May not be `NULL`.
 * @param[in] data The JSON to attach to the event. Ownership of `data` is
 * transferred. May be `NULL`.
 * @param[in] metric A metric to be assocated with the event.
 * @return True if the event was queued, False on error.
 */
LD_EXPORT(LDBoolean)
LDClientTrackMetric(
    struct LDClient *const     client,
    const char *const          key,
    const struct LDUser *const user,
    struct LDJSON *const       data,
    const double               metric);

/**
 * @brief Record a alias event
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] currentUser The new version of a user. Ownership is not
 * transferred. May not be `NULL`.
 * @param[in] previousUser The previous version of a user. Ownership is not
 * transferred. May not be `NULL`.
 * @return True if the event was queued, False on error.
 */
LD_EXPORT(LDBoolean)
LDClientAlias(
    struct LDClient *const     client,
    const struct LDUser *const currentUser,
    const struct LDUser *const previousUser);

/**
 * @brief Generates an identify event for a user.
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] user The user to generate the event for. Ownership is not
 * transferred. May not be `NULL`.
 * @return True if the event was queued, False on error.
 */
LD_EXPORT(LDBoolean)
LDClientIdentify(
    struct LDClient *const client, const struct LDUser *const user);

/**
 * @brief Whether the LaunchDarkly client is in offline mode.
 * @param[in] client The client to use. May not be `NULL`.
 * @return True if offline, False if not or on error.
 */
LD_EXPORT(LDBoolean) LDClientIsOffline(struct LDClient *const client);

/**
 * @brief Immediately flushes queued events.
 * @param[in] client The client to use. May not be `NULL`.
 * @return True if signalled, False on error.
 */
LD_EXPORT(LDBoolean) LDClientFlush(struct LDClient *const client);

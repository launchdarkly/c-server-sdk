/*!
 * @file ldconfig.h
 * @brief Public API Interface for Configuration
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/* **** Forward Declarations **** */

struct LDStore; struct LDConfig;

/**
 * @brief Creates a new default configuration. The configuration object is
 * intended to be modified until it is passed to LDClientInit, at which point
 * it should no longer be modified.
 * @return NULL on failure.
 */
struct LDConfig *LDConfigNew();

/**
 * @brief Destory a config not associated with a client instance.
 * @param[in] config May be NULL.
 * @return Void.
 */
void LDConfigFree(struct LDConfig *const config);

/**
 * @brief Set the base URI for connecting to LaunchDarkly. You probably don't
 * need to set this unless instructed by LaunchDarkly.
 * Defaults to "https://app.launchdarkly.com".
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] baseURI The new base URI to use. May not be NULL (assert).
 * @return True on success, False on failure
 */
bool LDConfigSetBaseURI(struct LDConfig *const config,
    const char *const baseURI);

/**
 * @brief Set the streaming URI for connecting to LaunchDarkly. You probably
 * don't need to set this unless instructed by LaunchDarkly.
 * Defaults to "https://stream.launchdarkly.com".
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] streamURI The new stream URI to use. May not be NULL (assert).
 * @return True on success, False on failure
 */
bool LDConfigSetStreamURI(struct LDConfig *const config,
    const char *const streamURI);

/**
 * @brief Set the events URI for connecting to LaunchDarkly. You probably don't
 * need to set this unless instructed by LaunchDarkly.
 * Defaults to "https://events.launchdarkly.com".
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] eventsURI The new events URI to use. May not be NULL (assert).
 * @return True on success, False on failure
 */
bool LDConfigSetEventsURI(struct LDConfig *const config,
    const char *const eventsURI);

/**
 * @brief Enables or disables real-time streaming flag updates. When set to
 * false, an efficient caching polling mechanism is used.
 * We do not recommend disabling streaming unless you have been instructed to do
 * so by LaunchDarkly support. Defaults to true.
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] stream True for streaming, False for polling.
 * @return Void.
 */
void LDConfigSetStream(struct LDConfig *const config, const bool stream);

/**
 * @brief Sets whether to send analytics events back to LaunchDarkly. By
 * default, the client will send events. This differs from Offline in that it
 * only affects sending events, not streaming or polling for events from the
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] sendEvents
 * @return Void.
 */
void LDConfigSetSendEvents(struct LDConfig *const config,
    const bool sendEvents);

/**
 * @brief The capacity of the events buffer. The client buffers up to this many
 * events in memory before flushing. If the capacity is exceeded before the
 * buffer is flushed, events will be discarded.
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] eventsCapacity
 * @return Void.
 */
void LDConfigSetEventsCapacity(struct LDConfig *const config,
    const unsigned int eventsCapacity);

/**
 * @brief The connection timeout to use when making requests to LaunchDarkly.
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] milliseconds
 * @return Void.
 */
void LDConfigSetTimeout(struct LDConfig *const config,
    const unsigned int milliseconds);

/**
 * @brief The time between flushes of the event buffer. Decreasing the flush
 * interval means that the event buffer is less likely to reach capacity.
 * @param[in] milliseconds
 * @return Void.
 */
void LDConfigSetFlushInterval(struct LDConfig *const config,
    const unsigned int milliseconds);

/**
 * @brief The polling interval (when streaming is disabled).
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] milliseconds
 * @return Void.
 */
void LDConfigSetPollInterval(struct LDConfig *const config,
    const unsigned int milliseconds);

/**
 * @brief Sets whether this client is offline.
 * An offline client will not make any network connections to LaunchDarkly,
 * and will return default values for all feature flags.
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] offline
 * @return Void.
 */
void LDConfigSetOffline(struct LDConfig *const config, const bool offline);

/**
 * @brief Sets whether this client should use the LaunchDarkly relay
 * in daemon mode. In this mode, the client does not subscribe to the streaming
 * or polling API, but reads data only from the feature store.
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] useLDD
 * @return Void.
 */
void LDConfigSetUseLDD(struct LDConfig *const config, const bool useLDD);

/**
 * @brief Sets whether or not all user attributes (other than the key) should be
 * hidden from LaunchDarkly. If this is true, all user attribute values will be
 * private, not just the attributes specified in PrivateAttributeNames.
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] allAttributesPrivate
 * @return Void.
 */
void LDConfigSetAllAttributesPrivate(struct LDConfig *const config,
    const bool allAttributesPrivate);

/**
 * @brief The number of user keys that the event processor can remember at an
 * one time, so that duplicate user details will not be sent in analytics..
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] userKeysCapacity
 * @return Void
 */
void LDConfigSetUserKeysCapacity(struct LDConfig *const config,
    const unsigned int userKeysCapacity);

/**
 * @brief The interval at which the event processor will reset its set of known
 * user keys.
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] userKeysFlushInterval
 * @return Void
 */
void LDConfigSetUserKeysFlushInterval(struct LDConfig *const config,
    const unsigned int userKeysFlushInterval);

/**
 * @brief Marks a set of user attribute names private. Any users sent to
 * LaunchDarkly with this configuration active will have attributes with these
 * names removed.
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] attribute May not be NULL (assert).
 * @return NULL on failure.
 */
bool LDConfigAddPrivateAttribute(struct LDConfig *const config,
    const char *const attribute);

/**
 * @brief Sets the implementation of FeatureStore for holding feature flags and
 * related data received from LaunchDarkly.
 * @param[in] config The configuration to modify. May not be NULL (assert).
 * @param[in] store. May not be NULL (assert).
 * @return NULL on failure.
 */
void LDConfigSetFeatureStore(struct LDConfig *const config,
    struct LDStore *const store);

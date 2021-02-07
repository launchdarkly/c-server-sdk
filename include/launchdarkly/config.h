/*!
 * @file config.h
 * @brief Public API Interface for Configuration
 */

#pragma once

#include <launchdarkly/boolean.h>
#include <launchdarkly/export.h>
#include <launchdarkly/store.h>

/**
 * @struct LDConfig
 * @brief An opaque config object.
 */
struct LDConfig;

/**
 * @brief Creates a new default configuration. The configuration object is
 * intended to be modified until it is passed to LDClientInit, at which point
 * it should no longer be modified.
 * @param[in] key Your projects SDK key. May not be `NULL`.
 * @return `NULL` on failure.
 */
LD_EXPORT(struct LDConfig *) LDConfigNew(const char *const key);

/**
 * @brief Destroy a config not associated with a client instance.
 * @param[in] config May be `NULL`.
 * @return Void.
 */
LD_EXPORT(void) LDConfigFree(struct LDConfig *const config);

/**
 * @brief Set the base URI for connecting to LaunchDarkly. You probably don't
 * need to set this unless instructed by LaunchDarkly.
 * Defaults to "https://app.launchdarkly.com".
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] baseURI The new base URI to use. May not be `NULL`.
 * @return True on success, False on failure
 */
LD_EXPORT(LDBoolean)
LDConfigSetBaseURI(struct LDConfig *const config, const char *const baseURI);

/**
 * @brief Set the streaming URI for connecting to LaunchDarkly. You probably
 * don't need to set this unless instructed by LaunchDarkly.
 * Defaults to "https://stream.launchdarkly.com".
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] streamURI The new stream URI to use. May not be `NULL`.
 * @return True on success, False on failure
 */
LD_EXPORT(LDBoolean)
LDConfigSetStreamURI(
    struct LDConfig *const config, const char *const streamURI);

/**
 * @brief Set the events URI for connecting to LaunchDarkly. You probably don't
 * need to set this unless instructed by LaunchDarkly.
 * Defaults to "https://events.launchdarkly.com".
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] eventsURI The new events URI to use. May not be `NULL`.
 * @return True on success, False on failure
 */
LD_EXPORT(LDBoolean)
LDConfigSetEventsURI(
    struct LDConfig *const config, const char *const eventsURI);

/**
 * @brief Enables or disables real-time streaming flag updates. When set to
 * false, an efficient caching polling mechanism is used.
 * We do not recommend disabling streaming unless you have been instructed to do
 * so by LaunchDarkly support. Defaults to true.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] stream True for streaming, False for polling.
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetStream(struct LDConfig *const config, const LDBoolean stream);

/**
 * @brief Sets whether to send analytics events back to LaunchDarkly. By
 * default, the client will send events. This differs from Offline in that it
 * only affects sending events, not streaming or polling for events from the
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] sendEvents
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetSendEvents(
    struct LDConfig *const config, const LDBoolean sendEvents);

/**
 * @brief The capacity of the events buffer. The client buffers up to this many
 * events in memory before flushing. If the capacity is exceeded before the
 * buffer is flushed, events will be discarded.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] eventsCapacity
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetEventsCapacity(
    struct LDConfig *const config, const unsigned int eventsCapacity);

/**
 * @brief The connection timeout to use when making requests to LaunchDarkly.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] milliseconds
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetTimeout(
    struct LDConfig *const config, const unsigned int milliseconds);

/**
 * @brief The time between flushes of the event buffer. Decreasing the flush
 * interval means that the event buffer is less likely to reach capacity.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] milliseconds
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetFlushInterval(
    struct LDConfig *const config, const unsigned int milliseconds);

/**
 * @brief The polling interval (when streaming is disabled).
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] milliseconds
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetPollInterval(
    struct LDConfig *const config, const unsigned int milliseconds);

/**
 * @brief Sets whether this client is offline.
 * An offline client will not make any network connections to LaunchDarkly,
 * and will return default values for all feature flags.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] offline
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetOffline(struct LDConfig *const config, const LDBoolean offline);

/**
 * @brief Sets whether this client should use the LaunchDarkly relay
 * in daemon mode. In this mode, the client does not subscribe to the streaming
 * or polling API, but reads data only from the feature store.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] useLDD
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetUseLDD(struct LDConfig *const config, const LDBoolean useLDD);

/**
 * @brief Sets whether or not all user attributes (other than the key) should be
 * hidden from LaunchDarkly. If this is true, all user attribute values will be
 * private, not just the attributes specified in PrivateAttributeNames.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] allAttributesPrivate
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetAllAttributesPrivate(
    struct LDConfig *const config, const LDBoolean allAttributesPrivate);

/**
 * @brief Set to true if you need to see the full user details in every
 * analytics event.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] inlineUsersInEvents
 * @return Void.
 */
LD_EXPORT(void)
LDConfigInlineUsersInEvents(
    struct LDConfig *const config, const LDBoolean inlineUsersInEvents);

/**
 * @brief The number of user keys that the event processor can remember at an
 * one time, so that duplicate user details will not be sent in analytics.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] userKeysCapacity
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetUserKeysCapacity(
    struct LDConfig *const config, const unsigned int userKeysCapacity);

/**
 * @brief The interval at which the event processor will reset its set of known
 * user keys.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] milliseconds
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetUserKeysFlushInterval(
    struct LDConfig *const config, const unsigned int milliseconds);

/**
 * @brief Marks a set of user attribute names private. Any users sent to
 * LaunchDarkly with this configuration active will have attributes with these
 * names removed.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] attribute May not be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDConfigAddPrivateAttribute(
    struct LDConfig *const config, const char *const attribute);

/**
 * @brief Sets the implementation of FeatureStore for holding feature flags and
 * related data received from LaunchDarkly.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] backend May be `NULL`.
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetFeatureStoreBackend(
    struct LDConfig *const config, struct LDStoreInterface *const backend);

/**
 * @brief When a feature store backend is provided, configure how long items
 * will be cached in memory. Set the value to zero to always hit the external
 * store backend. If no backend exists this value is ignored. The default cache
 * time is 30 seconds.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] milliseconds How long the cache should live.
 * @return Void.
 */
LD_EXPORT(void)
LDConfigSetFeatureStoreBackendCacheTTL(
    struct LDConfig *const config, const unsigned int milliseconds);

/**
 * @brief Indicates to LaunchDarkly the name and version of an SDK wrapper
 * library. If `wrapperVersion` is set `wrapperName` must be set.
 * @param[in] config The configuration to modify. May not be `NULL`.
 * @param[in] wrapperName The name of the wrapper. May be `NULL`.
 * @param[in] wrapperVersion The version of the wrapper. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDConfigSetWrapperInfo(
    struct LDConfig *const config,
    const char *const      wrapperName,
    const char *const      wrapperVersion);

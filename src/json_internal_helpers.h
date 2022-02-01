/*!
 * @file json_internal_helpers.h
 * @brief Contains internal helpers for interacting with cJSON.
 */
#pragma once

#include <launchdarkly/json.h>
#include <launchdarkly/boolean.h>
#include <launchdarkly/export.h>

/**
 * @brief Sets the provided key in an object to a string value.
 * If the key already exists the original value is deleted.
 * This function clones the given string.
 * @param[in] object Must be of type `LDJSONObject`.
 * @param[in] key The key of the value. Must not be `NULL`.
 * @param[in] value The value to assign.
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDObjectSetString(struct LDJSON *object, const char * key, const char* value);

/**
 * @brief Sets the provided key in an object to a boolean value.
 * If the key already exists the original value is deleted.
 * @param[in] object Must be of type `LDJSONObject`.
 * @param[in] key The key of the value. Must not be `NULL`.
 * @param[in] value The value to assign.
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDObjectSetBool(struct LDJSON *object, const char * key, LDBoolean value);

/**
 * @brief Sets the provided key in an object to a double value.
 * If the key already exists the original value is deleted.
 * @param[in] object Must be of type `LDJSONObject`.
 * @param[in] key The key of the value. Must not be `NULL`.
 * @param[in] number The value to assign.
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDObjectSetNumber(struct LDJSON *object, const char * key, double number);

/**
 * @brief Sets the provided key in an object to a new object, which is returned.
 * If the key already exists the original value is deleted.
 * @param[in] object Must be of type `LDJSONObject`.
 * @param[in] key The key of the object. Must not be `NULL`.
 * @return Pointer to new object on success, `NULL` on failure.
 */
LD_EXPORT(struct LDJSON*)
LDObjectNewChild(struct LDJSON *object, const char *key);

/**
 * @brief Sets the provided key in an object to a reference to another item.
 * This means the referenced object will not be deleted if LDJSONFree is called on the parent object.
 * @param object[in] Must be of type `LDJSONObject`.
 * @param key[in] Key of the item. Must not be `NULL`.
 * @param ref[in] Reference to another item. Must not be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDObjectSetReference(struct LDJSON *object, const char *key, struct LDJSON *ref);

/*!
 * @file ldjson.h
 * @brief Public API Interface for JSON usage
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/* **** Forward Declarations **** */

struct LDJSON;

/** @brief Represents the type of a LaunchDarkly JSON node */
typedef enum {
    /** @brief JSON Null (not JSON undefined) */
    LDNull = 0,
    /** @brief UTF-8 JSON string */
    LDText,
    /** @brief JSON number (Double or Integer) */
    LDNumber,
    /** @brief JSON boolean (True or False) */
    LDBool,
    /** @brief JSON string indexed map */
    LDObject,
    /** @brief JSON integer indexed array */
    LDArray
} LDJSONType;

/***************************************************************************//**
 * @name Constructing values
 * Routines for constructing values
 * @{
 ******************************************************************************/

/**
 * @brief Constructs a JSON node of type `LDJSONNull`.
 * @return NULL on failure.
 */
struct LDJSON *LDNewNull();

/**
 * @brief Constructs a JSON node of type `LDJSONBool`.
 * @param[in] boolean The value to assign the new node
 * @return NULL on failure.
 */
struct LDJSON *LDNewBool(const bool boolean);

/**
 * @brief Constructs a JSON node of type `LDJSONumber`.
 * @param[in] number The value to assign the new node
 * @return NULL on failure.
 */
struct LDJSON *LDNewNumber(const double number);

/**
 * @brief Returns a a new constructed JSON node of type `LDJSONText`.
 * @param[in] text The text to copy and then assign the new node
 * @return NULL on failure.
 */
struct LDJSON *LDNewText(const char *const text);

/**
 * @brief Constructs a JSON node of type `LDJSONObject`.
 * @return NULL on failure.
 */
struct LDJSON *LDNewObject();

/**
 * @brief Constructs a JSON node of type `LDJSONArray`.
 * @return NULL on failure.
 */
struct LDJSON *LDNewArray();

/*@}*/

/***************************************************************************//**
 * @name Setting values
 * Routines for setting values of existing nodes
 * @{
 ******************************************************************************/

bool LDSetNumber(struct LDJSON *const node, const double number);

/*@}*/

/***************************************************************************//**
 * @name Cleanup and Utility
 * Miscellaneous Operations
 * @{
 ******************************************************************************/

/**
 * @brief Frees any allocated JSON structure.
 * @param[in] json May be NULL.
 * @return Void.
 */
void LDJSONFree(struct LDJSON *const json);

/**
 * @brief Duplicates an existing JSON strucutre. This acts as a deep copy.
 * @param[in] json JSON to be duplicated.
 * @return NULL on failure
 */
struct LDJSON *LDJSONDuplicate(const struct LDJSON *const json);

/**
 * @brief Get the type of a JSON structure
 * @param[in] json May be not be NULL (assert)
 * @return The JSON type
 */
LDJSONType LDJSONGetType(const struct LDJSON *const json);

/**
 * @brief Deep compare to check if two JSON structures are equal
 * @param[in] left May be NULL.
 * @param[in] right May be NULL.
 * @return True if equal, false otherwise.
 */
bool LDJSONCompare(const struct LDJSON *const left,
    const struct LDJSON *const right);

/*@}*/

/***************************************************************************//**
 * @name Reading Values
 * Routines for reading values
 * @{
 ******************************************************************************/

/**
 * @brief Get the value from a node of type `LDJSONBool`.
 * @param[in] node Node to read value from. Must be correct type (assert).
 * @return The boolean nodes value
 */
bool LDGetBool(const struct LDJSON *const node);

/**
 * @brief Get the value from a node of type `LDJSONNumber`.
 * @param[in] node Node to read value from. Must be correct type (assert).
 * @return The number nodes value
 */
double LDGetNumber(const struct LDJSON *const node);

/**
 * @brief Get the value from a node of type `LDJSONText`.
 * @param[in] node Node to read value from. Must be correct type (assert).
 * @return The text nodes value. NULL on failure.
 */
const char *LDGetText(const struct LDJSON *const node);

/*@}*/

/***************************************************************************//**
 * @name Iterator Operations
 * Routines for working with collections (Objects, Arrays)
 * @{
 ******************************************************************************/

/**
 * @brief Returns the next item in the sequence
 * @param[in] iter May be not be NULL (assert)
 * @return Item, or NULL if the iterator is finished.
 */
struct LDJSON *LDIterNext(const struct LDJSON *const iter);

/**
 * @brief Allows iteration over an array. Modification of the array invalidates
 * this iterator.
 * @param[in] collection May not be NULL (assert),
 * must be of type `LDJSONArray` or `LDJSONObject` (assert).
 * @return First child iterator, or NULL if empty
 */
struct LDJSON *LDGetIter(const struct LDJSON *const collection);

/**
 * @brief Returns the key associated with the iterator
 * Must be an object iterator.
 * @param[in] iter The iterator obtained from an object.
 * May not be NULL (assert).
 * @return The key on success, or NULL if there is no key (wrong type).
 */
const char *LDIterKey(const struct LDJSON *const iter);

/*@}*/

/***************************************************************************//**
 * @name Array Operations
 * Routines for working with arrays
 * @{
 ******************************************************************************/

/**
 * @brief Return the size of a JSON array
 * @param[in] array May not be NULL (assert),
 * must be of type `LDJSONArray` (assert).
 * @return The size of the array
 */
unsigned int LDCollectionGetSize(const struct LDJSON *const array);

/**
 * @brief Lookup up the value of an index for a given array
 * @param[in] array May not be NULL (assert),
 * must be of type `LDJSONArray` (assert).
 * @param[in] index The index to lookup in the array
 * @return Item if it exists, otherwise NULL
 */
struct LDJSON *LDArrayLookup(const struct LDJSON *const array,
    const unsigned int index);

/**
 * @brief Adds an item to the end of an existing array.
 * @param[in] array Must be of type `LDJSONArray` (assert).
 * @param[in] item The value to append to the array. This item is consumed.
 * @return True on success, False on failure.
 */
bool LDArrayPush(struct LDJSON *const array, struct LDJSON *const item);

/**
 * @brief Appends the array on the right to the array on the left
 * @param[in] prefix Must be of type `LDJSONArray` (assert).
 * @param[in] suffix Must be of type `LDJSONArray` (assert).
 * @return True on success, False on failure.
 */
bool LDArrayAppend(struct LDJSON *const prefix,
    const struct LDJSON *const suffix);

/***************************************************************************//**
 * @name Object Operations
 * Routines for working with objects
 * @{
 ******************************************************************************/

/**
 * @brief Lookup up the value of a key for a given object
 * @param[in] object May not be NULL (assert),
 * must be of type `LDJSONObject` (assert).
 * @param[in] key The key to lookup in the object. May not be NULL (assert),
 * @return The item if it exists, otherwise NULL.
 */
struct LDJSON *LDObjectLookup(const struct LDJSON *const object,
    const char *const key);

/**
 * @brief Sets the provided key in an object to item.
 * If the key already exists the original value is deleted.
 * @param[in] object Must be of type `LDJSONObject` (assert).
 * @param[in] key The key that is being written to in the object.
 * May not be NULL (assert).
 * @param[in] item The value to assign to key. This item is consumed.
 * @return True on success, False on failure.
 */
bool LDObjectSetKey(struct LDJSON *const object, const char *const key,
    struct LDJSON *const item);

/**
 * @brief Delete the provided key from the given object.
 * @param[in] object May not be NULL (assert),
 * must be of type `LDJSONObject` (assert).
 * @param[in] key The key to delete from the object. May not be NULL (assert),
 * @return Void
 */
void LDObjectDeleteKey(struct LDJSON *const object, const char *const key);

/**
 * @brief Detach the provided key from the given object. The returned value is
 * no longer owned by the object and must be manually deleted.
 * @param[in] object May not be NULL (assert),
 * must be of type `LDJSONObject` (assert).
 * @param[in] key The key to detach from the object. May not be NULL (assert),
 * @return The value associated, or NULL if it does not exit
 */
struct LDJSON *LDObjectDetachKey(struct LDJSON *const object,
    const char *const key);

/**
 * @brief Copy keys from one object to another. If a key already exists it is
 * overwritten by the new value.
 * @param[in] to Object to assign to. May not be NULL (assert).
 * @param[in] from Object to copy keys from. May not be NULL (assert).
 * @return True on success, `to` is polluted on failure.
 */
bool LDObjectMerge(struct LDJSON *const to, const struct LDJSON *const from);

 /*@}*/

/***************************************************************************//**
 * @name Serialization / Deserialization
 * Working with textual representations of JSON
 * @{
 ******************************************************************************/

/**
 * @brief Serialize JSON text into a JSON structure.
 * @param[in] json Structure to serialize.
 * May be NULL depending on implementation.
 * @return NULL on failure
 */
char *LDJSONSerialize(const struct LDJSON *const json);

/**
 * @brief Deserialize JSON text into a JSON structure.
 * @param[in] text JSON text to deserialize. May not be NULL (ASSERT).
 * @return JSON structure on success, NULL on failure.
 */
struct LDJSON *LDJSONDeserialize(const char *const text);

/*@}*/

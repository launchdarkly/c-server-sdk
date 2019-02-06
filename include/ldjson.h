/*!
 * @file ldjson.h
 * @brief Public API Interface for JSON usage
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/* **** Forward Declarations **** */

struct LDJSON; struct LDArrayIter; struct LDObjectIter;

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
  * @param[out] result Where to place the node. May not be NULL (assert). On failure this parameter is not mutated.
  * @return True on success, False on failure.
  */
bool LDNewNull(struct LDJSON **const result);

 /**
  * @brief Constructs a JSON node of type `LDJSONBool`.
  * @param[in] boolean The value to assign the new node
  * @param[out] result Where to place the node. May not be NULL (assert). On failure this parameter is not mutated.
  * @return True on success, False on failure.
  */
bool LDNewBool(const bool boolean, struct LDJSON **const result);

/**
 * @brief Constructs a JSON node of type `LDJSONumber`.
 * @param[in] number The value to assign the new node
 * @param[out] result Where to place the node. May not be NULL (assert). On failure this parameter is not mutated.
 * @return True on success, False on failure.
 */
bool LDNewNumber(const double number, struct LDJSON **const result);

/**
 * @brief Returns a a new constructed JSON node of type `LDJSONText`.
 * @param[in] text The text to copy and then assign the new node
 * @return NULL on failure.
 */
bool LDNewText(const char *const text, struct LDJSON **const result);

 /**
  * @brief Constructs a JSON node of type `LDJSONObject`.
  * @param[out] result Where to place the node. May not be NULL (assert). On failure this parameter is not mutated.
  * @return True on success, False on failure.
  */
bool LDNewObject(struct LDJSON **const result);

 /**
  * @brief Constructs a JSON node of type `LDJSONArray`.
  * @param[out] result Where to place the node. May not be NULL (assert). On failure this parameter is not mutated.
  * @return True on success, False on failure.
  */
bool LDNewArray(struct LDJSON **const result);

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
 * @param[in] input JSON to be duplicated.
 * @param[out] output Where to place the node. May not be NULL (assert). On failure this parameter is not mutated.
 * @return True on success, False on failure.
 */
bool LDJSONDuplicate(const struct LDJSON *const input, struct LDJSON **const output);

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
char *LDGetText(const struct LDJSON *const node);

/*@}*/

/***************************************************************************//**
 * @name Array Operations
 * Routines for working with arrays
 * @{
 ******************************************************************************/

/**
 * @brief Lookup up the value of an index for a given array
 * @param[in] array May not be NULL (assert), must be of type `LDJSONArray` (assert).
 * @param[in] index The index to lookup in the array
 * @param[out] result Where to place the iter. May not be NULL (assert). If the key does not exist this parameter is not mutated.
 * @return True item exists, False item does not exist
 */
bool LDArrayLookup(const struct LDJSON *const array, const unsigned int index, struct LDJSON **const result);

/**
 * @brief Allows iteration over an array. Modification of the array invalidates this iterator.
 * @param[in] array May not be NULL (assert), must be of type `LDJSONArray` (assert).
 * @param[out] result Where to place the iter. May not be NULL (assert). On failure this parameter is not mutated.
 * @return True on success, False on failure.
 */
bool LDArrayGetIter(const struct LDJSON *const array, struct LDArrayIter **const result);

/**
 * @brief Returns the latest value and advances the iterator. The value returned is a non owning reference.
 * @param[in] iter May be NULL depending on implementation
 * @param[out] result Where to place the current value. May be NULL to only advance.
 * @return False indicates end of array, result will not be set.
 */
bool LDArrayIterNext(struct LDArrayIter **const iter, struct LDJSON **const result);

/**
 * @brief Returns the value the iterator is currently on without advancing. The value returned is a non owning reference.
 * @param[in] iter May be NULL depending on implementation
 * @param[out] result Where to place the current value. May not be NULL (assert).
 * @return False indicates the end of the array, result will not be set.
 */
bool LDArrayIterValue(const struct LDArrayIter *const iter, struct LDJSON **const result);

/**
 * @brief Frees an iterator provided by `LDArrayGetIter`.
 * @param[in] iter May be NULL.
 * @return Void.
 */
void LDArrayIterFree(struct LDArrayIter *const iter);

/***************************************************************************//**
 * @name Object Operations
 * Routines for working with objects
 * @{
 ******************************************************************************/

/**
 * @brief Lookup up the value of a key for a given object
 * @param[in] object May not be NULL (assert), must be of type `LDJSONObject` (assert).
 * @param[in] key The key to lookup in the object. May not be NULL (assert),
 * @param[out] result Where to place the iter. May not be NULL (assert). If the key does not exist this parameter is not mutated.
 * @return True item exists, False item does not exist
 */
bool LDObjectLookup(const struct LDJSON *const object, const char *const key, struct LDJSON **const result);

 /**
  * @brief Allows iteration over an array. Modification of the array invalidates this iterator.
  * @param[in] object May not be NULL (assert), must be of type `LDJSONObject` (assert).
  * @param[out] result Where to place the iter. May not be NULL (assert). On failure this parameter is not mutated.
  * @return True on success, False on failure.
  */
bool LDObjectGetIter(const struct LDJSON *const object, struct LDObjectIter **const result);

/**
 * @brief Returns the latest value and advances the iterator. The value returned is a non owning reference.
 * @param[in] iter May be NULL depending on implementation
 * @param[out] result Where to place the current value. May be NULL to only advance.
 * @return False indicates end of object, result will not be set.
 */
bool LDObjectIterNext(struct LDObjectIter **const iter, struct LDJSON **const result);

/**
 * @brief Returns the value the iterator is currently on without advancing. The value returned is a non owning reference.
 * @param[in] iter May be NULL depending on implementation
 * @param[out] result Where to place the current value. May not be NULL (assert).
 * @return False indicates the end of the object, result will not be set.
 */
bool LDObjectIterValue(const struct LDObjectIter *const iter, struct LDJSON **const result);

/**
 * @brief Frees an iterator provided by `LDObjectGetIter`.
 * @param[in] iter May be NULL.
 * @return Void.
 */
void LDObjectIterFree(struct LDObjectIter *const iter);

 /*@}*/

/***************************************************************************//**
 * @name Serialization / Deserialization
 * Working with textual representations of JSON
 * @{
 ******************************************************************************/

 /**
  * @brief Serialize JSON text into a JSON structure.
  * @param[in] json Structure to serialize. May be NULL depending on implementation.
  * @return NULL on failure
  */
char *LDJSONSerialize(const struct LDJSON *const json);

/**
 * @brief Deserialize JSON text into a JSON structure.
 * @param[in] text JSON text to deserialize. May not be NULL (ASSERT).
 * @param[out] json May not be NULL (ASSERT). On failure this parameter is not mutated.
 * @return True on success, False on failure
 */
bool LDJSONDeserialize(const char *const text, struct LDJSON **const json);

/*@}*/

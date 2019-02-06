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
    LDNBool,
    /** @brief JSON string indexed map */
    LDObject,
    /** @brief JSON integer indexed array */
    LDArray
} LDJSONType;

/***************************************************************************//**
 * @name LD JSON New
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
 * @name Array Operations
 * Routines for working with arrays
 * @{
 ******************************************************************************/

 /**
  * @brief Allows iteration over an array.
  * @param[in] array May not be NULL (assert), must be of type `LDJSONArray` (assert).
  * @return NULL on failure.
  */
struct LDArrayIter *LDArrayGetIter(const struct LDJSON *const array);

/**
 * @brief Returns the latest value and advances the iterator.
 * @param[in] iter May not be NULL (assert).
 * @return NULL indicates the end of the array..
 */
struct LDJSON *LDArrayIterNext(struct LDArrayIter **const iter);

/**
 * @brief Returns the value the iterator is currently on without advancing.
 * @param[in] iter May not be NULL (assert).
 * @return NULL indicates the end of the array.
 */
struct LDJSON *LDArrayIterValue(const struct LDArrayIter *const iter);

/**
 * @brief Frees an iterator provided by `LDArrayGetIter`.
 * @param[in] iter May be NULL.
 * @return Void.
 */
void LDArrayIterFree(struct LDArrayIter *const iter);

 /*@}*/

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
  * @brief Returns a a new constructed JSON node of type `LDJSONNull`.
  * @return NULL on failure.
  */
struct LDJSON *LDNewNull();

/**
 * @brief Returns a a new constructed JSON node of type `LDJSONBool`.
 * @return NULL on failure.
 */
struct LDJSON *LDNewBool(const bool boolean);

/**
 * @brief Returns a a new constructed JSON node of type `LDJSONNumber`.
 * @return NULL on failure.
 */
struct LDJSON *LDNewNumber(const double number);

/**
 * @brief Returns a a new constructed JSON node of type `LDJSONText`.
 * @return NULL on failure.
 */
struct LDJSON *LDNewText(const char *const text);

/**
 * @brief Returns a a new constructed JSON node of type `LDJSONObject`.
 * @param[in] May not be NULL (assert).
 * @return NULL on failure.
 */
struct LDJSON *LDNewObject();

/**
 * @brief Returns a a new constructed JSON node of type `LDJSONArray`.
 * @return NULL on failure.
 */
struct LDJSON *LDNewArray();

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

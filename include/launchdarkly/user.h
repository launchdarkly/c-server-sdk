/*!
 * @file user.h
 * @brief Public API Interface for User construction
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <launchdarkly/json.h>
#include <launchdarkly/export.h>

/**
 * @brief Allocate a new empty user Object
 * @return `NULL` on failure.
 */
LD_EXPORT(struct LDUser *) LDUserNew(const char *const userkey);

/**
 * @brief Destroy an existing user object
 * @param[in] user The user free, may be `NULL`.
 * @return Void.
 */
LD_EXPORT(void) LDUserFree(struct LDUser *const user);

/**
 * @brief Mark the user as anonymous.
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] anon If the user should be anonymous or not
 * @return True on success, False on failure.
 */
LD_EXPORT(void) LDUserSetAnonymous(struct LDUser *const user, const bool anon);

/**
 * @brief Set the user's IP.
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] ip The user's IP, may be `NULL` (assert).
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetIP(struct LDUser *const user, const char *const ip);

/**
 * @brief Set the user's first name.
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] firstName The user's first name, may be `NULL` (assert).
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetFirstName(struct LDUser *const user,
    const char *const firstName);

/**
 * @brief Set the user's last name
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] lastName The user's last name, may be `NULL` (assert).
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetLastName(struct LDUser *const user,
    const char *const lastName);

/**
 * @brief Set the user's email.
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] email The user's email, may be `NULL` (assert).
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetEmail(struct LDUser *const user,
    const char *const email);

/**
 * @brief Set the user's name.
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] name The user's name, may be `NULL` (assert).
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetName(struct LDUser *const user,
    const char *const name);

/**
 * @brief Set the user's avatar.
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] avatar The user's avatar, may be `NULL` (assert).
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetAvatar(struct LDUser *const user,
    const char *const avatar);

/**
 * @brief Set the user's country
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] country The user's country, may be `NULL` (assert).
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetCountry(struct LDUser *const user,
    const char *const country);

/**
 * @brief Set the user's secondary key.
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] secondary The user's secondary key, may be `NULL` (assert).
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetSecondary(struct LDUser *const user,
    const char *const secondary);

/**
 * @brief Set the user's custom JSON.
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] custom Custom JSON for the user, may be `NULL` assert.
 * @return Void.
 */
LD_EXPORT(void) LDUserSetCustom(struct LDUser *const user,
    struct LDJSON *const custom);

/**
 * @brief Mark an attribute as private.
 * @param[in] user The user to mutate, may not be `NULL` (assert).
 * @param[in] attribute Attribute to mark as private, may not be `NULL`
 * (assert).
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserAddPrivateAttribute(struct LDUser *const user,
    const char *const attribute);

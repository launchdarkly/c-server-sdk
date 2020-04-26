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
 * @struct LDUser
 * @brief An opaque user object
 */
struct LDUser;

/**
 * @brief Allocate a new empty user Object.
 * @param[in] key A string that identifies the user, may not be `NULL`.
 * @return `NULL` on failure.
 */
LD_EXPORT(struct LDUser *) LDUserNew(const char *const key);

/**
 * @brief Destroy an existing user object.
 * @param[in] user The user free. May be `NULL`.
 * @return Void.
 */
LD_EXPORT(void) LDUserFree(struct LDUser *const user);

/**
 * @brief Mark the user as anonymous.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] anon If the user should be anonymous or not.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetAnonymous(struct LDUser *const user, const bool anon);

/**
 * @brief Set the user's IP.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] ip The user's IP. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetIP(struct LDUser *const user, const char *const ip);

/**
 * @brief Set the user's first name.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] firstName The user's first name. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetFirstName(struct LDUser *const user,
    const char *const firstName);

/**
 * @brief Set the user's last name.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] lastName The user's last name. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetLastName(struct LDUser *const user,
    const char *const lastName);

/**
 * @brief Set the user's email.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] email The user's email. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetEmail(struct LDUser *const user,
    const char *const email);

/**
 * @brief Set the user's name.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] name The user's name. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetName(struct LDUser *const user,
    const char *const name);

/**
 * @brief Set the user's avatar.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] avatar The user's avatar. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetAvatar(struct LDUser *const user,
    const char *const avatar);

/**
 * @brief Set the user's country.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] country The user's country. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetCountry(struct LDUser *const user,
    const char *const country);

/**
 * @brief Set the user's secondary key.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] secondary The user's secondary key. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetSecondary(struct LDUser *const user,
    const char *const secondary);

/**
 * @brief Set the user's custom JSON.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] custom Custom JSON for the user. May be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserSetCustom(struct LDUser *const user,
    struct LDJSON *const custom);

/**
 * @brief Mark an attribute as private.
 * @param[in] user The user to mutate. May not be `NULL`.
 * @param[in] attribute Attribute to mark as private. May not be `NULL`.
 * @return True on success, False on failure.
 */
LD_EXPORT(bool) LDUserAddPrivateAttribute(struct LDUser *const user,
    const char *const attribute);

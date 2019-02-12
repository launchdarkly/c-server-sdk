/*!
 * @file lduser.h
 * @brief Public API Interface for User construction
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ldjson.h"

/**
 * @brief Allocate a new empty user Object
 * @return NULL on failure.
 */
struct LDUser *LDUserNew(const char *const userkey);

/**
 * @brief Destroy an existing user object
 * @param[in] user The user free, may be NULL.
 * @return Void.
 */
void LDUserFree(struct LDUser *const user);

/**
 * @brief Mark the user as anonymous.
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @return True on success, False on failure.
 */
void LDUserSetAnonymous(struct LDUser *const user, const bool anon);

/**
 * @brief Set the user's IP.
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @param[in] ip The users IP, may be NULL.
 * @return True on success, False on failure.
 */
bool LDUserSetIP(struct LDUser *const user, const char *const ip);

/**
 * @brief Set the user's first name.
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @param[in] firstName The users first name, may be NULL.
 * @return True on success, False on failure.
 */
bool LDUserSetFirstName(struct LDUser *const user, const char *const firstName);

/**
 * @brief Set the user's last name
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @param[in] lastname The users last name, may be NULL.
 * @return True on success, False on failure.
 */
bool LDUserSetLastName(struct LDUser *const user, const char *const lastName);

/**
 * @brief Set the user's email.
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @param[in] email The users email, may be NULL.
 * @return True on success, False on failure.
 */
bool LDUserSetEmail(struct LDUser *const user, const char *const email);

/**
 * @brief Set the user's name.
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @param[in] name The users name, may be NULL.
 * @return True on success, False on failure.
 */
bool LDUserSetName(struct LDUser *const user, const char *const name);

/**
 * @brief Set the user's avatar.
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @param[in] avatar The users avatar, may be NULL.
 * @return True on success, False on failure.
 */
bool LDUserSetAvatar(struct LDUser *const user, const char *const avatar);

/**
 * @brief Set the user's secondary key.
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @param[in] secondary The users secondary key, may be NULL.
 * @return True on success, False on failure.
 */
bool LDUserSetSecondary(struct LDUser *const user, const char *const secondary);

/**
 * @brief Set the user's custom JSON.
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @param[in] custom Custom JSON for the user, may be NULL.
 * @return Void.
 */
void LDUserSetCustom(struct LDUser *const user, struct LDJSON *const custom);

/**
 * @brief Mark an attribute as private.
 * @param[in] user The user to mutate, may not be NULL (assert).
 * @param[in] attribute Attribute to mark as private, may not be NULL (assert).
 * @return True on success, False on failure.
 */
bool LDUserAddPrivateAttribute(struct LDUser *const user,
    const char *const attribute);

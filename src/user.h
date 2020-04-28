#pragma once

#include <stdbool.h>

#include <launchdarkly/json.h>

struct LDUser {
    char *key;
    bool anonymous;
    char *secondary;
    char *ip;
    char *firstName;
    char *lastName;
    char *email;
    char *name;
    char *avatar;
    char *country;
    struct LDJSON *privateAttributeNames; /* Array of Text */
    struct LDJSON *custom; /* Object, may be NULL */
};

struct LDJSON *LDi_valueOfAttribute(const struct LDUser *const user,
    const char *const attribute);

struct LDJSON *LDUserToJSON(const struct LDConfig *const config,
    const struct LDUser *const lduser, const bool redact);

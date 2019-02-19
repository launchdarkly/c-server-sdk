#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ldjson.h"
#include "ldconfig.h"
#include "lduser.h"
#include "ldlogging.h"
#include "ldclient.h"

/* **** LDVariations **** */

bool LDBoolVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const bool fallback,
    struct LDJSON **const details);

int LDIntVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const int fallback,
    struct LDJSON **const details);

double LDDoubleVariation(struct LDClient *const client,
    struct LDUser *const user, const char *const key, const double fallback,
    struct LDJSON **const details);

char *LDStringVariation(struct LDClient *const client,
    struct LDUser *const user, const char *const key,
    const char* const fallback, struct LDJSON **const details);

struct LDJSON *LDJSONVariation(struct LDClient *const client,
    struct LDUser *const user, const char *const key,
    const struct LDJSON *const fallback,
    struct LDJSON **const details);

/* **** LDUtility **** */

bool LDSetString(char **const target, const char *const value);

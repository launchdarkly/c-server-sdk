#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ldjson.h"
#include "ldconfig.h"

/* **** Forward Declarations **** */

struct LDUser; struct LDStore;

/* **** LDUser **** */

struct LDUser *LDUserNew(const char *const userkey);
void LDUserFree(struct LDUser *const user);

void LDUserSetAnonymous(struct LDUser *const user, const bool anon);
bool LDUserSetIP(struct LDUser *const user, const char *const ip);
bool LDUserSetFirstName(struct LDUser *const user, const char *const firstName);
bool LDUserSetLastName(struct LDUser *const user, const char *const lastName);
bool LDUserSetEmail(struct LDUser *const user, const char *const email);
bool LDUserSetName(struct LDUser *const user, const char *const name);
bool LDUserSetAvatar(struct LDUser *const user, const char *const avatar);
bool LDUserSetSecondary(struct LDUser *const user, const char *const secondary);
void LDUserSetCustom(struct LDUser *const user, struct LDJSON *const custom);
bool LDUserAddPrivateAttribute(struct LDUser *const user,
    const char *const attribute);

/* **** LDClient **** */

struct LDVariationDetails { int placeholder; };

struct LDClient *LDClientInit(struct LDConfig *const config,
    const unsigned int maxwaitmilli);
void LDClientClose(struct LDClient *const client);

bool LDBoolVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const bool fallback,
    struct LDVariationDetails *const details);

int LDIntVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const int fallback,
    struct LDVariationDetails *const details);

double LDDoubleVariation(struct LDClient *const client,
    struct LDUser *const user, const char *const key, const double fallback,
    struct LDVariationDetails *const details);

char *LDStringVariation(struct LDClient *const client,
    struct LDUser *const user, const char *const key,
    const char* const fallback, struct LDVariationDetails *const details);

struct LDJSON *LDJSONVariation(struct LDClient *const client,
    struct LDUser *const user, const char *const key,
    const struct LDJSON *const fallback,
    struct LDVariationDetails *const details);

/* **** LDLogging **** */

typedef enum {
    LD_LOG_FATAL = 0,
    LD_LOG_CRITICAL,
    LD_LOG_ERROR,
    LD_LOG_WARNING,
    LD_LOG_INFO,
    LD_LOG_DEBUG,
    LD_LOG_TRACE
} LDLogLevel;

const char *LDLogLevelToString(const LDLogLevel level);

void LDBasicLogger(const LDLogLevel level, const char *const text);

void LDConfigureGlobalLogger(const LDLogLevel level,
    void (*logger)(const LDLogLevel level, const char *const text));

/* **** LDUtility **** */

bool LDSetString(char **const target, const char *const value);

#pragma once

#include <stdbool.h>
#include <stddef.h>

/* **** Forward Declarations **** */

struct LDConfig; struct LDUser; struct LDNode;

/* **** LDConfig **** */

struct LDConfig *LDConfigNew();
void LDConfigFree(struct LDConfig *const config);

bool LDConfigSetBaseURI(struct LDConfig *const config, const char *const baseURI);
bool LDConfigSetStreamURI(struct LDConfig *const config, const char *const streamURI);
bool LDConfigSetEventsURI(struct LDConfig *const config, const char *const eventsURI);

void LDConfigSetStream(struct LDConfig *const config, const bool stream);
void LDConfigSetSendEvents(struct LDConfig *const config, const bool sendEvents);
void LDConfigSetTimeout(struct LDConfig *const config, const unsigned int milliseconds);
void LDConfigSetFlushInterval(struct LDConfig *const config, const unsigned int milliseconds);
void LDConfigSetPollInterval(struct LDConfig *const config, const unsigned int milliseconds);
void LDConfigSetOffline(struct LDConfig *const config, const bool offline);
void LDConfigSetUseLDD(struct LDConfig *const config, const bool useLDD);
void LDConfigSetAllAttributesPrivate(struct LDConfig *const config, const bool allAttributesPrivate);
void LDConfigSetUserKeysCapacity(struct LDConfig *const config, const unsigned int userKeysCapacity);
void LDConfigSetUserKeysFlushInterval(struct LDConfig *const config, const unsigned int userKeysFlushInterval);
bool LDConfigAddPrivateAttribute(struct LDConfig *const config, const char *const attribute);

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
void LDUserSetCustom(struct LDUser *const user, struct LDNode *const custom);
bool LDUserAddPrivateAttribute(struct LDUser *const user, const char *const attribute);

/* **** LDClient **** */

struct LDVariationDetails { int placeholder; };

struct LDClient *LDClientInit(struct LDConfig *const config, const unsigned int maxwaitmilli);
void LDClientClose(struct LDClient *const client);

bool LDBoolVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const bool fallback, struct LDVariationDetails *const details);

int LDIntVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const int fallback, struct LDVariationDetails *const details);

double LDDoubleVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const double fallback, struct LDVariationDetails *const details);

char *LDStringVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const char* const fallback, struct LDVariationDetails *const details);

struct LDNode *LDJSONVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const struct LDNode *const fallback, struct LDVariationDetails *const details);

/* **** LDNode **** */

typedef enum {
    LDNodeNull = 0,
    LDNodeText,
    LDNodeNumber,
    LDNodeBool,
    LDNodeObject,
    LDNodeArray
} LDNodeType;

struct LDNode *LDNodeNewNull();
struct LDNode *LDNodeNewBool(const bool boolean);
struct LDNode *LDNodeNewNumber(const double number);
struct LDNode *LDNodeNewText(const char *const text);
struct LDNode *LDNodeNewObject();
struct LDNode *LDNodeNewArray();

void LDNodeFree(struct LDNode *const node);

bool LDNodeGetBool(const struct LDNode *const node);
double LDNodeGetNumber(const struct LDNode *const node);
struct LDNode *LDNodeArrayGetIterator(const struct LDNode *const array);
struct LDNode *LDNodeObjectGetIterator(const struct LDNode *const object);
const char *LDNodeGetText(const struct LDNode *const node);
LDNodeType LDNodeGetType(const struct LDNode *const node);

bool LDNodeObjectSetItem(struct LDNode *const object, const char *const key, struct LDNode *const item);
bool LDNodeArrayAppendItem(struct LDNode *const array, struct LDNode *const item);
unsigned int LDNodeArrayIterGetIndex(struct LDNode *const iter);
const char *LDNodeObjectIterGetKey(struct LDNode *const iter);
struct LDNode *LDNodeAdvanceIterator(const struct LDNode *const iter);
struct LDNode *LDNodeArrayLookupIndex(const struct LDNode *const array, const unsigned int index);
struct LDNode *LDNodeObjectLookupKey(const struct LDNode *const object, const char *const key);

char *LDNodeToJSONString(const struct LDNode *const node);
struct LDNode *LDNodeFromJSONString(const char *const serialized);

struct LDNode *LDNodeDeepCopy(const struct LDNode *const node);

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

void LDConfigureGlobalLogger(const LDLogLevel level, void (*logger)(const LDLogLevel level, const char *const text));

/* **** LDUtility **** */

bool LDSetString(char **const target, const char *const value);

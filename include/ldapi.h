#pragma once

#include <stdbool.h>
#include <stddef.h>

/* **** LDConfig **** */

struct LDConfig;

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

/* **** LDUser **** */

struct LDUser;

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

/* **** LDUtility **** */

bool LDSetString(char **const target, const char *const value);

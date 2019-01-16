#include <string.h>
#include <stdlib.h>

#include "cJSON.h"

#include "ldinternal.h"

struct LDUser *
LDUserNew(const char *const key)
{
    struct LDUser *const user = malloc(sizeof(struct LDUser));

    if (!user) {
        return NULL;
    }

    memset(user, 0, sizeof(struct LDUser));

    if (!LDSetString(&user->key, key)){
        LDUserFree(user);

        return NULL;
    }

    user->secondary             = NULL;
    user->ip                    = NULL;
    user->firstName             = NULL;
    user->lastName              = NULL;
    user->email                 = NULL;
    user->name                  = NULL;
    user->privateAttributeNames = NULL;
    user->avatar                = NULL;
    user->custom                = NULL;

    return user;
}

void
LDUserFree(struct LDUser *const user)
{
    if (user) {
        LDHashSetFree(user->privateAttributeNames);

        free(       user->key       );
        free(       user->secondary );
        free(       user->firstName );
        free(       user->lastName  );
        free(       user->email     );
        free(       user->name      );
        free(       user->avatar    );
        LDNodeFree( user->custom    );
        free(       user            );
    }
}

void
LDUserSetAnonymous(struct LDUser *const user, const bool anon)
{
    LD_ASSERT(user);

    user->anonymous = anon;
}

bool
LDUserSetIP(struct LDUser *const user, const char *const ip)
{
    LD_ASSERT(user);

    return LDSetString(&user->ip, ip);
}

bool
LDUserSetFirstName(struct LDUser *const user, const char *const firstName)
{
    LD_ASSERT(user);

    return LDSetString(&user->firstName, firstName);
}

bool
LDUserSetLastName(struct LDUser *const user, const char *const lastName)
{
    LD_ASSERT(user);

    return LDSetString(&user->lastName, lastName);
}

bool
LDUserSetEmail(struct LDUser *const user, const char *const email)
{
    LD_ASSERT(user);

    return LDSetString(&user->email, email);
}

bool
LDUserSetName(struct LDUser *const user, const char *const name)
{
    LD_ASSERT(user);

    return LDSetString(&user->name, name);
}

bool
LDUserSetAvatar(struct LDUser *const user, const char *const avatar)
{
    LD_ASSERT(user);

    return LDSetString(&user->avatar, avatar);
}

bool
LDUserSetSecondary(struct LDUser *const user, const char *const secondary)
{
    LD_ASSERT(user);

    return LDSetString(&user->secondary, secondary);
}

void
LDUserSetCustom(struct LDUser *const user, struct LDNode *const custom)
{
    LD_ASSERT(custom);

    user->custom = custom;
}

bool
LDUserAddPrivateAttribute(struct LDUser *const user, const char *const attribute)
{
    LD_ASSERT(user); LD_ASSERT(attribute);

    return LDHashSetAddKey(&user->privateAttributeNames, attribute);
}

static bool
isPrivateAttr(struct LDClient *const client, struct LDUser *const user, const char *const key)
{
    bool global = false;

    if (client) {
        global = client->config->allAttributesPrivate  ||
            (LDHashSetLookup(client->config->privateAttributeNames, key) != NULL);
    }

    return global || (LDHashSetLookup(user->privateAttributeNames, key) != NULL);
}

static void
addHidden(cJSON **ref, const char *const value){
    if (!(*ref)) { *ref = cJSON_CreateArray(); }
    cJSON_AddItemToArray(*ref, cJSON_CreateString(value));
}

cJSON *
LDUserToJSON(struct LDClient *const client, struct LDUser *const lduser, const bool redact)
{
    cJSON *hidden = NULL; cJSON *const json = cJSON_CreateObject();

    if (!json) {
        return NULL;
    }

    cJSON_AddStringToObject(json, "key", lduser->key);

    if (lduser->anonymous) {
        cJSON_AddBoolToObject(json, "anonymous", lduser->anonymous);
    }

    #define addstring(field)                                                   \
        if (lduser->field) {                                                   \
            if (redact && isPrivateAttr(client, lduser, #field)) {             \
                addHidden(&hidden, #field);                                    \
            }                                                                  \
            else {                                                             \
                cJSON_AddStringToObject(json, #field, lduser->field);          \
            }                                                                  \
        }                                                                      \

    addstring(secondary);
    addstring(ip);
    addstring(firstName);
    addstring(lastName);
    addstring(email);
    addstring(name);
    addstring(avatar);

    if (lduser->custom) {
        cJSON *const custom = LDNodeToJSON(lduser->custom);
        if (redact && cJSON_IsObject(custom)) {
            cJSON *item = NULL;
            for (item = custom->child; item;) {
                cJSON *const next = item->next; /* must record next to make delete safe */
                if (isPrivateAttr(client, lduser, item->string)) {
                    cJSON *const current = cJSON_DetachItemFromObjectCaseSensitive(custom, item->string);
                    addHidden(&hidden, item->string);
                    cJSON_Delete(current);
                }
                item = next;
            }
        }
        cJSON_AddItemToObject(json, "custom", custom);
    }

    if (hidden) {
        cJSON_AddItemToObject(json, "privateAttrs", hidden);
    }

    return json;

    #undef addstring
}

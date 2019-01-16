#include <string.h>
#include <stdlib.h>

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

    utarray_new(user->privateAttributeNames, &ut_str_icd);

    user->secondary = NULL;
    user->ip        = NULL;
    user->firstName = NULL;
    user->lastName  = NULL;
    user->email     = NULL;
    user->name      = NULL;
    user->avatar    = NULL;
    user->custom    = NULL;

    return user;
}

void
LDUserFree(struct LDUser *const user)
{
    if (user) {
        if (user->privateAttributeNames) {
            utarray_free(user->privateAttributeNames);
        }

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

void
LDUserAddPrivateAttribute(struct LDUser *const user, const char *const attribute)
{
    LD_ASSERT(user); LD_ASSERT(attribute);

    utarray_push_back(user->privateAttributeNames, &attribute);
}

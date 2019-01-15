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

    user->secondary = NULL;
    user->ip        = NULL;
    user->firstName = NULL;
    user->lastName  = NULL;
    user->email     = NULL;
    user->name      = NULL;
    user->avatar    = NULL;

    return user;
}

void
LDUserFree(struct LDUser *const user)
{
    if (user) {
        free( user->key       );
        free( user->secondary );
        free( user->firstName );
        free( user->lastName  );
        free( user->email     );
        free( user->name      );
        free( user->avatar    );
        free( user            );
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

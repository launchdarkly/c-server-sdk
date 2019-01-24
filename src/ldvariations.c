#include "ldinternal.h"

bool
LDBoolVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const bool fallback, struct LDVariationDetails *const details)
{
    LD_ASSERT(client); LD_ASSERT(user); LD_ASSERT(key);

    LD_ASSERT(details); /* place holder */

    return fallback;
}

int
LDIntVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const int fallback, struct LDVariationDetails *const details)
{
    LD_ASSERT(client); LD_ASSERT(user); LD_ASSERT(key);

    LD_ASSERT(details); /* place holder */

    return fallback;
}

double
LDDoubleVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const double fallback, struct LDVariationDetails *const details)
{
    LD_ASSERT(client); LD_ASSERT(user); LD_ASSERT(key);

    LD_ASSERT(details); /* place holder */

    return fallback;
}

char *
LDStringVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const char* const fallback, struct LDVariationDetails *const details)
{
    LD_ASSERT(client); LD_ASSERT(user); LD_ASSERT(key);

    LD_ASSERT(details); /* place holder */

    return strdup(fallback);
}

struct LDNode *
LDJSONVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const struct LDNode *const fallback, struct LDVariationDetails *const details)
{
    LD_ASSERT(client); LD_ASSERT(user); LD_ASSERT(key);

    LD_ASSERT(details); /* place holder */

    return LDNodeDeepCopy(fallback);
}
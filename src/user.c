#include <launchdarkly/user.h>

#include "assertion.h"
#include "user.h"

struct LDUser *
LDUserNew(const char *const key)
{
    LD_ASSERT_API(key);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (key == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDUserNew NULL key");

            return NULL;
        }
    #endif

    return LDi_userNew(key);
}

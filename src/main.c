#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "ldapi.h"

#define YOUR_SDK_KEY "<put your SDK key here>"
#define YOUR_FEATURE_KEY "<put your feature key here>"

int
main()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct LDUser *user;

    setbuf(stdout, NULL);

    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    config = LDConfigNew(YOUR_SDK_KEY);
    assert(config);

    client = LDClientInit(config, 0);
    assert(client);

    user = LDUserNew("abc");
    assert(user);

    while (true) {
        const bool flag = LDBoolVariation(
            client, user, YOUR_FEATURE_KEY, false, NULL);

        if (flag) {
            LD_LOG(LD_LOG_INFO, "feature flag is true");
        } else {
            LD_LOG(LD_LOG_INFO, "feature flag is false");
        }

        sleep(1);
    }

    return 0;
}

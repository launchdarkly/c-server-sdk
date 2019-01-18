#include <stdio.h>
#include <stdlib.h>

#include "ldinternal.h"

int
main()
{
    struct LDConfig *config; struct LDClient *client;

    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    config = LDConfigNew("****");

    LD_ASSERT(LDConfigSetBaseURI(config, "https://ld-stg.launchdarkly.com"));

    client = LDClientInit(config, 0);

    if (client) {
        LDi_log(LD_LOG_INFO, "LDClientInit Success");

        getchar();

        LDClientClose(client);
    } else {
        LDi_log(LD_LOG_INFO, "LDClientInit Failed\n");
    }

    return 0;
}

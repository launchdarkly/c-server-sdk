#include <string.h>
#include <stdlib.h>

#include "ldinternal.h"

struct LDClient *
LDClientInit(struct LDConfig *const config, const unsigned int maxwaitmilli)
{
    struct LDClient *client = NULL;

    LD_ASSERT(config);

    if (!(client = malloc(sizeof(struct LDClient)))) {
        return NULL;
    }

    memset(client, 0, sizeof(struct LDClient));

    client->shuttingdown = false;
    client->config       = config;

    if (!LDi_networkinit(client)) {
        free(client);

        return NULL;
    }

    if (!LDi_rwlockinit(&client->lock)) {
        free(client);

        return NULL;
    }

    if (!LDi_createthread(&client->thread, LDi_networkthread, client)) {
        free(client);

        return NULL;
    }

    if (maxwaitmilli) {} /* Temp NO-OP*/

    return client;
}

void
LDClientClose(struct LDClient *const client)
{
    if (client) {
        /* signal shutdown to background */
        LD_ASSERT(LDi_wrlock(&client->lock));
        client->shuttingdown = true;
        LD_ASSERT(LDi_wrunlock(&client->lock));

        /* wait until background exits */
        LD_ASSERT(LDi_jointhread(client->thread));

        /* cleanup resources */
        LD_ASSERT(LDi_rwlockdestroy(&client->lock));
        LDConfigFree(client->config);

        free(client);
    }
}

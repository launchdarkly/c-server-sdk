#include <string.h>
#include <stdlib.h>

#include "ldinternal.h"

struct LDClient *LDClientInit(struct LDConfig *const config, const unsigned int maxwaitmilli)
{
    struct LDClient *const client = malloc(sizeof(struct LDClient));

    if (!client) {
        return NULL;
    }

    memset(client, 0, sizeof(struct LDClient));

    client->config = config;

    if (maxwaitmilli) {} /* Temp NO-OP*/

    return client;
}

void LDClientClose(struct LDClient *const client)
{
    if (client) {
        LDConfigFree(client->config);
        free(client);
    }
}

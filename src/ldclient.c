#include <string.h>
#include <stdlib.h>

#include "ldinternal.h"
#include "ldstore.h"

struct LDClient *
LDClientInit(struct LDConfig *const config, const unsigned int maxwaitmilli)
{
    struct LDClient *client = NULL;

    LD_ASSERT(config);

    if (!(client = malloc(sizeof(struct LDClient)))) {
        return NULL;
    }

    memset(client, 0, sizeof(struct LDClient));

    if (config->store) {
        config->defaultStore = false;
    } else {
        if (!(config->store = makeInMemoryStore())) {
            free(client);

            return NULL;
        }

        config->defaultStore = true;
    }

    if (!config->store->initialized(config->store->context)) {
        struct LDVersionedSet *sets = NULL;

        const char *namespace = NULL;
        struct LDVersionedSet *set = NULL;

        if (!(set = malloc(sizeof(struct LDVersionedSet)))) {
            goto error;
        }

        set->kind = getSegmentKind();
        set->elements = NULL;
        namespace = set->kind.getNamespace();

        HASH_ADD_KEYPTR(hh, sets, namespace, strlen(namespace), set);

        if (!(set = malloc(sizeof(struct LDVersionedSet)))) {
            goto error;
        }

        set->kind = getFlagKind();
        set->elements = NULL;
        namespace = set->kind.getNamespace();

        HASH_ADD_KEYPTR(hh, sets, namespace, strlen(namespace), set);

        if (!config->store->init(config->store->context, sets)) {
            goto error;
        }
    }

    client->shuttingdown = false;
    client->config       = config;

    if (!LDi_networkinit(client)) {
        goto error;
    }

    if (!LDi_rwlockinit(&client->lock)) {
        goto error;
    }

    if (!LDi_createthread(&client->thread, LDi_networkthread, client)) {
        goto error;
    }

    if (maxwaitmilli) {} /* Temp NO-OP */

    return client;

  error:
    if (config->defaultStore && config->store) {
        freeStore(config->store);
    }

    free(client);

    return NULL;
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

        if (client->config->defaultStore) {
            freeStore(client->config->store);
        }

        LDConfigFree(client->config);

        free(client);
    }
}

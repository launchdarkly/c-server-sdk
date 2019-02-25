#include <string.h>
#include <stdio.h>

#include "ldnetwork.h"
#include "ldinternal.h"

struct StreamContext {
    int placeholder;
};

static void
done(struct LDClient *const client, void *const rawcontext)
{
    struct StreamContext *const context = rawcontext;

    LD_ASSERT(client);
    LD_ASSERT(context);
}

static void
destroy(void *const rawcontext)
{
    struct StreamContext *const context = rawcontext;

    LD_ASSERT(context);

    LD_LOG(LD_LOG_INFO, "streaming destroyed");

    free(context);
}

static CURL *
poll(struct LDClient *const client, void *const rawcontext)
{
    struct StreamContext *const context = rawcontext;

    LD_ASSERT(client);
    LD_ASSERT(context);

    return NULL;
}

struct NetworkInterface *
constructStreaming(struct LDClient *const client)
{
    struct NetworkInterface *interface = NULL;
    struct StreamContext *context = NULL;

    LD_ASSERT(client);

    if (!(interface = malloc(sizeof(struct NetworkInterface)))) {
        goto error;
    }

    if (!(context = malloc(sizeof(struct StreamContext)))) {
        goto error;
    }

    interface->done    = done;
    interface->poll    = poll;
    interface->context = context;
    interface->destroy = destroy;
    interface->current = NULL;

    return interface;

  error:
    free(context);
    free(interface);
    return NULL;
}

#include "ldinternal.h"

#include "curl/curl.h"

THREAD_RETURN
LDi_networkthread(void* const clientref)
{
    struct LDClient *const client = clientref;

    LD_ASSERT(client);

    while (true) {
        LDi_rdlock(&client->lock);
        if (client->shuttingdown) {
            LDi_rdunlock(&client->lock);

            break;
        }
        LDi_rdunlock(&client->lock);
    }

    return THREAD_RETURN_DEFAULT;
}

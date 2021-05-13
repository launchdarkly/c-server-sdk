#include "assertion.h"

#include "test-utils/client.h"

struct LDClient *
makeTestClient()
{
    struct LDClient *client;
    struct LDConfig *config;

    LD_ASSERT(config = LDConfigNew("key"));
    LD_ASSERT(client = LDClientInit(config, 0));

    return client;
}

struct LDClient *
makeOfflineClient()
{
    struct LDConfig *config;
    struct LDClient *client;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LDConfigSetOffline(config, LDBooleanTrue);

    LD_ASSERT(client = LDClientInit(config, 0));

    return client;
}

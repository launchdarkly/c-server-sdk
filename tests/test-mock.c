#include <stdio.h>
#include <string.h>

#include <launchdarkly/api.h>
#include <launchdarkly/json.h>

/* http_server must be imported before concurrency.h because of windows.h */
#include "test-utils/http_server.h"
#include "test-utils/flags.h"

#include "assertion.h"
#include "concurrency.h"
#include "network.h"

static ld_socket_t acceptFD;
static int acceptPort;

static void
testBasicPoll_sendResponse(ld_socket_t fd)
{
    char *serialized;
    struct LDJSON *payload, *flags, *segments, *flag, *value;

    LD_ASSERT(payload = LDNewObject());
    LD_ASSERT(flags = LDNewObject());
    LD_ASSERT(segments = LDNewObject());

    LD_ASSERT(flag = makeMinimalFlag("flag1", 52, true, false));
    LD_ASSERT(value = LDNewBool(true));
    addVariation(flag, value);
    setFallthrough(flag, 0);

    LD_ASSERT(LDObjectSetKey(flags, "flag1", flag));

    LD_ASSERT(LDObjectSetKey(payload, "flags", flags));
    LD_ASSERT(LDObjectSetKey(payload, "segments", segments))

    LD_ASSERT(serialized = LDJSONSerialize(payload));

    LDi_send200(fd, serialized);

    LDJSONFree(payload);
    LDFree(serialized);
}

static THREAD_RETURN
testBasicPoll_thread(void *const unused)
{
    struct LDHTTPRequest request;

    LD_ASSERT(unused == NULL);

    LDHTTPRequestInit(&request);

    LDi_readHTTPRequest(acceptFD, &request);

    LD_ASSERT(strcmp("/sdk/latest-all", request.requestURL) == 0);
    LD_ASSERT(strcmp("GET", request.requestMethod) == 0);
    LD_ASSERT(request.requestBody == NULL);

    LD_ASSERT(strcmp(
        "key",
        LDGetText(LDObjectLookup(request.requestHeaders, "Authorization"))
    ) == 0);

    LD_ASSERT(strcmp(
        "CServerClient/" LD_SDK_VERSION,
        LDGetText(LDObjectLookup(request.requestHeaders, "User-Agent"))
    ) == 0);

    testBasicPoll_sendResponse(request.requestSocket);

    LDHTTPRequestDestroy(&request);

    return THREAD_RETURN_DEFAULT;
}

static void
testBasicPoll()
{
    ld_thread_t thread;
    struct LDConfig *config;
    struct LDClient *client;
    struct LDUser *user;
    char pollURL[1024];

    LDi_listenOnRandomPort(&acceptFD, &acceptPort);
    LDi_thread_create(&thread, testBasicPoll_thread, NULL);

    snprintf(pollURL, 1024, "http://127.0.0.1:%d", acceptPort);

    LD_ASSERT(config = LDConfigNew("key"));
    LDConfigSetStream(config, false);
    LDConfigSetBaseURI(config, pollURL);

    LD_ASSERT(client = LDClientInit(config, 1000));
    LD_ASSERT(user = LDUserNew("my-user"));

    LD_ASSERT(LDBoolVariation(client, user, "flag1", false, NULL));

    LDUserFree(user);
    LDClientClose(client);
    LDi_closeSocket(acceptFD);
    LDi_thread_join(&thread);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testBasicPoll();

    return 0;
}

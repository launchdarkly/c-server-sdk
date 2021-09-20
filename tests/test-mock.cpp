#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <stdio.h>
#include <string.h>

#include <launchdarkly/api.h>
#include <launchdarkly/json.h>

/* http_server must be imported before concurrency.h because of windows.h */
#include "test-utils/flags.h"
#include "test-utils/http_server.h"

#include "assertion.h"
#include "concurrency.h"
#include "network.h"
}

static ld_socket_t acceptFD;
static int acceptPort;

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class MockFixture : public CommonFixture,
                    public testing::WithParamInterface<int> {
};

static struct LDJSON *
makeBasicPutBody() {
    struct LDJSON *payload, *flags, *segments, *flag, *value;

    LD_ASSERT(payload = LDNewObject());
    LD_ASSERT(flags = LDNewObject());
    LD_ASSERT(segments = LDNewObject());

    LD_ASSERT(
            flag = makeMinimalFlag("flag1", 52, LDBooleanTrue, LDBooleanFalse));
    LD_ASSERT(value = LDNewBool(LDBooleanTrue));
    addVariation(flag, value);
    setFallthrough(flag, 0);

    LD_ASSERT(LDObjectSetKey(flags, "flag1", flag));

    LD_ASSERT(LDObjectSetKey(payload, "flags", flags));
    LD_ASSERT(LDObjectSetKey(payload, "segments", segments));

    return payload;
}

static struct LDJSON *
makeBasicStreamPutBody() {
    struct LDJSON *payload, *payloadData, *payloadPath;

    LD_ASSERT(payload = LDNewObject());
    LD_ASSERT(payloadData = makeBasicPutBody());
    LD_ASSERT(payloadPath = LDNewText("/"));

    LDObjectSetKey(payload, "data", payloadData);
    LDObjectSetKey(payload, "path", payloadPath);

    return payload;
}

static void
testBasicPoll_sendResponse(ld_socket_t fd) {
    char *serialized;
    struct LDJSON *payload;

    LD_ASSERT(payload = makeBasicPutBody());

    LD_ASSERT(serialized = LDJSONSerialize(payload));

    LDi_send200(fd, serialized);

    LDJSONFree(payload);
    LDFree(serialized);
}

static THREAD_RETURN
testBasicPoll_thread(void *const unused) {
    struct LDHTTPRequest request;

    LD_ASSERT(unused == NULL);

    LDHTTPRequestInit(&request);

    LDi_readHTTPRequest(acceptFD, &request);

    LD_ASSERT(strcmp("/sdk/latest-all", request.requestURL) == 0);
    LD_ASSERT(strcmp("GET", request.requestMethod) == 0);
    LD_ASSERT(request.requestBody == NULL);

    LD_ASSERT(
            strcmp(
                    "key",
                    LDGetText(
                            LDObjectLookup(request.requestHeaders, "Authorization"))) == 0);

    LD_ASSERT(
            strcmp(
                    "CServerClient/" LD_SDK_VERSION,
            LDGetText(LDObjectLookup(request.requestHeaders, "User-Agent"))) ==
            0);

    testBasicPoll_sendResponse(request.requestSocket);

    LDHTTPRequestDestroy(&request);

    return THREAD_RETURN_DEFAULT;
}

TEST_F(MockFixture, BasicPoll) {
    ld_thread_t thread;
    struct LDConfig *config;
    struct LDClient *client;
    struct LDUser *user;
    char pollURL[1024];

    LDi_listenOnRandomPort(&acceptFD, &acceptPort);
    LDi_thread_create(&thread, testBasicPoll_thread, NULL);

    ASSERT_GE(snprintf(pollURL, 1024, "http://127.0.0.1:%d", acceptPort), 0);

    ASSERT_TRUE(config = LDConfigNew("key"));
    LDConfigSetStream(config, LDBooleanFalse);
    LDConfigSetBaseURI(config, pollURL);

    ASSERT_TRUE(client = LDClientInit(config, 1000 * 10));
    ASSERT_TRUE(user = LDUserNew("my-user"));

    ASSERT_TRUE(LDBoolVariation(client, user, "flag1", LDBooleanFalse, NULL));

    LDUserFree(user);
    LDClientClose(client);
    LDi_closeSocket(acceptFD);
    LDi_thread_join(&thread);
}

static void
testBasicStream_sendResponse(ld_socket_t fd) {
    char *putBodySerialized;
    struct LDJSON *putBody;
    char payload[1024];

    LD_ASSERT(putBody = makeBasicStreamPutBody());

    LD_ASSERT(putBodySerialized = LDJSONSerialize(putBody));

    LD_ASSERT(
            snprintf(payload, 1024, "event: put\ndata: %s\n\n", putBodySerialized) >
            0);

    LDi_send200(fd, payload);

    LDJSONFree(putBody);
    LDFree(putBodySerialized);
}

static THREAD_RETURN
testBasicStream_thread(void *const unused) {
    struct LDHTTPRequest request;

    LD_ASSERT(unused == NULL);

    LDHTTPRequestInit(&request);

    LDi_readHTTPRequest(acceptFD, &request);

    LD_ASSERT(strcmp("/all", request.requestURL) == 0);
    LD_ASSERT(strcmp("GET", request.requestMethod) == 0);
    LD_ASSERT(request.requestBody == NULL);

    LD_ASSERT(
            strcmp(
                    "key",
                    LDGetText(
                            LDObjectLookup(request.requestHeaders, "Authorization"))) == 0);

    LD_ASSERT(
            strcmp(
                    "CServerClient/" LD_SDK_VERSION,
            LDGetText(LDObjectLookup(request.requestHeaders, "User-Agent"))) ==
            0);

    LD_ASSERT(
            strcmp(
                    "text/event-stream",
                    LDGetText(LDObjectLookup(request.requestHeaders, "Accept"))) == 0);

    testBasicStream_sendResponse(request.requestSocket);

    LDHTTPRequestDestroy(&request);

    return THREAD_RETURN_DEFAULT;
}

TEST_F(MockFixture, BasicStream) {
    ld_thread_t thread;
    struct LDConfig *config;
    struct LDClient *client;
    struct LDUser *user;
    char streamURL[1024];

    LDi_listenOnRandomPort(&acceptFD, &acceptPort);
    LDi_thread_create(&thread, testBasicStream_thread, NULL);

    ASSERT_GE(snprintf(streamURL, 1024, "http://127.0.0.1:%d", acceptPort), 0);

    ASSERT_TRUE(config = LDConfigNew("key"));
    LDConfigSetStreamURI(config, streamURL);

    ASSERT_TRUE(client = LDClientInit(config, 1000 * 10));
    ASSERT_TRUE(user = LDUserNew("my-user"));

    ASSERT_TRUE(LDBoolVariation(client, user, "flag1", LDBooleanFalse, NULL));

    LDUserFree(user);
    LDClientClose(client);
    LDi_closeSocket(acceptFD);
    LDi_thread_join(&thread);
}

static THREAD_RETURN
testWrapperHeader_thread(void *const headerCase) {
    struct LDHTTPRequest request;


    LDHTTPRequestInit(&request);

    LDi_readHTTPRequest(acceptFD, &request);

    switch (*((int *) headerCase)) {
        case 0:
            LD_ASSERT(
                    strcmp(
                            "abc/123",
                            LDGetText(LDObjectLookup(
                                    request.requestHeaders, "X-LaunchDarkly-Wrapper"))) == 0);
            break;
        case 1:
            LD_ASSERT(
                    strcmp(
                            "xyz",
                            LDGetText(LDObjectLookup(
                                    request.requestHeaders, "X-LaunchDarkly-Wrapper"))) == 0);
            break;
        case 2:
            LD_ASSERT(
                    LDObjectLookup(request.requestHeaders, "X-LaunchDarkly-Wrapper") ==
                    NULL);
            break;
        default:
            LD_ASSERT(LDBooleanFalse);
            break;
    }

    testBasicStream_sendResponse(request.requestSocket);

    LDHTTPRequestDestroy(&request);

    return THREAD_RETURN_DEFAULT;
}


TEST_P(MockFixture, WrapperHeader) {

    ld_thread_t thread;
    struct LDConfig *config;
    struct LDClient *client;
    char streamURL[1024];

    int wrapperHeaderCase = GetParam();

    LDi_listenOnRandomPort(&acceptFD, &acceptPort);
    LDi_thread_create(&thread, testWrapperHeader_thread, &wrapperHeaderCase);

    ASSERT_GE(snprintf(streamURL, 1024, "http://127.0.0.1:%d", acceptPort), 0);

    ASSERT_TRUE(config = LDConfigNew("key"));


    switch (wrapperHeaderCase) {
        case 0:
            ASSERT_TRUE(LDConfigSetWrapperInfo(config, "abc", "123"));
            break;
        case 1:
            ASSERT_TRUE(LDConfigSetWrapperInfo(config, "xyz", NULL));
            break;
        case 2:
            /* do nothing */
            break;
        default:
            ASSERT_TRUE(LDBooleanFalse);
            break;
    }

    LDConfigSetStreamURI(config, streamURL);

    ASSERT_TRUE(client = LDClientInit(config, 1000 * 10));

    LDClientClose(client);
    LDi_closeSocket(acceptFD);
    LDi_thread_join(&thread);
}

INSTANTIATE_TEST_SUITE_P(
        WrapperHeaderTests,
        MockFixture,
        ::testing::Values(
                0, 1, 2
        ));

static THREAD_RETURN
testBasicFlush_thread(void *const unused) {
    struct LDHTTPRequest request;
    struct LDJSON *got, *expected, *tmp;

    LD_ASSERT(unused == NULL);

    LDHTTPRequestInit(&request);

    LDi_readHTTPRequest(acceptFD, &request);

    LD_ASSERT(strcmp("/bulk", request.requestURL) == 0);
    LD_ASSERT(strcmp("POST", request.requestMethod) == 0);

    LD_ASSERT(
            strcmp(
                    "key",
                    LDGetText(
                            LDObjectLookup(request.requestHeaders, "Authorization"))) == 0);

    LD_ASSERT(
            strcmp(
                    "CServerClient/" LD_SDK_VERSION,
            LDGetText(LDObjectLookup(request.requestHeaders, "User-Agent"))) ==
            0);

    LD_ASSERT(request.requestBody != NULL);

    LD_ASSERT(got = LDJSONDeserialize(request.requestBody));
    LD_ASSERT(
            expected = LDJSONDeserialize(
                    "[{\"kind\":\"identify\","
                    "\"key\":\"my-user\",\"user\":{\"key\":\"my-user\"}}]"));

    LDObjectDeleteKey(LDArrayLookup(got, 0), "creationDate");

    LD_ASSERT(LDJSONCompare(got, expected));

    LDi_send200(request.requestSocket, NULL);

    LDHTTPRequestDestroy(&request);

    LDJSONFree(got);
    LDJSONFree(expected);
    return THREAD_RETURN_DEFAULT;
}

TEST_F(MockFixture, BasicFlush) {
    ld_thread_t thread;
    struct LDConfig *config;
    struct LDClient *client;
    struct LDUser *user;
    char pollURL[1024];

    LDi_listenOnRandomPort(&acceptFD, &acceptPort);
    LDi_thread_create(&thread, testBasicFlush_thread, NULL);

    LD_ASSERT(snprintf(pollURL, 1024, "http://127.0.0.1:%d", acceptPort) > 0);

    LD_ASSERT(config = LDConfigNew("key"));
    LD_ASSERT(LDConfigSetStreamURI(config, "http://192.0.2.0"));
    LD_ASSERT(LDConfigSetEventsURI(config, pollURL));

    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("my-user"));

    LD_ASSERT(LDClientIdentify(client, user));
    LDClientFlush(client);

    LDi_thread_join(&thread);
    LDi_closeSocket(acceptFD);

    LDUserFree(user);
    LDClientClose(client);
}

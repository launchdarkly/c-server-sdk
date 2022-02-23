#include "gtest/gtest.h"
#include "concurrencyfixture.h"

extern "C" {

#include <launchdarkly/api.h>
#include "client.h"

#include "test-utils/client.h"
}

// This test is intended to demonstrate that LDClientFlush may be safely called by concurrent threads without
// causing a data-race. This is not meant to be a rigorous test.
TEST_F(ConcurrencyFixture, TestClientFlush) {
    struct LDClient *client;

    ASSERT_TRUE(client = makeTestClient());

    Defer([client](){ LDClientClose(client); });

    const std::size_t THREAD_CONCURRENCY = 100;

    RunMany(THREAD_CONCURRENCY, [=]() {
        Sleep();
        LDClientFlush(client);
    });
}

#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <string.h>
#include <time.h>

#include <launchdarkly/api.h>

#include "client.h"
#include "config.h"
#include "event_processor_internal.h"
#include "events.h"
#include "store.h"
#include "user.h"
#include "utility.h"

#include "test-utils/client.h"
#include "test-utils/flags.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class EventsFixture : public CommonFixture {
};

TEST_F(EventsFixture, ParseHTTPDate) {
    struct tm tm;
    const char *const header = "Fri, 29 Mar 2019 17:55:35 GMT";
    EXPECT_TRUE(LDi_parseRFC822(header, &tm));
}

TEST_F(EventsFixture, ParseServerTimeHeaderActual) {
    struct LDClient *client;
    const char *const header = "Date: Fri, 29 Mar 2019 17:55:35 GMT\r\n";
    const size_t total = strlen(header);

    EXPECT_TRUE(client = makeOfflineClient());

    ASSERT_EQ(LDi_onHeader(header, total, 1, client), total);
    ASSERT_GE(client->eventProcessor->lastServerTime, 1553880000000);
    ASSERT_LE(client->eventProcessor->lastServerTime, 1553911000000);

    LDClientClose(client);
}

TEST_F(EventsFixture, ParseServerTimeHeaderAlt) {
    struct LDClient *client;
    const char *const header = "date:Fri, 29 Mar 2019 17:55:35 GMT   \r\n";
    const size_t total = strlen(header);

    EXPECT_TRUE(client = makeOfflineClient());

    ASSERT_EQ(LDi_onHeader(header, total, 1, client), total);
    ASSERT_GE(client->eventProcessor->lastServerTime, 1553880000000);
    ASSERT_LE(client->eventProcessor->lastServerTime, 1553911000000);

    LDClientClose(client);
}

TEST_F(EventsFixture, ParseServerTimeHeaderBad) {
    struct LDClient *client;
    const char *const header1 = "Date: not a valid date\r\n";
    const size_t total1 = strlen(header1);
    const char *const header2 = "Date:\r\n";
    const size_t total2 = strlen(header2);

    EXPECT_TRUE(client = makeOfflineClient());

    ASSERT_EQ(LDi_onHeader(header1, total1, 1, client), total1);
    ASSERT_EQ(client->eventProcessor->lastServerTime, 0);

    ASSERT_EQ(LDi_onHeader(header2, total2, 1, client), total2);
    ASSERT_EQ(client->eventProcessor->lastServerTime, 0);

    LDClientClose(client);
}

#include <string.h>
#include <time.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "config.h"
#include "event_processor_internal.h"
#include "events.h"
#include "store.h"
#include "user.h"
#include "utility.h"

#include "test-utils/client.h"
#include "test-utils/flags.h"

static void
testParseHTTPDate()
{
    struct tm         tm;
    const char *const header = "Fri, 29 Mar 2019 17:55:35 GMT";
    LD_ASSERT(LDi_parseRFC822(header, &tm));
}

static void
testParseServerTimeHeaderActual()
{
    struct LDClient * client;
    const char *const header = "Date: Fri, 29 Mar 2019 17:55:35 GMT\r\n";
    const size_t      total  = strlen(header);

    LD_ASSERT(client = makeOfflineClient());

    LD_ASSERT(LDi_onHeader(header, total, 1, client) == total);
    LD_ASSERT(client->eventProcessor->lastServerTime >= 1553880000000);
    LD_ASSERT(client->eventProcessor->lastServerTime <= 1553911000000);

    LDClientClose(client);
}

static void
testParseServerTimeHeaderAlt()
{
    struct LDClient * client;
    const char *const header = "date:Fri, 29 Mar 2019 17:55:35 GMT   \r\n";
    const size_t      total  = strlen(header);

    LD_ASSERT(client = makeOfflineClient());

    LD_ASSERT(LDi_onHeader(header, total, 1, client) == total);
    LD_ASSERT(client->eventProcessor->lastServerTime >= 1553880000000);
    LD_ASSERT(client->eventProcessor->lastServerTime <= 1553911000000);

    LDClientClose(client);
}

static void
testParseServerTimeHeaderBad()
{
    struct LDClient * client;
    const char *const header1 = "Date: not a valid date\r\n";
    const size_t      total1  = strlen(header1);
    const char *const header2 = "Date:\r\n";
    const size_t      total2  = strlen(header2);

    LD_ASSERT(client = makeOfflineClient());

    LD_ASSERT(LDi_onHeader(header1, total1, 1, client) == total1);
    LD_ASSERT(client->eventProcessor->lastServerTime == 0);

    LD_ASSERT(LDi_onHeader(header2, total2, 1, client) == total2);
    LD_ASSERT(client->eventProcessor->lastServerTime == 0);

    LDClientClose(client);
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    testParseHTTPDate();
    testParseServerTimeHeaderActual();
    testParseServerTimeHeaderAlt();
    testParseServerTimeHeaderBad();

    LDBasicLoggerThreadSafeShutdown();

    return 0;
}

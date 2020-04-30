#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "events.h"
#include "network.h"
#include "user.h"
#include "client.h"
#include "config.h"
#include "utility.h"
#include "lru.h"

bool
LDi_notNull(const struct LDJSON *const json)
{
    if (json) {
        if (LDJSONGetType(json) != LDNull) {
            return true;
        }
    }

    return false;
}

struct AnalyticsContext {
    bool active;
    double lastFlush;
    struct curl_slist *headers;
    struct LDClient *client;
    char *buffer;
    unsigned int failureTime;
    char payloadId[LD_UUID_SIZE + 1];
};

static void
resetMemory(struct AnalyticsContext *const context)
{
    LD_ASSERT(context);

    curl_slist_free_all(context->headers);
    context->headers = NULL;

    LDFree(context->buffer);
    context->buffer = NULL;

    context->failureTime = 0;
}

static void
done(struct LDClient *const client, void *const rawcontext,
    const int responseCode)
{
    struct AnalyticsContext *context;
    const bool success = responseCode == 200 || responseCode == 202;

    LD_ASSERT(client);
    LD_ASSERT(rawcontext);

    context = (struct AnalyticsContext *)rawcontext;

    context->active = false;

    LD_LOG(LD_LOG_TRACE, "events network interface called done");

    if (success) {
        LD_LOG(LD_LOG_TRACE, "event batch send successful");

        LDi_rwlock_wrlock(&client->lock);
        client->shouldFlush = false;
        LDi_rwlock_wrunlock(&client->lock);

        LDi_getMonotonicMilliseconds(&context->lastFlush);

        resetMemory(context);
    } else {
        double now;

        /* failed twice so just discard the payload */
        if (context->failureTime) {
            LD_LOG(LD_LOG_ERROR, "failed sending events twice, discarding");

            resetMemory(context);
        } else {
            LD_LOG(LD_LOG_WARNING, "failed sending events, retrying");
            /* setup waiting and retry logic */
            LDi_getMonotonicMilliseconds(&now);

            context->failureTime = now;

            curl_slist_free_all(context->headers);
            context->headers = NULL;
        }
    }
}

static void
destroy(void *const rawcontext)
{
    struct AnalyticsContext *context;

    LD_ASSERT(rawcontext);

    context = (struct AnalyticsContext *)rawcontext;

    LD_LOG(LD_LOG_INFO, "analytics destroyed");

    resetMemory(context);

    LDFree(context);
}

static const char *
strnchr(const char *str, const char c, size_t len)
{
    for (; str && len; len--, str++) {
        if (*str == c) {
            return str;
        }
    }

    return NULL;
}

bool
LDi_parseRFC822(const char *const date, struct tm *tm)
{
    return strptime(date, "%a, %d %b %Y %H:%M:%S %Z", tm) != NULL;
}

/* curl spec says these may not be NULL terminated */
size_t
LDi_onHeader(const char *buffer, const size_t size,
    const size_t itemcount, void *const context)
{
    struct LDClient *client;
    const size_t total = size * itemcount;
    const char *const dateheader = "Date:";
    const size_t dateheaderlen = strlen(dateheader);
    struct tm tm;
    char datebuffer[128];
    const char *headerend;

    LD_ASSERT(context);

    client = context;

    memset(&tm, 0, sizeof(struct tm));

    /* ensures we do not segfault if not terminated */
    if (!(headerend = strnchr(buffer, '\r', total))) {
        LD_LOG(LD_LOG_ERROR, "failed to find end of header");

        return total;
    }

    /* guard segfault for very short headers */
    if (total <= dateheaderlen) {
        return total;
    }

    /* check if header is the date header */
    if (LDi_strncasecmp(buffer, dateheader, dateheaderlen) != 0) {
        return total;
    }

    buffer += dateheaderlen;

    /* skip any spaces or tabs after header type */
    while (*buffer == ' ' || *buffer == '\t') {
        buffer++;
    }

    /* copy just date segment into own buffer */
    if ((size_t)(headerend - buffer + 1) > sizeof(datebuffer)) {
        LD_LOG(LD_LOG_ERROR, "not enough room to parse date");

        return total;
    }
    strncpy(datebuffer, buffer, headerend - buffer);
    datebuffer[headerend - buffer] = 0;

    if (!LDi_parseRFC822(datebuffer, &tm)) {
        LD_LOG(LD_LOG_ERROR, "failed to extract date from server");

        return total;
    }

    LDi_setServerTime(
        client->eventProcessor,
        1000.0 * (double)mktime(&tm)
    );

    return total;

}

static CURL *
poll(struct LDClient *const client, void *const rawcontext)
{
    CURL *curl;
    struct AnalyticsContext *context;
    char url[4096];
    const char *mime, *schema;
    bool shouldFlush;
    bool lastFailed;

    LD_ASSERT(rawcontext);

    curl        = NULL;
    shouldFlush = false;
    mime        = "Content-Type: application/json";
    schema      = "X-LaunchDarkly-Event-Schema: 3";
    context     = (struct AnalyticsContext *)rawcontext;
    lastFailed  = context->failureTime != 0;

    /* decide if events should be sent */

    if (context->active) {
        return NULL;
    }

    if (context->failureTime) {
        double now;

        LDi_getMonotonicMilliseconds(&now);

        /* wait for one second before retrying send */
        if (now <= context->failureTime + 1000) {
            return NULL;
        }
    }

    if (!lastFailed) {
        struct LDJSON *events;

        events = NULL;

        LDi_rwlock_rdlock(&client->lock);
        shouldFlush = client->shouldFlush;
        LDi_rwlock_rdunlock(&client->lock);

        if (!shouldFlush) {
            double now;

            LDi_getMonotonicMilliseconds(&now);
            LD_ASSERT(now >= context->lastFlush);

            if (now - context->lastFlush <
                client->config->flushInterval)
            {
                return NULL;
            }
        }

        if (!LDi_bundleEventPayload(client->eventProcessor, &events)) {
            LD_LOG(LD_LOG_ERROR, "failed bundling events");

            return NULL;
        }

        if (!events) {
            /* no events to send */
            LDi_rwlock_wrlock(&client->lock);
            shouldFlush = false;
            LDi_rwlock_wrunlock(&client->lock);

            return NULL;
        }

        if (!(context->buffer = LDJSONSerialize(events))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            return NULL;
        }

        /* Only generate a UUID once per payload. We want the header to remain
        the same during a retry */
        context->payloadId[LD_UUID_SIZE] = 0;

        if (!LDi_UUIDv4(context->payloadId)) {
            LD_LOG(LD_LOG_ERROR, "failed to generate payload identifier");

            goto error;
        }
    }

    /* prepare request */

    if (snprintf(url, sizeof(url), "%s/bulk",
        client->config->eventsURI) < 0)
    {
        LD_LOG(LD_LOG_CRITICAL, "snprintf URL failed");

        return NULL;
    }

    LD_LOG_1(LD_LOG_INFO, "connection to analytics url: %s", url);

    if (!LDi_prepareShared(client->config, url, &curl, &context->headers)) {
        goto error;
    }

    if (!(context->headers = curl_slist_append(context->headers, mime))) {
        goto error;
    }

    if (!(context->headers = curl_slist_append(context->headers, schema))) {
        goto error;
    }

    {
        int status;
        /* This is done as a macro so that the string is a literal */
        #define LD_PAYLOAD_ID_HEADER "X-LaunchDarkly-Payload-ID: "

        /* do not need to add space for null termination because of sizeof */
        char payloadIdHeader[sizeof(LD_PAYLOAD_ID_HEADER) + LD_UUID_SIZE];

        status = snprintf(payloadIdHeader, sizeof(payloadIdHeader),
            "%s%s", LD_PAYLOAD_ID_HEADER, context->payloadId);
        LD_ASSERT(status == sizeof(payloadIdHeader) - 1);

        #undef LD_PAYLOAD_ID_HEADER

        if (!(context->headers = curl_slist_append(context->headers,
            payloadIdHeader)))
        {
            goto error;
        }
    }

    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, context->headers)
        != CURLE_OK)
    {
        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, LDi_onHeader)
        != CURLE_OK)
    {
        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HEADERDATA, client)
        != CURLE_OK)
    {
        goto error;
    }

    /* add outgoing buffer */

    if (curl_easy_setopt(curl, CURLOPT_POSTFIELDS, context->buffer)
        != CURLE_OK)
    {
        goto error;
    }

    context->active = true;

    return curl;

  error:
    curl_slist_free_all(context->headers);

    curl_easy_cleanup(curl);

    return NULL;
}

struct NetworkInterface *
LDi_constructAnalytics(struct LDClient *const client)
{
    struct NetworkInterface *netInterface;
    struct AnalyticsContext *context;

    LD_ASSERT(client);

    netInterface = NULL;
    context      = NULL;

    if (!(netInterface =
        (struct NetworkInterface *)LDAlloc(sizeof(struct NetworkInterface))))
    {
        goto error;
    }

    if (!(context =
        (struct AnalyticsContext *)LDAlloc(sizeof(struct AnalyticsContext))))
    {
        goto error;
    }

    context->active      = false;
    context->headers     = NULL;
    context->client      = client;
    context->buffer      = NULL;
    context->failureTime = 0;

    LDi_getMonotonicMilliseconds(&context->lastFlush);

    netInterface->done     = done;
    netInterface->poll     = poll;
    netInterface->context  = context;
    netInterface->destroy  = destroy;
    netInterface->current  = NULL;

    return netInterface;

  error:
    LDFree(context);

    LDFree(netInterface);

    return NULL;
}

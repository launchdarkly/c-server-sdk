#include "ldjsonrc.h"
#include "assertion.h"
#include "concurrency.h"
#include "launchdarkly/api.h"

struct LDJSONRC
{
    struct LDJSON *value;
    ld_mutex_t lock;
    unsigned int count;

    /* An RC item may contain references to other  RC items.
     * When it does, then those are tracked as associated items. */
    struct LDJSONRC **associated;
    unsigned int associatedCount;
};

static void LDJSONRCRetainAssociated(struct LDJSONRC *const rc);
static void LDJSONRCReleaseAssociated(struct LDJSONRC *const rc);

struct LDJSONRC *
LDJSONRCNew(struct LDJSON *const json)
{
    struct LDJSONRC *result;

    LD_ASSERT(json);

    if (!(result = (struct LDJSONRC *)LDAlloc(sizeof(struct LDJSONRC)))) {
        return NULL;
    }


    result->value = json;
    result->associated = NULL;
    result->associatedCount = 0;
    result->count = 1;

    LDi_mutex_init(&result->lock);

    return result;
}

void
LDJSONRCAssociate(struct LDJSONRC *const rc, struct LDJSONRC **const associates, unsigned int associateCount) {
    LD_ASSERT(rc);

    LDi_mutex_lock(&rc->lock);

    /* Remove any existing associations. Decrease reference counts to those associations. */
    if(rc->associated) {
        LDJSONRCReleaseAssociated(rc);

        LDFree(rc->associated);
        rc->associated = NULL;
        rc->associatedCount = 0;
    }

    rc->associated = associates;
    rc->associatedCount = associateCount;

    /* For the duration of the association, all the items have an additional reference. */
    LDJSONRCRetainAssociated(rc);

    LDi_mutex_unlock(&rc->lock);
}

void
LDJSONRCRetain(struct LDJSONRC *const rc)
{
    LD_ASSERT(rc);

    LDi_mutex_lock(&rc->lock);
    rc->count++;

    LDJSONRCRetainAssociated(rc);

    LDi_mutex_unlock(&rc->lock);
}

static void
destroyJSONRC(struct LDJSONRC *const rc)
{
    if (rc) {
        LDJSONFree(rc->value);
        LDi_mutex_destroy(&rc->lock);
        if(rc->associated) {
            LDFree(rc->associated);
        }
        LDFree(rc);
    }
}

void
LDJSONRCRelease(struct LDJSONRC *const rc)
{
    if (rc) {
        LDi_mutex_lock(&rc->lock);
        LD_ASSERT(rc->count > 0);
        rc->count--;

        LDJSONRCReleaseAssociated(rc);

        if (rc->count == 0) {
            LDi_mutex_unlock(&rc->lock);

            destroyJSONRC(rc);
        } else {
            LDi_mutex_unlock(&rc->lock);
        }
    }
}

struct LDJSON *
LDJSONRCGet(struct LDJSONRC *const rc)
{
    LD_ASSERT(rc);

    return rc->value;
}

/**
 * Increment associated reference counters.
 * Parent RC should be locked when calling this method.
 * @param rc
 */
static void
LDJSONRCRetainAssociated(struct LDJSONRC *const rc) {
    unsigned int associatedIndex = 0;
    struct LDJSONRC *associatedItem = NULL;

    if(rc->associated) {
        for(associatedIndex = 0; associatedIndex < rc->associatedCount; ++associatedIndex) {
            associatedItem = rc->associated[associatedIndex];
            LDJSONRCRetain(associatedItem);
        }
    }
}

/**
 * Decrement associated reference counters.
 * Parent RC should be locked when calling this method.
 * @param rc
 */
static void
LDJSONRCReleaseAssociated(struct LDJSONRC *const rc) {
    unsigned int associatedIndex = 0;
    struct LDJSONRC *associatedItem = NULL;

    if(rc->associated) {
        for(associatedIndex = 0; associatedIndex < rc->associatedCount; ++associatedIndex) {
            associatedItem = rc->associated[associatedIndex];
            LDJSONRCRelease(associatedItem);
        }
    }
}

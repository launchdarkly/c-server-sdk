#pragma once

#include "launchdarkly/api.h"

/**
 * Structure and associated methods for implementing a reference counting system.
 * When an RC item is created it has 1 reference. References are added by retaining
 * and removed by retaining. When the count hits 0 the items resources are released.
 */
struct LDJSONRC;

/**
 * Create a new reference counted JSON object.
 * @param json The JSON to take ownership of.
 * @return
 */
struct LDJSONRC *
LDJSONRCNew(struct LDJSON *const json);

/**
 * Retain a reference to the LDJSONRC.
 * @param rc
 */
void
LDJSONRCRetain(struct LDJSONRC *const rc);

/**
 * Release a reference to the LDJSONRC.
 * If there are no more references, then the item will be deleted.
 * @param rc
 */
void
LDJSONRCRelease(struct LDJSONRC *const rc);

/**
 * Get the LDJSON associated with the LDJSONRC.
 * @param rc
 * @return The associated LDJSON.
 */
struct LDJSON *
LDJSONRCGet(struct LDJSONRC *const rc);

/**
 * Associate a series of LDJSONRC items with the given LDJSONRC item.
 * The LDJSONRC item will release/retain the associated items with the primary items.
 * The associated items may have additional references being retained.
 * @param rc The LDJSONRC to associate the items with.
 * @param associates The LDJSONRC items to associate.
 * @param associateCount The count of items.
 */
void
LDJSONRCAssociate(struct LDJSONRC *const rc, struct LDJSONRC **const associates, unsigned int associateCount);


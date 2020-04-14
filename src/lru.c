#include <launchdarkly/memory.h>

#include "assertion.h"
#include "lru.h"
#include "utility.h"

#include "utlist.h"
#include "uthash.h"

struct LDLRUNode {
    char *key;
    struct LDLRUNode *next, *prev;
    UT_hash_handle hh;
};

struct LDLRU {
    unsigned int elements;
    unsigned int capacity;
    struct LDLRUNode *list;
    struct LDLRUNode *hash;
};

struct LDLRU *
LDLRUInit(const unsigned int capacity)
{
    struct LDLRU *const lru = LDAlloc(sizeof(struct LDLRU));

    if (lru == NULL) {
        return NULL;
    }

    lru->elements = 0;
    lru->capacity = capacity;
    lru->list = NULL;
    lru->hash = NULL;

    return lru;
}

void
LDLRUFree(struct LDLRU *const lru)
{
    if (lru) {
        LDLRUClear(lru);

        LDFree(lru);
    }
}

enum LDLRUStatus
LDLRUInsert(struct LDLRU *const lru, const char *const key)
{
    struct LDLRUNode *existing = NULL;

    LD_ASSERT(lru);
    LD_ASSERT(key);

    if (lru->capacity == 0) {
        return LDLRUSTATUS_NEW;
    }

    HASH_FIND_STR(lru->hash, key, existing);

    if (existing == NULL) {
        struct LDLRUNode *node;
        char *keyDuplicate;

        if (!(keyDuplicate = LDStrDup(key))) {
            return LDLRUSTATUS_ERROR;
        }

        if (lru->elements == lru->capacity) {
            node = lru->list->prev;

            HASH_DEL(lru->hash, node);
            CDL_DELETE(lru->list, node);

            LDFree(node->key);
        } else {
            node = LDAlloc(sizeof(struct LDLRUNode));

            if (node == NULL) {
                return LDLRUSTATUS_ERROR;
            }

            lru->elements++;
        }

        memset(node, 0, sizeof(struct LDLRUNode));

        node->key = keyDuplicate;

        CDL_PREPEND(lru->list, node);

        HASH_ADD_KEYPTR(hh, lru->hash, node->key, strlen(node->key), node);

        return LDLRUSTATUS_NEW;
    } else {
        CDL_DELETE(lru->list, existing);
        CDL_PREPEND(lru->list, existing);

        return LDLRUSTATUS_EXISTED;
    }
}

void
LDLRUClear(struct LDLRU *const lru)
{
    struct LDLRUNode *iter, *tmp1, *tmp2;

    LD_ASSERT(lru);

    iter = NULL;
    tmp1 = NULL;
    tmp2 = NULL;

    HASH_ITER(hh, lru->hash, iter, tmp1) {
        HASH_DEL(lru->hash, iter);

        LDFree(iter->key);
    }

    CDL_FOREACH_SAFE(lru->list, iter, tmp1, tmp2) {
        CDL_DELETE(lru->list, iter);

        LDFree(iter);
    }

    lru->elements = 0;
    lru->list     = NULL;
    lru->hash     = NULL;
}

#include <string.h>
#include <stdlib.h>

#include "ldinternal.h"

bool
LDSetString(char **const target, const char *const value)
{
    if (value) {
        char *const tmp = strdup(value);

        if (tmp) {
            free(*target); *target = tmp; return true;
        } else {
            return false;
        }
    } else {
        free(*target); *target = NULL; return true;
    }
}

bool
LDHashSetAddKey(struct LDHashSet **const set, const char *const key)
{
    struct LDHashSet *node = NULL;

    LD_ASSERT(key);

    node = malloc(sizeof(struct LDHashSet));

    if (!node) {
        return false;
    }

    memset(node, 0, sizeof(struct LDHashSet));

    node->key = strdup(key);

    if (!node->key) {
        free(node);

        return false;
    }

    HASH_ADD_KEYPTR(hh, *set, node->key, strlen(node->key), node);

    return true;
}

void
LDHashSetFree(struct LDHashSet *set)
{
    struct LDHashSet *item = NULL, *tmp = NULL;

    HASH_ITER(hh, set, item, tmp) {
        HASH_DEL(set, item);

        free(item->key);
    }
}

struct LDHashSet *
LDHashSetLookup(const struct LDHashSet *const set, const char *const key)
{
    struct LDHashSet *result = NULL;

    if (set) {
        HASH_FIND_STR(set, key, result);
    }

    return result;
}

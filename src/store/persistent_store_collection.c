#include "string.h"

#include "assertion.h"
#include "persistent_store_collection.h"
#include "store_utilities.h"

void
LDi_makeKindCollection(const char* kind, struct LDJSON *const items, struct LDStoreCollectionState* collection) {
    struct LDJSON *setItem = NULL;
    struct LDStoreCollectionStateItem *itemIter = NULL;
    unsigned int allocationSize;

    LD_ASSERT(kind);
    LD_ASSERT(items);
    LD_ASSERT(collection);

    allocationSize = sizeof(struct LDStoreCollectionStateItem) *
                LDCollectionGetSize(items);

    collection->items = (struct LDStoreCollectionStateItem *)LDAlloc(
            allocationSize);
    memset(collection->items, 0, allocationSize);
    itemIter = collection->items;

    for (setItem = LDGetIter(items); setItem;
         setItem = LDIterNext(setItem)) {
        char *serialized;

        serialized = NULL;

        if (!LDi_validateData(setItem)) {
            LD_LOG(
                    LD_LOG_ERROR,
                    "LDStoreInit failed to validate feature");

            itemIter++;
            continue;
        }

        serialized = LDJSONSerialize(setItem);

        itemIter->key = LDi_getDataKey(setItem);

        itemIter->item.buffer     = (void *)serialized;
        itemIter->item.bufferSize = strlen(serialized);
        itemIter->item.version    = LDi_getDataVersion(setItem);
    }

    collection->kind = kind;
}

void
LDi_makeCollections(struct LDJSON *const sets, struct LDStoreCollectionState **collections, unsigned int *count) {
    struct LDStoreCollectionState *collectionsIter = NULL;
    struct LDJSON *set = NULL;
    unsigned int collectionSize;
    unsigned int allocationSize;

    LD_ASSERT(sets);
    LD_ASSERT(collections);
    LD_ASSERT(count);

    collectionSize = LDCollectionGetSize(sets);
    allocationSize = collectionSize * sizeof (struct  LDStoreCollectionState);

    *count = collectionSize;

    *collections = LDAlloc(allocationSize);
    LD_ASSERT(*collections);
    memset(*collections, 0, allocationSize);

    collectionsIter = *collections;

    for (set = LDGetIter(sets); set; set = LDIterNext(set)) {
        LDi_makeKindCollection(LDIterKey(set), set, collectionsIter);
        collectionsIter++;
    }
}

void
LDi_freeCollections(struct LDStoreCollectionState *collections, unsigned int count) {
    if (collections != NULL) {
        unsigned int collectionIndex;

        for (collectionIndex = 0; collectionIndex < count; collectionIndex++) {
            struct LDStoreCollectionState *collection;
            unsigned int itemIndex;

            collection = &(collections[collectionIndex]);

            for (itemIndex = 0; itemIndex < collection->itemCount; itemIndex++) {
                struct LDStoreCollectionStateItem *item;

                item = &(collection->items[itemIndex]);
                LDFree(item->item.buffer);
            }

            LDFree(collection->items);
        }

        LDFree(collections);
    }
}

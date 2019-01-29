#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ldschema.h"

/* **** Forward Declarations **** */

struct LDVersionedData; struct LDVersionedDataKind; struct LDVersionedSet; struct LDFeatureStore;

/* **** Kind Interfaces **** */

struct LDVersionedDataKind getFlagKind();
struct LDVersionedDataKind getSegmentKind();

struct LDVersionedData *segmentToVersioned(struct Segment *const segment);
struct LDVersionedData *flagToVersioned(struct FeatureFlag *const flag);

/* **** Store Interfaces **** */

/* hh key from data->getKey */
struct LDVersionedData {
    void *data;
    bool (*isDeleted)(const void *const object);
    unsigned int (*getVersion)(const void *const object);
    const char *(*getKey)(const void *const object);
    char *(*serialize)(const void *const object);
    void (*destructor)(void *const object);
    UT_hash_handle hh;
};

struct LDVersionedDataKind {
    const char *(*getNamespace)();
    struct LDVersionedData *(*parse)(const char *const text);
    struct LDVersionedData *(*makeDeletedItem)(const char *const key, const unsigned int version);
};

/* hh key from kind->getNamespace() */
struct LDVersionedSet {
    struct LDVersionedDataKind kind;
    struct LDVersionedData *elements;
    UT_hash_handle hh;
};

struct LDFeatureStore {
    bool (*init)(void *const context, struct LDVersionedSet *const sets);
    struct LDVersionedData *(*get)(void *const context, const char *const key, const struct LDVersionedDataKind kind);
    /* must be first method of interface called after get */
    void (*finalizeGet)(void *const context, struct LDVersionedData *const value);
    struct LDVersionedData *(*all)(void *const context, const struct LDVersionedDataKind kind);
    /* must be first method of interface called after all */
    void (*finalizeAll)(void *const context, struct LDVersionedData *const all);
    bool (*delete)(void *const context, const struct LDVersionedDataKind kind, const char *const key, const unsigned int version);
    bool (*upsert)(void *const context, const struct LDVersionedDataKind kind, struct LDVersionedData *data);
    bool (*initialized)(void *const context);
    void (*destructor)(void *const context);
    void (*finishAction)();
    void *context;
};

void freeStore(struct LDFeatureStore *const store);

/* **** Memory Concrete Instance **** */

struct LDFeatureStore *makeInMemoryStore();

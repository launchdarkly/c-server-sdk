#pragma once

#include <stdbool.h>
#include <stddef.h>

/* hh key from data->getKey */
struct LDVersionedData {
    void *data;
    bool (*isDeleted)(void *const object);
    unsigned int (*getVersion)(const void *const object);
    const char *(*getKey)(const void *const object);
    char *(*serialize)(const void *const object);
    void (*destructor)(void *const object);
    struct LDVersionedData *(*deepCopy)(void *const object);
    UT_hash_handle hh;
};

struct LDVersionedDataKind {
    const char *(*getNamespace)();
    struct LDVersionedData *(*parse)(const char *const text);
    struct LDVersionedData *(*makeDeletedItem)(const char *const key, const unsigned int version);
};

/* hh key from kind->getNamespace() */
struct LDVersionedSet {
    struct LDVersionedDataKind* kind;
    struct LDVersionedData *elements;
    UT_hash_handle hh;
};

struct LDFeatureStore {
    bool (*init)(void *const context, struct LDVersionedSet *const sets);
    struct LDVersionedData *(*get)(void *const context, const char *const key, const struct LDVersionedDataKind *const kind);
    struct LDVersionedData *(*all)(void *const context, const struct LDVersionedDataKind *const kind);
    bool (*delete)(void *const context, const struct LDVersionedDataKind *const kind, const char *const key, const unsigned int version);
    bool (*upsert)(void *const context, const struct LDVersionedDataKind *const kind, struct LDVersionedData *data);
    bool (*initialized)(void *const context);
    void (*destructor)(void *const context);
    void *context;
};

struct LDFeatureStore *makeInMemoryStore();
void freeInMemoryStore(struct LDFeatureStore *const store);

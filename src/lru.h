#pragma once

enum LDLRUStatus {
    LDLRUSTATUS_ERROR,
    LDLRUSTATUS_EXISTED,
    LDLRUSTATUS_NEW
};

struct LDLRU;

struct LDLRU *LDLRUInit(const unsigned int capacity);

void LDLRUFree(struct LDLRU *const lru);

enum LDLRUStatus LDLRUInsert(struct LDLRU *const lru, const char *const key);

void LDLRUClear(struct LDLRU *const lru);

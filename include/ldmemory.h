/*!
 * @file ldmemory.h
 * @brief Public API. Operations for managing memory.
 */

#pragma once

void *LDAlloc(const size_t bytes);
void LDFree(void *const buffer);
char *LDStrDup(const char *const string);
void *LDRealloc(void *const buffer, const size_t bytes);
void *LDCalloc(const size_t nmemb, const size_t size);
char *LDStrNDup(const char *const str, const size_t n);

void LDSetMemoryRoutines(void *(*const newMalloc)(const size_t),
    void (*const newFree)(void *const),
    void *(*const newRealloc)(void *const, const size_t),
    char *(*const newStrDup)(const char *const),
    void *(*const newCalloc)(const size_t, const size_t),
    char *(*const newStrNDup)(const char *const, const size_t));

void LDGlobalInit();

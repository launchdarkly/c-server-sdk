/*!
 * @file ldmemory.h
 * @brief Public API. Operations for managing memory.
 */

#pragma once

void *LDAlloc(const size_t bytes);
void LDFree(void *const buffer);
char *LDStrDup(const char *const string);
void *LDRealloc(void *const buffer, const size_t bytes);

void LDSetAlloc(void *(*f)(const size_t));
void LDSetFree(void (*f)(void *const));
void LDSetStrDup(char *(*f)(const char *const));
void LDSetRealloc(void *(*f)(void *const, const size_t));

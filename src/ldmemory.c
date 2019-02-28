#include <stdlib.h>
#include <string.h>

#include "ldmemory.h"

static void *(*customAlloc)(const size_t bytes) = malloc;

void *
LDAlloc(const size_t bytes)
{
    return customAlloc(bytes);
}

void
LDSetAlloc(void *(*f)(const size_t))
{
    customAlloc = f;
}

static void (*customFree)(void *const buffer) = free;

void
LDFree(void *const buffer)
{
    customFree(buffer);
}

void
LDSetFree(void (*f)(void *const))
{
    customFree = f;
}

static char *(*customStrDup)(const char *const string) = strdup;

char *
LDStrDup(const char *const string)
{
    return customStrDup(string);
}

void
LDSetStrDup(char *(*f)(const char *const))
{
    customStrDup = f;
}

static void *(*customRealloc)(void *const buffer, const size_t bytes) = realloc;

void *
LDRealloc(void *const buffer, const size_t bytes)
{
    return customRealloc(buffer, bytes);
}

void
LDSetRealloc(void *(*f)(void *const, const size_t))
{
    customRealloc = f;
}

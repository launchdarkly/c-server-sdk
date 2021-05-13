/*!
 * @file ldevents.h
 * @brief Internal API Interface for Evaluation
 */

#pragma once

#include <stddef.h>
#include <time.h>

#include <launchdarkly/variations.h>

size_t
LDi_onHeader(
    const char * buffer,
    const size_t size,
    const size_t itemcount,
    void *const  context);

LDBoolean
LDi_parseRFC822(const char *const date, struct tm *tm);

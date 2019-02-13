/*!
 * @file ldlogging.h
 * @brief Public API Interface for Logging.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    LD_LOG_FATAL = 0,
    LD_LOG_CRITICAL,
    LD_LOG_ERROR,
    LD_LOG_WARNING,
    LD_LOG_INFO,
    LD_LOG_DEBUG,
    LD_LOG_TRACE
} LDLogLevel;

void LDBasicLogger(const LDLogLevel level, const char *const text);

void LDConfigureGlobalLogger(const LDLogLevel level,
    void (*logger)(const LDLogLevel level, const char *const text));

const char *LDLogLevelToString(const LDLogLevel level);

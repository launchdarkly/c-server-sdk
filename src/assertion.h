/*!
 * @file assertion.h
 * @brief Internal Assertion Definitions.
 */

#pragma once

#include "logging.h"

#ifdef LAUNCHDARKLY_USE_ASSERT
    #define LD_ASSERT(condition) \
        if (!(condition)) { \
            LD_LOG(LD_LOG_FATAL, "LD_ASSERT failed: " #condition " aborting"); \
            abort(); \
        }

    #define LD_ASSERT_API(condition) LD_ASSERT(condition)
#else
    #define LD_ASSERT(condition)
    #define LD_ASSERT_API(condition)
#endif

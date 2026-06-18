#pragma once

#include "defines.h"

#ifdef ZEN_DEBUG

// Defined in logger.cpp
void logger_report_assertion_failure(const char* expression,
                                     const char* message, const char* file,
                                     int line);

#if _MSC_VER
#include <intrin.h>
#define zen_debug_break() __debugbreak()
#else
#define zen_debug_break() __builtin_trap()
#endif

#define ZEN_ASSERT(expr)                                                       \
    {                                                                          \
        if (expr) {                                                            \
        } else {                                                               \
            logger_report_assertion_failure(#expr, "", __FILE__, __LINE__);    \
            zen_debug_break();                                                 \
        }                                                                      \
    }

#define ZEN_ASSERT_MESSAGE(expr, message)                                      \
    {                                                                          \
        if (expr) {                                                            \
        } else {                                                               \
            logger_report_assertion_failure(#expr, message, __FILE__,          \
                                            __LINE__);                         \
            zen_debug_break();                                                 \
        }                                                                      \
    }

#else

#define ZEN_ASSERT(expr)
#define ZEN_ASSERT_MESSAGE(expr, message)

#endif

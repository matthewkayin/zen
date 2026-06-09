#pragma once

#include <stdbool.h>

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    #define ZEN_PLATFORM_WINDOWS 1
    #ifndef _WIN64
        #error "64-bit is required"
    #endif
#elif defined(__linux__) || defined(__gnu_linux__)
    #define ZEN_PLATFORM_LINUX 1
#elif __APPLE__
    #define ZEN_PLATFORM_MACOS 1
#endif

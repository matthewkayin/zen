#pragma once

#include <stdbool.h>

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    #define ZEN_PLATFORM_WINDOWS 1
    #ifndef _WIN64
        #error "64-bit is required"
    #endif
    #define ZEN_PLATFORM_STR "Windows"
#elif defined(__linux__) || defined(__gnu_linux__)
    #define ZEN_PLATFORM_LINUX 1
    #define ZEN_PLATFORM_STR "Linux"
#elif __APPLE__
    #define ZEN_PLATFORM_MACOS 1
    #define ZEN_PLATFORM_STR "MacOS"
#endif

// Debug configuration
#if _DEBUG
    #define ZEN_DEBUG 1
    #define ZEN_RELEASE 0
    #define ZEN_BUILD_STR "Debug"
#else
    #define ZEN_DEBUG 0
    #define ZEN_RELEASE 1
    #define ZEN_BUILD_STR "Release"
#endif

#define ZEN_APP_NAME "Zen"

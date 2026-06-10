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

// API export / import
#ifdef ZEN_EXPORT
    #ifdef _MSC_VER
        #define ZEN_API __declspec(dllexport)
    #else
        #define ZEN_API __attribute__((visibility("default")))
    #endif
#else
    #ifdef _MSC_VER
        #define ZEN_API __declspec(dllimport)
    #else
        #define ZEN_API
    #endif
#endif

// Debug configuration
#if _DEBUG
    #define ZEN_DEBUG 1
    #define ZEN_RELEASE 0
#else
    #define ZEN_DEBUG 0
    #define ZEN_RELEASE 1
#endif

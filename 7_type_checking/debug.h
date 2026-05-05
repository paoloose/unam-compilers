#pragma once
#include <stdio.h>
#include <string.h>

#define UNAM_RESET   "\x1b[0m"

#define UNAM_BLACK        "\x1b[30m"
#define UNAM_RED          "\x1b[31m"
#define UNAM_GREEN        "\x1b[32m"
#define UNAM_YELLOW       "\x1b[33m"
#define UNAM_BLUE         "\x1b[34m"
#define UNAM_MAGENTA      "\x1b[35m"
#define UNAM_CYAN         "\x1b[36m"
#define UNAM_WHITE        "\x1b[37m"

#define UNAM_BG_BLACK      "\x1b[40m"
#define UNAM_BG_RED        "\x1b[41m"
#define UNAM_BG_GREEN      "\x1b[42m"
#define UNAM_BG_YELLOW     "\x1b[43m"
#define UNAM_BG_BLUE       "\x1b[44m"
#define UNAM_BG_MAGENTA    "\x1b[45m"
#define UNAM_BG_CYAN       "\x1b[46m"
#define UNAM_BG_WHITE      "\x1b[47m"

// Returns the filename only
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Column macro for portability
#ifndef __COLUMN__
#   if defined(__clang__)
#       define __COLUMN__ __builtin_COLUMN()
#   else
#       define __COLUMN__ 0
#   endif
#endif

// Environment
#ifndef UNAM_IS_DEBUG
    #define UNAM_IS_DEBUG 1
#endif

#if UNAM_IS_DEBUG == 1
#   define UNAM_DEBUG(x, ...) \
        fprintf( \
            stderr, \
            UNAM_CYAN "%s:" UNAM_BLUE "%d:%d(" UNAM_CYAN "%s" UNAM_BLUE "): " UNAM_RESET x, \
            __FILENAME__, \
            __LINE__, \
            __COLUMN__, \
            __func__, \
            ##__VA_ARGS__ \
        );

    #define UNAM_DEBUG_PLAIN(x, ...) \
        fprintf(stderr, x, ##__VA_ARGS__);

#   define UNAM_ASSERT(cond, msg, ...) \
        do { \
            if (!(cond)) { \
                UNAM_DEBUG(UNAM_RED "Assertion failed: (%s): " UNAM_RESET msg __VA_ARGS__ "\n", #cond, ##__VA_ARGS__); \
                exit(1); \
            } \
        } while (0)
#else
#   define UNAM_DEBUG(x, ...) ((void)0)
#   define UNAM_ASSERT(cond, ...) ((void)0)
#   define UNAM_DEBUG_PLAIN(x, ...) ((void)0)
#endif

#if UNAM_IS_DEBUG == 1
#   define UNAM_DEBUGP(x, ...) \
        fprintf( \
            stderr, \
            UNAM_RESET x, \
            ##__VA_ARGS__ \
        );
#else
#   define UNAM_DEBUGP(x, ...) ((void)0)
#endif

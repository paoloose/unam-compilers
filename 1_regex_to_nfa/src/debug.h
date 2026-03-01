#pragma once
#include <stdio.h>
#include <string.h>

#define UNAM_RESET   "\x1b[0m"
#define UNAM_RED     "\x1b[31m"
#define UNAM_MAGENTA "\x1b[35m"
#define UNAM_BLUE    "\x1b[34m"
#define UNAM_CYAN    "\x1b[36m"
#define UNAM_BRIGHT_RED   "\x1b[91m"
#define UNAM_BRIGHT_GREEN "\x1b[92m"

// Returns the filename only
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Environment
#ifndef UNAM_IS_DEBUG
    #define UNAM_IS_DEBUG 1
#endif

#if UNAM_IS_DEBUG == 1
#   define UNAM_DEBUG(x, ...) \
        fprintf( \
            stderr, \
            UNAM_CYAN "%s:" UNAM_BLUE "%d(" UNAM_CYAN "%s" UNAM_BLUE "): " UNAM_RESET x, \
            __FILENAME__, \
            __LINE__, \
            __func__, \
            ##__VA_ARGS__ \
        );
#else
#   define UNAM_DEBUG(x, ...) ((void)0)
#endif

#pragma once

#ifdef ST_TESTER
#   include <stdio.h>
#   include <stdlib.h>
#   define st_assert(cond, fmt, ...)    \
        if (!(cond)) {                  \
            fprintf(stderr,             \
                "%s:%d:%s(): " fmt "\n",\
                __FILE__,               \
                __LINE__,               \
                __func__,               \
                ##__VA_ARGS__);         \
            abort();                    \
        }
#else
#   define st_assert(cond, fmt, ...)
#endif

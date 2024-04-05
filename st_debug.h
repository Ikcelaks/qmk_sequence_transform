// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "st_defaults.h"

typedef enum
{
    ST_DBG_GENERAL = 1,
    ST_DBG_SEQ_MATCH,
    ST_DBG_RULE_SEARCH,
    ST_DBG_CURSOR,
    ST_DBG_BACKSPACE,
} st_debug_flag_t;

void        st_debug_set_all_flags(void);
void        st_debug_clear_all_flags(void);
void        st_debug_set_flag(st_debug_flag_t flag);
void        st_debug_set_flag_str(const char *flag);
bool        st_debug_test_flag(st_debug_flag_t flag);
const char  **st_debug_get_flag_names(void);

// The do { ... } while (0) idiom ensures that the code acts like a statement (function call).
// The unconditional use of the code ensures that the compiler always checks that your
// debug code is valid — but the optimizer will remove the code when SEQUENCE_TRANSFORM_DEBUG is 0.

#define st_debug(flag, fmt, ...)            \
    do {                                    \
        if (SEQUENCE_TRANSFORM_DEBUG &&     \
            st_debug_test_flag(flag)) {     \
            uprintf(fmt, ##__VA_ARGS__);    \
        }                                   \
    } while (0)

#define st_debug_check(flag)     \
    (SEQUENCE_TRANSFORM_DEBUG && \
     st_debug_test_flag(flag))



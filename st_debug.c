// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <string.h>
#include "st_debug.h"

static long st_debug_bits = 0;

#define FLAG_BIT(flag) (1 << (flag - 1))

//////////////////////////////////////////////////////////////////////
static const char *st_debug_flag_names[] = {
    [ST_DBG_GENERAL]        = "general",
    [ST_DBG_SEQ_MATCH]      = "sequence_match",
    [ST_DBG_RULE_SEARCH]    = "rule_search",
    [ST_DBG_CURSOR]         = "cursor",
    [ST_DBG_BACKSPACE]      = "backspace",
    0
};

//////////////////////////////////////////////////////////////////////
const char **st_debug_get_flag_names(void)
{
    return st_debug_flag_names;
}
//////////////////////////////////////////////////////////////////////
void st_debug_set_flag_str(const char *flag)
{
    if (!strcmp(flag, "all")) {
        st_debug_set_all_flags();
        return;
    }
    for (int i = 1; st_debug_flag_names[i]; ++i) {
        if (!strcmp(flag, st_debug_flag_names[i])) {
            st_debug_set_flag(i);
            return;
        }
    }
    if (!strcmp(flag, "off")) {
        st_debug_clear_all_flags();
    }
}
//////////////////////////////////////////////////////////////////////
void st_debug_set_flag(st_debug_flag_t flag)
{
    st_debug_bits |= FLAG_BIT(flag);
}
//////////////////////////////////////////////////////////////////////
void st_debug_set_all_flags(void)
{
    st_debug_bits = 0xFFFFFFFF;
}
//////////////////////////////////////////////////////////////////////
void st_debug_clear_all_flags(void)
{
    st_debug_bits = 0;
}
//////////////////////////////////////////////////////////////////////
bool st_debug_test_flag(st_debug_flag_t flag)
{
    return (st_debug_bits & FLAG_BIT(flag)) ? true : false;
}

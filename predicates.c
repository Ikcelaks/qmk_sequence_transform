// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrapper.h"
#include "st_assert.h"
#include "st_debug.h"
#include "predicates.h"

//////////////////////////////////////////////////////////////////////
bool st_pred_upper_alpha(uint8_t triecode)
{
    const bool res = triecode >= 'A' && triecode <= 'Z';
    st_debug(ST_DBG_SEQ_MATCH, " st_pred_upper_alpha: Res: %d; Code: %#04X\n", res, triecode);
    return res;
}
//////////////////////////////////////////////////////////////////////
bool st_pred_alpha(uint8_t triecode)
{
    const bool res = (triecode >= 'A' && triecode <= 'Z') || (triecode >= 'a' && triecode <= 'z');
    st_debug(ST_DBG_SEQ_MATCH, " st_pred_alpha: Res: %d; Code: %#04X\n", res, triecode);
    return res;
}
//////////////////////////////////////////////////////////////////////
bool st_pred_digit(uint8_t triecode)
{
    const bool res = triecode >= '0' && triecode <= '9';
    st_debug(ST_DBG_SEQ_MATCH, " st_pred_digit: Res: %d; Code: %#04X\n", res, triecode);
    return res;
}
//////////////////////////////////////////////////////////////////////
bool st_pred_terminating_punct(uint8_t triecode)
{
    bool res;
    switch (triecode) {
        case '.':
        case '!':
        case '?':
            res = true;
            break;
        default:
            res = false;
            break;
    }
    st_debug(ST_DBG_SEQ_MATCH, " st_pred_terminating_punct: Res: %d; Code: %#04X\n", res, triecode);
    return res;
}
//////////////////////////////////////////////////////////////////////
bool st_pred_nonterminating_punct(uint8_t triecode)
{
    bool res;
    switch (triecode) {
        case ',':
        case ';':
        case ':':
            res = true;
            break;
        default:
            res = false;
            break;
    }
    st_debug(ST_DBG_SEQ_MATCH, " st_pred_nonterminating_punct: Res: %d; Code: %#04X\n", res, triecode);
    return res;
}
//////////////////////////////////////////////////////////////////////
bool st_pred_punct(uint8_t triecode)
{
    const bool res = st_pred_terminating_punct(triecode) || st_pred_nonterminating_punct(triecode);
    st_debug(ST_DBG_SEQ_MATCH, " st_pred_punct: Res: %d; Code: %#04X\n", res, triecode);
    return res;
}
//////////////////////////////////////////////////////////////////////
bool st_pred_nonalpha(uint8_t triecode)
{
    const bool res = triecode < 0x80 && !st_pred_alpha(triecode);
    st_debug(ST_DBG_SEQ_MATCH, " st_pred_nonalpha: Res: %d; Code: %#04X\n", res, triecode);
    return res;
}
//////////////////////////////////////////////////////////////////////
bool st_pred_any(uint8_t triecode)
{
    const bool res = true;
    st_debug(ST_DBG_SEQ_MATCH, " st_pred_any: Res: %d; Code: %#04X\n", res, triecode);
    return res;
}
//////////////////////////////////////////////////////////////////////
static const st_predicate_t st_predicates[ST_PREDICATE_COUNT] = {
    st_pred_upper_alpha,
    st_pred_alpha,
    st_pred_digit,
    st_pred_terminating_punct,
    st_pred_nonterminating_punct,
    st_pred_punct,
    st_pred_nonalpha,
    st_pred_any
};
//////////////////////////////////////////////////////////////////////
bool st_predicate_test_triecode(uint8_t predicate_index, uint8_t triecode)
{
    st_assert(predicate_index < ST_PREDICATE_COUNT, "Error: attempted to test predicate %d, which is past the last predicat %d",
                    predicate_index, ST_PREDICATE_COUNT - 1);
    if (predicate_index >= ST_PREDICATE_COUNT) {
        return false;
    }
    return st_predicates[predicate_index](triecode);
}

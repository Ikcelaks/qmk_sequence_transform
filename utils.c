// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "qmk_wrapper.h"
#include "utils.h"

////////////////////////////////////////////////////////////////////////////////
void st_multi_tap(uint16_t keycode, int count)
{
    for (int i = 0; i < count; ++i)
        tap_code16(keycode);
}
//////////////////////////////////////////////////////////////////////////////////////////
void st_send_key(uint16_t keycode)
{
    // Apply shift to sent key if caps word is enabled.
#ifdef CAPS_WORD_ENABLE
    if (is_caps_word_on() && IS_ALPHA_KEYCODE(keycode))
        add_weak_mods(MOD_BIT(KC_LSFT));
#endif
    tap_code16(keycode);
}
//////////////////////////////////////////////////////////////////////
int st_max(int a, int b)
{
    return (a > b ? a : b);
}
//////////////////////////////////////////////////////////////////////
int st_min(int a, int b)
{
    return (a < b ? a : b);
}
//////////////////////////////////////////////////////////////////////
int st_clamp(int val, int min_val, int max_val)
{
    return (val < min_val ? min_val : (val > max_val ? max_val : val));
}

// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source/inspiration: https://getreuer.info/posts/keyboards/autocorrection

#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "st_debug.h"
#include "st_assert.h"
#include "triecodes.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "utils.h"

#ifndef SEQUENCE_TRANSFORM_GENERATOR_VERSION_3_2
#  error "sequence_transform_data.h was generated with an incompatible version of the generator script"
#endif

#if SEQUENCE_TRANSFORM_ENHANCED_BACKSPACE
static bool post_process_do_enhanced_backspace = false;
// Track backspace hold time
static uint32_t backspace_timer = 0;
#endif

#if SEQUENCE_TRANSFORM_RULE_SEARCH
static bool post_process_do_rule_search = false;
void schedule_rule_search(void)
{
    post_process_do_rule_search = true;
}
#else
void schedule_rule_search(void){}
#endif

#define KEY_AT(i) st_key_buffer_get_triecode(&key_buffer, (i))

//////////////////////////////////////////////////////////////////
// Key history buffer
#define KEY_BUFFER_CAPACITY MIN(255, SEQUENCE_MAX_LENGTH + COMPLETION_MAX_LENGTH + SEQUENCE_TRANSFORM_EXTRA_BUFFER)
static st_key_action_t key_buffer_data[KEY_BUFFER_CAPACITY] = {{' ', 0, ST_DEFAULT_KEY_ACTION}};
static uint8_t seq_ref_cache[KEY_BUFFER_CAPACITY*2] = {'\0'};
static st_key_buffer_t key_buffer = {
    key_buffer_data,
    KEY_BUFFER_CAPACITY,
    1,
    0,
    seq_ref_cache,
    KEY_BUFFER_CAPACITY*2,
    1,
    0
};

//////////////////////////////////////////////////////////////////////////////////////////
uint16_t sequence_transform_past_keycode(int index) {
    return st_key_buffer_get_triecode(&key_buffer, index);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Reset buffer on timeout
#if SEQUENCE_TRANSFORM_IDLE_TIMEOUT > 0
static uint32_t sequence_timer = 0;
void sequence_transform_task(void) {
    if (key_buffer.size > 1 &&
        timer_elapsed32(sequence_timer) > SEQUENCE_TRANSFORM_IDLE_TIMEOUT) {
        st_key_buffer_reset(&key_buffer);
        sequence_timer = timer_read32();
    }
}
#endif

//////////////////////////////////////////////////////////////////
// Trie key stack used for searches
#define ST_STACK_SIZE MAX(SEQUENCE_MAX_LENGTH, MAX_BACKSPACES + TRANSFORM_MAX_LENGTH)
static uint8_t trie_key_stack_data[ST_STACK_SIZE] = {0};
static st_key_stack_t trie_stack = {
    trie_key_stack_data,
    ST_STACK_SIZE,
    0
};

//////////////////////////////////////////////////////////////////
// Trie node and completion data
static const st_trie_t trie = {
    SEQUENCE_TRIE_SIZE,
    sequence_transform_trie,
    COMPLETIONS_SIZE,
    sequence_transform_completions_data,
    COMPLETION_MAX_LENGTH,
    MAX_BACKSPACES
};

//////////////////////////////////////////////////////////////////
// Trie cursor
static st_cursor_t trie_cursor = {
    &key_buffer,
    &trie,
    {0, 255,0, false},
    {0},
    false,
};

//////////////////////////////////////////////////////////////////
#ifdef ST_TESTER
const st_trie_t *st_get_trie(void) { return &trie; }
st_key_buffer_t *st_get_key_buffer(void) { return &key_buffer; }
st_cursor_t     *st_get_cursor(void) { return &trie_cursor; }
#endif

/**
 * @brief determine if sequence_transform should process this keypress,
 *        and remove any mods from keycode.
 *
 * @param keycode Keycode registered by matrix press, per keymap
 * @param record keyrecord_t structure
 * @param mods allow processing of mod status
 * @return true Allow sequence_transform
 * @return false Stop processing and escape from sequence_transform
 */
bool st_process_check(uint16_t *keycode,
                      const keyrecord_t *record,
                      uint8_t *mods) {
    // See quantum_keycodes.h for reference on these matched ranges.
    switch (*keycode) {
        // Exclude these keycodes from processing.
        case KC_LSFT:
        case KC_RSFT:
        case KC_CAPS:
        case QK_TO ... QK_TO_MAX:
        case QK_MOMENTARY ... QK_MOMENTARY_MAX:
        case QK_DEF_LAYER ... QK_DEF_LAYER_MAX:
        case QK_TOGGLE_LAYER ... QK_TOGGLE_LAYER_MAX:
        case QK_ONE_SHOT_LAYER ... QK_ONE_SHOT_LAYER_MAX:
        case QK_LAYER_TAP_TOGGLE ... QK_LAYER_TAP_TOGGLE_MAX:
        case QK_LAYER_MOD ... QK_LAYER_MOD_MAX:
        case QK_ONE_SHOT_MOD ... QK_ONE_SHOT_MOD_MAX:
            return false;

        // bake shift mod into keycode symbols
        case KC_1 ... KC_SLASH:
            if (*mods & MOD_MASK_SHIFT) {
                *keycode |= QK_LSFT;
            }
            break;
        // Clear shift for alphas
        case LSFT(KC_A) ... LSFT(KC_Z):
        case RSFT(KC_A) ... RSFT(KC_Z):
            if (*keycode >= QK_LSFT && *keycode <= (QK_LSFT + 255)) {
                *mods |= MOD_LSFT;
            } else {
                *mods |= MOD_RSFT;
            }
            // *keycode = QK_MODS_GET_BASIC_KEYCODE(*keycode); // Get the basic keycode.
            break;
#ifndef NO_ACTION_TAPPING
        // Exclude tap-hold keys when they are held down
        // and mask for base keycode when they are tapped.
        case QK_LAYER_TAP ... QK_LAYER_TAP_MAX:
#    ifdef NO_ACTION_LAYER
            // Exclude Layer Tap, if layers are disabled
            // but action tapping is still enabled.
            return false;
#    else
            // Exclude hold keycode
            if (!record->tap.count) {
                return false;
            }
            *keycode = QK_LAYER_TAP_GET_TAP_KEYCODE(*keycode);
            break;
#    endif
        case QK_MOD_TAP ... QK_MOD_TAP_MAX:
            // Exclude hold keycode
            if (!record->tap.count) {
                return false;
            }
            *keycode = QK_MOD_TAP_GET_TAP_KEYCODE(*keycode);
            if (*mods & MOD_MASK_SHIFT) {
                *keycode |= QK_LSFT;
            }
            break;
#else
        case QK_MOD_TAP ... QK_MOD_TAP_MAX:
        case QK_LAYER_TAP ... QK_LAYER_TAP_MAX:
            // Exclude if disabled
            return false;
#endif
        // Exclude swap hands keys when they are held down
        // and mask for base keycode when they are tapped.
        case QK_SWAP_HANDS ... QK_SWAP_HANDS_MAX:
#ifdef SWAP_HANDS_ENABLE
            // Note: IS_SWAP_HANDS_KEYCODE() actually tests for the special action keycodes like SH_TOGG, SH_TT, ...,
            // which currently overlap the SH_T(kc) range.
            if (IS_SWAP_HANDS_KEYCODE(*keycode)
#    ifndef NO_ACTION_TAPPING
                || !record->tap.count
#    endif // NO_ACTION_TAPPING
            ) {
                return false;
            }
            *keycode = QK_SWAP_HANDS_GET_TAP_KEYCODE(*keycode);
            break;
#else
            // Exclude if disabled
            return false;
#endif
    }
    // Disable autocorrect while a mod other than shift is active.
    if (((*mods | QK_MODS_GET_MODS(*keycode)) & ~MOD_MASK_SHIFT) != 0) {
        st_debug(ST_DBG_GENERAL, "clearing buffer (mods: 0x%04X)\n", *mods);
        st_key_buffer_reset(&key_buffer);
        return false;
    }

    return true;
}
///////////////////////////////////////////////////////////////////////////////
void log_rule(const uint16_t trie_match_index) {
#if SEQUENCE_TRANSFORM_RECORD_RULE_USAGE
    uprintf("st_rule,%d\n", trie_match_index);
#endif
}
//////////////////////////////////////////////////////////////////////
__attribute__((weak)) void sequence_transform_on_missed_rule_user(const st_trie_rule_t *rule)
{
#ifndef NO_PRINT
    uprintf("Missed rule! %s -> %s\n", rule->sequence, rule->transform);
#endif
}
//////////////////////////////////////////////////////////////////
bool st_handle_oneshot_shift(const st_trie_payload_t *action)
{
    // Check if the new last key press triggered the OSS with an output function
    if (action && action->func_code == 1) {
        set_oneshot_mods(MOD_LSFT);
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////
bool st_handle_completion(st_cursor_t *cursor, uint8_t shift_flags)
{
    const st_trie_payload_t *action = st_cursor_get_action(cursor);
    const uint16_t completion_start = action->completion_index;
    if (!action || completion_start == ST_DEFAULT_KEY_ACTION) {
        return false;
    }
    const uint16_t completion_end = completion_start + action->completion_len;
    for (int i = completion_start; i < completion_end; ++i) {
        uint8_t triecode = CDATA(cursor->trie, i);
        if (st_is_trans_seq_ref_triecode(triecode)) {
            triecode = st_cursor_get_seq_ascii(cursor, triecode);
            st_assert(triecode, "Unable to retrieve seq ref (%d) needed to produce the completion\n", triecode);
            st_key_buffer_push_seq_ref(&key_buffer, triecode);
        }
        uint16_t keycode = st_ascii_to_keycode(triecode);
        if (shift_flags & ST_KEY_FLAG_IS_ONE_SHOT_SHIFT) {
            shift_flags &= ~ST_KEY_FLAG_IS_ONE_SHOT_SHIFT;
            keycode = S(keycode);
        } else if (shift_flags & ST_KEY_FLAG_IS_FULL_SHIFT) {
            keycode = S(keycode);
        }
        st_send_key(keycode);
    }
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////
void st_handle_result(const st_trie_t *trie,
                      const st_trie_search_result_t *res) {
    // Most recent key in the buffer triggered a match action, record it in the buffer
    st_key_action_t *current_key = st_key_buffer_get(&key_buffer, 0);
    current_key->action_taken = res->trie_match.trie_match_index;
    current_key->key_flags |= res->trie_match.is_chained_match ? 0 : ST_KEY_FLAG_IS_ANCHOR_MATCH;
    // Log newly added rule match
    log_rule(res->trie_match.trie_match_index);
    // Send backspaces
    const uint8_t num_backspaces = res->trie_payload.num_backspaces;
    st_multi_tap(KC_BSPC, num_backspaces);
    // Send completion string
    st_cursor_init(&trie_cursor, 0, false);
    const uint8_t replaced_shift_flags = num_backspaces ? st_cursor_get_shift_of_nth(&trie_cursor, num_backspaces) : 0;
    if (res->trie_payload.func_code != 2) {
        clear_oneshot_mods();
        current_key->key_flags &= ~ST_KEY_FLAG_IS_ONE_SHOT_SHIFT;
    }
    if (replaced_shift_flags & ST_KEY_FLAG_IS_ONE_SHOT_SHIFT) {
        current_key->key_flags |= ST_KEY_FLAG_IS_ONE_SHOT_SHIFT;
    }
    st_handle_completion(&trie_cursor, current_key->key_flags);
    st_handle_oneshot_shift(&res->trie_payload);
}
//////////////////////////////////////////////////////////////////////////////////////////
#if SEQUENCE_TRANSFORM_ENHANCED_BACKSPACE
void st_handle_backspace() {
    // initialize cursor as input cursor, so that `st_cursor_get_action` is stable
    st_cursor_init(&trie_cursor, 0, false);
    const st_trie_payload_t *action = st_cursor_get_action(&trie_cursor);
    if (action->completion_index == ST_DEFAULT_KEY_ACTION) {
        // previous key-press didn't trigger a rule action. One total backspace required
        st_debug(ST_DBG_BACKSPACE, "Undoing backspace after non-matching keypress\n");
        // backspace was already sent on keydown
        st_key_buffer_pop(&key_buffer);
        // Check if the new last key press triggered the OSS with an output function
        st_cursor_init(&trie_cursor, 0, false);
        st_handle_oneshot_shift(st_cursor_get_action(&trie_cursor));
        return;
    }
    // Undo a rule action
    int backspaces_needed_count = action->completion_len - 1;
    int resend_count = action->num_backspaces;
    if (backspaces_needed_count < 0) {
        // The natural backspace is unwanted. We need to resend that extra keypress
        resend_count -= backspaces_needed_count;
        backspaces_needed_count = 0;
    }
    st_debug(ST_DBG_BACKSPACE, "Undoing previous key action: bs: %d, restore: %d\n",
        backspaces_needed_count, resend_count);
    // If previous action used backspaces, restore the deleted output from earlier actions
    if (resend_count > 0) {
        // reinitialize cursor as output cursor one keystroke before the previous action
        if (st_cursor_init(&trie_cursor, 1, true) &&
            st_cursor_push_to_stack(&trie_cursor, &trie_stack, resend_count)) {
            // Send backspaces now that we know we can do the full undo
            st_multi_tap(KC_BSPC, backspaces_needed_count);
            // Send saved keys in original order
            for (int i = trie_stack.size - 1; i >= 0; --i) {
                st_send_key(st_ascii_to_keycode(trie_stack.buffer[i]));
            }
        } else {
            // The output state is no longer confidently known.
            // Reset the buffer to prevent unintended matches.
            st_key_buffer_reset(&key_buffer);
            return;
        }
    } else {
        // Send backspaces since no resend is needed to complete the undo
        st_multi_tap(KC_BSPC, backspaces_needed_count);
    }
    st_key_buffer_pop(&key_buffer);
    // Check if the new last key press triggered the OSS with an output function
    st_cursor_init(&trie_cursor, 0, false);
    st_handle_oneshot_shift(st_cursor_get_action(&trie_cursor));
}
#endif

/**
 * @brief Performs sequence transform if a match is found in the trie
 *
 * @return true if sequence transform was performed
 */
bool st_perform() {
    // Get completion string from trie for our current key buffer.
    st_trie_search_result_t res = {{0,  {0, 0, 0}, 0}, {0,  0,  0, 0}};
    if (st_trie_get_completion(&trie_cursor, &res)) {
        st_handle_result(&trie, &res);
        return true;
    }
    return false;
}

/**
 * @return false if we should reset the buffer and skip sequence matching
 */
bool st_is_processable_keycode(uint16_t keycode)
{
    switch (keycode) {
        case KC_A ... KC_ENTER:
        case S(KC_A)... S(KC_0):
        case KC_TAB ... KC_SLASH:
        case S(KC_MINUS)... S(KC_SLASH):
            return true;
    }
    // reset for key buffer and don't process this keypress
    return false;
}

/**
 * @brief sets flag to later perform enhanced backspace
 *        or simply clears the buffer on key hold
 */
void st_on_backspace(keyrecord_t *record)
{
#if SEQUENCE_TRANSFORM_ENHANCED_BACKSPACE
    if (record->event.pressed) {
        backspace_timer = timer_read32();
        // set flag so that post_process_sequence_transform will perfom an undo
        post_process_do_enhanced_backspace = true;
        return;
    }
    // This is a release
    if (timer_elapsed32(backspace_timer) > TAPPING_TERM) {
        // Clear the buffer if the backspace key was held past the tapping term
        st_key_buffer_reset(&key_buffer);
    }
#else
    if (record->event.pressed) {
        st_key_buffer_reset(&key_buffer);
    }
#endif
}

/**
 * @brief Process handler for sequence_transform feature.
 *
 * @param keycode Keycode registered by matrix press, per keymap
 * @param record keyrecord_t structure
 * @param sequence_token_start starting keycode index for sequence tokens used in rules
 * @return true Continue processing keycodes, and send to host
 * @return false Stop processing keycodes, and don't send to host
 */
bool process_sequence_transform(uint16_t keycode,
                                keyrecord_t *record,
                                uint16_t sequence_token_start)
{
#if SEQUENCE_TRANSFORM_IDLE_TIMEOUT > 0
    sequence_timer = timer_read32();
#endif

    uint8_t mods = get_mods();
    uint8_t key_flags = (mods & MOD_MASK_SHIFT) ? ST_KEY_FLAG_IS_FULL_SHIFT : 0;
#ifndef NO_ACTION_ONESHOT
    const uint8_t one_shot_mods = get_oneshot_mods();
    mods |= one_shot_mods;
    if (one_shot_mods & MOD_MASK_SHIFT) {
        key_flags |= ST_KEY_FLAG_IS_ONE_SHOT_SHIFT;
    }
#endif
    if (mods & MOD_MASK_SHIFT) {
        keycode = S(keycode);
    }

    st_debug(ST_DBG_GENERAL, "pst keycode: 0x%04X, mods: 0x%02X, pressed: %d\n",
        keycode, mods, record->event.pressed);

    // Keycode verification and extraction
    const bool is_seq_tok = st_is_seq_token_keycode(keycode, sequence_token_start);
    if (!is_seq_tok && !st_process_check(&keycode, record, &mods)) {
        return true;
    }
    // Handle backspace
    if (keycode == KC_BSPC) {
        st_on_backspace(record);
        return true;
    }
    // Don't process on key up
    if (!record->event.pressed) {
        schedule_rule_search();
        return true;
    }
    // if we can't process the keycode, reset the buffer and pass it along to the pipeline
    if (!is_seq_tok && !st_is_processable_keycode(keycode)) {
        st_key_buffer_reset(&key_buffer);
        return true;
    }
    // Convert to triecode and add it to our buffer
    const uint8_t triecode = st_keycode_to_triecode(keycode, sequence_token_start);
    st_debug(ST_DBG_GENERAL, "  translated keycode: 0x%04X (%c)\n",
        keycode, st_triecode_to_ascii(triecode));
    st_key_buffer_push(&key_buffer, triecode, key_flags);
    if (st_debug_check(ST_DBG_GENERAL)) {
        st_key_buffer_print(&key_buffer);
    }
    // Try to perform a sequence transform!
    bool st_perform_res;
    st_log_time_with_result(st_perform(), &st_perform_res);
    if (st_perform_res) {
        // tell QMK to not process this key
        return false;
    }
    return true;
}

/**
 * @brief Performs sequence transform related actions that must occur after normal processing
 *
 * Should be called from the `post_process_record_user` function
 */
void post_process_sequence_transform()
{
#if SEQUENCE_TRANSFORM_ENHANCED_BACKSPACE
    if (post_process_do_enhanced_backspace) {
        // remove last key from the buffer
        //   and undo the action of that key
        st_log_time(st_handle_backspace());
        post_process_do_enhanced_backspace = false;
    }
#endif
}

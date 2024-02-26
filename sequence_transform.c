// Copyright 2021 Google LLC
// Copyright 2021 @filterpaper
// Copyright 2023 Pablo Martinez (@elpekenin) <elpekenin@elpekenin.dev>
// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// SPDX-License-Identifier: Apache-2.0
// Original source: https://getreuer.info/posts/keyboards/autocorrection

#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include <string.h>
#include "keycodes.h"
#include "quantum.h"
#include "quantum_keycodes.h"
#include "keycode_config.h"
#include "send_string.h"
#include "action_util.h"
#include "../general/custom_keys.h"
#include "print.h"
#include "utils.h"

// todo: compute max in script
#define SEQUENCE_TRANSFORM_DICTIONARY_SIZE DICTIONARY_SIZE
#define SPECIAL_KEY_COUNT SEQUENCE_TRANSFORM_COUNT
#define SPECIAL_KEY_TRIECODE_0 0x0100
#define TDATA(L) pgm_read_word(&trie->data[L])
#define CDATA(L) pgm_read_byte(&trie->completions[L])
#define IS_ALPHA_KEYCODE(code) ((code) >= KC_A && (code) <= KC_Z)

//////////////////////////////////////////////////////////////////////////////////////////
// Add KC_SPC on timeout
#if MAGIC_IDLE_TIMEOUT > 0
static uint32_t magic_timer = 0;
void context_magic_task(void)
{
    if (timer_elapsed32(magic_timer) > MAGIC_IDLE_TIMEOUT) {
        reset_buffer();
        magic_timer = timer_read32();
    }
}
#endif

//////////////////////////////////////////////////////////////////
// Key history buffer
static uint16_t key_buffer[SEQUENCE_MAX_LENGTH] = {KC_SPC};
static uint16_t key_buffer_size = 1;

//////////////////////////////////////////////////////////////////
// List of tries
static trie_t trie = {
    SEQUENCE_TRANSFORM_DICTIONARY_SIZE,  sequence_transform_data,  COMPLETIONS_SIZE, sequence_transform_completions_data
};


// Default implementation of sequence_transform_map_key_user().
__attribute__((weak)) bool sequence_transform_map_key_user(uint16_t* keycode, keyrecord_t* record, uint8_t* mods) {
    // Assume that special keys have keycodes ranging from SEQUENCE_TRANSFORM_SPECIAL_KEY_0 to SEQUENCE_TRANSFORM_SPECIAL_KEY_0 + SPECIAL_KEY_COUNT - 1
    if (*keycode >= SEQUENCE_TRANSFORM_SPECIAL_KEY_0 && *keycode < SEQUENCE_TRANSFORM_SPECIAL_KEY_0 + SPECIAL_KEY_COUNT) {
        *keycode = *keycode - SEQUENCE_TRANSFORM_SPECIAL_KEY_0 + SPECIAL_KEY_TRIECODE_0;
    }
    return true;
}

/**
 * @brief determine if context_magic should process this keypress,
 *        and remove any mods from keycode.
 *
 * @param keycode Keycode registered by matrix press, per keymap
 * @param record keyrecord_t structure
 * @param mods allow processing of mod status
 * @return true Allow context_magic
 * @return false Stop processing and escape from context_magic.
 */
bool process_check(uint16_t *keycode, keyrecord_t *record, uint8_t *mods)
{
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
            return true;
        // Clear shift for alphas
        case LSFT(KC_A) ... LSFT(KC_Z):
        case RSFT(KC_A) ... RSFT(KC_Z):
            if (*keycode >= QK_LSFT && *keycode <= (QK_LSFT + 255)) {
                *mods |= MOD_LSFT;
            } else {
                *mods |= MOD_RSFT;
            }
            *keycode = QK_MODS_GET_BASIC_KEYCODE(*keycode); // Get the basic keycode.
            return true;
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
    if ((*mods & ~MOD_MASK_SHIFT) != 0) {
        reset_buffer();
        return false;
    }
    return sequence_transform_map_key_user(keycode, record, mods);
}

/**
 * @brief Add keycode to our key buffer.
 *
 * @param keycode Keycode registered by matrix press, per keymap
 */
void enqueue_keycode(uint16_t keycode)
{
    // Store all alpha chars as lowercase
    const bool shifted = keycode & QK_LSFT;
    const uint8_t lowkey = keycode & 0xFF;
    if (shifted && IS_ALPHA_KEYCODE(lowkey))
        keycode = lowkey;
    // Rotate oldest character if buffer is full.
    if (key_buffer_size >= SEQUENCE_MAX_LENGTH) {
        memmove(key_buffer, key_buffer + 1, sizeof(uint16_t) * (SEQUENCE_MAX_LENGTH - 1));
        key_buffer_size = SEQUENCE_MAX_LENGTH - 1;
    }
    key_buffer[key_buffer_size++] = keycode;
}

//////////////////////////////////////////////////////////////////
void reset_buffer(void)
{
    key_buffer[0] = KC_SPC;
    key_buffer_size = 1;
}
/**
 * @brief Remove num keys from our buffer.
 *
 * @param num number of keys to remove
 */
void dequeue_keycodes(uint8_t num)
{
    key_buffer_size -= MIN(num, key_buffer_size);
    if (!key_buffer_size)
        reset_buffer();
}

/**
 * @brief Find longest chain in trie matching our current key_buffer.
 *
 * @param trie   trie_t struct containing trie data/size
 * @param res    result containing current best
 * @param offset current offset in trie data
 * @param depth  current depth in trie
 * @return       true if match found
 */
bool find_longest_chain(trie_t *trie, trie_search_result_t *res, int offset, int depth)
{
    // Sanity checks
    if (offset >= trie->data_size) {
        uprintf("find_longest_chain() Error: tried reading outside trie data! Offset: %d", offset);
        return false;
    }
    uint16_t code = TDATA(offset);
    if (!code) {
        uprintf("find_longest_chain() Error: unexpected null code! Offset: %d", offset);
        return false;
    }
    uprintf("FIND_LONGEST_CHAIN (%d, %d)", offset, code);
	// Match Node if bit 15 is set
	if (code & 0x8000) {
        uprintf("Match found at Offset: %d", offset);
        // match nodes are side attachments, so decrease depth
		depth--;
        // If bit 14 is also set, there is a child node after the completion string
        if ((code & 0x4000) && find_longest_chain(trie, res, offset+2, depth+1)) {
            uprintf("Looking for deeper match at Offset: %d", offset+2);
            return true;
        }
        // If no better match found deeper, this is the result!
        // char buf[20];
        // sprintf(buf, "|0x%04x|", code);
        // send_string(buf);
		res->completion_offset = TDATA(offset + 1);
        res->func_code = (code >> 11 & 0x0007);
        res->complete_len = code & 127;
        res->num_backspaces = (code >> 7) & 15;
        // Found a match so return true!
		return true;
	}
	// Branch Node (with multiple children) if bit 14 is set
	if (code & 0x4000) {
        if (depth > key_buffer_size)
			return false;
		code &= 0x3FFF;
        // Find child that matches our current buffer location
        const uint16_t cur_key = key_buffer[key_buffer_size - depth];
		for (; code; offset += 2, code = TDATA(offset)) {
            if (code == cur_key) {
                // 16bit offset to child node is built from next uint16_t
                const int child_offset = TDATA(offset+1);
                // Traverse down child node
                return find_longest_chain(trie, res, child_offset, depth+1);
            }
        }
        // Couldn't go deeper, so return false.
        return false;
	}
    // No high bits set, so this is a chain node
	// Travel down chain until we reach a zero byte, or we no longer match our buffer
	for (; code; depth++, code = TDATA(++offset)) {
		if (depth > key_buffer_size ||
            code != key_buffer[key_buffer_size - depth])
			return false;
	}
	// After a chain, there should be a leaf or branch
	return find_longest_chain(trie, res, offset+1, depth);
}
//////////////////////////////////////////////////////////////////////////////////////////
void record_send_key(uint16_t keycode)
{
    enqueue_keycode(keycode);
    // Apply shift to sent key if caps word is enabled.
#ifdef CAPS_WORD_ENABLED
    if (is_caps_word_on() && IS_ALPHA_KEYCODE(keycode))
        add_weak_mods(MOD_BIT(KC_LSFT));
#endif
    tap_code16(keycode);
}
//////////////////////////////////////////////////////////////////////////////////////////
void handle_repeat_key()
{
    uint16_t keycode = KC_NO;
    for (int i = key_buffer_size - 1; i >= 0; --i) {
        keycode = key_buffer[i];
        if (!(keycode & SPECIAL_KEY_TRIECODE_0)) {
            break;
        }
    }
    if (keycode && !(keycode & SPECIAL_KEY_TRIECODE_0)) {
        dequeue_keycodes(1);
        enqueue_keycode(keycode);
        // Apply shift to sent key if caps word is enabled.
#ifdef CAPS_WORD_ENABLED
        if (is_caps_word_on() && IS_ALPHA_KEYCODE(keycode))
            add_weak_mods(MOD_BIT(KC_LSFT));
#endif
        tap_code16(keycode);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////
void handle_result(trie_t *trie, trie_search_result_t *res)
{
    // Send backspaces
    multi_tap(KC_BSPC, res->num_backspaces);
    // Send completion string
    bool ends_with_wordbreak;
    ends_with_wordbreak = (res->complete_len > 0 && CDATA(res->completion_offset + res->complete_len - 1) == ' ');
    for (int i = res->completion_offset; i < res->completion_offset + res->complete_len && i < trie->completions_size; ++i) {
        char ascii_code = CDATA(i);
        tap_code16(char_to_keycode(ascii_code));
    }

    switch (res->func_code) {
        case 1:  // repeat
            handle_repeat_key();
            break;
        case 2:  // set one-shot shift
            set_oneshot_mods(MOD_LSFT);
            break;
        case 3:  // disable auto-wordbreak
            ends_with_wordbreak = false;
    }

    if (ends_with_wordbreak) {
        enqueue_keycode(KC_SPC);
    }
}

/**
 * @brief Handles magic/repeat key press
 *
 * @param keycode Keycode registered by matrix press, per keymap
 * @return false if keycode isn't a registered magic key
 */
bool perform_magic()
{
    // Do nothing if key buffer is empty
    if (!key_buffer_size)
        return false;

    // Look for chain matching our buffer in the trie.
    trie_search_result_t res  = {0, 0};
    if (find_longest_chain(&trie, &res, 0, 1)) {
        handle_result(&trie, &res);
        return true;
    }
    return false;
}

/**
 * @brief Process handler for context_magic feature.
 *
 * @param keycode Keycode registered by matrix press, per keymap
 * @param record keyrecord_t structure
 * @return true Continue processing keycodes, and send to host
 * @return false Stop processing keycodes, and don't send to host
 */
bool process_context_magic(uint16_t keycode, keyrecord_t *record)
{

    uprintf("Process_context_magic for keycode: %d", keycode);
#if MAGIC_IDLE_TIMEOUT > 0
    magic_timer = timer_read32();
#endif
    uint8_t mods = get_mods();
#ifndef NO_ACTION_ONESHOT
    mods |= get_oneshot_mods();
#endif

    if (!record->event.pressed)
        return true;

    // keycode verification and extraction
    if (!process_check(&keycode, record, &mods))
        return true;

    // keycode buffer check
    switch (keycode) {
        case SPECIAL_KEY_TRIECODE_0 ... SPECIAL_KEY_TRIECODE_0 + SPECIAL_KEY_COUNT:
        case KC_A ... KC_0:
        case S(KC_1) ... S(KC_0):
        case KC_MINUS ...KC_SLASH:
            // process normally
            break;
        case S(KC_MINUS) ...S(KC_SLASH):
            // treat " (shifted ') as a word boundary
            if (keycode == S(KC_QUOTE))
                keycode = KC_SPC;
            break;
        case KC_BSPC:
            // remove last character from the buffer
            reset_buffer();
            return true;
        default:
            // set word boundary if some other non-alpha key is pressed
            keycode = KC_SPC;
     }
    // append `keycode` to buffer
    enqueue_keycode(keycode);

    uprintf("  translated keycode: %d", keycode);

    // perform magic action if this is one of our registered keycodes
    if (perform_magic())
        return false;

    return true;
}

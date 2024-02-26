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
#include "print.h"
#include "utils.h"

#define TDATA(L) pgm_read_word(&trie->data[L])
#define CDATA(L) pgm_read_byte(&trie->completions[L])

// todo: script should define these directly in generated .h
#define SEQUENCE_MAX_LENGTH MAGICKEY_MAX_LENGTH
#define SPECIAL_KEY_COUNT 2
#define SPECIAL_KEY_TRIECODE_0 0x0100
#define TRIE_MATCH_BIT      0x8000
#define TRIE_BRANCH_BIT     0x4000
#define TRIE_CODE_MASK      0x3FFF

//////////////////////////////////////////////////////////////////////////////////////////
// Add KC_SPC on timeout
#if SEQUENCE_TRANSFORM_IDLE_TIMEOUT > 0
static uint32_t sequence_timer = 0;
void sequence_transform_task(void)
{
    if (timer_elapsed32(sequence_timer) > SEQUENCE_TRANSFORM_IDLE_TIMEOUT) {
        reset_buffer();
        sequence_timer = timer_read32();
    }
}
#endif

//////////////////////////////////////////////////////////////////
// Key history buffer
static uint16_t key_buffer[SEQUENCE_MAX_LENGTH] = {KC_SPC};
static uint16_t key_buffer_size = 1;

//////////////////////////////////////////////////////////////////
// Trie node and completion data
static trie_t trie = {
    DICTIONARY_SIZE,
    magickey_data,
    COMPLETIONS_SIZE,
    magickey_completions_data
};

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
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
        uprintf("clearing buffer (mods: 0x%04X)\n", *mods);
#endif
        reset_buffer();
        return false;
    }
    return true;
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
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("buffer: |");
    for (int i = 0; i < key_buffer_size; ++i) {
        uprintf("%c", keycode_to_char(key_buffer[i]));
    }
    uprintf("| (%d)\n", key_buffer_size);
#endif
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
bool find_longest_chain(trie_t *trie, trie_search_result_t *res, uint16_t offset, uint8_t depth)
{
    // Sanity checks
    if (offset >= trie->data_size) {
        uprintf("find_longest_chain() Error: tried reading outside trie data! Offset: %d\n", offset);
        return false;
    }
    uint16_t code = TDATA(offset);
    if (!code) {
        uprintf("find_longest_chain() Error: unexpected null code! Offset: %d\n", offset);
        return false;
    }
	// Match Node if bit 15 is set
	if (code & TRIE_MATCH_BIT) {
        // match nodes are side attachments, so decrease depth
        depth--;
        // If bit 14 is also set, there is a child node after the completion string
        if ((code & TRIE_BRANCH_BIT) && find_longest_chain(trie, res, offset+2, depth+1))
            return true;
        // If no better match found deeper, this is the result!
        res->completion_offset = TDATA(offset + 1);
        res->func_code = (code >> 11) & 7;
        res->num_backspaces = (code >> 7) & 15;
        res->complete_len = code & 127;        
        // Found a match so return true!
        return true;
	}
	// Branch Node (with multiple children) if bit 14 is set
	if (code & TRIE_BRANCH_BIT) {
        if (depth > key_buffer_size)
			return false;
		code &= TRIE_CODE_MASK;
        // Find child that matches our current buffer location
        const uint16_t cur_key = key_buffer[key_buffer_size - depth];
		for (; code; offset += 2, code = TDATA(offset)) {
            if (code == cur_key) {
                // 16bit offset to child node is built from next uint16_t
                const uint16_t child_offset = TDATA(offset+1);
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
    send_key(keycode);
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
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("repeat keycode: 0x%04X\n", keycode);
#endif
    if (keycode && !(keycode & SPECIAL_KEY_TRIECODE_0)) {
        dequeue_keycodes(1);
        enqueue_keycode(keycode);
        send_key(keycode);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////
void handle_result(trie_t *trie, trie_search_result_t *res)
{
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("search res: offset: %d, len: %d, bspaces: %d, func: %d\n",
            res->completion_offset, res->complete_len, res->num_backspaces, res->func_code);
#endif
    // bounds check completion data
    if (res->completion_offset + res->complete_len > trie->completions_size) {
        uprintf("ERROR: trying to read past end of completion data buffer! offset: %d, len: %d\n",
                res->completion_offset, res->complete_len);
        return;
    }
    // Send backspaces
    multi_tap(KC_BSPC, res->num_backspaces);
    // Send completion string
    bool ends_with_wordbreak = (res->complete_len > 0 && CDATA(res->completion_offset + res->complete_len - 1) == ' ');
    for (uint16_t i = res->completion_offset, end = i+res->complete_len; i < end; ++i) {
        char ascii_code = CDATA(i);
        send_key(char_to_keycode(ascii_code));
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
 * @brief Performs sequence transform if a match is found in the trie
 *
 * @return true if sequence transform was performed
 */
bool perform_sequence_transform()
{
    // Do nothing if key buffer is empty
    if (!key_buffer_size)
        return false;

    // Look for chain matching our buffer in the trie.
    trie_search_result_t res  = {0, 0, 0, 0};
    if (find_longest_chain(&trie, &res, 0, 1)) {
        handle_result(&trie, &res);
        return true;
    }
    return false;
}

/**
 * @brief Process handler for sequence_transform feature.
 *
 * @param keycode Keycode registered by matrix press, per keymap
 * @param record keyrecord_t structure
 * @param special_key_start starting keycode index for special keys used in rules
 * @return true Continue processing keycodes, and send to host
 * @return false Stop processing keycodes, and don't send to host
 */
bool process_sequence_transform(uint16_t keycode, keyrecord_t *record, uint16_t special_key_start)
{
#if SEQUENCE_TRANSFORM_IDLE_TIMEOUT > 0
    sequence_timer = timer_read32();
#endif
    if (!record->event.pressed)
        return true;
    uint8_t mods = get_mods();
#ifndef NO_ACTION_ONESHOT
    mods |= get_oneshot_mods();
#endif
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("pst keycode: 0x%04X\n", keycode);
#endif
    // If this is one of the special keycodes, convert to our internal trie code
    if (keycode >= special_key_start && keycode < special_key_start + SPECIAL_KEY_COUNT) {
        keycode = keycode - special_key_start + SPECIAL_KEY_TRIECODE_0;
    }
    // keycode verification and extraction
    if (!process_check(&keycode, record, &mods))
        return true;

    // keycode buffer check
    switch (keycode) {
        case SPECIAL_KEY_TRIECODE_0 ... SPECIAL_KEY_TRIECODE_0 + SPECIAL_KEY_COUNT:
        case KC_A ... KC_0:
        case S(KC_1)... S(KC_0):
        case KC_MINUS ... KC_SLASH:
            // process normally
            break;
        case S(KC_MINUS)... S(KC_SLASH):
            // treat " (shifted ') as a word boundary
            if (keycode == S(KC_QUOTE)) keycode = KC_SPC;
            break;
        case KC_BSPC:
            // remove last character from the buffer
            reset_buffer();
            return true;
        default:
            // set word boundary if some other non-alpha key is pressed
            keycode = KC_SPC;
    }
#ifdef SEQUENCE_TRANSFORM_LOG_GENERAL
    uprintf("  translated keycode: 0x%04X (%c)\n", keycode, keycode_to_char(keycode));
#endif
    enqueue_keycode(keycode);
    if (perform_sequence_transform()) {
        // tell QMK to not process this key
        return false;
    } else {
        // TODO: search for rules
    }
    return true;
}

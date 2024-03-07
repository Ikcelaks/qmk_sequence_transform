#include "qmk_wrapper.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "sim_output_buffer.h"
#include "tester_utils.h"
#include "tester.h"

//////////////////////////////////////////////////////////////////////
// (partial) simulation of process_sequence_transform logic
// to test st_find_missed_rule
void sim_st_find_missed_rule(const uint16_t *keycodes)
{
    missed_rule_seq[0] = 0;
    missed_rule_transform[0] = 0;
    st_key_buffer_t *buf = st_get_key_buffer();
    // reset search_len_from_space by first sending a single space
    st_key_buffer_push(buf, KC_SPACE);
    st_find_missed_rule();
    // send input rule seq so we can get output transform to test
    sim_st_perform(keycodes);
    buf->context_len = 0;
    // send the output into input buffer
    // to simulate user typing it directly
    char *output = sim_output_get(false);
    for (char c = *output; c; c = *++output) {
        const uint16_t key = ascii_to_keycode(c);
        st_key_buffer_push(buf, key);
    }
    // from this new input buffer, find missed rule
    st_find_missed_rule();
}
//////////////////////////////////////////////////////////////////////
void test_find_rule(const st_test_rule_t *rule, st_test_result_t *res)
{
    static char message[256];
    res->message = message;
    // Test #3: st_find_missed_rule
    sim_st_find_missed_rule(rule->seq_keycodes);
    res->pass = !strcmp(missed_rule_transform, rule->transform_str);
    if (res->pass) {
        sprintf_s(message, sizeof(message), "OK!");
    } else {
        if (strlen(missed_rule_seq)) {
            sprintf_s(message, sizeof(message), "found: %s ⇒ %s",
                missed_rule_seq, missed_rule_transform);
        } else {
            sprintf_s(message, sizeof(message), "found nothing!");
        }
    }
}

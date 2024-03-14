#include <stdio.h>
#include "qmk_wrapper.h"
#include "keybuffer.h"
#include "key_stack.h"
#include "trie.h"
#include "cursor.h"
#include "sequence_transform.h"
#include "sequence_transform_data.h"
#include "sequence_transform_test.h"
#include "sim_output_buffer.h"
#include "tester.h"

//////////////////////////////////////////////////////////////////////
void test_cursor(const st_test_rule_t *rule, st_test_result_t *res)
{
    static char message[512];
    res->message = message;
    st_cursor_t *cursor = st_get_cursor();
    st_cursor_init(cursor, 0, false);
    for (int i = 0; i < 200; ++i) {
        st_cursor_next(cursor);
    }
    res->pass = cursor->cursor_pos.index == cursor->buffer->context_len;
    if (!res->pass) {
        snprintf(message, sizeof(message), "input cursor didn't stop at end: cursor index %d; context_len: %d",
                    cursor->cursor_pos.index,
                    cursor->buffer->context_len);
    }
    if (st_cursor_init(cursor, 0, true)) {
        for (int i = 0; i < 200; ++i) {
            st_cursor_next(cursor);
        }
    }
    res->pass = cursor->cursor_pos.index == cursor->buffer->context_len;
    if (res->pass) {
        snprintf(message, sizeof(message), "OK!");
    } else {
        snprintf(message, sizeof(message), "output cursor didn't stop at end: cursor index %d; context_len: %d",
                    cursor->cursor_pos.index,
                    cursor->buffer->context_len);
    }
}

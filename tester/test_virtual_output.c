#include "st_defaults.h"
#include "qmk_wrapper.h"
#include "sequence_transform.h"
#include "sim_output_buffer.h"
#include "tester.h"

//////////////////////////////////////////////////////////////////
// compare the virtual output buffer state to the sim output buffer
// FIXME: we should really be comparing keycodes instead of chars!
bool compare_output(char *virtual_output, const char *sim_output, int count)
{
    for (int i = 0; i < count; i++) {
        const char simc = sim_output[i];
        const char virc = virtual_output[count - 1 - i];
        if (simc != virc) {
            return false;
        }
    }
    return true;
}
//////////////////////////////////////////////////////////////////////
void test_virtual_output(const st_test_rule_t *rule, st_test_result_t *res)
{
    sim_st_perform(rule->seq_triecodes);
    const char *sim_output = sim_output_get(false);
    const int sim_len = sim_output_get_size();
    char virtual_output[256] = {0};
    const int virt_len = st_get_virtual_output(virtual_output, 255);
    if (virt_len != sim_len) {
        RES_FAIL("virt len (%d) != sim len (%d)", virt_len, sim_len);
        return;
    }
    if (!compare_output(virtual_output, sim_output, virt_len)) {
        RES_FAIL("mismatch! virt: |%s| sim: |%s|", virtual_output, sim_output);
    }
}

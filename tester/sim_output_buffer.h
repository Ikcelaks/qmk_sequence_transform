#pragma once

void        sim_output_reset(void);
void        sim_output_push(char c);
void        sim_output_pop(int n);
const char  *sim_output_get(bool trim_spaces);
int         sim_output_get_size();
void        sim_output_print();

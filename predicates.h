// Copyright 2024 Guillaume Stordeur <guillaume.stordeur@gmail.com>
// Copyright 2024 Matt Skalecki <ikcelaks@gmail.com>
// Copyright 2024 QKekos <q.kekos.q@gmail.com>
// SPDX-License-Identifier: Apache-2.0
#pragma once

#define ST_PBIT_NONALPHA 0x01
#define ST_PBIT_DIGIT 0x02
#define ST_PBIT_ALPHA 0x04
#define ST_PBIT_UPPERALPHA 0x08
#define ST_PBIT_PUNCT_TERMINILNAL 0x10
#define ST_PBIT_PUNCT_CONNECTING 0x20

#define ST_PREDICATE_COUNT 8

typedef bool (*st_predicate_t)(uint8_t triecode);

bool st_pred_alpha(uint8_t triecode);
bool st_pred_upper_alpha(uint8_t triecode);
bool st_pred_digit(uint8_t triecode);
bool st_pred_terminating_punct(uint8_t triecode);
bool st_pred_nonterminating_punct(uint8_t triecode);
bool st_pred_punct(uint8_t triecode);
bool st_pred_nonalpha(uint8_t triecode);
bool st_pred_any(uint8_t triecode);

bool st_predicate_test_triecode(uint8_t predicate_index, uint8_t triecode);

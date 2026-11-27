// SPDX-License-Identifier: GPL-2.0-or-later
#include "quantum.h"

enum combos { ao, aoe, kt, mt };

uint16_t const ao_combo[]  = {KC_A, KC_O, COMBO_END};
uint16_t const aoe_combo[] = {KC_A, KC_O, KC_E, COMBO_END};
uint16_t const kt_combo[]  = {KC_K, KC_T, COMBO_END};
uint16_t const mt_combo[]  = {RALT_T(KC_K), KC_T, COMBO_END};

// clang-format off
combo_t key_combos[] = {
    [ao]  = COMBO(ao_combo, KC_Q),
    [aoe] = COMBO(aoe_combo, KC_W),
    [kt]  = COMBO(kt_combo, KC_Z),
    [mt]  = COMBO(mt_combo, KC_X),
};
// clang-format on

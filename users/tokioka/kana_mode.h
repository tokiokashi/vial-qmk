// Kana mode switching for the tokioka keymaps: TK on-choku, f1898 or the
// Tsukuyomi.
#pragma once

#include QMK_KEYBOARD_H

// Same order as customKeycodes in each vial.json.
enum kana_keycodes {
    NG_SW = QK_KB_0, // switch the kana layout that hj goes to
    NG_POST,         // switch whether a shift pressed after the kana applies
    NG_ONCHOKU,      // make hj go to TK on-choku, for a tap dance or a layer
    NG_NAGINATA,     // make hj go to f1898
    NG_TKY_SW,       // make hj go to f1898 or Tsukuyomi, whichever it did not
    NG_TSUKUYOMI,    // make hj go to Tsukuyomi
};

// How a Vial combo slot behaves by mode.
typedef enum {
    COMBO_ALWAYS,       // IME on/off, thumb macros, volume
    COMBO_NOT_NAGINATA, // overlaps kana keys, so off while typing f1898
    COMBO_ONCHOKU,      // TK on-choku, on only while the IME is on in on-choku mode and not in English
} combo_class_t;

void kana_mode_init(void);
// pos is the key's place in QWERTY order (Q..P = 0..9, A..; = 10..19, Z../ = 20..29), or -1.
// Returns false when the key was used up.
bool kana_mode_process_record(uint16_t keycode, keyrecord_t *record, int8_t pos);
bool kana_mode_combo_allowed(combo_class_t c);

// Each keymap gives the QWERTY position of a key (see kana_mode_process_record), or -1.
int8_t ng_pos(keypos_t k);

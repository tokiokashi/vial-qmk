// Naginata-style kana engine (tokioka private edition, trial).
// Pure C: no QMK headers, so the decision logic can be tested on a PC.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Key positions in QWERTY order, row by row: Q..P = 0..9, A..; = 10..19, Z../ = 20..29.
#define NG_KEYS 30

// Settings the keymap may change at run time.
extern bool    ng_post_shift; // a shift pressed after the kana still applies
extern uint8_t ng_overlap_pct; // how much of a key's press a shift must cover, in percent

void ng_press(uint8_t pos, uint16_t time);
void ng_release(uint8_t pos, uint16_t time);
// Output every key still waiting, as if all keys were released now.
void ng_flush(uint16_t time);
bool ng_pending(void);

// Provided by the caller.
void ng_send_romaji(const char *romaji);
void ng_ime(bool on);

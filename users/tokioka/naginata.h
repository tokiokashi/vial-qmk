// Naginata-style kana engine (tokioka private edition, trial).
// Pure C: no QMK headers, so the decision logic can be tested on a PC.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Key positions in QWERTY order, row by row: Q..P = 0..9, A..; = 10..19, Z../ = 20..29.
#define NG_KEYS 30

// Settings the keymap may change at run time.
extern bool    ng_post_shift; // a shift pressed after the kana still applies
// Whether the key at pos is held down, and for how long.
bool     ng_down(uint8_t pos);
uint16_t ng_down_ms(uint8_t pos, uint16_t time);
// Drop a held key from the engine, so it gives no kana when released.
void ng_cancel(uint8_t pos);
// Drop a key that is already up, so it gives no kana.
void ng_forget(uint8_t pos);
// Whether keys at positions a and b can make kana together.
bool ng_can_chord(uint8_t a, uint8_t b);
extern uint8_t ng_yoon_gap_ms; // a youon pair pressed within this many ms is a chord
extern uint8_t ng_overlap_pct; // how much of a key's press a shift must cover, in percent

void ng_press(uint8_t pos, uint16_t time);
void ng_release(uint8_t pos, uint16_t time);
// Output every key still waiting, as if all keys were released now.
void ng_flush(uint16_t time);
bool ng_pending(void);

// Provided by the caller.
void ng_send_romaji(const char *romaji);
void ng_ime(bool on);
void ng_tap_esc(void);

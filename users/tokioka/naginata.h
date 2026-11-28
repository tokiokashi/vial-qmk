// Kana engine for tokiokashiki, tokioka's own chord layout.
// Pure C: no QMK headers, so the decision logic can be tested on a PC.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Key positions in QWERTY order, row by row: Q..P = 0..9, A..; = 10..19, Z../ = 20..29.
#define NG_KEYS 30

// Settings the keymap may change at run time.
extern bool    ng_sequential; // the keys go to Tsukuyomi in seqkana.c
// In Tsukuyomi, a kana typed with n keys goes out as n parts of
// its romaji this many ms apart, so a typing tester can count the keys. 0 sends it at once.
extern uint8_t ng_stroke_gap_ms;
// In Tsukuyomi, , and . or . and / pressed within this many ms
// give ! or ?. kana_mode.c sets it to the combo term set in Vial.
extern uint16_t ng_mark_gap_ms;
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
// Whether a and b are the pair for ? and went down close enough to be one.
bool ng_mark_chord(uint8_t a, uint8_t b, uint16_t time);
// The letter to put before [ or ] typed next, z after the toku key (the
// middle-finger shift + T, or D and then P in Tsukuyomi) and s
// after both shifts + Z (L and then P), or 0; clears it. [ and ] come from outside the engine.
char ng_take_bracket_prefix(void);
// In Tsukuyomi, drop a prefix key typed last, for Del. Kana
// waiting before it are sent first. Returns whether there was one.
bool ng_drop_prefix(uint16_t time);

// Provided by the caller.
void ng_send_romaji(const char *romaji);
void ng_ime(bool on);
void ng_tap_esc(void);
void ng_wait_ms(uint16_t ms);

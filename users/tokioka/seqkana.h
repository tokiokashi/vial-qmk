// Kana engine for the sequential layout, typed one key after another.
// naginata.c hands every call to these when ng_sequential is set, so they
// follow the ng_ functions of naginata.h one for one.
#pragma once

#include <stdbool.h>
#include <stdint.h>

void     sq_press(uint8_t pos, uint16_t time);
void     sq_release(uint8_t pos, uint16_t time);
void     sq_flush(uint16_t time);
bool     sq_pending(void);
bool     sq_can_chord(uint8_t a, uint8_t b);
bool     sq_mark_chord(uint8_t a, uint8_t b, uint16_t time);
bool     sq_down(uint8_t pos);
uint16_t sq_down_ms(uint8_t pos, uint16_t time);
void     sq_cancel(uint8_t pos);
void     sq_forget(uint8_t pos);
bool     sq_drop_prefix(uint16_t time);
// Forget every key and count, as at power on. For the tests.
void     sq_reset(void);

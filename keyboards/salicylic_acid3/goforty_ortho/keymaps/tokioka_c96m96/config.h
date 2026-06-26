/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* Salicylic-acid3 公式 vial キーマップと同じ UID を維持 */
#define VIAL_KEYBOARD_UID {0xB0, 0x32, 0x55, 0x63, 0x11, 0x63, 0x5E, 0xC2}
#define VIAL_UNLOCK_COMBO_ROWS { 0, 1 }
#define VIAL_UNLOCK_COMBO_COLS { 0, 12 }

/* 10 レイヤー（公式 vial キーマップと同じ） */
#define DYNAMIC_KEYMAP_LAYER_COUNT 10

/* 容量拡張: combo・macro を 96 まで。
   tap_dance / key_override / alt_repeat は EEPROM 拡張で自動 32 */
#define VIAL_COMBO_ENTRIES          96
#define DYNAMIC_KEYMAP_MACRO_COUNT  96
#define VIAL_TAP_DANCE_ENTRIES      32
#define VIAL_KEY_OVERRIDE_ENTRIES   32
#define VIAL_ALT_REPEAT_KEY_ENTRIES 32

/* RP2040 flash の wear_leveling EEPROM を 8KB に拡張（Corne/ID75 と同じ） */
#define WEAR_LEVELING_BACKING_SIZE 16384
#define WEAR_LEVELING_LOGICAL_SIZE  8192

#define TAPPING_TERM 180

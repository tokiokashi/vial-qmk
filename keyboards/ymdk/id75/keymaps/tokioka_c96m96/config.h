/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* 既存の改造ファーム（現行 .vil）と同じ UID にすることで、
   現行キーマップ .vil を（combo/macro をパディングするだけで）インポート可能にする */
#define VIAL_KEYBOARD_UID {0x2B, 0xA9, 0x75, 0x50, 0x6B, 0x7B, 0x08, 0xFE}

#define VIAL_UNLOCK_COMBO_ROWS {0, 1}
#define VIAL_UNLOCK_COMBO_COLS {0, 1}

/* レイヤー数（現行 .vil と同じ 10 レイヤー維持） */
#undef DYNAMIC_KEYMAP_LAYER_COUNT
#define DYNAMIC_KEYMAP_LAYER_COUNT 10

/* 容量拡張: コンボ・マクロを 96 まで。
   tap_dance / key_override / alt_repeat は EEPROM 拡張により自動で 32 になる */
#define VIAL_COMBO_ENTRIES         96
#define DYNAMIC_KEYMAP_MACRO_COUNT 96
#define VIAL_TAP_DANCE_ENTRIES     32
#define VIAL_KEY_OVERRIDE_ENTRIES  32
#define VIAL_ALT_REPEAT_KEY_ENTRIES 32

/* RP2040 flash の wear_leveling EEPROM 領域を拡張（10 レイヤー + 96 コンボ等を収容） */
#define WEAR_LEVELING_BACKING_SIZE 16384
#define WEAR_LEVELING_LOGICAL_SIZE  8192

#define TAPPING_TERM 180

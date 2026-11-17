// hj (KC_LNG1) turns the IME on and goes to the kana layout chosen by NG_SW:
// TK on-choku or the Naginata private edition. fg (KC_LNG2) turns it off.
// In Naginata mode the alpha keys of layer 0 are read by position, and a
// tap-hold key on the bottom row still gives its modifier when held.

#include "kana_mode.h"
#include "naginata.h"
#include "keymap_introspection.h"

#define NG_POS_V 23
#define NG_POS_M 26

typedef union {
    uint32_t raw;
    struct {
        bool naginata : 1;
        bool prefix_only : 1;
    };
} user_config_t;

static user_config_t cfg;
static bool          ime_on = true;

static bool naginata_on(void) { return ime_on && cfg.naginata; }

static bool on_kana_layer(void) { return get_highest_layer(layer_state | default_layer_state) == 0; }

void ng_send_romaji(const char *romaji) { send_string(romaji); }

void ng_ime(bool on) {
    tap_code(on ? KC_LNG1 : KC_LNG2);
    ime_on = on;
}

void ng_tap_esc(void) { tap_code(KC_ESC); }

void eeconfig_init_user(void) {
    cfg.raw = 0;
    eeconfig_update_user(cfg.raw);
}

void kana_mode_init(void) {
    cfg.raw       = eeconfig_read_user();
    ng_post_shift = !cfg.prefix_only;
}

bool kana_mode_process_record(uint16_t keycode, keyrecord_t *record, int8_t pos) {
    uint16_t time    = record->event.time;
    bool     pressed = record->event.pressed;

    switch (keycode) {
        case KC_LNG1:
        case KC_LNG2:
            if (pressed) {
                ng_flush(time);
                ime_on = keycode == KC_LNG1;
            }
            return true;
        case NG_SW:
            if (pressed) {
                ng_flush(time);
                cfg.naginata = !cfg.naginata;
                eeconfig_update_user(cfg.raw);
            }
            return false;
        case NG_POST:
            if (pressed) {
                cfg.prefix_only = !cfg.prefix_only;
                ng_post_shift   = !cfg.prefix_only;
                eeconfig_update_user(cfg.raw);
            }
            return false;
    }

    if (pos >= 0 && naginata_on() && on_kana_layer() && !(get_mods() | get_oneshot_mods())) {
        bool hold = IS_QK_MOD_TAP(keycode) && record->tap.count == 0;
        if (!hold) {
            if (pressed) {
                ng_press(pos, time);
            } else {
                ng_release(pos, time);
            }
            return false;
        }
    }
    if (pressed && ng_pending()) ng_flush(time);
    return true;
}

// Permissive hold would turn a quick X held over a kana key into Ctrl, so a chord
// such as hi+yo for hyo could only be typed by pressing yo first. While typing
// Naginata a tap-hold key becomes a modifier only after the tapping term.
bool qs_permissive_hold_allowed_user(uint16_t keycode, keyrecord_t *record) {
    return !(naginata_on() && on_kana_layer() && !(get_mods() | get_oneshot_mods()));
}

// V and M are tap-hold Shift keys in the .vil. While typing Naginata they are
// plain kana keys on layer 0, so a key held inside a chord never becomes Shift.
// The rest follows the default keymap_key_to_keycode; no board here has
// encoder or DIP switch maps.
uint16_t keymap_key_to_keycode(uint8_t layer, keypos_t key) {
#ifdef VIAL_ENABLE
    if (vial_unlock_in_progress) return KC_NO;
    if (key.row == VIAL_MATRIX_MAGIC && key.col == VIAL_MATRIX_MAGIC) return g_vial_magic_keycode_override;
#endif
    if (key.row >= MATRIX_ROWS || key.col >= MATRIX_COLS) return KC_NO;
    uint16_t keycode = keycode_at_keymap_location(layer, key.row, key.col);
    if (layer == 0 && naginata_on() && IS_QK_MOD_TAP(keycode)) {
        int8_t pos = ng_pos(key);
        if (pos == NG_POS_V || pos == NG_POS_M) return QK_MOD_TAP_GET_TAP_KEYCODE(keycode);
    }
    return keycode;
}

bool kana_mode_combo_allowed(combo_class_t c) {
    switch (c) {
        case COMBO_ALWAYS:
            return true;
        case COMBO_NOT_NAGINATA:
            return !(naginata_on() && on_kana_layer());
        case COMBO_ONCHOKU:
        default:
            return ime_on && !cfg.naginata;
    }
}

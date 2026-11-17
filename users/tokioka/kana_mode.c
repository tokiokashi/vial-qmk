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

// Tap-hold keys held as kana when a key outside the engine comes, such as Tab
// for Ctrl+Shift+Tab with hi and ko held, turn into their modifiers. Space,
// Enter and Backspace often come at the end of a roll, so for them a key must
// have been held this long.
#define RETRO_HOLD_MS 150

static keypos_t key_at[NG_KEYS];
static uint8_t  retro_mods[NG_KEYS];

static uint8_t mod_tap_mods(uint16_t keycode) {
    uint8_t m = QK_MOD_TAP_GET_MODS(keycode);
    return (m & 0x10) ? (uint8_t)((m & 0x0F) << 4) : (m & 0x0F);
}

static bool roll_end_key(uint16_t keycode) {
    if (IS_QK_LAYER_TAP(keycode)) keycode = QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
    if (IS_QK_MOD_TAP(keycode)) keycode = QK_MOD_TAP_GET_TAP_KEYCODE(keycode);
    return keycode == KC_SPC || keycode == KC_ENT || keycode == KC_BSPC;
}

static void take_held_mods(uint16_t keycode, uint16_t time) {
    uint8_t mods = 0;
    for (uint8_t p = 0; p < NG_KEYS; p++) {
        if (retro_mods[p] || !ng_down(p)) continue;
        uint16_t kc = keycode_at_keymap_location(0, key_at[p].row, key_at[p].col);
        if (!IS_QK_MOD_TAP(kc)) continue;
        if (roll_end_key(keycode) && ng_down_ms(p, time) < RETRO_HOLD_MS) continue;
        ng_cancel(p);
        retro_mods[p] = mod_tap_mods(kc);
        mods |= retro_mods[p];
    }
    // Kana typed before are sent first, without the modifiers.
    if (ng_pending()) ng_flush(time);
    if (mods) register_mods(mods);
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

    if (pos >= 0 && !pressed && retro_mods[pos]) {
        unregister_mods(retro_mods[pos]);
        retro_mods[pos] = 0;
        ng_release(pos, time);
        return false;
    }
    // A key the engine took stays with it until released, even if a modifier came on.
    if (pos >= 0 && !pressed && ng_down(pos)) {
        ng_release(pos, time);
        return false;
    }

    if (pos >= 0 && naginata_on() && on_kana_layer() && !(get_mods() | get_oneshot_mods())) {
        bool hold = IS_QK_MOD_TAP(keycode) && record->tap.count == 0;
        if (!hold) {
            if (pressed) {
                key_at[pos] = record->event.key;
                ng_press(pos, time);
            } else {
                ng_release(pos, time);
            }
            return false;
        }
    }
    if (pressed && naginata_on()) {
        take_held_mods(keycode, time);
    } else if (pressed && ng_pending()) {
        ng_flush(time);
    }
    return true;
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

// A tap-hold key and a key pressed during it that make kana together, such as
// hi and yo for hyo or ha and a dakuten key, are settled as a tap at once. Any
// other key leaves it to permissive hold, so Ctrl and the like work without
// waiting out the tapping term.
int8_t qs_chordal_hold_user(uint16_t tap_hold_keycode, keyrecord_t *tap_hold_record, uint16_t other_keycode, keyrecord_t *other_record) {
    if (!(naginata_on() && on_kana_layer()) || (get_mods() | get_oneshot_mods())) return -1;
    int8_t a = ng_pos(tap_hold_record->event.key), b = ng_pos(other_record->event.key);
    if (a < 0 || b < 0 || !ng_can_chord(a, b)) return -1;
    return 0;
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

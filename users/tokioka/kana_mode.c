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

// Shift and a letter in a word being typed put the IME in English for the
// rest of it, as in Tokyo, and the kana and corner brackets after it would come
// out as letters, as in Tokyo]. The IME on key before them brings it back to
// kana within the same word. Sent when not needed, it does nothing.
static bool ime_eng;

static void ime_back_to_kana(void) {
    if (!ime_eng || !ime_on) return;
    // Sent without the Shift held for ( and ), so the IME sees the plain key.
    uint8_t mods = get_mods();
    if (mods) {
        clear_mods();
        send_keyboard_report();
    }
    tap_code(KC_LNG1);
    if (mods) {
        set_mods(mods);
        send_keyboard_report();
    }
    ime_eng = false;
}

void ng_send_romaji(const char *romaji) {
    ime_back_to_kana();
    send_string(romaji);
}

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
// for Ctrl+Shift+Tab with hi and ko held, turn into their modifiers. Kana are
// decided on release, so Space or Enter after a roll does not meet a held key.
static keypos_t key_at[NG_KEYS];
static uint8_t  retro_mods[NG_KEYS];

#define TAKE_MODS_WAIT_MS 1

static uint8_t mod_tap_mods(uint16_t keycode) {
    uint8_t m = QK_MOD_TAP_GET_MODS(keycode);
    return (m & 0x10) ? (uint8_t)((m & 0x0F) << 4) : (m & 0x0F);
}

static void take_held_mods(uint16_t time) {
    uint8_t mods = 0;
    for (uint8_t p = 0; p < NG_KEYS; p++) {
        if (retro_mods[p] || !ng_down(p)) continue;
        uint16_t kc = keycode_at_keymap_location(0, key_at[p].row, key_at[p].col);
        if (!IS_QK_MOD_TAP(kc)) continue;
        ng_cancel(p);
        retro_mods[p] = mod_tap_mods(kc);
        mods |= retro_mods[p];
    }
    // Kana typed before are sent first, without the modifiers.
    if (ng_pending()) ng_flush(time);
    if (mods) {
        register_mods(mods);
        // The modifier goes out in its own report a moment before the key, as
        // if held all along; Alt+Tab over a remote desktop dropped it otherwise.
        send_keyboard_report();
        wait_ms(TAKE_MODS_WAIT_MS);
    }
}

// A kana key released inside a held tap-hold key, such as yo inside hi, waits
// this long. If the tap-hold key is still held then, it becomes its modifier
// and the inner key is typed with it (Ctrl+I); otherwise they make kana (hyo).
// V and M are left out as holders: they are the half-dakuten shift and are held
// long. Held with a holder, they become Shift along with it (Ctrl+Shift+T).
// The , . and / pairs for ! and ? are left out too, as , and . are tap-hold keys.
#define INNER_WAIT_MS 60

static int8_t   inner_pos = -1, inner_holder = -1;
static uint16_t inner_time;

static bool is_vm(int8_t p) { return p == NG_POS_V || p == NG_POS_M; }

// A held tap-hold key pressed before p, other than V and M, or -1.
static int8_t held_tap_hold_before(uint8_t p, uint16_t time) {
    for (uint8_t h = 0; h < NG_KEYS; h++) {
        if (h == p || is_vm(h) || retro_mods[h] || !ng_down(h)) continue;
        // The ! and ? pairs are kana chords however they are released.
        if (ng_mark_chord(h, p, time)) continue;
        if (ng_down_ms(h, time) <= ng_down_ms(p, time)) continue;
        if (IS_QK_MOD_TAP(keycode_at_keymap_location(0, key_at[h].row, key_at[h].col))) return h;
    }
    return -1;
}

// Give the waiting release to the engine as it happened.
static void inner_settle(void) {
    if (inner_pos < 0) return;
    int8_t p  = inner_pos;
    inner_pos = -1;
    ng_release(p, inner_time);
}

// V or M tapped and then held alone for VM_WORD_MS: the first tap's ko or na
// is erased at once and the key is Shift until released, for capitals. The
// other one of V and M is a letter then.
// Otherwise V and M make kana as usual: a half dakuten chord and the
// continuous shift after it, the comma or period with the other one of V and
// M, and their own kana when tapped alone.
#define VM_TAP_MS 200
// The hold time follows the tapping term set in Vial, so a V or M hold matches
// the other tap-hold keys. The lone D, K, F and J hold below uses it too.
#ifdef QMK_SETTINGS
#    include "qmk_settings.h"
#    define VM_WORD_MS (QS.tapping_term)
#else
#    define VM_WORD_MS TAPPING_TERM
#endif

static bool     passed[NG_KEYS];
static int8_t   vm_first = -1, vm_tap = -1, vm_hold = -1, vm_word = -1;
static uint16_t vm_time;

// One letter typed in that Shift and the key let go: the word goes on in
// English, as in Tokyo, the keys passing as they are until Enter, Esc, an IME
// key (hj, fg), Tab or a shortcut, or until the cursor is left of the capital.
// Two or more letters, as in USB, go back to kana at once.
static uint8_t vm_word_letters;
static bool    eng_word;
static int16_t eng_len, eng_pos;

static bool any_kana_key_down(void) {
    for (uint8_t p = 0; p < NG_KEYS; p++) {
        if (ng_down(p)) return true;
    }
    return false;
}

// Runs from the housekeeping task, so the kana is erased while the key is
// still held alone.
static void vm_word_task(void) {
    if (vm_hold < 0 || timer_elapsed(vm_time) < VM_WORD_MS) return;
    int8_t p = vm_hold;
    vm_hold  = -1;
    vm_first = -1;
    uint16_t kc = keycode_at_keymap_location(0, key_at[p].row, key_at[p].col);
    if (!ng_down(p) || !IS_QK_MOD_TAP(kc) || !naginata_on()) return;
    ng_cancel(p);
    tap_code(KC_BSPC);
    vm_word         = p;
    vm_word_letters = 0;
    retro_mods[p] = mod_tap_mods(kc);
    register_mods(retro_mods[p]);
}

// D, K, F, J, V and M held alone for the tapping term and let go give nothing,
// so a shift key pressed by mistake can be dropped by holding it a while.
#define LONE_SHIFT_MS VM_WORD_MS

static int8_t   lone_pos = -1;
static uint16_t lone_time;

static bool is_lone_shift(int8_t p) { return p == 12 || p == 13 || p == 16 || p == 17 || is_vm(p); }

static void lone_track(int8_t pos, bool pressed, uint16_t time) {
    if (pressed) {
        lone_pos  = is_lone_shift(pos) && naginata_on() && on_kana_layer() && !any_kana_key_down() ? pos : -1;
        lone_time = time;
        return;
    }
    if (pos >= 0 && pos == lone_pos && TIMER_DIFF_16(time, lone_time) >= LONE_SHIFT_MS) ng_cancel(pos);
    if (pos == lone_pos) lone_pos = -1;
}

// Called for every key event before the engine sees it.
static void vm_track(int8_t pos, bool pressed, uint16_t time) {
    if (!pressed) {
        if (pos == vm_hold) vm_hold = -1;
        if (pos >= 0 && pos == vm_word) {
            vm_word  = -1;
            if (vm_word_letters) ime_eng = true;
            eng_word = vm_word_letters == 1;
            eng_len  = 1;
            eng_pos  = 1;
        }
        if (pos >= 0 && pos == vm_first) {
            vm_tap   = TIMER_DIFF_16(time, vm_time) < VM_TAP_MS ? pos : -1;
            vm_first = -1;
            vm_time  = time;
        }
        return;
    }
    if (vm_word >= 0 && pos >= 0) vm_word_letters++;
    bool alone = !eng_word && is_vm(pos) && naginata_on() && on_kana_layer() && !(get_mods() | get_oneshot_mods()) && !any_kana_key_down();
    vm_hold    = alone && pos == vm_tap && TIMER_DIFF_16(time, vm_time) < VM_TAP_MS ? pos : -1;
    vm_first   = alone ? pos : -1;
    vm_tap     = -1;
    vm_time    = time;
}

void housekeeping_task_user(void) {
    vm_word_task();
    if (inner_pos < 0 || timer_elapsed(inner_time) < INNER_WAIT_MS) return;
    int8_t p = inner_pos, h = inner_holder;
    inner_pos = -1;
    if (!ng_down(h)) {
        ng_release(p, inner_time);
        return;
    }
    // The holder and any other held tap-hold key, V and M among them for
    // Ctrl+Shift, become their modifiers.
    ng_forget(p);
    take_held_mods(timer_read());
    // Type the inner key again through the keymap, so key overrides apply.
    action_exec(MAKE_KEYEVENT(key_at[p].row, key_at[p].col, true));
    action_exec(MAKE_KEYEVENT(key_at[p].row, key_at[p].col, false));
}

// The keyboard cannot see the IME, and another window may have it in the
// other state, so the kana layouts would not match it: on-choku combos off
// with the IME on, or romaji typed as letters with it off. After a shortcut
// with Alt or GUI, which may switch windows, or after a pause long enough to
// click elsewhere, the next typed key first sends the state the keyboard has.
#define IME_RESYNC_IDLE_MS 10000

static bool     ime_resync;
static uint32_t ime_last_press;

static bool resync_skips(uint16_t k) {
    if (k == KC_NO || IS_MODIFIER_KEYCODE(k)) return true;
    if (IS_QK_MOMENTARY(k) || IS_QK_TO(k) || IS_QK_TOGGLE_LAYER(k) || IS_QK_ONE_SHOT_LAYER(k) || IS_QK_ONE_SHOT_MOD(k)) return true;
    switch (k) {
        case KC_ENT: case KC_ESC: case KC_BSPC: case KC_TAB: case KC_DEL:
        case KC_LEFT: case KC_RIGHT: case KC_UP: case KC_DOWN:
        case KC_HOME: case KC_END: case KC_PGUP: case KC_PGDN: case KC_INS:
            return true;
    }
    return k >= KC_F1 && k <= KC_F12;
}

static void ime_resync_track(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return;
    uint32_t now = timer_read32();
    if (TIMER_DIFF_32(now, ime_last_press) >= IME_RESYNC_IDLE_MS) ime_resync = true;
    ime_last_press = now;

    uint16_t k = keycode;
    if (IS_QK_LAYER_TAP(k)) k = record->tap.count ? QK_LAYER_TAP_GET_TAP_KEYCODE(k) : KC_NO;
    if (IS_QK_MOD_TAP(k)) k = record->tap.count ? QK_MOD_TAP_GET_TAP_KEYCODE(k) : KC_NO;
    uint8_t m = get_mods() | get_oneshot_mods();
    if (k == KC_LGUI || k == KC_RGUI || ((m & (MOD_MASK_ALT | MOD_MASK_GUI)) && !IS_MODIFIER_KEYCODE(k) && k != KC_NO)) {
        ime_resync = true;
        return;
    }
    if (!ime_resync || (m & MOD_MASK_CTRL) || resync_skips(k)) return;
    ime_resync = false;
    // Typed without the Shift held for a capital, so the IME sees the plain key.
    uint8_t mods = get_mods();
    if (mods) {
        clear_mods();
        send_keyboard_report();
    }
    tap_code(ime_on ? KC_LNG1 : KC_LNG2);
    if (mods) {
        set_mods(mods);
        send_keyboard_report();
    }
}

bool kana_mode_process_record(uint16_t keycode, keyrecord_t *record, int8_t pos) {
    uint16_t time    = record->event.time;
    bool     pressed = record->event.pressed;

    switch (keycode) {
        case KC_LNG1:
        case KC_LNG2:
            if (pressed) {
                eng_word   = false;
                ime_eng    = false;
                ime_resync = false;
                ng_flush(time);
                ime_on = keycode == KC_LNG1;
            }
            return true;
        case NG_SW:
        case NG_ONCHOKU:
        case NG_NAGINATA:
            if (pressed) {
                ng_flush(time);
                cfg.naginata = keycode == NG_SW ? !cfg.naginata : keycode == NG_NAGINATA;
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

    ime_resync_track(keycode, record);

    if (pressed && eng_word) {
        uint16_t k = keycode;
        if (IS_QK_LAYER_TAP(k)) k = record->tap.count ? QK_LAYER_TAP_GET_TAP_KEYCODE(k) : KC_NO;
        if (IS_QK_MOD_TAP(k)) k = record->tap.count ? QK_MOD_TAP_GET_TAP_KEYCODE(k) : KC_NO;
        // Enter and Esc end the word. Tab and shortcuts may move the focus
        // away from it, so they end it too. The cursor is followed in the word
        // as the IME does: back to kana once it is left of the capital, by
        // Backspace, Left or Home.
        if (k == KC_ENT || k == KC_ESC || k == KC_TAB || ((get_mods() | get_oneshot_mods()) & MOD_MASK_CAG)) {
            eng_word = false;
            ime_eng  = false;
        } else if (k == KC_BSPC) {
            if (eng_pos > 0) {
                eng_pos--;
                eng_len--;
            }
        } else if (k == KC_DEL) {
            if (eng_pos < eng_len) eng_len--;
        } else if (k == KC_LEFT) {
            if (eng_pos > 0) eng_pos--;
        } else if (k == KC_RIGHT) {
            if (eng_pos < eng_len) eng_pos++;
        } else if (k == KC_HOME) {
            eng_pos = 0;
        } else if (k == KC_END) {
            eng_pos = eng_len;
        } else if (IS_BASIC_KEYCODE(k) && !IS_MODIFIER_KEYCODE(k) && k != KC_UP && k != KC_DOWN) {
            eng_pos++;
            eng_len++;
        }
        if (eng_pos <= 0) {
            eng_word = false;
            ime_eng  = false;
        }
    }

    // A letter typed with Shift, in either kana layout, may start English in
    // the IME. Enter and Esc end the word being typed and English with it.
    if (pressed && ime_on) {
        uint16_t k = keycode;
        if (IS_QK_LAYER_TAP(k)) k = record->tap.count ? QK_LAYER_TAP_GET_TAP_KEYCODE(k) : KC_NO;
        if (IS_QK_MOD_TAP(k)) k = record->tap.count ? QK_MOD_TAP_GET_TAP_KEYCODE(k) : KC_NO;
        if (k == KC_ENT || k == KC_ESC) {
            ime_eng = false;
        } else if (k >= KC_A && k <= KC_Z) {
            uint8_t m = get_mods() | get_oneshot_mods();
            if ((m & MOD_MASK_SHIFT) && !(m & MOD_MASK_CAG)) ime_eng = true;
        } else if (((k == KC_LBRC || k == KC_RBRC) && !(get_mods() & MOD_MASK_SHIFT)) || k == KC_LPRN || k == KC_RPRN || ((k == KC_9 || k == KC_0) && (get_mods() & MOD_MASK_SHIFT))) {
            // The corner brackets and the parentheses end the English word:
            // the IME is back in kana for them and for what follows.
            eng_word = false;
            ime_back_to_kana();
        }
    }

    lone_track(pos, pressed, time);
    vm_track(pos, pressed, time);

    // Any other event first settles a waiting release as kana.
    if (inner_pos >= 0) inner_settle();

    // A key sent as it is, such as a letter typed with Shift, is released the
    // same way even if the Shift is gone by then.
    if (pos >= 0 && !pressed && passed[pos]) {
        passed[pos] = false;
        return true;
    }
    if (pos >= 0 && !pressed && retro_mods[pos]) {
        unregister_mods(retro_mods[pos]);
        retro_mods[pos] = 0;
        ng_release(pos, time);
        return false;
    }
    // A key the engine took stays with it until released, even if a modifier came on.
    if (pos >= 0 && !pressed && ng_down(pos)) {
        int8_t h = held_tap_hold_before(pos, time);
        if (h >= 0 && naginata_on()) {
            inner_pos    = pos;
            inner_holder = h;
            inner_time   = time;
            return false;
        }
        ng_release(pos, time);
        return false;
    }

    if (pos >= 0 && naginata_on() && on_kana_layer() && vm_word < 0 && !eng_word && !(get_mods() | get_oneshot_mods())) {
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
    if (pressed && naginata_on() && vm_word < 0 && !eng_word) {
        take_held_mods(time);
    } else if (pressed && ng_pending()) {
        ng_flush(time);
    }
    // Entries of the romaji table for keys from other layers:
    // - Y and then [ or ] give z[ and z], the double corner brackets, and the
    //   middle-finger shift + Y gives s[ and s], the lenticular ones
    // - Y and then a digit gives m and the digit, the circled number
    // Another typed key ends the prefix; a layer key pressed to reach them does not.
    if (pressed && IS_BASIC_KEYCODE(keycode) && !IS_MODIFIER_KEYCODE(keycode)) {
        char c = ng_take_bracket_prefix();
        if (c == 'z' && keycode >= KC_1 && keycode <= KC_0) c = 'm';
        else if (keycode != KC_LBRC && keycode != KC_RBRC) c = 0;
        if (c && naginata_on() && !(get_mods() & MOD_MASK_SHIFT)) {
            const char s[2] = {c, 0};
            send_string(s);
        }
    }
    if (pos >= 0 && pressed) passed[pos] = true;
    return true;
}

// The bottom row has tap-hold keys in the .vil (Ctrl, Alt, Shift). While typing
// Naginata they are plain kana keys on layer 0, so a key pressed last in a
// chord, such as hi in bi-yo after a and su, never waits out the tapping term
// and becomes its modifier, and comma and period reach the engine at once for
// ! and ?. Held, they still become modifiers through take_held_mods and the
// inner-key rule. With Ctrl, Alt or GUI already on when one goes down, it
// keeps its tap-hold for shortcuts, decided at the press so its release
// matches.
static bool is_plain_pos(int8_t p) { return p >= 20 && p < NG_KEYS; }
static bool keep_tap_hold[NG_KEYS];

bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        int8_t pos = ng_pos(record->event.key);
        if (is_plain_pos(pos)) keep_tap_hold[pos] = eng_word || (get_mods() & MOD_MASK_CAG);
    }
    return true;
}

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
        if (is_plain_pos(pos) && !keep_tap_hold[pos]) return QK_MOD_TAP_GET_TAP_KEYCODE(keycode);
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
            return eng_word || !(naginata_on() && on_kana_layer());
        case COMBO_ONCHOKU:
        default:
            return ime_on && !cfg.naginata;
    }
}

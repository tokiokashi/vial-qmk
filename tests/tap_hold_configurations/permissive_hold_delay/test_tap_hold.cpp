// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_tapping.h"
#include "test_fixture.hpp"
#include "test_keymap_key.hpp"

using testing::_;
using testing::InSequence;

// Mod-tap keys wait 60 ms after a key typed inside them; layer-tap keys do not.
extern "C" uint16_t get_permissive_hold_delay(uint16_t keycode, keyrecord_t *record) {
    return IS_QK_MOD_TAP(keycode) ? 60 : 0;
}

class PermissiveHoldDelay : public TestFixture {};

// m held, a typed inside it, m let go 20 ms after a: a roll, so m and a.
TEST_F(PermissiveHoldDelay, mod_tap_let_go_soon_after_the_inner_key_is_a_tap) {
    TestDriver driver;
    InSequence s;
    auto       mod_tap_key = KeymapKey(0, 1, 0, RALT_T(KC_M));
    auto       regular_key = KeymapKey(0, 2, 0, KC_A);
    set_keymap({mod_tap_key, regular_key});

    EXPECT_NO_REPORT(driver);
    mod_tap_key.press();
    run_one_scan_loop();
    idle_for(30);
    regular_key.press();
    run_one_scan_loop();
    idle_for(30);
    regular_key.release();
    run_one_scan_loop();
    idle_for(20);
    VERIFY_AND_CLEAR(driver);

    EXPECT_REPORT(driver, (KC_M));
    EXPECT_REPORT(driver, (KC_M, KC_A));
    EXPECT_REPORT(driver, (KC_M));
    EXPECT_EMPTY_REPORT(driver);
    mod_tap_key.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// m still held 60 ms after a is let go: Alt+a.
TEST_F(PermissiveHoldDelay, mod_tap_held_on_after_the_inner_key_is_a_hold) {
    TestDriver driver;
    InSequence s;
    auto       mod_tap_key = KeymapKey(0, 1, 0, RALT_T(KC_M));
    auto       regular_key = KeymapKey(0, 2, 0, KC_A);
    set_keymap({mod_tap_key, regular_key});

    EXPECT_NO_REPORT(driver);
    mod_tap_key.press();
    run_one_scan_loop();
    idle_for(30);
    regular_key.press();
    run_one_scan_loop();
    idle_for(30);
    regular_key.release();
    run_one_scan_loop();
    idle_for(50);
    VERIFY_AND_CLEAR(driver);

    EXPECT_REPORT(driver, (KC_RIGHT_ALT));
    EXPECT_REPORT(driver, (KC_RIGHT_ALT, KC_A));
    EXPECT_REPORT(driver, (KC_RIGHT_ALT));
    idle_for(15);
    VERIFY_AND_CLEAR(driver);

    EXPECT_EMPTY_REPORT(driver);
    mod_tap_key.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// A third key pressed while waiting does not decide it: m let go in time is
// still a tap, and the keys come in the order typed.
TEST_F(PermissiveHoldDelay, next_key_pressed_while_waiting_keeps_the_tap) {
    TestDriver driver;
    InSequence s;
    auto       mod_tap_key = KeymapKey(0, 1, 0, RALT_T(KC_M));
    auto       key_a       = KeymapKey(0, 2, 0, KC_A);
    auto       key_k       = KeymapKey(0, 3, 0, KC_K);
    set_keymap({mod_tap_key, key_a, key_k});

    EXPECT_NO_REPORT(driver);
    mod_tap_key.press();
    run_one_scan_loop();
    idle_for(30);
    key_a.press();
    run_one_scan_loop();
    idle_for(30);
    key_a.release();
    run_one_scan_loop();
    idle_for(10);
    key_k.press();
    run_one_scan_loop();
    idle_for(10);
    VERIFY_AND_CLEAR(driver);

    EXPECT_REPORT(driver, (KC_M));
    EXPECT_REPORT(driver, (KC_M, KC_A));
    EXPECT_REPORT(driver, (KC_M));
    EXPECT_REPORT(driver, (KC_M, KC_K));
    EXPECT_REPORT(driver, (KC_K));
    mod_tap_key.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_EMPTY_REPORT(driver);
    key_k.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

// A layer-tap key keeps plain permissive hold.
TEST_F(PermissiveHoldDelay, layer_tap_settles_at_once) {
    TestDriver driver;
    InSequence s;
    auto       layer_tap_key = KeymapKey(0, 1, 0, LT(1, KC_SPC));
    auto       regular_key   = KeymapKey(0, 2, 0, KC_A);
    auto       layer_key     = KeymapKey(1, 2, 0, KC_B);
    set_keymap({layer_tap_key, regular_key, layer_key});

    EXPECT_NO_REPORT(driver);
    layer_tap_key.press();
    run_one_scan_loop();
    regular_key.press();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_REPORT(driver, (KC_B));
    EXPECT_EMPTY_REPORT(driver);
    regular_key.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);

    EXPECT_NO_REPORT(driver);
    layer_tap_key.release();
    run_one_scan_loop();
    VERIFY_AND_CLEAR(driver);
}

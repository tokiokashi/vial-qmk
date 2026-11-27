// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboard_report_util.hpp"
#include "keycode.h"
#include "test_common.hpp"
#include "action_tapping.h"
#include "test_driver.hpp"
#include "test_fixture.hpp"
#include "test_keymap_key.hpp"

using testing::_;
using testing::InSequence;

// The keys of the tests: A and O make a combo, as do A, O and E, as in the
// vowel chords of on-choku. K starts another combo, as the consonants do.
class ComboRoll : public TestFixture {};

// K held from the kana before, A pressed, K let go, then O: the A+O chord.
TEST_F(ComboRoll, chord_started_while_a_combo_key_is_held) {
    TestDriver driver;
    KeymapKey  key_a(0, 0, 0, KC_A);
    KeymapKey  key_o(0, 1, 0, KC_O);
    KeymapKey  key_e(0, 2, 0, KC_E);
    KeymapKey  key_k(0, 3, 0, KC_K);
    KeymapKey  key_t(0, 4, 0, KC_T);
    set_keymap({key_a, key_o, key_e, key_k, key_t});

    InSequence s;
    EXPECT_REPORT(driver, (KC_K));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_Q));
    EXPECT_EMPTY_REPORT(driver);

    key_k.press();
    run_one_scan_loop();
    idle_for(30);
    key_a.press();
    run_one_scan_loop();
    idle_for(10);
    key_k.release();
    run_one_scan_loop();
    idle_for(10);
    key_o.press();
    run_one_scan_loop();
    idle_for(20);
    key_a.release();
    run_one_scan_loop();
    key_o.release();
    run_one_scan_loop();
    idle_for(100);
    VERIFY_AND_CLEAR(driver);
}

// The same with a key outside any combo held before: it too is let go in
// the middle of the chord.
TEST_F(ComboRoll, chord_started_while_a_plain_key_is_held) {
    TestDriver driver;
    KeymapKey  key_a(0, 0, 0, KC_A);
    KeymapKey  key_o(0, 1, 0, KC_O);
    KeymapKey  key_e(0, 2, 0, KC_E);
    KeymapKey  key_k(0, 3, 0, KC_K);
    KeymapKey  key_t(0, 4, 0, KC_T);
    KeymapKey  key_m(0, 5, 0, KC_M);
    set_keymap({key_a, key_o, key_e, key_k, key_t, key_m});

    InSequence s;
    EXPECT_REPORT(driver, (KC_M));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_Q));
    EXPECT_EMPTY_REPORT(driver);

    key_m.press();
    run_one_scan_loop();
    idle_for(30);
    key_a.press();
    run_one_scan_loop();
    idle_for(10);
    key_m.release();
    run_one_scan_loop();
    idle_for(10);
    key_o.press();
    run_one_scan_loop();
    idle_for(20);
    key_a.release();
    run_one_scan_loop();
    key_o.release();
    run_one_scan_loop();
    idle_for(100);
    VERIFY_AND_CLEAR(driver);
}

// A let go before O is pressed is not a chord: a then o.
TEST_F(ComboRoll, roll_without_overlap_types_both_keys) {
    TestDriver driver;
    KeymapKey  key_a(0, 0, 0, KC_A);
    KeymapKey  key_o(0, 1, 0, KC_O);
    KeymapKey  key_e(0, 2, 0, KC_E);
    KeymapKey  key_k(0, 3, 0, KC_K);
    KeymapKey  key_t(0, 4, 0, KC_T);
    set_keymap({key_a, key_o, key_e, key_k, key_t});

    InSequence s;
    EXPECT_REPORT(driver, (KC_A));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_O));
    EXPECT_EMPTY_REPORT(driver);

    key_a.press();
    run_one_scan_loop();
    idle_for(20);
    key_a.release();
    run_one_scan_loop();
    idle_for(10);
    key_o.press();
    run_one_scan_loop();
    idle_for(20);
    key_o.release();
    run_one_scan_loop();
    idle_for(100);
    VERIFY_AND_CLEAR(driver);
}

// A held, K tapped inside it, then O: K came after A, so A goes first as a
// letter and the chord is not made, keeping the order typed.
TEST_F(ComboRoll, key_tapped_inside_a_held_combo_key_keeps_the_order) {
    TestDriver driver;
    KeymapKey  key_a(0, 0, 0, KC_A);
    KeymapKey  key_o(0, 1, 0, KC_O);
    KeymapKey  key_e(0, 2, 0, KC_E);
    KeymapKey  key_k(0, 3, 0, KC_K);
    KeymapKey  key_t(0, 4, 0, KC_T);
    set_keymap({key_a, key_o, key_e, key_k, key_t});

    InSequence s;
    EXPECT_REPORT(driver, (KC_A));
    EXPECT_REPORT(driver, (KC_A, KC_K));
    EXPECT_REPORT(driver, (KC_A));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_O));
    EXPECT_EMPTY_REPORT(driver);

    key_a.press();
    run_one_scan_loop();
    idle_for(10);
    key_k.press();
    run_one_scan_loop();
    idle_for(10);
    key_k.release();
    run_one_scan_loop();
    idle_for(10);
    key_o.press();
    run_one_scan_loop();
    idle_for(20);
    key_a.release();
    run_one_scan_loop();
    key_o.release();
    run_one_scan_loop();
    idle_for(100);
    VERIFY_AND_CLEAR(driver);
}

// The plain chord still fires after the wait for A+O+E.
TEST_F(ComboRoll, plain_chord) {
    TestDriver driver;
    KeymapKey  key_a(0, 0, 0, KC_A);
    KeymapKey  key_o(0, 1, 0, KC_O);
    KeymapKey  key_e(0, 2, 0, KC_E);
    KeymapKey  key_k(0, 3, 0, KC_K);
    KeymapKey  key_t(0, 4, 0, KC_T);
    set_keymap({key_a, key_o, key_e, key_k, key_t});

    InSequence s;
    EXPECT_REPORT(driver, (KC_Q));
    EXPECT_EMPTY_REPORT(driver);

    key_a.press();
    run_one_scan_loop();
    idle_for(10);
    key_o.press();
    run_one_scan_loop();
    idle_for(30);
    key_a.release();
    run_one_scan_loop();
    key_o.release();
    run_one_scan_loop();
    idle_for(100);
    VERIFY_AND_CLEAR(driver);
}

// A tap-hold key, as m with Alt, held when the chord starts is still a tap.
TEST_F(ComboRoll, chord_started_while_a_mod_tap_key_is_held) {
    TestDriver driver;
    KeymapKey  key_a(0, 0, 0, KC_A);
    KeymapKey  key_o(0, 1, 0, KC_O);
    KeymapKey  key_e(0, 2, 0, KC_E);
    KeymapKey  key_k(0, 3, 0, RALT_T(KC_K));
    KeymapKey  key_t(0, 4, 0, KC_T);
    set_keymap({key_a, key_o, key_e, key_k, key_t});

    InSequence s;
    EXPECT_REPORT(driver, (KC_K));
    EXPECT_EMPTY_REPORT(driver);
    EXPECT_REPORT(driver, (KC_Q));
    EXPECT_EMPTY_REPORT(driver);

    key_k.press();
    run_one_scan_loop();
    idle_for(30);
    key_a.press();
    run_one_scan_loop();
    idle_for(10);
    key_k.release();
    run_one_scan_loop();
    idle_for(10);
    key_o.press();
    run_one_scan_loop();
    idle_for(20);
    key_a.release();
    run_one_scan_loop();
    key_o.release();
    run_one_scan_loop();
    idle_for(100);
    VERIFY_AND_CLEAR(driver);
}

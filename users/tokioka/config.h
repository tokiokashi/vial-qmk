// Options shared by the tokioka keymaps.
#pragma once

// A key let go in the middle of a chord, as the consonant before a vowel
// chord, does not send the keys of the chord on their own.
#define COMBO_KEEP_PENDING_ON_RELEASE

// A mod-tap key with a key typed inside it waits before it becomes a
// modifier (see get_permissive_hold_delay in kana_mode.c).
#define PERMISSIVE_HOLD_DELAY_PER_KEY

VIA_ENABLE          = yes
VIAL_ENABLE         = yes
LTO_ENABLE          = yes

# RGB: rev4 hardware is per-key RGB Matrix (46 LEDs), not the rev1 54-LED underglow.
RGB_MATRIX_ENABLE   = yes
RGBLIGHT_ENABLE     = no
VIALRGB_ENABLE      = yes   # exposes the Lighting tab (per-key RGB matrix) in Vial

# Features required by the imported .vil
COMBO_ENABLE        = yes   # 64 dynamic combo slots (see config.h)
TAP_DANCE_ENABLE    = yes   # TD0/TD1 layer toggles
KEY_OVERRIDE_ENABLE = yes   # ~30 overrides drive the Onishi shifted behaviour
MOUSEKEY_ENABLE     = yes   # layer 2 mouse/wheel
EXTRAKEY_ENABLE     = yes
CAPS_WORD_ENABLE    = yes   # combo: O+- -> Caps Word
QMK_SETTINGS        = yes   # preserve the QMK Settings tuning stored in the .vil

REPEAT_KEY_ENABLE   = yes   # Alt Repeat Key tab in Vial

OLED_ENABLE         = no
LAYER_LOCK_ENABLE   = no

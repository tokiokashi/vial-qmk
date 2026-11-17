// Naginata-style kana engine (tokioka private edition, trial).
//
// Every key is decided when it is released. A shift key (D/K = middle-finger
// shift, F/J = dakuten, V/M = half dakuten) applies to a kana key when the two
// presses overlap by at least ng_overlap_pct percent of the kana key's press.
// Kana are sent as romaji, so the IME must be in romaji mode.

#include "naginata.h"
#include <stddef.h>
#include <string.h>

bool    ng_post_shift  = true;
uint8_t ng_overlap_pct = 66;
// A youon pair also counts when its two keys go down within this many ms, as a
// QMK combo does, so the key pressed first need not be covered for long.
uint8_t ng_yoon_gap_ms = 50;

enum {
    P_Q, P_W, P_E, P_R, P_T, P_Y, P_U, P_I, P_O, P_P,
    P_A, P_S, P_D, P_F, P_G, P_H, P_J, P_K, P_L, P_SC,
    P_Z, P_X, P_C, P_V, P_B, P_N, P_M, P_CM, P_DT, P_SL,
};

// "#S": next kana small, "#E": IME off, "#J": IME on, "#X": Esc.
static const char *const base_face[NG_KEYS] = {
    "#X", "ki", "te", "si", "ri",  "sa", "ta", "su", "ru", "he",
    "e",  "ke", "nn", "ka", "-",   "ku", "a",  "i",  "u",  "xtu",
    "ho", "hi", "ha", "ko", "so",  "o",  "na", "to", "ra", "re",
};
static const char *const shift_face[NG_KEYS] = {
    "#S", "me", "ni", "ne", NULL,  NULL, "e",  "yo", "ro", "yu",
    "se", "mi", "mo", "ma", "ti",  "ya", "no", "mo", "tu", "hu",
    NULL, NULL, "nu", ",",  NULL,  "wo", ".",  "mu", "wa", "/",
};
// Dakuten of whichever kana on the key, base or shift face, can take one.
// wo is read as the dakuten of o.
static const char *const daku_face[NG_KEYS] = {
    NULL, "gi", "de", "zi", NULL,  "za", "da", "zu", NULL, "be",
    "ze", "ge", NULL, "ga", "di",  "gu", NULL, NULL, "du", "bu",
    "bo", "bi", "ba", "go", "zo",  "wo", NULL, "do", NULL, NULL,
};
static const char *const handaku_face[NG_KEYS] = {
    NULL, NULL, NULL, NULL, NULL,  NULL, NULL, NULL, NULL, "pe",
    NULL, NULL, NULL, NULL, NULL,  NULL, NULL, NULL, NULL, "pu",
    "po", "pi", "pa", NULL, NULL,  NULL, NULL, NULL, NULL, NULL,
};

// Small kana, for the key after the middle-finger shift + Q.
// lka and lke are entries of tokioka's own romaji table.
static const char *const small_face[NG_KEYS] = {
    NULL,  NULL,  NULL,  NULL,  NULL,   NULL,  "xe",  "xyo", NULL,  "xyu",
    "xe",  "lke", NULL,  "lka", NULL,   "xya", "xa",  "xi",  "xu",  "xtu",
    NULL,  NULL,  NULL,  NULL,  NULL,   "xo",  NULL,  NULL,  "xwa", NULL,
};

// Youon: a generator key with H = ya, I = yo, P = yu.
typedef struct {
    uint8_t     pos;
    const char *plain, *daku, *handaku;
} gen_t;
static const gen_t gens[] = {
    {P_W, "ky", "gy", NULL}, {P_R, "sy", "zy", NULL}, {P_G, "ty", "dy", NULL}, {P_X, "hy", "by", "py"},
    {P_S, "my", NULL, NULL}, {P_T, "ry", NULL, NULL}, {P_E, "ny", NULL, NULL},
};

// Gairaion: a marker, a generator key and a vowel key pressed together.
// The marker is a half-dakuten key (V or M) or a dakuten key (F or J).
// Positions follow the Kikyo definition of the private edition.
enum { MK_HANDAKU, MK_DAKU };
typedef struct {
    uint8_t     marker, gen, vowel;
    const char *romaji;
} gairai_t;
static const gairai_t gairai[] = {
    {MK_HANDAKU, P_E, P_P, "thu"},   {MK_HANDAKU, P_E, P_K, "thi"},  {MK_HANDAKU, P_CM, P_L, "twu"},
    {MK_HANDAKU, P_R, P_Y, "she"},   {MK_HANDAKU, P_R, P_O, "she"},  {MK_HANDAKU, P_R, P_A, "she"},
    {MK_HANDAKU, P_G, P_Y, "che"},   {MK_HANDAKU, P_G, P_O, "che"},
    {MK_HANDAKU, P_DT, P_Y, "fe"},   {MK_HANDAKU, P_DT, P_U, "fo"},  {MK_HANDAKU, P_DT, P_O, "fe"},  {MK_HANDAKU, P_DT, P_P, "fyu"},
    {MK_HANDAKU, P_DT, P_A, "fe"},   {MK_HANDAKU, P_DT, P_J, "fa"},  {MK_HANDAKU, P_DT, P_K, "fi"},  {MK_HANDAKU, P_DT, P_N, "fo"},
    {MK_HANDAKU, P_L, P_Y, "we"},    {MK_HANDAKU, P_L, P_U, "who"},  {MK_HANDAKU, P_L, P_O, "we"},   {MK_HANDAKU, P_L, P_A, "we"},
    {MK_HANDAKU, P_L, P_K, "wi"},    {MK_HANDAKU, P_L, P_N, "who"},
    {MK_HANDAKU, P_K, P_Y, "ye"},    {MK_HANDAKU, P_K, P_O, "ye"},   {MK_HANDAKU, P_K, P_A, "ye"},   {MK_HANDAKU, P_K, P_L, "wi"},
    {MK_HANDAKU, P_H, P_Y, "qe"},    {MK_HANDAKU, P_H, P_U, "qo"},   {MK_HANDAKU, P_H, P_O, "qe"},   {MK_HANDAKU, P_H, P_A, "qe"},
    {MK_HANDAKU, P_H, P_J, "qa"},    {MK_HANDAKU, P_H, P_K, "qi"},   {MK_HANDAKU, P_H, P_L, "kuxwa"}, {MK_HANDAKU, P_H, P_N, "qo"},
    {MK_HANDAKU, P_H, P_DT, "kuxwa"},
    {MK_HANDAKU, P_SC, P_Y, "tse"},  {MK_HANDAKU, P_SC, P_U, "tso"}, {MK_HANDAKU, P_SC, P_O, "tse"}, {MK_HANDAKU, P_SC, P_A, "tse"},
    {MK_HANDAKU, P_SC, P_J, "tsa"},  {MK_HANDAKU, P_SC, P_K, "tsi"}, {MK_HANDAKU, P_SC, P_N, "tso"},
    {MK_DAKU, P_R, P_Y, "je"}, {MK_DAKU, P_R, P_O, "je"}, {MK_DAKU, P_R, P_A, "je"},
    {MK_DAKU, P_G, P_Y, "dye"}, {MK_DAKU, P_G, P_O, "dye"}, {MK_DAKU, P_G, P_A, "dye"},
    {MK_DAKU, P_E, P_P, "dhu"}, {MK_DAKU, P_E, P_K, "dhi"}, {MK_DAKU, P_CM, P_L, "dwu"},
    {MK_DAKU, P_L, P_Y, "ve"}, {MK_DAKU, P_L, P_U, "vo"}, {MK_DAKU, P_L, P_O, "ve"}, {MK_DAKU, P_L, P_P, "vyu"},
    {MK_DAKU, P_L, P_A, "ve"}, {MK_DAKU, P_L, P_J, "va"}, {MK_DAKU, P_L, P_K, "vi"}, {MK_DAKU, P_L, P_SC, "vu"},
    {MK_DAKU, P_L, P_N, "vo"},
    {MK_DAKU, P_H, P_Y, "gwe"}, {MK_DAKU, P_H, P_U, "gwo"}, {MK_DAKU, P_H, P_O, "gwe"}, {MK_DAKU, P_H, P_A, "gwe"},
    {MK_DAKU, P_H, P_J, "gwa"}, {MK_DAKU, P_H, P_K, "gwi"}, {MK_DAKU, P_H, P_L, "guxwa"}, {MK_DAKU, P_H, P_N, "gwo"},
    {MK_DAKU, P_H, P_DT, "guxwa"},
};

typedef struct {
    uint16_t t_down, t_up;
    bool     down;
    bool     live; // has a press that has not been decided yet
    bool     used;    // served as a shift at least once, so it outputs nothing itself
    bool     touched; // another key was down at some point during this press
} ngkey_t;

// Q alone gives Esc only when tapped within this many ms.
#define NG_ESC_TERM 200

static ngkey_t keys[NG_KEYS];
static bool  small_next;

static bool is_chuu(uint8_t p) { return p == P_D || p == P_K; }
static bool is_daku(uint8_t p) { return p == P_F || p == P_J; }
// V and M also give the half dakuten, as in the original Naginata.
static bool is_handaku(uint8_t p) { return p == P_V || p == P_M; }
static bool is_shift(uint8_t p) { return is_chuu(p) || is_daku(p) || is_handaku(p); }
// V and M pressed together are the comma or the period, not shifts of each other.
static bool is_vm_pair(uint8_t a, uint8_t b) { return is_handaku(a) && is_handaku(b) && a != b; }

// Times as how long ago, so that 16-bit timer wraparound does not matter.
static uint16_t ago(uint16_t now, uint16_t t) { return (uint16_t)(now - t); }
static uint16_t start_ago(const ngkey_t *k, uint16_t now) { return ago(now, k->t_down); }
static uint16_t end_ago(const ngkey_t *k, uint16_t now) { return k->down ? 0 : ago(now, k->t_up); }

static uint16_t duration(uint8_t p, uint16_t now) {
    uint16_t d = start_ago(&keys[p], now) - end_ago(&keys[p], now);
    return d ? d : 1;
}

static uint16_t overlap(uint8_t a, uint8_t b, uint16_t now) {
    uint16_t sa = start_ago(&keys[a], now), sb = start_ago(&keys[b], now);
    uint16_t ea = end_ago(&keys[a], now), eb = end_ago(&keys[b], now);
    uint16_t s = sa < sb ? sa : sb;
    uint16_t e = ea > eb ? ea : eb;
    return s > e ? s - e : 0;
}

// Whether b covers enough of a's press.
static bool covers(uint8_t b, uint8_t a, uint16_t now) {
    return (uint32_t)overlap(a, b, now) * 100 >= (uint32_t)ng_overlap_pct * duration(a, now);
}

static bool pressed_before(uint8_t a, uint8_t b, uint16_t now) { return start_ago(&keys[a], now) >= start_ago(&keys[b], now); }

static void emit(const char *r) {
    if (!r) return;
    if (r[0] == '#') {
        if (r[1] == 'S') {
            small_next = true;
        } else if (r[1] == 'X') {
            small_next = false;
            ng_tap_esc();
        } else {
            small_next = false;
            ng_ime(r[1] == 'J');
        }
        return;
    }
    ng_send_romaji(r);
}

static const char *single(uint8_t p, int daku, bool chuu) {
    if (daku >= 2) return handaku_face[p];
    if (daku == 1) return daku_face[p];
    return chuu ? shift_face[p] : base_face[p];
}

static const gen_t *gen_of(uint8_t p) {
    for (size_t i = 0; i < sizeof(gens) / sizeof(gens[0]); i++) {
        if (gens[i].pos == p) return &gens[i];
    }
    return NULL;
}

static const char *yoon_vowel(uint8_t p) {
    switch (p) {
        case P_H: return "a";
        case P_I: return "o";
        case P_P: return "u";
        default: return NULL;
    }
}

// Whether a and b went down within ng_yoon_gap_ms of each other and overlap.
static bool pressed_close(uint8_t a, uint8_t b, uint16_t now) {
    uint16_t sa = start_ago(&keys[a], now), sb = start_ago(&keys[b], now);
    return overlap(a, b, now) > 0 && (sa > sb ? sa - sb : sb - sa) <= ng_yoon_gap_ms;
}

static bool is_yoon_pair(uint8_t x, uint8_t y) { return (gen_of(x) && yoon_vowel(y)) || (gen_of(y) && yoon_vowel(x)); }

// Romaji for a two-key chord of x and y, or NULL.
static const char *pair_romaji(uint8_t x, uint8_t y, int daku, char *buf, uint16_t now) {
    // M then V gives the comma and V then M the period, in the order pressed.
    if (daku == 0 && ((x == P_V && y == P_M) || (x == P_M && y == P_V))) return pressed_before(P_M, P_V, now) ? "," : ".";
    // ! and ? on the bottom row, like the combos of TK on-choku.
    if (daku == 0 && ((x == P_CM && y == P_DT) || (x == P_DT && y == P_CM))) return "!";
    if (daku == 0 && ((x == P_DT && y == P_SL) || (x == P_SL && y == P_DT))) return "?";
    const gen_t *g = gen_of(x);
    const char  *v = yoon_vowel(y);
    if (!g || !v) {
        g = gen_of(y);
        v = yoon_vowel(x);
    }
    if (!g || !v) return NULL;
    const char *c = daku >= 2 ? g->handaku : daku == 1 ? g->daku : g->plain;
    if (!c) return NULL;
    strcpy(buf, c);
    strcat(buf, v);
    return buf;
}

static void resolve(uint8_t x, uint16_t now);

// Whether p is pressed together with x: x itself, or a key covering enough of x's press.
static bool in_chord(uint8_t p, uint8_t x, uint16_t now) {
    if (p == x) return true;
    return keys[p].live && !keys[p].used && covers(p, x, now);
}

// A gairaion chord that x is part of, or NULL. Sets the marker key.
static const gairai_t *find_gairai(uint8_t x, uint16_t now, uint8_t *marker) {
    for (size_t i = 0; i < sizeof(gairai) / sizeof(gairai[0]); i++) {
        const gairai_t *g = &gairai[i];
        if (!in_chord(g->gen, x, now) || !in_chord(g->vowel, x, now)) continue;
        const uint8_t cand[2] = {g->marker == MK_HANDAKU ? P_V : P_F, g->marker == MK_HANDAKU ? P_M : P_J};
        for (int j = 0; j < 2; j++) {
            uint8_t m = cand[j];
            if (m == g->gen || m == g->vowel) continue;
            // A marker key held for earlier kana may still mark this chord.
            if (m != x && !(keys[m].live && covers(m, x, now))) continue;
            if (x != m && x != g->gen && x != g->vowel) continue;
            *marker = m;
            return g;
        }
    }
    return NULL;
}

// Decide keys pressed before x that are still waiting, so output keeps press order.
static void flush_earlier(uint8_t x, uint16_t now, const bool *skip) {
    for (;;) {
        int first = -1;
        for (uint8_t p = 0; p < NG_KEYS; p++) {
            if (p == x || skip[p] || !keys[p].live || keys[p].used) continue;
            if (!pressed_before(p, x, now)) continue;
            if (first < 0 || pressed_before(p, (uint8_t)first, now)) first = p;
        }
        if (first < 0) return;
        resolve((uint8_t)first, now);
    }
}

static void resolve(uint8_t x, uint16_t now) {
    if (!keys[x].live) return;
    keys[x].live = false;
    if (keys[x].used) return;

    bool    skip[NG_KEYS] = {false};
    uint8_t mods[4];
    int     nmods = 0, daku = 0;
    bool    chuu = false;
    for (uint8_t s = 0; s < NG_KEYS; s++) {
        if (!is_shift(s) || s == x || !keys[s].live || is_vm_pair(s, x)) continue;
        if (!covers(s, x, now)) continue;
        if (!ng_post_shift && !is_shift(x) && !pressed_before(s, x, now)) continue;
        mods[nmods++] = s;
        // Both dakuten keys together are still the dakuten; V or M gives the half dakuten.
        if (is_daku(s) && daku < 1) daku = 1;
        if (is_handaku(s)) daku = 2;
        if (is_chuu(s)) chuu = true;
    }

    // Gairaion chords come first: their vowel key may be a shift key.
    uint8_t         marker;
    const gairai_t *g = find_gairai(x, now, &marker);
    if (g) {
        skip[marker] = skip[g->gen] = skip[g->vowel] = true;
        flush_earlier(x, now, skip);
        for (int i = 0; i < 3; i++) {
            uint8_t p = i == 0 ? marker : i == 1 ? g->gen : g->vowel;
            if (p == x) continue;
            // A held dakuten key keeps working as a shift; the others are used up.
            if ((is_daku(p) || is_handaku(p)) && p == marker) {
                keys[p].used = true;
            } else {
                keys[p].live = false;
            }
        }
        small_next = false;
        emit(g->romaji);
        return;
    }

    // Q held with a key gives that key's small kana, as the Kikyo <q> face did.
    // k is the key that is not Q.
    for (uint8_t y = 0; y < NG_KEYS; y++) {
        uint8_t k = x == P_Q ? y : x;
        if (y == x || (x != P_Q && y != P_Q) || !small_face[k]) continue;
        if (!keys[y].live || keys[y].used) continue;
        // Q is held first, and the other key's press lies mostly within Q's.
        if (!pressed_before(P_Q, k, now)) continue;
        if (!covers(y, x, now) && !(!keys[y].down && covers(x, y, now))) continue;
        for (int i = 0; i < nmods; i++) skip[mods[i]] = true;
        skip[y] = true;
        flush_earlier(x, now, skip);
        keys[y].live = false;
        for (int i = 0; i < nmods; i++) keys[mods[i]].used = true;
        small_next = false;
        emit(small_face[k]);
        return;
    }

    // The key after the middle-finger shift + Q gives a small kana, whatever shift is held.
    if (small_next && small_face[x]) {
        for (int i = 0; i < nmods; i++) skip[mods[i]] = true;
        flush_earlier(x, now, skip);
        for (int i = 0; i < nmods; i++) keys[mods[i]].used = true;
        small_next = false;
        emit(small_face[x]);
        return;
    }
    small_next = false;

    // Two-key chords: youon and vu.
    char buf[8];
    for (uint8_t y = 0; y < NG_KEYS; y++) {
        if (y == x || (is_shift(y) && !is_vm_pair(x, y)) || !keys[y].live || keys[y].used) continue;
        if (!covers(y, x, now) && !(is_yoon_pair(x, y) && pressed_close(x, y, now))) continue;
        const char *r = pair_romaji(x, y, daku, buf, now);
        if (!r) continue;
        for (int i = 0; i < nmods; i++) skip[mods[i]] = true;
        skip[y] = true;
        flush_earlier(x, now, skip);
        keys[y].live = false;
        for (int i = 0; i < nmods; i++) keys[mods[i]].used = true;
        emit(r);
        return;
    }

    const char *r = single(x, daku, chuu);

    // J+H alone turns the IME on and F+G alone turns it off, like the hj and fg
    // combos of TK on-choku. Their dakuten (gu, di) are typed with the other hand.
    if (nmods == 1 && x == P_H && mods[0] == P_J) r = "#J";
    if (nmods == 1 && x == P_G && mods[0] == P_F) r = "#E";

    // Two shift keys together: x may be the shift and the other key the kana.
    if (!r && is_shift(x)) {
        for (int i = 0; i < nmods; i++) {
            uint8_t m = mods[i];
            if (keys[m].used) continue;
            const char *r2 = single(m, is_daku(x) ? 1 : is_handaku(x) ? 2 : 0, is_chuu(x));
            if (!r2) continue;
            skip[m] = true;
            flush_earlier(x, now, skip);
            keys[m].live = false;
            emit(r2);
            return;
        }
    }

    // A dakuten that cannot apply is not a shift for this key.
    if (!r && daku > 0) {
        int n = 0;
        for (int i = 0; i < nmods; i++) {
            if (is_chuu(mods[i])) mods[n++] = mods[i];
        }
        nmods = n;
        daku  = 0;
        r     = single(x, 0, chuu);
    }
    // A blank on the shift face gives the base kana; the middle finger stays a shift.
    if (!r) r = base_face[x];
    // Q is Esc only for a short tap that no other key overlapped, so a missed
    // Q chord or a long press does not throw away the text being composed.
    if (x == P_Q && r == base_face[P_Q] && (keys[P_Q].touched || duration(P_Q, now) > NG_ESC_TERM)) r = NULL;

    for (int i = 0; i < nmods; i++) skip[mods[i]] = true;
    flush_earlier(x, now, skip);
    for (int i = 0; i < nmods; i++) keys[mods[i]].used = true;
    emit(r);
}

void ng_press(uint8_t pos, uint16_t time) {
    if (pos >= NG_KEYS) return;
    if (keys[pos].live) resolve(pos, time);
    keys[pos] = (ngkey_t){.t_down = time, .down = true, .live = true};
    for (uint8_t q = 0; q < NG_KEYS; q++) {
        if (q == pos || !keys[q].down) continue;
        keys[q].touched   = true;
        keys[pos].touched = true;
    }
}

// Whether a key that p may still shift is held: a kana key, or a shift key pressed after p.
static bool shiftable_key_down(uint8_t p, uint16_t now) {
    for (uint8_t q = 0; q < NG_KEYS; q++) {
        if (q == p || !keys[q].live || !keys[q].down) continue;
        if (!is_shift(q) || !pressed_before(q, p, now)) return true;
    }
    return false;
}

// Decide released keys in press order. A released shift key waits while a key it
// may still shift is held.
static void settle(uint16_t now) {
    for (;;) {
        int first = -1;
        for (uint8_t p = 0; p < NG_KEYS; p++) {
            if (!keys[p].live || keys[p].down) continue;
            if (is_shift(p) && shiftable_key_down(p, now)) continue;
            if (first < 0 || pressed_before(p, (uint8_t)first, now)) first = p;
        }
        if (first < 0) return;
        resolve((uint8_t)first, now);
    }
}

void ng_release(uint8_t pos, uint16_t time) {
    if (pos >= NG_KEYS) return;
    keys[pos].down = false;
    keys[pos].t_up = time;
    if (!is_shift(pos)) resolve(pos, time);
    settle(time);
}

void ng_flush(uint16_t time) {
    // A key outside the engine came while these were held.
    for (uint8_t p = 0; p < NG_KEYS; p++) {
        if (keys[p].down) keys[p].touched = true;
    }
    for (;;) {
        int first = -1;
        for (uint8_t p = 0; p < NG_KEYS; p++) {
            if (!keys[p].live) continue;
            if (first < 0 || pressed_before(p, (uint8_t)first, time)) first = p;
        }
        if (first < 0) break;
        resolve((uint8_t)first, time);
    }
    for (uint8_t p = 0; p < NG_KEYS; p++) {
        // Held keys are finished; their release must not output again.
        keys[p].live = false;
        keys[p].used = keys[p].down;
    }
}

bool ng_pending(void) {
    for (uint8_t p = 0; p < NG_KEYS; p++) {
        if (keys[p].live) return true;
    }
    return false;
}

// Whether p takes part in gairaion chord g, as its marker, generator or vowel.
static bool in_gairai(const gairai_t *g, uint8_t p) {
    if (p == g->gen || p == g->vowel) return true;
    return g->marker == MK_HANDAKU ? is_handaku(p) : is_daku(p);
}

bool ng_can_chord(uint8_t a, uint8_t b) {
    if (a >= NG_KEYS || b >= NG_KEYS || a == b) return false;
    for (size_t i = 0; i < sizeof(gairai) / sizeof(gairai[0]); i++) {
        if (in_gairai(&gairai[i], a) && in_gairai(&gairai[i], b)) return true;
    }
    // A shift key is a partner only when it changes the other key: ho and hi
    // have nothing on the middle-finger shift face, so Ctrl+D and Ctrl+K on
    // them still work at once.
    if (is_shift(a) && is_shift(b)) return true;
    if (is_shift(a) || is_shift(b)) {
        uint8_t s = is_shift(a) ? a : b, k = is_shift(a) ? b : a;
        if (is_chuu(s)) return shift_face[k] != NULL;
        if (is_handaku(s)) return handaku_face[k] != NULL;
        return daku_face[k] != NULL;
    }
    char buf[8];
    for (int d = 0; d <= 2; d++) {
        if (pair_romaji(a, b, d, buf, 0)) return true;
    }
    return false;
}

bool ng_down(uint8_t pos) { return pos < NG_KEYS && keys[pos].down; }

uint16_t ng_down_ms(uint8_t pos, uint16_t time) { return pos < NG_KEYS ? start_ago(&keys[pos], time) : 0; }

void ng_cancel(uint8_t pos) {
    if (pos >= NG_KEYS) return;
    keys[pos].live = false;
    keys[pos].used = true;
}

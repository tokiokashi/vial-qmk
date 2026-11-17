// Naginata-style kana engine (tokioka private edition, trial).
//
// Every key is decided when it is released. A shift key (D/K = middle-finger
// shift, F/J = dakuten) applies to a kana key when the two presses overlap by
// at least ng_overlap_pct percent of the kana key's press. Holding both F and J
// gives handakuten. Kana are sent as romaji, so the IME must be in romaji mode.

#include "naginata.h"
#include <stddef.h>
#include <string.h>

bool    ng_post_shift  = true;
uint8_t ng_overlap_pct = 66;

enum {
    P_Q, P_W, P_E, P_R, P_T, P_Y, P_U, P_I, P_O, P_P,
    P_A, P_S, P_D, P_F, P_G, P_H, P_J, P_K, P_L, P_SC,
    P_Z, P_X, P_C, P_V, P_B, P_N, P_M, P_CM, P_DT, P_SL,
};

// "#S": next kana small, "#E": IME off, "#J": IME on.
static const char *const base_face[NG_KEYS] = {
    "ro", "ki", "te", "si", "ri",  "sa", "ta", "su", "ru", "he",
    "e",  "ke", "nn", "ka", "-",   "ku", "a",  "i",  "u",  "xtu",
    "ho", "hi", "ha", "ko", "so",  "o",  "na", "to", "ra", "re",
};
static const char *const shift_face[NG_KEYS] = {
    "#S", "me", "ni", "ne", "#E",  "#J", "e",  "yo", "ro", "yu",
    "se", "mi", "mo", "ma", "ti",  "ya", "no", "mo", "tu", "hu",
    NULL, NULL, "nu", ",",  NULL,  "wo", ".",  "mu", "wa", "/",
};
// Dakuten of whichever kana on the key, base or shift face, can take one.
static const char *const daku_face[NG_KEYS] = {
    NULL, "gi", "de", "zi", NULL,  "za", "da", "zu", NULL, "be",
    "ze", "ge", NULL, "ga", "di",  "gu", NULL, NULL, "du", "bu",
    "bo", "bi", "ba", "go", "zo",  NULL, NULL, "do", NULL, NULL,
};
static const char *const handaku_face[NG_KEYS] = {
    NULL, NULL, NULL, NULL, NULL,  NULL, NULL, NULL, NULL, "pe",
    NULL, NULL, NULL, NULL, NULL,  NULL, NULL, NULL, NULL, "pu",
    "po", "pi", "pa", NULL, NULL,  NULL, NULL, NULL, NULL, NULL,
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

typedef struct {
    uint16_t t_down, t_up;
    bool     down;
    bool     live; // has a press that has not been decided yet
    bool     used; // served as a shift at least once, so it outputs nothing itself
} ngkey_t;

static ngkey_t keys[NG_KEYS];
static bool  small_next;

static bool is_chuu(uint8_t p) { return p == P_D || p == P_K; }
static bool is_daku(uint8_t p) { return p == P_F || p == P_J; }
static bool is_shift(uint8_t p) { return is_chuu(p) || is_daku(p); }

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
        } else {
            small_next = false;
            ng_ime(r[1] == 'J');
        }
        return;
    }
    if (small_next) {
        small_next = false;
        static const char *const smallable[] = {"a", "i", "u", "e", "o", "ya", "yu", "yo", "tu", "wa"};
        for (size_t i = 0; i < sizeof(smallable) / sizeof(smallable[0]); i++) {
            if (strcmp(r, smallable[i]) == 0) {
                char buf[4] = "x";
                strcat(buf, r);
                ng_send_romaji(buf);
                return;
            }
        }
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

// Romaji for a two-key chord of x and y, or NULL.
static const char *pair_romaji(uint8_t x, uint8_t y, int daku, char *buf) {
    if (daku >= 1 && ((x == P_L && y == P_SC) || (x == P_SC && y == P_L))) return "vu";
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
        if (!is_shift(s) || s == x || !keys[s].live) continue;
        if (!covers(s, x, now)) continue;
        if (!ng_post_shift && !is_shift(x) && !pressed_before(s, x, now)) continue;
        mods[nmods++] = s;
        if (is_daku(s)) daku++;
        if (is_chuu(s)) chuu = true;
    }

    // Two-key chords: youon and vu.
    char buf[8];
    for (uint8_t y = 0; y < NG_KEYS; y++) {
        if (y == x || is_shift(y) || !keys[y].live || keys[y].used) continue;
        if (!covers(y, x, now)) continue;
        const char *r = pair_romaji(x, y, daku, buf);
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
            const char *r2 = single(m, is_daku(x) ? 1 : 0, is_chuu(x));
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
            if (!is_daku(mods[i])) mods[n++] = mods[i];
        }
        nmods = n;
        daku  = 0;
        r     = single(x, 0, chuu);
    }
    // A blank on the shift face gives the base kana; the middle finger stays a shift.
    if (!r) r = base_face[x];

    for (int i = 0; i < nmods; i++) skip[mods[i]] = true;
    flush_earlier(x, now, skip);
    for (int i = 0; i < nmods; i++) keys[mods[i]].used = true;
    emit(r);
}

void ng_press(uint8_t pos, uint16_t time) {
    if (pos >= NG_KEYS) return;
    if (keys[pos].live) resolve(pos, time);
    keys[pos] = (ngkey_t){.t_down = time, .down = true, .live = true};
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

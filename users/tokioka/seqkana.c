// Kana engine for the sequential layout (MF5-29). Each kana is one key, or a
// prefix key and then a key:
// - D or K and then a key of the other hand gives the kana on its back face.
//   Followed by a key of the same hand, it gives っ and that key as usual.
//   Pressed twice, it gives the back face of a key of the same hand, or っ
//   and the back face of a key of the other hand.
// - L and then a key gives the dakuten of whichever kana on the key can take one.
// - P and then a key gives the half dakuten. P and then keys pressed together
//   give the gairaion spelt by the letters of those keys in TK on-choku.
// - D and then P is the toku key: then H, J, K or L gives an arrow, T and Y
//   the leaders, and [ or ] from another layer z[ or z]. L and then P makes
//   [ and ] s[ and s]. D and then Y gives ・, and D and then N gives っ alone.
// - や, ゆ and よ right after a kana of the i row are small, and う and お
//   are ゅう and ょう. L before them gives the other one.
// - Del right after a prefix key drops the prefix.
// - A kana typed with n keys is sent as n parts of its romaji, ng_stroke_gap_ms
//   apart, so a typing tester that counts signals 10 ms apart counts the keys.
//   Keys left over by a short romaji, as あ typed with K and V, are added to
//   the parts of the kana sent next.
//
// Keys are taken in the order they went down, and each one is decided when it
// or a key pressed after it is released. A tap-hold key held as a modifier
// is dropped from the queue by kana_mode.c before that.

#include "naginata.h"
#include "seqkana.h"
#include <stddef.h>
#include <string.h>

enum {
    P_Q, P_W, P_E, P_R, P_T, P_Y, P_U, P_I, P_O, P_P,
    P_A, P_S, P_D, P_F, P_G, P_H, P_J, P_K, P_L, P_SC,
    P_Z, P_X, P_C, P_V, P_B, P_N, P_M, P_CM, P_DT, P_SL,
};

// The faces in romaji. D, K, L and P are prefix keys and have no front face.
static const char *const front_face[NG_KEYS] = {
    NULL, "ko", "no", "ni", ",",  ".",  "u",  "si", "ri", NULL,
    "to", "i",  NULL, "ka", "o",  "-",  "nn", NULL, NULL, "na",
    "ya", "ku", "yo", "ta", "yu", "tu", "te", "su", "ru", "ki",
};
static const char *const back_face[NG_KEYS] = {
    NULL, "mi", "se", "ho", NULL, "/",  "wa", "ro", "ti", NULL,
    "mu", "hu", "ha", "mo", "he", "hi", "sa", "ke", "ra", "so",
    "nu", "e",  "re", "a",  NULL, "ltu", "ma", "wo", "me", "ne",
};

typedef struct {
    const char *plain, *voiced;
} pair_t;

static const pair_t dakuten[] = {
    {"ka", "ga"}, {"ki", "gi"}, {"ku", "gu"}, {"ke", "ge"}, {"ko", "go"},
    {"sa", "za"}, {"si", "zi"}, {"su", "zu"}, {"se", "ze"}, {"so", "zo"},
    {"ta", "da"}, {"ti", "di"}, {"tu", "du"}, {"te", "de"}, {"to", "do"},
    {"ha", "ba"}, {"hi", "bi"}, {"hu", "bu"}, {"he", "be"}, {"ho", "bo"},
    {"u", "vu"},
};
static const pair_t handakuten[] = {
    {"ha", "pa"}, {"hi", "pi"}, {"hu", "pu"}, {"he", "pe"}, {"ho", "po"},
};

// The letters of TK on-choku, which is the Onishi layout, by key position.
static const char onchoku_letter[NG_KEYS + 1] = "qlu,.fwryp" "eiao-ktnsh" "zxcv;gdmjb";
static char letter(uint8_t p) {
    char c = onchoku_letter[p];
    return c >= 'a' && c <= 'z' ? c : 0;
}

// Gairaion after P, by the letters of the keys pressed together.
static const char *const gairai[] = {
    "fa",  "fi",  "fe",  "fo",  "fyu", "va",  "vi",  "ve",  "vo",  "thi",
    "thu", "dhi", "dhu", "twu", "dwu", "wi",  "we",  "who", "tsa", "tsi",
    "tse", "tso", "che", "she", "je",  "ye",  "qa",  "qi",  "qe",  "qo",
    "gwa", "swi", "zwi", "tyu", "dyu",
};

// The kana of the i row, after which や, ゆ, よ, う and お change.
static const char *const i_row[] = {"ki", "si", "ti", "ni", "hi", "mi", "ri", "gi", "zi", "di", "bi", "pi"};

typedef enum { PRE_NONE, PRE_D, PRE_K, PRE_DD, PRE_KK, PRE_L, PRE_P, PRE_TOKU } prefix_t;

// The letter to put before [ or ] typed next: z after the toku key, s after
// L and then P, or 0.
static char bracket_prefix;

// The romaji that tokioka's own romaji table turns into the arrows and the
// leaders, for the key after the toku key.
static const char *toku_romaji(uint8_t p) {
    switch (p) {
        case P_H: return "xk"; // left arrow
        case P_J: return "xt"; // down arrow
        case P_K: return "xn"; // up arrow
        case P_L: return "xs"; // right arrow
        case P_T: return "z,"; // two-dot leader
        case P_Y: return "z."; // three-dot leader
        default: return NULL;
    }
}

typedef struct {
    uint16_t t_down;
    bool     down;
    bool     live; // in the queue, not decided yet
} sqkey_t;

static sqkey_t  keys[NG_KEYS];
static uint8_t  queue[NG_KEYS];
static uint8_t  qlen;
static prefix_t prefix;
// The kana sent last, or NULL after a key outside the engine.
static const char *last;

uint8_t ng_stroke_gap_ms = 15;
// Keys typed for the kana not sent yet, and whether this call sent anything.
static uint8_t strokes;
// Keys typed for kana sent already that their romaji was too short to show.
static uint8_t carry;
#define CARRY_MAX 8
static bool    sent_in_call;

static bool is_left(uint8_t p) { return p % 10 < 5; }

static const char *find_pair(const pair_t *t, size_t n, const char *k) {
    if (!k) return NULL;
    for (size_t i = 0; i < n; i++) {
        if (!strcmp(t[i].plain, k)) return t[i].voiced;
    }
    return NULL;
}
static const char *voiced(const char *k) { return find_pair(dakuten, sizeof(dakuten) / sizeof(dakuten[0]), k); }
static const char *half_voiced(const char *k) { return find_pair(handakuten, sizeof(handakuten) / sizeof(handakuten[0]), k); }

static bool after_i_row(void) {
    if (!last) return false;
    for (size_t i = 0; i < sizeof(i_row) / sizeof(i_row[0]); i++) {
        if (!strcmp(i_row[i], last)) return true;
    }
    return false;
}

static bool is_y(const char *k) { return k && k[0] == 'y'; }
static bool is_uo(const char *k) { return k && (!strcmp(k, "u") || !strcmp(k, "o")); }

// にや, みや, りや and によ stay large after the i row; L makes them small.
static bool youon_exception(const char *y) {
    if (!last) return false;
    if (!strcmp(y, "ya")) return !strcmp(last, "ni") || !strcmp(last, "mi") || !strcmp(last, "ri");
    return !strcmp(y, "yo") && !strcmp(last, "ni");
}

// The romaji goes out in as many parts as the keys typed for it and those
// carried over: one letter each, and the rest with the last part.
static void send(const char *r, const char *kana) {
    size_t len = strlen(r), want = (strokes ? strokes : 1) + carry, parts = want < len ? want : len;
    carry      = want - parts > CARRY_MAX ? CARRY_MAX : (uint8_t)(want - parts);
    for (size_t i = 0; i < parts; i++) {
        char   buf[8];
        size_t n = i + 1 < parts ? 1 : len - i;
        if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
        memcpy(buf, r + i, n);
        buf[n] = 0;
        if (sent_in_call && ng_stroke_gap_ms) ng_wait_ms(ng_stroke_gap_ms);
        ng_send_romaji(buf);
        sent_in_call = true;
    }
    strokes = 0;
    last    = kana;
}

// っ before the kana of the key k, each with its own key: the prefix keys
// before k go with っ.
static void send_ltu(void) {
    strokes--;
    send("ltu", NULL);
    strokes = 1;
}

static void send_small_y(const char *y) {
    static const char *const small[] = {"xya", "xyu", "xyo"};
    send(small[y[1] == 'a' ? 0 : y[1] == 'u' ? 1 : 2], NULL);
}

// A kana from a face, changed by the kana before it as the rules above say.
static void send_kana(const char *k) {
    if (!k) return;
    if (after_i_row()) {
        if (is_y(k)) {
            if (youon_exception(k)) {
                send(k, k);
            } else {
                send_small_y(k);
            }
            return;
        }
        if (is_uo(k)) {
            send(k[0] == 'u' ? "xyuu" : "xyou", NULL);
            return;
        }
    }
    send(k, k);
}

static bool is_prefix(uint8_t p) { return p == P_D || p == P_K || p == P_L || p == P_P; }

static void dispatch(uint8_t k);

// Keys typed for nothing sent, and extra keys such as Del, go to the kana
// sent next.
static void drop_strokes(uint8_t extra) {
    uint16_t c = (uint16_t)carry + strokes + extra;
    carry      = c > CARRY_MAX ? CARRY_MAX : (uint8_t)c;
    strokes    = 0;
}

static void step(uint8_t k) {
    strokes++;
    bracket_prefix = 0;
    dispatch(k);
    // A key that gave nothing, such as P and then a key with no half dakuten,
    // goes to the kana sent next.
    if (prefix == PRE_NONE) drop_strokes(0);
}

static void dispatch(uint8_t k) {
    prefix_t p = prefix;
    prefix     = PRE_NONE;
    switch (p) {
        case PRE_NONE:
            if (is_prefix(k)) {
                prefix = k == P_D ? PRE_D : k == P_K ? PRE_K : k == P_L ? PRE_L : PRE_P;
                return;
            }
            send_kana(front_face[k]);
            return;
        case PRE_D:
        case PRE_K: {
            uint8_t s = p == PRE_D ? P_D : P_K;
            if (k == s) {
                prefix = p == PRE_D ? PRE_DD : PRE_KK;
                return;
            }
            if (is_left(k) == is_left(s)) {
                send_ltu();
                dispatch(k);
                return;
            }
            if (k == P_P) {
                prefix         = PRE_TOKU;
                bracket_prefix = 'z';
                return;
            }
            send_kana(back_face[k]);
            return;
        }
        case PRE_TOKU: {
            const char *r = toku_romaji(k);
            if (r) {
                send(r, NULL);
            } else {
                dispatch(k);
            }
            return;
        }
        case PRE_DD:
        case PRE_KK:
            if (k == P_P) {
                dispatch(k);
                return;
            }
            if (is_left(k) != (p == PRE_DD)) send_ltu();
            send_kana(back_face[k]);
            return;
        case PRE_L: {
            const char *f = front_face[k], *b = back_face[k];
            if (k == P_P) {
                bracket_prefix = 's';
                return;
            }
            if (after_i_row()) {
                const char *y = is_y(f) ? f : is_y(b) ? b : NULL;
                if (y) {
                    if (youon_exception(y)) {
                        send_small_y(y);
                    } else {
                        send(y, y);
                    }
                    return;
                }
                const char *uo = is_uo(f) ? f : is_uo(b) ? b : NULL;
                if (uo) {
                    send(uo, uo);
                    return;
                }
            }
            const char *v = voiced(f) ? f : voiced(b) ? b : NULL;
            if (v) {
                send(voiced(v), voiced(v));
            } else if (f) {
                send(f, f);
            }
            return;
        }
        case PRE_P: {
            const char *f = front_face[k], *b = back_face[k];
            const char *h = half_voiced(f) ? f : half_voiced(b) ? b : NULL;
            if (h) send(half_voiced(h), half_voiced(h));
            return;
        }
    }
}

static int8_t queue_index(uint8_t pos) {
    for (uint8_t i = 0; i < qlen; i++) {
        if (queue[i] == pos) return (int8_t)i;
    }
    return -1;
}

static void queue_drop(uint8_t i) {
    keys[queue[i]].live = false;
    memmove(&queue[i], &queue[i + 1], qlen - i - 1);
    qlen--;
}

static void sort_letters(char *s, uint8_t n) {
    for (uint8_t i = 1; i < n; i++) {
        char    c = s[i];
        uint8_t j = i;
        for (; j > 0 && s[j - 1] > c; j--) s[j] = s[j - 1];
        s[j] = c;
    }
}

// The keys in the queue after P, pressed together: the gairaion their
// letters spell, or else the half dakuten of the first and the rest one by one.
static void resolve_group(void) {
    char    letters[8];
    uint8_t n = 0;
    for (uint8_t i = 0; i < qlen && n < sizeof(letters) - 1; i++) letters[n++] = letter(queue[i]);
    letters[n] = 0;
    sort_letters(letters, n);
    if (n >= 2) {
        for (size_t g = 0; g < sizeof(gairai) / sizeof(gairai[0]); g++) {
            char   s[8];
            size_t l = strlen(gairai[g]);
            if (l != n) continue;
            memcpy(s, gairai[g], l + 1);
            sort_letters(s, n);
            if (strcmp(s, letters)) continue;
            prefix = PRE_NONE;
            strokes += n;
            while (qlen) queue_drop(0);
            send(gairai[g], NULL);
            return;
        }
    }
    while (qlen) {
        uint8_t k = queue[0];
        queue_drop(0);
        step(k);
    }
}

// Decide the queue up to and including pos, or all of it when pos is NG_KEYS.
static void run_queue(uint8_t pos) {
    while (qlen) {
        if (prefix == PRE_P) {
            resolve_group();
            return;
        }
        uint8_t k = queue[0];
        queue_drop(0);
        step(k);
        if (k == pos) return;
    }
}

void sq_press(uint8_t pos, uint16_t time) {
    if (pos >= NG_KEYS) return;
    if (keys[pos].live) {
        int8_t i = queue_index(pos);
        if (i >= 0) queue_drop((uint8_t)i);
    }
    keys[pos].down   = true;
    keys[pos].live   = true;
    keys[pos].t_down = time;
    queue[qlen++]    = pos;
}

void sq_release(uint8_t pos, uint16_t time) {
    (void)time;
    if (pos >= NG_KEYS) return;
    keys[pos].down = false;
    sent_in_call   = false;
    if (keys[pos].live) run_queue(pos);
}

void sq_flush(uint16_t time) {
    (void)time;
    sent_in_call = false;
    run_queue(NG_KEYS);
    prefix = PRE_NONE;
    drop_strokes(0);
    last = NULL;
}

bool sq_drop_prefix(uint16_t time) {
    (void)time;
    sent_in_call = false;
    run_queue(NG_KEYS);
    bool had = prefix != PRE_NONE;
    prefix   = PRE_NONE;
    // Del is not sent when it drops a prefix, so it counts with the next kana.
    drop_strokes(had ? 1 : 0);
    last = NULL;
    return had;
}

bool sq_pending(void) { return qlen || prefix != PRE_NONE || last; }

// A gairaion may be on its way: P is decided, or waits in the queue.
static bool chord_pending(void) { return prefix == PRE_P || queue_index(P_P) >= 0; }

bool sq_can_chord(uint8_t a, uint8_t b) { return a < NG_KEYS && b < NG_KEYS && a != b && chord_pending(); }

bool sq_mark_chord(uint8_t a, uint8_t b, uint16_t time) {
    (void)time;
    return sq_can_chord(a, b) && keys[a].live && keys[b].live;
}

bool sq_down(uint8_t pos) { return pos < NG_KEYS && keys[pos].down; }

uint16_t sq_down_ms(uint8_t pos, uint16_t time) { return pos < NG_KEYS ? (uint16_t)(time - keys[pos].t_down) : 0; }

void sq_cancel(uint8_t pos) {
    if (pos >= NG_KEYS || !keys[pos].live) return;
    queue_drop((uint8_t)queue_index(pos));
}

void sq_forget(uint8_t pos) {
    sq_cancel(pos);
    if (pos < NG_KEYS) keys[pos].down = false;
}

void sq_reset(void) {
    memset(keys, 0, sizeof(keys));
    qlen    = 0;
    prefix  = PRE_NONE;
    strokes = 0;
    carry   = 0;
    last    = NULL;
    bracket_prefix = 0;
}

char sq_take_bracket_prefix(void) {
    char c         = bracket_prefix;
    bracket_prefix = 0;
    if (prefix == PRE_TOKU) prefix = PRE_NONE;
    return c;
}

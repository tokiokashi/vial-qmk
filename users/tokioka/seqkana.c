// Kana engine for Tsukuyomi (月夜見), a sequential layout. Each kana is one
// key, or a prefix key and then a key:
// - D or K and then a key of the other hand gives the kana on its back face.
//   Followed by a key of the same hand, it gives っ and that key as usual.
//   Pressed twice, it gives the back face of a key of the same hand, or っ
//   and the back face of a key of the other hand.
// - L and then a key gives the dakuten of whichever kana on the key can take one.
// - P and then a key gives the half dakuten. P and then one or two consonant
//   keys held and a vowel chord give the gairaion spelt by their letters in
//   TK on-choku, as ふぁ for f and a. The vowel chord is any vowel chord of
//   TK on-choku, so i and e give ye, as in じぇ, and i, a, o and x give yeku,
//   as in じぇく. It is decided when one of its keys is released, a key
//   outside it goes down, or ng_mark_gap_ms passes after its last key. A
//   romaji whose first kana is a plain one, such as ka, is no gairaion.
//   Two or three consonant keys pressed within ng_mark_gap_ms after P give
//   the word of the TK on-choku combo, as です for d and s.
// - D and then P is the toku key: then H, J, K or L gives an arrow, T and Y
//   the leaders, and [ or ] from another layer z[ or z]. L and then P makes
//   [ and ] s[ and s]. D and then Y gives ・, and D and then N gives っ alone.
// - , and . pressed within ng_mark_gap_ms give !, and . and / give ?, as in
//   TK on-choku.
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

// The letters of TK on-choku by key position. They differ from the Onishi
// layout on Z, X and the full stop key.
static const char onchoku_letter[NG_KEYS + 1] = "qlu,.fwryp" "eiao-ktnsh" "xjcv/gdmzb";
static char letter(uint8_t p) {
    char c = onchoku_letter[p];
    return c >= 'a' && c <= 'z' ? c : 0;
}

static bool is_vowel(char c) { return c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o'; }

// Consonants of two letters, as the IME spells them, by their letters in
// alphabetical order.
static const char *const clusters[][2] = {
    {"ht", "th"}, {"dh", "dh"}, {"st", "ts"}, {"hw", "wh"}, {"kw", "kw"}, {"gw", "gw"}, {"sw", "sw"},
    {"wz", "zw"}, {"ch", "ch"}, {"hs", "sh"}, {"tw", "tw"}, {"dw", "dw"}, {"fy", "fy"}, {"vy", "vy"},
};

// Gairaion with no y before the vowel. Any one consonant, y and a vowel, as
// zye for じぇ, is one too.
static const char *const gairai[] = {
    "fa",  "fi",  "fe",  "fo",  "va",  "vi",  "vu",  "ve",  "vo",  "thi", "thu", "dhi", "dhu",
    "twu", "dwu", "wi",  "we",  "who", "tsa", "tsi", "tse", "tso", "che", "she", "je",  "ja",
    "ju",  "jo",  "ye",  "qa",  "qi",  "qe",  "qo",  "kwa", "gwa", "swi", "zwi", "fyu", "vyu",
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

static void run_queue(uint8_t pos);

static void sort_chars(char *s) {
    for (uint8_t i = 1; s[i]; i++) {
        char    c = s[i];
        uint8_t j = i;
        for (; j > 0 && s[j - 1] > c; j--) s[j] = s[j - 1];
        s[j] = c;
    }
}

// The vowel chords of TK on-choku: their keys by letter in any order, and the
// romaji they type after a consonant.
static const char *const vowel_chords[][2] = {
    {"a", "a"},       {"i", "i"},       {"u", "u"},       {"e", "e"},       {"o", "o"},
    {"ia", "ya"},     {"iu", "yu"},     {"ie", "ye"},     {"io", "yo"},     {"iue", "iu"},
    {"euo", "ie"},    {"ieuo", "io"},   {"aei", "ae"},    {"ia-", "yau"},   {"oae", "oo"},
    {"i-", "ii"},     {"eio", "oi"},    {"eo", "ei"},     {"eu", "ui"},     {"ea", "ai"},
    {"jo", "onn"},    {"iuo", "yuu"},   {"iao", "you"},   {"uo", "uu"},     {"oa", "ou"},
    {"a-", "au"},     {"av", "ann"},    {"iv", "inn"},    {"uv", "unn"},    {"ev", "enn"},
    {"ov", "onn"},    {"iav", "yann"},  {"iuv", "yunn"},  {"iev", "yenn"},  {"iov", "yonn"},
    {"ax", "aku"},    {"ix", "iku"},    {"ux", "uku"},    {"axo", "eku"},   {"ox", "oku"},
    {"iax", "yaku"},  {"iux", "yuku"},  {"iaox", "yeku"}, {"iox", "yoku"},  {"a,", "atu"},
    {"i,", "itu"},    {"u,", "utu"},    {"e,", "etu"},    {"o,", "otu"},    {"ia,", "yatu"},
    {"iu,", "yutu"},  {"ie,", "yetu"},  {"io,", "yotu"},  {"aj", "aki"},    {"ic", "iki"},
    {"uj", "uki"},    {"ec", "eki"},    {"oc", "oki"},    {"al", "ati"},    {"ioae", "iti"},
    {"ul", "uti"},    {"el", "eti"},    {"ol", "oti"},
};

// The romaji of the vowel chord whose keys spell s, sorted, or NULL.
static const char *vowel_chord(const char *s) {
    for (size_t i = 0; i < sizeof(vowel_chords) / sizeof(vowel_chords[0]); i++) {
        char k[8];
        strcpy(k, vowel_chords[i][0]);
        sort_chars(k);
        if (!strcmp(k, s)) return vowel_chords[i][1];
    }
    return NULL;
}

static bool in_vowel_chord(uint8_t p) {
    char c = onchoku_letter[p];
    return c && strchr("aiueo-,xjcvl", c);
}

// The gairaion spelt by queue[from] to queue[end - 1], one or two consonant
// keys and then a vowel chord, written to out. False when they spell none,
// or a plain kana such as ka.
static bool gairai_of(uint8_t from, uint8_t end, char *out) {
    char    cons[3], vow[8];
    uint8_t nc = 0, nv = 0;
    for (uint8_t i = from; i < end; i++) {
        uint8_t p = queue[i];
        if (!nv && !is_vowel(onchoku_letter[p])) {
            if (!letter(p) || nc == 2) return false;
            cons[nc++] = letter(p);
            continue;
        }
        if (!in_vowel_chord(p) || nv == sizeof(vow) - 1) return false;
        vow[nv++] = onchoku_letter[p];
    }
    if (!nc || !nv) return false;
    cons[nc] = 0;
    vow[nv]  = 0;
    sort_chars(cons);
    sort_chars(vow);
    const char *cl = nc == 1 ? cons : NULL;
    for (size_t i = 0; nc == 2 && i < sizeof(clusters) / sizeof(clusters[0]); i++) {
        if (!strcmp(clusters[i][0], cons)) cl = clusters[i][1];
    }
    const char *v = vowel_chord(vow);
    if (!cl || !v) return false;
    // The first kana, as ふぁ in ふぁん, must be a gairaion.
    char   head[6];
    size_t hl = strlen(cl);
    strcpy(head, cl);
    head[hl]     = v[0];
    head[hl + 1] = v[0] == 'y' ? v[1] : 0;
    head[hl + 2] = 0;
    bool ok      = nc == 1 && v[0] == 'y';
    for (size_t g = 0; !ok && g < sizeof(gairai) / sizeof(gairai[0]); g++) ok = !strcmp(gairai[g], head);
    if (!ok) return false;
    strcpy(out, cl);
    strcat(out, v);
    return true;
}

// The chord after P is open from its first vowel until one of its keys is
// released, a key outside it goes down, or ng_mark_gap_ms passes after its
// last key.
static bool     grp;
static uint16_t grp_since;

// Where the keys after P start in the queue, or -1 when P is not the last
// prefix before them. P waiting in the queue counts when nothing before it
// is a prefix that would change it.
static int8_t after_p(void) {
    if (prefix == PRE_P) return 0;
    int8_t ip = queue_index(P_P);
    if (ip < 0 || prefix != PRE_NONE) return -1;
    for (int8_t i = 0; i < ip; i++) {
        if (is_prefix(queue[i])) return -1;
    }
    return ip + 1;
}

uint16_t ng_mark_gap_ms = 50;

// The word chords of TK on-choku: their consonant keys by letter in any order,
// and the romaji.
static const char *const word_chords[][2] = {
    {"ds", "desu"}, {"ms", "masu"},   {"st", "site"},     {"sr", "suru"},   {"kr", "kara"}, {"th", "toha"},
    {"nd", "nado"}, {"dh", "deha"},   {"nh", "niha"},     {"sth", "siteha"}, {"mst", "masita"},
    {"tn", "koto"}, {"dsn", "desita"}, {"dm", "mono"},    {"sk", "sika"},   {"tm", "tame"}, {"stn", "sita"},
};

// The word typed by the consonant keys queue[from] to queue[end - 1], all
// pressed within ng_mark_gap_ms of the first, as a combo of TK on-choku.
static const char *word_of(uint8_t from, uint8_t end) {
    char    w[4];
    uint8_t n = 0;
    if (end - from < 2 || end - from > 3) return NULL;
    for (uint8_t i = from; i < end; i++) {
        uint8_t p = queue[i];
        if (!letter(p) || is_vowel(letter(p))) return NULL;
        if ((uint16_t)(keys[p].t_down - keys[queue[from]].t_down) > ng_mark_gap_ms) return NULL;
        w[n++] = letter(p);
    }
    w[n] = 0;
    sort_chars(w);
    for (size_t i = 0; i < sizeof(word_chords) / sizeof(word_chords[0]); i++) {
        char k[4];
        strcpy(k, word_chords[i][0]);
        sort_chars(k);
        if (!strcmp(k, w)) return word_chords[i][1];
    }
    return NULL;
}

// Close the chord over the keys before queue index end: send the gairaion
// they spell, or leave them to be decided one by one.
static void close_group(uint8_t end) {
    grp         = false;
    int8_t from = after_p();
    if (from < 0 || from >= end) return;
    char        r[12];
    const char *w = word_of((uint8_t)from, end);
    if (w) {
        strcpy(r, w);
    } else if (!gairai_of((uint8_t)from, end, r)) {
        return;
    }
    if (prefix != PRE_P) {
        // Send the keys before P and take P as the prefix.
        uint8_t before = qlen;
        run_queue(P_P);
        if (prefix != PRE_P) return;
        end -= before - qlen;
        from = 0;
    }
    for (uint8_t n = end - (uint8_t)from; n; n--) {
        queue_drop((uint8_t)from);
        strokes++;
    }
    prefix = PRE_NONE;
    send(r, NULL);
}

static uint8_t group_len(void) {
    int8_t from = after_p();
    return from < 0 ? 0 : qlen - (uint8_t)from;
}

// The key at the end of the queue went down: open the chord after P when it
// is a vowel after held consonant keys or a second consonant key held with
// the first, or close the chord when the key is no part of it.
static void press_group(uint16_t time) {
    uint8_t p = queue[qlen - 1];
    if (grp) {
        bool vowel = false;
        for (int8_t i = after_p(); i >= 0 && i < qlen - 1; i++) vowel |= is_vowel(onchoku_letter[queue[i]]);
        if (in_vowel_chord(p) || (!vowel && letter(p) && group_len() <= 3)) {
            grp_since = time;
        } else {
            close_group(qlen - 1);
        }
        return;
    }
    int8_t from = after_p();
    if (from < 0) return;
    uint8_t nc = qlen - 1 - (uint8_t)from;
    for (uint8_t i = (uint8_t)from; i < qlen - 1; i++) {
        char c = letter(queue[i]);
        if (!c || is_vowel(c) || !keys[queue[i]].down) return;
    }
    if (is_vowel(onchoku_letter[p]) ? nc < 1 || nc > 2 : nc != 1 || !letter(p)) return;
    grp       = true;
    grp_since = time;
}


// ! or ? for keys a and b that went down within ng_mark_gap_ms of each other.
static const char *mark_of(uint8_t a, uint8_t b) {
    uint16_t d = keys[a].t_down - keys[b].t_down;
    if ((uint16_t)(keys[b].t_down - keys[a].t_down) < d) d = keys[b].t_down - keys[a].t_down;
    if (d > ng_mark_gap_ms) return NULL;
    if ((a == P_CM && b == P_DT) || (a == P_DT && b == P_CM)) return "!";
    if ((a == P_DT && b == P_SL) || (a == P_SL && b == P_DT)) return "?";
    return NULL;
}

// Decide the queue up to and including pos, or all of it when pos is NG_KEYS.
static void run_queue(uint8_t pos) {
    while (qlen) {
        const char *mark = qlen >= 2 && prefix == PRE_NONE ? mark_of(queue[0], queue[1]) : NULL;
        if (mark) {
            uint8_t a = queue[0], b = queue[1];
            queue_drop(0);
            queue_drop(0);
            bracket_prefix = 0;
            strokes += 2;
            send(mark, NULL);
            if (a == pos || b == pos) return;
            continue;
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
    sent_in_call     = false;
    press_group(time);
}

void sq_tick(uint16_t time) {
    if (!grp || (uint16_t)(time - grp_since) <= ng_mark_gap_ms) return;
    sent_in_call = false;
    close_group(qlen);
}

void sq_release(uint8_t pos, uint16_t time) {
    (void)time;
    if (pos >= NG_KEYS) return;
    keys[pos].down = false;
    sent_in_call   = false;
    if (grp) {
        // A consonant key let go after the vowel leaves the gairaion open.
        int8_t from = after_p(), i = queue_index(pos);
        bool   held = from >= 0 && i >= from;
        bool   vowel = false;
        for (int8_t j = from; held && j < qlen; j++) {
            if (!is_vowel(onchoku_letter[queue[j]])) continue;
            vowel = true;
            if (j <= i) held = false;
            break;
        }
        if (held && vowel) return;
        close_group(qlen);
    }
    if (keys[pos].live) run_queue(pos);
}

void sq_flush(uint16_t time) {
    (void)time;
    sent_in_call = false;
    if (grp) close_group(qlen);
    run_queue(NG_KEYS);
    prefix = PRE_NONE;
    drop_strokes(0);
    last = NULL;
}

bool sq_drop_prefix(uint16_t time) {
    (void)time;
    sent_in_call = false;
    grp          = false;
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

bool sq_chord_pending(void) { return chord_pending(); }

bool sq_can_chord(uint8_t a, uint8_t b) { return a < NG_KEYS && b < NG_KEYS && a != b && chord_pending(); }

bool sq_mark_chord(uint8_t a, uint8_t b, uint16_t time) {
    (void)time;
    if (a >= NG_KEYS || b >= NG_KEYS || !keys[a].live || !keys[b].live) return false;
    return sq_can_chord(a, b) || mark_of(a, b);
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
    grp            = false;
}

char sq_take_bracket_prefix(void) {
    char c         = bracket_prefix;
    bracket_prefix = 0;
    if (prefix == PRE_TOKU) prefix = PRE_NONE;
    return c;
}

/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * factory patterns (SAVE > PHRASES) and the parts' sounds at power-on. */
#include "dsp.c"

/* The parts' engine memory (1.5): a part plays one engine at a time, so the engines with per-part state
 * (engine_t.mem: DRUM's lanes, FM6's notes, FMTONE's, SWARM's, WAVETONE's and FMDRUM's slots) keep it in one region
 * per part instead of an array each. The region holds the state of the engine that last claimed it (eng_mem_owner);
 * a switch to another such engine zeroes its engine_t.mem bytes, which is the state each had at power-on.
 * Sized by the largest, DRUM (8 lanes); an engine that needs more than ENG_MEM_PART grows it for every part (x NPART of
 * the pool). Zero-initialised pool section: main.c fm1_cstart clears it. (Up to 1.5 it was 12888 bytes, PHYS's, and
 * GRAIN's 7112: those engines are gone.) */
#define ENG_MEM_PART 2048u
static union {
    uint64_t align;
    uint8_t b[ENG_MEM_PART];
} eng_mem[NPART] __attribute__((section(".pool")));
static uint8_t eng_mem_owner[NPART];       /* engine index + 1 whose state is in the region, 0 = none (zeros) */
#define ENG_MEM(p) ((void *)eng_mem[p].b)  /* part p's region (p < NPART) */
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= ENG_MEM_PART, name " state over ENG_MEM_PART")

/* The DRUM engine is gone; what the sequencer and the UI still read of it: a step's lane hits (step_t.hit) play these GM
 * notes on any engine, the lanes' names, and the EDIT layer's lane mutes (none now). */
static uint8_t drum_mute[NPART];
static const uint8_t DRUM_LANE_NOTE[NLANE] = {36, 38, 39, 42, 46, 45, 37, 56};
/* the lane a GM note strikes (35 .. 81; a note outside it: its pitch class on the lanes) */
static uint32_t drum_lane(uint32_t note)
{
    static const uint8_t L[47] = {0, 0, 6, 1, 2, 1, 5, 3, 5, 3, 5, 4, 5, 5, 7, 5, 7, 7, 7, 7, 5, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                                  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};   /* (35 .. 81) */
    return note >= 35u && note <= 81u ? L[note - 35u] : (note % 12u) & (NLANE - 1u);
}
/* the lanes step s strikes: its hits, and its notes on their lanes (a NOTE step only) */
static uint32_t step_lanes(const step_t *s)
{
    uint32_t m = 0, i;
    if (s->time != ST_NOTE)
        return 0;
    for (i = 0; i < s->n && i < 4u; i++)
        m |= 1u << drum_lane(s->note[i]);
    return m | s->hit;
}
static uint32_t step_accents(const step_t *s) { return s->flags & SF_ACCENT ? step_lanes(s) : s->acc & step_lanes(s); }
/* a step's notes that are a lane's note become that lane's hits (the grid's edits) */
static void step_to_grid(step_t *s)
{
    uint32_t i;
    if (s->time != ST_NOTE)
        return;
    for (i = s->n < 4u ? s->n : 4u; i-- > 0;) {
        uint32_t l = drum_lane(s->note[i]);
        if (s->note[i] == DRUM_LANE_NOTE[l]) {
            s->hit |= (uint8_t)(1u << l);
            step_drop_note(s, i);
        }
    }
    if (!s->n && (s->flags & SF_ACCENT)) {
        s->acc |= s->hit;
        s->flags &= (uint8_t)~SF_ACCENT;
    }
}
static const char *drum_lane_name(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL"};
    (void)t;
    return N[l & (NLANE - 1u)];
}
static const char *drum_lane_abbr(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    (void)t;
    return N[l & (NLANE - 1u)];
}

#if FELUCCA_TEST_ENGINE
#include "eng_testsynth.c"      /* TESTSYNTH: a saw voice, for the host tests */
#endif
#if FELUCCA_TR808
#include "eng_tr808.c"          /* TR808: a TR-808 of 16 voices after the 8W8 module's circuit models (GPL-3.0), in fixed point */
#endif

/* The engines that are removed: every one of Mini-Tone's but TR808 (24). The stores name engines by index and the table is
 * append-only, so their numbers stay: a sound that names one loads with its values as they were and plays silence
 * (ENG_GONE), and the engine is never offered (eng_ok). */
#define ENG_REMOVED_MASK (((1u << 24) - 1u) & ~((uint32_t)FELUCCA_TEST_ENGINE << 12) & ~((uint32_t)FELUCCA_TEST_ENGINE << 10))
static void gone_note_on(track_t *t, voice_t *v) { (void)t; (void)v; }
static void gone_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
#define GONE_E {"-", F_INT, -128, 127, 0, 0, 0}
static const engine_t ENG_GONE = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {GONE_E, GONE_E, GONE_E, GONE_E, GONE_E, GONE_E, GONE_E, GONE_E},
    .note_on = gone_note_on,
    .render = gone_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE,   /* 0..5 */
    &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE,
#if FELUCCA_TEST_ENGINE
    &ENG_TESTDRUM,                                                 /* 10 (ENGI_DRUM: the host tests' grid engine) */
#else
    &ENG_GONE,
#endif
    &ENG_GONE,   /* 6..11 */
#if FELUCCA_TEST_ENGINE
    &ENG_TESTSYNTH, &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE,   /* 12 (the host tests' melodic engine), 13..17 */
#else
    &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE,   /* 12..17 */
#endif
    &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE,   /* 18..23 */
    &ENG_TR808,                  /* 24 (ENGI_TR808) */
};

/* the engine's name as the knob column shows it (about 6 characters: the column's small font ellipsises a longer one);
 * the protocol and the editor keep engine_t.name */
static const char *eng_label(const engine_t *e)
{
    return e->name;
}

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* part t's engine memory goes to its engine (t->engine; voice.c engine_block, once a block: on a switch, after
 * the old engine's fade, before any note on the new one): zeroed (its engine_t.mem bytes) when another
 * engine's state is in it. An engine without memory leaves it as it is: a switch back finds its state. */
static void eng_mem_claim(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), e, k, n;
    uint32_t *w;
    if (p >= NPART)
        return;
    e = eng_idx(t->engine);
    n = ENGINES[e]->mem;
    if (!n || eng_mem_owner[p] == e + 1u)
        return;
    w = (uint32_t *)ENG_MEM(p);
    for (k = 0; k < (n + 3u) / 4u; k++)
        w[k] = 0;
    eng_mem_owner[p] = (uint8_t)(e + 1u);
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with FELUCCA_FM4 it follows FM6). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
#if FELUCCA_TEST_ENGINE
    12,                          /* TESTSYNTH */
    10,                          /* TESTDRUM */
#endif
    24,                          /* TR808 */
};

/* the engines one can pick (engine 1 only with FELUCCA_FM4), in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e) { return e < NENGINES && (FELUCCA_FM4 || e != ENGI_DIGITAL) && !((ENG_REMOVED_MASK >> e) & 1u); }
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = ENGI_TR808;
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* factory sequence patterns: SAVE > PHRASES loads one into the selected track (ui.c pat_load); a preset
 * only suggests one with PAT(n), loading a sound never touches the steps. Absolute notes, loaded as they
 * are (DRUM plays them as GM drums, SLICE as slices; SCL TRANS and OCT transpose): 0 = rest;
 * flags 1 = accent, 2 = slide, 4 = tie (holds the previous note). Names: at most 8 characters */
#define T_ 4
static const struct {
    const char *name;
    uint8_t note[16], flags[16];
} PATTERNS[] = {
    {"ACID", {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50},          /* 1 */
     {1, 0, 2, 0, 0, 0, 1, 2, 0, 0, 1, 0, 0, 2, 0, 1}},
    {"OFFBEAT", {0, 36, 0, 36, 0, 36, 0, 48, 0, 36, 0, 36, 0, 39, 0, 43},          /* 2 bass */
     {0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}},
    {"MELODY", {60, 0, 67, 0, 72, 67, 0, 64, 62, 0, 69, 0, 74, 69, 0, 67},         /* 3 pluck */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    {"LEAD", {72, 0, 0, 74, 0, 0, 76, 0, 79, 0, 76, 0, 74, 0, 0, 0},               /* 4 */
     {1, T_, 0, 2, T_, 0, 0, 0, 1, 0, 2, 0, 0, T_, T_, 0}},
    {"PAD", {60, 0, 0, 0, 0, 0, 0, 0, 57, 0, 0, 0, 55, 0, 0, 0},                   /* 5 long notes */
     {0, T_, T_, T_, T_, T_, T_, 0, 0, T_, T_, 0, 0, T_, T_, 0}},
    {"KEYS", {0, 0, 60, 0, 0, 63, 0, 0, 0, 0, 60, 0, 0, 65, 0, 63},                /* 6 offbeat stabs */
     {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}},
    {"BELL", {72, 0, 0, 79, 0, 0, 84, 0, 0, 0, 76, 0, 0, 0, 0, 0},                 /* 7 sparse */
     {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"SUB", {36, 0, 0, 0, 0, 0, 0, 36, 0, 0, 34, 0, 0, 0, 0, 0},                   /* 8 low, held */
     {1, T_, T_, T_, 0, 0, 0, 0, 0, 0, 0, T_, T_, T_, 0, 0}},
    /* SLICE (eng_slice.c): note = C4 + slice */
    {"CHOP", {60, 61, 62, 67, 64, 65, 60, 69, 68, 70, 62, 67, 72, 72, 74, 64},     /* 9 16 slices re-ordered */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"STUTTER", {60, 60, 61, 61, 62, 0, 63, 63, 64, 65, 65, 0, 66, 66, 66, 67},    /* 10 8 slices, repeats */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"SLICES", {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75},   /* 11 in order */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* DRUM (General MIDI: 36 kick, 38 snare, 42 closed / 46 open hi-hat) */
    {"BEAT", {36, 42, 42, 42, 38, 42, 36, 42, 36, 42, 42, 36, 38, 42, 46, 42},     /* 12 */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* 16ths up a C minor arpeggio, twice: the line the ARP presets (RAVE, ARP 8BIT, ARP LEAD) suggest
     * now that the arpeggiator is the track's and a preset no longer switches it on */
    {"ARP", {48, 51, 55, 60, 63, 67, 72, 75, 48, 51, 55, 60, 63, 67, 72, 75},       /* 13 */
     {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}},
};
#undef T_
#define NPATTERNS (sizeof PATTERNS / sizeof PATTERNS[0])

/* the parts at power-on (engine, preset, PATTERNS[n - 1] in the sequencer, 0 = empty: all are): bass, pad, lead, drums */
#if FELUCCA_TEST_ENGINE
static const uint8_t TRK_DEF[NPART][3] = {{ENGI_TESTSYNTH, 2, 0}, {ENGI_TESTSYNTH, 4, 0}, {ENGI_TESTSYNTH, 3, 0}, {ENGI_DRUM, 0, 0}, {ENGI_TESTSYNTH, 0, 0}, {ENGI_TESTSYNTH, 1, 0}};
#else
static const uint8_t TRK_DEF[NPART][3] = {{ENGI_TR808, 0, 0}, {ENGI_TR808, 0, 0}, {ENGI_TR808, 0, 0}, {ENGI_TR808, 0, 0}, {ENGI_TR808, 0, 0}, {ENGI_TR808, 0, 0}};
#endif
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }

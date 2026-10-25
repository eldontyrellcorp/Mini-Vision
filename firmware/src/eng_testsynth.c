/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Stephane Schmitz */
/* TESTSYNTH: a saw voice with the track's ADSR, for the HOST TESTS only (FELUCCA_TEST_ENGINE=1: tests/run_tests.sh); the firmware
 * does not build it. mini-vision has only TR808, an engine of hits (always POLY, keys on the General MIDI map, no key-off), and
 * the sequencer, the keyboard, the voice modes, the chords and the mix tests need a melodic engine to mean anything. It takes
 * the number of Mini-Tone's FM6 (12) and goes the day the wavetable engine can do its job.
 *
 * EDIT 1: DET  a second saw, detuned in 1/16 semitones;  PW, AUX, AUX2: X5..X8: values the tests edit, lock and modulate (they sound nothing). */
#define ENGI_TESTSYNTH 12u
static void ts_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = 0;
    v->ph[1] = 0x40000000u;
}
static void ts_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t i, inc = pitch_inc(clamp(m->pitch16, 0, 2047)), inc2 = pitch_inc(clamp(m->pitch16 + t->p[P_E0], 0, 2047));
    uint32_t p0 = v->ph[0], p1 = v->ph[1];
    for (i = 0; i < n; i++) {
        int32_t s = ((int32_t)(p0 >> 18) - 8192) + ((int32_t)(p1 >> 18) - 8192);
        p0 += inc;
        p1 += inc2;
        out[i] += voice_amp(s, m, i);
    }
    v->ph[0] = p0;
    v->ph[1] = p1;
}
/* the presets FM6 had, in its order and with its categories (K U B L P U K U): the host tests name them by number */
static const preset_t TS_PRESETS[] = {
    {"TEST EP", {0, 64, 0, 0, 0, 0, 0, 0}, {0, 60, 100, 40}, 0, 0, FX(0, 0, 0, 0), PAT(6)},
    {"TEST BELL", {3, 64, 0, 0, 0, 0, 0, 0}, {0, 60, 40, 60}, 0, 0, FX(0, 0, 0, 0), PAT(7)},
    {"TEST BASS", {0, 64, 0, 0, 0, 0, 0, 0}, {0, 60, 100, 40}, 0, 1, FX(0, 0, 0, 0), PAT(2)},
    {"TEST BRASS", {7, 64, 0, 0, 0, 0, 0, 0}, {10, 60, 80, 60}, 0, 0, FX(0, 0, 0, 0), PAT(4)},
    {"TEST PAD", {5, 64, 0, 0, 0, 0, 0, 0}, {40, 60, 100, 80}, 0, 0, FX(0, 0, 0, 0), PAT(5)},
    {"TEST MARIMBA", {0, 64, 0, 0, 0, 0, 0, 0}, {0, 40, 0, 20}, 0, 0, FX(0, 0, 0, 0), PAT(3)},
    {"TEST ORGAN", {2, 64, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 10}, 0, 0, FX(0, 0, 0, 0), PAT(6)},
    {"PLUCK", {2, 64, 0, 0, 0, 0, 0, 0}, {0, 40, 0, 20}, 0, 0, FX(0, 0, 0, 0), PAT(13)},
};
static const engine_t ENG_TESTSYNTH = {
    .name = "TESTSYNTH",
    .page_title = {"EDIT 1", "EDIT 2"},
    .edit = {
        {"DET", F_INT, 0, 32, 0, 0, 0},
        {"PW", F_PCT, 0, 127, 64, 0, 0}, {"AUX", F_PCT, 0, 127, 0, 0, 0}, {"AUX2", F_INT, -24, 24, 0, 0, 0},
        {"X5", F_PCT, 0, 127, 0, 0, 0}, {"X6", F_PCT, 0, 127, 0, 0, 0}, {"X7", F_PCT, 0, 127, 0, 0, 0}, {"X8", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = TS_PRESETS,
    .npresets = NELEM(TS_PRESETS),
    .note_on = ts_note_on,
    .render = ts_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 0,                   /* all of the budget of voices */
};

/* TESTDRUM: Mini-Tone's DRUM engine reduced to what the host tests need of it: the engine the DRUM GRID (seq.c drum_track) is for, an
 * engine of hits (oneshot: always POLY), engine number 10 (ENGI_DRUM) and the eight lanes' levels on the LANES pages. A click per hit. */
static void td_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = 0;
    v->s[0] = 1 << 24;
}
static void td_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t i, inc = pitch_inc(clamp(m->pitch16, 0, 2047));
    uint32_t p0 = v->ph[0];
    int32_t e = v->s[0];
    (void)t;
    for (i = 0; i < n; i++) {
        out[i] += voice_amp(mulq15(((int32_t)(p0 >> 17) - 16384), e >> 9), m, i);
        p0 += inc;
        e -= e >> 10;
    }
    v->ph[0] = p0;
    v->s[0] = e;
}
/* the keys: the General MIDI drum map from the first C, as DRUM's (and TR808's) */
static int32_t td_keys(const track_t *t, uint32_t k)
{
    (void)t;
    return clamp(29 + 12 * song.octave + (int32_t)k, 0, 127);
}
static const preset_t TD_PRESETS[] = {
    {"DRUM KIT", {0, 64, 70, 64, 64, 100, 0, 0}, {0, 60, 100, 40}, 0, 0, FX(0, 0, 0, 0), PAT(12)},
};
static const engine_t ENG_TESTDRUM = {
    .name = "DRUM",
    .page_title = {"EDIT 1", "EDIT 2"},
    .edit = {
        {"KIT", F_INT, 0, 8, 0, 0, 0}, {"TUNE", F_PCT, 0, 127, 64, 0, 0}, {"TONE", F_PCT, 0, 127, 70, 0, 0}, {"DECY", F_PCT, 0, 127, 64, 0, 0},
        {"SNAP", F_PCT, 0, 127, 64, 0, 0}, {"ACC", F_PCT, 0, 127, 100, 0, 0}, {"KICK", F_INT, 0, 1, 0, 0, 0}, {"DRV", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = TD_PRESETS,
    .npresets = NELEM(TD_PRESETS),
    .note_on = td_note_on,
    .render = td_render,
    .knob = {P_E1, P_E2, P_E3, P_E4},
    .poly = 0,
    .oneshot = 1,
    .keys = td_keys,
};

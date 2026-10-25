/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Stephane Schmitz */
/* WTSYNTH: mini-vision's wavetable voice, after the structure of Roland's n/vision (docs/NVISION_SPECS.md; its sounds and wavetables
 * are not Roland's: tools/gen_nvision_wt.py makes 63 tables of 8 key frames, each frame band limited at 5 mip levels).
 *
 * A voice has two wavetable layers. Each reads its table (WAV1, WAV2: 1 .. 63) at the same POSITION (0 .. 127, which morphs
 * through the table's key frames), the layers are crossfaded by MIX, the second one is OCT2 octaves under the first (the first
 * is RANGE: 16' 8' 4' 2'). The result goes through a wave shaper (WS: a sine fold, from the signal as it is to one that wraps
 * four times), then the resonant voice filter (dsp.c rsvf: TYPE LP / BP / HP, CUT, RES, KEY), then the track's amplifier envelope.
 * PM is phase modulation: layer 2 modulates layer 1's phase.
 *
 * EDIT 1 (WAVE):  WAV1  WAV2  POS  MIX
 * EDIT 2 (SHAPE): OCT2  RANGE WS   PM
 * OSC ENV:        A D (the oscillator's own attack / decay envelope), TO POS and TO MIX (how far it moves the position and the mix)
 * FILTER:         TYPE CUT RES KEY
 * The track's ENV page (ATK DEC SUS REL) is the amplifier; its LFO and ENV pages reach the pitch, the filter, the position
 * (SHAPE: LFO -> SHP, ENV -> SHP, vmod_t.shape) and the amplitude as for every engine; the modulation matrix has E1 .. E8.
 *
 * Voice state: ph[0], ph[1] the layers' phases; s[0] the position of the last block (Q8, -1 before the first), s[1] the oscillator
 * envelope (Q24), s[2] its stage (0 attack, 1 decay), s[4], s[5] the filter. */
#include "nvision_wt.h"

static const char *const N_WT_RANGE[] = {"16'", "8'", "4'", "2'"};   /* P_E5: the octave of the layers (16' is an octave under 8') */
#define ENGI_WT 13u              /* engines.c ENGINES[]: below FAV_ENGINES, so its presets can be starred */
#define WT_POS_MAX (127 * 256)   /* the position, Q8 */

/* the voice filter on the operator slots (engine_t.tdesc), and the oscillator envelope. The slots' defaults are TP's (params.c FMOP) */
#define WT_CUT P_FM1_SUS
#define WT_RES P_FM1_REL
#define WT_KEY P_FM1_LEVEL
#define WT_OA P_FM2_ATK
#define WT_OD P_FM2_DEC
#define WT_TPOS P_FM2_REL        /* -63 .. 63 */
#define WT_TMIX P_FM3_REL
static const param_desc_t WT_TD[P_FM4_LEVEL - P_FM1_ATK + 1] = {
    [WT_CUT - P_FM1_ATK] = {"CUT", F_CUTOFF, 0, 127, 127, 0, 0},
    [WT_RES - P_FM1_ATK] = {"RES", F_PCT, 0, 127, 0, 0, 0},
    [WT_KEY - P_FM1_ATK] = {"KEY", F_PCT, 0, 127, 127, 0, 0},
    [WT_OA - P_FM1_ATK] = {"A", F_TIME, 0, 127, 0, 0, 0},
    [WT_OD - P_FM1_ATK] = {"D", F_TIME, 0, 127, 0, 0, 0},
    [WT_TPOS - P_FM1_ATK] = {"TO POS", F_INT, -63, 63, 0, 0, 0},
    [WT_TMIX - P_FM1_ATK] = {"TO MIX", F_INT, -63, 63, 0, 0, 0},
};
static const param_desc_t *wt_tdesc(uint32_t id)
{
    const param_desc_t *d = &WT_TD[id - P_FM1_ATK];
    return d->label ? d : 0;
}

/* the level of a table for a note: the first whose harmonics (NV_CAP) stay under 0.9 of Nyquist at the layer's increment */
static const uint32_t WT_INC_MAX[NV_NLEV] = {30198988u, 60397977u, 120795955u, 241591910u, 0xFFFFFFFFu};   /* 0.45 x 2^32 / cap */
static inline uint32_t wt_level(uint32_t inc)
{
    uint32_t k = 0;
    while (k < NV_NLEV - 1u && inc > WT_INC_MAX[k])
        k++;
    return k;
}

static void wt_note_on(track_t *t, voice_t *v)
{
    v->ph[0] = v->ph[1] = 0;
    v->s[0] = -1;
    v->s[1] = 0;
    v->s[2] = 0;
    v->s[4] = 0;                                          /* the voice filter (a retrigger keeps it: engine_t.keep) */
    v->s[5] = rsvf_seed(t->p[WT_RES]);
}

/* a layer's sample at phase ph: its table's frames f0, f1 (n + 1 samples each, the guard point last), pf (0 .. 256) the way from one
 * to the other, lg = log2 n; Q15 */
static inline int32_t wt_read(const int8_t *f0, const int8_t *f1, uint32_t ph, uint32_t lg, int32_t pf)
{
    uint32_t i = ph >> (32u - lg), fr = (ph >> (24u - lg)) & 255u;
    int32_t a0 = f0[i], b0 = f1[i], a = a0 * 256 + (f0[i + 1u] - a0) * (int32_t)fr, b = b0 * 256 + (f1[i + 1u] - b0) * (int32_t)fr;
    return a + (((b - a) * pf) >> 8);
}

static void wt_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, inc1, inc2, lv1, lv2, lg1, lg2, st1, st2;
    int32_t pos0 = v->s[0], pos1, dpos, mixq, wsd = clamp(p[P_E6], 0, 127), pm = clamp(p[P_E7], 0, 127);
    int32_t env = v->s[1], tp = clamp(p[WT_TPOS], -63, 63), tm = clamp(p[WT_TMIX], -63, 63);
    int32_t fcut = clamp(p[WT_CUT], 0, 127), fres = clamp(p[WT_RES], 0, 127), fi1 = v->s[4], fi2 = v->s[5];
    uint32_t fty = (uint32_t)p[P_FTYPE] % 3u, fon = fcut < 127 || fres > 0 || fty, ph0 = v->ph[0], ph1 = v->ph[1], rng, oct2;
    const int8_t *b1, *b2;
    rsvf_t rf;
    if (!n)
        return;
    /* the oscillator envelope, once a block: attack to 1 (the way ENV_EXP[A] says), then decay to 0 */
    if (v->s[2] == 0) {
        if (!p[WT_OA]) {
            env = 1 << 24;
            v->s[2] = 1;
        } else {
            env += mulq16((1 << 24) - env, ENV_EXP[clamp(p[WT_OA], 0, 127)]);
            if (env > (1 << 24) - (1 << 17))
                v->s[2] = 1;
        }
    } else {
        env -= mulq16(env, ENV_EXP[clamp(p[WT_OD], 0, 127)]);
    }
    /* the position (Q8): the knob, the LFO / ENV -> SHP (vmod_t.shape, centred on 64 << 8), the oscillator envelope */
    pos1 = clamp(p[P_E2] * 256 + (m->shape - (64 << 8)) + (((env >> 9) * tp * 516) >> 15), 0, WT_POS_MAX);
    if (pos0 < 0)
        pos0 = pos1;
    dpos = (pos1 - pos0) / (int32_t)n;
    mixq = clamp(p[P_E3] + (((env >> 9) * tm * 2) >> 15), 0, 127);       /* MIX, the oscillator envelope added */
    mixq = (mixq * 256 + 63) / 127;                                      /* layer 2's share, 0 .. 256 */
    rng = (uint32_t)clamp(p[P_E5], 0, 3);
    oct2 = (uint32_t)clamp(-p[P_E4], 0, 2);
    inc1 = rng == 0u ? m->inc >> 1 : m->inc << (rng - 1u);
    inc2 = inc1 >> oct2;
    lv1 = wt_level(inc1);
    lv2 = wt_level(inc2);
    lg1 = NV_LEN_LOG2[lv1];
    lg2 = NV_LEN_LOG2[lv2];
    st1 = (1u << lg1) + 1u;
    st2 = (1u << lg2) + 1u;
    b1 = NV_TAB[lv1] + (uint32_t)(clamp(p[P_E0], 1, 63) - 1) * NV_NFRAME * st1;
    b2 = NV_TAB[lv2] + (uint32_t)(clamp(p[P_E1], 1, 63) - 1) * NV_NFRAME * st2;
    if (fon)
        rsvf_coef(&rf, rsvf_key(fcut, m->cutoff, clamp(p[WT_KEY], 0, 127), m->pitch16), fres);
    for (i = 0; i < n; i++) {
        int32_t pos = pos0 + dpos * (int32_t)i, fq = (int32_t)(((int64_t)pos * 3613) >> 16), pf, l1, l2, s;
        uint32_t f = (uint32_t)fq >> 8;
        if (f >= NV_NFRAME - 1u) {
            f = NV_NFRAME - 2u;
            pf = 256;
        } else {
            pf = fq & 255;
        }
        l2 = wt_read(b2 + f * st2, b2 + (f + 1u) * st2, ph1, lg2, pf);
        l1 = wt_read(b1 + f * st1, b1 + (f + 1u) * st1, pm ? ph0 + (uint32_t)((int64_t)l2 * pm * 1024) : ph0, lg1, pf);
        s = (l1 * (256 - mixq) + l2 * mixq) >> 8;
        if (wsd) {                                                   /* the sine fold: 1x .. 4x the signal into a quarter-wave sine */
            int32_t u = (s * (256 + wsd * 6)) >> 8, fold = sine_i((uint32_t)u << 15);
            s += ((fold - s) * wsd) >> 7;
        }
        if (fon)
            s = soft_knee(rsvf_run(&rf, s >> 1, &fi1, &fi2, fty), 16000) << 1;
        out[i] += voice_amp(s, m, i);
        ph0 += inc1;
        ph1 += inc2;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = pos1;
    v->s[1] = env;
    v->s[4] = fi1;
    v->s[5] = fi2;
}

/* name, {WAV1 WAV2 POS MIX OCT2 RANGE WS PM}, {A D S R}, fenv, mono. The wavetables: 1 .. 12 GEO (Sine>Saw, Sine>Square, Tri>Square,
 * Saw>Pulse, Sine Drive, Saw Clip, PWM Sweep, Tri Fold, Skew Sine, Saw Opening, Rect Sine, Staircase), 13 .. 22 VOW (Vowels AEIOU, UOIEA,
 * Bass Voice, Child Voice, Choir Ooh-Aah, Robo Vox, Nasal, Growl, Whistle, Diphthong), 23 .. 36 MET (FM 1:1, 1:2, 1:3, 2:5 Bell, Stack,
 * Tubular Bell, Glass, Gong, Ring Mod, Clang, Hollow Pipe, Crystal, Digital Bell, Chime), 37 .. 50 SPC (Deep Space, Nebula, Pulsar, Quasar,
 * Orbit, Event Horizon, Dark Matter, Comet, Redshift, Supernova, Binary Star, Cosmic Dust, Gravity Well, Aurora), 51 .. 63 LOF (Grit,
 * Bit Crush, Rate Crush, Static, Tape Hiss, Crackle, Rust, Dirt, Radio, Chip 8-bit, Gravel, Hum, Glitch) */
static const preset_t WT_PRESETS[] = {
    {"SAW SWEEP", {1, 4, 20, 0, 0, 1, 0, 0}, {4, 70, 110, 40}, 24, 0, FX(0, 12, 30, 25), PAT(4)},
    {"VOWEL PAD", {13, 17, 20, 40, 0, 1, 0, 0}, {80, 64, 120, 90}, 0, 0, FX(0, 25, 35, 55), PAT(5)},
    {"FM BELLS", {26, 29, 70, 35, -1, 2, 0, 0}, {0, 70, 50, 70}, 0, 0, FX(0, 15, 35, 60), PAT(7)},
    {"SUB WAVE", {1, 9, 0, 50, -1, 0, 0, 0}, {0, 64, 127, 25}, 0, 1, FX(10, 0, 10, 10), PAT(2)},
    {"GLASS KEYS", {29, 34, 40, 30, 0, 1, 0, 0}, {0, 64, 70, 60}, 0, 0, FX(0, 20, 35, 55), PAT(6)},
    {"DEEP SPACE", {37, 43, 10, 50, -2, 0, 0, 0}, {95, 64, 127, 100}, 0, 0, FX(0, 30, 40, 80), PAT(5)},
    {"ROBO LEAD", {18, 3, 60, 20, 0, 1, 30, 0}, {0, 64, 127, 35}, 0, 1, FX(10, 0, 35, 25), PAT(4)},
    {"PWM BASS", {7, 1, 40, 0, 0, 0, 0, 0}, {0, 60, 100, 25}, 20, 1, FX(10, 0, 10, 10), PAT(2)},
    {"CRYSTAL ARP", {34, 36, 80, 50, 0, 2, 0, 20}, {0, 55, 40, 40}, 0, 0, FX(0, 15, 40, 45), PAT(13)},
    {"GROWL", {20, 1, 30, 25, -1, 0, 60, 0}, {0, 64, 127, 30}, 30, 1, FX(20, 0, 20, 20), PAT(2)},
    {"FOLD LEAD", {8, 10, 30, 0, 0, 1, 90, 0}, {0, 64, 127, 40}, 0, 1, FX(10, 10, 35, 30), PAT(4)},
    {"ORBIT PAD", {41, 50, 30, 50, 0, 1, 0, 30}, {90, 64, 127, 100}, 0, 0, FX(0, 30, 40, 70), PAT(5)},
    {"BIT PLUCK", {60, 52, 20, 30, 0, 1, 0, 0}, {0, 72, 0, 20}, 0, 0, FX(0, 0, 30, 25), PAT(3)},
    {"HOLLOW PIPE", {33, 22, 30, 30, 0, 1, 0, 0}, {20, 64, 110, 50}, 0, 0, FX(0, 15, 25, 40), PAT(4)},
    {"DIRTY CLOCK", {58, 61, 20, 40, -1, 1, 40, 0}, {0, 50, 60, 30}, 0, 1, FX(40, 0, 30, 20), PAT(2)},
    {"GRIT NOISE", {51, 54, 90, 60, 0, 1, 0, 0}, {0, 66, 0, 30}, 0, 0, FX(0, 0, 20, 30), PAT(1)},
};
/* the filter and the oscillator envelope of each preset (wt_track_loaded): {RES, 127 - CUT, 127 - KEY, TYPE, A, D, TO POS, TO MIX}; zeros
 * are the defaults (open, 100 % key, LP, no oscillator envelope) */
static const int8_t WT_X[NELEM(WT_PRESETS)][8] = {
    [0] = {30, 45, 0, 0, 0, 70, 25, 0},          /* SAW SWEEP: the position opens with the note, the filter follows the key */
    [1] = {0, 50, 0, 0, 70, 90, 30, 10},
    [2] = {0, 20, 0, 0, 0, 80, -10, 0},
    [3] = {20, 60, 0, 0, 0, 0, 0, 0},
    [4] = {0, 30, 0, 0, 0, 70, 15, 0},
    [5] = {0, 40, 0, 0, 100, 100, 40, -20},
    [6] = {50, 55, 0, 0, 0, 60, 20, 0},
    [7] = {40, 70, 0, 0, 0, 55, 25, 0},
    [8] = {0, 10, 0, 0, 0, 45, 30, 0},
    [9] = {60, 60, 0, 0, 0, 80, 20, 0},
    [10] = {35, 50, 0, 0, 0, 50, 15, 0},
    [11] = {0, 35, 0, 0, 100, 110, 25, 30},
    [12] = {0, 25, 0, 0, 0, 40, 0, 0},
    [13] = {10, 40, 0, 0, 40, 60, 15, 0},
    [14] = {45, 55, 0, 0, 0, 40, 20, 0},
    [15] = {0, 60, 0, 1, 0, 0, 0, 0},
};

static const engine_t ENG_WT = {
    .name = "WTSYNTH",
    .page_title = {"WAVE", "SHAPE"},
    .edit = {
        {"WAV1", F_INT, 1, 63, 1, 0, 0},
        {"WAV2", F_INT, 1, 63, 2, 0, 0},
        {"POS", F_INT, 0, 127, 0, 0, 0},
        {"MIX", F_INT, 0, 127, 0, 0, 0},
        {"OCT2", F_INT, -2, 0, 0, 0, 0},
        {"RANGE", F_ENUM, 0, 3, 1, N_WT_RANGE, 0},
        {"WS", F_INT, 0, 127, 0, 0, 0},
        {"PM", F_INT, 0, 127, 0, 0, 0},
    },
    .presets = WT_PRESETS,
    .npresets = NELEM(WT_PRESETS),
    .note_on = wt_note_on,
    .render = wt_render,
    .knob = {P_E2, P_E3, P_E6, WT_CUT},
    .poly = 0,
    .tdesc = wt_tdesc,
    .keep = 0x30,                /* the voice filter */
};

/* the preset a track just loaded (ui.c apply_preset_to, tests/hostsim.c): its filter and oscillator envelope */
static void wt_track_loaded(track_t *t)
{
    const int8_t *x;
    if (t->eng_req != ENGI_WT)
        return;
    x = WT_X[(uint32_t)t->preset % NELEM(WT_X)];
    t->p[WT_RES] = x[0];
    t->p[WT_CUT] = 127 - x[1];
    t->p[WT_KEY] = 127 - x[2];
    t->p[P_FTYPE] = x[3];
    t->p[WT_OA] = x[4];
    t->p[WT_OD] = x[5];
    t->p[WT_TPOS] = x[6];
    t->p[WT_TMIX] = x[7];
}

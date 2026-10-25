/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Stephane Schmitz */
/* WTSYNTH: mini-vision's wavetable voice, after the structure of Roland's n/vision (docs/NVISION_SPECS.md; its sounds and wavetables
 * are not Roland's: tools/gen_nvision_wt.py makes 63 tables of 8 key frames, each frame band limited at 5 mip levels).
 *
 * A voice has two wavetable layers. Each reads its table (WAV1, WAV2: 1 .. 63) at the same POSITION (0 .. 127, which morphs
 * through the table's key frames), the layers are crossfaded by MIX, the second one is OCT2 octaves under the first (the first
 * is RANGE: 16' 8' 4' 2'). The result goes through a wave shaper (WS: a sine fold, from the signal as it is to one that wraps
 * four times), then the voice filter (below), then the track's amplifier envelope.
 * PM is phase modulation: layer 2 modulates layer 1's phase.
 *
 * EDIT 1 (WAVE):  WAV1  WAV2  POS  MIX
 * EDIT 2 (SHAPE): OCT2  RANGE WS   PM
 * OSC ENV:        A D (the oscillator's own attack / decay envelope), TO POS and TO MIX (how far it moves the position and the mix)
 * FILTER:         TYPE CUT RES KEY     TYPE: LP12 BP12 HP12 (dsp.c rsvf, 2 poles), LP24 BP24 HP24 (two of them in series, the first with
 *                                      at most RES 20), LP.R LP.M LP.S (a 4-pole ladder, after a Roland, a Moog and a Sequential)
 * FILTER 2:       DRIVE                0 .. 3: the signal into the filter through a tanh, 1x 2x 5x 12x (in the filter's path: it
 *                                      works with the filter open too)
 * The track's ENV page (ATK DEC SUS REL) is the amplifier; its LFO and ENV pages reach the pitch, the filter, the position
 * (SHAPE: LFO -> SHP, ENV -> SHP, vmod_t.shape) and the amplitude as for every engine; the modulation matrix has E1 .. E8.
 *
 * Voice state: ph[0], ph[1] the layers' phases; s[0] the position of the last block (Q8, -1 before the first), s[1] the oscillator
 * envelope (Q24), s[2] its stage (0 attack, 1 decay), s[4] .. s[7] the filter (two 2-pole stages: s[4] s[5], s[6] s[7]; the ladder's
 * four poles: s[4] .. s[7]). */
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
#define WT_DRV P_FM3_ATK         /* 0 .. 3 */
static const param_desc_t WT_TD[P_FM4_LEVEL - P_FM1_ATK + 1] = {
    [WT_CUT - P_FM1_ATK] = {"CUT", F_CUTOFF, 0, 127, 127, 0, 0},
    [WT_RES - P_FM1_ATK] = {"RES", F_PCT, 0, 127, 0, 0, 0},
    [WT_KEY - P_FM1_ATK] = {"KEY", F_PCT, 0, 127, 127, 0, 0},
    [WT_OA - P_FM1_ATK] = {"A", F_TIME, 0, 127, 0, 0, 0},
    [WT_OD - P_FM1_ATK] = {"D", F_TIME, 0, 127, 0, 0, 0},
    [WT_TPOS - P_FM1_ATK] = {"TO POS", F_INT, -63, 63, 0, 0, 0},
    [WT_TMIX - P_FM1_ATK] = {"TO MIX", F_INT, -63, 63, 0, 0, 0},
    [WT_DRV - P_FM1_ATK] = {"DRIVE", F_INT, 0, 3, 0, 0, 0},
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
    v->s[4] = v->s[5] = v->s[6] = v->s[7] = 0;            /* the voice filter (a retrigger keeps it: engine_t.keep) */
    if (t->p[P_FTYPE] >= 6)
        v->s[7] = rsvf_seed(t->p[WT_RES]);                /* the ladder rings from its last pole */
    else
        v->s[5] = rsvf_seed(t->p[WT_RES]);
}

/* the ladder filter (LP.R LP.M LP.S): four one-pole low passes (Zavalishin's trapezoidal form) in a feedback loop of gain k, solved
 * for the zero-delay loop (y4 = (G^4 in + S) / (1 + k G^4)), the input saturating (and, for the Sequential, each pole). All Q24 but the
 * states, which have 8 more bits than the signal (a full scale of 16384 is 16384 << 8). g comes from RSVF_G as rsvf_coef takes it.
 *   LP.M  the Moog: k to 4.2, the passband falls as the resonance rises, one soft input stage
 *   LP.R  the Roland: k to 3.9, the input raised with k (the bass stays), a gentler knee
 *   LP.S  the Sequential: k to 4.2, half of the Roland's bass compensation, every pole saturates (from 0.25 of full scale): a thicker, rounder ring */
typedef struct { int32_t G, G2, G3, G4, omg, k, inv, comp; uint32_t type; } wt_lad_t;
static inline void wt_lad_coef(wt_lad_t *c, int32_t cut, int32_t res, uint32_t type)
{
    int32_t i, g, den;
    uint32_t inv;
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = (int32_t)RSVF_G[i] + (int32_t)(((int64_t)((int32_t)RSVF_G[i + 1] - (int32_t)RSVF_G[i]) * (cut & 255)) >> 8);
    inv = (0xFFFFFFFFu / (((uint32_t)g + (1u << 24)) >> 8)) << 8;           /* 1 / (1 + g), Q24 */
    c->G = (int32_t)(((int64_t)g * inv) >> 24);
    c->G2 = (int32_t)(((int64_t)c->G * c->G) >> 24);
    c->G3 = (int32_t)(((int64_t)c->G2 * c->G) >> 24);
    c->G4 = (int32_t)(((int64_t)c->G3 * c->G) >> 24);
    c->omg = (1 << 24) - c->G;
    c->k = (int32_t)(((int64_t)clamp(res, 0, 127) * (type == 6u ? 509100 : 554832)));   /* 0 .. 3.9 (Roland), 0 .. 4.2 */
    den = (1 << 24) + (int32_t)(((int64_t)c->k * c->G4) >> 24);
    c->inv = (int32_t)((0xFFFFFFFFu / ((uint32_t)den >> 8)) << 8);
    c->comp = type == 6u ? (1 << 24) + (int32_t)(((int64_t)c->k * 6710886) >> 24) :          /* LP.R: the input x (1 + 0.4 k) */
              type == 8u ? (1 << 24) + (int32_t)(((int64_t)c->k * 3355443) >> 24) : (1 << 24);   /* LP.S: x (1 + 0.2 k) */
    c->type = type;
}
static inline int32_t wt_lad_run(const wt_lad_t *c, int32_t in, int32_t *s)
{
    uint32_t i4;
    int32_t in8 = (int32_t)(((int64_t)(in << 8) * c->comp) >> 24), acc, y4, x, v, y, knee = c->type == 6u ? 15000 : c->type == 7u ? 11000 : 4000;
    acc = (int32_t)(((int64_t)c->G3 * s[0] + (int64_t)c->G2 * s[1] + (int64_t)c->G * s[2]) >> 24) + s[3];
    y4 = (int32_t)((((int64_t)c->G4 * in8 + (int64_t)c->omg * acc) >> 24) * (int64_t)c->inv >> 24);   /* G^4 in + (1 - G) acc, over 1 + k G^4 */
    x = rsvf_sat(in8 - (int32_t)(((int64_t)c->k * y4) >> 24), c->type == 7u ? 12000 : 15000);
    for (i4 = 0; i4 < 4; i4++) {
        v = (int32_t)(((int64_t)(x - s[i4]) * c->G) >> 24);
        y = v + s[i4];
        s[i4] = y + v;
        x = c->type == 8u ? rsvf_sat(y, knee) : y;
    }
    return (x + 128) >> 8;
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
    int32_t fcut = clamp(p[WT_CUT], 0, 127), fres = clamp(p[WT_RES], 0, 127), fi1 = v->s[4], fi2 = v->s[5], fi3 = v->s[6], fi4 = v->s[7];
    int32_t drv = clamp(p[WT_DRV], 0, 3), ls[4];
    uint32_t fty = (uint32_t)clamp(p[P_FTYPE], 0, 8), fon = fcut < 127 || fres > 0 || fty || drv, ph0 = v->ph[0], ph1 = v->ph[1], rng, oct2;
    const int8_t *b1, *b2;
    rsvf_t rf, rf2;
    wt_lad_t ld;
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
    if (fon) {
        int32_t cut = rsvf_key(fcut, m->cutoff, clamp(p[WT_KEY], 0, 127), m->pitch16);
        if (fty >= 6u) {
            wt_lad_coef(&ld, cut, fres, fty);
            ls[0] = fi1; ls[1] = fi2; ls[2] = fi3; ls[3] = fi4;
        } else {
            rsvf_coef(&rf, cut, fty >= 3u && fres > 20 ? 20 : fres);          /* (24 dB: the first stage with a low Q) */
            if (fty >= 3u)
                rsvf_coef(&rf2, cut, fres);
        }
    }
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
        if (fon) {
            static const int32_t DRV_G[4] = {0, 2, 5, 12};
            int32_t u = s >> 1, f;
            if (drv)
                u = (softclip((u * DRV_G[drv]) << 1) * 3) >> 3;             /* tanh of 2x, 5x or 12x, less 2.5 dB */
            if (fty >= 6u)
                f = wt_lad_run(&ld, u, ls);
            else if (fty >= 3u)
                f = rsvf_run(&rf2, rsvf_run(&rf, u, &fi1, &fi2, fty - 3u), &fi3, &fi4, fty - 3u);
            else
                f = rsvf_run(&rf, u, &fi1, &fi2, fty);
            s = soft_knee(f, 16000) << 1;
        }
        out[i] += voice_amp(s, m, i);
        ph0 += inc1;
        ph1 += inc2;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->s[0] = pos1;
    v->s[1] = env;
    if (fty >= 6u) {
        fi1 = ls[0]; fi2 = ls[1]; fi3 = ls[2]; fi4 = ls[3];
    }
    v->s[4] = fi1;
    v->s[5] = fi2;
    v->s[6] = fi3;
    v->s[7] = fi4;
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
/* the filter and the oscillator envelope of each preset (wt_track_loaded): {RES, 127 - CUT, 127 - KEY, TYPE, A, D, TO POS, TO MIX, DRIVE}; zeros
 * are the defaults (open, 100 % key, LP12, no oscillator envelope, no drive) */
static const int8_t WT_X[NELEM(WT_PRESETS)][9] = {
    [0] = {30, 45, 0, 3, 0, 70, 25, 0, 0},       /* SAW SWEEP: LP24; the position opens with the note, the filter follows the key */
    [1] = {0, 50, 0, 0, 70, 90, 30, 10, 0},
    [2] = {0, 20, 0, 0, 0, 80, -10, 0, 0},
    [3] = {20, 60, 0, 7, 0, 0, 0, 0, 1},         /* SUB WAVE: the Moog ladder */
    [4] = {0, 30, 0, 0, 0, 70, 15, 0, 0},
    [5] = {0, 40, 0, 0, 100, 100, 40, -20, 0},
    [6] = {50, 55, 0, 6, 0, 60, 20, 0, 1},       /* ROBO LEAD: the Roland ladder */
    [7] = {40, 70, 0, 7, 0, 55, 25, 0, 2},       /* PWM BASS: the Moog ladder, driven */
    [8] = {0, 10, 0, 0, 0, 45, 30, 0, 0},
    [9] = {60, 60, 0, 8, 0, 80, 20, 0, 2},       /* GROWL: the Sequential ladder, driven */
    [10] = {35, 50, 0, 6, 0, 50, 15, 0, 0},
    [11] = {0, 35, 0, 3, 100, 110, 25, 30, 0},
    [12] = {0, 25, 0, 0, 0, 40, 0, 0, 0},
    [13] = {10, 40, 0, 0, 40, 60, 15, 0, 0},
    [14] = {45, 55, 0, 8, 0, 40, 20, 0, 3},      /* DIRTY CLOCK: the Sequential ladder, driven hard */
    [15] = {0, 60, 0, 1, 0, 0, 0, 0, 0},
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
    .keep = 0xF0,                /* the voice filter (s[4] .. s[7]) */
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
    t->p[WT_DRV] = x[8];
}

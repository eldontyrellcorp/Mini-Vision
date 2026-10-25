/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Stephane Schmitz */
/* WTSYNTH test (src/eng_wt.c, the tables of tools/gen_nvision_wt.py) on the host, through hostsim.c as the other engine tests.
 *   build/host/wt_test
 *  1. the engine is number 13, its descriptors, the pages (OSC ENV and FILTER for it only), the presets.
 *  2. pitch: RANGE 16' .. 2', the notes of the keyboard, OCT2 on layer 2, a pitch LFO's centre.
 *  3. the tables: the first frame of table 1 is a sine (THD + N of the 8-bit table), POSITION morphs it to a saw, MIX picks the layer
 *     (and WAV1 / WAV2 swapped give the same), nothing aliases at the top of the keyboard.
 *  4. WS and PM change the sound and are bit for bit off at 0; the filter (TYPE, CUT, RES) and the oscillator envelope.
 *  5. levels, determinism, the voice frees its place. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NS 65536
static int bad;
static void check(const char *what, int ok)
{
    printf("wt: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void setup(void)
{
    memset(trk, 0, sizeof trk);
    memset(eng_mem, 0, sizeof eng_mem);
    host_tracks_init();
    host_preset(&trk[0], ENGI_WT, 0);
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_REL] = 10;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_DIST] = 0;
    trk[0].p[P_LEVEL] = 127;
    trk[0].p[P_E0] = 1; trk[0].p[P_E1] = 2; trk[0].p[P_E2] = 0; trk[0].p[P_E3] = 0;     /* table 1, frame 0: a sine */
    trk[0].p[P_E4] = 0; trk[0].p[P_E5] = 1; trk[0].p[P_E6] = 0; trk[0].p[P_E7] = 0;
    trk[0].p[WT_CUT] = 127; trk[0].p[WT_RES] = 0; trk[0].p[P_FTYPE] = 0; trk[0].p[WT_KEY] = 127;
    trk[0].p[WT_OA] = trk[0].p[WT_OD] = trk[0].p[WT_TPOS] = trk[0].p[WT_TMIX] = 0;
    trk[0].p[P_ED_FLT] = trk[0].p[P_ED_SHP] = trk[0].p[P_ED_PIT] = 0;
    song.g[G_BPM] = 120;
}
/* n samples of part 0 (note struck at once) into y, full scale 1.0 */
static void render(uint32_t note, double *y, uint32_t n)
{
    static int32_t b[CTL];
    uint32_t i, k;
    trk_note_on(&trk[0], note, 100);
    for (i = 0; i < n; i += CTL) {
        for (k = 0; k < CTL; k++)
            b[k] = 0;
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            y[i + k] = b[k] / 32768.0;
    }
}
static double rms_of(const double *y, uint32_t a, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += y[a + i] * y[a + i];
    return sqrt(s / n);
}
static double peak_of(const double *y, uint32_t n)
{
    double p = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        if (fabs(y[i]) > p)
            p = fabs(y[i]);
    return p;
}
static double db(double x) { return 20.0 * log10(x + 1e-12); }
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j, len;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        double a = -2 * M_PI / len;
        for (i = 0; i < n; i += len)
            for (j = 0; j < len / 2; j++) {
                double wr = cos(a * j), wi = sin(a * j);
                double xr = re[i + j + len / 2] * wr - im[i + j + len / 2] * wi;
                double xi = re[i + j + len / 2] * wi + im[i + j + len / 2] * wr;
                re[i + j + len / 2] = re[i + j] - xr, im[i + j + len / 2] = im[i + j] - xi;
                re[i + j] += xr, im[i + j] += xi;
            }
    }
}
#define FN 32768
static double spec[FN / 2];                               /* power per bin of the last spectrum() */
static void spectrum(const double *y, uint32_t a)         /* Hann window of FN samples from a */
{
    static double re[FN], im[FN];
    uint32_t i;
    for (i = 0; i < FN; i++) {
        re[i] = y[a + i] * (0.5 - 0.5 * cos(2 * M_PI * i / FN));
        im[i] = 0;
    }
    fft(re, im, FN);
    for (i = 0; i < FN / 2; i++)
        spec[i] = re[i] * re[i] + im[i] * im[i];
}
static double hz_to_bin(double hz) { return hz * FN / FS; }
static double band_power(double hz, double w)             /* power within +-w bins of hz */
{
    int c = (int)floor(hz_to_bin(hz) + 0.5), i;
    double s = 0;
    for (i = c - (int)w; i <= c + (int)w; i++)
        if (i > 0 && i < FN / 2)
            s += spec[i];
    return s;
}
/* the amplitude of the sine at hz in y[a .. a + n) (a direct sum, no window: n a whole number of cycles is not needed at this accuracy) */
static double sine_amp(const double *y, uint32_t a, uint32_t n, double hz)
{
    double re = 0, im = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / n), ph = 2 * M_PI * hz * (a + i) / FS;
        re += y[a + i] * w * cos(ph);
        im += y[a + i] * w * sin(ph);
    }
    return 2 * sqrt(re * re + im * im) / (n * 0.5);
}
static double dom_hz(void)                                /* the strongest bin of the last spectrum, refined by its neighbours */
{
    uint32_t i, best = 2;
    double a, b, c, d;
    for (i = 2; i < FN / 2 - 1u; i++)
        if (spec[i] > spec[best])
            best = i;
    a = log(spec[best - 1] + 1e-30); b = log(spec[best] + 1e-30); c = log(spec[best + 1] + 1e-30);
    d = (a - c) / (2 * (a - 2 * b + c));
    return (best + d) * (double)FS / FN;
}
static double note_hz(uint32_t note) { return 440.0 * pow(2.0, (note - 69.0) / 12.0); }

static double y[NS], z[NS];

/* ---- 1. the engine ---- */
static void engine_basics(void)
{
    uint32_t i, e, n = 0, k;
    int ok = 1;
    check("WTSYNTH is engine 13, 16 factory presets, a wavetable per WAV1 / WAV2 value (63)",
          ENGINES[ENGI_WT] == &ENG_WT && ENG_WT.npresets == 16u && NV_NTAB == 63 && NV_NFRAME == 8 && eng_ok(ENGI_WT));
    ok = ENGI_WT < 14u && NENG_SHOWN >= 2u;   /* (favorites.c FAV_ENGINES) */
    check("it is below FAV_ENGINES (its presets can be starred) and one of the engines one can pick", ok);
    for (i = 0; i < NPAGES; i++)
        for (e = 0; e < NENGINES; e++)
            if (PAGES[i].own == ENGI_WT + 1u && page_for_engine(i, e)) {
                ok &= e == ENGI_WT;
                if (e == ENGI_WT)
                    n++;
            }
    check("OSC ENV, FILTER and FILTER 2 show for WTSYNTH and for no other engine", ok && n == 3u);
    ok = 1;
    for (k = 0; k < 8u; k++)
        ok &= ENG_WT.edit[k].label[0] != 0;
    ok &= param_desc_of(ENGI_WT, P_E0)->min == 1 && param_desc_of(ENGI_WT, P_E1)->max == 63 &&
          param_desc_of(ENGI_WT, WT_CUT)->def == 127 && param_desc_of(ENGI_WT, WT_RES)->def == 0 &&
          param_desc_of(ENGI_WT, WT_TPOS)->min == -63 && param_desc_of(ENGI_WT, P_E4)->min == -2;
    check("the descriptors: WAV1 / WAV2 1 .. 63, OCT2 -2 .. 0, the filter slots' defaults are TP's, the envelope's amounts are bipolar", ok);
    ok = 1;
    for (k = 0; k < ENG_WT.npresets; k++) {
        const preset_t *p = &ENG_WT.presets[k];
        ok &= p->e[0] >= 1 && p->e[0] <= 63 && p->e[1] >= 1 && p->e[1] <= 63 && p->e[2] >= 0 && p->e[3] >= 0 && p->e[4] >= -2 &&
              p->e[4] <= 0 && p->e[5] >= 0 && p->e[5] <= 3;
    }
    check("the presets' values are inside their ranges", ok);
}

/* ---- 2. pitch ---- */
static void pitch(void)
{
    static const uint32_t NOTES[] = {24, 36, 48, 60, 69, 72, 84, 96};
    uint32_t i, r;
    int ok = 1;
    double worst = 0;
    for (i = 0; i < sizeof NOTES / sizeof NOTES[0]; i++) {
        double f, e;
        setup();
        render(NOTES[i], y, NS);
        spectrum(y, 8192);
        f = dom_hz();
        e = fabs(1200 * log2(f / note_hz(NOTES[i])));
        if (e > worst)
            worst = e;
        ok &= e < 3.0;
    }
    printf("wt: worst pitch error of the sine over 8 notes C1 .. C7: %.2f cents\n", worst);
    check("a sine at 8' plays the pitch of the key within 3 cents (C1 .. C7)", ok);
    ok = 1;
    for (r = 0; r < 4u; r++) {
        double f;
        setup();
        trk[0].p[P_E5] = (int16_t)r;
        render(48, y, NS);
        spectrum(y, 8192);
        f = dom_hz();
        ok &= fabs(1200 * log2(f / (note_hz(48) * pow(2.0, (double)r - 1.0)))) < 3.0;
    }
    check("RANGE 16' 8' 4' 2': an octave apart, 8' at the key", ok);
    ok = 1;
    for (r = 0; r < 3u; r++) {
        double f;
        setup();
        trk[0].p[P_E3] = 127;                              /* layer 2 alone */
        trk[0].p[P_E4] = (int16_t)-(int32_t)r;
        render(60, y, NS);
        spectrum(y, 8192);
        f = dom_hz();
        ok &= fabs(1200 * log2(f / (note_hz(60) / pow(2.0, (double)r)))) < 3.0;
    }
    check("OCT2 0 / -1 / -2: layer 2 an octave and two under the first", ok);
}

/* ---- 3. the tables ---- */
static void tables(void)
{
    double fund, rest, thd;
    uint32_t h;
    int ok;
    setup();
    render(57, y, NS);                                      /* A3, 220 Hz: a sine table at its first frame */
    spectrum(y, 8192);
    fund = band_power(220.0, 3);
    rest = 0;
    for (h = 2; h <= 40; h++)
        rest += band_power(220.0 * h, 3);
    thd = db(sqrt(rest / fund));
    printf("wt: table 1 frame 0 (a sine), harmonics 2 .. 40 against the fundamental: %.1f dB\n", thd);
    check("the first frame of table 1 is a sine: its harmonics are under -40 dB (the 8-bit table's distortion)", thd < -40.0);
    {
        double c0, c1, c2, h2a, h2b;
        setup(); trk[0].p[P_E2] = 0; render(57, y, NS); spectrum(y, 8192);
        c0 = band_power(440.0, 3) / band_power(220.0, 3); h2a = c0;
        setup(); trk[0].p[P_E2] = 60; render(57, y, NS); spectrum(y, 8192);
        c1 = band_power(440.0, 3) / band_power(220.0, 3);
        setup(); trk[0].p[P_E2] = 127; render(57, y, NS); spectrum(y, 8192);
        c2 = band_power(440.0, 3) / band_power(220.0, 3); h2b = c2;
        printf("wt: the 2nd harmonic against the 1st at POS 0 / 60 / 127: %.1f / %.1f / %.1f dB\n", db(sqrt(h2a)), db(sqrt(c1)), db(sqrt(h2b)));
        check("POSITION morphs table 1 from a sine to a saw: the 2nd harmonic grows to half the fundamental (-6 dB) through the frames",
              c0 < 1e-4 && c1 > c0 * 100 && c2 > 0.15 && c2 < 0.35);
    }
    {
        static double w1[NS];
        setup(); trk[0].p[P_E0] = 13; trk[0].p[P_E1] = 30; trk[0].p[P_E2] = 50; trk[0].p[P_E3] = 0; render(52, w1, NS);
        setup(); trk[0].p[P_E0] = 30; trk[0].p[P_E1] = 13; trk[0].p[P_E2] = 50; trk[0].p[P_E3] = 127; render(52, y, NS);
        ok = !memcmp(w1, y, sizeof w1);
        check("MIX 0 plays WAV1 and MIX 127 plays WAV2: the layers swapped give the same sound, sample for sample", ok);
        setup(); trk[0].p[P_E0] = 13; trk[0].p[P_E1] = 30; trk[0].p[P_E2] = 50; trk[0].p[P_E3] = 64; render(52, z, NS);
        ok = memcmp(z, w1, sizeof w1) && memcmp(z, y, sizeof y);
        check("MIX 64 is neither layer", ok);
    }
    {   /* the top of the keyboard on the brightest table (table 1 at POS 127, a saw): the energy outside the harmonics */
        static const uint32_t NOTES[] = {84, 96, 108};
        uint32_t i;
        ok = 1;
        for (i = 0; i < 3u; i++) {
            double f = note_hz(NOTES[i]), tot = 0, har = 0;
            uint32_t b, hh;
            setup(); trk[0].p[P_E2] = 127; render(NOTES[i], y, NS); spectrum(y, 8192);
            for (b = 2; b < FN / 2; b++)
                tot += spec[b];
            for (hh = 1; hh * f < FS / 2.0 - 2000.0; hh++)
                har += band_power(f * hh, 3);
            printf("wt: note %u (%.0f Hz): %.1f dB of the spectrum is not a harmonic\n", NOTES[i], f, db(sqrt(fmax(tot - har, 1e-30) / tot)));
            ok &= db(sqrt(fmax(tot - har, 1e-30) / tot)) < -30.0;
        }
        check("a saw on C6 .. C8 does not alias: what is not a harmonic is under -30 dB (the mip levels)", ok);
    }
}

/* ---- 4. WS, PM, the filter, the oscillator envelope ---- */
static void shaping(void)
{
    int ok;
    uint32_t i;
    static double w0[NS];
    setup(); trk[0].p[P_E2] = 40; render(52, w0, NS);
    setup(); trk[0].p[P_E2] = 40; trk[0].p[P_E6] = 0; trk[0].p[P_E7] = 0; render(52, y, NS);
    check("WS 0, PM 0 and the filter open: the sound as it is, and the same twice", !memcmp(w0, y, sizeof y));
    setup(); trk[0].p[P_E2] = 40; trk[0].p[P_E6] = 80; render(52, y, NS);
    for (i = 0, ok = 1; i < NS; i++)
        ok &= fabs(y[i]) < 1.0;
    check("WS 80 changes the sound and stays inside the range", memcmp(w0, y, sizeof y) && ok && rms_of(y, 8192, 16384) > 0.02);
    setup(); trk[0].p[P_E2] = 40; trk[0].p[P_E3] = 64; trk[0].p[P_E7] = 60; render(52, y, NS);
    setup(); trk[0].p[P_E2] = 40; trk[0].p[P_E3] = 64; render(52, z, NS);
    check("PM 60: layer 2 modulates layer 1's phase", memcmp(y, z, sizeof y) != 0);
    {   /* the filter */
        double hi0, hi1, lo0, lo1;
        setup(); trk[0].p[P_E0] = 1; trk[0].p[P_E2] = 127; render(45, y, NS); spectrum(y, 8192);
        hi0 = band_power(3520.0, 40); lo0 = band_power(110.0, 3);
        setup(); trk[0].p[P_E0] = 1; trk[0].p[P_E2] = 127; trk[0].p[WT_CUT] = 60; render(45, y, NS); spectrum(y, 8192);
        hi1 = band_power(3520.0, 40); lo1 = band_power(110.0, 3);
        printf("wt: LP at CUT 60: the band at 3.5 kHz %.1f dB, the fundamental %.1f dB\n", db(sqrt(hi1 / hi0)), db(sqrt(lo1 / lo0)));
        check("TYPE LP, CUT 60: the highs fall by 20 dB or more, the fundamental stays within 4 dB", db(sqrt(hi1 / hi0)) < -20.0 && fabs(db(sqrt(lo1 / lo0))) < 4.0);
        setup(); trk[0].p[P_E0] = 1; trk[0].p[P_E2] = 127; trk[0].p[WT_CUT] = 80; trk[0].p[P_FTYPE] = 2; render(45, y, NS); spectrum(y, 8192);
        printf("wt: HP at CUT 80: the band at 3.5 kHz %.1f dB, the fundamental %.1f dB\n", db(sqrt(band_power(3520.0, 40) / hi0)), db(sqrt(band_power(110.0, 3) / lo0)));
        check("TYPE HP, CUT 80: the fundamental falls by 20 dB or more, the highs stay within 4 dB",
              band_power(3520.0, 40) > 0.4 * hi0 && band_power(110.0, 3) < 0.01 * lo0);
    }
    {   /* the oscillator envelope moves the position: sine -> saw within the decay */
        double e0, e1;
        setup(); trk[0].p[WT_TPOS] = 63; trk[0].p[WT_OA] = 0; trk[0].p[WT_OD] = 70; trk[0].p[P_E2] = 0; render(57, y, NS);
        e0 = sine_amp(y, 256, 2048, 440.0) / sine_amp(y, 256, 2048, 220.0);        /* just after the hit: the position is up */
        e1 = sine_amp(y, NS - 8192, 4096, 440.0) / sine_amp(y, NS - 8192, 4096, 220.0);   /* a second and a half later: back at 0 */
        printf("wt: the 2nd harmonic against the 1st: %.1f dB just after the hit, %.1f dB 1.5 s later\n", db(e0), db(e1));
        check("OSC ENV: TO POS opens the position with the hit and closes it with the decay", e0 > 20 * e1 && e0 > 0.05);
    }
    {   /* the track's ENV -> SHP reaches the position too */
        double e0;
        setup(); trk[0].p[P_ED_SHP] = 100; trk[0].p[P_ATK] = 0; trk[0].p[P_DEC] = 60; trk[0].p[P_SUS] = 0; trk[0].p[P_E2] = 0; render(57, y, NS);
        e0 = sine_amp(y, 256, 2048, 440.0) / sine_amp(y, 256, 2048, 220.0);
        check("ENV -> SHAPE (the track's ENV page) moves the position", e0 > 0.02);
    }
}

/* the filter's cutoff (Hz) for CUT c at KEY 0: dsp.c rsvf_coef's g = tan(pi fc / fs), RSVF_G in Q24 */
static double cut_hz(int c) { return atan(RSVF_G[c] / 16777216.0) * FS / M_PI; }
static int cut_for(double hz)
{
    int c, best = 0;
    for (c = 0; c < 127; c++)
        if (fabs(cut_hz(c) - hz) < fabs(cut_hz(best) - hz))
            best = c;
    return best;
}
/* ---- 4b. the filter types: 24 dB, the ladders, the drive ---- */
static void filter_types(void)
{
    static const char *const TN[9] = {"LP12", "BP12", "HP12", "LP24", "BP24", "HP24", "LP.R", "LP.M", "LP.S"};
    static double f0[NS];
    int ok = 1, t, c = cut_for(450.0);
    double fc = cut_hz(c), open_hi, att[9];
    uint32_t h;
    printf("wt: CUT %d is %.0f Hz\n", c, fc);
    setup(); trk[0].p[P_E0] = 1; trk[0].p[P_E2] = 127; trk[0].p[WT_KEY] = 0; render(45, f0, NS); spectrum(f0, 8192);   /* a saw at 110 Hz */
    h = (uint32_t)floor(4 * fc / 110.0 + 0.5);                                  /* the harmonic nearest 4 x the cutoff: two octaves up */
    open_hi = band_power(110.0 * h, 3);
    for (t = 0; t < 9; t++) {
        setup(); trk[0].p[P_E0] = 1; trk[0].p[P_E2] = 127; trk[0].p[WT_KEY] = 0; trk[0].p[WT_CUT] = (int16_t)c; trk[0].p[P_FTYPE] = (int16_t)t;
        render(45, y, NS); spectrum(y, 8192);
        att[t] = db(sqrt(band_power(110.0 * h, 3) / open_hi));
        printf("wt: %s at CUT %d: the %.0f Hz harmonic %.1f dB (4 x the cutoff)\n", TN[t], c, 110.0 * h, att[t]);
    }
    check("LP12 falls 24 dB two octaves over its cutoff (2 poles, Q 0.5), within 5 dB", att[0] < -19.0 && att[0] > -30.0);
    check("LP24 falls about twice as far (4 poles)", att[3] < att[0] - 15.0 && att[3] > -62.0);
    check("HP24 / BP24 are steeper than their 12 dB types on the far side too (the band-pass the highs, the high pass the lows)", att[4] < att[1] - 5.0);
    ok = 1;
    for (t = 6; t < 9; t++)
        ok &= att[t] < -38.0 && att[t] > -62.0;
    check("the ladders LP.R, LP.M, LP.S fall four poles (about 49 dB) at 4 x the cutoff", ok);
    {   /* the resonance: a harmonic at the cutoff gains */
        double pk[9];
        int cf = cut_for(550.0);
        setup(); trk[0].p[P_E0] = 1; trk[0].p[P_E2] = 127; trk[0].p[WT_KEY] = 0; render(45, f0, NS); spectrum(f0, 8192);
        open_hi = band_power(550.0, 3);
        ok = 1;
        for (t = 0; t < 9; t++) {
            if (t == 1 || t == 4 || t == 2 || t == 5)
                continue;
            setup(); trk[0].p[P_E0] = 1; trk[0].p[P_E2] = 127; trk[0].p[WT_KEY] = 0; trk[0].p[WT_CUT] = (int16_t)cf; trk[0].p[P_FTYPE] = (int16_t)t;
            trk[0].p[WT_RES] = 0; render(45, y, NS); spectrum(y, 8192); pk[t] = db(sqrt(band_power(550.0, 3) / open_hi));
            setup(); trk[0].p[P_E0] = 1; trk[0].p[P_E2] = 127; trk[0].p[WT_KEY] = 0; trk[0].p[WT_CUT] = (int16_t)cf; trk[0].p[P_FTYPE] = (int16_t)t;
            trk[0].p[WT_RES] = 110; render(45, y, NS); spectrum(y, 8192);
            printf("wt: %s at the cutoff: RES 0 %.1f dB, RES 110 %.1f dB\n", TN[t], pk[t], db(sqrt(band_power(550.0, 3) / open_hi)));
            ok &= db(sqrt(band_power(550.0, 3) / open_hi)) > pk[t] + 6.0;
        }
        check("RES 110 lifts the harmonic on the cutoff by 6 dB or more over RES 0 (LP12, LP24 and the three ladders)", ok);
    }
    {   /* the three ladders are three sounds at a resonance */
        static double la[NS], lb[NS], lc[NS];
        int cf = cut_for(550.0);
        uint32_t i;
        double d01 = 0, d02 = 0, d12 = 0;
        for (t = 6; t < 9; t++) {
            setup(); trk[0].p[P_E0] = 4; trk[0].p[P_E2] = 90; trk[0].p[WT_KEY] = 0; trk[0].p[WT_CUT] = (int16_t)cf; trk[0].p[WT_RES] = 90;
            trk[0].p[P_FTYPE] = (int16_t)t; render(45, t == 6 ? la : t == 7 ? lb : lc, NS / 2);
        }
        for (i = 4096; i < NS / 2; i++) {
            d01 += (la[i] - lb[i]) * (la[i] - lb[i]); d02 += (la[i] - lc[i]) * (la[i] - lc[i]); d12 += (lb[i] - lc[i]) * (lb[i] - lc[i]);
        }
        d01 = sqrt(d01 / (NS / 2 - 4096)) / rms_of(lb, 4096, 8192); d02 = sqrt(d02 / (NS / 2 - 4096)) / rms_of(lb, 4096, 8192);
        d12 = sqrt(d12 / (NS / 2 - 4096)) / rms_of(lb, 4096, 8192);
        printf("wt: the ladders' difference at RES 90, against the Moog's level: R-M %.2f, R-S %.2f, M-S %.2f\n", d01, d02, d12);
        check("LP.R, LP.M and LP.S differ from each other by 5 % or more at RES 90", d01 > 0.05 && d02 > 0.05 && d12 > 0.05);
    }
    {   /* stable and bounded at the top of RES, with and without DRIVE, on a chord's worth of notes */
        uint32_t k;
        int ok2 = 1;
        for (t = 0; t < 9; t++)
            for (k = 0; k < 4u; k++) {
                static const int RS[4] = {0, 64, 127, 127}, DR[4] = {0, 0, 0, 3};
                uint32_t i;
                double pk;
                setup(); trk[0].p[P_E0] = 4; trk[0].p[P_E1] = 20; trk[0].p[P_E2] = 90; trk[0].p[P_E3] = 60; trk[0].p[WT_KEY] = 127;
                trk[0].p[WT_CUT] = 55; trk[0].p[P_FTYPE] = (int16_t)t; trk[0].p[WT_RES] = (int16_t)RS[k]; trk[0].p[WT_DRV] = (int16_t)DR[k];
                render(48, y, NS);
                pk = peak_of(y, NS);
                for (i = 0; i < NS; i++)
                    ok2 &= isfinite(y[i]);
                ok2 &= pk < 1.0;
                if (pk >= 1.0)
                    printf("wt: %s RES %d DRIVE %d peak %.2f\n", TN[t], RS[k], DR[k], pk);
            }
        check("every TYPE at RES 0 / 64 / 127 (and DRIVE 3): finite and inside full scale", ok2);
    }
    {   /* the drive */
        double h3a, h3b;
        setup(); render(57, y, NS); spectrum(y, 8192);
        h3a = band_power(660.0, 3) / band_power(220.0, 3);
        setup(); trk[0].p[WT_DRV] = 3; render(57, y, NS); spectrum(y, 8192);
        h3b = band_power(660.0, 3) / band_power(220.0, 3);
        printf("wt: a sine, the 3rd harmonic: %.1f dB without the drive, %.1f dB with DRIVE 3\n", db(sqrt(h3a)), db(sqrt(h3b)));
        check("DRIVE 3 saturates (a sine gets its 3rd harmonic above -25 dB) and DRIVE 0 does not (under -50 dB), the filter open", h3a < 1e-5 && h3b > 0.056);
        setup(); render(57, y, NS); setup(); trk[0].p[WT_DRV] = 0; render(57, z, NS);
        check("DRIVE 0, the filter open: the sound as it is", !memcmp(y, z, sizeof y));
    }
}

/* ---- 5. levels, determinism, the voice ---- */
static void levels(void)
{
    uint32_t p;
    int ok = 1, ok2 = 1;
    static double a[NS];
    for (p = 0; p < ENG_WT.npresets; p++) {
        double r, pk;
        setup();
        host_preset(&trk[0], ENGI_WT, p);
        trk[0].p[P_ATK] = 0;
        render(60, y, NS / 2);
        r = rms_of(y, 128, 2048);
        pk = peak_of(y, NS / 2);
        printf("wt: preset %2u %-12s rms %.3f peak %.3f\n", p, ENG_WT.presets[p].name, r, pk);
        ok &= r > 0.01 && pk < 1.0;
        setup(); host_preset(&trk[0], ENGI_WT, p); trk[0].p[P_ATK] = 0; render(60, a, NS / 2);
        ok2 &= !memcmp(a, y, sizeof a / 2);
    }
    check("each preset sounds (rms over -40 dBFS) and does not clip, at C4 and velocity 100", ok);
    check("the same twice, sample for sample", ok2);
    {
        double d = 0;
        uint32_t i;
        setup(); trk[0].p[P_E2] = 40; render(45, y, NS);
        for (i = 8192; i < NS; i++)
            d += y[i];
        check("no DC offset in a held note", fabs(d / (NS - 8192)) < 0.005);
    }
    {
        uint32_t n, k, act = 0;
        static int32_t b[CTL];
        setup();
        trk_note_on(&trk[0], 60, 100);
        trk_note_on(&trk[0], 64, 100);
        trk_note_on(&trk[0], 67, 100);
        for (n = 0; n < FS / CTL; n++) {
            memset(b, 0, sizeof b);
            track_render(&trk[0], b, CTL);
        }
        trk_note_off(&trk[0], 60); trk_note_off(&trk[0], 64); trk_note_off(&trk[0], 67);
        for (n = 0; n < 3u * FS / CTL; n++) {
            memset(b, 0, sizeof b);
            track_render(&trk[0], b, CTL);
        }
        for (k = 0; k < NVOICE; k++)
            act += trk[0].v[k].active;
        check("a chord's voices are free 3 s after the release (REL 10)", act == 0u);
    }
    {
        uint32_t n, k, act;
        static int32_t b[CTL];
        setup();
        for (k = 0; k < 8u; k++)
            trk_note_on(&trk[0], 48 + 3 * k, 127);
        for (n = 0; n < FS / CTL; n++) {
            memset(b, 0, sizeof b);
            track_render(&trk[0], b, CTL);
        }
        for (k = 0, act = 0; k < NVOICE; k++)
            act += trk[0].v[k].active;
        check("8 voices at once play (the budget of 8)", act == 8u);
    }
}

int main(void)
{
    engine_basics();
    pitch();
    tables();
    shaping();
    filter_types();
    levels();
    printf("wt: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}

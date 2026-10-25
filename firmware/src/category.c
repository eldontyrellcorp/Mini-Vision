/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Sound categories (1.2, Discussion #90): every sound in the PRESETS list belongs to one, so KNOB 4 (LIST) can show
 * only the basses, the pads, .. (ui.c list_mode). Factory presets: a static table here (a letter per preset, in the
 * engine's preset order); user presets: a byte of their record (upreset.c up_cat, set on the NAME screen when saving
 * or renaming, ui_name.c). Included by ui.c and upreset.c (the host build of the user presets needs the numbers only).
 * The numbers are stored (a user preset's record, the editor's UP_LIST / UP_GET / UP_PUT): append-only. CAT_NONE is a
 * record of before (or the editor's without one): its category follows from its engine (cat_of_engine) */
#ifndef FELUCCA_CATEGORY
#define FELUCCA_CATEGORY 1
enum { CAT_NONE, CAT_BASS, CAT_LEAD, CAT_PAD, CAT_PLUCK, CAT_KEYS, CAT_DRUM, CAT_FX, CAT_OTHER, CAT_N };
static const char *const CAT_NAME[CAT_N] = {"-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "DRUM", "FX", "OTHER"};
/* the category a sound without one is listed under: a kit (DRUM, SLICE: engines 10, 13) DRUM, any other OTHER */
static uint32_t cat_of_engine(uint32_t e) { return e == 10u || e == 13u || e == 24u ? CAT_DRUM : CAT_OTHER; }   /* (24: TR808) */

#ifndef UP_HOST
/* the factory presets' categories: per engine number (engines.c ENGINES[]), a letter per preset in its order (B BASS,
 * L LEAD, P PAD, U PLUCK, K KEYS, D DRUM, F FX, O OTHER); a preset past its string (DIGITAL's, FELUCCA_FM4 builds
 * only; SAMPLE's retired PERC, past SMP_NPRESETS): its engine's (cat_of_engine). tests/ui_test.c checks every string
 * against its engine's preset count. A preset's category is a hint for browsing: nothing else reads it */
static const char *const PRESET_CAT[NENGINES] = {
    [10] = "D",                /* TESTDRUM (the host tests' grid engine): DRUM KIT */
    [12] = "KUBLPUKU",              /* TESTSYNTH (the host tests' melodic engine, FELUCCA_TEST_ENGINE): the presets FM6 had (TEST EP, BELL, BASS, BRASS, PAD, MARIMBA, ORGAN, PLUCK) */
    [13] = "LPUBKPLBUBLPUKBF",   /* 13 WTSYNTH: SAW SWEEP, VOWEL PAD, FM BELLS, SUB WAVE, GLASS KEYS, DEEP SPACE, ROBO LEAD, PWM BASS,
                                  * CRYSTAL ARP, GROWL, FOLD LEAD, ORBIT PAD, BIT PLUCK, HOLLOW PIPE, DIRTY CLOCK, GRIT NOISE */
    [24] = "DDDDD",              /* TR808: 808 KIT, BOOM KIT, TIGHT KIT, DIRTY KIT, METAL KIT */
};
_Static_assert(sizeof PRESET_CAT / sizeof PRESET_CAT[0] >= NENGINES, "PRESET_CAT: a string for every engine");
static uint32_t preset_cat(uint32_t e, uint32_t k)    /* factory preset k of engine e */
{
    static const char L[] = "?BLPUKDFO";               /* (a letter's index is its CAT_*) */
    const char *s = e < NELEM(PRESET_CAT) && PRESET_CAT[e] ? PRESET_CAT[e] : "";
    uint32_t i, c;
    for (i = 0; i < k && s[i]; i++)
        ;
    for (c = 1; s[i] && L[c] && L[c] != s[i]; c++)
        ;
    return s[i] && L[c] ? c : cat_of_engine(e);
}
#endif
#endif

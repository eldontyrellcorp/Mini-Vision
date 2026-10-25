# mini-vision

**mini-vision 1.0.0** is a wavetable synthesizer firmware for the M-VAVE FM-1 (JieLi AC79), after Roland's n/vision. It is a fork of
Mini-Tone 1.0.4 (Stephane Schmitz), itself based on Felucca 1.5 by Leo Kuroshita (Hügelton Instruments), GPL-3.0-only. The specifications it
follows are gathered in `docs/NVISION_SPECS.md`; Mini-Tone's own release notes are in `docs/MINI_TONE_HISTORY.md`.

## What it is

- **WTSYNTH (engine 13, `firmware/src/eng_wt.c`) is the synth.** Two wavetable layers (WAV1, WAV2: 63 tables of 8 key frames, 5 mip levels,
  8 bit, made at build time by `tools/gen_nvision_wt.py` in five families: geometric, vowels, metallic, spectral, lo-fi), one POSITION through the
  frames, MIX and OCT2 between the layers, RANGE, a sine-fold wave shaper (WS), phase modulation (PM), and the voice filter: TYPE LP12 BP12 HP12,
  LP24 BP24 HP24 and three ladder low passes after a Roland, a Moog and a Sequential (LP.R LP.M LP.S), with CUT, RES, KEY and DRIVE 0..3;
  an oscillator AD envelope that moves the position and the mix (OSC ENV), and the track's ENV, LFO, modulation matrix and FX pages. 16
  presets; 8 voices shared by the six tracks. `tests/wt_test.c` checks it.
- **TR808** (engine 24) is kept: a TR-808 of 16 voices (see Engines).
- **Every other engine of Mini-Tone is gone.** Their engine numbers are kept (the stores and the editor protocol name engines by index): a sound
  that names one loads with its values and plays silence, and a user preset saved with one is no longer listed.
- **The interface follows the n/vision GUI design.** The NVISION palette and the LINE style by default, the graph panel on top and the four knob
  cards at the bottom, Chakra Petch (labels, titles) and JetBrains Mono (values), a header with the track, the play state, the tempo and POLY n/8.
  HOME is the PERFORM view. WTSYNTH's pages draw two layer boxes with a frame ruler, the wave shaper's curve, the oscillator envelope, the filter's
  response with its type chips, and the drive; the envelopes have handles, the LFO shape chips, FX is a chain of boxes. The SEQ pages are STEP,
  STEP 2, PATTERN, AUTOMATION, MOTION and STOCH.
- **Not yet** (the n/vision has them): the HPF, the wave shaper's tables, P-MOD's X / Y, the step LFO, the drawing mode, SPREAD, the EQ, the MFX
  effects, 56 voices (8 here). The web editor (`web/`) is still Mini-Tone's, with FM6 and the other engines in it.
- **Flash:** the app slot is 568.7 kB of 581.6 (12.9 kB left); the wavetables take 252 kB of the store region.
- **TR808 fix:** a tom, or the kick at a low TUNE and a long DECY, could settle into a limit cycle that never ended and kept its voice in
  the budget of 8; the voices now end late in a hit (`T8_TOM_TAIL_X`, `T8_BD_TAIL_QX`).

## Building and testing

`./build.sh` builds `build/felucca.bin` and the package `build/felucca.fwsc` (the JieLi toolchain: `tools/get_toolchain.sh`).
`sh tests/run_tests.sh` runs the host tests, with a melodic and a drum-grid test engine of their own (`firmware/src/eng_testsynth.c`,
`FELUCCA_TEST_ENGINE=1`, never in the firmware); `GOLDEN_UPDATE=1` rewrites the golden renders. Known failures: the web editor's mock-table checks
(`node web/test_web.mjs`), and `tests/settings_test.c:65` (a favourites check for the removed SAMPLE and DRUM engines).

## Features

- **Six tracks**, one synth part each with its own engine and sound; 8 voices shared between them. ALGORITHM selects the track on every page
- **Sequencer:** 64 steps per track with chords, ties, accent, slide, per-step chance, velocity, a force for each note, fill conditions and
  ratchets; a piano roll where KNOB 3 sets a note's length; automation (recording of knob moves per step), parameter locks, live loop
  recording with overdub, step recording; per-step NUDGE; SEQ TOOLS (CLEAR, REVERSE, SHIFT, RANDOM, COOK); STOCHASTIC and EUCLID (with the
  boolean second rhythm) modes; the motion lanes
- **Songs** (GLO > SONG): up to 16 sections, each naming the project slot (A-D) every track plays, with its repeats
- **Chord keys, arpeggiator, 16 scales** with a white-key mode, glide, MONO / LEGATO / UNISON
- **LFO** (RATE or SYNC, TRIG, POL), a **modulation matrix** of 4 slots per track with S&H and SLEW, **ENV SYNC**
- **Effects:** distortion, an INSERT (drives, CRUSH, phaser, flanger, chorus), the SLICER, chorus, a ping-pong stereo delay, a reverb
  (HALL, ROOM, SPRING, ELEK), a master limiter; the **FX layer** (repeat, reverse, filter sweeps, tape stop, freeze, harmonizer, flanger, phaser)
- **Quick layers** (hold FX, GLO, SCL, EDIT, REC or SEQ), MOMENTARY (hold LFO and turn a knob), one-step undo (SAVE held)
- **Presets:** factory presets, 32 user preset slots and 4 projects, named on the device; MENU > RESTORE LAST keeps the music across power cycles
- **Screen:** 11 palettes (NVISION, GREY, GREEN, AMBER, ICE, VIOLET, ROSE, PAPER, HI-CON, NIGHT, MONO), FLAT or LINE style, MENU > LARGE
  for tall knob cards, the menu in five tabs, MENU > HELP hints
- **LEDs, USB** (class-compliant MIDI and a stereo audio input at 44.1 or 48 kHz), **MIDI** (USB and TRS in, clock, CCs, MIDI LEARN)

## Controls

![FM-1 controls with Felucca](docs/controls.jpg)

- Tap a page button for its page, again for the next; HOME returns home. Page buttons act when let go
- Hold FX, GLO, SCL, EDIT, REC or SEQ for its quick layer; hold SAVE to undo, HOME for the menu; hold LFO and turn a
  knob for a MOMENTARY change
- Double-tap a layer's button to keep its layer open without holding; tap it again to close
- On SEQ > STEP (a melodic track) KNOB 1 is the note's **START**: a detent moves the note under the cursor by 1/64 of a step (its
  NUDGE, which plays with the track's QUANTIZE off) and, past the end of its step, on into the next one (its TIEs, chance, force,
  condition and locks with it) and so on, 64 detents a step, onto free steps only. The cursor moves with the PRESETS knob (and the
  grid's keys); on a step with no note KNOB 1 does nothing. KNOB 2 NOTE, KNOB 3 LEN, KNOB 4 FLAG as before. The drum grid keeps KNOB 1 as its cursor
- **STOCHASTIC mode:** tap SEQ to go on from AUTOMATION to the STOCH page and, with MODE on STOCH, its three others (NOTES, SHAPE, STACK), for the selected track.
  With MODE ON the track draws each step as it plays, from these settings; its pattern is not played and not changed. MODE OFF plays the pattern as before.
  - **STOCH:** MODE (KNOB 1, OFF / ON), DENS (the chance a step plays, 0..100 %), GROOVE (how much likelier the downbeats are than the
    offbeats), LOOP (FREE: a new draw every pass; 1..64: the same loop of that many steps every pass, for the same SEED)
  - **NOTES:** KNOB 1 picks a pitch class C..B (a DRUM track: a lane), KNOB 2 its WEIGHT 0..15 (0 never plays; a note outside the
    track's scale is marked OUT and is not heard), OCT (the lowest octave) and RANGE (1..4 octaves). The bars show the weights
    and a dot marks the notes of the step playing
  - **SHAPE:** VEL MIN and VEL MAX (each velocity is drawn between them), RAT (the chance of a ratchet x2..x4), SLIDE (the chance a note
    slides into the next)
  - **STACK:** the chance of each extra note of a chord (up to two more), and the SEED of the loop
- **EUCLID mode:** the EUCLID page follows the STOCH page when MODE is EUCLID (NOTES, SHAPE and STACK are then hidden, as EUCLID is with the other modes). MODE on the STOCH page (KNOB 1) steps OFF / STOCH / EUCLID: one
  at a time, the track's pattern rests untouched in both. With EUCLID the track plays the pattern's first note step (its notes,
  velocity, accent, slide and ratchet; C4, or lane 1 on a DRUM track, if the pattern holds none) on each hit of the rhythm.
  - **EUCLID:** LEN (KNOB 1, 1..64 steps: the rhythm's own loop, restarted when the song starts), PULSES (KNOB 2, 0..LEN: the hits, spread
    as evenly as they can be), ROT (KNOB 3, 0..LEN-1: the rhythm turned that many steps later). The row of steps shows the hits and
    the step playing
  - **EUCLID 2:** a second rhythm B with its own LEN, PULSES and ROT, combined with A step by step: OP (KNOB 1) is OFF, AND (both hit), OR (either),
    XOR (one of them) or SUB (A without B). B loops on its own length against A's, so the pair repeats after lcm(LEN A, LEN B) steps. The page shows A, B and the result
    (OFF keeps B's numbers; projects saved before load with B off)
  - On a STOCH page **OCT+ tapped is REROLL** (another SEED; with LOOP FREE every pass is new anyway) and **OCT+ held (0.4 s) is LOCK**: the
    LEN steps it would draw are written into the pattern, MODE goes OFF, and you edit them as any pattern (one undo, SAVE held)
  - Settings are saved with the project (in its extension; projects saved before load with the mode OFF), not in user presets
- **The sequencer's pages (mini-vision):** **STEP 2** edits the step under the cursor as the n/vision does: KNOB 1 STEP (the cursor), KNOB 2 VEL,
  KNOB 3 PROB (5 % a detent), KNOB 4 RATCH (x1..x4); the page draws the step's notes in four slots and bars for VEL, PROB and RATCH. **PATTERN**: LEN, DIV, SWG, GATE
  with the pattern drawn as up to four rows of 16 cells (the playhead yellow) and the swing and gate as bars. **MOTION**: the parameters the
  track's automation and locks move, up to four lanes, each a line over the pattern (a value holds until the next record; a lock is a dot);
  KNOB 1 picks the lane (read-only: the records are edited on AUTOMATION)
- **MENU > CONTROL > SELECT:** what the SELECT knob does. TEMPO (the default): the tempo, as before. PAGES: a detent steps to the next or previous page of the family shown (a wrap; from HOME, the EDIT pages), and the tempo is set with GLO held + SELECT, GLO's F4 TAP or HOME > CLOCK
- On SEQ > STEP, hold a step and turn KNOB 1–4 to lock those parameters on that step (PRESETS: its NUDGE)
- On SEQ > STEP, hold OCT+ for 0.4 s for the **sheet**, after [SLOOP](https://github.com/isod89/sloop-fm1) 2.8's NOTES page and its
  sheet. KNOB 1 picks a row (six show, they scroll), KNOB 2 sets it, PRESETS zooms the roll (8, 16, 32 or 64 steps), OCT+ tapped
  does SLIDE / COPY / DELETE, OCT− closes it; OCT+ tapped without it is the octave as before (on release). One undo per turn
  (SAVE held). On a melodic track the rows are **NOTE** (which note of the step's chord the next rows edit), **PITCH** (that note
  alone; on an empty step it writes your last note), **LEN** (the note and its TIEs), **FORCE** (that note's GHOST / SOFT / NORM /
  HARD, velocity 42 / 72 / step's / 127; ACC is the step's accent), **RATCH** (that note's own x2..x4, the others on the step's),
  **SLIDE**, **NUDGE** (a 64th of the step a detent, −32..+31; it plays with the track's QUANTIZE off, and live recording keeps
  the timing at the same resolution), **FILL** (NORMAL, FILL: the step plays only during a fill, NO FILL: only outside one),
  **MOVE** (the note a step on or back with its TIEs, chance, nudge, force, condition and parameter locks, onto free steps only),
  **COPY** (just after it, then MOVE moves the copy) and **DELETE** (the picked note; the last one takes the TIEs and locks too).
  On the DRUM grid the sheet works on the hit of the lane at the cursor: **ACC**, **RATCH**, **NUDGE** and **FILL** (the last three
  are the step's), **MOVE**, **COPY**, **DELETE**; the white keys play while it is open
- **Fills:** hold GLO and **E4** for a fill while it is held, or tap **D#4** to make the next bar a fill (again: cancelled).
  Steps set to FILL play only then, steps set to NO FILL only outside it; STOP ends it
- While a song plays, each row of HOME > TRACKS shows the letter (A..D) of the slot its section plays that track from
- The extra per-step data (force, ratchet per note, condition, finer nudge) is saved with the project (in its extension, older
  projects load as they were); a user preset's pattern and the web editor's steps keep none of it
- On action pages and in dialogs, OCT+ does it and OCT− goes back; in the menu, OCT+ / OCT− change the value and HOME closes it
- Save a sound: stop, tap SAVE, pick a slot with KNOB 1, then OCT+ and OCT+ again (name it with the keys)

The pages by button:

| Button | Taps go round | Held |
|---|---|---|
| HOME | HOME, MIXER, CLOCK (BPM, swing, clock source) | the menu |
| SEQ | STEP (the piano roll), STEP 2, PATTERN, AUTOMATION, MOTION, STOCH | SEQ TOOLS, on every page |
| GLO | SONG | the GLO layer (mutes, solos, levels, TAP tempo, D4: MIDI LEARN) |
| SAVE | USER, PRESETS, PHRASES (the pattern loader), PROJECT, TOOLS | UNDO |
| LFO | LFO, LFO 2, LFO DEST, MOD | MOMENTARY (with a knob) |
| EDIT | WTSYNTH: EDIT 1 (WAV1 WAV2 POS MIX), EDIT 2 (OCT2 RANGE WS PM), OSC ENV, FILTER, FILTER 2 (DRIVE), VOICE pages; TR808: EDIT 1, EDIT 2, LEVELS 1..4 | the EDIT layer (the engines) |
| FX, SCL, ENV, ARP | their pages (FX: INSERT and INSERT 2 after the SLICER) | FX and SCL: their layers |
| REC | arms the track | the REC layer (CLEAR, CLICK, COUNT-IN, CLICK LEVEL) |

## Menu

Hold **HOME** for the menu, in five tabs: **DISPLAY** (COLOR, STYLE, LARGE, ANIM, LEDS, SCREEN OFF, SCOPE, HOME),
**CONTROL** (HOLD, KNOB ACCEL, FX LATCH, BPM LOCK, SCALE LEDS, STEP PREVIEW, CHORD ENTRY), **AUDIO** (SPEAKER EQ,
USB LEVEL, CLICK, CLICK LEVEL, COUNT-IN, TUNE), **MIDI** (MIDI IN, LEARN CLEAR) and **SYSTEM** (USB SERIAL,
RESTORE LAST, HELP, HARDWARE CALIBRATION, INFO, ABOUT).
ALGORITHM moves between the tabs and PRESETS through the rows of one; any of KNOB 1–4, or OCT+ / OCT−,
changes the value (OCT+ opens HARDWARE CALIBRATION, INFO and ABOUT); press HOME to close the menu. Every new
setting defaults to the earlier behaviour. The web editor's Settings tab reads and changes them too, saved the same
way as from the menu, in the same tabs.

- **COLOR:** the palette. GREY (grayscale, called MONO up to 1.0.1), GREEN, AMBER, ICE, VIOLET, ROSE,
  PAPER (light), HI-CON (high contrast), NIGHT (the 0.9 look: true black, green-tinted text) and MONO
  (black and white)
- **STYLE:** FLAT (filled cards) or LINE (areas divided by thin lines)
- **LARGE:** OFF or ON: on HOME and the value pages the four knob cards grow tall, marked K1–K4, with
  larger labels and values about twice the size; the graph below becomes a strip. Lists, the piano roll,
  the drum grid and the quick layers keep their layout with larger labels
- **ANIM:** ON, OFF or IDLE. OFF shows every change at once, without rolling digits, a gliding piano roll or
  the LED sweep at power-on. IDLE is ON plus a gentle LED animation after a minute without input: a soft light drifts
  over the keys and the buttons breathe; any key, button or knob ends it and does what it always does. It never runs
  while playing, with a track armed, in a layer, the menu or a dialog
- **LEDS:** OFF (no glow), DIM LO, DIM HI (default) or INV (the idle LEDs lit, the active ones dark).
  What can be pressed breathes up to about 60 % of a lit LED, about 30 % with DIM LO
- **SCREEN OFF:** NEVER (default), 5 MIN, 15 MIN, 30 MIN or 60 MIN: after that long with no button, key or knob
  touched, the screen goes dark; the sound, the sequencer, MIDI and USB go on. The backlight stays on (on the FM-1
  its line also enables the buttons and keys). The next button, key or knob only turns the screen back on
- **SCOPE:** OUT (default) or MIX: what HOME's oscilloscope shows. OUT the sound after MASTER (the picture shrinks
  with the volume), MIX the mix before MASTER, the same size at any volume
- **HOME:** SCOPE (default), TRACKS or LEVELS: under HOME's four cards, the oscilloscope or the four tracks, a row
  each with its sound, its steps playing, MUTE and a meter. With TRACKS only the look changes; with LEVELS KNOB 1–4
  set the LEVEL of T1–T4 (the engine's main parameters are on EDIT)
- **HOLD:** how long a layer's button is held before its map shows
- **KNOB ACCEL:** OFF (one step per click) or ON: a fast, steady turn of a wide value moves 2 to 4 steps
  per click, up to 8 on values of more than 64 steps, the FX and GLO layers' knobs included; lists never
  jump. ON or OFF, every click of a fast turn counts
- **FX LATCH:** ON, FX + an effect key turns the effect on until pressed again, the knob macros stay
  where you leave them, and FX + OCT− turns everything off
- **BPM LOCK:** ON, SELECT no longer changes the tempo; hold GLO and turn SELECT, tap F4 in the GLO
  layer or use HOME > CLOCK instead
- **SCALE LEDS:** OFF or ON: the keys show the selected track's scale all the time (the root a little
  brighter); not on DRUM or SLICE tracks
- **STEP PREVIEW:** OFF (default) or ON: on SEQ > STEP with the transport stopped, moving the cursor plays the step
  it lands on once (its notes or hits, through the track's sound)
- **CHORD ENTRY:** HOLD (default) or ADD: how step recording writes. HOLD: the keys held together become the step,
  letting go moves on. ADD: each key adds its note to the step, up to 4 (a key whose note is there takes it out),
  and KNOB 1 moves on, so a chord goes in one finger at a time
- **SPEAKER EQ:** FLAT, LOWCUT or BASS+, a tone setting for the built-in speaker (not a switch). It also
  shapes the headphone out and USB audio, so keep it on FLAT when recording. The firmware cannot turn
  the speaker off, but headphones in the jack do
- **USB LEVEL:** MASTER (the MASTER knob sets the USB audio level too) or FIXED (USB always at full
  level, MASTER sets only the speaker and headphones)
- **CLICK:** OFF (default), REC (while a track is armed) or ON: a click on every beat while playing, the
  bar's first beat higher. **CLICK LEVEL:** LOW, MID (default) or HIGH; MASTER sets it too
- **COUNT-IN:** OFF (default), 1 BAR or 2 BARS: with a track armed, PLAY counts in first (clicking even with
  CLICK OFF), and a note played in the last half beat lands on step 1. Not with an external clock
- **TUNE:** the global tuning, −50 to +50 cents. It is the project's, saved and loaded with it
- **MIDI IN:** CH1-4 (default), SEL, CH5-8, CH9-12 or CH13-16 (see MIDI above). The project's too
- **LEARN CLEAR:** how many CCs MIDI LEARN has set (NONE, 1 CC … 16 CC); OCT+ clears them all
- **USB SERIAL:** ON or OFF, applied when the menu closes (the FM-1 reconnects). OFF leaves out the
  serial console, a developer tool, so the FM-1 is a plain audio + MIDI device; this lets macOS 13–15
  see its USB audio input. MIDI, the editor and the installer work either way
- **RESTORE LAST:** ON (default) or OFF. ON, the FM-1 starts with the music you left (the header says
  RESTORED), kept in an autosave apart from the four projects; it is written when the music changed, the
  transport is stopped and nothing has been touched for 10 s, at most once a minute. OFF starts with the
  power-on sounds
- **HELP:** OFF (default) or ON: a short hint in the footer for about 2 s when a page opens, and under a quick
  layer's map while it shows
- **HARDWARE CALIBRATION**, **INFO** (the version, the USB link and the CPU load) and **ABOUT**


## Menu

Hold **HOME** for the menu, in five tabs: **DISPLAY** (COLOR, STYLE, LARGE, ANIM, LEDS, SCREEN OFF, SCOPE, HOME),
**CONTROL** (HOLD, KNOB ACCEL, FX LATCH, BPM LOCK, SCALE LEDS, STEP PREVIEW, CHORD ENTRY), **AUDIO** (SPEAKER EQ,
USB LEVEL, CLICK, CLICK LEVEL, COUNT-IN, TUNE), **MIDI** (MIDI IN, LEARN CLEAR) and **SYSTEM** (USB SERIAL,
RESTORE LAST, HELP, HARDWARE CALIBRATION, INFO, ABOUT).
ALGORITHM moves between the tabs and PRESETS through the rows of one; any of KNOB 1–4, or OCT+ / OCT−,
changes the value (OCT+ opens HARDWARE CALIBRATION, INFO and ABOUT); press HOME to close the menu. Every new
setting defaults to the earlier behaviour. The web editor's Settings tab reads and changes them too, saved the same
way as from the menu, in the same tabs.

- **COLOR:** the palette. NVISION (the default: the n/vision design's dark blue-black with cyan, pink, yellow and green), GREY (grayscale, called MONO up to 1.0.1), GREEN, AMBER, ICE, VIOLET, ROSE,
  PAPER (light), HI-CON (high contrast), NIGHT (the 0.9 look: true black, green-tinted text) and MONO
  (black and white)
- **STYLE:** FLAT (filled cards) or LINE (areas divided by thin lines; the default on a fresh start)
- **LARGE:** OFF or ON: on HOME and the value pages the four knob cards grow tall, marked K1–K4, with
  larger labels and values about twice the size; the graph below becomes a strip. Lists, the piano roll,
  the drum grid and the quick layers keep their layout with larger labels
- **ANIM:** ON, OFF or IDLE. OFF shows every change at once, without rolling digits, a gliding piano roll or
  the LED sweep at power-on. IDLE is ON plus a gentle LED animation after a minute without input: a soft light drifts
  over the keys and the buttons breathe; any key, button or knob ends it and does what it always does. It never runs
  while playing, with a track armed, in a layer, the menu or a dialog
- **LEDS:** OFF (no glow), DIM LO, DIM HI (default) or INV (the idle LEDs lit, the active ones dark).
  What can be pressed breathes up to about 60 % of a lit LED, about 30 % with DIM LO
- **SCREEN OFF:** NEVER (default), 5 MIN, 15 MIN, 30 MIN or 60 MIN: after that long with no button, key or knob
  touched, the screen goes dark; the sound, the sequencer, MIDI and USB go on. The backlight stays on (on the FM-1
  its line also enables the buttons and keys). The next button, key or knob only turns the screen back on
- **SCOPE:** OUT (default) or MIX: what HOME's oscilloscope shows. OUT the sound after MASTER (the picture shrinks
  with the volume), MIX the mix before MASTER, the same size at any volume
- **HOME:** PERFORM (default), TRACKS, LEVELS or SCOPE: under HOME's four cards, the PERFORM view (the patch number and name, the wavetable frames with the one at POSITION lit, the layers, the filter type), the four tracks, a row
  each with its sound, its steps playing, MUTE and a meter, or the oscilloscope. With TRACKS only the look changes; with LEVELS KNOB 1–4
  set the LEVEL of T1–T4 (the engine's main parameters are on EDIT)
- **HOLD:** how long a layer's button is held before its map shows
- **KNOB ACCEL:** OFF (one step per click) or ON: a fast, steady turn of a wide value moves 2 to 4 steps
  per click, up to 8 on values of more than 64 steps, the FX and GLO layers' knobs included; lists never
  jump. ON or OFF, every click of a fast turn counts
- **FX LATCH:** ON, FX + an effect key turns the effect on until pressed again, the knob macros stay
  where you leave them, and FX + OCT− turns everything off
- **BPM LOCK:** ON, SELECT no longer changes the tempo; hold GLO and turn SELECT, tap F4 in the GLO
  layer or use HOME > CLOCK instead
- **SCALE LEDS:** OFF or ON: the keys show the selected track's scale all the time (the root a little
  brighter); not on DRUM or SLICE tracks
- **STEP PREVIEW:** OFF (default) or ON: on SEQ > STEP with the transport stopped, moving the cursor plays the step
  it lands on once (its notes or hits, through the track's sound)
- **CHORD ENTRY:** HOLD (default) or ADD: how step recording writes. HOLD: the keys held together become the step,
  letting go moves on. ADD: each key adds its note to the step, up to 4 (a key whose note is there takes it out),
  and KNOB 1 moves on, so a chord goes in one finger at a time
- **SPEAKER EQ:** FLAT, LOWCUT or BASS+, a tone setting for the built-in speaker (not a switch). It also
  shapes the headphone out and USB audio, so keep it on FLAT when recording. The firmware cannot turn
  the speaker off, but headphones in the jack do
- **USB LEVEL:** MASTER (the MASTER knob sets the USB audio level too) or FIXED (USB always at full
  level, MASTER sets only the speaker and headphones)
- **CLICK:** OFF (default), REC (while a track is armed) or ON: a click on every beat while playing, the
  bar's first beat higher. **CLICK LEVEL:** LOW, MID (default) or HIGH; MASTER sets it too
- **COUNT-IN:** OFF (default), 1 BAR or 2 BARS: with a track armed, PLAY counts in first (clicking even with
  CLICK OFF), and a note played in the last half beat lands on step 1. Not with an external clock
- **TUNE:** the global tuning, −50 to +50 cents. It is the project's, saved and loaded with it
- **MIDI IN:** CH1-4 (default), SEL, CH5-8, CH9-12 or CH13-16 (see MIDI above). The project's too
- **LEARN CLEAR:** how many CCs MIDI LEARN has set (NONE, 1 CC … 16 CC); OCT+ clears them all
- **USB SERIAL:** ON or OFF, applied when the menu closes (the FM-1 reconnects). OFF leaves out the
  serial console, a developer tool, so the FM-1 is a plain audio + MIDI device; this lets macOS 13–15
  see its USB audio input. MIDI, the editor and the installer work either way
- **RESTORE LAST:** ON (default) or OFF. ON, the FM-1 starts with the music you left (the header says
  RESTORED), kept in an autosave apart from the four projects; it is written when the music changed, the
  transport is stopped and nothing has been touched for 10 s, at most once a minute. OFF starts with the
  power-on sounds
- **HELP:** OFF (default) or ON: a short hint in the footer for about 2 s when a page opens, and under a quick
  layer's map while it shows
- **HARDWARE CALIBRATION**, **INFO** (the version, the USB link and the CPU load) and **ABOUT**


## Engines

- **WTSYNTH**: the wavetable synth described above. **EDIT 1** WAV1, WAV2 (the tables), POS, MIX; **EDIT 2** OCT2, RANGE (16' 8' 4' 2'), WS, PM;
  **OSC ENV** A, D, TO POS, TO MIX; **FILTER** TYPE, CUT, RES, KEY; **FILTER 2** DRIVE.
- **TR808**: a TR-808 of 16 voices after the circuit models of the 8W8 module for Ableton Move (<https://github.com/athousanddetails/schwung-8W8>,
  GPL-3.0), rewritten in fixed point: the bass drum (a bridged-T resonator in an op-amp loop: DECY is its loop gain, with the pitch sigh and the
  attack-time jump), the snare (two shells at 173 and 336 Hz and a noise path through a swing-type VCA), the three toms and three congas
  (the same resonator, struck two ways), the rim shot and the claves, the maracas, the hand clap (three bursts and a tail), the cowbell, the
  closed and the open hat and the cymbal (one bank of six Schmitt oscillators, shared, through each voice's own filters and gate). General MIDI
  notes (35..81) strike the voices; a hit on a drum still ringing adds to it, as on the machine. `tests/tr808_test.c` renders every voice
  and compares it with the 8W8's own render of it (peak, level, pitch). Not in it: 8W8's reverb, delay, compressor and per-voice distortions
  (the track's sends, DIST and INSERT do those). **EDIT 1** TUNE (all voices, +-2 semitones for the toms and congas, +-12 for the others;
  64 is the machine's), DECY (each voice's own decay pot moved together), BTON and BATK (the kick's tone and attack), **EDIT 2** SNAP (the
  snare's snappy), DRV (a soft clip), VEL (how far velocity moves the level), CHOK (OFF, CH>OH, BOTH: the hats are one hat), **LEVELS 1..4**
  the 16 levels (BD SD LT MT, HT LC MC HC, RS CL MA CP, CB CH OH CY). Five kits: 808 KIT, BOOM KIT, TIGHT KIT, DIRTY KIT, METAL KIT

## Scale keyboard

On the **SCL** page, set **QNT** to WHITE to play the selected scale using only the
white keys (SNAP keeps every key and rounds it down to the scale). C4 plays **ROOT**; consecutive white keys play consecutive scale notes
above and below it. Black keys are silent, including during live recording and
step entry. **TRN** transposes the resulting notes; the octave buttons shift them
by full octaves. Set QNT to OFF for the normal chromatic keyboard. QNT SEQ snaps the keys like SNAP and
also maps the sequenced notes onto the current ROOT / SCALE as they play (the steps are not changed; drum
kits are never quantized).

Available scales: chromatic (CHR), major (MAJ), natural minor (MIN), Dorian (DOR),
Mixolydian (MIX), major pentatonic (PEN), minor pentatonic (MPEN), harmonic minor
(HARM), Phrygian (PHRY), Lydian (LYD), Locrian (LOC), ascending melodic minor (MEL),
minor blues (BLUES), whole tone (WHOLE), half-whole diminished (DIMHW), and
whole-half diminished (DIMWH). Scales with other than seven notes continue across
the white keys without repeating notes; their roots need not fall on every C key.
Drum kits and incoming MIDI keep their own note mapping.

Press **SCL** again for the **CHORD** page: CHRD picks the chord keys (OFF, the scale's triads or
sevenths, or a fixed shape) and VOIC the voicing.

## Layout

| Path | What |
| --- | --- |
| `firmware/` | firmware sources: `src/` app, `hal/` hardware layer, `loader/` update loader |
| `tools/` | build script, generators, package maker, installer and sample uploader |
| `assets/` | UI font, icon names, CC0 instrument samples |
| `web/` | the web installer, the earlier editor and the browser emulator (`web/emu/`); the new editor: [Felucca-WebApp](https://github.com/hugelton/Felucca-WebApp) |
| `tests/` | tests that run on the build machine |
| `LICENSES/` | licence texts of the bundled fonts, icons, ported DSP and SDK files |

## If the FM-1 does not start

If an update is interrupted and the FM-1 stays black, check whether a computer sees it as a USB device named
**WL80UBOOT** (or a USB mass-storage device with ID 4C4A:8057). That is the chip's built-in boot mode, and
the FM-1 can be brought back:

- First try another USB data cable, and close every other app that uses MIDI, then run the web installer again.
- If it stays in boot mode, [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) reads and
  writes the FM-1's flash from a Mac through a Seeed XIAO RP2040 (three wires to the FM-1's USB lines). Back up
  the flash first, then write the official firmware (M-VAVE's FM-1.fwsc).
- Questions: [Issues](https://github.com/hugelton/Felucca/issues).

## Support

If Felucca is useful to you, [sponsoring on GitHub](https://github.com/sponsors/hugelton) or a donation
on [itch.io](https://hugelton.itch.io/felucca) helps keep its development going.

Issues are for reproducible bugs (one per issue). Ideas and requests go to
[Discussions](https://github.com/hugelton/Felucca/discussions), and feature requests posted as issues will be
moved there. Pull requests are welcome: see [CONTRIBUTING.md](CONTRIBUTING.md).

## AI disclaimer

Felucca is developed with the assistance of AI coding agents. These tools are used for coding, testing, documentation, translation, and maintenance.
The instrument's design, features, sound design, and overall direction are determined by the maintainer or community.
No generative AI is used to create music, icons, or visual artwork for this project.
For more details, see [On AI-Assisted Development and Responsibility](https://github.com/hugelton/Felucca/discussions/166).

## Credits

- **Stephane Schmitz**: Mini-Tone and mini-vision.
- **[Hügelton Instruments](https://hugelton.com)** (Leo Kuroshita, [@kurogedelic](https://github.com/kurogedelic)):
  Felucca itself; the Hügelton Sample
  Pack (the drum samples, GPL-3.0-only, not CC0); the [Fukiai](https://github.com/hugelton/Fukiai) icon
  font ([MIT](LICENSES/MIT-Fukiai.txt))
- Fonts: [Chakra Petch](https://github.com/cadsondemak/chakra-petch) by Cadson Demak and [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) by The JetBrains Mono Project Authors, [SIL OFL 1.1](assets/fonts/OFL-ChakraPetch.txt) ([JetBrains](assets/fonts/OFL-JetBrainsMono.txt)); [Inter Tight](https://github.com/rsms/inter-tight) by The Inter Project Authors, [SIL OFL 1.1](assets/fonts/OFL.txt) (the earlier UI font, still a build preset);
  the browser emulator's labels: [DotGothic16](https://github.com/fontworks-fonts/DotGothic16) by The DotGothic16 Project Authors, [SIL OFL 1.1](LICENSES/OFL-DotGothic16.txt)
- Samples: [Versilian Studios](https://versilian-studios.com/) [VSCO-2 Community Edition](https://github.com/sgossner/VSCO-2-CE) and [VCSL](https://github.com/sgossner/VCSL), CC0 1.0: the SAMPLE sets, also SLICE's PIANO and PIANO HD ([attribution](assets/samples-cc0/ATTRIBUTION.txt))
- Browser emulator: after [X0X](https://github.com/charlesvestal/fm1-x0x) by [charlesvestal](https://github.com/charlesvestal) (GPL-3.0), a Felucca fork whose browser build showed the way
- Sequencer sheet, fills and per-note editing: after [SLOOP](https://github.com/isod89/sloop-fm1) 2.8 by isod89 (GPL-3.0), itself a Felucca fork
  (its NOTES page after Melodee by [keremimo](https://github.com/keremimo))
- Package format and boot files: [JieLi AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK) ([Apache-2.0](LICENSES/Apache-2.0.txt); three of its files are in every package, none in this tree)
- Contributions: [keremimo](https://github.com/keremimo) (white-key scales, #2), [ChanceTheMaker](https://github.com/ChanceTheMaker)
  (TRS MIDI, bend, sustain and clock, palettes, favourites, editor display settings: #8, #10, #11, #12),
  [andreahaku](https://github.com/andreahaku) (sample recording and trim, #29; SLICE manual slices and tests, #27, #22),
  [spinkham](https://github.com/spinkham) (the boot fix for 0.9 projects, #111),
  [zednaked](https://github.com/zednaked) (ratchets, #100),
  [jasonpersinger](https://github.com/jasonpersinger) (the ROOM reverb click fix, #121)

## Licence

Free software: [GPL-3.0-only](LICENSE), the Hügelton Sample Pack included. The bundled fonts and the
ported DSP keep their own licences ([LICENSES/](LICENSES/)); details in [LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their respective owners. Felucca is not affiliated with or endorsed by them.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments

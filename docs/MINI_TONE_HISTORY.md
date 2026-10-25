# Mini-Tone history (before the mini-vision fork)

The release notes of Mini-Tone 1.0.0 .. 1.0.4, kept from the README the fork started from.

## Mini-Tone 1.0.4

What changed since 1.0.3:

- **EUCLID 2, a second page for the EUCLID mode** (after the Digitone 2's): a second rhythm B with its own LEN, PULSES and ROT, combined
  with A step by step by a boolean operator. OP (KNOB 1) is OFF, AND (both hit), OR (either), XOR (one of them) or SUB (A without B). B loops
  on its own length against A's, so the pair repeats after lcm(LEN A, LEN B) steps. The page draws A, B and the result, the step playing outlined
  (OFF keeps B's numbers). It shows with EUCLID on, after the EUCLID page. B is saved with the project in bytes of the stochastic settings that were
  unused, so the project format and its size do not change and projects saved before load with B off
- **DUALVCO's CNFG explained on screen**: the MIX page keeps the scope, shorter, and under it says what the picked configuration is, for
  example `*S+T` reads "SINE + TRIANGLE / OSC 1 AND OSC 2 MIXED / PHASES RESTART AT EACH NOTE" and `ZFMZ` "SINE/SAW FM SINE/SAW / FM: OSC 2
  MODULATES OSC 1 / PHASES RUN ON". The letters are the waves (S sine, K bent sine, T triangle, Z sine/saw, A saw), then the mode (`+` mix,
  `R` ring, `FM`, `F+R` FM then ring modulated by oscillator 2), and a `*` first means the phases restart at each note. The text is built
  from the CNFG number, so it costs next to no flash

## Mini-Tone 1.0.3

What changed since 1.0.2:

- **A new TR808 engine**: a TR-808 of 16 voices (bass drum, snare, three toms and three congas, rim shot, claves, maracas, clap, cowbell,
  closed and open hats, cymbal), the circuit models of the 8W8 module for Ableton Move (GPL-3.0) rewritten in fixed point and checked against
  its own renders (Engines, below). General MIDI notes strike the voices, a hit on a ringing drum adds to it, and a closed hat cuts the open one
- **EUCLID mode** (after the Digitone 2's): a track plays an Euclidean rhythm E(PULSES, LEN) turned by ROT, in a loop of its own
  (a polymeter against the pattern's LEN), instead of its pattern (SEQ > EUCLID, one layer; exclusive with STOCHASTIC)
- **SEQ shows the pages of the track's generator mode only**: OFF the STOCH page (its MODE), STOCHASTIC its four, EUCLID STOCH and EUCLID
- **The web editor follows the firmware again**: six tracks, the 25 engine numbers, the 111-parameter layout (LFO 2, the INSERT, SPREAD,
  QUANTIZE, ENV SYNC), each engine's own EDIT pages, the delay TIME as the device shows it; `web/sync_mock.mjs` regenerates its tables from the
  firmware (`tests/descdump.c`) so they cannot drift. The editor protocol names enums up to 32 values (24 before, the engine list has 25)
- **Fixes**: the engine list (MENU / the editor) had no name for SLICE's number, so every engine after it was shifted by one name and TB303 could
  not be picked from it; the host tests for the old four-track formats, the removed engines and STOCHASTIC mode run again (the suite fails only
  on one BRAIDS hash that depends on the host's libm)

## Mini-Tone 1.0.2

What changed since 1.0.1:

- **Sloop 2.8's sequencer features**, ported onto Mini-Tone's own steps (credits at the end):
  - a **sheet** on SEQ > STEP and on the drum grid: hold OCT+ for NOTE, PITCH, LEN, FORCE, RATCH, SLIDE, NUDGE, FILL, MOVE, COPY and DELETE
    (the drum grid's: ACC, RATCH, NUDGE, FILL, MOVE, COPY, DELETE); PRESETS zooms the roll to 8, 16, 32 or 64 steps
  - **per-note force** (GHOST, SOFT, NORM, HARD, ACC) and **per-note ratchet** (each note of a chord x2..x4 on its own)
  - **FILL conditions**: a step plays only during a fill, or only outside one; hold GLO + E4 for a fill, GLO + D#4 for the next bar
  - **nudge in 64ths of a step** (it was sixteenths); live recording keeps your timing at that resolution; on SEQ > STEP **KNOB 1 is the
    note's START**, a 64th a detent, on into the next step and so on (PRESETS now moves the cursor)
  - the **playing section's letter** (A..D) on each row of HOME > TRACKS while a song plays
  - the extra step data is saved with the project (in its extension; projects of 1.0.0 load as they were), not with user presets
    or in the web editor
- **STOCHASTIC mode** (after the OXI ONE's): a track generates its steps as they come, from odds you set, instead of playing its pattern
  (SEQ > STOCH, NOTES, SHAPE, STACK; see Controls). The pattern rests untouched, and LOCK writes what it draws into the pattern
- **KNOB 1 on SEQ > STEP is the note's START**, a 64th of a step a detent and on into the next step (see Controls)

## Mini-Tone 1.0.1

What changed since 1.0.0:

- **A new SID engine**: a port of reSID 0.16 (bit-exact waveform generator, the 6581 / 8580 filters, the combined waveforms of the real chips),
  with a MODEL switch and 26 presets that show what it can do (Engines, below)
- **A shared resonant filter** in the Elektron engines (SWARM, FMTONE, WAVETONE, CHIP, DUALVCO, ANALOG4): CUT, RES and KEY, TYPE LP / BP / HP, up
  to self-oscillation that follows the keys, on a FILTER page of its own (SWARM, FMTONE, WAVETONE, CHIP, DUALVCO) or in ANALOG4's two filters; 10 new presets.
  A fix with it: an engine that fades out on a switch now keeps its own operator values, so the switch no longer clicks
- **Delay TIME in dotted notes and milliseconds**, and **LEN any length from 1 to 64** (7 against 16 is a polyrhythm)
- **FMTONE's algorithm diagram** on its EDIT pages, as FM6 has

## Mini-Tone 1.0.0

Mini-Tone is Stephane Schmitz's firmware, based on Felucca 1.5 by Leo Kuroshita (Hügelton Instruments), GPL-3.0-only. What
it changes:

- **Six tracks** instead of four (the voice budget stays 8, shared). The panel shows four at a time: the MIXER, HOME
  TRACKS and LEVELS, the GLO layer's levels, solos and mutes and the FX layer's mutes follow a window of four tracks,
  tracks 1..4, or 3..6 when track 5 or 6 is selected. Tracks 5 and 6 are saved with each project slot in an extension
  (flash 0xA0000..0xA9FFF); a project saved by Felucca loads with them at their power-on sound
- **New engines**: SID (MOS 6581 / 8580, after reSID), CHIP (Elektron's SY CHIP), DUALVCO (Elektron's SY DUAL VCO), ANALOG4 (Elektron's Analog
  Four), BRAIDS (Mutable Instruments' Braids: its 48 models), SWARMER (Digitone 2), FMTONE (Digitone 2), WAVETONE (Digitone 2), FMDRUM (Digitone 2) 
  and TB303 (Roland's TB-303); the engines ANALOG, PHASE, LOFI, SAMPLE, VOICE, TRIO, WHEEL, GRAIN, PHYS, NOISE and SLICE (and the user sample slots) are removed
- **A ping-pong stereo delay** (MENU > AUDIO > PING PONG, DLY HALF: the line at half the sample rate for twice the time)
  with a **TIME** of 16 note values (4BAR to 1/32, triplets, and the dotted 1/1D 1/2D 1/4D 1/8D 1/16D 1/32D) and then **1 to 1486 ms** off the
  grid (the tempo does not move them); the time glides to a new TIME (8 samples a block, a tape's glide) instead of clicking
- **The ELEK reverb** (FX > REVERB TYPE ELEK, pages ELEK REV and ELEK REV 2: PRE DEC FRQ GAIN HPF LPF VOL)
- **LEN any length, 1 to 64**, a step a detent (the knob went in blocks of 16): 7 against 16, 15 against 4 are polyrhythms, each track cycling on its own
  (tests/poly_test.c); themes by MENU > DISPLAY > COLOR
- MIDI IN: CH1-6, SEL, CH5-10, CH9-14, CH13-16 (the block of channels the six tracks follow)


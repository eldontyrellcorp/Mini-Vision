# Roland n/vision: specifications gathered (2026-10-25)

Sources: Roland's product page (https://www.roland.com/us/products/n_vision/), the GearNews review
(https://www.gearnews.com/roland-n-vision-review-synth/), SonicState (https://sonicstate.com/news/2026/10/20/nvision-rolands-first-wavetable-hardware-synth/),
KVR, Andertons, Thomann, Perfect Circuit, Bonedo (search results only). Part 2 below comes from Roland's online Reference Manual v1.00 (https://static.roland.com/manuals/nvision_reference_v100/en-US/index.html; HTML only, no PDF
link; it lists NO default values). Where part 1 and part 2 differ, part 2 is the manual. Items marked (?) differ between sources or are unconfirmed.

## What it is
A Boutique-format (compact, 4 AA or USB-C powered) two-layer wavetable synthesizer with a built-in speaker, a 4-character 7-segment display and the
K-25m 25-key velocity-sensitive keyboard dock (included; it also tilts the panel). Said to build on the principles of the Enzyme synth engine of
the Fantom EX (SonicState). System program 1.01 at the time of the Roland page. Shipping from the end of October 2026.

## Sound engine
- Two wavetable layers per voice (Layer 1, Layer 2); each has 63 wavetables x 127 positions (63 factory wavetables in all)
- LAYER MIX crossfader, modulated by the envelope and / or the LFO (MIX ENV, MIX LFO)
- Layer 2 detunes against Layer 1 in octaves only (LAYER2 OCT switch); Layer 1 has a RANGE switch
- One POSITION (slider) shared by both layers; modulated by an LFO, an envelope, or RANDOM (POSITION RANDOM, POSITION LFO, POSITION ENV)
- Phase modulation, in a menu; a menu-based Drawing Mode; no user wavetable import (yet)
- Roland wave shaper, global: non-linear, adds harmonics; WAVE SHAPER, W.S LFO, W.S ENV knobs; 15 selectable shaping tables
- Polyphony: up to 56 voices (Roland, GearNews) or ~50 shared between layers (SonicState) (?); varies with load; the 24 dB filter modes use more
  voices, so polyphony drops there
- Two parts, Upper and Lower, with DUAL and SPLIT modes; SPREAD (stereo spread of the layers); PORTAMENTO (button and knob); HOLD

## Filter
- Analog-modelling, with DRIVE (three steps, SonicState; three distortion styles, GearNews)
- Modes: LP, BP, HP, each 12 or 24 dB, plus three vintage LP types (Roland, M-style, S-style) = 9 modes (GearNews); TYPE button
- Controls: FREQ, RES, ENV amount, LFO amount (sliders), HPF (knob, a resonance-free high-pass), KYBD tracking (knob)

## Envelopes
- Three ADSR envelopes (oscillator / position, filter, amp) sharing one set of ATTACK DECAY SUSTAIN RELEASE sliders; ENV SELECT picks which, or all together

## LFO
- One LFO: WAVE (six waveforms), RATE, AMP, DELAY TIME, KEY TRIGGER; clock sync and key sync
- Targets from the panel: volume, wavetable position, layer mix, pitch (PITCH LFO), waveshaper, filter cutoff
- STEP LFO: rhythmic modulation steps, programmed in the menu
- Other sources (menu assignable): mod wheel, velocity. No aftertouch, even via MIDI

## Effects
- MFX: 17 types (Roland): chorus x3 (incl. the Juno chorus), delay x4 (incl. reverse delay), phaser x2, pitch shifter x2, overdrive, fuzz, drive,
  bit crusher, lo-fi compressor, filter ("Super Filter"); reached with the CHORUS I / II buttons (Bonedo counts 19) (?)
- Dedicated DELAY (DELAY, TIME, FEEDBACK sliders) and REVERB (REVERB, TIME knobs): five reverb types and a SHIMMER mode (octave-shifted colour), up to 10 s

## Sequencer and arpeggiator
- Polyphonic step sequencer, two tracks (Upper / Lower), 64 steps, up to 8 notes per step; step entry and real-time recording
- Per-step gate time and velocity; motion recording of controller movements; playback direction, shuffle, pattern randomizer (menu)
- Arpeggio function (ARPEGGIO button); SEQ and START buttons; external clock in
- Memory: 240 sound patches (Patch Bank 1-4 / 5-8 and number buttons 1/A .. 6/F), 96 patterns

## Panel
NOTE, MENU, VALUE knob, UPPER, LOWER, DUAL, SPLIT, MANUAL buttons; 4-character 7-segment display (cannot show letters well: parameter names are
cryptic); LEVEL slider; volume knob and power switch on the rear. Menu navigation is hard without the manual or an editor.

## Connections
EXT CLOCK IN (mono mini jack, analog clock), OUTPUT (stereo mini jack), MIX IN (stereo mini jack), PHONES (stereo mini jack), MIDI IN / OUT (5-pin),
USB-C (MIDI and audio; multichannel audio to a DAW with Roland's driver, Windows 11 and macOS Sonoma 14 or later), built-in speaker (one or two (?)).

## Power and size
4 AA (Ni-MH or alkaline, ~6 h with 1900 mAh) or USB bus power (500 mA). 308 x 209 x 54 mm, 1.7 kg with the K-25m and batteries. Optional DK-01 dock.

## Price
About $549.99 (Sweetwater) to $649 (Thomann); GBP 589-592, EUR 689-699.

## Reviewers' remarks
Strong at glassy, hollow, sharp, nasal wavetable tones, layered and moving textures; digital and direct. Limits: octave-only layer 2 tuning, one position
for both layers, no aftertouch, polyphony drops with 24 dB filters (CPU near its limit), USB needs a driver, small keyboard, menu diving.

---

# Part 2: the Reference Manual (v1.00), parameter by parameter

Display names are the 4-character 7-segment names the unit shows (some accented glyphs in the manual's text are extraction artefacts). The manual
gives ranges, not defaults. Not read in detail: the 17 MFX effect pages (parameters per effect), the MIDI Implementation Charts, "How the
n/vision is structured" (its page came back empty), pattern/patch saving, backup/restore.

## Structure and memory
- Parts: LOWER and UPPER; panel mode picks the part edited; DUAL plays both, SPLIT splits the keyboard (split point C3..C5, the split key belongs to the upper part)
- The sound list has **96 patches** (PC 1..96, MSB 71 LSB 3), ids A11..B86: 16 banks (A1..A8, B1..B8) of 6 patches; 8 categories (Synth Bellpad, Pad/Str,
  PolyKey, Pulsating, Lead, Bass, Pluck, FX). So "240 sound patches" in the press material is not what the manual's list shows (user patches?) (?)
- 96 patterns (4 x 4 banks selected with START + [1-4]/[5-8] + [1/A]..[6/F]); a part's volume is stored per pattern
- 63 wavetables, numbered 01..63 (names such as "4waves morph", "Sine Garden", "EM Piano", "FM Bells", "Crystalshyft", "Hubble", "Mosquito", "Guitar WT 1", "Ripple", "Vector")
- Voicing: SOLO ([16] with NOTE lit) or POLY ([PAGE/TIE]); portamento slides in mono; 56 voices max (load dependent)
- Utilities (step buttons): P.CLr init pattern, T.CLr init patch, T.rnd randomise patch

## Panel controls
| Section | Control | Range / options |
|---|---|---|
| OSC | POSITION (both layers) | 0..127 (drawing OFF) |
| OSC | LAYER1 WAVE, LAYER2 WAVE | 001..063 |
| OSC | LAYER MIX | 0..127 |
| OSC | RANGE | 16', 8', 4', 2' |
| OSC | LAYER2 OCT | -2..0 |
| OSC | POSITION LFO (+/- switch) | -63..+63 |
| OSC | POSITION ENV (NORMAL / INVERT switch) | -63..+63 |
| OSC | POSITION RANDOM | 0..63 |
| OSC | PITCH LFO | -63..+63 |
| OSC | MIX ENV, MIX LFO | -63..+63 |
| OSC | SPREAD | 0 (off), 1..20 (reduces polyphony) |
| OSC | WAVE SHAPER (S-MOD depth), W.S LFO, W.S ENV | 0..127, -63..+63, -63..+63 |
| FILTER | FREQ, RES | 0..127 |
| FILTER | ENV (panel 0..+63; negative in the menu), LFO (panel 0..+100; negative in the menu) | menu: -63..63, -100..100 |
| FILTER | DRIVE | 0..3 |
| FILTER | HPF (-6 dB high pass) | 0..127 |
| FILTER | KYBD (key follow) | panel 0..+200; menu -200..+200 |
| FILTER | TYPE | LP.12 LP.24 bP.12 bP.24 HP.12 HP.24, and three vintage LP types LP.r LP.N LP.S (24 dB slopes cost polyphony) |
| AMP | LEVEL | 0..127 |
| ENV | ATTACK DECAY SUSTAIN RELEASE | 0..127 each; ENV SELECT OSC (A and D only) / FILTER / AMP / ALL |
| ENV | PORTAMENTO switch and time | OFF/ON; 0..127 |
| LFO | WAVE | SINE TRI SAW-UP SAW-DOWN SQR RND; pressing the knob toggles tempo sync |
| LFO | RATE | 0..127, or 23 note values 64t..4 when synced |
| LFO | AMP (volume depth), DELAY TIME, KEY TRG | -100..+100; 0..127; OFF/ON |
| FX | DELAY send / TIME / FEEDBACK | 0..127 / 1..1300 ms or a note value / 0..98 % |
| FX | REVERB send / TIME | 0..127 / 0.1..10.0 s |
| FX | CHORUS I, II, I+II buttons | on/off, long press opens the MFX settings |
| Control | NOTE, ARPEGGIO, SEQ, START, MENU, VALUE, MANUAL, [1]..[16], PAGE/TIE, UPPER, LOWER, DUAL, SPLIT, HOLD | |

Note values (23): 64t, 1_64, 32t, 1_32, 16t, 1_32., 1_16, 1_8t, 1_16., 1_8, 1_4t, 1_8., 1_4, 1_2t, 1_4., 1_2, 1t, 1_2., 1, 2t, 1., 2, 4.

## Menu: parts
Part volume 0..127 (per pattern), pan L64..r63, part tune -48..48 st, fine tune -50..50 ct, layer mix curve (0 linear, 1 tapered), position random 0..63,
layer mix random 0..63, spread switch, spread depth 0..20, spread tempo sync, spread delay time 0..1023 (or a note value), filter env depth -63..63,
LFO filter depth -100..100, cutoff key follow -200..200.

## Menu: P-MOD (phase modulation, X and Y directions)
Limiter OFF/ON (restricts X), X -63..63, Y -63..63, LFO depth (applied in Y) -63..63, env depth (OSC envelope on X) -63..63, X random 0..63, Y random 0..63,
X velocity sens -100..100, Y velocity sens -100..100.

## Menu: S-MOD (the wave shaper)
Table 1..15, shaping depth 0..127 (panel), drive 0..127, LFO depth -63..63 (panel), env depth -63..63 (panel), depth random 0..63, drive random 0..63,
depth velocity sens -100..100, drive velocity sens -100..100.

## Menu: modulation (wheel / lever)
Bend range up 0..48, down 0..48; portamento curve LinE / EXP; portamento mode norm / legato only; mod destination (one of: pitch LFO depth, filter LFO depth,
amp LFO depth, P-MOD depth, S-MOD depth, P&S-MOD depth, wavetable position depth, mix/rough depth, position+mix depth) with a depth for each, -63..63.

## Menu: velocity
Velocity curve FIX or 1..7; sensitivities -100..100 for: amp level, cutoff, resonance, filter env depth, filter env attack time, filter env release time (by
key release speed), amp env attack time, amp env release time. LFO phase position 0..3 (start phase 1, 1/4, 1/2, 3/4 cycle when key trigger is on).

## Menu: EQ (per patch)
Switch; low, mid, high gain -24..+24 dB; low, mid, high frequency 20..16000 Hz; mid Q 0.5..16.0.

## Menu: step LFO (16 steps)
Step size 1..16; envelope trigger OFF/ON (steps retrigger the OSC / filter / amp envelope); rate: 22 note values 64t..4. Pitch switch, P-MOD switch (Y),
S-MOD switch, position switch (drawing OFF), P-MOD depth, S-MOD depth, position depth (-63..63), scale switch + 19 scale tables (Chromatic, Major, Minor,
Japanese Minor, Nin1..Nin6 ...). Per step: OSC depth -72..72 and curve 0..36 (pitch, P-MOD, S-MOD, position); filter-amp depth -72..72 and curve 0..36;
filter switch, amp switch, filter depth -100..100; step on/off; step KICK (a falling pitch envelope per step, needs the pitch switch). Tool: destination
(depth / curve / both), target (pitch LFO / filter-amp LFO / both), random, reset.

## Menu: drawing (user waveforms)
Drawing switch (ON replaces the wavetable pair: layer mix, position and their modulation are disabled), morph rough 0..127, 32 wave data values -72..72
(a single cycle in 32 ranges, joined smoothly), tools: form 1 0..127 (sine 0, triangle 42, saw 84, square 127), form 2 0..127 (sine .. metallic), sync,
width, AM depth (a comb filter), execute (VALUE knob press).

## Menu: keyboard, MIDI, system
Keyboard: transpose -5..+6, key velocity rEAL or 1..127, velocity curve light / mid / heavy, split point C3..C5.
MIDI: channel 1..16/OFF, lower part channel, upper part channel, remote keyboard OFF/MIDI/USB, sync source Auto/Int/MIDI/USB, sync out OFF/MIDI/USB/ALL, thru.
System: MIX IN level 0..127, auto off OFF/30/240 min (disabled on USB), master tune 415.3..466.2 Hz. Also factory reset, a battery-fixed operation mode,
backup / restore.

## Arpeggio
Rate 4, 8, 8t, 16, 16t, 32; order UP, DOWN, UP-DOWN, RANDOM, AS PLAYED; shuffle -100..100 %; shuffle resolution 16th/8th; octave range -3..+3;
transpose -36..36; gate 0..100 %; velocity REAL or 1..127. Plays from NOTE mode or a chord on the keyboard; HOLD latches.

## Sequencer
Per pattern: shuffle -90..90, scale (step length) 8, 16, 32, 4t, 8t, 16t, length 1..64 steps, direction forward / reverse / forward-reverse / invert (odd and
even steps swapped) / random / key-triggered, CC (motion output) OFF/ON; actions: double (copy to double the length), random, undo, redo, copy, paste, clear
motions, clear notes, clear all. Input: step input, real-time recording, ties, PAGE (steps 17+), part to record, per-step velocity and gate time, motion
recording; tempo setting; 8 notes per step (spec).

## Delay (2 taps)
Send 0..127, sync OFF/ON, time 1..1300 ms or a note value, feedback -98..98 %, high damp 200 Hz..8 kHz or bypass (17 values), pan 1 and 2 (L64..R63),
level 1 and 2 (0..127), output level, reverb send 0..127.

## Reverb
Send 0..127; mode STANDARD or SHIMMER. Standard: type Room 1, Room 2, Hall 1, Hall 2, Plate, pre-delay 0..100, time 1..100, level. Shimmer: low and high
damp -50..50, time 0.1..10.0 s, density 1..10, pre-delay 0..100 ms, modulation rate and depth 0..100, two pitch shifters -24..+24 st each with feedback
-98..98 % and level 0..100, output level.

## Chorus / MFX: 17 effects (pages not yet read)
JUNO-106 Chorus, CE-1, SDD-320, Time Ctrl Delay, 2Tap Pan Delay, Modulation Delay, Reverse Delay, T-Scream (overdrive), Fuzz, Tone Fattener (drive), Bit
Crusher, LOFI Compress, Script 90 (phaser), Multi Stage Phaser, Super Filter, Pitch Shifter, 2Voice Pitch Shifter.

## What the manual tells us about the engine
Per voice: two wavetable oscillators (one shared position, a layer-mix morph), phase modulation (X, Y), a wave shaper with 15 tables, a filter with 9 types
and a drive, three ADSR envelopes (OSC: AD only), one LFO plus a 16-step LFO that can retrigger the envelopes; every modulation has velocity and random
sensitivity; the drawing mode swaps the wavetables for a 32-point drawn cycle. Per part: spread (a delayed copy), EQ, send to delay / reverb, MFX chorus.

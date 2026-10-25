#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Stephane Schmitz
"""mini-vision's 63 wavetables, in five families (original content, made here; no Roland data).

  GEO  12  geometric shapes that morph into saturated, filtered, folded or asymmetric versions
  VOW  10  formants and vowels (A E I O U, voices, robots, growls)
  MET  14  metal, bells and FM (odd-rich, crystalline, clangorous)
  SPC  14  complex and inharmonic spectra, after space and physics
  LOF  13  noisy and lo-fi cycles (bit and rate crush, static, crackle, grit)

A table is `--frames` keyframes (default 8) of one cycle; the synth interpolates between them over its 0..127
POSITION. Each frame is built in the time domain at 2048 points, then band limited by an FFT at the mip levels
(LEVELS: a harmonic cap H and a length N, as WAVETONE's gen_tables.py) so the pitch picks the level and nothing
aliases. Every frame has no DC and the RMS 0.5 of full scale if its peak allows (a peak of 0.9 at most: a narrow
pulse or a crackle comes out quieter), so the morph keeps its loudness.

  tools/gen_nvision_wt.py [--out build/gen/nvision_wt.h] [--frames 8] [--bits 8|16] [--preview DIR]
"""
import argparse
import math
import sys
from pathlib import Path

import numpy as np

N0 = 2048                                               # a frame, before the band limiting
LEVELS = ((64, 256), (32, 128), (16, 64), (8, 32), (4, 16))      # (harmonic cap H, table length N)
TH = np.arange(N0) * 2 * np.pi / N0


def harm(amps, phases=None):
    """sum of amps[h-1] sin(h th + phases[h-1])"""
    w = np.zeros(N0)
    for i, a in enumerate(amps):
        if a:
            h = i + 1
            w += a * np.sin(h * TH + (phases[i] if phases is not None else 0.0))
    return w


def hs(n=127):
    return np.arange(1, n + 1, dtype=float)


def saw(n=127):
    return harm(1 / hs(n))


def square(n=127):
    a = np.where(hs(n) % 2 == 1, 1 / hs(n), 0.0)
    return harm(a)


def tri(n=127):
    h = hs(n)
    a = np.where(h % 2 == 1, 1 / h ** 2, 0.0) * np.where(((h - 1) / 2) % 2 == 0, 1, -1)
    return harm(a)


def pulse(w):
    """a +-1 pulse of duty w (time domain; the band limiting is done per mip level)"""
    return np.where(TH < 2 * np.pi * w, 1.0, -1.0)


def lp(w, cut, order=2):
    """low pass on the harmonics: cut is the harmonic number of the -3 dB point"""
    s = np.fft.rfft(w)
    h = np.arange(len(s))
    s = s / np.sqrt(1 + (h / max(cut, 0.5)) ** (2 * order))
    return np.fft.irfft(s, N0)


def tanh_drive(w, g):
    return np.tanh(g * w / max(np.max(np.abs(w)), 1e-9))


def lerp(a, b, t):
    return a + (b - a) * t


def fold(w, g):
    x = g * w / max(np.max(np.abs(w)), 1e-9)
    return np.sin(x * np.pi / 2)


def rng_of(name):
    return np.random.default_rng(abs(hash_name(name)) % (2 ** 32))


def hash_name(s):
    h = 2166136261
    for c in s.encode():
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


# ----------------------------------------------------------------------------------------------- GEO
def geo_tables():
    T = []

    def add(name, f):
        T.append(("GEO", name, f))
    sine = np.sin(TH)
    add("Sine>Saw", lambda t: lerp(sine, saw(), t))
    add("Sine>Square", lambda t: lerp(sine, square(), t))
    add("Tri>Square", lambda t: lerp(tri(), square(), t))
    add("Saw>Pulse", lambda t: lerp(saw(), pulse(0.5 - 0.42 * t), t))
    add("Sine Drive", lambda t: tanh_drive(sine, 1 + 11 * t))
    add("Saw Clip", lambda t: tanh_drive(saw(), 1 + 6 * t))
    add("PWM Sweep", lambda t: pulse(0.5 - 0.46 * t))
    add("Tri Fold", lambda t: fold(tri(), 1 + 5 * t))
    add("Skew Sine", lambda t: lerp(sine, np.sin(TH + 1.6 * np.sin(TH - 0.8 * np.cos(TH))), t))
    add("Saw Opening", lambda t: lp(saw(), 1 + 60 * t ** 2, 2))
    add("Rect Sine", lambda t: lerp(sine, np.abs(sine) - np.mean(np.abs(sine)), t) + 0.3 * t * saw(8))
    add("Staircase", lambda t: np.round(sine * (2 + 40 * (1 - t) ** 2) / 1.0) / (2 + 40 * (1 - t) ** 2))
    return T


# ----------------------------------------------------------------------------------------------- VOW
VOWELS = {"A": (800, 1150, 2800), "E": (450, 1750, 2600), "I": (270, 2300, 3000), "O": (500, 900, 2500), "U": (320, 800, 2500)}
F0REF = 100.0


def formants(freqs, bw=(90, 110, 170), f0=F0REF, tilt=1.0, nh=127, src=None, gains=(1.0, 0.7, 0.4)):
    h = hs(nh)
    base = (1 / h ** tilt) if src is None else src
    env = np.zeros(nh)
    for f, b, g in zip(freqs, bw, gains):
        env += g * np.exp(-0.5 * ((h * f0 - f) / b) ** 2)
    a = base * (0.03 + env)
    return harm(a)


def vow_tables():
    T = []

    def add(name, f):
        T.append(("VOW", name, f))

    def vseq(seq, **kw):
        def f(t):
            x = t * (len(seq) - 1)
            i = min(int(x), len(seq) - 2)
            u = x - i
            fa = np.array(seq[i], float)
            fb = np.array(seq[i + 1], float)
            return formants(lerp(fa, fb, u), **kw)
        return f
    V = VOWELS
    add("Vowels AEIOU", vseq([V["A"], V["E"], V["I"], V["O"], V["U"]]))
    add("Vowels UOIEA", vseq([V["U"], V["O"], V["I"], V["E"], V["A"]], tilt=0.8))
    add("Bass Voice", vseq([V["A"], V["O"], V["U"], V["O"]], f0=60.0, tilt=0.9))
    add("Child Voice", vseq([V["I"], V["E"], V["A"], V["O"]], f0=220.0, bw=(120, 150, 220)))
    add("Choir Ooh-Aah", vseq([V["U"], V["O"], V["A"]], tilt=1.3, bw=(60, 80, 120)))
    add("Robo Vox", lambda t: tanh_drive(formants(lerp(np.array(V["A"], float), np.array(V["I"], float), t), tilt=0.5, bw=(40, 50, 70)) + 0.4 * square(30), 2.5))
    add("Nasal", lambda t: formants((lerp(300, 450, t), lerp(1000, 2100, t), 2800), bw=(40, 60, 90), tilt=0.9) + 0.25 * formants((250, 2000, 3100)))
    add("Growl", lambda t: tanh_drive(formants((lerp(300, 700, t), lerp(600, 1100, t), 2200), f0=50.0, bw=(30, 50, 90), tilt=0.7), 2 + 3 * t))
    add("Whistle", lambda t: formants((lerp(900, 3200, t),), bw=(25,), tilt=1.0, gains=(1.0,)) + 0.15 * np.sin(TH))
    add("Diphthong", vseq([V["A"], (700, 1300, 2600), V["E"], (300, 1900, 2700), V["I"]], tilt=1.0))
    return T


# ----------------------------------------------------------------------------------------------- MET
def fm(c, m, idx):
    return np.sin(c * TH + idx * np.sin(m * TH))


def partials(ps, ph=None):
    w = np.zeros(N0)
    for i, (h, a) in enumerate(ps):
        w += a * np.sin(h * TH + (ph[i] if ph is not None else 0.0))
    return w


def met_tables():
    T = []

    def add(name, f):
        T.append(("MET", name, f))
    add("FM 1:1", lambda t: fm(1, 1, 7 * t))
    add("FM 1:2", lambda t: fm(1, 2, 6 * t))
    add("FM 1:3", lambda t: fm(1, 3, 6 * t))
    add("FM 2:5 Bell", lambda t: fm(2, 5, 5 * t) * 0.7 + 0.3 * fm(1, 7, 4 * t))
    add("FM Stack", lambda t: fm(1, 2, 3 * t) + 0.6 * fm(3, 5, 5 * t * t))
    add("Tubular Bell", lambda t: partials([(1, 1.0), (2, 0.7 * (1 - t) + 0.2), (3, 0.5), (4, 0.2 + 0.6 * t), (5, 0.6 * t), (6, 0.35 * t), (9, 0.5 * t), (11, 0.4 * t)]))
    add("Glass", lambda t: partials([(1, 1.0), (3, 0.5 + 0.3 * t), (5, 0.4 * t), (7, 0.35 * t), (9, 0.25 * t), (13, 0.2 * t * t), (17, 0.15 * t * t)]))
    add("Gong", lambda t: partials([(h, (1.0 / math.sqrt(h)) * (0.5 + 0.5 * math.sin(h * 1.3 + 6 * t))) for h in range(1, 6 + int(26 * t))]))
    add("Ring Mod", lambda t: np.sin(TH) * np.sin((1 + int(round(7 * t))) * TH + 3 * t) * 1.2 + 0.35 * np.sin(TH))
    add("Clang", lambda t: partials([(h, 1.0 / (h - 6) ** 0.5) for h in range(9, 10 + int(2 + 18 * t))]))
    add("Hollow Pipe", lambda t: partials([(h, 1.0 / h ** (1.2 - 0.6 * t) * (1.6 if h == 7 else 1.0)) for h in range(1, 40, 2)]))
    add("Crystal", lambda t: partials([(1, 1.0)] + [(h, 0.5 * t / math.sqrt(h - 6)) for h in range(8, 8 + 6 + int(8 * t))]))
    add("Digital Bell", lambda t: np.round(fm(1, 3, 3 + 4 * t) * (4 + 28 * (1 - t))) / (4 + 28 * (1 - t)))
    add("Chime", lambda t: partials([(h, 0.93 ** h * (1.0 if h % (1 + int(round(4 * t))) == 0 else 0.12)) for h in range(1, 40)]))
    return T


# ----------------------------------------------------------------------------------------------- SPC
def spc_tables():
    T = []

    def add(name, f):
        T.append(("SPC", name, f))

    def rphase(name):
        return rng_of(name).uniform(0, 2 * np.pi, 127)
    nm = "Deep Space"
    ph = rphase(nm)
    add(nm, lambda t: harm(1 / hs() ** (0.7 + 0.5 * t), ph + 3 * t * np.sin(hs() * 0.37)))
    nm = "Nebula"
    ph = rphase(nm)

    def nebula(t):
        h = hs()
        a = sum(np.exp(-0.5 * ((h - c0 - 14 * t * s) / w) ** 2) for c0, w, s in ((4, 1.5, 1), (16, 3, -0.6), (30, 4, 0.8)))
        return harm(a / h ** 0.3, ph)
    add(nm, nebula)
    nm = "Pulsar"
    add(nm, lambda t: pulse(0.04 + 0.2 * t) * 0.8 + 0.4 * lp(pulse(0.5), 3 + 10 * t))
    nm = "Quasar"
    ph = rphase(nm)
    add(nm, lambda t: harm(np.where((hs() % (2 + int(round(6 * t)))) == 0, 1 / hs() ** 0.5, 0.05 / hs()), ph))
    nm = "Orbit"
    add(nm, lambda t: np.sin(TH) + 0.8 * np.sin(2 * TH + 2 * np.pi * t) + 0.7 * np.sin(3 * TH - 4 * np.pi * t) + 0.4 * np.sin(5 * TH + 6 * np.pi * t))
    nm = "Event Horizon"
    ph = rphase(nm)
    add(nm, lambda t: harm(1 / hs() ** (0.3 + 2.2 * t), ph * 0.3))
    nm = "Dark Matter"
    ph = rphase(nm)
    add(nm, lambda t: harm(np.exp(-0.5 * ((hs() - (1 + 6 * t)) / 1.4) ** 2) + 0.3 / hs(), ph))
    nm = "Comet"
    add(nm, lambda t: lp(saw(), 4 + 50 * t, 1) + 0.5 * np.roll(lp(saw(), 3 + 20 * t, 1), int(N0 * (0.1 + 0.3 * t))) * -1)
    nm = "Redshift"
    ph = rphase(nm)
    add(nm, lambda t: harm((1 / hs() ** 0.8) * (0.2 + 2.5 * np.exp(-0.5 * ((hs() - (30 - 26 * t)) / 3.0) ** 2)), ph))
    nm = "Supernova"
    ph = rphase(nm)
    add(nm, lambda t: harm(np.where(rng_of(nm).uniform(size=127) < 0.1 + 0.9 * t, 1 / hs() ** 0.6, 0.0), ph * (0.2 + t)))
    nm = "Binary Star"
    ph = rphase(nm)
    add(nm, lambda t: harm(0.25 / hs() + np.exp(-0.5 * ((hs() - 6) / 1.2) ** 2) + np.exp(-0.5 * ((hs() - (6 + 18 * t)) / 1.5) ** 2), ph * 0.5))
    nm = "Cosmic Dust"
    ph = rphase(nm)
    add(nm, lambda t: harm(np.where(rng_of(nm).uniform(size=127) < 0.08 + 0.4 * t, rng_of(nm + "a").uniform(0.2, 1, 127) / hs() ** 0.4, 0.0) + 0.3 * (np.arange(1, 128) == 1), ph))
    nm = "Gravity Well"
    add(nm, lambda t: np.sin(TH + (0.2 + 4.5 * t) * np.sin(TH + 1.0 * np.sin(TH) * t)))
    nm = "Aurora"
    ph = rphase(nm)
    add(nm, lambda t: harm((1 / hs() ** 0.9) * (1 + 0.8 * np.sin(hs() * 0.5 + 6.28 * t)), ph + 2 * np.pi * t * (hs() % 4)))
    return T


# ----------------------------------------------------------------------------------------------- LOF
def lof_tables():
    T = []

    def add(name, f):
        T.append(("LOF", name, f))

    def noise(name, smooth=0, n=N0):
        r = rng_of(name).standard_normal(n)
        return lp(r, smooth) if smooth else r
    sine = np.sin(TH)
    add("Grit", lambda t: lerp(sine, noise("grit", 40), t) * 1.0 + (1 - t) * 0.0)
    add("Bit Crush", lambda t: np.round(saw() / np.max(np.abs(saw())) * 2 ** (6 - 5 * t)) / 2 ** (6 - 5 * t))
    add("Rate Crush", lambda t: np.repeat(lp(saw(), 60, 1)[::max(1, int(2 + 126 * t))], max(1, int(2 + 126 * t)))[:N0] if t > 0 else lp(saw(), 60, 1))
    add("Static", lambda t: noise("static", 200 - 190 * t))
    add("Tape Hiss", lambda t: sine + (0.05 + 0.9 * t) * noise("hiss", 90))
    add("Crackle", lambda t: sine * 0.5 + np.where(rng_of("crackle").uniform(size=N0) < 0.003 + 0.04 * t, rng_of("crk").uniform(-1, 1, N0) * 6, 0.0))
    add("Rust", lambda t: saw() + (0.05 + 2.5 * t) * np.roll(noise("rust", 30), 17) * np.abs(saw()))
    add("Dirt", lambda t: fold(saw(), 1 + 4 * t) + 0.3 * t * noise("dirt", 60))
    add("Radio", lambda t: np.sign(sine) * 0.8 + 2 * t * noise("radio", 120) * (np.sin(TH * 3) > 0.95 - 1.9 * t))
    add("Chip 8-bit", lambda t: np.round(lerp(tri(), pulse(0.25), t) / np.max(np.abs(lerp(tri(), pulse(0.25), t))) * 7) / 7)
    add("Gravel", lambda t: noise("gravel", 80) * (0.3 + 0.7 * (saw() / np.max(np.abs(saw())) + 1) / 2) * (0.4 + 0.6 * t) + (1 - t) * 0.6 * sine)

    def hum(t):
        h = hs()
        return harm(np.where(h <= 3 + 20 * t, 1 / h ** 0.8, 0.0)) + 0.4 * t * noise("hum", 50)
    add("Hum", hum)

    def glitch(t):
        w = lp(saw(), 12 + 30 * t, 1)
        r = rng_of("glitch")
        segs = np.array_split(w, 8)
        out = []
        for s in segs:
            u = r.uniform()
            out.append(s[::-1] if u < 0.3 * t else (segs[0] if u < 0.5 * t else s))
        return np.concatenate(out)
    add("Glitch", glitch)
    return T


def all_tables():
    T = geo_tables() + vow_tables() + met_tables() + spc_tables() + lof_tables()
    return T


# ----------------------------------------------------------------------------------------------- output
def normalise(w):
    w = w - np.mean(w)
    rms = math.sqrt(np.mean(w ** 2)) or 1.0
    s = min(0.5 / rms, 1.0 / max(np.max(np.abs(w)), 1e-9)) * 0.9
    return w * s


def band_limit(w, cap, n):
    """the harmonics up to cap, on n points (+ the guard point)"""
    s = np.fft.rfft(w) / (N0 / 2)
    s[0] = 0
    s[cap + 1:] = 0
    m = np.zeros(n // 2 + 1, complex)
    k = min(cap, n // 2 - 1)
    m[1:k + 1] = s[1:k + 1]
    return np.fft.irfft(m * (n / 2), n)


def build(frames):
    tabs = all_tables()
    data = []
    for fam, name, f in tabs:
        fr = []
        for i in range(frames):
            t = i / (frames - 1)
            w = np.asarray(f(t), float)
            assert w.shape == (N0,), (name, w.shape)
            w = normalise(w)
            pk = max(np.max(np.abs(band_limit(w, cap, n))) for cap, n in LEVELS)      # (a Gibbs overshoot at a low level)
            fr.append(w / pk if pk > 1.0 else w)
        data.append((fam, name, fr))
    return data


def emit(path, data, frames, bits):
    ct, top = ("int8_t", 127) if bits == 8 else ("int16_t", 32767)
    L = ["/* generated by tools/gen_nvision_wt.py: %d wavetables x %d keyframes x %d mip levels, %d bit */" % (len(data), frames, len(LEVELS), bits),
         "#pragma once", "#include <stdint.h>",
         f"#define NV_NTAB {len(data)}", f"#define NV_NFRAME {frames}", f"#define NV_NLEV {len(LEVELS)}",
         "static const uint8_t NV_LEN_LOG2[NV_NLEV] = {" + ", ".join(str(int(math.log2(n))) for _, n in LEVELS) + "};",
         "static const uint8_t NV_CAP[NV_NLEV] = {" + ", ".join(str(h) for h, _ in LEVELS) + "};",
         "static const char *const NV_NAME[NV_NTAB] = {"]
    for fam, name, _ in data:
        L.append(f'    "{name}",   /* {fam} */')
    L.append("};")
    total = 0
    for k, (cap, n) in enumerate(LEVELS):
        vals = []
        for fam, name, fr in data:
            for w in fr:
                t = band_limit(w, cap, n)
                t = np.round(t * top).astype(int)
                assert np.max(np.abs(t)) <= top
                vals += list(t) + [int(t[0])]
        total += len(vals)
        L.append(f"static const {ct} NV_L{k}[{len(vals)}] = {{")
        for i in range(0, len(vals), 24):
            L.append("    " + ", ".join(str(v) for v in vals[i:i + 24]) + ",")
        L.append("};")
    L.append("static const %s *const NV_TAB[NV_NLEV] = {%s};" % (ct, ", ".join(f"NV_L{k}" for k in range(len(LEVELS)))))
    L.append(f"/* {total} samples: {total * bits // 8} bytes of flash */")
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("\n".join(L) + "\n")
    return total * bits // 8


def preview(dirp, data, frames, f0=110.0, fs=44100):
    import wave
    Path(dirp).mkdir(parents=True, exist_ok=True)
    for i, (fam, name, fr) in enumerate(data):
        out = []
        ph = 0.0
        secs = 4.0
        n = int(secs * fs)
        for j in range(n):
            pos = j / n * (frames - 1)
            a = int(min(pos, frames - 2))
            u = pos - a
            idx = ph * N0
            i0 = int(idx) % N0
            fa = fr[a][i0] * (1 - u) + fr[a + 1][i0] * u
            out.append(fa)
            ph = (ph + f0 / fs) % 1.0
        pcm = (np.clip(np.array(out), -1, 1) * 0.6 * 32767).astype("<i2")
        with wave.open(str(Path(dirp) / f"{i + 1:02d}_{fam}_{name.replace(' ', '_').replace('>', '-').replace(':', '-')}.wav"), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(fs)
            w.writeframes(pcm.tobytes())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="build/gen/nvision_wt.h")
    ap.add_argument("--frames", type=int, default=8)
    ap.add_argument("--bits", type=int, default=8, choices=(8, 16))
    ap.add_argument("--preview")
    a = ap.parse_args()
    data = build(a.frames)
    size = emit(a.out, data, a.frames, a.bits)
    print(f"{len(data)} tables x {a.frames} frames x {len(LEVELS)} mip levels, {a.bits} bit: {size} bytes -> {a.out}")
    if a.preview:
        preview(a.preview, data, a.frames)
        print(f"previews in {a.preview}")


if __name__ == "__main__":
    main()

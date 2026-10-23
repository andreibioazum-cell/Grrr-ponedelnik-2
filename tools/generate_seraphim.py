#!/usr/bin/env python3
"""Generates Azum vs Astra battle track in style of And_The_Seraphim_Wept (~93s, D minor, 112 BPM) without copying."""

from __future__ import annotations

from array import array
import math
from pathlib import Path
import wave

ROOT = Path(__file__).resolve().parents[1]
AUDIO = ROOT / "assets" / "audio"
RATE = 22050
TAU = math.tau
_noise_state = 0x6A09E667

def stereo(seconds: float):
    frames = int(round(seconds * RATE))
    return array("f", [0.0]) * frames, array("f", [0.0]) * frames

def midi(note: float) -> float:
    return 440.0 * 2.0 ** ((note - 69.0) / 12.0)

def pan(level: float, position: float):
    angle = (max(-1.0, min(1.0, position)) + 1.0) * math.pi / 4.0
    return level * math.cos(angle), level * math.sin(angle)

def add_tone(left, right, start, duration, note, level, position=0.0, shape="soft", attack=0.01, release=0.18):
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(duration * RATE)), len(left) - begin)
    if count <= 0:
        return
    freq = midi(note)
    gl, gr = pan(level, position)
    attack_n = max(1, int(attack * RATE))
    release_n = max(1, int(release * RATE))
    if shape == "sine":
        partials = ((1.0, 1.0),)
    elif shape == "bass":
        partials = ((1.0, 1.0), (2.0, 0.30), (3.0, 0.12))
    elif shape == "string":
        partials = ((1.0, 1.0), (2.0, 0.34), (3.0, 0.20), (4.0, 0.09))
    elif shape == "brass":
        partials = ((1.0, 1.0), (2.0, 0.48), (3.0, 0.30), (4.0, 0.18), (5.0, 0.10))
    elif shape == "choir":
        partials = ((1.0, 1.0), (2.0, 0.22), (3.0, 0.14), (5.0, 0.08), (6.0, 0.05))
    else:
        partials = ((1.0, 1.0), (2.0, 0.18), (3.0, 0.08))
    norm = sum(weight for _, weight in partials)
    for i in range(count):
        env = min(1.0, i / attack_n)
        remaining = count - i
        if remaining < release_n:
            env *= remaining / release_n
        t = i / RATE
        value = 0.0
        for ratio, weight in partials:
            value += math.sin(TAU * freq * ratio * t) * weight
        value = value / norm * env
        left[begin + i] += value * gl
        right[begin + i] += value * gr

def add_pluck(left, right, start, duration, note, level, position=0.0):
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(duration * RATE)), len(left) - begin)
    if count <= 0:
        return
    freq = midi(note)
    gl, gr = pan(level, position)
    for i in range(count):
        t = i / RATE
        env = min(1.0, t * 180.0) * math.exp(-4.8 * t)
        value = (math.sin(TAU * freq * t) + 0.32 * math.sin(TAU * freq * 2.01 * t)) * env / 1.32
        left[begin + i] += value * gl
        right[begin + i] += value * gr

def add_bell(left, right, start, duration, note, level, position=0.0):
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(duration * RATE)), len(left) - begin)
    if count <= 0:
        return
    freq = midi(note)
    gl, gr = pan(level, position)
    partials = ((1.0, 1.0, 2.1), (2.01, 0.46, 3.4), (2.74, 0.30, 4.8), (4.08, 0.17, 6.2))
    for i in range(count):
        t = i / RATE
        attack = min(1.0, t * 300.0)
        value = 0.0
        for ratio, weight, decay in partials:
            value += weight * math.exp(-decay * t) * math.sin(TAU * freq * ratio * t)
        value = value * attack / 1.93
        left[begin + i] += value * gl
        right[begin + i] += value * gr

def noise_value() -> float:
    global _noise_state
    value = _noise_state
    value ^= value << 13
    value ^= value >> 17
    value ^= value << 5
    _noise_state = value & 0xFFFFFFFF
    return (_noise_state / 0xFFFFFFFF) * 2.0 - 1.0

def add_noise(left, right, start, duration, level, position=0.0, decay=24.0):
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(duration * RATE)), len(left) - begin)
    if count <= 0:
        return
    gl, gr = pan(level, position)
    previous = 0.0
    for i in range(count):
        t = i / RATE
        raw = noise_value()
        value = (raw - previous) * math.exp(-decay * t) * min(1.0, t * 400.0)
        previous = raw
        left[begin + i] += value * gl
        right[begin + i] += value * gr

def add_kick(left, right, start, level=0.4):
    begin = max(0, int(round(start * RATE)))
    count = min(int(0.34 * RATE), len(left) - begin)
    phase = 0.0
    for i in range(count):
        t = i / RATE
        freq = 46.0 + 78.0 * math.exp(-t * 35.0)
        phase += TAU * freq / RATE
        value = math.sin(phase) * math.exp(-t * 10.5) * level
        left[begin + i] += value
        right[begin + i] += value

def write_track(path, left, right, fade_in, fade_out):
    fade_in_n = max(1, int(fade_in * RATE))
    fade_out_n = max(1, int(fade_out * RATE))
    frames = len(left)
    peak = 0.0
    for i in range(frames):
        gain = 1.0
        if i < fade_in_n:
            gain *= i / fade_in_n
        remaining = frames - 1 - i
        if remaining < fade_out_n:
            gain *= remaining / fade_out_n
        left[i] = math.tanh(left[i] * gain)
        right[i] = math.tanh(right[i] * gain)
        peak = max(peak, abs(left[i]), abs(right[i]))
    scale = 0.88 / max(peak, 1e-9)
    pcm = array("h")
    for i in range(frames):
        pcm.append(int(max(-32767, min(32767, left[i] * scale * 32767))))
        pcm.append(int(max(-32767, min(32767, right[i] * scale * 32767))))
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(pcm.tobytes())
    print(f"{path.relative_to(ROOT)}: {frames / RATE:.2f} s peak {peak:.3f}")

def seraphim_track():
    # Original: 93.28s, ~112 BPM, D minor gothic melancholic, low sub bass, strings, choir, bell
    # We create original composition in same style but not copying melody.
    total = 93.3
    left, right = stereo(total)
    bpm = 112.0
    beat = 60.0 / bpm  # 0.5357s
    bar = beat * 4  # 2.1428s
    # Chord roots in MIDI D minor progression (inspired but not copied)
    # We'll do 16 chords each 2 bars = 4.285s, then loop. 16*4.285=68.57, plus intro/outro fill to 93.3
    # Use D minor harmonic context: D,F,A is i; G,A#,D is iv; F,A,C is III; Bb,D,F is VI; A,C#,E is V; etc.
    # Sequence designed to evoke gothic weeping:
    # Dm - Gm - Bbmaj - A7 - Dm - F - Gm - A7 - Dm - Gm - C - F - Dm - A7 - Dm (hold)
    chord_roots = [38, 43, 46, 45, 38, 41, 43, 45, 38, 43, 36, 41, 38, 45, 38, 38]  # MIDI bass D2 etc
    chord_voicings = [
        (50, 53, 57), # Dm: D3 F3 A3
        (55, 58, 62), # Gm: G3 Bb3 D4
        (58, 62, 65), # Bb: Bb3 D4 F4
        (57, 61, 64), # A: A3 C#4 E4
        (50, 53, 57),
        (53, 57, 60), # F: F3 A3 C4
        (55, 58, 62),
        (57, 61, 64),
        (50, 53, 57),
        (55, 58, 62),
        (48, 52, 55), # C: C3 E3 G3
        (53, 57, 60),
        (50, 53, 57),
        (57, 61, 64),
        (50, 53, 57),
        (38, 50, 53), # low octave D2+D3+F3 for final weight
    ]
    # For dynamics: volume curve matches original RMS analysis
    # rms 5s 0.02, 15s 0.05, 25s 0.08, 35s 0.16, 45s 0.19, 55s 0.24, 65s 0.12, 75s 0.29, 85s 0.15
    # We'll map sections:
    # 0-12s intro quiet
    # 12-30s verse
    # 30-55s build
    # 55-75s climax
    # 75-93s outro
    def dynamic(t):
        # approximate curve 0.3 to 1.0 factor
        if t < 12:
            return 0.25 + 0.25 * (t / 12)
        elif t < 30:
            return 0.5 + 0.25 * ((t - 12) / 18)
        elif t < 55:
            return 0.75 + 0.25 * ((t - 30) / 25)
        elif t < 75:
            return 1.0 if t < 68 else 1.0 - 0.3 * ((t - 68) / 7)
        else:
            return 0.7 - 0.3 * ((t - 75) / 18)

    # Drone bass: continuous low D2 with slow swell, matching 0-12s intro
    for i, root in enumerate(chord_roots):
        start = i * bar * 2
        dur = bar * 2 + 0.6
        # Bass drone
        vel = 0.12 * dynamic(start + 1)
        add_tone(left, right, start, dur, root, vel, 0.0, "bass", 0.6, 0.8)
        # also octave below for weight in climax
        if start > 30:
            add_tone(left, right, start, dur, root - 12, 0.08 * dynamic(start), 0.0, "bass", 0.8, 0.9)
        # Pad chords (strings)
        for v, note in enumerate(chord_voicings[i]):
            pos = (v - 1) * 0.5
            level = 0.09 * dynamic(start) if start < 30 else 0.13 * dynamic(start)
            if start >= 55 and start < 75:
                level *= 1.2
            add_tone(left, right, start, dur, note, level, pos, "string", 0.35 if start < 30 else 0.18, 0.6)
            # Choir doubler an octave up in climax
            if start >= 30:
                add_tone(left, right, start, dur, note + 12, 0.055 * dynamic(start), pos * 0.7, "choir", 0.8, 0.9)
        # Brass in climax
        if start >= 45:
            for note in chord_voicings[i]:
                add_tone(left, right, start, dur, note + 7, 0.045 * dynamic(start), 0.0, "brass", 0.5, 0.7)

    # Melody: original but in same mood, D minor scale, weeping contour descending
    # We craft 4-bar phrases, each 8 notes (half-beat)
    # Use midi notes in high octave: D4=62, F4=65, A4=69 etc.
    # Create 8 phrases corresponding to first 8 chords, then repeat variation
    # Melody inspired by gothic weeping: stepwise, small intervals, sighing
    melody_phrases = [
        # Dm: D5 F5 A5 G5 F5 E5 D5 C#5
        (74, 77, 81, 79, 77, 76, 74, 73),
        # Gm: G5 A#5 D6 C6 A#5 A5 G5 F5
        (79, 82, 86, 84, 82, 81, 79, 77),
        # Bb: F5 D5 Bb4 D5 F5 A5 G5 F5
        (77, 74, 70, 74, 77, 81, 79, 77),
        # A: E5 C#5 A4 C#5 E5 G5 F5 E5
        (76, 73, 69, 73, 76, 79, 77, 76),
        # Dm: D5 F5 A5 G5 F5 E5 D5 C#5 (variation)
        (74, 77, 81, 79, 77, 76, 74, 73),
        # F: C5 F5 A5 G5 F5 E5 D5 C5
        (72, 77, 81, 79, 77, 76, 74, 72),
        # Gm: G5 A#5 D6 D6 C6 A#5 G5 F5
        (79, 82, 86, 86, 84, 82, 79, 77),
        # A: A4 C#5 E5 G5 F5 E5 D5 C#5
        (69, 73, 76, 79, 77, 76, 74, 73),
        # Repeat with higher intensity for climax
        (86, 84, 82, 81, 79, 77, 76, 74),
        (79, 82, 86, 84, 82, 81, 79, 77),
        (72, 74, 76, 79, 81, 83, 81, 79),
        (77, 81, 84, 83, 81, 79, 77, 76),
        (86, 84, 81, 79, 77, 76, 74, 73),
        (76, 73, 69, 73, 76, 79, 77, 76),
        (74, 77, 81, 79, 77, 76, 74, 62), # final low D
        (62, 62, 62, 62, 62, 62, 62, 62), # hold low drone
    ]
    for idx, phrase in enumerate(melody_phrases):
        start = idx * bar * 2
        if start >= total - 2:
            break
        for p, note in enumerate(phrase):
            t = start + p * beat  # half-note? Actually beat each note to be slower, more weeping
            # But original had faster: we do 0.535 per note = 8 notes = 4.28s = 2 bars perfect
            # Slight variation: every 4th note longer
            dur = beat * 0.9 if p % 4 != 3 else beat * 1.4
            level = 0.0
            if start < 12:
                level = 0.0  # intro no melody
            elif start < 30:
                level = 0.075 * dynamic(start)
            elif start < 55:
                level = 0.11 * dynamic(start)
            elif start < 75:
                level = 0.14 * dynamic(start)
            else:
                level = 0.06 * dynamic(start)
            if level == 0:
                continue
            shape = "string" if start < 45 else "choir"
            pos = 0.25 if p % 2 == 0 else -0.25
            # For climax, add strong
            add_tone(left, right, t, dur, note, level, pos, shape, 0.04 if start < 45 else 0.08, 0.3)
            # Bell overlay on downbeats in climax for ethereal weeping
            if start >= 30 and p % 2 == 0:
                add_bell(left, right, t, 1.8, note + 12, 0.035 * dynamic(start), pos * 0.8)
            # Pluck echo after 55s
            if start >= 55 and p % 4 == 0:
                add_pluck(left, right, t + beat * 0.5, beat * 0.7, note - 12, 0.09 * dynamic(start), -pos)

    # High bell arpeggios mirroring strings in climax (like seraphim tears)
    for idx in range(len(chord_roots)):
        start = idx * bar * 2
        if start < 30 or start >= total:
            continue
        voicing = chord_voicings[idx]
        for arp in range(4):
            t = start + arp * beat
            note = voicing[arp % 3] + 24  # two octaves up
            add_bell(left, right, t, 1.2, note, 0.03 * dynamic(start), 0.6 if arp % 2 == 0 else -0.6)
            add_bell(left, right, t + beat / 2, 1.0, note + 3, 0.022 * dynamic(start), -0.5)

    # Subtle pulse: soft kick on each beat after 12s, mimicking original's low throb around 43Hz
    for i in range(int(total / beat)):
        t = i * beat
        if t < 12:
            continue
        # original had strong low 43Hz and 32Hz energy; we emulate with kick
        # add kick on beats 1 and 3, softer on 2/4
        is_down = (i % 4) in (0, 2)
        if is_down:
            lvl = 0.09 * dynamic(t) if t < 75 else 0.05
            add_kick(left, right, t, lvl)
            add_noise(left, right, t, 0.04, 0.015 * dynamic(t), 0.0, 18.0)
        # Hi-hat-like airy on off-beats after 30s
        if t >= 30 and i % 2 == 1:
            add_noise(left, right, t, 0.06, 0.012 * dynamic(t), 0.2, 45.0)

    # Ambient wind-like noise sweeps for gothic atmosphere 0-93s
    for seg in range(0, int(total), 6):
        add_noise(left, right, seg, 6.5, 0.018, 0.0, 8.0)
        if seg > 30:
            add_noise(left, right, seg + 3, 2.0, 0.012, 0.35, 12.0)

    # Solo piano-like pluck intro 0-12s: D minor arpeggio D4-F4-A4-D5
    intro_arps = [(50, 53, 57, 62), (55, 58, 62, 67), (53, 57, 60, 65), (45, 49, 52, 57)]
    for i, arp in enumerate(intro_arps):
        start = i * bar
        if start >= 12:
            break
        for n, note in enumerate(arp):
            t = start + n * beat / 2
            add_pluck(left, right, t, beat * 1.2, note + 12, 0.11, -0.2 + 0.15 * n)
            add_tone(left, right, t, beat * 1.0, note + 24, 0.025, 0.3, "sine", 0.02, 0.4)

    # Final fade: add low D2 drone held with slow release last 8s
    add_tone(left, right, total - 10, 10.5, 38, 0.10, 0.0, "bass", 1.2, 1.5)
    add_tone(left, right, total - 10, 10.5, 50, 0.08, 0.0, "string", 1.0, 2.0)
    add_tone(left, right, total - 8, 8.5, 62, 0.06, 0.2, "choir", 0.8, 2.5)

    return left, right

def main():
    left, right = seraphim_track()
    write_track(AUDIO / "azum_vs_astra.wav", left, right, 1.2, 2.0)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

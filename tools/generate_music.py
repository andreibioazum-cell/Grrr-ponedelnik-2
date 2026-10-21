#!/usr/bin/env python3
"""Создаёт три оригинальные музыкальные дорожки игры без внешних библиотек."""

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


def stereo(seconds: float) -> tuple[array, array]:
    frames = int(round(seconds * RATE))
    return array("f", [0.0]) * frames, array("f", [0.0]) * frames


def midi(note: float) -> float:
    return 440.0 * 2.0 ** ((note - 69.0) / 12.0)


def pan(level: float, position: float) -> tuple[float, float]:
    angle = (max(-1.0, min(1.0, position)) + 1.0) * math.pi / 4.0
    return level * math.cos(angle), level * math.sin(angle)


def add_tone(
    left: array,
    right: array,
    start: float,
    duration: float,
    note: float,
    level: float,
    position: float = 0.0,
    shape: str = "soft",
    attack: float = 0.01,
    release: float = 0.18,
) -> None:
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


def add_pluck(
    left: array,
    right: array,
    start: float,
    duration: float,
    note: float,
    level: float,
    position: float = 0.0,
) -> None:
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


def add_bell(
    left: array,
    right: array,
    start: float,
    duration: float,
    note: float,
    level: float,
    position: float = 0.0,
) -> None:
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


def add_noise(
    left: array,
    right: array,
    start: float,
    duration: float,
    level: float,
    position: float = 0.0,
    decay: float = 24.0,
) -> None:
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


def add_kick(left: array, right: array, start: float, level: float = 0.4) -> None:
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


def write_track(path: Path, left: array, right: array, fade_in: float, fade_out: float) -> None:
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
    print(f"{path.relative_to(ROOT)}: {frames / RATE:.2f} с")


def lobby_track() -> tuple[array, array]:
    left, right = stereo(35.0)
    beat = 0.625
    bar = beat * 4
    chords = ((50, (62, 65, 69)), (46, (58, 62, 65)), (53, (65, 69, 72)), (48, (60, 64, 67)),
              (43, (55, 58, 62)), (46, (58, 62, 65)), (45, (57, 61, 64))) * 2
    melody = ((74, 77, 81, 77, 74, 72, 69, 72), (70, 74, 77, 74, 70, 69, 65, 69),
              (77, 81, 84, 81, 79, 77, 72, 74), (72, 76, 79, 76, 72, 71, 67, 71),
              (67, 70, 74, 70, 67, 65, 62, 65), (70, 74, 77, 74, 72, 70, 69, 65),
              (69, 73, 76, 73, 69, 67, 64, 61)) * 2
    for index, (root, tones) in enumerate(chords):
        start = index * bar
        for voice, note in enumerate(tones):
            add_tone(left, right, start, bar + 0.05, note, 0.105, (voice - 1) * 0.55, "string", 0.12, 0.35)
        for pulse in range(8):
            lift = 12 if pulse in (3, 7) else 0
            add_pluck(left, right, start + pulse * beat / 2, beat * 0.48, root + lift, 0.23, -0.08)
        for pulse, note in enumerate(melody[index]):
            if pulse % 2 == 0 or index >= 7:
                add_pluck(left, right, start + pulse * beat / 2, beat * 0.72, note, 0.13, 0.24)
        for pulse in range(4):
            add_kick(left, right, start + pulse * beat, 0.26)
            add_noise(left, right, start + pulse * beat + beat / 2, 0.055, 0.065, 0.35, 35.0)
        for pulse in (1, 3):
            add_noise(left, right, start + pulse * beat, 0.15, 0.12, -0.15, 13.0)
    return left, right


def winter_track() -> tuple[array, array]:
    left, right = stereo(35.0)
    beat = 0.625
    bar = beat * 4
    roots = (55, 52, 48, 50, 55, 48, 50) * 2
    chords = ((67, 71, 74), (64, 67, 71), (60, 64, 67), (62, 66, 69), (67, 71, 74), (60, 64, 67), (62, 66, 69)) * 2
    melody = ((79, 81, 83, 86, 83, 81, 79, 74), (76, 79, 83, 81, 79, 76, 74, 71),
              (72, 76, 79, 84, 79, 76, 74, 72), (74, 78, 81, 86, 81, 78, 76, 74),
              (79, 83, 86, 91, 86, 83, 81, 79), (84, 83, 79, 76, 79, 76, 74, 72),
              (74, 76, 78, 81, 79, 78, 74, 71)) * 2
    for index, tones in enumerate(chords):
        start = index * bar
        for voice, note in enumerate(tones):
            add_tone(left, right, start, bar + 0.08, note - 12, 0.075, (voice - 1) * 0.6, "string", 0.2, 0.5)
        for pulse in range(8):
            position = -0.35 if pulse % 2 == 0 else 0.35
            add_bell(left, right, start + pulse * beat / 2, 1.0, melody[index][pulse], 0.26, position)
            add_noise(left, right, start + pulse * beat / 2, 0.09, 0.055, -position, 28.0)
        for pulse in range(4):
            add_pluck(left, right, start + pulse * beat, beat * 0.8, roots[index] + (12 if pulse == 3 else 0), 0.15, 0.0)
    return left, right


def main() -> int:
    tracks = (
        ("lobbymusic.wav", lobby_track(), 0.025, 0.025),
        ("winter_jingle.wav", winter_track(), 0.025, 0.025),
    )
    for name, (left, right), fade_in, fade_out in tracks:
        write_track(AUDIO / name, left, right, fade_in, fade_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

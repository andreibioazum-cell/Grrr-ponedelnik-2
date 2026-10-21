#!/usr/bin/env python3
"""Синтезирует звуковые эффекты способностей в assets/audio/*.wav.

Файлы в репозитории лежат готовые; этот скрипт — их исходник: если звук
захочется поменять, правим синтез здесь и перегенерируем, а не ищем, чем
был нарезан wav. Только стандартная библиотека (wave + math + random).

Звуки (все — короткий моно 22050 Гц, 16 бит):
  punch.wav         удар: шумовой хлопок + низкий толчок;
  dash.wav          рывок Азума: шумовой свист вверх-вниз;
  grab.wav          захват Астры: электронный зум вниз;
  gift.wav          подарок Деда Мороза: три ноты колокольчиком;
  station.wav       станция буКа: металлический лязг + бип;
  overclock.wav     оверклок кодкрэкера: разгон синуса вверх;
  zombie_punch.wav  удар зомби-Азума: низкий рык с тремоло;
  zombie_dash.wav   рывок зомби-Азума: стонущий свист вниз.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "audio"
RATE = 22050


def synth(seconds: float, fn) -> list[int]:
    n = int(RATE * seconds)
    out = []
    for i in range(n):
        t = i / RATE
        v = fn(t, i)
        v = max(-1.0, min(1.0, v))
        out.append(int(v * 32767 * 0.85))
    return out


def save(name: str, samples: list[int]) -> None:
    path = OUT / name
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", s) for s in samples))
    print("%s: %d сэмплов" % (name, len(samples)))


def env(t: float, dur: float, attack: float = 0.005) -> float:
    if t < attack:
        return t / attack
    k = (t - attack) / max(0.001, dur - attack)
    return max(0.0, 1.0 - k) ** 1.6


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    rng = random.Random(42)
    noise = [rng.uniform(-1, 1) for _ in range(RATE)]

    def nz(t: float) -> float:
        return noise[int(t * RATE) % RATE]

    # Удар: короткий шум + толчок 110 Гц.
    save("punch.wav", synth(0.20, lambda t, i:
                            env(t, 0.20) * (0.65 * nz(t) * max(0.0, 1 - t * 14) +
                                            0.55 * math.sin(2 * math.pi * 110 * t) * max(0.0, 1 - t * 7))))

    # Рывок: свист шума, громкость горбом, тон вверх.
    save("dash.wav", synth(0.35, lambda t, i:
                           math.sin(math.pi * min(1.0, t / 0.35)) ** 1.5 *
                           (0.5 * nz(t * (1 + t * 2)) + 0.25 * math.sin(2 * math.pi * (300 + 900 * t) * t))))

    # Захват Астры: зум 900 -> 180 с вибрато.
    save("grab.wav", synth(0.40, lambda t, i:
                           env(t, 0.40, 0.01) * 0.55 *
                           math.sin(2 * math.pi * (900 - 1750 * t) * t + 0.6 * math.sin(2 * math.pi * 30 * t))))

    # Подарок Санты: три колокольчика E5-G5-C6.
    def gift(t: float, i: int) -> float:
        v = 0.0
        for start, freq in ((0.0, 659.25), (0.12, 783.99), (0.24, 1046.5)):
            if t >= start:
                lt = t - start
                v += 0.33 * math.exp(-lt * 9) * (math.sin(2 * math.pi * freq * lt) +
                                                0.4 * math.sin(2 * math.pi * freq * 2 * lt))
        return v
    save("gift.wav", synth(0.55, gift))

    # Станция буКа: лязг (пара негармонических частичных) + короткий бип.
    def station(t: float, i: int) -> float:
        clang = math.exp(-t * 16) * (0.4 * math.sin(2 * math.pi * 620 * t) +
                                     0.3 * math.sin(2 * math.pi * 935 * t) +
                                     0.2 * math.sin(2 * math.pi * 1470 * t))
        beep = 0.25 * math.sin(2 * math.pi * 1250 * t) if 0.16 < t < 0.28 else 0.0
        return clang + beep * env(t, 0.35)
    save("station.wav", synth(0.35, station))

    # Оверклок: разгон 180 -> 1400 Гц с искрами.
    save("overclock.wav", synth(0.50, lambda t, i:
                                env(t, 0.50, 0.02) * (0.45 * math.sin(2 * math.pi * (180 + 2440 * t) * t) +
                                                      0.18 * math.sin(2 * math.pi * (720 + 9760 * t) * t))))

    # Зомби-удар: рык — пила 65 Гц вниз, тремоло 11 Гц, немного шума.
    def zpunch(t: float, i: int) -> float:
        f = 78 - 60 * t
        ph = (f * t) % 1.0
        saw = 2 * ph - 1
        return env(t, 0.38, 0.01) * (0.75 * saw * (0.7 + 0.3 * math.sin(2 * math.pi * 11 * t)) +
                                     0.25 * nz(t))
    save("zombie_punch.wav", synth(0.38, zpunch))

    # Зомби-рывок: стон — синус 320 -> 85 с тремоло + шумовое дыхание.
    def zdash(t: float, i: int) -> float:
        f = 320 - 470 * t
        moan = math.sin(2 * math.pi * f * t) * (0.65 + 0.35 * math.sin(2 * math.pi * 9 * t))
        return math.sin(math.pi * min(1.0, t / 0.5)) * (0.5 * moan + 0.3 * nz(t * 0.7))
    save("zombie_dash.wav", synth(0.50, zdash))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

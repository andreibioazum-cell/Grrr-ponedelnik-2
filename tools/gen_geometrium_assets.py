#!/usr/bin/env python3
"""Generate all brand-new 32x32 RGBA textures, clean synthesized WAV sound effects,
and monochrome dark-gray isometric cube launcher icons for Geometrium."""

import math
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEX_DIR = os.path.join(ROOT, "assets", "textures")
AUD_DIR = os.path.join(ROOT, "assets", "audio")


def write_png(path, w, h, pixels):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            r, g, b, a = pixels[y][x]
            raw.extend((max(0, min(255, int(r))), max(0, min(255, int(g))),
                        max(0, min(255, int(b))), max(0, min(255, int(a)))))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def h2(x, y, seed):
    n = (x * 374761393 + y * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def clamp_rgb(r, g, b, a=255):
    return (max(0, min(255, int(r))), max(0, min(255, int(g))), max(0, min(255, int(b))), max(0, min(255, int(a))))


def make_tex(fn):
    return [[fn(x, y) for x in range(32)] for y in range(32)]


def generate_textures():
    os.makedirs(TEX_DIR, exist_ok=True)
    for old in os.listdir(TEX_DIR):
        os.remove(os.path.join(TEX_DIR, old))

    texs = {}

    # 1. Turf Top & Side + Loam Soil
    def loam_fn(x, y):
        n = h2(x, y, 11)
        pebble = 1 if h2(x // 2, y // 2, 12) > 0.87 else 0
        base = 96 + (n - 0.5) * 28 + pebble * 26
        return clamp_rgb(base * 1.12, base * 0.84, base * 0.60)

    texs["loam.png"] = make_tex(loam_fn)

    def turf_top_fn(x, y):
        n = h2(x, y, 21)
        blade = ((x * 3 + y * 5) % 7 == 0)
        g = 148 + (n - 0.5) * 34 + (14 if blade else 0)
        return clamp_rgb(g * 0.50, g, g * 0.42)

    texs["turf_top.png"] = make_tex(turf_top_fn)

    def turf_side_fn(x, y):
        overhang = 6 + int(h2(x, 0, 31) * 5)
        if y < overhang:
            c = turf_top_fn(x, y)
            if y == overhang - 1:
                return clamp_rgb(c[0] * 0.72, c[1] * 0.72, c[2] * 0.72)
            return c
        return loam_fn(x, y)

    texs["turf_side.png"] = make_tex(turf_side_fn)

    # 2. Slate Stone
    def slate_fn(x, y):
        band = math.sin((y + h2(x, y // 4, 41) * 3.0) * 0.65) * 10
        n = (h2(x, y, 42) - 0.5) * 20
        seam = -16 if (y % 8 == 0 and (x + (y // 8) * 9) % 16 != 3) else 0
        v = 122 + band + n + seam
        return clamp_rgb(v * 0.96, v * 0.99, v * 1.04)

    texs["slate.png"] = make_tex(slate_fn)

    # 3. Basalt Geometric Tiles
    def basalt_fn(x, y):
        gx, gy = x % 16, y % 16
        bevel = 16 if (gx == 1 or gy == 1) else (-18 if (gx == 15 or gy == 15 or gx == 0 or gy == 0) else 0)
        diag = -8 if abs(gx - 8) + abs(gy - 8) == 5 else 0
        v = 62 + (h2(x, y, 51) - 0.5) * 12 + bevel + diag
        return clamp_rgb(v, v, v * 1.03)

    texs["basalt_tiles.png"] = make_tex(basalt_fn)

    # 4. Honed Limestone
    def limestone_fn(x, y):
        row = y // 8
        mortar = (y % 8 == 0) or ((x + row * 8) % 16 == 0)
        v = 204 + (h2(x, y, 61) - 0.5) * 16 - (28 if mortar else 0)
        return clamp_rgb(v * 1.03, v * 0.99, v * 0.88)

    texs["limestone.png"] = make_tex(limestone_fn)

    # 5. Terracotta Clinker Brick
    def brick_fn(x, y):
        row = y // 8
        mortar = (y % 8 == 0) or ((x + (row % 2) * 8) % 16 == 0)
        if mortar:
            m = 164 + (h2(x, y, 71) - 0.5) * 14
            return clamp_rgb(m, m * 0.98, m * 0.93)
        shade = (h2(row, (x + (row % 2) * 8) // 16, 72) - 0.5) * 26
        n = (h2(x, y, 73) - 0.5) * 14
        r = 168 + shade + n
        return clamp_rgb(r, r * 0.53, r * 0.40)

    texs["terracotta_brick.png"] = make_tex(brick_fn)

    # 6. Dune Sand
    def dune_fn(x, y):
        ripple = math.sin((y * 0.55) + math.sin(x * 0.25) * 1.6) * 11
        n = (h2(x, y, 81) - 0.5) * 14
        v = 216 + ripple + n
        return clamp_rgb(v * 1.04, v * 0.96, v * 0.72)

    texs["dune.png"] = make_tex(dune_fn)

    # 7. Frost Top & Frost Side
    def frost_top_fn(x, y):
        n = (h2(x, y, 91) - 0.5) * 14
        spark = 12 if h2(x, y, 92) > 0.93 else 0
        v = 236 + n + spark
        return clamp_rgb(v * 0.97, v * 0.99, v)

    texs["frost_top.png"] = make_tex(frost_top_fn)

    def frost_side_fn(x, y):
        edge = 8 + int(h2(x, 1, 93) * 5)
        if y < edge:
            return frost_top_fn(x, y)
        return loam_fn(x, y)

    texs["frost_side.png"] = make_tex(frost_side_fn)

    # 8. Cedar Bark & Top + Cedar Planks + Pale Ash Panel
    def cedar_bark_fn(x, y):
        ridge = math.sin(x * 0.9 + h2(x, y // 6, 101) * 1.5) * 14
        n = (h2(x, y, 102) - 0.5) * 16
        v = 84 + ridge + n
        return clamp_rgb(v * 1.18, v * 0.80, v * 0.52)

    texs["cedar_bark.png"] = make_tex(cedar_bark_fn)

    def cedar_top_fn(x, y):
        d = math.hypot(x - 15.5, y - 15.5)
        if d > 13.0:
            return cedar_bark_fn(x, y)
        ring = math.sin(d * 1.55) * 13
        v = 166 + ring + (h2(x, y, 103) - 0.5) * 10
        return clamp_rgb(v * 1.10, v * 0.86, v * 0.58)

    texs["cedar_top.png"] = make_tex(cedar_top_fn)

    def planks_fn(x, y):
        plank = y // 8
        seam = (y % 8 == 0) or ((x + plank * 11) % 32 == 0)
        grain = math.sin(x * 0.35 + plank * 2.1) * 8 + (h2(x, y, 111) - 0.5) * 12
        v = 158 + grain - (26 if seam else 0)
        return clamp_rgb(v * 1.14, v * 0.85, v * 0.54)

    texs["cedar_planks.png"] = make_tex(planks_fn)

    def ash_fn(x, y):
        plank = x // 8
        seam = (x % 8 == 0) or (y == 0) or (y == 31)
        grain = math.sin(y * 0.4 + plank * 1.7) * 7 + (h2(x, y, 121) - 0.5) * 10
        v = 212 + grain - (22 if seam else 0)
        return clamp_rgb(v * 1.02, v * 0.97, v * 0.84)

    texs["ash_panel.png"] = make_tex(ash_fn)

    # 9. Canopy Foliage
    def canopy_fn(x, y):
        cluster = h2(x // 2, y // 2, 131)
        n = h2(x, y, 132)
        dark = -28 if cluster < 0.24 else (16 if cluster > 0.78 else 0)
        g = 116 + dark + (n - 0.5) * 24
        return clamp_rgb(g * 0.44, g, g * 0.48)

    texs["canopy.png"] = make_tex(canopy_fn)

    # 10. Architectural Clear Framed Glass (translucent!)
    def glass_fn(x, y):
        border = x < 2 or x >= 30 or y < 2 or y >= 30
        inner_frame = (x == 2 or x == 29 or y == 2 or y == 29)
        streak = (10 <= x + y <= 13) or (24 <= x + y <= 26 and x > 8 and y < 22)
        if border:
            return clamp_rgb(196, 206, 214, 240)
        if inner_frame:
            return clamp_rgb(120, 136, 148, 190)
        if streak:
            return clamp_rgb(235, 246, 255, 145)
        return clamp_rgb(175, 210, 232, 58)

    texs["clear_glass.png"] = make_tex(glass_fn)

    # 11. Crystal Water (translucent!)
    def water_fn(x, y):
        caustic = math.sin(x * 0.39 + math.cos(y * 0.39) * 2.0) + math.cos(y * 0.39 - math.sin(x * 0.2) * 1.5)
        c = caustic * 16 + (h2(x, y, 151) - 0.5) * 10
        return clamp_rgb(36 + c * 0.6, 126 + c * 0.85, 208 + c, 182)

    texs["azure_water.png"] = make_tex(water_fn)

    # 12. Prism Ice (translucent!)
    def ice_fn(x, y):
        facet = abs(((x + y) % 16) - 8) * 3.2
        glint = 22 if (x - y) % 15 == 0 else 0
        v = 178 + facet + glint
        return clamp_rgb(v * 0.80, v * 0.94, min(255, v * 1.08), 212)

    texs["prism_ice.png"] = make_tex(ice_fn)

    # 13. Warm Lumen Lantern & Cold Neon Monolith
    def lumen_fn(x, y):
        frame = x < 3 or x >= 29 or y < 3 or y >= 29 or (abs(x - 15.5) > 10.5 and abs(y - 15.5) > 10.5)
        if frame:
            return clamp_rgb(58, 54, 50)
        d = max(abs(x - 15.5), abs(y - 15.5)) / 13.0
        glow = max(0.0, 1.0 - d * 0.42)
        return clamp_rgb(255 * glow, 232 * glow, 168 * glow)

    texs["lumen_core.png"] = make_tex(lumen_fn)

    def cyan_lamp_fn(x, y):
        frame = x < 3 or x >= 29 or y < 3 or y >= 29 or x == 15 or x == 16 or y == 15 or y == 16
        if frame:
            return clamp_rgb(44, 48, 52)
        d = math.hypot(x - 15.5, y - 15.5) / 18.0
        g = max(0.45, 1.0 - d * 0.35)
        return clamp_rgb(172 * g, 240 * g, 255 * g)

    texs["cyan_lamp.png"] = make_tex(cyan_lamp_fn)

    # 14. Brushed Steel Plate
    def steel_fn(x, y):
        border = x == 0 or y == 0 or x == 31 or y == 31
        rivet = (x in (3, 28) and y in (3, 28))
        brush = (h2(0, y, 181) - 0.5) * 14 + math.sin((x + y) * 0.16) * 10
        v = (96 if border else (212 if rivet else 156 + brush))
        return clamp_rgb(v * 0.98, v, v * 1.03)

    texs["steel_plate.png"] = make_tex(steel_fn)

    # 15. Core Monolith (Foundation)
    def monolith_fn(x, y):
        cell = ((x // 4) ^ (y // 4)) & 3
        v = 34 + cell * 8 + (h2(x, y, 191) - 0.5) * 14
        return clamp_rgb(v, v, v * 1.04)

    texs["monolith.png"] = make_tex(monolith_fn)

    # 16. Fluted White Marble Pillar
    def marble_fluted_fn(x, y):
        flute = math.cos((x % 8 - 3.5) * 0.72) * 18
        seam = -16 if (y == 0 or y == 31) else 0
        v = 224 + flute + seam + (h2(x, y, 201) - 0.5) * 6
        return clamp_rgb(v, v, v * 1.01)

    texs["marble_fluted.png"] = make_tex(marble_fluted_fn)

    def marble_top_fn(x, y):
        ring = max(abs(x - 15.5), abs(y - 15.5))
        b = -18 if (int(ring) in (2, 11, 15)) else 0
        v = 232 + b + (h2(x, y, 202) - 0.5) * 8
        return clamp_rgb(v, v, v * 1.01)

    texs["marble_top.png"] = make_tex(marble_top_fn)

    # 17. Obsidian Geometric Grid
    def obsidian_grid_fn(x, y):
        line_on = (x % 8 == 0 or y % 8 == 0)
        v = 92 if line_on else (30 + (h2(x, y, 211) - 0.5) * 10)
        return clamp_rgb(v, v, v * 1.05)

    texs["obsidian_grid.png"] = make_tex(obsidian_grid_fn)

    # 18. Amber Vein & Cobalt Mosaic
    def amber_fn(x, y):
        crystal = h2(x // 2, y // 2, 221)
        if crystal > 0.77 and 4 <= x <= 27 and 4 <= y <= 27:
            g = 215 + (h2(x, y, 222) - 0.5) * 40
            return clamp_rgb(255, g * 0.78, g * 0.22)
        return slate_fn(x, y)

    texs["amber_vein.png"] = make_tex(amber_fn)

    def cobalt_fn(x, y):
        gx, gy = x % 8, y % 8
        grout = gx == 0 or gy == 0
        if grout:
            return clamp_rgb(42, 54, 78)
        v = 150 + ((x // 8 + y // 8) % 2) * 18 + (h2(x, y, 231) - 0.5) * 16
        return clamp_rgb(v * 0.36, v * 0.58, v * 1.15)

    texs["cobalt_mosaic.png"] = make_tex(cobalt_fn)

    # 19. Demolition Charge Cube
    def charge_side_fn(x, y):
        if 10 <= y <= 21:
            stripe = ((x + y) // 4) % 2 == 0
            return clamp_rgb(232, 232, 232) if stripe else clamp_rgb(48, 48, 48)
        v = 178 + (h2(x, y, 241) - 0.5) * 20 - (24 if x % 8 == 0 else 0)
        return clamp_rgb(v, v * 0.30, v * 0.26)

    texs["charge_side.png"] = make_tex(charge_side_fn)

    def charge_top_fn(x, y):
        d = max(abs(x - 15.5), abs(y - 15.5))
        if d < 4.0:
            return clamp_rgb(245, 220, 120)
        if d < 7.0:
            return clamp_rgb(48, 48, 48)
        v = 168 + (h2(x, y, 242) - 0.5) * 18
        return clamp_rgb(v, v * 0.32, v * 0.28)

    texs["charge_top.png"] = make_tex(charge_top_fn)

    def charge_bot_fn(x, y):
        v = 68 + (h2(x, y, 243) - 0.5) * 16 - (16 if (x % 8 == 0 or y % 8 == 0) else 0)
        return clamp_rgb(v, v, v)

    texs["charge_bot.png"] = make_tex(charge_bot_fn)

    # 20. Geometrium Monochrome Architect Avatar (32x32 head, body, limb)
    def avatar_head_fn(x, y):
        border = x < 2 or x >= 30 or y < 2 or y >= 30
        visor = 9 <= y <= 17 and 5 <= x <= 26
        visor_glow = 11 <= y <= 15 and ((7 <= x <= 12) or (19 <= x <= 24))
        if visor_glow:
            return clamp_rgb(244, 244, 244)
        if visor:
            return clamp_rgb(24, 24, 24)
        v = 62 if border else (96 + (h2(x, y, 251) - 0.5) * 10)
        return clamp_rgb(v, v, v)

    texs["avatar_head.png"] = make_tex(avatar_head_fn)

    def avatar_body_fn(x, y):
        seam = x < 2 or x >= 30 or y < 2 or y >= 30 or x in (15, 16)
        emblem = 8 <= y <= 18 and abs(x - 15.5) + abs(y - 13.0) <= 5.5
        if emblem:
            return clamp_rgb(220, 220, 220)
        v = 44 if seam else (76 + (h2(x, y, 252) - 0.5) * 10)
        return clamp_rgb(v, v, v)

    texs["avatar_body.png"] = make_tex(avatar_body_fn)

    def avatar_limb_fn(x, y):
        cuff = y >= 24
        seam = x == 0 or x == 31
        v = 40 if seam else ((118 if cuff else 68) + (h2(x, y, 253) - 0.5) * 8)
        return clamp_rgb(v, v, v)

    texs["avatar_limb.png"] = make_tex(avatar_limb_fn)

    # 21. 10-stage 32x32 Mining Fracture Overlays (crack_0.png .. crack_9.png)
    for stage in range(10):
        reach = 3.0 + stage * 2.2
        branches = 3 + stage

        def make_crack(st, rch, br):
            def fn(x, y):
                dx, dy = x - 15.5, y - 15.5
                dist = math.hypot(dx, dy)
                if dist > rch:
                    return (0, 0, 0, 0)
                ang = math.atan2(dy, dx)
                jag = math.sin(dist * 1.35 + st) * 0.22
                best = min(abs(((ang + jag - k * (2.0 * math.pi / br) + math.pi) % (2.0 * math.pi)) - math.pi)
                           for k in range(br))
                ring_crack = st >= 5 and abs(dist - rch * 0.58) < 0.85
                if best * max(1.5, dist) < 0.92 or ring_crack:
                    return (18, 18, 18, min(235, 130 + st * 11))
                return (0, 0, 0, 0)
            return fn

        texs[f"crack_{stage}.png"] = make_tex(make_crack(stage, reach, branches))

    for name, px in sorted(texs.items()):
        write_png(os.path.join(TEX_DIR, name), 32, 32, px)

    return len(texs)


def write_wav(path, samples, rate=22050):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    pcm = bytearray()
    for s in samples:
        v = max(-32767, min(32767, int(s * 32767.0)))
        pcm.extend(struct.pack("<h", v))
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(pcm)) + bytes(pcm))


def generate_audio():
    os.makedirs(AUD_DIR, exist_ok=True)
    for old in os.listdir(AUD_DIR):
        os.remove(os.path.join(AUD_DIR, old))
    rate = 22050

    # Calm minimalist ambient loop (4.0 s seamless loop)
    amb_n = rate * 4
    amb = []
    for i in range(amb_n):
        t = i / rate
        # Exact integer harmonics of 0.25 Hz for seamless zero-click loop
        v = (0.045 * math.sin(2 * math.pi * 110.0 * t)
             + 0.030 * math.sin(2 * math.pi * 165.0 * t)
             + 0.022 * math.sin(2 * math.pi * 220.0 * t) * (0.6 + 0.4 * math.sin(2 * math.pi * 0.25 * t))
             + 0.014 * math.sin(2 * math.pi * 330.0 * t) * (0.5 + 0.5 * math.cos(2 * math.pi * 0.50 * t)))
        amb.append(v)
    write_wav(os.path.join(AUD_DIR, "geo_ambient.wav"), amb, rate)

    # UI tap (crisp soft monochrome click)
    write_wav(os.path.join(AUD_DIR, "ui_tap.wav"),
              [0.24 * math.exp(-i / (rate * 0.012)) * math.sin(2 * math.pi * 620.0 * (i / rate))
               for i in range(int(rate * 0.05))], rate)

    # Block place (solid architectural thud)
    write_wav(os.path.join(AUD_DIR, "block_place.wav"),
              [0.34 * math.exp(-i / (rate * 0.028)) * (
                  0.7 * math.sin(2 * math.pi * (190.0 - 70.0 * (i / (rate * 0.09))) * (i / rate))
                  + 0.3 * (h2(i, 7, 1) * 2 - 1))
               for i in range(int(rate * 0.09))], rate)

    # Block break (crisp mineral shatter)
    write_wav(os.path.join(AUD_DIR, "block_break.wav"),
              [0.36 * math.exp(-i / (rate * 0.045)) * (
                  0.55 * (h2(i, 13, 2) * 2 - 1) + 0.45 * math.sin(2 * math.pi * 145.0 * (i / rate)))
               for i in range(int(rate * 0.13))], rate)

    # Footstep
    write_wav(os.path.join(AUD_DIR, "step.wav"),
              [0.16 * math.exp(-i / (rate * 0.018)) * (h2(i, 19, 3) * 2 - 1)
               for i in range(int(rate * 0.055))], rate)

    # Jump
    write_wav(os.path.join(AUD_DIR, "jump.wav"),
              [0.18 * math.exp(-i / (rate * 0.04)) * math.sin(2 * math.pi * (150.0 + 180.0 * (i / (rate * 0.08))) * (i / rate))
               for i in range(int(rate * 0.08))], rate)

    # Demolition Charge Boom
    write_wav(os.path.join(AUD_DIR, "charge_boom.wav"),
              [0.52 * math.exp(-i / (rate * 0.16)) * (
                  0.65 * (h2(i // 2, 23, 4) * 2 - 1) + 0.35 * math.sin(2 * math.pi * 58.0 * (i / rate)))
               for i in range(int(rate * 0.55))], rate)

    # Water splash
    write_wav(os.path.join(AUD_DIR, "water_splash.wav"),
              [0.24 * math.exp(-i / (rate * 0.06)) * (h2(i, 31, 5) * 2 - 1) * (0.6 + 0.4 * math.sin(i * 0.09))
               for i in range(int(rate * 0.18))], rate)


def generate_icons():
    densities = [
        ("mipmap-mdpi", 48),
        ("mipmap-hdpi", 72),
        ("mipmap-xhdpi", 96),
        ("mipmap-xxhdpi", 144),
        ("mipmap-xxxhdpi", 192),
    ]
    for folder, sz in densities:
        cx = cy = (sz - 1) * 0.5
        rad = sz * 0.36
        px = []
        for y in range(sz):
            row = []
            for x in range(sz):
                # Rounded dark-gray squircle badge + crisp isometric monochrome 3D Geometrium cube
                nx = abs(x - cx) / (sz * 0.47)
                ny = abs(y - cy) / (sz * 0.47)
                inside_bg = (nx ** 5 + ny ** 5) <= 1.0
                if not inside_bg:
                    row.append((0, 0, 0, 0))
                    continue
                dx, dy = (x - cx) / rad, (y - cy) / rad
                # Isometric hexagon test: |dx| <= cos(30)=0.866 and |dy| + 0.577*|dx| <= 1.0
                ax = abs(dx)
                in_hex = ax <= 0.866 and (abs(dy) + 0.57735 * ax) <= 1.0
                if not in_hex:
                    bg = 28 + int((1.0 - (y / sz)) * 14)
                    border = 78 if (nx ** 5 + ny ** 5) > 0.84 else bg
                    row.append((border, border, border, 255))
                    continue
                # Three isometric cube faces: top, left, right + crisp wireframe edges
                top_face = dy < -0.57735 * ax + 0.0 and (dy + 0.57735 * ax) >= -1.0 and dy < 0.57735 * (-ax) + 0.0
                # Simpler exact isometric face split about center (0, 0):
                # Vector from center: if dy < 0.57735 * dx and dy < -0.57735 * dx -> TOP face!
                edge_top_r = abs(dy - 0.57735 * dx) < (1.6 / rad) and dx >= -0.02
                edge_top_l = abs(dy + 0.57735 * dx) < (1.6 / rad) and dx <= 0.02
                edge_mid = abs(dx) < (1.5 / rad) and dy >= -0.02
                outer_edge = ax > 0.866 - (1.8 / rad) or (abs(dy) + 0.57735 * ax) > 1.0 - (1.8 / rad)
                if dy < 0.57735 * dx and dy < -0.57735 * dx:
                    lum = 228
                elif dx < 0.0:
                    lum = 152
                else:
                    lum = 96
                if outer_edge or edge_top_r or edge_top_l or edge_mid:
                    lum = 248
                row.append((lum, lum, lum, 255))
            px.append(row)
        out_p = os.path.join(ROOT, "platform", "android", "res", folder, "ic_launcher.png")
        write_png(out_p, sz, sz, px)


def main():
    n_tex = generate_textures()
    generate_audio()
    generate_icons()
    print(f"Geometrium assets generated: {n_tex} 32x32 PNG textures, 8 WAV sounds, 5 launcher icons")


if __name__ == "__main__":
    main()

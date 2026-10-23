#!/usr/bin/env python3
"""Generates assets/textures/mc_atlas.png (256x256 RGBA, 16x16 pixel-art tiles)."""

import math
import os
import struct
import zlib

W = 256
H = 256
px = [(0, 0, 0, 0)] * (W * H)


def h2d(x: int, y: int, seed: int) -> float:
    n = (x * 374761393 + y * 668265263 + seed * 1274126177) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    n = n ^ (n >> 16)
    return (n & 0xFFFF) / 65535.0


def clamp_byte(v: float) -> int:
    return max(0, min(255, int(round(v))))


def shade_rgb(rgb, k: float, alpha: int = 255):
    return (clamp_byte(rgb[0] * k), clamp_byte(rgb[1] * k), clamp_byte(rgb[2] * k), alpha)


def put(tile: int, x: int, y: int, rgba) -> None:
    tx = (tile % 16) * 16 + x
    ty = (tile // 16) * 16 + y
    px[ty * W + tx] = rgba


def fill_noise(tile: int, base, spread: float, seed: int, alpha: int = 255) -> None:
    for y in range(16):
        for x in range(16):
            k = 1.0 + (h2d(x, y, seed) - 0.5) * 2.0 * spread
            put(tile, x, y, shade_rgb(base, k, alpha))


def draw_ores(tile: int, vein_rgb, hi_rgb, seed: int) -> None:
    fill_noise(tile, (126, 126, 126), 0.14, 30)
    spots = [(3, 3), (4, 3), (3, 4), (10, 4), (11, 5), (5, 10), (6, 10), (6, 11), (12, 11), (11, 12)]
    for i, (sx, sy) in enumerate(spots):
        dx = int((h2d(i, 1, seed) - 0.5) * 2)
        dy = int((h2d(i, 2, seed) - 0.5) * 2)
        cx = max(1, min(14, sx + dx))
        cy = max(1, min(14, sy + dy))
        put(tile, cx, cy, hi_rgb if i % 3 == 0 else vein_rgb)


def build_blocks() -> None:
    # 0: Grass top
    fill_noise(0, (89, 193, 53), 0.16, 10)
    # 2: Dirt
    fill_noise(2, (134, 96, 67), 0.18, 20)
    for sx, sy in ((3, 5), (11, 2), (7, 12), (13, 9), (2, 14)):
        put(2, sx, sy, (112, 108, 104, 255))
    # 1: Grass side + 16: Snowy grass side
    for t, cap_base, seed in ((1, (89, 193, 53), 15), (16, (242, 248, 255), 16)):
        for y in range(16):
            for x in range(16):
                put(t, x, y, px[(2 // 16 * 16 + y) * W + (2 % 16) * 16 + x])
        for x in range(16):
            edge = 3 + int(h2d(x, 0, seed) * 2.9)
            for y in range(edge):
                put(t, x, y, shade_rgb(cap_base, 1.0 + (h2d(x, y, seed + 5) - 0.5) * 0.22))
            if edge < 16:
                r, g, b, _ = px[(t // 16 * 16 + edge) * W + (t % 16) * 16 + x]
                put(t, x, edge, (int(r * 0.72), int(g * 0.72), int(b * 0.72), 255))

    # 3: Stone (layered strata)
    for y in range(16):
        band = 0.94 + 0.10 * math.sin(y * 1.15)
        for x in range(16):
            k = band * (1.0 + (h2d(x // 2 + x % 2, y, 31) - 0.5) * 0.18)
            put(3, x, y, shade_rgb((125, 125, 125), k))

    # 4: Cobblestone (cellular stones + dark mortar)
    centers = [(3, 3), (9, 2), (14, 4), (2, 9), (7, 8), (12, 9), (4, 14), (10, 14), (15, 14)]
    for y in range(16):
        for x in range(16):
            dists = []
            for idx, (cx, cy) in enumerate(centers):
                dx = min(abs(x - cx), 16 - abs(x - cx))
                dy = min(abs(y - cy), 16 - abs(y - cy))
                dists.append((math.hypot(dx * 1.05, dy), idx))
            dists.sort()
            edge_gap = dists[1][0] - dists[0][0]
            if edge_gap < 0.85:
                put(4, x, y, shade_rgb((66, 66, 66), 0.9 + 0.2 * h2d(x, y, 41)))
            else:
                k = 0.88 + 0.28 * h2d(dists[0][1], 0, 42) + (0.08 if y < centers[dists[0][1]][1] else -0.05)
                put(4, x, y, shade_rgb((132, 132, 132), k))

    # 5: Oak Planks
    for y in range(16):
        plank = y // 4
        joint_x = (plank * 7 + 3) % 16
        for x in range(16):
            if y % 4 == 3 or x == joint_x:
                put(5, x, y, shade_rgb((118, 86, 48), 0.95))
            else:
                k = 0.93 + 0.14 * h2d(x // 3, y + plank * 5, 51)
                put(5, x, y, shade_rgb((178, 137, 82), k))

    # 6: Oak Log side, 7: Oak Log top
    for y in range(16):
        for x in range(16):
            ridge = 0.84 + 0.28 * h2d(x, y // 4, 61)
            put(6, x, y, shade_rgb((102, 76, 44), ridge))
            d = max(abs(x - 7.5), abs(y - 7.5))
            if d > 6.0:
                put(7, x, y, shade_rgb((102, 76, 44), 0.9 + 0.15 * h2d(x, y, 71)))
            else:
                ring_k = 0.90 + 0.14 * math.sin(d * 2.2)
                put(7, x, y, shade_rgb((186, 150, 96), ring_k))

    # 8: Leaves (with crisp leaf alpha cutouts)
    for y in range(16):
        for x in range(16):
            v = h2d(x, y, 81)
            if v < 0.14:
                put(8, x, y, (32, 84, 24, 0))
            else:
                put(8, x, y, shade_rgb((56, 152, 42), 0.72 + 0.55 * v))

    # 9: Sand, 10: Sandstone side, 11: Sandstone top
    for y in range(16):
        for x in range(16):
            wave = 0.95 + 0.08 * math.sin(y * 1.4 + x * 0.35) + (h2d(x, y, 91) - 0.5) * 0.08
            put(9, x, y, shade_rgb((222, 209, 158), wave))
            put(11, x, y, shade_rgb((226, 214, 166), 0.96 + (h2d(x, y, 92) - 0.5) * 0.08))
            band = 0.85 if y in (3, 7, 12) else (1.04 if y < 3 else 0.97)
            put(10, x, y, shade_rgb((218, 204, 152), band * (0.97 + 0.06 * h2d(x, y, 93))))

    # 12: Translucent Water
    for y in range(16):
        for x in range(16):
            ripple = math.sin(x * 0.8 + y * 0.9) * 0.5 + 0.5
            col = (38 + int(42 * ripple), 112 + int(56 * ripple), 224 + int(28 * ripple), 174)
            put(12, x, y, col)

    # 13: Red Bricks
    for y in range(16):
        row = y // 4
        off = 0 if row % 2 == 0 else 4
        for x in range(16):
            if y % 4 == 3 or (x + off) % 8 == 0:
                put(13, x, y, shade_rgb((196, 190, 182), 0.95 + 0.08 * h2d(x, y, 131)))
            else:
                brick_id = row * 4 + ((x + off) // 8)
                k = 0.88 + 0.22 * h2d(brick_id, y % 4, 132)
                put(13, x, y, shade_rgb((168, 74, 58), k))

    # 14: Glass (crisp cyan-white border + diagonal glint + transparent inside)
    for y in range(16):
        for x in range(16):
            border = x in (0, 15) or y in (0, 15)
            glint = (x + y in (5, 6) and 2 <= x <= 6) or (x + y == 21 and 9 <= x <= 13)
            if border:
                put(14, x, y, (205, 238, 250, 245))
            elif glint:
                put(14, x, y, (235, 252, 255, 220))
            else:
                put(14, x, y, (180, 230, 250, 26))

    # 15: Snow, 17: Ice
    fill_noise(15, (242, 248, 255), 0.05, 151)
    for y in range(16):
        for x in range(16):
            streak = 1.08 if (x - y) % 7 in (0, 1) else 0.96
            put(17, x, y, shade_rgb((142, 192, 252), streak * (0.97 + 0.06 * h2d(x, y, 171)), 204))

    # 18: Coal Ore, 19: Iron Ore, 21: Diamond Ore
    draw_ores(18, (34, 34, 38, 255), (58, 58, 64, 255), 181)
    draw_ores(19, (214, 174, 142, 255), (242, 206, 178, 255), 191)
    draw_ores(21, (56, 226, 232, 255), (168, 255, 255, 255), 211)

    # 20: Gold Block, 22: Diamond Block, 23: Emerald Block
    for tile, base in ((20, (244, 202, 52)), (22, (74, 232, 226)), (23, (46, 214, 96))):
        for y in range(16):
            for x in range(16):
                bevel = 1.18 if (x <= 1 or y <= 1) else (0.76 if (x >= 14 or y >= 14) else 1.0)
                sheen = 1.12 if (x + y) % 13 in (3, 4) else 0.97
                put(tile, x, y, shade_rgb(base, bevel * sheen))

    # 24: Obsidian
    for y in range(16):
        for x in range(16):
            v = h2d(x, y, 241)
            put(24, x, y, (28 + int(32 * v), 18 + int(20 * v), 48 + int(48 * v), 255))

    # 25: Glowstone
    for y in range(16):
        for x in range(16):
            v = h2d(x, y, 251)
            if v > 0.62:
                put(25, x, y, (255, 248, 184, 255))
            elif v > 0.28:
                put(25, x, y, (238, 192, 84, 255))
            else:
                put(25, x, y, (158, 108, 42, 255))

    # 26: Bookshelf
    book_cols = [(186, 48, 48), (48, 112, 186), (52, 158, 72), (196, 162, 48), (138, 62, 168)]
    for y in range(16):
        for x in range(16):
            if y in (0, 1, 7, 8, 14, 15) or x in (0, 15):
                put(26, x, y, shade_rgb((178, 137, 82), 0.92 + 0.1 * h2d(x, y, 261)))
            else:
                shelf = 0 if y < 7 else 1
                col = book_cols[((x // 2) + shelf * 3) % len(book_cols)]
                spine = 0.72 if x % 2 == 0 else 1.05
                if y in (3, 10):
                    spine *= 1.18
                put(26, x, y, shade_rgb(col, spine))

    # 27: Crafting Table top, 28: Crafting Table side
    for y in range(16):
        for x in range(16):
            put(27, x, y, px[(5 // 16 * 16 + y) * W + (5 % 16) * 16 + x])
            put(28, x, y, px[(5 // 16 * 16 + y) * W + (5 % 16) * 16 + x])
            if 2 <= x <= 13 and 2 <= y <= 13:
                if x in (2, 6, 9, 13) or y in (2, 6, 9, 13):
                    put(27, x, y, (92, 62, 34, 255))
                else:
                    put(27, x, y, (198, 158, 104, 255))
            if y < 3:
                put(28, x, y, (112, 74, 40, 255))
            elif 4 <= x <= 6 and 5 <= y <= 12:
                put(28, x, y, (186, 194, 204, 255) if y < 9 else (112, 74, 40, 255))
            elif 10 <= x <= 12 and 5 <= y <= 11:
                put(28, x, y, (196, 62, 54, 255))

    # 29: TNT top, 30: TNT side, 31: TNT bottom, 35: TNT flash
    fill_noise(29, (206, 52, 42), 0.10, 291)
    fill_noise(31, (186, 46, 38), 0.10, 311)
    fill_noise(35, (255, 252, 246), 0.03, 351)
    for y in range(6, 10):
        for x in range(6, 10):
            put(29, x, y, (38, 38, 38, 255) if (x + y) % 2 == 0 else (96, 96, 96, 255))
    for y in range(16):
        for x in range(16):
            if 5 <= y <= 10:
                put(30, x, y, shade_rgb((242, 240, 236), 0.96 + 0.06 * h2d(x, y, 301)))
            else:
                put(30, x, y, shade_rgb((210, 48, 38), 0.88 + 0.18 * h2d(x // 2, y, 302)))
    # Crisp "TNT" pixel glyphs across white band rows 6..9
    tnt_rows = [
        "111010010111",
        "010011010010",
        "010010110010",
        "010010010010",
    ]
    for ry, row in enumerate(tnt_rows):
        for rx, ch in enumerate(row):
            if ch == "1":
                put(30, 2 + rx, 6 + ry, (28, 28, 28, 255))

    # 32: Red Wool, 33: Blue Wool, 34: Bedrock
    fill_noise(32, (196, 48, 48), 0.14, 321)
    fill_noise(33, (48, 96, 204), 0.14, 331)
    for y in range(16):
        for x in range(16):
            v = h2d(x, y, 341)
            g = 26 + int(74 * (v * v))
            put(34, x, y, (g, g, g + 4, 255))


def build_skins() -> None:
    skins = [
        # (face_tile, body_tile, skin_rgb, hair_rgb, eye_rgb, shirt_rgb, accent_rgb)
        (48, 49, (214, 168, 134), (78, 52, 34), (58, 92, 198), (0, 174, 186), (62, 74, 158)),   # Ordinary / Steve
        (50, 51, (64, 32, 38), (24, 16, 22), (255, 58, 68), (168, 32, 46), (38, 24, 32)),       # Azum
        (52, 53, (88, 158, 76), (54, 108, 48), (24, 38, 22), (48, 146, 152), (62, 84, 132)),    # Zombie Azum
        (54, 55, (232, 192, 162), (218, 38, 42), (42, 82, 148), (214, 36, 40), (246, 246, 250)),# Santa
        (56, 57, (226, 182, 142), (238, 186, 42), (42, 64, 96), (206, 118, 42), (92, 98, 112)), # buK
        (58, 59, (228, 188, 152), (42, 158, 64), (42, 96, 58), (44, 164, 68), (236, 198, 54)),  # Elf buK
        (60, 61, (198, 176, 232), (88, 42, 156), (224, 128, 255), (124, 58, 206), (236, 196, 255)), # Astra
        (62, 63, (36, 44, 40), (18, 22, 20), (64, 255, 118), (28, 34, 32), (56, 238, 108)),     # CodeCracker
    ]
    for ft, bt, skin, hair, eye, shirt, acc in skins:
        for y in range(16):
            for x in range(16):
                col = hair if (y < 4 or (y < 6 and x in (0, 1, 14, 15))) else skin
                if y in (7, 8) and x in (3, 4, 11, 12):
                    col = eye
                elif y in (7, 8) and x in (2, 13):
                    col = (245, 248, 255)
                elif ft == 54 and y >= 11:
                    col = (248, 248, 252)
                elif y == 12 and 6 <= x <= 9:
                    col = shade_rgb(skin, 0.72)[:3]
                put(ft, x, y, (col[0], col[1], col[2], 255))

                bcol = shirt if y < 11 else acc
                if y in (8, 9) and 6 <= x <= 9:
                    bcol = acc
                put(bt, x, y, shade_rgb(bcol, 0.94 + 0.12 * h2d(x, y, bt)))


def build_cracks() -> None:
    for stage in range(10):
        tile = 240 + stage
        reach = 1.8 + stage * 1.15
        for y in range(16):
            for x in range(16):
                dx = x - 7.5
                dy = y - 7.5
                dist_c = math.hypot(dx, dy)
                ang = math.atan2(dy, dx)
                spoke = abs(math.sin(ang * 3.0 + math.sin(dist_c * 0.9) * 1.2))
                ring_crack = abs((dist_c % 3.4) - 1.7)
                hit = dist_c <= reach and (spoke < 0.20 + stage * 0.015 or (stage >= 4 and ring_crack < 0.28))
                if hit:
                    put(tile, x, y, (18, 18, 20, min(225, 115 + stage * 12)))
                else:
                    put(tile, x, y, (0, 0, 0, 0))


def write_png(path: str) -> None:
    raw = bytearray()
    for y in range(H):
        raw.append(0)
        row = y * W
        for x in range(W):
            r, g, b, a = px[row + x]
            raw.extend((r, g, b, a))

    def chunk(tag: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(data)


if __name__ == "__main__":
    build_blocks()
    build_skins()
    build_cracks()
    out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "textures", "mc_atlas.png")
    write_png(out)
    print(f"Generated {out} ({os.path.getsize(out)} bytes)")

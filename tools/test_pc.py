#!/usr/bin/env python3
"""Checks of the PC build: it compiles and plays without a window.

Builds the Linux harness (gcc) from the same sources as the .exe and runs the real
game with fixed frames: the warning screen -> lobby -> Play -> Solo -> battle, the
stick and a punch through the same layer the PC keyboard uses. A separate run
checks signing in against the fake database in test/fake_firebase.py over real
HTTP (the only way to see the whole network path: WinHTTP cannot run in the
sandbox, and the POSIX transport in the harness behaves like it), and the joke
HitboxChaos flag set from the admin panel.

Run: python3 tools/test_pc.py            (needs gcc + g++)
     python3 tools/test_pc.py --no-build (harness already built)
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "pc"
BINARY = BUILD / "cb4_headless"
SCRATCH = BUILD / "test_pc"
FAKE_PORT = 18779
FAILED: list[str] = []


def check(name: str, ok: bool, detail: str = "") -> None:
    print(f"  {'ok  ' if ok else 'FAIL'} {name}" + (f" - {detail}" if detail and not ok else ""))
    if not ok:
        FAILED.append(name)


def run(command: list[str], cwd: Path = ROOT, timeout: int = 600) -> subprocess.CompletedProcess:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=timeout)


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def build(fake_url: str) -> None:
    print("== 1. Сборка ==")
    result = run(
        [
            sys.executable,
            "tools/build_pc.py",
            "--linux",
            "--no-assets",
            "--firebase",
            fake_url,
            "--data-path",
            str(SCRATCH / "home"),
        ]
    )
    check("tools/build_pc.py собрал харнесс", result.returncode == 0, result.stderr[-2000:])
    check("бинарник на месте и непустой", BINARY.is_file() and BINARY.stat().st_size > 200_000)


def stage_assets() -> Path:
    """The APK staging layout (textures/, fonts/, sounds/) is what the game
    reads on both platforms; the harness gets its own copy."""
    assets = SCRATCH / "assets"
    if not (assets / "grass.png").is_file():
        shutil.rmtree(assets, ignore_errors=True)
        result = run([sys.executable, "stage_assets.py", "assets", str(assets)])
        check("ресурсы подготовлены stage_assets.py", result.returncode == 0, result.stderr[-1500:])
    return assets


def harness(assets: Path, data: Path, args: list[str]) -> subprocess.CompletedProcess:
    shutil.rmtree(data, ignore_errors=True)
    data.mkdir(parents=True, exist_ok=True)
    return run([str(BINARY), "--headless", "--size", "540x960", "--assets", str(assets), "--data", str(data), *args])


def parse_reports(stdout: str) -> dict[int, dict[str, str]]:
    reports: dict[int, dict[str, str]] = {}
    for line in stdout.splitlines():
        match = re.match(r"frame (\d+): (.*)", line)
        if not match:
            continue
        fields = dict(part.split("=", 1) for part in match.group(2).split() if "=" in part)
        reports[int(match.group(1))] = fields
    return reports


def read_bmp(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    width, height = struct.unpack_from("<ii", data, 18)
    offset = struct.unpack_from("<I", data, 10)[0]
    stride = (width * 3 + 3) // 4 * 4
    return width, height, data[offset : offset + stride * height]


def bmp_colors(pixels: bytes, width: int, height: int, step: int = 3) -> dict[bytes, int]:
    stride = (width * 3 + 3) // 4 * 4
    colors: dict[bytes, int] = {}
    for y in range(0, height, step):
        row = pixels[y * stride : (y + 1) * stride]
        for x in range(0, width, step):
            color = row[x * 3 : x * 3 + 3]
            colors[color] = colors.get(color, 0) + 1
    return colors


def test_pixels(assets: Path) -> None:
    print("\n== 2. Растеризатор: известные фигуры ==")
    shot = SCRATCH / "gfx.bmp"
    result = harness(assets, SCRATCH / "gfx_home", ["--gfx-test", "--dump", f"0:{shot}"])
    check("тестовая сцена нарисована", result.returncode == 0 and shot.is_file(), result.stderr[-1500:])
    if not shot.is_file():
        return
    width, height, pixels = read_bmp(shot)
    stride = (width * 3 + 3) // 4 * 4

    def pixel(x: int, y: int) -> str:
        row = pixels[(height - 1 - y) * stride : (height - y) * stride]  # BMP is bottom-up
        blue, green, red = row[x * 3], row[x * 3 + 1], row[x * 3 + 2]  # and stores B,G,R
        return f"{red:02x}{green:02x}{blue:02x}"

    check("фон залит цветом bg_screen", pixel(5, 5) == "1a1a2e", pixel(5, 5))
    check("прямоугольник нужного цвета", pixel(60, 40) == "e94560", pixel(60, 40))
    check("круг (треугольники) залит", pixel(240, 90) == "00ff00", pixel(240, 90))
    check("скруглённый прямоугольник залит", pixel(60, 330) == "3366ff", pixel(60, 330))
    check("текст отрисован глифами", any(c not in (b"\x1a\x1a\x2e", b"\x00\x00\x00") for c in
                                         bmp_colors(pixels[420 * stride : 470 * stride], width, 108)))


def test_menu_and_battle(assets: Path) -> None:
    print("\n== 3. Меню и бой ==")
    shot = SCRATCH / "battle.bmp"
    result = harness(
        assets,
        SCRATCH / "play",
        [
            "--frames", "700",
            "--touch", "220:360:686:0", "--touch", "223:360:686:1",   # «Понимаю и принимаю»
            "--touch", "400:270:336:0", "--touch", "403:270:336:1",   # «Играть»
            "--touch", "460:270:404:0", "--touch", "463:270:404:1",   # «Соло»
            "--report", "236",
            "--report", "430",
            "--report", "590",
            "--stick", "600:0:-1",                                    # клавиша W
            "--atk", "620:1", "--atk", "640:0",                       # пробел
            "--report", "650",
            "--dump", f"690:{shot}",
            "--report", "699",
        ],
    )
    check("игра дошла до конца без ошибок", result.returncode == 0, result.stderr[-2000:])
    reports = parse_reports(result.stdout)
    after_consent = reports.get(236, {})
    check("согласие принято, предупреждение закрылось", after_consent.get("state") == "0", str(after_consent))
    modes = reports.get(430, {})
    check("«Играть» открывает выбор режима", modes.get("state") == "2", str(modes))
    battle = reports.get(650, {})
    before_punch = reports.get(590, {})
    check("бой начался (ST_SOLO)", battle.get("state") == "1" and battle.get("battle") == "1", str(battle))
    check("игрок жив и двигается стиком", float(battle.get("y", "0") or 0) < 470, str(battle))
    check(
        "удар дошёл до врага",
        float(battle.get("foe", "inf")) < float(before_punch.get("foe", "-inf")),
        f"до удара {before_punch.get('foe')}, после {battle.get('foe')}",
    )
    check("в бою не было ошибок JS-хуков", battle.get("error") == "0", str(battle))
    end = reports.get(699, {})
    check(
        "враг отвечает (игрок получает урон)",
        float(end.get("hp", "inf")) < float(battle.get("hp", "inf")),
        f"после удара {battle.get('hp')}, к кадру 699 {end.get('hp')}",
    )
    check("кадр боя сохранён", shot.is_file() and shot.stat().st_size > 100_000)
    if shot.is_file():
        width, height, pixels = read_bmp(shot)
        colors = bmp_colors(pixels, width, height, 5)
        check("кадр боя не пустой (много цветов)", len(colors) > 50, f"{len(colors)} цветов")


def test_controls(assets: Path) -> None:
    """PC keys: E holds the block, Q does not touch the screen for a class with no
    super.

    The ladder of taps leads into a solo fight with the ordinary cube (it has no
    super). Q used to press empty screen, the game read that as the stick and
    teleported the player into the corner of the arena.
    """
    print("\n== 3b. PC controls (mouse / block / super) ==")
    start = [
        "--touch", "220:360:686:0", "--touch", "223:360:686:1",   # consent
        "--touch", "400:270:336:0", "--touch", "403:270:336:1",   # Play
        "--touch", "460:270:404:0", "--touch", "463:270:404:1",   # Solo
    ]

    def play(name: str, extra: list[str]) -> dict[int, dict[str, str]]:
        result = harness(
            assets,
            SCRATCH / name,
            ["--frames", "1300", *start, "--report", "600", "--report", "800", "--report", "1000", *extra],
        )
        return parse_reports(result.stdout)

    plain = play("ctl_plain", [])
    held = play("ctl_block", ["--blk", "600:1"])
    check(
        "E (block) reduces damage taken",
        float(held.get(1000, {}).get("hp", "0")) > float(plain.get(1000, {}).get("hp", "0")),
        f"no block {plain.get(1000, {}).get('hp')}, with block {held.get(1000, {}).get('hp')}",
    )
    super_run = play("ctl_super", ["--super", "700:1", "--super", "720:0"])
    check(
        "Q does not move a player whose class has no super",
        super_run.get(800, {}).get("x") == plain.get(800, {}).get("x")
        and super_run.get(800, {}).get("y") == plain.get(800, {}).get("y"),
        f"without Q {plain.get(800, {}).get('x')},{plain.get(800, {}).get('y')}; "
        f"with Q {super_run.get(800, {}).get('x')},{super_run.get(800, {}).get('y')}",
    )


def test_walk_feel(assets: Path) -> None:
    """The cube ramps up and stops in about 0.08 s instead of tearing off.

    A released stick used to stop the cube in the same frame; now the speed fades
    over walk_stop_time and the ramp takes walk_accel_time, both 0.08 s. Checked
    from the per-frame reports: the first frame after the press moves much less
    than a steady walk, and after the release the cube still moves for a few
    frames and then stands.
    """
    print("\n== 3c. Cube ramp up and brake ==")
    start = [
        "--touch", "220:360:686:0", "--touch", "223:360:686:1",
        "--touch", "400:270:336:0", "--touch", "403:270:336:1",
        "--touch", "460:270:404:0", "--touch", "463:270:404:1",
    ]
    events = ["--frames", "760", *start, "--stick", "600:1:0"]
    events += [item for frame in range(600, 641) for item in ("--report", str(frame))]
    events += ["--stick", "640:0:0"]
    events += [item for frame in range(641, 661) for item in ("--report", str(frame))]
    reports = parse_reports(harness(assets, SCRATCH / "walk", events).stdout)

    def x(frame: int) -> float:
        return float(reports.get(frame, {}).get("x", "nan"))

    steady = x(639) - x(638)                  # steady walk, px per frame
    first = x(601) - x(600)                   # the first frame after the press
    check("the cube starts softly instead of jumping to full speed",
          0 < first < steady * 0.5, f"first frame {first:.2f} px, steady {steady:.2f} px")
    ramp = next((f for f in range(600, 640) if x(f) - x(f - 1) > steady * 0.9), None)
    check("the ramp takes about 0.08-0.15 s",
          ramp is not None and 4 <= ramp - 600 <= 10,
          f"full speed on frame {ramp} ({(ramp - 600) / 60 if ramp else -1:.3f} s)")

    moved = [f for f in range(641, 661) if x(f) > x(f - 1) + 0.01]
    stop = max(moved) if moved else 640
    check("after the release the cube stops in about 0.08 s",
          3 <= stop - 640 <= 8, f"stopped after {(stop - 640 + 1) / 60:.3f} s")
    check("after stopping the cube stands still",
          abs(x(660) - x(stop)) < 0.01, f"{x(stop):.2f} -> {x(660):.2f}")


def fake_request(path: str, body: bytes | None = None, method: str = "GET") -> bytes:
    url = f"http://127.0.0.1:{FAKE_PORT}{path}"
    request = urllib.request.Request(url, body, {"Content-Type": "application/json"}, method=method)
    with urllib.request.urlopen(request, timeout=5) as response:
        return response.read()


def set_chaos_flag(value: int) -> None:
    """Writes /HitboxChaos the way the admin panel does: as a signed-in admin."""
    login = json.dumps({"email": "dimasi4ek229@cb4.game", "password": "admin|cb4v1"}).encode()
    token = json.loads(fake_request("/v1/accounts:signInWithPassword?key=K", login, "POST"))["idToken"]
    fake_request(f"/HitboxChaos.json?auth={token}", str(value).encode(), "PUT")


def test_chaos(assets: Path) -> None:
    """The masterpiece hitboxes from the panel: the flag reaches the game and the
    punch stops landing.

    The flag lives in the database root, so it is set through the fake database
    over HTTP. The same punch is thrown twice, once with the flag off and once
    with it on: with the flag on the hitbox turns away from the enemy and runs,
    so the enemy keeps its health, while the enemy still hits the player back.
    """
    print("\n== 3d. Masterpiece hitboxes from the panel ==")
    fake = None
    own = not wait_port(FAKE_PORT, timeout=0.3)
    if own:
        fake = subprocess.Popen(
            [sys.executable, "test/fake_firebase.py", str(FAKE_PORT)],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    try:
        if not wait_port(FAKE_PORT):
            check("подставная база поднялась", False)
            return

        def swing(flag: int) -> dict[int, dict[str, str]]:
            set_chaos_flag(flag)
            result = harness(
                assets,
                SCRATCH / f"chaos{flag}",
                [
                    "--frames", "700",
                    "--touch", "220:360:686:0", "--touch", "223:360:686:1",
                    "--touch", "400:270:336:0", "--touch", "403:270:336:1",
                    "--touch", "460:270:404:0", "--touch", "463:270:404:1",
                    "--report", "590",
                    "--stick", "600:0:-1",
                    "--atk", "620:1", "--atk", "640:0",
                    "--report", "650",
                ],
            )
            return parse_reports(result.stdout)

        normal = swing(0)
        joke = swing(1)
        before, after = joke.get(590, {}), joke.get(650, {})
        check("флаг HitboxChaos дошёл до игры", after.get("chaos") == "1", str(after))
        check(
            "с флагом удар улетает мимо врага",
            float(after.get("foe", "nan")) == float(before.get("foe", "nan")),
            f"до удара {before.get('foe')}, после {after.get('foe')}",
        )
        check(
            "без флага тот же удар попадает",
            float(normal.get(650, {}).get("foe", "nan")) < float(normal.get(590, {}).get("foe", "nan")),
            f"до удара {normal.get(590, {}).get('foe')}, после {normal.get(650, {}).get('foe')}",
        )
        check(
            "шедевро-режим не отключает врага",
            float(after.get("hp", "nan")) < float(before.get("hp", "nan")),
            f"{before.get('hp')} -> {after.get('hp')}",
        )
        check("в шедевро-режиме нет ошибок", after.get("error") == "0", str(after))
    finally:
        try:
            set_chaos_flag(0)
        except OSError:
            pass
        if own and fake:
            fake.terminate()
            fake.wait(timeout=5)


def wait_port(port: int, timeout: float = 5.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return True
        except OSError:
            time.sleep(0.05)
    return False


def test_login(assets: Path) -> None:
    """Полный сетевой путь: экран входа -> форма -> Firebase Auth -> профиль.

    Подставная база поднимается на том же порту, что зашит в харнесс при
    сборке (FAKE_PORT), поэтому адрес совпадает с боевым кодом вызова.
    """
    print("\n== 4. Вход в аккаунт через настоящий HTTP ==")
    if not wait_port(FAKE_PORT, timeout=0.3):
        fake = subprocess.Popen(
            [sys.executable, "test/fake_firebase.py", str(FAKE_PORT)],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        own_fake = True
    else:
        fake = None
        own_fake = False
    try:
        if not wait_port(FAKE_PORT):
            check("подставная база поднялась", False)
            return
        result = harness(
            assets,
            SCRATCH / "login",
            [
                "--frames", "900",
                "--touch", "220:360:686:0", "--touch", "223:360:686:1",  # warning
                "--touch", "400:270:558:0", "--touch", "403:270:558:1",  # "Account"
                "--logininfo", "430",
                # The password goes in without the suffix: the game adds it,
                # as on a phone. The fake knows the admin password.
                "--login", "450:Dimasi4ek229:admin",
                "--report", "700",
                "--report", "890",
            ],
        )
        reports = parse_reports(result.stdout)
        info = next((ln for ln in result.stdout.splitlines() if ln.startswith("logininfo:")), "")
        field_line = next((ln for ln in result.stdout.splitlines() if "nick=(" in ln), "")
        check("экран входа найден (координаты полей известны)", "button=(" in field_line, field_line)
        frame = reports.get(700, {})
        check("вход выполнен (NET_LOGIN_OK)", frame.get("login") == "0" and frame.get("nick") == "Dimasi4ek229", str(frame))
        check("лобби после входа (экран переключился)", frame.get("state") == "0", str(frame))
        check(
            "профиль прочитан из базы (кубки админа 4200)",
            "cups=4200" in result.stderr or "cups=4200" in result.stdout,
            result.stderr[-1200:],
        )
        check("после входа не было ошибок", frame.get("error") == "0", str(frame))
        late = reports.get(890, {})
        check("сессия держится", late.get("login") == "0", str(late))
        if info:
            print("  " + info)
    finally:
        if own_fake and fake:
            fake.terminate()
            fake.wait(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser(description="Проверка ПК-версии")
    parser.add_argument("--no-build", action="store_true", help="не пересобирать харнесс")
    args = parser.parse_args()

    SCRATCH.mkdir(parents=True, exist_ok=True)
    fake_url = f"http://127.0.0.1:{FAKE_PORT}"  # порт фиксирован: см. test_login
    if not args.no_build or not BINARY.is_file():
        build(fake_url)
    assets = stage_assets()
    test_pixels(assets)
    test_menu_and_battle(assets)
    test_controls(assets)
    test_walk_feel(assets)
    test_chaos(assets)
    test_login(assets)

    print()
    if FAILED:
        print(f"ПРОВАЛЕНО ({len(FAILED)}): " + ", ".join(FAILED))
        return 1
    print("ПК-версия в порядке: игра собирается, меню нажимается, бой идёт, кадр рисуется.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

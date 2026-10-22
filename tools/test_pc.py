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


def fake_signs_in() -> bool:
    """The fake remembers every account it handed out, so a server left over
    from an older run can hold a password the game no longer sends. A sign-up
    with a throwaway address tells a live server from a stale one."""
    body = json.dumps({"email": f"probe{time.time()}@cb4.game", "password": "probe"}).encode()
    request = urllib.request.Request(
        f"http://127.0.0.1:{FAKE_PORT}/v1/accounts:signUp?key=K",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=5) as response:
            return response.status == 200
    except Exception:
        return False


class fake_server:
    """The fake database on FAKE_PORT: started here unless a test already has
    one running, and stopped again when the block ends."""

    def __enter__(self) -> "fake_server":
        self.process = None
        if wait_port(FAKE_PORT, timeout=0.3) and fake_signs_in():
            return self
        if wait_port(FAKE_PORT, timeout=0.1):
            raise RuntimeError(
                f"на порту {FAKE_PORT} висит чужая подставная база со старыми паролями - перезапусти "
                "test/fake_firebase.py"
            )
        self.process = subprocess.Popen(
            [sys.executable, "test/fake_firebase.py", str(FAKE_PORT)],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if not wait_port(FAKE_PORT):
            raise RuntimeError("подставная база не поднялась")
        return self

    def __exit__(self, *_exc) -> None:
        if self.process:
            self.process.terminate()
            self.process.wait(timeout=5)


def login_taps() -> list[str]:
    """Consent, the account screen and the admin sign-in, in one line."""
    return [
        "--touch", "220:360:686:0", "--touch", "223:360:686:1",
        "--touch", "400:270:558:0", "--touch", "403:270:558:1",
        "--login", "450:Dimasi4ek229:admin",
    ]


def test_multitouch(assets: Path) -> None:
    """Four fingers at once: stick, block, punch and a held ult.

    Every control keeps its own pointer, so a finger that holds the stick does
    not take the pads away from the others. The run walks a step with the
    stick, keeps a finger on the block, taps the punch under the guard (the
    rules ignore it, the guard has to stay) and charges the ult with a fourth
    finger, then drops the guard and lets the ball fly.
    """
    print("\n== 3e. Multi-touch: four fingers at once ==")
    with fake_server():
        result = harness(
            assets,
            SCRATCH / "multitouch",
            [
                "--frames", "1400",
                *login_taps(),
                "--class", "750:3",
                "--goto", "800:1",
                "--report", "890",
                # finger 0: the stick, walked one step and back to the middle
                "--touch", "900:130:810:0:0", "--touch", "910:130:745:2:0",
                "--touch", "930:130:810:2:0",
                # finger 1: guard up, with the stick finger still down
                "--touch", "940:282:726:0:1", "--report", "960",
                # finger 2: punch under the guard, guard must not fall
                "--touch", "970:408:822:0:2", "--touch", "995:408:822:1:2",
                "--report", "1000",
                # guard down, then the ult pad with the stick still held
                "--touch", "1020:282:726:1:1",
                "--touch", "1040:408:630:0:3", "--report", "1080",
                "--touch", "1100:408:630:1:3",
                "--report", "1110", "--report", "1220", "--report", "1360",
            ],
        )
        check("мультитач-бой без ошибок", result.returncode == 0, result.stderr[-1500:])
        reports = parse_reports(result.stdout)
        before, guarded = reports.get(890, {}), reports.get(960, {})
        punched, charged = reports.get(1000, {}), reports.get(1080, {})
        after = reports.get(1360, {})
        check(
            "палец на стике двигает кубика, пока второй держит блок",
            float(guarded.get("y", "999")) < float(before.get("y", "0")),
            f"было {before.get('y')}, стало {guarded.get('y')}",
        )
        check("второй палец держит блок", guarded.get("block") == "1", str(guarded))
        check("удар под блоком не сбивает блок", punched.get("block") == "1", str(punched))
        check("четвёртый палец зарядил мяч, пока стик занят", charged.get("charge") == "1", str(charged))
        check(
            "стик не сбит ультой",
            float(charged.get("y", "999")) < float(before.get("y", "0")),
            f"было {before.get('y')}, стало {charged.get('y')}",
        )
        check("мяч улетел и погас", after.get("ball") == "0", str(after))
        check(
            "мяч снял здоровье врагу",
            float(after.get("foe", "nan")) < float(before.get("foe", "nan")),
            f"враг {before.get('foe')} -> {after.get('foe')}",
        )


def test_ball(assets: Path) -> None:
    """Hold the ult pad for 0.6 s and the super turns into a bouncing ball.

    A quick tap has to fire the ordinary super, a real hold has to charge the
    ball, and the released ball has to hurt the enemy.
    """
    print("\n== 3f. The charged ball ult ==")
    with fake_server():
        def fight(name: str, events: list[str]) -> dict[int, dict[str, str]]:
            result = harness(
                assets,
                SCRATCH / name,
                ["--frames", "1200", *login_taps(), "--class", "750:3", "--goto", "800:1", "--report", "880", *events],
            )
            return parse_reports(result.stdout)

        tapped = fight("ball_tap", ["--touch", "900:408:630:0:7", "--touch", "920:408:630:1:7", "--report", "1010"])
        held = fight(
            "ball_hold",
            # 0.6 s at 60 fps is 36 frames: 20 frames is still a tap, 60 is a hold
            ["--touch", "900:408:630:0:7", "--report", "920", "--report", "960",
             "--touch", "970:408:630:1:7", "--report", "1000", "--report", "1100"],
        )
    tap, hold_short, hold_long = tapped.get(1010, {}), held.get(920, {}), held.get(960, {})
    after = held.get(1100, {})
    base = held.get(880, {})
    check("быстрое нажатие не превращается в мяч", tap.get("ball") == "0" and tap.get("charge") == "0", str(tap))
    check("до 0.6 с ульта не заряжена", hold_short.get("charge") == "0", str(hold_short))
    check("после 0.6 с удержания ульта стала мячом", hold_long.get("charge") == "1", str(hold_long))
    check(
        "мяч попал во врага и снял здоровье",
        float(after.get("foe", "nan")) < float(base.get("foe", "nan")),
        f"до {base.get('foe')}, после {after.get('foe')}",
    )
    check("мяч не остался на экране навсегда", after.get("ball") == "0", str(after))


def test_ult_mode_switch(assets: Path) -> None:
    """The little button above the ult of "буК": dispenser or ball.

    The class has two ults, so it gets its own switch instead of a hold. The
    run taps the switch, checks the pad now throws the ball, and taps the pad:
    the ball flies and the enemy loses health.
    """
    print("\n== 3g. The buK ult switch ==")
    with fake_server():
        result = harness(
            assets,
            SCRATCH / "ult_mode",
            [
                "--frames", "1200",
                *login_taps(),
                "--class", "750:3",
                "--goto", "800:1",
                "--report", "880",
                # the switch sits right above the ult pad
                "--touch", "900:408:533:0:5", "--touch", "912:408:533:1:5",
                "--report", "940",
                # the ball mode throws the ball on the touch itself
                "--touch", "960:408:630:0:6", "--touch", "972:408:630:1:6",
                "--report", "1000", "--report", "1120",
            ],
        )
        check("переключение ульты без ошибок", result.returncode == 0, result.stderr[-1500:])
        reports = parse_reports(result.stdout)
        before, after_tap = reports.get(880, {}), reports.get(940, {})
        hit = reports.get(1120, {})
        check("у буКа появилась своя кнопка режима", before.get("mode") == "0", str(before))
        check("кнопка переключает ульту на шарик", reports.get(1000, {}).get("mode") == "1", str(reports.get(1000, {})))
        check("в режиме шарика ульта пускает мяч", "ball away" in result.stderr, result.stderr[-800:])
        check(
            "мяч снял здоровье врагу",
            float(hit.get("foe", "nan")) < float(before.get("foe", "nan")),
            f"враг {before.get('foe')} -> {hit.get('foe')}",
        )
        check("в обычном режиме мяч не летит", before.get("ball") == "0", str(before))


def test_longer_fights(assets: Path) -> None:
    """Both fighters start a fight with enough health to use their abilities."""
    print("\n== 3h. Longer fights ==")
    with fake_server():
        result = harness(
            assets,
            SCRATCH / "long_fight",
            ["--frames", "900", *login_taps(), "--class", "750:3", "--goto", "800:1", "--report", "880"],
        )
        frame = parse_reports(result.stdout).get(880, {})
        hp = float(frame.get("hp", "0"))
        foe = float(frame.get("foe", "0"))
        check("у игрока больше здоровья", hp >= 50, f"hp={hp}")
        check("у врага тоже", foe >= 50 and foe == hp, f"hp={hp}, foe={foe}")


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
    parser.add_argument("--only", default="", help="прогнать только тесты, в названии которых есть это слово")
    args = parser.parse_args()

    SCRATCH.mkdir(parents=True, exist_ok=True)
    fake_url = f"http://127.0.0.1:{FAKE_PORT}"  # порт фиксирован: см. test_login
    if not args.no_build or not BINARY.is_file():
        build(fake_url)
    assets = stage_assets()
    tests = [
        test_pixels,
        test_menu_and_battle,
        test_controls,
        test_walk_feel,
        test_multitouch,
        test_ball,
        test_ult_mode_switch,
        test_longer_fights,
        test_chaos,
        test_login,
    ]
    for test in tests:
        if args.only and args.only not in test.__name__:
            continue
        test(assets)

    print()
    if FAILED:
        print(f"ПРОВАЛЕНО ({len(FAILED)}): " + ", ".join(FAILED))
        return 1
    print("ПК-версия в порядке: игра собирается, меню нажимается, бой идёт, кадр рисуется.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

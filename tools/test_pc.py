#!/usr/bin/env python3
"""Проверка ПК-версии Cubic Battle 4: сборка и прогон без окна.

Собирает Linux-харнесс (gcc) из тех же исходников, что и .exe, и гоняет
настоящую игру фиксированными кадрами: экран предупреждения -> лобби ->
«Играть» -> «Соло» -> бой, стик и удар через тот же слой, что и клавиатура на
ПК. Отдельным прогоном проверяется вход в аккаунт против подставной базы
test/fake_firebase.py по настоящему HTTP (это единственный способ увидеть
весь сетевой путь: WinHTTP в песочнице не запустить, а POSIX-транспорт в
харнессе повторяет его поведение).

Запуск: python3 tools/test_pc.py            (нужен gcc + g++)
        python3 tools/test_pc.py --no-build (уже собранный харнесс)
"""

from __future__ import annotations

import argparse
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "pc"
BINARY = BUILD / "cb4_headless"
SCRATCH = BUILD / "test_pc"
FAKE_PORT = 18779
FAILED: list[str] = []


def check(name: str, ok: bool, detail: str = "") -> None:
    print(f"  {'ok  ' if ok else 'FAIL'} {name}" + (f" — {detail}" if detail and not ok else ""))
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
    check("бой начался (ST_SOLO)", battle.get("state") == "1" and battle.get("battle") == "1", str(battle))
    check("игрок жив и двигается стиком", float(battle.get("y", "0") or 0) < 470, str(battle))
    check("удар дошёл до врага", float(battle.get("foe", "20")) < 20, str(battle))
    check("в бою не было ошибок JS-хуков", battle.get("error") == "0", str(battle))
    end = reports.get(699, {})
    check("враг отвечает (игрок получает урон)", float(end.get("hp", "20")) < 20, str(end))
    check("кадр боя сохранён", shot.is_file() and shot.stat().st_size > 100_000)
    if shot.is_file():
        width, height, pixels = read_bmp(shot)
        colors = bmp_colors(pixels, width, height, 5)
        check("кадр боя не пустой (много цветов)", len(colors) > 50, f"{len(colors)} цветов")


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
                "--touch", "220:360:686:0", "--touch", "223:360:686:1",  # предупреждение
                "--touch", "400:270:558:0", "--touch", "403:270:558:1",  # «Аккаунт»
                "--logininfo", "430",
                # Пароль вводится без хвоста: хвост игра добавляет сама, как
                # на телефоне. Фейк знает админский пароль «admin|cb4v1».
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
    test_login(assets)

    print()
    if FAILED:
        print(f"ПРОВАЛЕНО ({len(FAILED)}): " + ", ".join(FAILED))
        return 1
    print("ПК-версия в порядке: игра собирается, меню нажимается, бой идёт, кадр рисуется.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

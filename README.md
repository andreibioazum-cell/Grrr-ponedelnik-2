# Ponedelnik 2D Engine

Ponedelnik 2D is a native C/Vulkan base for small 2D games and interactive projects. Its example gameplay is written in **Pona**, a statically typed, Lua-style language. Pona is compiled ahead of time to ordinary C as part of the native build; the shipped runtime has **no interpreter and no bytecode VM**.

## Current scope

- Android 8+ (API 26), Vulkan 1.0, native `NativeActivity` host;
- Vulkan renderer and drawing primitives reused from the C engine;
- Pona AOT compiler with `global`, typed functions, `local`, `if`/`else`, `while`, numeric `for`, `break`, returns, arithmetic, comparisons, boolean operators, strings and colors;
- 2D primitives, text, PNG sprites, screen/time/random helpers, keyboard/touch input, numeric/string arrays and basic math helpers;
- incremental, non-moving mark-and-sweep GC with a bounded per-frame work budget, explicit roots and write barriers;
- sample project in `examples/pond2d/main.pona`.

This is an early engine foundation, not a feature-complete editor or general-purpose cross-platform release. The initial host is Android/Vulkan. Scripts are type-checked and compiled during the build, then linked into the native app.

## Pona quick start

```pona
@export global speed: f32 = 220.0
global x: f32 = 100.0

function update(dt: f32)
    if key_down("right") or key_down("d") then
        x = x + speed * dt
    end
end

function draw()
    clear(#101827)
    circle(x, 120.0, 24.0, #55D8C2)
    text("Hello, Pona", 24.0, 32.0, #FFFFFF)
end
```

Pona blocks close with `end`. Variables use the types `f32`, `i32`, `bool`, `color`, `str` and `array`; `@export` marks a global for native tooling. `draw()` and `update([dt: f32])` are optional lifecycle hooks. `init()` and `stop()` may also be provided.

Supported drawing calls include `clear`, `rect`, `roundrect`, `circle`, `line`, `sprite`, `text` and `text_scaled`. Input helpers include `key_down`, `key_pressed`, `key_released`, `pointer_x`, `pointer_y`, `pointer_down`, `pointer_pressed` and `pointer_released`. Math/time helpers include `sin`, `cos`, `sqrt`, `abs`, `min`, `max`, `clamp`, `lerp`, `time`, `random`, `screen_width` and `screen_height`. Arrays use `array_new`, `array_push_num`, `array_push_str`, `array_get_num`, `array_get_str`, `array_len` and `array_clear`.

Colors are `#RRGGBB` (opaque) or `#AARRGGBB`. Screen coordinates are in pixels. Sprite paths refer to staged APK assets, for example `sprite("textures/hero.png", x, y, 1.0)`.

## Build and test

Run compiler, generated-C, GC, input, Android-host syntax and asset-staging tests on a host with Python 3 and Clang or GCC:

```sh
python3 tools/test_pona.py
python3 tools/test_stage_assets.py
```

Check or generate native C directly:

```sh
python3 tools/ponac.py examples/pond2d/main.pona --check
python3 tools/ponac.py examples/pond2d/main.pona -o /tmp/pona_script.c
```

Build the Android native library with the NDK (example for arm64):

```sh
cmake -S . -B build/android-arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/android-arm64 --parallel
```

Choose a different Pona entry script with `-DPONA_SOURCE=/path/to/project/main.pona`. Stage the project's asset directory with `python3 stage_assets.py assets staging/assets`; relative asset paths are preserved. The GitHub Actions workflow builds and packages a signed **debug** APK for `arm64-v8a`, `armeabi-v7a` and `x86_64`; its signing key is temporary and the APK is for development/testing, not store release.

## Runtime design notes

- Pona source is parsed and statically checked at build time, emitted as C, and compiled by Clang/NDK. No script parsing, bytecode decoding or interpretation occurs in the app.
- The GC is incremental and non-moving. `pond_app_end_frame()` advances it by at most 64 units of work, so the amount of collector work per frame is bounded. Globals containing managed objects and interned source literals are roots; array writes and root writes use barriers.
- The example is intentionally small and serves as an executable reference project. The compiler and built-in API are still evolving; see `tools/ponac.py` and `src/engine2d/api.h` for the current surface.

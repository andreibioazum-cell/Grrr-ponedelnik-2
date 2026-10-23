# Cubic Battle 4

<p align="center">
  <img src="assets/icons/ic_launcher_xxh.png" alt="Cubic Battle 4" width="128" height="128">
</p>

## Русский

Cubic Battle 4 — нативная 3D-песочница в стиле Minecraft на чистом C99 и Vulkan для Android (8.0 – 15) с единой облачной системой аккаунтов, скинов, достижений, промокодов, подарков, комнат и живой мультиплеерной синхронизацией блоков через Firebase Realtime Database (`cubicbattleserver-19ae2-default-rtdb.firebaseio.com`).

### Возможности 3D-движка и игры

- **Настоящий 3D-воксельный мир (`src/game/mc/`)**: процедурная генерация биомов (равнины, леса с дубами и берёзами, снежная тундра, пустыня с кактусами, пляжи, озёра, подземные пещеры, руды и постройки), 3D-рендеринг через Vulkan (`ds_gfx_quad`), плавное вершинное освещение (Ambient Occlusion), динамическое свечение факелов и светокамня, атмосферный туман и облака.
- **Ломание и установка блоков**: 3D DDA-рейкастинг, 10 стадий трещин при добыче, горячая панель на 9 слотов и полный инвентарь блоков, физика AABB со скольжением по стенам, плавание в воде, творческий полёт, камера от 1-го и 3-го лица, живой динамит (TNT) с воронками взрыва и 3D-сущности персонажей с анимацией и всеми скинами.
- **Онлайн-мультиплеер и сохранение мира**: ломание и установка любых блоков мгновенно синхронизируются между всеми игроками комнаты и сохраняются в комнате Firebase (`/rooms/<room>/chat/b_<x>_<y>_<z>.json`), а одиночный мир сохраняется на устройстве (`mc_solo.dat`).
- **Полное сохранение экосистемы Cubic Battle 4**: оригинальное меню, регистрация и вход через Firebase Auth, облачный прогресс, кубки, конфеты, боевой пропуск, достижения, ежедневные награды, промокоды, подарки от разработчика, баннеры событий и веб-панель администратора (`index.html`).

### Сборка Android APK

Сборка подписанного APK (`arm64-v8a`, `armeabi-v7a`, `x86_64`) выполняется автоматически в GitHub Actions (`.github/workflows/main.yml`) с помощью Android SDK, Android NDK `r25c`, CMake/Ninja и Vulkan:

- Игровой код и 3D-движок: `src/game/` (`game.c`, `mc/*.inc`)
- Рантайм, Vulkan-рендерер, звук и сеть: `src/engine/`
- Слой платформы Android: `src/platform/android/`, `platform/android/`
- Проверки и модульные тесты: `tools/`
- Веб-панель администратора и её тесты: `index.html`, `firebase.rules.json`, `test/`

---

## English

Cubic Battle 4 is a pure-C99 native Vulkan 3D voxel sandbox game for Android (8.0 – 15) powered by the shared Firebase Auth & Realtime Database backend (`cubicbattleserver-19ae2-default-rtdb.firebaseio.com`).

### Key Features

- **Real 3D Voxel Engine (`src/game/mc/`)**: procedural multi-biome terrain, trees, caves, ores, water, smooth per-vertex ambient occlusion, torch/glowstone point lighting, distance fog, 3D animated character models with all character skins, live TNT craters, 1st/3rd-person camera, creative flight, and multi-touch controls.
- **Block Breaking & Placing + Live Multiplayer Sync**: 3D DDA raycasting, 10-stage mining cracks, 9-slot hotbar + block inventory, persistent solo world (`mc_solo.dat`), and real-time persistent multiplayer block sync across rooms via Firebase RTDB.
- **Full Account & Admin Ecosystem**: original menu & UI, Firebase Auth accounts, cloud progress, trophies, candies, Battle Pass, achievements, promo codes, developer gifts, live events, and the web admin panel (`index.html`).

## RU

Полная версия Cubic Battle 4 для Android. Игра написана на C, и немного на Java.

- Минимум: Android 8
- Цель: Android 15
- Требуется: Vulkan 1.0

## Веб-панель админа

Кроме APK в репозитории есть веб-версия — [index.html](index.html): панель
админа для базы Firebase (вход по нику и паролю от игры, снимок базы, поиск и
удаление мусора, ивенты 0..4, баннер, флаги магазина, баны, чистка чата и
слотов, резервная копия). Сборка не нужна: открой файл в браузере.

Потренироваться без боевой базы можно так:

```
python3 test/fake_firebase.py 8080
```

затем открой <http://localhost:8080/>, нажми «Тестовый режим» и войди как
`Dimasi4ek229` с паролем `admin`. Подробности, список починенных поломок и
проверки — в [docs/admin-panel.md](docs/admin-panel.md).

## Правовая информация

Возрастная категория 12+. Пользовательское соглашение, политика обработки
персональных данных и согласие на обработку лежат в [docs/legal](docs/legal);
там же список того, что владелец должен сделать вне кода (уведомление
Роскомнадзора, локализация данных и т. д.).

## Лицензия

Copyright (C) 2026 Дима Густенёв

Все права защищены. Код и материалы проекта нельзя копировать, использовать,
изменять или распространять без письменного разрешения правообладателя.
Полные условия - в файле [LICENSE](LICENSE).

SPDX-License-Identifier: LicenseRef-Proprietary

## EN

Full version of Cubic Battle 4 for Android. The game is written in C, with a bit of Java.

- Minimum: Android 8
- Target: Android 15
- Requires: Vulkan 1.0

## Web admin panel

Besides the APK the repository contains a web version - [index.html](index.html):
the Firebase admin panel (sign in with the game nick and password, database
snapshot, junk search and cleanup, events 0..4, banner, shop flags, bans, chat
and slot cleanup, backup copy). No build step: open the file in a browser.

To practise without touching the live database run

```
python3 test/fake_firebase.py 8080
```

then open <http://localhost:8080/>, press the test-mode button and sign in as
`Dimasi4ek229` with the password `admin`. Details in
[docs/admin-panel.md](docs/admin-panel.md).

## Legal

Age rating 12+. The Terms of Use, the personal data policy and the consent
form (in Russian) are in [docs/legal](docs/legal).

## License

Copyright (C) 2026 Дима Густенёв

All rights reserved. The code and materials of this project may not be
copied, used, modified or distributed without the copyright holder's written
permission. Full terms in the [LICENSE](LICENSE) file.

SPDX-License-Identifier: LicenseRef-Proprietary

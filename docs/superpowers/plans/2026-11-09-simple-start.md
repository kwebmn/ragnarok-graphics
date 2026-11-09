# Простой старт и настройки — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (владелец: «все в инлайн режиме», без
> агентов). Шаги — чекбоксы `- [ ]`. Каждый этап — своя бета `3.0.17-beta.N` через `$S/ship-beta.sh`.

**Goal:** игрок доходит от установки до игры за два клика; три пресета вместо сотен галочек; быстрые настройки в
игре; видно новое; откат на прошлую версию; проверка видеокарты без игры.

**Architecture:** решения без Windows — в `src/core/...` с тестами в `tests/` (как всё в проекте); окна программы —
`src/app/page_*.cpp`, в игре — `src/win/console.cpp`. Пресеты — таблица «вариант → ключи `uaro-graphics.ini`» в ядре;
программа и консоль показывают одно и то же. Откат — история версий в `release/update.ini` (tools/release_ini.py) и
тот же путь обновления, что уже качает архив по коммиту.

**Tech Stack:** C++20, Win32/GDI (программа), D3D12/11 (драйвер), Python (tools), свой тест-раннер `tests/test.hpp`.

**Spec:** Claude Doc «Концепция: простой старт и настройки uaRO Graphics»,
https://claude.ai/artifact/QCiLb6wLxWhpdsJrSwCNeN (2026-11-09).

## Global Constraints

- Ничего не удаляем из `uaro-graphics.ini`; старая программа и старый драйвер читают новый файл без ошибок.
- Новое, что меняет картинку, выходит выключенным; пресет не включает новые визуальные вещи сам.
- Каждая новая строка — в схеме (`src/core/settings/schema.cpp`) и видна в программе; названия — из колонки uk.
- Строки в `src/core/settings/strings.cpp` — в порядке enum (`static_assert` на `kTexts`), три языка.
- FXC: не использовать в шейдерах зарезервированные слова (pass, texture, point …); гейт `$S/gate.sh HEAD`.
- Старые программы читают `update.ini` формата 2: новые секции им не мешают (parseManifest пропускает незнакомые).
- Бета после релиза 3.0.16 — `3.0.17-beta.N`.

## Review Focus

1. Откат на версию, где нет ключей нового `uaro-graphics.ini` — старый драйвер должен их пропустить (уже так), не
   падать. Тест: `parseManifest` с секцией `[history]` в старом формате кода — тест в Task 1.
2. История в `update.ini`, где старый `[package]` той же версии (повторный выпуск) — не дублировать строку. Task 1.
3. Пресет при уже изменённых руками ключах — показывает «змінено N», не затирает без спроса. Task 3.
4. Пресет «Інтерфейс: Класичний» при `interface.minimap = 0` и отдельных частях, включённых вручную, — считается
   «змінено», а не «Новий». Task 3.
5. Сбой запуска после обновления считается только для текущей версии; после отката счётчик не тянется. Task 2.

---

## Этап 1 — откат (бета 1)

### Task 1: история версий в update.ini

**Files:**
- Modify: `tools/release_ini.py` (функция `write`, self-test)
- Modify: `src/core/update_check.hpp/.cpp` (`Manifest::history`, `parseManifest`)
- Test: `tests/test_update_check.cpp`

**Interfaces:**
- Produces: секция `[history]` в `update.ini`: строки `vN = <version> <ref> <size> <sha256>`, новейшая первая, не
  больше 5, без текущей версии. `struct Manifest { …; std::vector<Archive> history; }` — Archive с `file =
  "release/uaro-graphics.zip"`.

- [ ] Тест `update_manifest_history`: `parseManifest` текста с `[history]` из двух строк → `history.size()==2`,
  `history[0].version=="3.0.15"`, ref/size/sha256 как в тексте; строка с битым sha → пропущена, не ошибка.
- [ ] Запустить — FAIL (нет поля).
- [ ] Реализовать: поле и разбор; неизвестные строки в `[history]` пропускать.
- [ ] `release_ini.py write`: прежний `[package]` из старого `update.ini` (если версия другая) — первой строкой
  истории, затем старая история, всё без повторов версии и без новой версии, обрезать до 5. Self-test:
  два последовательных вызова → история из одной строки со старой версией; повтор той же версии → истории не прибавилось.
- [ ] Гейт, коммит.

### Task 2: откат из программы

**Files:**
- Modify: `src/app/update_run.hpp/.cpp` (установка заданного архива), `src/app/page_service.cpp` (список версий),
  `src/app/page_play.cpp` (строка «← Повернути X» на «Гра»), `src/core/settings/strings.*`
- Create: `src/core/start_failures.hpp/.cpp` — счётчик неудачных запусков по версии
- Test: `tests/test_start_failures.cpp`

**Interfaces:**
- Produces: `startfail::note(std::string& text, const std::string& version, bool ok)` → новый текст файла
  `uaro-graphics\start-failures.txt` (`<version> <count>`; ok или другая версия — сброс);
  `bool startfail::offerRollback(const std::string& text, const std::string& version)` — count ≥ 2.
- Consumes: `Manifest::history`, `installer::rollBack`, `installer::previousVersion`.

- [ ] Тесты: два неудачных запуска одной версии → offer; удачный сбрасывает; другая версия сбрасывает.
- [ ] Реализовать ядро; программа отмечает исход запуска там, где уже пишется «первая игра» (launcher outcome).
- [ ] «Гра»: при `canRollBack` — ссылка «← Повернути {версія}»; при offerRollback — заметная плашка «Гра двічі не
  запустилася після оновлення — повернути {версія}?».
- [ ] «Сервіс»: список `history` своего канала (до 5) с кнопкой «Встановити» → тот же путь, что обновление, с
  архивом из истории; `skip` = текущая версия (уже есть для отката).
- [ ] Гейт, бета `3.0.17-beta.1`.

## Этап 2 — три пресета (беты 2–3)

### Task 3: ядро пресетов

**Files:** Create `src/core/settings/presets.hpp/.cpp`; Test `tests/test_presets.cpp`

**Interfaces:**
- `enum class Picture { Auto, Game, Better, Remaster }`, `enum class Ui { New, NewScreen, Classic }`,
  `enum class Textures { Hd, HdLight, Original }`.
- `std::vector<std::pair<std::string,std::string>> keysOf(Picture|Ui|Textures)` — ключи `section.key` → токен.
- `struct Match { int variant; int changed; }` + `Match matchPicture(const Config&)` (и для Ui, Textures):
  ближайший вариант и сколько его ключей отличаются.
- Значения: Picture — `effects.preset` auto/off/medium/strong, `effects.tier` classic/classic/classic/remaster;
  Ui — `interface.minimap` 1/1/0, NewScreen = minimap 1 + все `…_window`/окна из `interfaceSections()` folded = 0;
  Textures — `ai.enabled` 1/1/0, HdLight: `ai.max_size` 256, `ai.small_pictures` 0.

- [ ] Тесты: keysOf не пересекаются между тремя выборами; дефолтный Config → Picture::Auto changed 0, Ui::New,
  Textures по дефолту; один ключ руками → changed 1; Classic при одной включённой части → changed ≥ 1 (Review 4).
- [ ] Реализовать, гейт, коммит.

### Task 4: пресеты в программе и в консоли

- «Головна» → «Огляд»: три карточки (вариант, «змінено N», ‹ ›); смена — вопрос «лишити мої зміни / скинути»
  при changed > 0. Консоль: строка-выбор для каждого из трёх сверху списка.
- Бета.

## Этап 3 — экран приветствия (бета)

### Task 5: «Гра» становится экраном приветствия

- `page_play.cpp`: большая «Грати»/«Оновити і грати», строка версии, «Ще вікно гри» (уже есть в multi-window),
  «Повідомити про помилку» (reports_window), блок «Як виглядає гра» (три пресета, «змінити ›» → «Огляд»), новости
  (лента сайта как у login news), низ: «⚙ Налаштування», «Довідка», «Мої звернення».
- Программа открывается на «Гра» во всех режимах (уже первая в списке).

## Этап 4 — быстрые настройки в игре (беты)

### Task 6: вкладки и плитки Alt+D

- `src/win/console.cpp`: вкладки Швидко/Картинка/Інтерфейс/Звук/Усе; «Швидко» — пресеты ‹ › и плитки; ★ закрепить
  строку (`[utility] pinned = key key …`, новый ключ в схеме — Сервіс); список «Усе» — нынешний.
- Ядро раскладки плиток — `src/core/console_tiles.hpp/.cpp` с тестами (сетка 4 в ряд, фокус клавишами/падом).

## Этап 5 — глубокие настройки (беты)

### Task 7: разделы со сворачиванием

- `navigation.cpp`: семь разделов (Картинка, Інтерфейс, Текстури HD, Звук і сповіщення, Керування і геймпад, Гра й
  чат, Сервіс); `levelOf` решает «видно сразу / в «Ще N»», режимы убираются из интерфейса (ключ `[utility] mode`
  остаётся, читается), «Розробник» — переключатель в «Сервіс».
- Фильтр «Лише змінені» — от пресета/рекомендованного (Model::recommended уже есть).

## Этап 6 — «Нові можливості» (бета)

### Task 8: каталог нового

- Ядро: версия появления строки уже есть (known-rows.txt / ★). Добавить `std::string whatItGives` в схему для
  главных строк (текст «що це дає») — strings, три языка.
- Программа: плитка «★ Нові можливості (N)» на «Гра», список с фильтрами; консоль: вкладка «Нове», «Спробувати».

## Этап 7 — проверка видеокарты без игры (несколько бет)

### Task 9: быстрая оценка

- Сайт отдаёт таблицу «карта → пресет» из `graphics_stat_segments` (только чтение БД; выгрузка — по разрешению
  владельца). Программа ставит предварительный пресет по названию карты/видеопамяти.

### Task 10: запись и прогон сцен

- Запись: драйвер пишет поток кадров моста (как идёт в uaro-render64) в файл (`[debug] record_scene`), 10–15 с.
- Прогон: uaro-render64 в окне программы читает файл, текстуры — из архивов игры игрока; наборы как в
  graphics_test; цена эффектов — из профиля (уже есть). Итог — карточка с пресетом и списком ✔/✖.
- Сначала измерить размер записи сцены 10 с — от этого зависит, идём ли дальше (вопрос 7 концепции).

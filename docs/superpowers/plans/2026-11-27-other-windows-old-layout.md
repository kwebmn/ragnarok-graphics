# Остальные окна в старой раскладке — план (часть 3 из 4)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** остальные наши окна — по раскладке старой игры с сохранёнными новшествами: все знаки из шрифта, узкие
«Навички», две колонки «Характеристик», выбор у NPC отдельным списком, «Запросити» в «Групі»; остальное — проверить.

**Architecture:** окна — uaUI (RmlUi): разметка RML в `src/uikit/*_live.cpp` и `src/win/*.cpp`, раскладка и слова — в
`src/core/*` (тесты на хосте), Windows-часть — `src/win/*`. Новые пакеты — `core/item_packets` + `win/item_net`, под
пробой тестеров.

**Tech Stack:** C++17, RmlUi, mingw (build-win, build-win64), host tests `build-host/uagfx_tests`.

**Spec:** `docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md` (одобрена 27.11) — «Остальной интерфейс»,
«Обязательно для всех окон», «Порядок» п. 5.

## Global Constraints

- Сервер игры не трогаем; номера и длины пакетов — из `/root/uAthena/db/packet_db.txt`, раздел `packet_ver: 22`
  (наш клиент 2007-05-07aSakexe), и обработчик должен быть в `src/map/clif.c` (0x02c4 описан, но не обрабатывается).
- Разметка RML руками, проверка парсером; ширины в % — border-box; без SetProperty каждый кадр.
- Тексты на трёх языках в `core/settings/strings.cpp`; «» — только вокруг названий интерфейса (gate).
- Строки ≤ 120; `dist/*.ini` — CRLF; сборки `nice -n 19 ionice -c3`, `-j4`; беты `gship.sh 3.0.24-beta.N`.
- Каждая бета — карточки в `core/checks_now.cpp`; новые пакеты — проба тестеров (выкл. по умолчанию).
- Шрифт окон — IBM Plex Sans (`core/plex_widths.inc`, `itemui::fontHas`); знаки, которых в нём нет, — рисованные
  значки или слова, не буквы (спецификация: «стрелки, звёзды и треугольники — рисованные значки»).

## Review Focus

1. Знак, которого нет в Plex, в строке, собранной кодом (не в разметке и не в strings.cpp) — тест его не видит;
   ожидание: и такие строки без «квадратиков». Тест: Task 1 проверяет и слова, которые окна пишут сами (★ ♛ ✕ ⚠ в
   `src/win/*.cpp` строковых литералах UTF-8).
2. Узкие «Навички» на маленьком экране и с длинным названием навыка — название обрезается многоточием, строка не
   ломается. Тест: Task 2 `skill_window_long_name_fits`.
3. Выбор NPC, когда справа нет места (окно разговора у правого края) — список встаёт слева или снизу, не за экраном.
   Тест: Task 4 `npc_menu_place_left_or_below`.
4. «Запросити» другу, который уже в нашей группе или не в сети, — кнопки нет. Тест: Task 5
   `party_invite_only_online_friends_not_members`.
5. Характеристики с бонусом 0 и отрицательным бонусом — «9 + 0» не пишем, «9 − 2» пишем. Тест: Task 3
   `stat_window_base_and_bonus_words`.

---

### Task 1: Все знаки окон — из шрифта

**Files:** `tests/test_markup_letters.cpp` (новый); все окна, где тест найдёт чужой знак: `src/win/*.cpp`,
`src/uikit/*.cpp`, `src/core/ui_theme_css.cpp`, `src/core/settings/strings.cpp`.

**Interfaces:**
- Consumes: `itemui::fontHas(const std::string&)` (часть 2).
- Produces: общее правило нарисованного крестика `.lw .x .cxa, .lw .x .cxb` (две повёрнутые полосы, как в окнах
  вещей) — разметка `<span class="x ..."><span class="cxa"></span><span class="cxb"></span></span>`.

- [ ] **Step 1:** тесты (читают исходники через `UAGFX_SOURCE_DIR`):
  `markup_letters_are_the_fonts` — каждый блок `R"RML(`…`)RML"` в `src/win/*.cpp`, `src/uikit/*.cpp` после раскрытия
  `&#xN;`, `&#N;`, `&amp; &lt; &gt; &quot; &nbsp;` проходит `fontHas`; в сообщении — файл и знак.
  `texts_letters_are_the_fonts` — каждая строка-литерал `core/settings/strings.cpp` проходит `fontHas`.
  `code_words_letters_are_the_fonts` — UTF-8 последовательности `\xE2\x..\x..` в строковых литералах
  `src/win/*.cpp` и `src/uikit/*.cpp` (знаки, которые окна пишут сами) проходят `fontHas`.
- [ ] **Step 2:** FAIL — сейчас: ✕ (24 места), ★, ↺, ⚠ в разметке; ▾ ↻ ⚠ ★ ≈ ▸ ● ⚔ ☆ ✕ ʼ в текстах.
- [ ] **Step 3:** замены: ✕ в разметке → нарисованный крестик; ★ в разметке → картинка `uiicons::Icon::Star`
  (как в сетке вещей); ↺ → слово окна; ⚠ в разметке → без знака (строка и так красная). В текстах: ▾ ▸ → ›,
  ↻ → *, ⚠ → !, ★ ☆ → слово «зірочка» / «star» / «звёздочка», ≈ → ~, ● → ·, ⚔ → слово, ✕ → ×, ʼ → ’ (если нет в
  Plex — ').
- [ ] **Step 4:** PASS, сборки host/win/win64.
- [ ] **Step 5:** commit; карточка «Знаки в окнах — з шрифту».

### Task 2: «Навички» — узкий список

**Files:** `src/core/skill_window.{hpp,cpp}`, `src/uikit/skills_live.cpp`, `src/win/skill_window.cpp`,
`src/win/info_card.cpp` (картка навыка), `src/core/ui_theme_css.cpp`; Test: `tests/test_skill_window.cpp`.

**Interfaces:**
- Produces: в виде «список» — без боковой панели (`L.side.w == 0`), ширина окна 420 при масштабе 1; строка:
  значок, название и под ним «Рівень N · SP M» (SP — уровня, который выбран стрелками), ‹ › уровень применения,
  «N/M», «+»; точки уровней в списке не рисуются. Вид «Дерево» — как был (широкий, со своей панелью).
- `std::vector<std::string> skillwin::cardFacts(const Show&, const Texts&, int skill)` — строки боковой панели
  («Рівень {1} з {2}», «Потрібно…», «Відкриває…») для картки навыка по ПКМ.

- [ ] **Step 1:** tests `skill_window_list_is_narrow` (ширина ≤ 420, `side.w == 0`, у дерева — как было),
  `skill_window_long_name_fits` (название шире строки — `name` с «…» в `words()`), `skill_window_card_facts`
  (навык с требованиями и тем, что открывает: строки в порядке «Рівень», «Потрібно», «Відкриває»).
- [ ] **Step 2–4:** FAIL → раскладка и слова → PASS; картка навыка (`win/info_card`) добавляет `cardFacts` после
  описания; сборки.
- [ ] **Step 5:** commit; карточка «Навички: вузький список, опис — ПКМ».

### Task 3: «Характеристики» — две колонки

**Files:** `src/core/stat_window.{hpp,cpp}`, `src/uikit/stats_live.cpp`, `src/win/stat_window.cpp`; Test:
`tests/test_stat_window.cpp`.

**Interfaces:**
- Produces: окно 520 при масштабе 1; левая колонка — STR…LUK: название, «база + бонус», «+» и цена; правая — ATK,
  MATK, HIT, CRIT, DEF, MDEF, FLEE, ASPD; под колонками — «Очки: N» и гильдия; строка «що дасть +1» под левой
  колонкой при наведении — как была; вкладка формул — как была.
- `std::string statwin::baseAndBonus(int base, int bonus)` — «9», «9 + 3», «9 − 2» (минус U+2212).

- [ ] **Step 1:** tests `stat_window_two_columns` (строки STR…LUK в левой половине, ATK…ASPD — в правой, ширина
  ≤ 520), `stat_window_base_and_bonus_words` («9», «9 + 3», «9 − 2»).
- [ ] **Step 2–4:** FAIL → PASS, сборки.
- [ ] **Step 5:** commit; карточка «Характеристики: дві колонки»; бета Task 2–3.

### Task 4: Разговор с NPC — выбор отдельным списком

**Files:** `src/core/npc_menu.{hpp,cpp}` (новое), `src/win/npc_talk.cpp`, `src/core/ui_theme_css.cpp`; Test:
`tests/test_npc_menu.cpp`.

**Interfaces:**
- Produces: `panel::Box npcmenu::place(const panel::Box& talk, float w, float h, int screenW, int screenH)` —
  справа от окна разговора с зазором 8; нет места — слева; нет и там — под ним; всегда в экране.
  `int npcmenu::byKey(int key, int count)` — 1–9 → номер строки (0..count-1), иначе −1.
- Выбор — свой живой документ «npc menu» (группа Windows): строка — номер и слова варианта; наведение подсвечивает,
  колесо листает, клик выбирает; ↑ ↓, 1–9, Enter, Esc (Esc — как «Закрити» у игры); пад — как сейчас (хрестовина,
  A, B). Варианты из окна разговора убираются; ответ — тем же путём, что сейчас (кнопки окна игры).

- [ ] **Step 1:** tests `npc_menu_place_right` (место справа есть), `npc_menu_place_left_or_below` (у правого края —
  слева; узкий экран — снизу; всегда внутри экрана), `npc_menu_keys` (1 → 0, 5 → 4, 0 и 10 → −1, 7 при count 5 → −1).
- [ ] **Step 2–4:** FAIL → PASS, сборки.
- [ ] **Step 5:** commit; карточка «NPC: вибір окремим списком».

### Task 5: «Група» — «Запросити» другу

**Files:** `src/core/friend_ids.{hpp,cpp}` (новое), `src/core/item_packets`, `src/win/item_net`,
`src/win/social_card.cpp`, `src/uikit/social_live.cpp`, config (`[interface] party_invite`, проба); Test:
`tests/test_friend_ids.cpp`.

**Interfaces:**
- Produces: `class friendids::Ids { void feed(uint16_t id, const uint8_t* p, size_t size); uint32_t accountOf(const
  std::string& name) const; }` — из 0x0201 (список: len, по 32 байта {account u32, char u32, name 24}) и 0x0209
  (друг добавлен: {result u16 @2, account u32 @4, char u32 @8, name 24 @12}); `itempackets::partyInvite(uint32_t
  account)` — 0x00fc, 6 байт; `bool friendids::canInvite(bool inParty, bool online, bool member)`.
- Кнопка «Запросити» на строке друга в сети, не из нашей группы, когда мы в группе; ответ сервера (0x00fd) показывает
  игра.

- [ ] **Step 1:** tests `friend_ids_from_the_list_and_an_add`, `party_invite_packet` (fc 00 + account LE),
  `party_invite_only_online_friends_not_members`.
- [ ] **Step 2–4:** FAIL → PASS, сборки.
- [ ] **Step 5:** commit; карточка «Група: запросити друга»; бета Task 4–5.

### Task 6: Окна, уже стоящие по старой раскладке

**Files:** `src/core/checks_now.cpp`; при находках — файлы окна.

- [ ] **Step 1:** сверка с концептом: «Гільдія» (вкладки сверху: інформація, учасники, посади, навички, вигнання,
  оголошення; герб, онлайн, опыт полоской), «Вибір персонажа» (карточки, «Увійти», «Створити», «Видалити», «Назад»,
  карусель), вход и создание персонажа (обе темы), основной экран (блок персонажа, миникарта, F1–F9, чат на старых
  местах; группа Hud — обе темы). Что расходится — исправить в этом шаге с тестом раскладки окна.
- [ ] **Step 2:** карточки «Перевіряємо зараз» для каждого из этих окон (обе темы).
- [ ] **Step 3:** commit; бета; итоговая проверка всей части.

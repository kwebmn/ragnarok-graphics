# Темы интерфейса (часть 1 из 4) — план работ

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** тёмная и светлая тема для всего нашего интерфейса в игре, тема по группам окон и свой цвет акцента; выбор в
Alt+O, в программе настроек и в лаунчере; «скляні вікна» и старые темы classic/contrast убраны.

**Architecture:** интерфейс в игре — uaUI поверх RmlUi: окна — RML, стили — RCSS. Тема — функция над цветами
(`core/ui_theme`): каждый цвет RCSS и каждый цвет, который окно задаёт из C++, проходит через тему своей группы. Каждая
группа получает свой файл стиля `uaui:theme-<группа>.rcss` (тот же встроенный стиль, перекрашенный темой группы);
документ подключает файл своей группы по своему имени. Лаунчер и программа настроек — не RmlUi (свой рисунок и окно
Windows): в них только переключатели, пишущие те же ключи `uaro-graphics.ini`.

**Tech Stack:** C++20, RmlUi (uaUI), Win32 (программа настроек, лаунчер), тесты `build-host/uagfx_tests`.

**Spec:** `docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md` (одобрен 27.11; разделы «Темы»,
«Решено по вопросам проверки», «Обязательно для всех окон»).

**Части работы по спецификации** (каждая — свой план, следующий пишется после предыдущего): 1 — темы (этот план);
2 — окна вещей в старой раскладке (общие детали + окна по одному, новые пакеты лавки, обмена, вещей); 3 — остальной
интерфейс в старой раскладке; 4 — цены с рынка сайта.

## Global Constraints

- Сервер игры не меняем. Правки сайта — ночью, конфиги сервера — только через ISPConfig (эта часть сайт не трогает).
- Строки ≤ 120 символов; `dist/*.ini` — CRLF.
- Тексты — в `src/core/settings/strings.cpp` на трёх языках (en / uk / ru); названия в отчёте владельцу — из столбца uk.
- Окна — RML руками (uaUI), разметка проверяется XML-парсером в тестах; без `SetProperty` каждый кадр.
- Сборки: `nice -n 19 ionice -c3`, не больше `-j4`; host (`uagfx_tests`), `build-win`, `build-win64`.
- Шейдеры (если тронуты) проверяются `bash /root/uagfx-tools/work/allshaders.sh` (`dxc -HV 2018`).
- Беты: `bash /root/uagfx-tools/work/gship.sh 3.0.24-beta.N "…"`, заметки `notes/<V>.{uk,en,ru}.md`; выпуск только по
  слову владельца; в каждой бете — карточка в «Перевіряємо зараз» (`src/core/checks_now.cpp`).
- Работа inline, без агентов.
- Значения из спецификации: тёмная тема по умолчанию; светлая — фон окна #f6f8fc, ячейки белые, линии #cbd3e2, текст
  #181c26, акцент #2f5fd0; акценты: золотой #e6cb8a (тёмной по умолчанию), синий #2f5fd0 (светлой по умолчанию),
  зелёный #2e8b57, червоний #c0392b, «свій колір» — палитра 3×10 + «#RRGGBB»; группы: основной экран, чат, окна вещей,
  торговля, остальные окна, настройки — «як усе / темна / світла».

## Review Focus

1. Цвет, который окно задаёт из C++ (`uisheet::Sheet::ground`, `hexOf(0x…)` в сетке вещей, `uaui::themed`), остался
   тёмным в светлой теме — тёмная заплатка на светлом окне. Тест: каждый такой вызов принимает группу (Task 3).
2. Старый `uaro-graphics.ini` с `theme = classic` / `contrast` / `standard` или `uaui_glass = 1` — игра открывается в
   тёмной теме без стекла, программа настроек не показывает пустой выбор. Тест в Task 2.
3. «Свій колір» с плохим значением (`accent_colour = zzz`, пусто, `#12345`) — используется акцент темы, ничего не
   падает. Тест в Task 1.
4. Смена темы или группы при открытых окнах — все документы перекрашиваются сразу, без перезапуска. Тест в Task 3
   (файл стиля группы меняется вместе с темой).
5. Документ с именем, которого нет в таблице групп, — получает группу «остальные окна», а не тёмный стиль по ошибке.
   Тест в Task 1.

---

### Task 1: Тон, группа и акцент в `core/ui_theme`

**Files:**
- Modify: `src/core/ui_theme.hpp`, `src/core/ui_theme.cpp`
- Test: `tests/test_ui_theme.cpp`

**Interfaces:**
- Produces:
  - `enum class Tone : uint8_t { Dark, Light };`
  - `enum class Group : uint8_t { Hud, Chat, Items, Trade, Windows, Settings }; constexpr int kGroups = 6;`
  - `enum class GroupTone : uint8_t { Same, Dark, Light };`
  - `struct Look { Tone tone = Tone::Dark; std::array<GroupTone, kGroups> groups{}; uint32_t accent = 0; };`
    (`accent` 0 — акцент тона: тёмный #e6cb8a, светлый #2f5fd0)
  - `Theme themeFor(const Look& look, Group g);` — `Theme` получает `Greys::Light` и поле `uint32_t accent`
  - `Group groupOf(const std::string& docName);`
  - `const char* groupWord(Group g);` — `"hud" "chat" "items" "trade" "windows" "settings"`
  - `uint32_t accentOf(const std::string& word, const std::string& colour);` — `"auto"`→0, `"gold"`→0xE6CB8A,
    `"blue"`→0x2F5FD0, `"green"`→0x2E8B57, `"red"`→0xC0392B, `"own"`→ значение `colour` (`#RRGGBB`), иначе 0
  - `uint32_t paletteColour(int row, int col);` — 3×10: тон `col/10`, светлота 0.35 / 0.55 / 0.75 по `row`,
    насыщенность 0.75
  - `std::string themedLink(const std::string& rml, Group g);` — `href="uaui:theme.rcss"` →
    `href="uaui:theme-<groupWord>.rcss"`
  - `named(name)`: `"light"` → светлый; любое другое (`dark`, `standard`, `classic`, `contrast`, неизвестное) → тёмный
    без изменения цветов (Warm и Spread удаляются)

- [ ] **Step 1: Write the failing tests** (старые `ui_theme_classic_…` и `ui_theme_contrast_…` удалить)

```cpp
TEST(ui_theme_light_turns_grounds_light_and_words_dark) {
    uitheme::Look look; look.tone = uitheme::Tone::Light;
    const uitheme::Theme t = uitheme::themeFor(look, uitheme::Group::Items);
    CHECK(lum(uitheme::colour(t, 0x10141C)) > 0.90f);   // the windows' ground
    CHECK(lum(uitheme::colour(t, 0xEEF2F8)) < 0.15f);   // the words
    CHECK(lum(uitheme::colour(t, 0x9AA3B5)) < 0.55f);   // the quiet words stay quieter than the words' ground
    CHECK(uitheme::colour(t, 0x5FD08A) == 0x5FD08Au);   // a colour that means something (HP's green) stays
}
TEST(ui_theme_accent_follows_the_tone_or_the_choice) {
    uitheme::Look look;
    CHECK(uitheme::colour(uitheme::themeFor(look, uitheme::Group::Hud), 0xE6CB8A) == 0xE6CB8Au);  // dark: gold
    look.tone = uitheme::Tone::Light;
    const uint32_t blue = uitheme::colour(uitheme::themeFor(look, uitheme::Group::Hud), 0xE6CB8A);
    CHECK((blue & 0xFF) > ((blue >> 16) & 0xFF));                                                // light: blue
    look.tone = uitheme::Tone::Dark; look.accent = 0x2E8B57;
    const uint32_t g = uitheme::colour(uitheme::themeFor(look, uitheme::Group::Hud), 0xC9A55B);
    CHECK(((g >> 8) & 0xFF) > ((g >> 16) & 0xFF) && ((g >> 8) & 0xFF) > (g & 0xFF));            // own: green
}
TEST(ui_theme_groups_follow_or_override_the_tone) {
    uitheme::Look look; look.tone = uitheme::Tone::Light;
    look.groups[int(uitheme::Group::Hud)] = uitheme::GroupTone::Dark;
    CHECK(uitheme::themeFor(look, uitheme::Group::Hud).greys == uitheme::Greys::Keep);
    CHECK(uitheme::themeFor(look, uitheme::Group::Chat).greys == uitheme::Greys::Light);
}
TEST(ui_theme_each_window_has_its_group) {
    using uitheme::Group;
    CHECK(uitheme::groupOf("charblock") == Group::Hud && uitheme::groupOf("hotbar") == Group::Hud);
    CHECK(uitheme::groupOf("minimap") == Group::Hud && uitheme::groupOf("mob hp") == Group::Hud);
    CHECK(uitheme::groupOf("chat") == Group::Chat && uitheme::groupOf("emotes") == Group::Chat);
    CHECK(uitheme::groupOf("items") == Group::Items && uitheme::groupOf("storage") == Group::Items);
    CHECK(uitheme::groupOf("cart") == Group::Items && uitheme::groupOf("equip") == Group::Items);
    CHECK(uitheme::groupOf("shop") == Group::Trade && uitheme::groupOf("vend shop") == Group::Trade);
    CHECK(uitheme::groupOf("trade") == Group::Trade && uitheme::groupOf("my shop") == Group::Trade);
    CHECK(uitheme::groupOf("skills") == Group::Windows && uitheme::groupOf("npc") == Group::Windows);
    CHECK(uitheme::groupOf("options") == Group::Settings);
    CHECK(uitheme::groupOf("no such window") == Group::Windows);
}
TEST(ui_theme_accent_words_and_own_colour) {
    CHECK(uitheme::accentOf("auto", "") == 0u && uitheme::accentOf("gold", "") == 0xE6CB8Au);
    CHECK(uitheme::accentOf("red", "") == 0xC0392Bu && uitheme::accentOf("own", "#C86BFF") == 0xC86BFFu);
    CHECK(uitheme::accentOf("own", "zzz") == 0u && uitheme::accentOf("own", "#12345") == 0u);
    CHECK(uitheme::accentOf("own", "") == 0u && uitheme::accentOf("purple", "") == 0u);
}
TEST(ui_theme_old_names_are_dark) {
    for (const char* n : {"standard", "classic", "contrast", "dark", "unknown"})
        CHECK(uitheme::named(n).greys == uitheme::Greys::Keep);
    CHECK(uitheme::named("light").greys == uitheme::Greys::Light);
}
TEST(ui_theme_palette_and_the_link) {
    CHECK(uitheme::paletteColour(0, 0) != uitheme::paletteColour(2, 0));
    CHECK(lum(uitheme::paletteColour(2, 4)) > lum(uitheme::paletteColour(0, 4)));
    CHECK(uitheme::themedLink("<link href=\"uaui:theme.rcss\"/>", uitheme::Group::Trade) ==
          "<link href=\"uaui:theme-trade.rcss\"/>");
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `nice -n 19 ionice -c3 cmake --build build-host -j4 --target uagfx_tests`, затем
`./build-host/uagfx_tests 2>&1 | grep -E "ui_theme|tests, "`
Expected: не собирается — `Tone`, `themeFor`, `groupOf` нет.

- [ ] **Step 3: Implement in `src/core/ui_theme.{hpp,cpp}`**

Светлый тон для серых (насыщенность ≤ 0.18 — как сейчас решает `colour`): светлота
`l' = clamp(0.976 − 1.097·(l − 0.13), 0.06, 0.985)`, оттенок сохраняется, насыщенность ≤ 0.15. Акцент: цвета с тоном
30–55° и насыщенностью > 0.25 (золото #e6cb8a, #c9a55b, #f3c653) заменяются акцентом темы с той же относительной
светлотой; в тёмном тоне без своего акцента — не меняются. Таблица `groupOf` — по именам документов:
Hud — buffs, charblock, exp bars, exp gain, hotbar, hunt, item toast, low hp edge, minimap, mvp, recap, target,
threat, tracker, cursor, big map, big map cross, skill wheel, pad panel ring, window size grip, cast, ground items,
hud edit, key hint, loading bar, mob card, mob hp, npc dots, npc tag, pad carry, pad hint, pad tiles, signs,
merc block, enter veil, showcase; Chat — chat, chat wheel, emote, emotes, commands, whisper setup, chat room setup,
room, answer, macro; Items — items, equip, storage, cart, ask, pick; Trade — shop, vend shop, vend setup, my shop,
trade; Settings — options; остальное — Windows.

- [ ] **Step 4: Run tests to verify they pass**

Run: та же команда. Expected: все `ui_theme_*` PASS, `0 failed checks`.

- [ ] **Step 5: Commit**

```bash
git add src/core/ui_theme.hpp src/core/ui_theme.cpp tests/test_ui_theme.cpp
git commit -m "feat: core/ui_theme - the light tone, the groups, the accent and its own colour; classic/contrast gone"
```

### Task 2: Ключи настроек и уход «скляних вікон»

**Files:**
- Modify: `src/core/config.hpp`, `src/core/config.cpp`, `src/core/settings/schema.cpp`,
  `src/core/settings/catalogue.cpp`, `src/core/settings/navigation.cpp` (`interfaceSections`),
  `src/core/settings_window.cpp` (`simpleLines`), `src/core/settings/strings.{hpp,cpp}`, `src/core/live_config.cpp`,
  `src/core/settings/old_names.cpp`
- Test: `tests/test_schema.cpp`, `tests/test_catalogue.cpp`, `tests/test_config.cpp`

**Interfaces:**
- Consumes: Task 1 — `uitheme::Look`, `GroupTone`, `accentOf`.
- Produces:
  - `[interface] theme = dark | light` (по умолчанию `dark`; `standard`/`classic`/`contrast` читаются как `dark`)
  - `[interface] theme_hud | theme_chat | theme_items | theme_trade | theme_windows | theme_settings = same | dark |
    light` (по умолчанию `same`)
  - `[interface] accent = auto | gold | blue | green | red | own` (по умолчанию `auto`),
    `[interface] accent_colour = #RRGGBB` (по умолчанию пусто)
  - `Config`: `std::string uiTheme = "dark"; std::array<std::string, uitheme::kGroups> uiGroups;
    std::string uiAccent = "auto"; std::string uiAccentColour;` и `uitheme::Look lookOf(const Config&)` в
    `config.cpp`
  - `uauiGlass` и ключ `interface.uaui_glass` удалены (старый ключ в ini просто игнорируется)
  - тексты: `InterfaceThemeDark`, `InterfaceThemeLight`, `InterfaceThemeGroups`, `InterfaceThemeSame`,
    `InterfaceGroupHud`, `InterfaceGroupChat`, `InterfaceGroupItems`, `InterfaceGroupTrade`, `InterfaceGroupWindows`,
    `InterfaceGroupSettings`, `InterfaceAccent`, `InterfaceAccentAuto`, `InterfaceAccentGold`, `…Blue`, `…Green`,
    `…Red`, `InterfaceAccentOwn`, `InterfaceAccentPick`, `SimpleTheme` (uk: «Темна», «Світла», «Кольори по групах»,
    «як усе», «Основний екран», «Чат», «Вікна речей», «Торгівля», «Інші вікна», «Налаштування Alt+O»,
    «Колір акценту», «за темою», «золотий», «синій», «зелений», «червоний», «свій колір», «Вибрати колір…»)

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(config_theme_old_values_are_dark_and_glass_is_gone) {
    Config c = parseConfig("[interface]\ntheme = contrast\nuaui_glass = 1\n");
    CHECK(c.uiTheme == "dark");
    CHECK(findSetting("interface.uaui_glass") == nullptr);
    CHECK(parseConfig("[interface]\ntheme = classic\n").uiTheme == "dark");
    CHECK(parseConfig("[interface]\ntheme = light\n").uiTheme == "light");
}
TEST(config_theme_groups_and_accent_make_the_look) {
    Config c = parseConfig("[interface]\ntheme = light\ntheme_hud = dark\naccent = own\naccent_colour = #C86BFF\n");
    const uitheme::Look look = lookOf(c);
    CHECK(look.tone == uitheme::Tone::Light);
    CHECK(look.groups[int(uitheme::Group::Hud)] == uitheme::GroupTone::Dark);
    CHECK(look.groups[int(uitheme::Group::Trade)] == uitheme::GroupTone::Same);
    CHECK(look.accent == 0xC86BFFu);
}
TEST(catalogue_theme_is_basic_groups_and_accent_detail) {
    const catalogue::Row* th = catalogue::find("interface.theme");
    CHECK(th && th->level == catalogue::Level::Basic && catalogue::homeOf(*th) == catalogue::Home::Game);
    for (const char* k : {"interface.theme_hud", "interface.theme_chat", "interface.theme_items",
                          "interface.theme_trade", "interface.theme_windows", "interface.theme_settings",
                          "interface.accent"}) {
        const catalogue::Row* r = catalogue::find(k);
        CHECK(r && r->level == catalogue::Level::Detail);
    }
    CHECK(catalogue::find("interface.uaui_glass") == nullptr);
    CHECK(settings::interfaceSections().front().keys.front() == std::string("interface.theme"));
}
TEST(settings_reset_all_clears_the_own_colour) {
    Config shown = parseConfig("[interface]\naccent = own\naccent_colour = #C86BFF\n");
    bool cleared = false;
    for (const settingswin::Reset& r : settingswin::resetsAll(shown, Config{}))
        cleared = cleared || (r.key == "interface.accent_colour" && r.token.empty());
    CHECK(cleared);
}
```

- [ ] **Step 2: Run tests to verify they fail** — та же команда; Expected: FAIL / не собирается.

- [ ] **Step 3: Implement** — строки схемы (`Control::Choice`), разбор в `config.cpp`, `tokenOf`, живое применение
в `live_config.cpp` (как у `uiTheme` сейчас — сразу), строка в `simpleLines` (`{Text::ViewInterface,
"interface.theme", Text::SimpleTheme}` первой в разделе интерфейса), каталог (`B("interface.theme")`,
`D("interface.theme_*")`, `D("interface.accent")`), удаление `uaui_glass` из схемы, каталога, конфига, строк.
`accent_colour` — без строки в схеме (его пишут палитра в игре и кнопка в программе настроек); читается
`config.cpp`. Существующие тесты каталога со списками и счётчиками — обновить под новые строки.

- [ ] **Step 4: Run tests to verify they pass** — Expected: `0 failed checks`.

- [ ] **Step 5: Commit** — `feat: the theme settings - dark/light, the groups, the accent; the glass windows gone`

### Task 3: uaUI красит каждую группу своей темой

**Files:**
- Modify: `src/win/uaui.hpp`, `src/win/uaui.cpp`, `src/win/context.cpp`, `src/win/ui_sheet.{hpp,cpp}`,
  `src/win/item_grid_ui.cpp`, все вызовы `uaui::themed(` и `uisheet::Sheet::ground(` (38 файлов в `src/win`,
  `src/uikit` — список: `grep -rln "Sheet::ground(\|hexOf(0x\|uaui::themed(" src/win src/uikit`)
- Remove: путь «скляних вікон» — `uaui::setGlass`, класс `glassy` в `src/core/ui_theme_css.cpp`, копия кадра для
  стекла в `src/win/gpu/draw.cpp`, шейдер стекла в `src/win/gpu/shaders.cpp` и его строка в
  `src/win/gpu/pipeline.cpp` / `renderer11.cpp`
- Test: `tests/test_ui_theme.cpp` (Task 1 уже покрывает ядро), `tests/test_uikit_options.cpp` (разметка всё ещё
  парсится)

**Interfaces:**
- Consumes: Task 1 (`themeFor`, `groupOf`, `groupWord`, `themedLink`), Task 2 (`lookOf(config)`).
- Produces:
  - `void uaui::begin(…, const uitheme::Look& look)` вместо `const std::string& themeName`
  - `bool Doc::make(name, rml, bind)` подставляет `themedLink(rml, groupOf(name))` перед загрузкой
  - файловый интерфейс отдаёт `uaui:theme-<groupWord>.rcss` = `themedCss(themeFor(look, g), kTheme)` + правила
    администратора и игрока (`theme.rcss`) — как сейчас для `uaui:theme.rcss`
  - `uint32_t uaui::themed(uint32_t rgb, uitheme::Group g)`; `uisheet::Sheet::ground(uint32_t rgb, float alpha,
    uitheme::Group g)`; заливки и рамки панелей красятся темой группы документа, который рисуется
  - смена любого ключа темы → `ClearStyleSheetCache`, `ReloadStyleSheet` всех документов, `uistyle::bump()` (как
    сейчас при смене темы)

- [ ] **Step 1: Write the failing test** — в `tests/test_ui_theme.cpp`:

```cpp
TEST(ui_theme_group_style_differs_by_tone) {
    uitheme::Look look; look.tone = uitheme::Tone::Light;
    look.groups[int(uitheme::Group::Hud)] = uitheme::GroupTone::Dark;
    const std::string css = "body { background-color: #10141c; }";
    CHECK(uitheme::themedCss(uitheme::themeFor(look, uitheme::Group::Hud), css) == css);
    CHECK(uitheme::themedCss(uitheme::themeFor(look, uitheme::Group::Chat), css) != css);
}
```

- [ ] **Step 2: Run it to verify it fails** (до Task 1 — не соберётся; после — проходит: тест закрепляет поведение
стиля по группам, на котором стоит Task 3).

- [ ] **Step 3: Implement** — подстановка ссылки в `Doc::make`; шесть виртуальных файлов; тема документа для
заливок; новые параметры группы во всех 38 файлах (группа — `uitheme::groupOf("<имя документа этого окна>")`);
`context.cpp` передаёт `lookOf(config)`; удаление пути стекла (+ его строк в `pipeline.cpp`, `renderer11.cpp`,
`context.cpp`).

- [ ] **Step 4: Verify**

Run: host-тесты; `nice -n 19 ionice -c3 cmake --build build-win -j4`; `… build-win64 -j4`;
`bash /root/uagfx-tools/work/allshaders.sh`. Expected: `0 failed checks`, обе сборки `0`, «shaders: N compiled with
DXC, ok»; `grep -rn "uaui:theme.rcss" src/win src/uikit | wc -l` — то же число, что до изменения (ссылка в разметке
остаётся, подменяется при загрузке).

- [ ] **Step 5: Commit** — `feat: uaUI paints each window group by its own theme; the glass windows' path removed`

### Task 4: Alt+O — тема картинками, группы и свой цвет

**Files:**
- Modify: `src/uikit/options_live.cpp` (разметка RML и события), `src/win/options_card.cpp` (данные строк),
  `src/win/settingswin` (запись ключей — тем же путём, что строки сейчас)
- Create: `src/core/theme_choice.{hpp,cpp}`
- Test: `tests/test_theme_choice.cpp`, существующие `tests/test_uikit_options.cpp`

**Interfaces:**
- Consumes: Task 1 (`paletteColour`, `accentOf`), Task 2 (ключи).
- Produces:
  - `src/core/theme_choice.{hpp,cpp}` (чистое ядро, тестируется): `bool themechoice::swatchRow(const std::string& key)`
    (true только для `interface.theme`); `std::vector<uint32_t> themechoice::palette()` (30 цветов `paletteColour`
    построчно); `std::pair<std::string, std::string> themechoice::pick(int row, int col)` → `{"own", "#RRGGBB"}`
  - строка `interface.theme` рисуется двумя карточками-образцами «Темна» / «Світла» (класс `.swatch`, выбранная —
    рамкой акцента); событие `set_choice` как у выбора
  - строка `interface.accent`: пять кнопок (за темою, золотий, синій, зелений, червоний) + «свій колір…»; «свій
    колір…» открывает в той же строке палитру 3×10 (`.pal` из `paletteColour`) и поле «#RRGGBB»; клик по ячейке
    пишет `accent = own`, `accent_colour = #RRGGBB`; пад и клавиши ходят по палитре как по ячейкам
  - строки `interface.theme_*` — три чипа «як усе / темна / світла» (как любой выбор)

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(theme_choice_swatch_and_palette) {
    CHECK(themechoice::swatchRow("interface.theme") && !themechoice::swatchRow("interface.theme_hud"));
    CHECK(themechoice::palette().size() == 30u);
    const auto pick = themechoice::pick(1, 8);
    CHECK(pick.first == "own" && pick.second.size() == 7 && pick.second[0] == '#');
    CHECK(uitheme::accentOf(pick.first, pick.second) == uitheme::paletteColour(1, 8));
}
```
(разметка Alt+O с новыми классами по-прежнему проходит XML-разбор существующих тестов `test_uikit_options.cpp` —
`uikit_options_*` должны остаться PASS.)

- [ ] **Step 2: Run to verify they fail.**
- [ ] **Step 3: Implement** — RML руками, без `SetProperty` каждый кадр; тексты из Task 2.
- [ ] **Step 4: Run to verify they pass**; сборки win/win64.
- [ ] **Step 5: Commit** — `feat: Alt+O - the theme as two pictures, the groups, the accent with its own colour`

### Task 5: Программа настроек — страница «Інтерфейс»

**Files:**
- Modify: `src/app/page_settings.cpp` (`buildInterface`, обработка кнопки), `src/core/settings/navigation.cpp`
  (раздел темы — Task 2 уже добавил ключи)
- Test: `tests/test_schema.cpp` (уровни строк: тема — «Простий» и «Докладний», группы и акцент — только
  «Докладний»)

**Interfaces:**
- Consumes: Task 2 (ключи, тексты).
- Produces: первый раздел страницы «Інтерфейс» — «Тема інтерфейсу гри» (выбор), в «Докладному» — шесть групп и
  «Колір акценту»; при `accent = own` — кнопка «Вибрати колір…» (`ChooseColorW`), которая пишет `accent_colour` через
  `Model::writeIni({{"interface", "accent_colour", "#RRGGBB"}}, lang)`.

- [ ] **Step 1: Write the failing test** — строки `interface.theme_hud` и `interface.accent` видны только на уровне
  «Докладний» (тот же помощник, что для других строк уровня в `test_schema.cpp`).
- [ ] **Step 2: Run to verify it fails.**
- [ ] **Step 3: Implement** — раздел в `interfaceSections()`; кнопка с id `kAccentPick` в `buildInterface`.
- [ ] **Step 4: Verify** — тесты; `build-win`; `build-win64`.
- [ ] **Step 5: Commit** — `feat: the settings app - the interface theme, the groups and the accent's own colour`

### Task 6: Лаунчер — «Інтерфейс: Темний / Світлий»

**Files:**
- Modify: `src/core/launcher_layout.{hpp,cpp}`, `src/app/launcher_view.{hpp,cpp}`, хост лаунчера
  (`src/app/launcher.cpp`)
- Test: `tests/test_launcher_layout.cpp`

**Interfaces:**
- Produces: `Layout::theme[2]` (Box) — над кнопкой «ГРАТИ», под строкой «Вигляд гри», слева направо «Темний»,
  «Світлий»; `LauncherHost::themeAction(bool light)` пишет `[interface] theme` (`dark` / `light`) тем же
  `Model::writeIni`; выбранная — рамкой акцента, как строка «Вигляд».

- [ ] **Step 1: Write the failing test**

```cpp
TEST(launcher_layout_theme_pair_sits_by_the_look_line) {
    const launcherview::Layout L = launcherview::layoutFor(1280, 720, 1.0f, 3, false);
    CHECK(L.theme[0].w > 0 && L.theme[1].x > L.theme[0].x);
    CHECK(L.theme[0].y + L.theme[0].h <= L.play.y);       // over the big button
    CHECK(L.theme[0].y >= L.look.y + L.look.h);           // under the look line
}
```
- [ ] **Step 2: Run to verify it fails.**
- [ ] **Step 3: Implement** — рисунок двух полей в `launcher_view` (как поле «Вигляд»), клик → `themeAction`.
- [ ] **Step 4: Verify** — тесты; сборки.
- [ ] **Step 5: Commit** — `feat: the launcher - the game interface's theme by the look line`

### Task 7: Бета для тестеров

**Files:**
- Modify: `src/core/checks_now.cpp`; `/root/uagfx-tools/work/notes/3.0.24-beta.N.{uk,en,ru}.md`

- [ ] **Step 1:** карточка «Тема інтерфейсу: темна й світла» (ключ `interface.theme`): «Alt+O → «Інтерфейс» → тема
  «Світла»: все окна светлые; «Кольори по групах» в повному режимі; «свій колір» в акценте».
- [ ] **Step 2:** заметки на трёх языках: где выбрать (Alt+O, лаунчер, программа настроек), что ушли «скляні вікна»
  и темы «класична» / «контрастна».
- [ ] **Step 3:** `bash /root/uagfx-tools/work/gship.sh 3.0.24-beta.N "interface themes: dark and light"`;
  Expected: `SHIPPED beta 3.0.24-beta.N`.
- [ ] **Step 4:** память `project-item-windows-redesign`: что вышло, что проверить в игре.

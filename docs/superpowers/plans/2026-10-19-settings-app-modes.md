# Settings App Modes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («делай без остановки в инлайн режиме»): executing-plans, no parallel agents, no stops.

**Goal:** uaro-graphics.exe with three modes, 8 pages instead of 15 (Главное in Simple mode, Отладка in Developer
mode), a resizable window that remembers itself, search, a status bar, changed-vs-recommended dots and «Что нового».

**Architecture:** What can be decided off Windows goes to `src/core/settings` with host tests: the modes, the views
(the list's entries) and which old pages they show, each setting's level, the search, the recommended values, the
Main page's quality choice, the notes of a version, the window's saved place. The app (`src/app`, Win32) keeps its
pages as they are and composes them: one `SettingsPage` shows several old pages, a parts page routes commands, the
Нейросеть view shows one old page per tab. MainWindow owns the navigation (mode, view, tab, query).

**Tech Stack:** C++17, MinGW-w64 i686, Win32 + comctl32 v6, the project's test runner (`tests/test.hpp`).

**Spec:** `docs/superpowers/specs/2026-10-19-settings-app-modes-design.md` (the approved canvas in words, with the
calls made).

## Global Constraints

- Work on main in /root/ragnarok-graphics (the project's way); no push, no version bump - `ship.sh` does both on the
  user's word.
- Gate after every task: `bash $S/gate.sh` → `GATE OK` (32/64-bit builds without warnings, check_exe imports
  Windows 7 has, check_texts: every Text used, host tests, lines ≤ 120).
- Page controls' ids ≥ 1000 (only those reach a page); the main window's own < 1000. Pages are made anew at every
  change: state that must outlive a rebuild lives in MainWindow or in a file static.
- Texts: append to `enum class Text` before `Count` and to `kTexts` in the same order, en/uk/ru; Russian copy as the
  canvas has it.
- Nothing leaves the ini; a hidden row keeps its value; Apply/Undo stay.
- Comments in English in the code's voice, new ones tagged `2.6:`; commits as KWEB Team, Russian messages, trailer
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. A saved window place on a monitor that is gone → the window opens at the default place on a monitor that is there.
2. A resize or a mode switch while a number field holds typed text → the text is taken (buildPage's EN_KILLFOCUS
   path), not lost.
3. A pending change on a row the new mode hides → still counted in «Не применено» and written by Apply.
4. `showPage(Page::Pack)` and «Сообщить о проблеме» from Simple mode → the view opens off the list at the Detailed
   level; clicking a list entry goes back.
5. Search for one letter, for nothing that matches, for Ukrainian і/ї/є/ґ and ё → no crash, a "nothing found" note,
   matches regardless of case.

---

### Task 1: Modes, views, places and levels (core)

**Files:**
- Create: `src/core/settings/navigation.hpp`, `src/core/settings/navigation.cpp`, `tests/test_navigation.cpp`
- Modify: `src/core/settings/strings.{hpp,cpp}` (mode, view and tab names)

**Interfaces:**
- Produces:
  - `enum class Mode : uint8_t { Simple, Detailed, Developer }; constexpr int kModeCount = 3;`
  - `const char* modeToken(Mode)` ("simple", "detailed", "developer"); `std::optional<Mode> parseMode(const
    std::string&)`; `Mode startMode(std::optional<Mode> saved, bool usedBefore)`; `Text modeName(Mode)`
  - `enum class View : uint8_t { Main, Graphics, Screen, Effects, Ai, Sound, Pad, GameChat, Service, Debug };
    constexpr int kViewCount = 10;` `Text viewName(View)`
  - `std::vector<View> viewsOf(Mode)`; `bool developerView(View)` (Debug)
  - `std::vector<Page> partsOf(View, int tab)` - the old pages a view's tab shows
  - `int tabCount(View)`; `Text tabName(View, int tab)`
  - `struct Place { View view; int tab; };` `Place placeOf(Page)`
  - `Mode levelOf(const IniSetting&)`; `bool mainKey(const std::string& sectionKey)` (the rows Главное shows)
  - `bool resettable(View, int tab)` - views made of schema rows: Graphics, Effects, Ai tabs 0-1, Sound
- [ ] **Step 1: Write the failing tests** `nav_simple_lists_main_effects_pad` (viewsOf(Simple) == {Main, Effects,
  Pad}), `nav_detailed_lists_eight_pages` (== {Graphics, Screen, Effects, Ai, Sound, Pad, GameChat, Service}),
  `nav_developer_adds_debug_last` (Detailed's + Debug, developerView(Debug)), `nav_every_page_has_a_place`
  (placeOf(Pack) == {Ai, 2}, placeOf(AiCache) == {Ai, 3}, placeOf(Frames).view == Graphics, placeOf(Chat).view ==
  GameChat, placeOf(Diagnostics).view == Service, and for every Page p: p is in partsOf(placeOf(p)...)),
  `nav_graphics_shows_four_pages` (partsOf(Graphics, 0) == {Render, Quality, Smoothing, Frames}),
  `nav_start_mode` (startMode(nullopt, false) == Simple, (nullopt, true) == Detailed, (Developer, false) ==
  Developer, parseMode round trip, parseMode("x") empty), `nav_levels` (levelOf("debug.profile") == Developer,
  levelOf("renderer.render_thread") == Developer, levelOf("quality.dlss") == Simple, levelOf("effects.preset") ==
  Simple, levelOf("effects.torch") == Detailed, levelOf("quality.msaa") == Detailed; every key of the Simple and
  Developer lists exists in the schema).
- [ ] **Step 2: Run** `cmake --build build-host -j && ./build-host/uagfx_tests nav_` → FAIL (no navigation.hpp).
- [ ] **Step 3: Implement.** Main keys (Simple): quality.super_light, quality.dlss, frames.frame_generation,
  frames.fps_limit, frames.vsync, display.hdr, sound.engine, frames.battery_saver, ai.enabled, effects.preset,
  effects.group_* and their _strength. Developer keys: renderer.render_thread, renderer.remix, every debug.* key,
  the rows Task 2 adds. Everything else Detailed. Tab names of Ai: Что улучшать, Модели по группам, Пакет, Готовые
  текстуры; parts of Ai by tab: {Ai}, {Ai}, {Pack}, {AiCache}. GameChat: {Game, Chat}; Service: {Service,
  Diagnostics}; Debug: {Debug} (Task 2 adds Page::Debug - this task maps Debug to {} until then).
- [ ] **Step 4: Run** the tests → PASS; `bash $S/gate.sh` → GATE OK.
- [ ] **Step 5: Commit** `feat: режимы и разделы программы настроек (ядро)`.

### Task 2: The Отладка rows in the schema

**Files:**
- Modify: `src/core/settings/schema.{hpp,cpp}` (Page::Debug, kPageCount 16, the rows), `src/core/live_config.cpp`
  (restart keys if the new rows wait for one), `src/core/settings/strings.{hpp,cpp}`,
  `src/core/settings/navigation.cpp` (Debug → {Page::Debug}), `src/app/page_diagnostics.cpp` (the log and profiler
  boxes go to Отладка), `tests/test_schema.cpp`, `tests/test_navigation.cpp`

**Interfaces:**
- Produces: `Page::Debug` (= 15, last); schema rows on it: renderer.render_thread, renderer.remix (moved from Render),
  debug.log, debug.profile, debug.xray (moved from Diagnostics), renderer.bridge_present (Choice auto / window /
  composition), quality.dlss_jitter and quality.dlss_motion (Check, tokens {"normal", "inverted"}),
  debug.shader_variants, debug.state_cache, debug.merge_draws, debug.fast_pick, debug.fast_ftol,
  renderer.bridge_dpi_scale (Check {"0", "1"}).
- [ ] **Step 1: Tests:** `schema_pages_follow_the_spec` expects kPageCount 16 and Page::Debug == 15 with
  pageName(Debug) == Text::PageDebug; `nav_debug_rows_are_developer` (every row with page Debug has levelOf ==
  Developer); the existing `schema_every_word_reads_back_through_the_driver` covers tokenOf of the new rows.
- [ ] **Step 2: Run** `./build-host/uagfx_tests schema_` → FAIL.
- [ ] **Step 3: Implement** the rows (tokenOf branches from Config: bridgePresent, dlssJitterInverted,
  dlssMotionInverted, shaderVariants, stateCache, mergeDraws, fastPick, fastFtol, bridgeDpiScale), labels and hints
  in three languages, and the restart list in `appliesNow` as the driver reads them (check each key's reader in
  `win/`); `schema_live_keys_are_the_specs` updated to the same list. DiagnosticsPage drops kVerbose and kProfile.
- [ ] **Step 4: Run** tests → PASS; gate → GATE OK.
- [ ] **Step 5: Commit** `feat: строки страницы «Отладка» в схеме настроек`.

### Task 3: Search (core)

**Files:**
- Create: `src/core/settings/search.{hpp,cpp}`, `tests/test_search.cpp`

**Interfaces:**
- Consumes: Task 1 (Mode, View, Place, placeOf, levelOf, mainKey, viewsOf).
- Produces:
  - `std::string foldCase(const std::string& utf8)` - Latin and Cyrillic (Russian and Ukrainian) lower case, ё → е
  - `bool textMatches(const std::string& text, const std::string& query)` - every word of the query is a part of the
    text, after foldCase
  - `enum class HitKind : uint8_t { Ini, Reg, Action };`
    `struct Hit { HitKind kind; size_t index; Place place; };` - index into iniSettings(), regSettings() or actions()
  - `struct Action { Text label; Page page; Mode level; };` `const std::vector<Action>& actions()` - buttons and
    pages worth finding: the pack's making, «Сжать», profiles, backups, update, the report, Palettes.grf, the model
    preview, one map's textures, the screen's mode and interface size, the gamepad
  - `std::vector<Hit> findSettings(const std::string& query, Lang, Mode)` - labels and hints in that language; rows
    above the mode left out; in Simple mode mainKey rows placed on Main; in the list's order of views, then the
    schema's; at most 80; a query under 2 letters finds nothing
- [ ] **Step 1: Tests** `search_folds_cyrillic_case` (foldCase("СВЕТ Ёлка ІЇЄҐ") == "свет елка іїєґ"),
  `search_needs_every_word` (textMatches("Свет факелов", "факел свет"), !textMatches("Свет факелов", "свет дым")),
  `search_finds_effects_by_russian_label` (findSettings("факел", Ru, Detailed) has an Ini hit for effects.torch with
  place.view == Effects), `search_hides_developer_rows` (findSettings("профилировщик"-like label of debug.profile, Ru,
  Detailed) empty, Developer finds it), `search_simple_places_main_rows` (findSettings("DLSS", Ru, Simple): every
  hit's place.view is Main or Effects), `search_short_query_finds_nothing` ("с").
- [ ] **Step 2: Run** `./build-host/uagfx_tests search_` → FAIL.
- [ ] **Step 3: Implement** (UTF-8 decode to code points, map А-Я/Ё/Є/І/Ї/Ґ and A-Z, encode back).
- [ ] **Step 4: Run** → PASS; gate → GATE OK.
- [ ] **Step 5: Commit** `feat: поиск по настройкам (ядро)`.

### Task 4: Recommended values, Главное's quality, notes, window place (core)

**Files:**
- Create: `src/core/settings/recommended.{hpp,cpp}`, `src/core/settings/whats_new.{hpp,cpp}`,
  `src/core/settings/window_place.{hpp,cpp}`, `tests/test_recommended.cpp`, `tests/test_whats_new.cpp`
- Modify: `src/core/settings/presets.{hpp,cpp}` (Main quality)

**Interfaces:**
- Produces:
  - `std::map<std::string, std::string> recommendedTokens(const Machine& m)` - every schema row's token from the
    shipped defaults (`parseConfig(defaultIni())`... the defaults the app resets to), with `autoSetup(m).edits` over
    them
  - `enum class MainQuality { Low, Medium, High, SuperLight };` `std::vector<IniEdit> mainQualityEdits(MainQuality)`
    (presetEdits of Speed / Balance / Maximum without the [frames] keys, plus super_light 0; SuperLight: super_light
    1 only); `std::optional<MainQuality> mainQualityOf(const Config&)` (super_light → SuperLight, else presetOf
    without the game's words: Speed → Low, Balance → Medium, Maximum → High)
  - `std::string notesOf(const std::string& markdown, const std::string& version)` - the section `# uaRO Graphics
    <version> ...` up to the next `# `, plain text: `**`/`` ` `` gone, a bullet's wrapped lines joined, "- " → "• ";
    "" when there is none
  - `struct WindowPlace { int left, top, right, bottom; bool maximized; };` `std::string placeToken(const
    WindowPlace&)` ("10 20 1110 740 0"); `std::optional<WindowPlace> parsePlace(const std::string&)` (five integers,
    width and height 200-20000, else empty)
- [ ] **Step 1: Tests** `recommended_takes_auto_setup_over_defaults` (a Machine with an RTX and 16 GB gives
  quality.dlss as autoSetup says, quality.sharpen "50"), `main_quality_round_trip` (for each q other than
  SuperLight: mainQualityOf(parseConfig(defaultIni + edits)) == q; SuperLight edits give SuperLight; none of the
  edits is a [frames] key), `whats_new_takes_one_version` (a sample with 2.5.27 and 2.5.26 gives only 2.5.27's
  bullets, wrapped lines joined, no `**`), `whats_new_missing_version_is_empty`, `window_place_round_trip` and
  `window_place_rejects_nonsense` ("1 2 3", "a b c d e", width 50).
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run** → PASS; gate → GATE OK.
- [ ] **Step 5: Commit** `feat: рекомендуемые значения, качество «Главного», заметки версии, место окна (ядро)`.

### Task 5: The model keeps the mode, the window, the seen version

**Files:**
- Modify: `src/app/model.{hpp,cpp}`, `tests/test_model.cpp`

**Interfaces:**
- Consumes: Task 1 (Mode, startMode, parseMode), Task 4 (WindowPlace, recommendedTokens).
- Produces (Model):
  - `settings::Mode startMode() const` - [utility] mode, else Simple for a new install, Detailed when the file holds
    [utility] language or [stats] send; `void saveMode(settings::Mode)`
  - `std::optional<settings::WindowPlace> savedPlace() const`; `void savePlace(const settings::WindowPlace&)`
  - `std::string seenVersion() const`; `void saveSeenVersion(const std::string&)`
  - `std::string recommended(const settings::IniSetting&) const` - from recommendedTokens of this computer (the
    Machine the Render page's auto setup builds, moved into `Model::machine()`), computed once
  - `bool usedBefore() const`
  - [utility] mode, window and seen kept across a reset and a profile (keptAcross)
- [ ] **Step 1: Tests** `model_new_install_starts_simple`, `model_used_install_starts_detailed` (ini with
  [utility] language = ru), `model_mode_and_place_survive_reset` (saveMode(Developer), savePlace, resetDefaults →
  both read back).
- [ ] **Step 2: Run** `./build-host/uagfx_tests model_` → FAIL. **Step 3: Implement** (writeIni, no backup, like
  saveLanguage). **Step 4: Run** → PASS; gate → GATE OK.
- [ ] **Step 5: Commit** `feat: программа настроек помнит режим, окно и показанную версию`.

### Task 6: Panel - two columns, tabs, dots

**Files:**
- Modify: `src/app/ui.{hpp,cpp}`

**Interfaces:**
- Produces (Panel):
  - `bool split(int minColumn96 = 400)` - two columns from here when each is at least that wide (true), else one;
    `void nextColumn()` - the right column from the split's top; `void join()` - full width under the taller column
  - `void tabs(int firstId, const std::vector<std::wstring>& names, int selected)` - push-like radio buttons in a row
  - `HWND dot(HWND row, const std::wstring& hint)` - a "●" in the accent colour left of the row's control
  - `Tone::Accent` (RGB 15, 108, 189); `size_t madeCount() const`; `HWND madeAt(size_t) const`
- [ ] **Step 1:** Implement; split keeps the area's left/right per column, height() covers the taller column.
- [ ] **Step 2:** gate → GATE OK (no host test: Win32 only). **Step 3: Commit** `feat: колонки, вкладки и метки в
  разметке страниц`.

### Task 7: The window grows and remembers itself

**Files:**
- Modify: `src/app/main_window.{hpp,cpp}`, `tools/check_exe.py` (GetWindowPlacement, SetWindowPlacement,
  MonitorFromRect if missing)

**Interfaces:**
- Consumes: Task 5 (savedPlace, savePlace).
- [ ] **Step 1:** WS_OVERLAPPEDWINDOW; WM_GETMINMAXINFO: 880×600 client at the window's DPI; first start 1100×720
  client when the work area has room; a saved place restored when MonitorFromRect(MONITOR_DEFAULTTONULL) finds its
  monitor; saved in WM_CLOSE once the close is confirmed.
- [ ] **Step 2:** WM_SIZE → layout() (and the scroll bar); the page made again at WM_EXITSIZEMOVE when the width
  changed, at once for maximize and restore.
- [ ] **Step 3:** gate → GATE OK. **Step 4: Commit** `feat: окно настроек растягивается, разворачивается и помнит
  размер`.

### Task 8: Header, list by mode, views

**Files:**
- Modify: `src/app/main_window.{hpp,cpp}`, `src/app/page.hpp`, `src/app/page_settings.cpp`, `src/app/page_ai.cpp`,
  `src/core/settings/strings.{hpp,cpp}`
- Create: `src/app/page_parts.cpp` (a page of pages), CMakeLists.txt (new app files)

**Interfaces:**
- Consumes: Tasks 1, 5, 6.
- Produces:
  - PageHost: `settings::Mode mode() const` (the level pages show: the mode, at least Detailed off the list)
  - `std::unique_ptr<Page> makeSettingsPage(PageHost&, std::vector<settings::Page> left,
    std::vector<settings::Page> right = {})` - rows of several pages, the right ones in a second column when wide;
    rows above `host.mode()` left out
  - `std::unique_ptr<Page> makePartsPage(PageHost&, std::vector<std::unique_ptr<Page>>)` - builds them in turn,
    a command goes to the part that made the control (Panel::madeAt ranges)
  - `enum class AiPart { What, Models };` `makeAiPage(PageHost&, AiPart)`
  - `virtual bool Page::buildRow(Panel&, const std::string& key)` (default false) - for Tasks 10-11
- [ ] **Step 1:** Header (52 px): three push-like mode buttons (ids 30-32), the search field (id 33, cue «Поиск по
  настройкам   Ctrl+F»), «Сообщить о проблеме» (id 34), the language list; the elevated banner under it when shown.
- [ ] **Step 2:** The list: owner-drawn ListBox of viewsOf(mode) (selected: grey fill, accent bar, bold), «Только
  разработчику» as a grey heading that cannot be selected, developer entries in brown.
- [ ] **Step 3:** MainWindow state: mode_, shown_ (View), tab_, offList_; showPage(Page) → placeOf; a view off the
  mode's list shows with no entry selected; the mode switch keeps the view when the new mode lists it, else the
  mode's first; the mode saved at once.
- [ ] **Step 4:** buildPage: the tabs row (ids 40-43) for Ai; the view's page: Graphics = SettingsPage({Render,
  Quality}, {Smoothing, Frames}); Sound; GameChat = SettingsPage({Game, Chat}); Service = Parts(Diagnostics,
  Service); Ai tabs = Ai(What), Ai(Models), Pack, AiCache; Screen, Effects, Pad as before; Debug = SettingsPage({Debug})
  (Task 9 gives it its tools); Main = Graphics until Task 10.
- [ ] **Step 5:** «Сообщить о проблеме» → Service, the report's comment field focused and scrolled to.
- [ ] **Step 6:** gate → GATE OK. **Step 7: Commit** `feat: режимы в шапке, восемь страниц вместо пятнадцати`.

### Task 9: Pages by mode; Отладка

**Files:**
- Modify: `src/app/page_effects.cpp`, `src/app/page_ai.cpp`, `src/app/page_pack.cpp`, `src/app/page_service.cpp`
- Create: `src/app/page_debug.cpp`

**Interfaces:**
- Consumes: PageHost::mode() (Task 8), Panel::split (Task 6).
- Produces: `std::unique_ptr<Page> makeDebugPage(PageHost&)`.
- [ ] **Step 1:** Effects: Simple mode - the set and the groups, no «Подробнее» buttons, no tier; groups in two
  columns when wide (5 + 5). Ai(Models): the model preview only in Developer mode; sections in two columns when wide.
  Pack: «Текстуры одной карты» and «Сжать» only in Developer mode. Service: Palettes.grf only in Developer mode.
- [ ] **Step 2:** Отладка: the Debug rows (SettingsPage({Debug}) inside), «Открыть журнал», «Папка uaro-graphics»,
  tools: «uaRO Studio» (starts uaro-graphics\uaro-studio.exe), «Сжать пакет» (→ Pack tab), «Собрать Palettes.grf»
  (→ Service), «Превью моделей» (→ Ai tab 1); the note with ~, Ctrl+Shift+F12, Ctrl+Shift+F8.
- [ ] **Step 3:** gate → GATE OK. **Step 4: Commit** `feat: страница «Отладка», строки разработчика только в его
  режиме`.

### Task 10: Главное

**Files:**
- Create: `src/app/page_main.cpp`, `src/app/status_info.{hpp,cpp}` (driver, update, pack, DLSS files - cached)
- Modify: `src/app/page_settings.cpp` (auto setup as `void runAutoSetup(PageHost&)`), `src/app/page_pad.cpp`
  (the switch as `bool padOn(const Model&)`, `void switchPad(PageHost&, bool on)`), strings

**Interfaces:**
- Consumes: Task 4 (MainQuality), Task 8 (buildRow, views), update.hpp (UpdateState).
- Produces: `std::unique_ptr<Page> makeMainPage(PageHost&)`; `struct PackSummary { bool exists; uint64_t pictures;
  uint64_t bytes; }; PackSummary packSummary(const std::wstring& dir)` (size and time decide when to count again).
- [ ] **Step 1:** Картинка: Качество (four items, «Своё» fifth when mainQualityOf is empty), the rows quality.dlss,
  frames.frame_generation, frames.fps_limit, frames.vsync, display.hdr via SettingsPage::buildRow; Прочее:
  sound.engine, the gamepad, frames.battery_saver; «Рекомендуемые настройки», «Что нового в X».
- [ ] **Step 2:** Состояние: driver version and update state, «Проверить обновление»; Direct3D 12 or 11 and the
  card; DLSS files; the game. HD-текстуры: ai.enabled, the pack's pictures and size, «Сделать пакет» (→ Pack tab),
  «Подробнее» (→ Ai).
- [ ] **Step 3:** gate → GATE OK. **Step 4: Commit** `feat: страница «Главное» простого режима`.

### Task 11: Search page

**Files:**
- Create: `src/app/page_search.cpp`
- Modify: `src/app/main_window.cpp`, `src/app/page_settings.cpp` and `src/app/page_effects.cpp` (buildRow, effects
  commands by schema row ids)

**Interfaces:**
- Consumes: Task 3 (findSettings, Hit), Task 8 (buildRow, query in MainWindow).
- Produces: `std::unique_ptr<Page> makeSearchPage(PageHost&, const std::string& query)`.
- [ ] **Step 1:** The field's text (after 250 ms without typing) shows the search page instead of the view: «N
  настроек со словом «…» на M страницах — меняйте прямо здесь», each hit its row in place (SettingsPage or
  EffectsPage buildRow, rowId ids) or its label, «Страница › группа» under it, «На страницу» (ids 3000 + n).
- [ ] **Step 2:** Ctrl+F focuses the field; Esc in it clears it and goes back; Enter opens the first hit's page.
- [ ] **Step 3:** gate → GATE OK. **Step 4: Commit** `feat: поиск по настройкам в шапке`.

### Task 12: Status bar, dots, page reset, «Что нового»

**Files:**
- Modify: `src/app/main_window.{hpp,cpp}`, `src/app/page_settings.cpp`, `src/app/page_effects.cpp`, strings

**Interfaces:**
- Consumes: Task 4 (notesOf), Task 5 (recommended, seenVersion), Task 6 (dot), Task 10 (packSummary, update
  state).
- [ ] **Step 1:** A 26 px strip at the bottom: «Драйвер X · обновлений нет / есть Y», «Пакет: N картинок · S ГБ»,
  the game's state on the right; refreshed with the 2 s timer.
- [ ] **Step 2:** SettingsPage and EffectsPage (the set, the groups) put a dot before rows whose token differs from
  model.recommended(row), its hint «Рекомендуется: …»; resettable views get «Сбросить страницу к рекомендуемым»
  (id 44) and the legend note; the button sets every shown row of the view to its recommended token.
- [ ] **Step 3:** At start (after the statistics question) a version other than seenVersion shows notesOf(
  TEST.<lang>.md of the client folder, kVersion) when usedBefore, then saves it; Главное's button shows them.
- [ ] **Step 4:** gate → GATE OK. **Step 5: Commit** `feat: строка состояния, метки «не как рекомендуется», сброс
  страницы, «Что нового»`.

### Task 13: Notes and memory

- [ ] **Step 1:** `$S/notes/2.6.0.{ru,uk,en}.md`: 2.5.28's items (it never shipped) + this redesign, with what to
  check on Windows.
- [ ] **Step 2:** Memory: project-settings-app-redesign.md → built, untested on Windows; MEMORY.md line.
- [ ] **Step 3:** Final review of the branch (self-review against this plan and the spec).

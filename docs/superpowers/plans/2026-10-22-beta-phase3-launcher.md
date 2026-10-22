# Beta Phase 3: The Launcher «Гра» Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («делай все фазы без остановки», «в инлайн режиме»): executing-plans, no subagents; the final
> review is a self-review.

**Goal:** the settings app becomes the game's launcher: a first page «Гра» with the site's news in the player's
language, the server's online count, the update and a big «Грати» / «Оновити й грати»; a desktop shortcut «uaRO»; a
direct start of uaRO.exe opens «Гра» unless «Запускати гру одразу» is ticked.

**Architecture:** The site gets one read-only JSON answer, `GET /api/public/launcher?lang=uk|ru|en` (Laravel, beside
`/api/public/status`): the online count and the newest published news. Core parses it (`core/launcher`) and keeps the
new view and key (`View::Play`, `Page::Launcher`, `[launcher] direct_start`). The app fetches it on a thread at its
start (`app/launcher`), shows it on `page_play.cpp`, and starts the game through one way (`playNow`): the update first
when one is offered, then `uaRO.exe` with `UARO_LAUNCHER=1` in its environment - the driver's sign that the launcher
started it. The installer makes the «uaRO» shortcut (COM IShellLink, `app/shortcuts`) and moves the game's own desktop
shortcuts aside, the removal puts them back.

**Tech Stack:** C++17, Win32/COM (IShellLinkW), WinHTTP, core/json; PHP 8.4 Laravel (controller, route, PHPUnit
feature test with DatabaseTransactions on the app's database).

**Spec:** `docs/superpowers/specs/2026-10-22-beta-release-design.md` (phase 3), the page QjTrLqpnyyw8qKiUZQ3i24
(«Лаунчер: новости и обновления перед игрой», its mockup), decisions 13 (direct start opens «Гра», «Запускати гру
одразу»), 16 (the shortcut «uaRO» replaces the game's), 18 (the name «uaRO»).

## Global Constraints

- Branch `beta/phase-3` from main, fast-forwarded at the end; gate after every task; releases only on the user's word.
- No connection to the site: no news and no online count shown, «Грати» works.
- The game never goes to the network; the driver only reads its environment and its settings.
- A direct start redirects only when uaro-graphics.exe is there and `[launcher] direct_start = 0` (the default);
  `UARO_LAUNCHER=1` (the launcher's, the update's restart) never redirects.
- Links the launcher opens are the site's only: `https://uaro.kiev.ua/`.
- The site: one new controller and test file, one route line in routes/web.php (backed up first); read-only queries;
  checked by its PHPUnit test (DatabaseTransactions, rolled back) and by curl; nothing else changes.
- Texts uk/ru/en in strings.cpp; the window's title becomes «uaRO».

## Review Focus

1. The site answers slowly or not at all: the app opens at once, «Гра» says the site is not answering, «Грати»
   starts the game - Task 4 `launcher_without_the_site_still_plays`.
2. A player who starts uaRO.exe from an old shortcut or another launcher: «Гра» opens instead of the game and the
   game's process ends cleanly; with the box ticked the game starts as before - Task 6 `the_game_redirects_only...`.
3. The update chosen from «Оновити й грати» restarts the game without opening «Гра» again - Task 5 (the update's
   restart sets `UARO_LAUNCHER`) and manual.
4. A news item's text in Russian or Ukrainian with quotes, new lines, an emoji: shown whole, no garbage - Task 2
   `launcher_reads_the_sites_news`.
5. A player with several desktop shortcuts of the game (their own names): all go aside and come back on removal -
   Task 7 `shortcuts_of_the_game_go_aside_and_come_back`.

---

### Task 1: The site's launcher answer

**Files (site, /var/www/uaro.kiev.ua/private/laravel):** Create `app/Http/Controllers/Public/LauncherController.php`,
`tests/Feature/Public/LauncherTest.php`; Modify `routes/web.php` (after `/api/public/status`). Backup: routes/web.php
into `$S/site-backup/`.

**Interfaces:** `GET /api/public/launcher?lang=uk|ru|en` (any other: uk) → `{"online": int, "peak": int, "news":
[{"title", "excerpt", "date": "YYYY-MM-DD", "pinned": bool, "url": "https://uaro.kiev.ua/<lang>/news/<slug>"}]}` -
at most 5 published news (News::published order), texts by News::pick in that language (uk → app locale ua),
`Cache-Control: public, max-age=60`.

- [ ] **Step 1: Write the failing test** `LauncherTest`: a published news with ua/ru/en titles → `?lang=ru` gives its
  ru title first with its url `/ru/news/<slug>`; a draft is absent; no lang → the ua text and `/uk/` urls; `online`
  and `peak` are integers.
- [ ] **Step 2: Run** `sudo -u web71 php vendor/bin/phpunit tests/Feature/Public/LauncherTest.php` - Expected: FAIL (404).
- [ ] **Step 3: Implement** the controller (invokable, `ServerStatus::snapshot()` for the numbers) and the route.
- [ ] **Step 4: Run** the test - Expected: OK; `curl -s https://uaro.kiev.ua/api/public/launcher?lang=uk` gives JSON.
- [ ] **Step 5: Record** (the site has no git of ours): the files' paths and the backup in the ledger.

### Task 2: Parsing the answer

**Files:** Create `src/core/launcher.hpp/.cpp`, `tests/test_launcher.cpp`.

**Interfaces:**
```cpp
namespace uagfx::launcher {
constexpr const char* kPath = "/api/public/launcher";
struct News { std::string title, excerpt, date, url; bool pinned = false; };
struct Info { int online = -1, peak = -1; std::vector<News> news; };
bool parse(const std::string& json, Info& out, std::string& error);  // a url not of https://uaro.kiev.ua/ -> ""
std::string langToken(settings::Lang l);                              // "uk", "ru", "en"
}
```
- [ ] **Step 1: Write the failing tests** `launcher_reads_the_sites_news` (the answer as Laravel writes it: `З...`
  escapes, a quote, `\n`, an emoji's surrogate pair → UTF-8 whole; two items, pinned first as given; a url of another
  host → ""), `launcher_refuses_what_is_not_its_answer` (an HTML page, `[]`, `{"news": 5}` → false), `langToken`.
- [ ] **Step 2: Run** - Expected: compile error.
- [ ] **Step 3: Implement** over `core/json`.
- [ ] **Step 4: Run** - Expected: PASS.
- [ ] **Step 5: Commit** `feat(core): the site's launcher answer - news in the player's language, the online count`.

### Task 3: The view «Гра» and «Запускати гру одразу»

**Files:** Modify `src/core/settings/navigation.*`, `schema.*`, `strings.*`, `src/core/config.*`, tests
`test_navigation`(or the file holding viewsOf's tests), `test_config.cpp`, `test_schema.cpp`.

**Interfaces:** `View::Play` first in every mode's list (`viewName` → Text::ViewPlay «Гра»); `Page::Launcher`
(`partsOf(Play) = {Launcher}`, `placeOf(Launcher) = {Play, 0}`); `Config::directStart` (bool, default false) from
`launcher.direct_start`; a schema row `{"launcher", "direct_start", Page::Launcher, Check, 0..1, default 0,
Text::LauncherDirect, Text::LauncherDirectHint}`, a restart key; Text::AppTitle → «uaRO».

- [ ] **Step 1: Write the failing tests**: `views_open_on_play` (each mode's first view is Play),
  `config_direct_start_is_off_by_default`, the schema's restart-key list gains `launcher.direct_start`.
- [ ] **Step 2: Run** - Expected: FAIL.
- [ ] **Step 3: Implement**.
- [ ] **Step 4: Run** - Expected: PASS.
- [ ] **Step 5: Commit** `feat(settings): the view «Гра» first, [launcher] direct_start, the name «uaRO»`.

### Task 4: The launcher's state and the way to play

**Files:** Create `src/app/launcher.hpp/.cpp`; Modify `src/app/download.hpp/.cpp` (a path of the site),
`src/app/model.cpp` (`launchGame` sets `UARO_LAUNCHER`), `src/app/modes.cpp` (the update's game restart too),
`tests/fake_windows.*`, `tests/test_launcher_state.cpp`.

**Interfaces:**
```cpp
bool fetchPath(const std::string& path, std::string& out, std::string& error);  // app/download: https://kSite<path>
struct LauncherState { enum Kind { Idle, Asking, Done, Failed } kind = Idle; launcher::Info info; std::string error; };
LauncherState launcherNow(settings::Lang l);  // host-tested
void startLauncherCheck(settings::Lang l);    // on a thread
LauncherState launcherState();
// One way to the game: an offered update first (--update --then-game, this app closes: true returned in `closing`),
// else uaRO.exe with UARO_LAUNCHER=1; "" or why not.
std::wstring playNow(Model& m, settings::Lang l, bool& closing);
```
- [ ] **Step 1: Write the failing tests**: `launcher_state_from_the_site` (fake web path → news), 
  `launcher_without_the_site_still_plays` (no answer → Failed; `playNow` launches the game: fake records the launch
  and `UARO_LAUNCHER`).
- [ ] **Step 2: Run** - Expected: compile error.
- [ ] **Step 3: Implement**; the fake records `launched` (file, args) and the environment's `UARO_LAUNCHER`.
- [ ] **Step 4: Run** - Expected: PASS.
- [ ] **Step 5: Commit** `feat(app): the launcher's news and online count, one way to play`.

### Task 5: The page «Гра»

**Files:** Create `src/app/page_play.cpp`; Modify `src/app/page.hpp`, `main_window.cpp` (the view's page, the
bottom «Грати» through `playNow`, the check at start), `CMakeLists.txt`, strings.

**Interfaces:** `std::unique_ptr<Page> makePlayPage(PageHost& host);` - blocks: «Новини» (cards: title, «закріплено ·
date» or the date, excerpt, «Докладніше →» opening its url; the first card «Що нового в X» after an update, opening the
notes), «Сервер» («Працює · N гравців онлайн» / «Сайт не відповідає»), «Оновлення» («uaRO Graphics X · N МБ» when
offered), the button «Грати» or «Оновити й грати» (with «Після оновлення гра запуститься сама.»), the row
«Запускати гру одразу».
- [ ] **Step 1**: texts added; the gate's "every text is used" is this task's test.
- [ ] **Step 2: Run** the gate - Expected: FAIL (texts unused) before the page uses them, `GATE OK` after.
- [ ] **Step 3: Implement**.
- [ ] **Step 4: Run** the gate - Expected: `GATE OK`.
- [ ] **Step 5: Commit** `feat(app): the page «Гра» - news, the server, the update, Грати`.

### Task 6: A direct start opens «Гра»

**Files:** Modify `src/core/launcher.hpp/.cpp` (the decision), `src/win/real_ddraw.cpp`, `tests/test_launcher.cpp`.

**Interfaces:** `bool launcher::redirect(bool startedByLauncher, bool directStart, bool appThere);`
- [ ] **Step 1: Write the failing test** `the_game_redirects_only_a_direct_start_without_the_box`.
- [ ] **Step 2: Run** - Expected: compile error.
- [ ] **Step 3: Implement**; the driver, first in `decide()` after the log opens: `UARO_LAUNCHER` read (and cleared),
  redirect → start `uaro-graphics.exe` (no arguments: it opens on «Гра»), log, `TerminateProcess`; else the phase-2
  question.
- [ ] **Step 4: Run** tests and the gate - Expected: PASS, `GATE OK`, ddraw.dll imports unchanged.
- [ ] **Step 5: Commit** `feat: a direct start of the game opens «Гра» unless «Запускати гру одразу»`.

### Task 7: The shortcut «uaRO»

**Files:** Create `src/app/shortcuts.hpp/.cpp`; Modify `src/app/installer.cpp` (finishInstall makes it once,
uninstall puts the game's back), `tests/fake_windows.*`, `tests/test_installer.cpp`, `tools/check_exe.py`.

**Interfaces:**
```cpp
struct Shortcut { std::wstring path, target; };
std::vector<Shortcut> desktopShortcuts();  // the user's desktop .lnk files and their targets
bool makeShortcut(const std::wstring& path, const std::wstring& target, const std::wstring& folder,
                  const std::wstring& icon);
std::wstring desktopFolder();
```
- [ ] **Step 1: Write the failing tests** `shortcuts_of_the_game_go_aside_and_come_back` (two desktop shortcuts to
  uaRO.exe and one to another program → after finishInstall: uaRO.lnk to uaro-graphics.exe, the two in
  uaro-graphics\shortcuts\, the other untouched; uninstall → the two back, uaRO.lnk gone),
  `the_shortcut_is_made_once` (deleted by the player → not made again by an update).
- [ ] **Step 2: Run** - Expected: compile error.
- [ ] **Step 3: Implement** (IShellLinkW + IPersistFile; SHGetFolderPathW CSIDL_DESKTOPDIRECTORY; the once-marker
  `uaro-graphics\shortcuts\made.txt`).
- [ ] **Step 4: Run** tests and the gate - Expected: PASS, `GATE OK`.
- [ ] **Step 5: Commit** `feat(app): the desktop shortcut «uaRO» instead of the game's`.

### Task 8: Notes and checks

- [ ] `$S/notes/2.11.0.{ru,uk,en}.md`: «Гра» with the site's news and online count, «Грати» / «Оновити й грати»,
  the shortcut «uaRO», a direct start opening «Гра», «Запускати гру одразу»; checks: a news item published on the
  site appears in all three languages; no connection → «Грати» works; the old shortcut / direct start → «Гра»; the box
  ticked → the game as before (with the update question); «Оновити й грати» restarts the game without «Гра».
- [ ] Gate, self-review against the Review Focus, main fast-forwarded.

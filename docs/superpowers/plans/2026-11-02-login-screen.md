# Login Screen («Вхід у гру») Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans — the owner chose inline execution
> («делай все без остановки, в инлайн режиме»): a beta per task (or task group) via `$S/ship-beta.sh`, releases only
> on «выпускай». Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** build the agreed login concept: the single server skipped, refusals and typing mistakes explained in words,
server status, language and news on the login screen, three views («Класичний», «Полиця», «Продовжити») over a
self-filling account shelf, the optional site account (all game logins and characters) and registration from the
client.

**Architecture:** driver parts follow the existing cards: `core/<part>` holds rules, layout, paint, words and hit tests
(host-tested), `win/<part>` holds the game facts (windows, fields, buttons pressed through the game's own SendMsg),
GDI words and threads. The driver reaches uaro.kiev.ua through WinHTTP and protects secrets with DPAPI, both loaded
lazily, so its four imports stay. The site gets a client API under `/api/client/*` with a token per computer, built
on the cabinet's services, staged in the sandbox `$S/site_db/app` and deployed by the owner.

**Tech Stack:** C++20 (MinGW cross build `build-win`, host tests `build-host/uagfx_tests`), Win32 GDI, WinHTTP,
DPAPI; Laravel (PHP 8) on MariaDB; Vue 3 + TypeScript (cabinet SPA).

**Spec:** Claude Docs «Вход в игру: концепция» https://claude.ai/artifact/2Ndw9j7VmCQWQP1nExJDZr (rev 60, 2026-11-02)
and the owner's decisions of 2026-11-02 (memory `project-login-screen-concept.md`).

## Global Constraints

- Betas only (`$S/ship-beta.sh V "msg"`), never main; releases only on the owner's «выпускай».
- Driver imports stay GDI32/KERNEL32/msvcrt/USER32: `winhttp.dll`, `crypt32.dll`, `shell32.dll`, `ws2_32` functions
  come through LoadLibrary/GetProcAddress.
- Gate per beta: host tests green, `build-win` builds, no added line over 120 characters, `tools/check_text_names.py`,
  `tools/check_layout.py`.
- Every new switch: config field + parse, live config, schema row under «Інтерфейс», console list, commented entry in
  `dist/uaro-graphics.ini`, strings uk/ru/en, a config test.
- Words live in `src/core/settings/strings.cpp` (uk/ru/en); names quoted to the owner from the uk column.
- Passwords: never in logs or reports, never drawn in clear except while the eye is held, stored only DPAPI-protected;
  the game's password field is the one with the mask (`logincard::fieldPlaces`, 3.0.11-beta.170), never an offset.
- No captcha anywhere; no new registration limits (the site keeps its existing `throttle:6,1` on auth routes).
- The window stays centred as now; default view «Полиця»; «Одразу в гру» on for every account; «Додати на полицю
  після входу» on by default; the shelf lives in the Windows profile (`%LOCALAPPDATA%\uaroGraphics`).
- The game server and the game DB are not touched; site data changes only through deployed site code; the sandbox
  uses its own MariaDB (port 3307, `$S/site_db/mdb`); deploy only by the owner running the prepared script.

## Facts (verified 2026-11-02 in `/root/BornRok/winEXE/uaRO.exe` and the live site code)

- Login window 3: mgr+0x88, class 0x697A70, SendMsg 0x457280; account edit +0x60, password edit +0x64 (edit +0x50 =
  `'*'`), remember +0x68; buttons 0x78 «Увійти», 0x9B «Вийти», 0x93 the box. Edit SetText = vtable+0x98 (thiscall,
  `const char*`), GetText 0x443300; manager SetFocus 0x4CE510 (thiscall(mgr, ctl)), focus at mgr+0x6C.
- Server select, both lists (service 0x2723 «NUMACCOUNT», char servers 0x2713 «NUMSERVER»): window 2, class 0x6979D8,
  SendMsg 0x4563C0; list control +0x60 (items +0x58/+0x5C, 16 bytes; chosen +0x64); OK 0x76.
- Login refusal 0x006a: FUN_005A40C0 (thiscall(mode, packet)), first bytes `55 8B EC 81 EC 00 01 00 00`; code =
  packet[2]; code 6 carries the date at packet+3. Code → msgstringtable line: 0→6, 1→0x10B, 2→8, 4→0x10A, 5→0x136,
  6→0x1C1, 7→0x1B7, 99→0x170, other→9. Code 1 counts to three and then asks (ErrorMsg text 0x6D8258) to open
  ragnarok.co.kr; code 0x10 asks with 0x6D8210 — both answered «no» silently.
- ErrorMsg 0x4CEDF0 is modal; our detour lives in `src/win/msg_box.cpp` (`errorMsg`).
- Character select window 5 (mgr+0x20C): `src/win/char_select.cpp` `choose(place)`, `press(Enter)` = 0x76, pages
  0x96/0x97; CHARACTER_INFO via the mode's message 8 (name +0x4A, job +0x34, hair +0x36, hair colour +0x46, level
  +0x3A); `headpicture::make(sex, hair, hairColour, side, picture)` draws a head without the game.
- Game windows share `Local\uaro-graphics-windows-2` (`core/game_windows.hpp`, 96-byte entries — layout frozen).
- Language: `[utility] language` in uaro-graphics.ini, read once at start (`real_ddraw.cpp` → `startup().lang`).
- The settings app's watcher fetches site data into `uaro-graphics\reference` (`src/app/reference_pack.cpp`:
  `kOnlinePath = "/api/public/online.json"`, which carries `"online"`).
- Site: `App\Services\GameAccountService` (`createLogin`, `linkExisting`, `changePassword`, `linkedAccounts`);
  `Cabinet\CharacterController::index` builds the characters list (char_num, class, base_level, last_map, online);
  `Cabinet\AuthController::register` = the two-password form; `bot_tokens` = the token-per-user precedent; cabinet
  SPA = Vue 3 in `frontend/cabinet`; deploys follow `$S/site_db/deploy-ping.sh` (staged files, BASELINE.sha256).

## Review Focus

1. A saved password changed elsewhere: a refusal on a one-click login marks the card «Пароль не підходить», stops
   «Одразу в гру» for that attempt and never retries by itself — test in Task C3.
2. The same account in two game windows: the second shows «вже в грі» and does not log in by itself — Task C7.
3. The login server down: one probe every 8 s; a login is retried only after the player pressed «Увійти», never a
   storm of boxes — Task B3.
4. `accounts.dat` unreadable (another PC or Windows user, a cut file): an empty shelf, the file kept as
   `accounts.dat.bad`, a log line without logins — never a crash or a lost file — Task C1.
5. The remembered character renamed or deleted: «Одразу в гру» stops at the character select — Task C3.

## File Structure

Driver, new:
- `src/core/login_errors.{hpp,cpp}` — refusal code → field, words, action.
- `src/core/login_hints.{hpp,cpp}` — Caps Lock and layout hints, Latin guess of Cyrillic typing.
- `src/core/server_status.{hpp,cpp}` — the status line's words and colour, the probe's pace.
- `src/core/reference_now.{hpp,cpp}` — the news line from `reference\now.json`.
- `src/core/login_chrome.{hpp,cpp}` — the bottom bar shared by the views: news, status, «Вигляд», language.
- `src/core/login_view.hpp` — `enum class View { Classic, Shelf, Continue }` and its ini tokens.
- `src/core/accounts.{hpp,cpp}` — the shelf's data, JSON, upsert/remove/move, last character.
- `src/core/login_shelf.{hpp,cpp}` — «Полиця» layout, cards, card menu, hit tests, words, paint.
- `src/core/login_continue.{hpp,cpp}` — «Продовжити» layout, words, paint.
- `src/core/class_art.{hpp,cpp}` — job id → class picture file.
- `src/core/game_logins.hpp` — the logged-in accounts block shared by game windows.
- `src/core/site_api.{hpp,cpp}` — site requests and answers (JSON).
- `src/core/register_form.{hpp,cpp}` — the registration rules (the site's).
- `src/core/shelf_merge.{hpp,cpp}` — local accounts + site logins/characters → what the shelf shows.
- `src/win/login_refusal.{hpp,cpp}` — detour of FUN_005A40C0.
- `src/win/server_probe.{hpp,cpp}` — TCP connect probe thread.
- `src/win/dpapi.{hpp,cpp}`, `src/win/accounts_store.{hpp,cpp}` — protected file I/O.
- `src/win/login_chrome.{hpp,cpp}`, `src/win/login_shelf.{hpp,cpp}`, `src/win/login_continue.{hpp,cpp}`.
- `src/win/game_logins.{hpp,cpp}` — writes/reads the shared logins block.
- `src/win/site_http.{hpp,cpp}`, `src/win/site_key.{hpp,cpp}`, `src/win/site_signin.{hpp,cpp}`,
  `src/win/register_card.{hpp,cpp}`.
- Tests: `tests/test_<module>.cpp` for every core module.

Driver, modified: `core/pick_card`, `core/login_card`, `win/server_card.cpp`, `win/login_card.cpp`,
`win/msg_box.cpp`, `win/char_select.cpp`, `win/real_ddraw.{hpp,cpp}` (live language), `core/reference_live`,
`core/config`, `core/live_config`, `core/settings/{schema,strings}`, `win/console.cpp`, `win/window_stack.cpp`,
`pad/win/runtime.cpp`, `win/context.cpp`, `win/windows_draw.cpp`, `core/layout.hpp`, `app/reference_pack.cpp`,
`app/page_settings.cpp`, `dist/uaro-graphics.ini`, the package script (class pictures).

Site (sandbox `$S/site_db/app`), new: migration `2026_11_03_100001_create_client_devices_tables.php`; models
`ClientDevice`, `ClientDeviceCode`; middleware `ClientToken`; services `Registration`, `Cabinet\CharacterList`;
controllers `Api\Client\{AuthController, DeviceController, AccountsController, RegisterController,
LoginFreeController}`, `Cabinet\DevicesController`, `Api\Public\NowController`; cabinet pages `DevicePage.vue` and
the «Пристрої» block of the security page; `$S/site_db/checks/clientapi.php`; `deploy-login.sh`, `rollback-login.sh`.
Modified: `routes/api.php`, `routes/web.php` (the `/account/device` page route if the SPA needs one),
`Cabinet\AuthController` and `Cabinet\CharacterController` (call the extracted services), `bootstrap/app.php`
(middleware alias).

## Per-beta routine (R)

Every shipping step below means:
- [ ] `cmake --build build-host -j8 && ./build-host/uagfx_tests` → `N tests, 0 failed checks`.
- [ ] `cmake --build build-win -j8` → exit 0 (rerun `cmake -S . -B build-win` / `build-host` when files were added).
- [ ] Long-line check on `git diff HEAD -U0` + new files → nothing over 120; `python3 tools/check_text_names.py` and
      `python3 tools/check_layout.py` → OK.
- [ ] Notes `$S/notes/V.{uk,ru,en}.md`; `git add dist/uaro-graphics.ini`; run
      `bash $S/ship-beta.sh V "msg" > $S/shipNNN.log 2>&1` in the background; wait for `SHIPPED|FAIL`; no writes to
      `src/` while it runs.
- [ ] Memory: append the beta to `project-login-screen-concept.md`.

---

## Part A — skip the single-server list (1 beta)

### Task 1 (A1): the only server chosen and confirmed at once

**Files:** Modify `src/core/pick_card.{hpp,cpp}`, `src/win/server_card.cpp`; Test `tests/test_pick_card.cpp`.

**Interfaces:** Produces `int pickcard::autoChoice(size_t entries, int chosen)` — the row to press OK on at once:
0 when there is exactly one entry, −1 otherwise (`chosen` is ignored for one entry).

- [ ] **Step 1: failing test**
```cpp
TEST(pick_card_single_entry_goes_on_by_itself) {
    CHECK(pickcard::autoChoice(1, -1) == 0);
    CHECK(pickcard::autoChoice(1, 0) == 0);
    CHECK(pickcard::autoChoice(2, 0) == -1);
    CHECK(pickcard::autoChoice(0, -1) == -1);
}
```
- [ ] **Step 2:** build host → FAIL (`autoChoice` not declared).
- [ ] **Step 3:** implement; in `servercardwin::frame`, once per window instance (`s.window != window` resets a
  `pressed` flag): `autoChoice(entries.size(), selected) == 0` → `choose(s, 0)` and `press(s, pc::kOkButton)`, log
  `servers: the only server - chosen at once`; the card is not drawn for that window.
- [ ] **Step 4:** host tests PASS; win builds.
- [ ] **Step 5:** ship (R) with notes «Вибір сервера пропускається, коли сервер один».

## Part B — words, hints, status, language, news (3 betas)

### Task 2 (B1): refusals in words, the game's boxes answered for us

**Files:** Create `src/core/login_errors.{hpp,cpp}`, `src/win/login_refusal.{hpp,cpp}`; Modify `src/win/msg_box.cpp`,
`src/core/login_card.{hpp,cpp}`, `src/win/login_card.cpp`, `src/core/settings/strings.{hpp,cpp}`; Test
`tests/test_login_errors.cpp`, `tests/test_login_card.cpp`.

**Interfaces:**
- Produces `namespace loginerr`: `enum class Field : uint8_t { None, Account, Password }`, `enum class Action :
  uint8_t { None, CreateAccount, ResetPassword }`, `struct Refusal { Field field; settings::Text text; Action action;
  bool inlineOnly; }`, `Refusal refusal(int code)`, `std::string words(const Refusal&, settings::Lang, const
  std::string& login, const std::string& until)`.
- Produces `loginrefusal::pending()` → `std::optional<int>` (the last code, cleared when read by the card) and
  `loginrefusal::until()`.
- `logincard::Fields` gains `std::string error; loginerr::Field errorField; loginerr::Action errorAction`.

- [ ] **Step 1: failing tests**
```cpp
TEST(login_errors_codes_to_fields) {
    using F = loginerr::Field;
    CHECK(loginerr::refusal(0).field == F::Account && loginerr::refusal(0).inlineOnly);
    CHECK(loginerr::refusal(0).action == loginerr::Action::CreateAccount);
    CHECK(loginerr::refusal(1).field == F::Password && loginerr::refusal(1).inlineOnly);
    CHECK(loginerr::refusal(1).action == loginerr::Action::ResetPassword);
    CHECK(loginerr::refusal(2).text == settings::Text::LoginErrExpired && !loginerr::refusal(2).inlineOnly);
    CHECK(loginerr::refusal(4).text == settings::Text::LoginErrMaintenance);
    CHECK(loginerr::refusal(6).text == settings::Text::LoginErrBannedUntil);
    CHECK(loginerr::refusal(7).text == settings::Text::LoginErrJammed);
    CHECK(loginerr::refusal(99).text == settings::Text::LoginErrRemoved);
    CHECK(loginerr::refusal(3).text == settings::Text::LoginErrRejected);  // any other code
}
TEST(login_errors_words_name_the_login) {
    const auto w = loginerr::words(loginerr::refusal(0), settings::Lang::Uk, "kwbe", "");
    CHECK(w.find("kwbe") != std::string::npos);
    CHECK(loginerr::words(loginerr::refusal(6), settings::Lang::En, "x", "2026-11-12 10:00").find("2026-11-12")
          != std::string::npos);
}
```
- [ ] **Step 2:** build host → FAIL (header missing).
- [ ] **Step 3:** implement `login_errors`; strings `LoginErrUnknownId` («Логіна {1} немає. Перевірте написання
  або»), `LoginErrPassword` («Пароль не підходить до {1}.»), `LoginErrExpired`, `LoginErrMaintenance`,
  `LoginErrOldExe`, `LoginErrBannedUntil` («Вхід заборонено до {1}.»), `LoginErrJammed`, `LoginErrRemoved`,
  `LoginErrRejected`, `LoginCreateAccount` («Створити акаунт»), `LoginResetPassword` («Відновити пароль на сайті»).
- [ ] **Step 4:** win: `login_refusal.cpp` detours FUN_005A40C0 (byte check as `msg_box.cpp` `detour`), records
  `code` and the date (code 6), then calls the original. `msg_box.cpp errorMsg`: when `[interface] login` is on and a
  refusal is pending with `inlineOnly` → return 0x76 without calling the original; text pointer 0x6D8258 or 0x6D8210 →
  return 0x77 without the box; other refusals → the card's words in place of the game's. `login_card` shows the error
  line under the named field (red border) with the action link (create: the site's register page until Task D5;
  reset: `https://uaro.kiev.ua/account/forgot`). Log: `login: the server refused (code N) - shown in words`.
- [ ] **Step 5:** add to `test_login_card.cpp`: `login_card_error_line_under_its_field` — with `f.error` set and
  `errorField == Password`, `words()` holds the error text inside `L.password`'s column below the field, and `paint()`
  outlines the field red (pixel at the field's border has red > green).
- [ ] **Step 6:** host PASS, win builds; ship together with B2.

### Task 3 (B2): Caps Lock and keyboard layout hints

**Files:** Create `src/core/login_hints.{hpp,cpp}`; Modify `src/win/login_card.cpp`, `src/core/login_card.cpp`;
Test `tests/test_login_hints.cpp`.

**Interfaces:** `namespace loginhint`: `enum class Kind : uint8_t { None, CapsLock, Layout }`, `struct Hint { Kind
kind; std::string guess; }`, `Hint forAccount(const std::string& utf8, bool caps)`, `Hint forPassword(bool caps)`,
`std::string latinGuess(const std::string& utf8)` (Ukrainian and Russian ЙЦУКЕН letters → the QWERTY keys; other
characters kept).

- [ ] **Step 1: failing test**
```cpp
TEST(login_hints_layout_and_caps) {
    CHECK(loginhint::latinGuess("лцуи") == "kweb");
    CHECK(loginhint::latinGuess("ЛЦУИ") == "KWEB");
    CHECK(loginhint::latinGuess("kweb_1") == "kweb_1");
    CHECK(loginhint::forAccount("лцуи", false).kind == loginhint::Kind::Layout);
    CHECK(loginhint::forAccount("лцуи", false).guess == "kweb");
    CHECK(loginhint::forAccount("kweb", true).kind == loginhint::Kind::CapsLock);
    CHECK(loginhint::forAccount("kweb", false).kind == loginhint::Kind::None);
    CHECK(loginhint::forPassword(true).kind == loginhint::Kind::CapsLock);
}
```
- [ ] **Step 2:** FAIL; **Step 3:** implement (letters: й q, ц w, у e, к r, е t, н y, г u, ш i, щ o, з p, х [, ї ],
  ъ ], ф a, і s, ы s, в d, а f, п g, р h, о j, л k, д l, ж ;, є ', э ', я z, ч x, с c, м v, и b, т n, ь m, б ,, ю .,
  ґ \`); win reads `GetKeyState(VK_CAPITAL) & 1` for the focused field; strings `LoginHintCaps` («Увімкнено Caps
  Lock»), `LoginHintLayout` («Розкладка {1} — логін зазвичай латиницею ({2}?)»).
- [ ] **Step 4:** PASS; ship B1+B2 (R), notes «Помилки входу словами й підказки під полями».

### Task 4 (B3): server status line and the quiet retry

**Files:** Create `src/core/server_status.{hpp,cpp}`, `src/win/server_probe.{hpp,cpp}`; Modify `src/core/reference_live.
{hpp,cpp}`, `src/win/msg_box.cpp`, `src/win/login_card.cpp`; Test `tests/test_server_status.cpp`,
`tests/test_reference_live.cpp`.

**Interfaces:**
- `reflive::Online` gains `int total = -1` (root `"online"` of online.json).
- `namespace srvstatus`: `enum class State : uint8_t { Unknown, Up, Down }`, `constexpr int kProbeSec = 8`,
  `struct Line { std::string text; uint32_t rgb; }`, `Line line(State, int pingMs, int online, int retryIn,
  settings::Lang)`, `bool retryLogin(bool lastFailedToConnect, bool playerPressed, int secsSinceFail)`.
- `serverprobe::start(const std::string& host, int port)`, `serverprobe::state()` → `{State, pingMs}`; a worker thread
  connects every `kProbeSec` while the login screens are up, stops in the world.

- [ ] **Step 1: failing tests**
```cpp
TEST(server_status_line_words) {
    const auto up = srvstatus::line(srvstatus::State::Up, 52, 157, 0, settings::Lang::Uk);
    CHECK(up.text.find("157") != std::string::npos && up.text.find("52") != std::string::npos);
    const auto down = srvstatus::line(srvstatus::State::Down, -1, -1, 8, settings::Lang::Uk);
    CHECK(down.text.find("8") != std::string::npos && down.rgb != up.rgb);
    CHECK(srvstatus::line(srvstatus::State::Up, 52, -1, 0, settings::Lang::En).text.find("online") ==
          std::string::npos);  // no count when the watcher has none
}
TEST(server_status_retry_only_after_a_press) {
    CHECK(!srvstatus::retryLogin(true, false, 30));
    CHECK(!srvstatus::retryLogin(true, true, 5));
    CHECK(srvstatus::retryLogin(true, true, 8));
    CHECK(!srvstatus::retryLogin(false, true, 8));
}
TEST(reference_live_reads_the_total) { /* online.json with "online":157 → total == 157; without → -1 */ }
```
- [ ] **Step 2:** FAIL; **Step 3:** implement. The login address: research step — find the globals FUN_004F3A10
  fills for the chosen connection (address string, port); fallback: parse `data\sclientinfo.xml` in the client
  folder (`<address>`, `<port>`). `msg_box.cpp`: the box «Failed to Connect to Server.» (the text of msgstringtable 1,
  compared by words) while the login card is on → answered 0x76 at once, the card shows the Down line and presses
  0x78 again when `retryLogin` says so; Esc on the card stops the retry. Strings `LoginStatusUp` («Сервер працює ·
  {1} гравців онлайн · пінг {2} мс»), `LoginStatusUpNoCount`, `LoginStatusDown` («Сервер не відповідає — пробуємо ще
  раз через {1} с»). Switch `[interface] login_status` (default 1) with the full switch set (Global Constraints).
- [ ] **Step 4:** PASS; ship (R), notes «Рядок стану сервера на екрані входу».

### Task 5 (B4): language on the login screen

**Files:** Create `src/core/login_chrome.{hpp,cpp}`, `src/win/login_chrome.{hpp,cpp}`; Modify `src/win/real_ddraw.
{hpp,cpp}`, `src/core/live_config.cpp`, `src/win/login_card.cpp`; Test `tests/test_login_chrome.cpp`,
`tests/test_live_config.cpp`.

**Interfaces:**
- `namespace loginchrome`: `struct Layout { float s; panel::Box news, status, viewSeg[3], langSeg[3]; }`,
  `Layout layout(int targetW, int targetH, float scale)`, `enum class Part : uint8_t { None, News, View0, View1,
  View2, Lang0, Lang1, Lang2 }`, `Part hitTest(const Layout&, float x, float y)`, `std::vector<Words> words(...)`,
  `void paint(panel::Picture&, const Layout&, const Show&)`.
- `win::setLanguage(settings::Lang)` and `win::language()` (atomic; `startup().lang` callers move to `language()`),
  a bump of `uistyle::generation()` on change (add `uistyle::bump()` if there is none); live config: `utility.language`
  change → `setLanguage`.

- [ ] **Step 1: failing tests** — `login_chrome_bottom_bar_places`: at 1920×1080 scale 1, `langSeg[2]` (ENG) ends
  ≤ 1920−24, `status` is centred (|centre − 960| ≤ 2), `viewSeg[0]` starts ≥ 24 at the left, nothing overlaps;
  `hitTest` returns `Lang1` at `langSeg[1]`'s centre. `live_config_language_change_is_told`: an ini change of
  `utility.language` from `uk` to `en` reports the language changed.
- [ ] **Step 2:** FAIL; **Step 3:** implement; the click writes `[utility] language = uk|ru|en` through the ini
  writer the console uses and calls `setLanguage`; our cards re-render (generation bump); the game's own texts are not
  touched (said in the notes).
- [ ] **Step 4:** PASS; ship with B5.

### Task 6 (B5): news line (client part)

**Files:** Create `src/core/reference_now.{hpp,cpp}`; Modify `src/app/reference_pack.cpp`, `src/core/layout.hpp`,
`src/win/login_chrome.cpp`; Test `tests/test_reference_now.cpp`. Site endpoint: Task D1 (`GET
/api/public/now.json?lang=ua|ru|en`).

**Interfaces:** `namespace refnow`: `struct Now { std::string title, url; long long startsAt = 0, endsAt = 0; }`,
`bool parse(const std::string& json, Now& out)` (false on `{"item":null}`); `layout::kReferenceNow =
"uaro-graphics\\reference\\now.json"`; `reference::now()` in `win/reference_load` (same pattern as `online()`).

- [ ] **Step 1: failing test** — `reference_now_reads_an_item_or_none`: `{"item":{"kind":"event","title":"Полювання
  на MVP","url":"https://uaro.kiev.ua/ua/events/7","starts_at":1762100000,"ends_at":1762110000}}` → title and url;
  `{"item":null}` → false; garbage → false.
- [ ] **Step 2:** FAIL; **Step 3:** implement; the watcher fetches `kNowPath` with the reference refresh (lang from
  `utility.language`: uk→ua); the chrome draws one line when `parse` is true and `[interface] login_news` (default 1,
  full switch set) is on; a click opens the url (ShellExecuteW via lazy shell32, as `login_card.cpp` does).
- [ ] **Step 4:** PASS; ship B4+B5 (R), notes «Мова й новини на екрані входу».

## Part C — three views and the account shelf (4 betas)

### Task 7 (C1): the account store

**Files:** Create `src/core/accounts.{hpp,cpp}`, `src/win/dpapi.{hpp,cpp}`, `src/win/accounts_store.{hpp,cpp}`;
Test `tests/test_accounts.cpp`.

**Interfaces:**
- `namespace accounts`: `struct LastChar { std::string name, job, map; int jobId = 0, level = 0, slot = -1, sex = 0,
  hair = 0, hairColour = 0; }`, `struct Account { std::string login, password, label; bool keepPassword = true,
  autoEnter = true, refused = false; LastChar last; long long usedAt = 0; }`, `struct Shelf { std::vector<Account>
  list; bool addAfterLogin = true; }`, `std::string toJson(const Shelf&)`, `bool fromJson(const std::string&, Shelf&)`,
  `void upsert(Shelf&, const std::string& login, const std::string& password, long long now)`, `bool remove(Shelf&,
  const std::string& login)`, `bool move(Shelf&, size_t index, int delta)`, `void recordChar(Shelf&, const
  std::string& login, const LastChar&, long long now)`, `uint64_t loginHash(const std::string& login)` (lower-case
  login through `hash64`).
- `dpapi::protect(const std::string& plain, std::string& blob)`, `dpapi::unprotect(const std::string& blob,
  std::string& plain)` (lazy crypt32 `CryptProtectData`/`CryptUnprotectData`, `CRYPTPROTECT_UI_FORBIDDEN`).
- `accountsstore::shelf()` → `accounts::Shelf&`, `accountsstore::save()`; file
  `%LOCALAPPDATA%\uaroGraphics\accounts.dat` (DPAPI blob of `toJson`), written atomically.

- [ ] **Step 1: failing tests**
```cpp
TEST(accounts_round_trip_and_upsert) {
    accounts::Shelf s;
    accounts::upsert(s, "kweb", "secret1", 100);
    accounts::upsert(s, "kweb_shop", "secret2", 200);
    accounts::upsert(s, "KWEB", "secret3", 300);  // same login, any case: password and time updated, order kept
    CHECK(s.list.size() == 2 && s.list[0].password == "secret3" && s.list[0].usedAt == 300);
    s.list[0].label = "Основний";
    s.list[0].autoEnter = false;
    accounts::Shelf back;
    CHECK(accounts::fromJson(accounts::toJson(s), back) && back.list.size() == 2);
    CHECK(back.list[0].label == "Основний" && !back.list[0].autoEnter && back.addAfterLogin);
}
TEST(accounts_bad_input_gives_an_empty_shelf) {
    accounts::Shelf s;
    CHECK(!accounts::fromJson("{\"list\":[{\"login\":1}", s) && s.list.empty());
}
TEST(accounts_move_remove_and_last_character) { /* move(0,+1) swaps; remove("kweb") true then false;
    recordChar sets last.name/slot/map and usedAt */ }
```
- [ ] **Step 2:** FAIL; **Step 3:** implement; `accountsstore` on a failed unprotect or parse renames the file to
  `accounts.dat.bad` (replacing an older one) and logs `login: the saved accounts could not be read - kept as
  accounts.dat.bad` (no logins in any log line).
- [ ] **Step 4:** PASS (no ship alone — ships with C2/C3).

### Task 8 (C2): added after login, last character, the toast

**Files:** Modify `src/win/login_card.cpp`, `src/win/char_select.cpp`, `src/core/login_card.{hpp,cpp}`, strings;
Test `tests/test_login_card.cpp`.

**Interfaces:** `logincard::Layout` gains `addShelf` (tick + words «Додати на полицю після входу»); `logincard::Part`
gains `AddShelf`; `logincard::Fields` gains `bool addShelf = true`; `loginwin::loggedIn()` → the login of the
accepted session (empty before); `charselectwin::entered()` → the `accounts::LastChar` of the character entered (empty
slot −1 before).

- [ ] **Step 1: failing test** — `login_card_add_to_shelf_tick`: the layout has `addShelf` under `remember`, the hit
  test returns `Part::AddShelf` there, `words()` contains «Додати на полицю після входу», and with
  `Fields.addShelf = true` the tick is painted gold.
- [ ] **Step 2:** FAIL; **Step 3:** implement. At «Увійти» (our button or the game's Enter: the card watches the
  window disappear after a press of 0x78) the card keeps the account and password read from the masked fields in
  memory only; when the character select appears (login accepted) and the tick is on → `accounts::upsert` + save +
  a toast «{1} додано на полицю · Скасувати» for 6 s (Скасувати removes it again); a refusal clears the kept
  password. On entering the world, `recordChar` with the slot's CHARACTER_INFO and the map name.
- [ ] **Step 4:** PASS; ship C1+C2 (R), notes «Акаунти самі додаються на полицю після входу».

### Task 9 (C3): one-click login and «Одразу в гру»

**Files:** Modify `src/win/login_card.cpp`, `src/win/char_select.cpp`, `src/core/accounts.{hpp,cpp}`; Test
`tests/test_accounts.cpp`.

**Interfaces:** `accounts::Plan accounts::planFor(const Account&, bool shiftHeld, bool busyElsewhere)` with `struct
Plan { bool fillPassword; bool enterAtOnce; bool autoCharacter; }`; `accounts::markRefused(Shelf&, const std::string&
login)` sets `refused` (shown on the card until a successful login); `int accounts::findSlot(const
std::vector<std::string>& slotNames, const std::string& name)` → the slot or −1; `loginwin::loginWith(const
accounts::Account&)`; `charselectwin::enterByName(const std::string& name)` → false when no slot carries the name.

- [ ] **Step 1: failing tests**
```cpp
TEST(accounts_plan_for_one_click) {
    accounts::Account a;
    a.login = "kweb"; a.password = "p"; a.last.name = "Raphael";
    auto p = accounts::planFor(a, false, false);
    CHECK(p.fillPassword && p.enterAtOnce && p.autoCharacter);
    CHECK(!accounts::planFor(a, true, false).autoCharacter);   // Shift: stop at the character select
    CHECK(!accounts::planFor(a, false, true).enterAtOnce);     // busy in another window
    a.password.clear();
    CHECK(!accounts::planFor(a, false, false).enterAtOnce);    // no password: the cursor goes to the password
    a.password = "p"; a.autoEnter = false;
    CHECK(!accounts::planFor(a, false, false).autoCharacter);
}
TEST(accounts_refusal_stops_the_next_auto_try) {
    accounts::Shelf s; accounts::upsert(s, "kweb", "old", 1);
    accounts::markRefused(s, "kweb");
    CHECK(s.list[0].refused && !accounts::planFor(s.list[0], false, false).enterAtOnce);
    accounts::upsert(s, "kweb", "new", 2);
    CHECK(!s.list[0].refused);
}
TEST(accounts_saved_character_found_by_name_only) {
    const std::vector<std::string> slots{"Raphael", "", "Ezzy", "Mira"};
    CHECK(accounts::findSlot(slots, "Ezzy") == 2);
    CHECK(accounts::findSlot(slots, "Raphaell") == -1);  // renamed or deleted: wait at the character select
    CHECK(accounts::findSlot(slots, "") == -1);
}
```
- [ ] **Step 2:** FAIL; **Step 3:** implement. `loginWith`: `SetFocus(mgr, 0)`, SetText(+0x60, login),
  SetText(+0x64, password) through vtable+0x98, press 0x78 — only when `fieldPlaces` is known. «Одразу в гру»: the
  character select card, on its first frame after a one-click login, calls `enterByName(last.name)` (`findSlot` over
  the pages' names, turning pages
  with 0x96/0x97 as needed); false → stays, log `characters: the saved character is not on this account - waiting`.
  A refusal (Task B1) → `markRefused` + the card shows «Пароль не підходить» with a password field.
- [ ] **Step 4:** PASS; ship with C4.

### Task 10 (C4): «Полиця», first launch, card menu

**Files:** Create `src/core/login_shelf.{hpp,cpp}`, `src/win/login_shelf.{hpp,cpp}`; Modify `src/win/login_card.cpp`,
`src/win/window_stack.cpp`, `src/win/context.cpp`, `src/win/windows_draw.cpp`, strings; Test
`tests/test_login_shelf.cpp`.

**Interfaces:** `namespace loginshelf`: `struct Card { std::string label, name, job, map, login; int level = 0;
bool busy = false, refused = false, hasPassword = true; const panel::Picture* art = nullptr; }`, `struct Layout {
float s; int x, y, w, h; std::vector<panel::Box> cards; panel::Box add, other, create, exit, title; int first = 0; }`,
`Layout layout(int targetW, int targetH, float scale, size_t cards, int first)` (four cards visible, ◀ ▶ beyond),
`enum class Part : uint8_t { None, Card, Menu, Add, Other, Create, Exit, Left, Right }`, `struct Hit { Part part;
int index; }`, `Hit hitTest(...)`, `bool classicInstead(size_t accounts)` (true for 0), menu:
`std::vector<MenuItem> menu(const Card&)` with items `AutoEnter, KeepPassword, Label, Edit, MoveLeft, MoveRight,
Remove`.

- [ ] **Step 1: failing tests** — `login_shelf_places`: 3 cards + add at 1920×1080 → centred row (|centre − 960| ≤ 2),
  equal gaps, the row's top under the logo's foot (y ≥ 300 at scale 1); 6 cards → 4 visible and `Right` hit at the
  right arrow. `login_shelf_first_launch_is_classic`: `classicInstead(0)` true, `classicInstead(1)` false.
  `login_shelf_menu_items`: a card with a password lists all seven items; `refused` → the card's words contain
  «Пароль не підходить».
- [ ] **Step 2:** FAIL; **Step 3:** implement; win draws the shelf over the login screen when the view is Shelf and
  the shelf is not empty (the game's login window stays off the screen as the classic card holds it); a click on a
  card → `loginWith`; RMB or ⋯ → the menu; «Інший акаунт» → the classic card; first launch → the classic card with
  the line «Полиця ще порожня: після входу акаунт з'явиться на ній, далі — вхід одним кліком».
- [ ] **Step 4:** PASS; ship C3+C4 (R), notes «Полиця акаунтів: вхід одним кліком».

### Task 11 (C5): «Класичний», «Продовжити», the «Вигляд» switch

**Files:** Create `src/core/login_view.hpp`, `src/core/login_continue.{hpp,cpp}`, `src/win/login_continue.{hpp,cpp}`;
Modify `src/core/login_card.{hpp,cpp}`, `src/win/login_card.cpp`, config/schema/strings/console/live config/ini;
Test `tests/test_login_continue.cpp`, `tests/test_login_card.cpp`, `tests/test_config.cpp`.

**Interfaces:** `enum class loginview::View : uint8_t { Classic, Shelf, Continue }`, `const char* token(View)`,
`std::optional<View> parse(const std::string&)`; `Config::loginView` (default Shelf), key `[interface] login_view =
shelf|classic|continue`; `logincard` gains the ▾ dropdown of saved logins, the eye (password shown while held), links
«Створити акаунт» / «Забули пароль?» under the buttons; `logincontinue::layout/hitTest/words/paint` (big «Грати»,
links «Інший персонаж», «Змінити акаунт ▾», «Створити акаунт»).

- [ ] **Step 1: failing tests** — `login_view_tokens` (round trip, unknown → nullopt);
  `config_interface_login_view` (absent → Shelf; `classic` → Classic; bad → Shelf + warning);
  `login_card_dropdown_and_eye` (▾ inside the account field's right end, the eye inside the password field, both hit);
  `login_continue_places` (the panel centred, «Грати» ≥ 56·s tall).
- [ ] **Step 2:** FAIL; **Step 3:** implement; the «Вигляд» segment of `loginchrome` writes `[interface] login_view`
  (live); the schema row is a choice with uk names «Класичний», «Полиця», «Продовжити».
- [ ] **Step 4:** PASS; ship (R), notes «Три види входу на вибір».

### Task 12 (C6): class pictures on the cards

**Files:** Create `src/core/class_art.{hpp,cpp}`, `tools/class_art.py`; Modify the package step (`dist/` list or
`tools/package*.py`), `src/win/login_shelf.cpp`; Test `tests/test_class_art.cpp`.

**Interfaces:** `const char* classart::pictureFor(int jobId)` → a file under `uaro-graphics\ui\classes\` or nullptr;
covers the site's 20 pictures (`/var/www/uaro.kiev.ua/web/docs/design/assets/art/*.png`, transcendent and extended
classes); `tools/class_art.py` crops 360×360 portraits to JPEG ≤ 40 KB each.

- [ ] **Step 1: failing test** — `class_art_known_jobs`: High Priest (4009) → "high-priest.jpg", Whitesmith (4011),
  Assassin Cross (4013), Lord Knight (4008) known; Novice (0) → nullptr.
- [ ] **Step 2:** FAIL; **Step 3:** implement; crop offsets per picture in `class_art.py` (face in the upper half);
  the card falls back to `headpicture::make(sex, hair, hairColour, side)` from `LastChar`, then to the job name only.
- [ ] **Step 4:** PASS; ship with C7.

### Task 13 (C7): «вже в грі» across game windows

**Files:** Create `src/core/game_logins.hpp`, `src/win/game_logins.{hpp,cpp}`; Modify `src/win/login_shelf.cpp`;
Test `tests/test_game_logins.cpp`.

**Interfaces:** `namespace gamelogins`: `constexpr int kMost = 8`, `struct Entry { uint32_t seq, pid; uint64_t
login; }` (16 bytes, static_assert), `struct Table { Entry e[kMost]; }`, `std::vector<uint64_t> busy(const Table&,
uint32_t selfPid, const std::vector<uint32_t>& alivePids)`, `int firstFree(const std::vector<uint64_t>& shelf,
const std::vector<uint64_t>& busy)`; block name `Local\uaro-graphics-logins-1`; a game writes its entry when the
login is accepted and clears it at exit or back at the login screen.

- [ ] **Step 1: failing tests** — `game_logins_busy_skips_self_and_dead` (own pid and pids not alive excluded);
  `game_logins_first_free` (shelf [a,b,c], busy [a] → 1; all busy → −1).
- [ ] **Step 2:** FAIL; **Step 3:** implement; busy cards show «вже в грі (вікно N)» and `planFor(..., busy=true)`.
- [ ] **Step 4:** PASS; ship C6+C7 (R), notes «Картки з картинками класів; «вже в грі» для кількох вікон».

### Task 14 (C8): the pad on the login screen

**Files:** Modify `src/pad/win/runtime.cpp`, `src/win/login_shelf.cpp`, `src/win/login_card.cpp`,
`src/win/login_chrome.cpp`; Test `tests/test_login_shelf.cpp`.

**Interfaces:** `loginshelf::padMove(int index, int count, int dir)` → the next card index (wraps off: stops at the
ends); `loginshelfwin::padCommand(PadCommand)`; runtime `loginPad(pad, now)` in the `drive && *top == Win::Login`
chain: ◀ ▶ card, A «Грати», X «Інший акаунт», Y the card menu, B «Вийти» (the game's exit question).

- [ ] **Step 1: failing test** — `login_shelf_pad_moves`: `padMove(0, 3, -1) == 0`, `padMove(0, 3, +1) == 1`,
  `padMove(2, 3, +1) == 2`.
- [ ] **Step 2:** FAIL; **Step 3:** implement + the hints bar words (A/X/Y/B badges drawn, not font glyphs).
- [ ] **Step 4:** PASS; ship with C9.

### Task 15 (C9): «Забути всі акаунти» in the settings app

**Files:** Modify `src/app/page_settings.cpp`, `src/app/model.{hpp,cpp}`, strings.

- [ ] **Step 1:** add the button next to «Забути логін» on «Чат і вхід» (id ≥ 1000), asking first, disabled while
  the game runs; it deletes `%LOCALAPPDATA%\uaroGraphics\accounts.dat`, `accounts.dat.bad` and `site.dat`.
- [ ] **Step 2:** build host (app sources compile in `build-win` only) → win builds; ship C8+C9 (R), notes «Геймпад
  на екрані входу; «Забути всі акаунти»».

## Part D — the site account and registration (site deploy + 3 betas)

### Task 16 (D1): the site's client API (sandbox) and cabinet devices

**Files (sandbox `$S/site_db/app`):** listed in File Structure.

**Interfaces (HTTP, JSON):**
- `POST /api/client/auth {email, password, device_name}` → 200 `{token, user:{nick,email}}` | 422 `{errors}`;
  `throttle:6,1`.
- `POST /api/client/device {device_name}` → `{device_code, user_code, verify_url, interval:5, expires_in:600}`;
  `POST /api/client/device/poll {device_code}` → 200 `{token,user}` | 428 pending | 410 expired.
- Cabinet (auth:web): `GET /api/cabinet/devices`, `POST /api/cabinet/devices/approve {user_code}`,
  `POST /api/cabinet/devices/{id}/revoke`, `POST /api/cabinet/devices/key {name}` → the key shown once.
- Client (header `Authorization: Bearer <token>`, middleware `client.token`): `GET /api/client/accounts` →
  `{logins:[{account_id, userid, primary, banned, ban_until, chars:[{name, class, job, base_level, last_map,
  last_map_name, char_num, online}]}]}`; `POST /api/client/accounts {game_userid, game_password, sex}`;
  `POST /api/client/accounts/link {game_userid, game_password}`; `POST /api/client/accounts/{account_id}/password
  {current_password, password}`.
- `GET /api/client/login-free?login=` → `{free:true|false}` (`throttle:30,1`); `POST /api/client/register
  {game_userid, password, email, sex, agree, device_name}` → 201 `{token,user,account_id}` (site password = game
  password, 8–32; nick = login; the verification e-mail as the cabinet's form sends it).
- `GET /api/public/now.json?lang=ua|ru|en` → `{item:{kind,title,url,starts_at,ends_at}|null}` (a running event, else
  the next within 7 days, else the newest news within 3 days; cached 5 min).
- Tables: `client_devices (id, user_id, name, token_hash char(64) unique, created_at, last_used_at, revoked_at)`,
  `client_device_codes (id, device_code_hash char(64) unique, user_code char(9) unique, device_name, expires_at,
  approved_user_id null, approved_at null, consumed_at null)`.

- [ ] **Step 1:** extract `App\Services\Registration::create(array $data, bool $onePassword)` from
  `Cabinet\AuthController::register` and `App\Services\Cabinet\CharacterList::forUser(int $userId)` from
  `Cabinet\CharacterController::index`; the cabinet controllers call them (behaviour unchanged).
- [ ] **Step 2:** write `$S/site_db/checks/clientapi.php` (runs against the sandbox DB): register → 201 + token;
  same login again → 422 on `game_userid`; auth with the password → token; wrong password → 422; device start →
  user_code `XXXX-XXXX`, poll → 428, approve as the user, poll → 200 token, poll again → 410; accounts with the token
  → one login, its chars array; revoked token → 401; login-free for the new login → false; now.json → item or null.
  Run it → FAIL (routes missing).
- [ ] **Step 3:** implement migrations, models, middleware, controllers, routes; `php artisan migrate` on the sandbox
  DB.
- [ ] **Step 4:** run the check → all lines `ok`.
- [ ] **Step 5:** cabinet: `DevicePage.vue` (`/account/device?code=`: shows the code and the device name, «Дозволити»
  / «Відхилити»), the «Пристрої» block on the security page (list, «Відкликати», «Ключ для клієнта» shown once);
  `node_modules/.bin/vite build` in the sandbox → bundle built.
- [ ] **Step 6:** `deploy-login.sh` / `rollback-login.sh` as `deploy-ping.sh`: BASELINE.sha256 of every modified live
  file, NEW.txt/MODIFIED.txt, `php -l`, backup, copy, migrate (`--path` exact), the cabinet bundle beside the old one
  and the index swapped, a probe (`GET /api/client/login-free?login=` with an empty login → 422 = route alive);
  dry run on a worktree copy of the live app → passes. Commit in the sandbox repo; tell the owner the command.

### Task 17 (D2): the client's HTTP, site key and API parsers

**Files:** Create `src/core/site_api.{hpp,cpp}`, `src/win/site_http.{hpp,cpp}`, `src/win/site_key.{hpp,cpp}`;
Test `tests/test_site_api.cpp`.

**Interfaces:**
- `namespace siteapi`: `struct Request { std::string method, path, body; bool auth; }`; builders `authPassword(email,
  password, device)`, `deviceStart(device)`, `devicePoll(deviceCode)`, `accounts()`, `createLogin(login, password,
  sex)`, `linkLogin(login, password)`, `changePassword(accountId, current, next)`, `loginFree(login)`,
  `registerAccount(login, password, email, sex, device)` (sex `'M'`/`'F'`, `agree` sent as true); answers
  `struct TokenAnswer { bool ok; std::string token, nick; std::map<std::string, std::string> fieldErrors; }`, `struct
  DeviceStart { bool ok; std::string deviceCode,
  userCode, verifyUrl; int interval = 5, expiresIn = 600; }`, `enum class Poll { Pending, Approved, Expired, Failed }`,
  `struct SiteChar { std::string name, job, map; int jobId = 0, level = 0, slot = -1; bool online = false; }`, `struct
  SiteLogin { int accountId = 0; std::string login; bool primary = false, banned = false; long long banUntil = 0;
  std::vector<SiteChar> chars; }`; parsers `parseToken(int status, const std::string& body)`,
  `parseDeviceStart(...)`, `parsePoll(int status, const std::string& body, TokenAnswer&)`, `parseAccounts(...)`,
  `parseFree(...)`.
- `sitehttp::send(const siteapi::Request&, std::function<void(int status, std::string body)>)` — a worker thread,
  lazy winhttp (`WinHttpOpen` … `WinHttpReadData`), 15 s timeouts, answers run on the game thread from
  `sitehttp::pump()` (called each frame); the token header from `sitekey::token()`.
- `sitekey::token()`, `sitekey::save(token, nick)`, `sitekey::forget()` — `%LOCALAPPDATA%\uaroGraphics\site.dat`
  (DPAPI); the computer's name for `device_name` = «uaRO · <Windows computer name>».

- [ ] **Step 1: failing tests** — `site_api_requests_are_json` (auth body has `"email"`, `"password"`,
  `"device_name"`, path `/api/client/auth`, method POST, `auth == false`; accounts `auth == true`);
  `site_api_parses_answers` (token 200; 422 with `{"errors":{"game_userid":["taken"]}}` → fieldErrors; poll 428 →
  Pending, 410 → Expired; accounts with two logins and three chars → sizes and `chars[0].slot`; free `{"free":true}`).
- [ ] **Step 2:** FAIL; **Step 3:** implement (JSON through `core/json`); no request or answer body is ever logged —
  only path and status (`site: /api/client/auth answered 200`).
- [ ] **Step 4:** PASS; ships with D3.

### Task 18 (D3): the site sign-in card (three ways)

**Files:** Create `src/win/site_signin.{hpp,cpp}`; Modify `src/core/login_card.{hpp,cpp}` (the link «Підтягнути всі
логіни»), `src/win/login_card.cpp`, `src/win/window_stack.cpp`, strings; Test `tests/test_login_card.cpp`.

**Interfaces:** the card reuses `logincard`-style core layout for fields (e-mail, site password), buttons «Увійти»,
«Підтвердити в браузері», «Ключ із кабінету»; device flow: `deviceStart` → open `verify_url` → show `user_code` →
`devicePoll` every `interval` s up to `expires_in`; key: a field taking Ctrl+V → saved as the token after
`accounts()` answers 200.

- [ ] **Step 1: failing test** — `login_card_site_link_on_first_launch`: with no accounts and the site not signed
  in, `words()` contains «Підтягнути всі логіни»; signed in → the line is the account chip («{1} · {2} ігрові
  логіни»).
- [ ] **Step 2:** FAIL; **Step 3:** implement; signed in → `accounts()` refreshes the shelf (Task D4); «Вийти з
  акаунта сайту» in the chip's menu → `sitekey::forget()`.
- [ ] **Step 4:** PASS; ship D2+D3 (R) after the owner deployed D1, notes «Вхід з акаунтом сайту».

### Task 19 (D4): the shelf with the site's logins and characters

**Files:** Create `src/core/shelf_merge.{hpp,cpp}`; Modify `src/core/login_shelf.{hpp,cpp}`, `src/win/login_shelf.cpp`,
`src/win/login_card.cpp`; Test `tests/test_shelf_merge.cpp`, `tests/test_login_shelf.cpp`.

**Interfaces:** `namespace shelfmerge`: `struct Row { std::string login, label; bool hasPassword = false, fromSite =
false, banned = false; long long banUntil = 0; std::vector<siteapi::SiteChar> chars; }`, `std::vector<Row>
merge(const accounts::Shelf&, const std::vector<siteapi::SiteLogin>&)` — local order first, site-only logins after
(primary first), logins compared case-insensitively; «Мої персонажі» layout: `loginshelf::characterRows(...)` (a row
per login, up to three character cards and «Новий персонаж»).

- [ ] **Step 1: failing tests** — `shelf_merge_local_and_site`: local [kweb(pw)], site [kweb, kweb_alt] →
  rows kweb(hasPassword, fromSite, chars) then kweb_alt(!hasPassword); a local-only login stays; a banned site login
  carries `banUntil`. `login_shelf_character_rows`: two logins with 2 and 1 chars → 2 rows, the first has 2 cards +
  «Новий персонаж».
- [ ] **Step 2:** FAIL; **Step 3:** implement; a click on a character → `loginWith` + `enterByName(name)`; no password
  → «Ввести один раз» (the password asked, then saved); «+ Новий ігровий логін» → `createLogin` with a password the
  player types (saved at once); «Прив'язати наявний логін» → `linkLogin`; card menu «Змінити пароль гри» →
  `changePassword` then the stored password updated.
- [ ] **Step 4:** PASS; ship (R), notes «Усі логіни й персонажі з акаунта сайту».

### Task 20 (D5): registration in the client

**Files:** Create `src/core/register_form.{hpp,cpp}`, `src/win/register_card.{hpp,cpp}`; Modify `src/win/login_card.cpp`
(«Створити акаунт» opens it), `src/win/char_select.cpp` (open «Створити» on an empty account), strings; Test
`tests/test_register_form.cpp`.

**Interfaces:** `namespace regform`: `enum class Field : uint8_t { Login, Password, Password2, Email, Sex, Agree }`,
`struct Form { std::string login, password, password2, email; int sex = -1; bool agree = false; }`, `std::vector<std::
pair<Field, settings::Text>> check(const Form&)` — login `^[A-Za-z0-9_-]{4,23}$`, password 8–32, the two equal,
e-mail `x@y.z` shape, sex chosen, agree ticked.

- [ ] **Step 1: failing test**
```cpp
TEST(register_form_rules_are_the_sites) {
    regform::Form f{"kweb_new", "secret123", "secret123", "kweb@example.com", 0, true};
    CHECK(regform::check(f).empty());
    auto bad = f; bad.login = "кв";
    CHECK(!regform::check(bad).empty() && regform::check(bad)[0].first == regform::Field::Login);
    bad = f; bad.password = bad.password2 = "short";
    CHECK(regform::check(bad)[0].first == regform::Field::Password);
    bad = f; bad.password2 = "secret124";
    CHECK(regform::check(bad)[0].first == regform::Field::Password2);
    bad = f; bad.login = std::string(24, 'a');
    CHECK(regform::check(bad)[0].first == regform::Field::Login);
    bad = f; bad.agree = false;
    CHECK(regform::check(bad)[0].first == regform::Field::Agree);
}
```
- [ ] **Step 2:** FAIL; **Step 3:** implement; the card: steps «1 Акаунт → 2 Персонаж → 3 У гру», «вільний»
  checked 600 ms after typing stops (`loginFree`), «Створити акаунт» → `registerAccount` → token saved
  (`sitekey::save`), the account saved with its password, `loginWith`, then the character select's «Створити» on the
  first empty place (our char make card opens); «Вже є акаунт на сайті? Увійти й додати логін» → the sign-in card,
  then `createLogin`.
- [ ] **Step 4:** PASS; ship (R), notes «Реєстрація прямо в клієнті».

### Task 21 (D6): the owner's deploy and the end-to-end checks

- [ ] **Step 1:** tell the owner `bash $S/site_db/deploy-login.sh` (and the rollback command); after it, run the
  live probes from loki: `curl -s https://uaro.kiev.ua/api/public/now.json?lang=ua` → JSON; `curl -s -o /dev/null
  -w '%{http_code}' 'https://uaro.kiev.ua/api/client/login-free?login='` → 422.
- [ ] **Step 2:** ask the owner for the Windows checks (one list): register a test account from the client; sign in
  by password, by the browser code, by a cabinet key; the shelf shows all logins/characters; a click enters the
  character; «вже в грі» with two windows; a changed password → «Пароль не підходить»; the server unreachable (the
  network off) → the status line and no boxes; language switch; news line (after the owner adds an event).
- [ ] **Step 3:** memory: the whole result in `project-login-screen-concept.md`; the MEMORY.md line updated.

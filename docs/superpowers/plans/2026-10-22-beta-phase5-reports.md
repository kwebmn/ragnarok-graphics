# Beta Phase 5: «Звернення» Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> The user works inline («делай все фазы без остановки», «в инлайн режиме»): executing-plans, no subagents; the final
> review is a self-review.

**Goal:** a tester reports a glitch in one press and sees its answer: the «Звернення» button in the app's header (with
the number of new answers) opens a window - their reports with statuses on the left, the conversation on the right, a
new report (text, kind, screenshots) - Ctrl+Shift+F10 in the game opens a new report with a full-size shot; the site
keeps statuses and answers, the admin answers in «Звіти графіки».

**Architecture:** The site (Laravel, the existing graphics_reports): an additive migration (computer key, kind,
status, fixed_in, updated_at, player_seen_at; a messages table), public endpoints keyed by a random computer key (no
login), admin endpoints and the React page's conversation. The app: core parses the site's answers
(`core/reports`), `app/reports` talks to the site (GET/POST, an offline queue in `uaro-graphics\reports\`),
`app/reports_window` is the window. The driver's Ctrl+Shift+F10 saves the shown picture and starts
`uaro-graphics.exe --report "<shot>" --map <map>`.

**Tech Stack:** C++17 Win32 + WinHTTP; PHP 8.4 Laravel + PHPUnit (DatabaseTransactions); React/TypeScript (the admin,
vite build).

**Spec:** `docs/superpowers/specs/2026-10-22-beta-release-design.md` (phase 5), the page QjTrLqpnyyw8qKiUZQ3i24
(«Звернення»: окно для тестировщиков, its two mockups), decisions 12 (the user answers in the admin; Claude drafts
and sets «Виправлено в X» at a release) and 15 (no contact field).

## Global Constraints

- Branch `beta/phase-5` from main, fast-forwarded at the end; gate after every task; releases only on the user's word.
- The site: additive only (new nullable columns, a new table, new routes); the touched tables backed up
  (`mysqldump` of graphics_reports) and edited files copied to `$S/site-backup-reports/` first; tested by PHPUnit
  under DatabaseTransactions; the site's git left uncommitted (as before); artisan and phpunit as web71.
- No login, no name, no contact field: a report belongs to a random computer key (the app's `[stats] id`, made when
  missing); one computer sees only its own.
- Statuses: new «Нове», read «Прочитано», need_info «Потрібна інформація», fixing «Виправляємо», fixed «Виправлено в
  X», closed «Закрито». Kinds: graphics «Графіка», crash «Вильот або не запускається», slow «Гальмує», other «Інше».
- Up to 6 screenshots the tester ticked; Windows' own Screenshots folder is never taken by itself.
- Open reports stay; closed ones and untouched automatic ones go after 90 days.

## Review Focus

1. No connection when «Надіслати» is pressed: the report waits in `uaro-graphics\reports\` and goes by itself later -
   Task 5 `a_report_without_the_site_waits_and_goes_later`.
2. Another computer's key never sees a report: the site answers 404 for a code that is not that computer's - Task 1
   `a_computer_sees_only_its_own_reports`.
3. An answer with quotes, new lines, Cyrillic, an emoji reaches the app whole - Task 4 `reports_read_the_conversation`.
4. A tester on 2.x (no key yet) updates: their next report gets a key; old reports without one stay in the admin only -
   Task 5 `the_computer_key_is_made_once`.
5. Ctrl+Shift+F10 while the game is full-screen: the shot is the game's picture and the app comes in front - manual
   (notes).

---

### Task 1: The site - the conversation's data and the public answers

**Files (site):** Create `database/migrations/2026_10_22_200001_add_conversation_to_graphics_reports.php`,
`app/Models/GraphicsReportMessage.php`, `tests/Feature/Public/GraphicsReportConversationTest.php`; Modify
`app/Models/GraphicsReport.php`, `app/Http/Controllers/Api/Public/GraphicsReportController.php`, `routes/api.php`.

**Interfaces:**
- graphics_reports + `computer` (string 64, nullable, index), `kind` (string 16, nullable), `status` (string 16,
  default 'new', index), `fixed_in` (string 32, nullable), `updated_at` (nullable), `player_seen_at` (nullable);
  graphics_report_messages: id, report_id (index), author ('player'|'dev'), text, shots (json nullable), created_at.
- POST /api/graphics/reports gains `computer` (`^[0-9a-f]{16,64}$`), `kind` (graphics|crash|slow|other); the comment
  becomes the first player message.
- GET /api/graphics/reports/mine?computer=K → `{"reports":[{"code","kind","status","fixed_in","title","auto","at",
  "updated","unread"}]}` (title: the comment's first line, ≤ 80; unread: dev messages after player_seen_at).
- GET /api/graphics/reports/{code}?computer=K → `{"code","kind","status","fixed_in","version","messages":[{"author",
  "text","at","shots":[url]}]}`; player_seen_at = now. 404 for another computer.
- POST /api/graphics/reports/{code}/messages {computer, text, screenshots[]} → a player message (need_info → read).
- POST /api/graphics/reports/{code}/verdict {computer, fixed: bool} → closed, or read with a player message «Ще є».
- GET /api/graphics/reports/{code}/shots/{name}?computer=K → a dev answer's JPEG.
- [ ] **Step 1**: backups (mysqldump graphics_reports; the edited files); the failing tests
  `a_computer_sees_only_its_own_reports`, `the_conversation_comes_back_with_its_unread_answers`,
  `a_player_answers_and_gives_a_verdict`, `open_reports_stay_and_closed_ones_go_after_90_days` (the model's prune).
- [ ] **Step 2**: run - FAIL (404s).
- [ ] **Step 3**: migrate (as web71), implement.
- [ ] **Step 4**: run - OK; curl `…/mine?computer=0123456789abcdef` → `{"reports":[]}`.
- [ ] **Step 5**: ledger the files and backups.

### Task 2: The site's admin answers

**Files (site):** Modify `app/Http/Controllers/Admin/GraphicsReportController.php`, `routes/api.php`, Create
`tests/Feature/Admin/GraphicsReportAnswerTest.php`.

**Interfaces:** index: `status` filter (new, waiting = the player's message is the last, open), each row's status,
kind, fixed_in, computer, last message; show: messages; PATCH /api/admin/graphics-reports/{id} {status, fixed_in};
POST /api/admin/graphics-reports/{id}/messages {text, shot (JPEG base64, optional)} → a dev message (new → read).
- [ ] **Steps**: failing test `the_admin_answers_and_sets_the_status` → implement → OK.

### Task 3: The admin page

**Files (site):** Modify `frontend/admin/src/pages/GraphicsReportsPage.tsx` (and its API client); build with vite into
`$S`, copy the assets to `web/admin` (backups of the old index.html and bundle names).
- The report's panel: a status select, «Виправлено в» field, the conversation, a reply box with a shot, the filters
  «Нові» and «Чекають на відповідь».
- [ ] **Steps**: build OK (tsc + vite), deployed, the page loads (curl the index and the bundle).

### Task 4: Reading the site's answers

**Files:** Create `src/core/reports.hpp/.cpp`, `tests/test_reports.cpp`.

**Interfaces:**
```cpp
namespace uagfx::reports {
enum class Status { New, Read, NeedInfo, Fixing, Fixed, Closed };
enum class Kind { Graphics, Crash, Slow, Other };
struct Summary { std::string code, title, fixedIn, at, updated; Status status = Status::New; Kind kind =
                 Kind::Other; bool automatic = false; int unread = 0; };
struct Message { bool fromDev = false; std::string text, at; std::vector<std::string> shots; };
struct Conversation { std::string code, fixedIn, version; Status status = Status::New; Kind kind = Kind::Other;
                      std::vector<Message> messages; };
bool parseList(const std::string& json, std::vector<Summary>& out, std::string& error);
bool parseConversation(const std::string& json, Conversation& out, std::string& error);
const char* kindToken(Kind k);  // "graphics", "crash", "slow", "other"
int unreadOf(const std::vector<Summary>& list);
}
```
- [ ] **Steps**: tests `reports_read_the_list`, `reports_read_the_conversation` (escapes, emoji), `reports_refuse_garbage`
  → implement → PASS.

### Task 5: The app talks to the site; the offline queue

**Files:** Create `src/app/reports.hpp/.cpp`, `tests/test_reports_client.cpp`; Modify `src/app/download.*` (a POST of
JSON), `src/core/report_upload.*` (computer, kind), `src/app/report_send.cpp`, `src/app/model.*` (computerKey),
`tests/fake_windows.*`.

**Interfaces:**
```cpp
bool postJson(const std::string& path, const std::string& body, std::string& answer, std::string& error);
std::string Model::computerKey();  // [stats] id, made (16 random hex) and kept when missing
bool listReports(const std::string& key, std::vector<reports::Summary>& out, std::string& error);
bool openReport(const std::string& key, const std::string& code, reports::Conversation& out, std::string& error);
bool answerReport(const std::string& key, const std::string& code, const std::string& text,
                  const std::vector<std::pair<std::string, std::string>>& shots, std::string& error);
bool verdict(const std::string& key, const std::string& code, bool fixed, std::string& error);
// A new report: sent, or kept in uaro-graphics\reports\ when the site does not answer ("" code then)
std::string sendNew(const Model& m, report::Upload u, std::string& error);
int sendQueued(const std::wstring& dir);  // the waiting ones, at the app's start and the window's opening
```
- [ ] **Steps**: tests `the_computer_key_is_made_once`, `a_report_without_the_site_waits_and_goes_later`,
  `the_list_and_the_conversation_come_from_the_site` (fake web, fake posts) → implement → PASS.

### Task 6: The window «Звернення»

**Files:** Create `src/app/reports_window.hpp/.cpp`; Modify `src/app/main_window.cpp` (the header button
«Звернення» with the unread number replaces «Повідомити про проблему»), strings, `CMakeLists.txt`.
- The window: the list (status, title, date), «+ Нове звернення»; the conversation (author and time, text,
  «Знімок» links), the reply field, «Додати знімок», «Надіслати»; on «Виправлено в X» with this app at X or later:
  «Підтверджую» / «Ще є». The new report: «Що сталося», «Що це» (four kinds), «Знімки» (the recent game shots to tick,
  «Додати файл…», «Вставити (Ctrl+V)»), the line of what goes with it, «Скасувати» / «Надіслати».
- [ ] **Steps**: texts; the gate (every text used); `GATE OK`.

### Task 7: Ctrl+Shift+F10 in the game

**Files:** Modify `src/win/context.cpp` (the key, the shot, the app started), `src/app/args.*` (`--report "<shot>"
--map <map>`), `src/app/main.cpp`, `src/core/start_screen.cpp` (the hint names the key), tests (`test_args.cpp`).
- [ ] **Steps**: `args_read_a_report_from_the_game` → implement → PASS; the gate.

### Task 8: Notes and checks

- [ ] `$S/notes/2.13.0.{ru,uk,en}.md`; gate; self-review; main fast-forwarded.

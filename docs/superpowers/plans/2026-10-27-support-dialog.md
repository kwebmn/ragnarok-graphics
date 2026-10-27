# «Звернення» support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** players see the developer's answers where they are (game, Windows, launcher), read them as a chat, and the
developer sees who waits and who read.

**Architecture:** the site adds read times and answer previews to its JSON (additive); the watcher writes
uaro-graphics\reports\answers.json; the driver shows a card from it (the windows' panel path: a picture in the frame)
or a Windows notification; the app gets an answer strip, `--answer CODE` and an owner-drawn chat; the admin page gets
tabs, chips, a chat and quick answers.

**Tech Stack:** C++17 Win32 (mingw), host tests (tests/*.cpp, test.hpp), Laravel 11 / PHP 8, React 18 + Fluent UI 9.

**Spec:** docs/superpowers/specs/2026-10-27-support-dialog-design.md

## Global Constraints
- Strings en/uk/ru in src/core/settings/strings.cpp; quote existing uk names; lines ≤ 120; ship-beta.sh gate.
- Site JSON changes additive; columns nullable; no phpunit; artisan as web71 via stdin; admin built with vite into the
  scratchpad, assets copied to /var/www/uaro.kiev.ua/web/admin (web71:client1), index.html swapped, old bundles kept.
- Statuses: new, read, need_info, fixing, fixed, closed. Player label of new: «Надіслано» (uk), «Отправлено» (ru),
  «Sent» (en).

## Review Focus
- answers.json missing, empty, half-written or from an older format: no card, no crash (parseAnswers false).
- The site down or throttled at the watcher's check: the old answers.json stays; the game is not slowed (the watcher
  is its own process).
- A computer that never sent a report: no request at all (no computer.txt).
- The card on a tiny window (800x600) or a huge scale: it stays inside the picture (geometry clamps the width).
- Ctrl+Shift+F10 with no card: still «Нове звернення» as before.

---

## Phase A — the notice (3.0.9-beta.4)

### Task 1: site — read times, answer previews
**Files:** Create `database/migrations/2026_10_27_100001_add_seen_times_to_graphics_reports.php`; Modify
`app/Http/Controllers/Api/Public/GraphicsReportController.php` (mine, show), `app/Http/Controllers/Admin/
GraphicsReportController.php` (show), `app/Models/GraphicsReport.php` (casts).
**Produces:** mine's report + `answer: {id, text (≤ 300 chars), status, at} | null` (the newest dev message when
unread > 0) and `last: {author, text (≤ 160), at} | null`; show + `dev_seen_at`; columns `player_seen_at`,
`dev_seen_at` (timestamp null); public show sets player_seen_at when player_seen moves; admin show sets dev_seen_at.
- [ ] php -l each file; migrate as web71; a rolled-back transaction script: mine of a test computer has answer/last.

### Task 2: core — answers in the list and in a file
**Files:** Modify `src/core/reports.hpp/.cpp`; Create `src/core/report_answers.hpp/.cpp`; Test
`tests/test_reports.cpp`, `tests/test_report_answers.cpp`.
**Produces:** `struct reports::Answer { long long id; std::string code, text, at, fixedIn; Status status; }`;
`Summary::answer` (`bool hasAnswer`), `Summary::lastAuthor/lastText/lastAt`;
`std::vector<Answer> reports::answersOf(const std::vector<Summary>&)` (unread with an answer, newest first);
`std::string answers::write(const std::vector<reports::Answer>&)` and
`bool answers::read(const std::string& json, std::vector<reports::Answer>& out)`.
- [ ] TEST(reports_list_reads_the_answer_and_the_last_message), TEST(reports_answers_of_the_unread_newest_first),
  TEST(report_answers_round_trip), TEST(report_answers_refuse_what_is_not_theirs) — red, then green.

### Task 3: app — the watcher asks, `--answer`, the strip
**Files:** Modify `src/app/report_send.cpp` (watchGame), `src/app/args.hpp/.cpp`, `src/app/main.cpp`,
`src/app/reports_window.hpp/.cpp` (ReportStart::code), `src/app/reports_thread.cpp` (+ `reportsAnswers()`),
`src/app/main_window.cpp/.hpp` (strip), `src/core/settings/strings.*`; Test `tests/test_args.cpp`.
- watchGame: after checkNow, when computer.txt exists: listReports → write answers.json (atomic); then wait for the
  game 10 minutes at a time, asking again after each; the old early return keeps its meaning for reports/stats.
- `--answer R-XXXXXX` → Settings mode, ReportStart{code} → showReports opens that conversation.
- Strip: orange, under the header, while `reportsAnswers()` is not empty: «Є відповідь на звернення {1} · {2} — {3}»
  + «Відкрити» (need_info: «розробник чекає на вашу відповідь.»; fixed: «оновіться й перевірте.»; else «нова відповідь
  розробника.»).
- [ ] TEST(args_answer_round_trip) red → green; gate build.

### Task 4: driver — the card and the notification
**Files:** Create `src/core/answer_card.hpp/.cpp`, `src/win/answer_card.hpp/.cpp`; Modify `src/core/notify.*`
(Kind::Answer), `src/core/config.*` (box report_answer), `src/win/notifier.*` (answer()), `src/win/context.*`
(frame, draw, Ctrl+Shift+F10), `src/win/window_switch.cpp` (mouse), `src/pad/win/game_window.*` (two blocked
rects), strings; Test `tests/test_answer_card.cpp`, `tests/test_notify.cpp`.
**Produces:** `card::Geometry card::geometry(float scale, int roomWidth)`; `card::Part card::hitTest(const
Geometry&, float x, float y)` (None, Open, Later); `card::position(...)` top centre 8·scale from the top;
`void card::paint(panel::Picture&, const Geometry&, reports::Status, card::Part hover)`;
`std::vector<std::string> card::fitLines(const std::string& text, size_t most, const std::function<bool(const
std::string&)>& fits)` (words kept whole, «…» on the last).
- [ ] tests red → green: geometry inside the room, hit test of both buttons, fitLines two lines with «…»,
  notify::wanted for Answer; gate build.

### Task 5: ship 3.0.9-beta.4 (notes en/uk/ru).

## Phase B — the messenger (3.0.9-beta.5)

### Task 6: core — the timeline and the stepper
**Files:** Create `src/core/report_talk.hpp/.cpp`; Test `tests/test_report_talk.cpp`.
**Produces:** `struct talk::Item { enum Kind { AutoSent, Message, Status, DevRead } kind; size_t message; }`;
`std::vector<talk::Item> talk::timeline(const reports::Conversation&)` (DevRead after the last player message before
dev_seen_at); `struct talk::Step { reports::Status status; bool done, current; }`;
`std::vector<talk::Step> talk::steps(const reports::Conversation&)`; `bool talk::closedTab(reports::Status)`;
Conversation + `devSeenAt`.
- [ ] tests red → green.

### Task 7: app — the chat window
**Files:** Create `src/app/chat_view.hpp/.cpp`; Modify `src/app/reports_window.cpp`, strings.
- Owner-drawn list cards + tabs; header chip, meta, stepper painted; ChatView (bubbles, thumbnails fetched on a
  thread, click opens); bars; Ctrl+Enter sends.
- [ ] gate build; the window's layout checked at 980x660 and maximised by reading the code paths (no Windows here).

### Task 8: ship 3.0.9-beta.5.

## Phase C — the admin (site)

### Task 9: API — tabs, filters, receipts, language
Admin index: `tab=live|auto`, `status=asked` (need_info), fields `last_at`, `last_dev_id`, `player_seen`,
`player_seen_at`; show: `lang` (utility.language from the report text, else uk), `dev_seen_at`, `player_seen_at`.
- [ ] php -l; rolled-back transaction checks.

### Task 10: admin page
Modify `frontend/admin/src/pages/GraphicsReportsPage.tsx` (+ a `ReportChat.tsx`): tabs, chips, rows, chat bubbles
with receipts, quick answers by language, status + «Виправлено в». Build (vite → scratchpad), deploy assets, swap
index.html.
- [ ] tsc via vite build passes; the page loads (HTTP 200 for the new bundle).

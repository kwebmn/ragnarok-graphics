# «Звернення»: support that players notice — design

Date: 2026-10-27. Approved by the user («делай все») after the 5-part proposal in chat and the canvas
https://claude.ai/artifact/DMddvu8933XYj9hfqW3wev (screens 1-4 with notes 1-8).

## Why
- Players never learn that the developer answered: the answer shows only on the app's «Звернення (N)» button, and an
  HD-client player goes launcher → game (R-ZSDWYU, FirePro: player_seen 0 after three starts).
- The conversation is one grey read-only EDIT; screenshots are a separate list; a status says nothing about what to do.
- The developer cannot see whether the player read the answer (only by a DB query); the admin mixes crash reports with
  living conversations.

## Goals
1. **The notice where the player is.**
   - In the game, in front: a card at the top centre of the game picture — «Відповідь на звернення R-XXXXXX», status
     chip, the developer's words (2 lines at most), «Відкрити · Ctrl+Shift+F10», «Пізніше», the time. Drawn the way
     the windows' panel is (a picture in the frame: every renderer). «Відкрити» (click, or Ctrl+Shift+F10 while it is
     shown) starts `uaro-graphics.exe --answer R-XXXXXX`: «Звернення» opened on that conversation. «Пізніше» hides it
     until the next game start. The card hides once its answer is opened.
   - The game behind or minimised: a Windows notification (the driver's notifier) — new [notify] box
     `report_answer`, on by default; a click brings the game back, the card shows there.
   - The app (page «Гра» is what uaRO.exe shows: the launcher): an orange strip under the header while an answer is
     unread — «Є відповідь на звернення R-XXXXXX · <status> — <what to do>» + «Відкрити»; «Звернення (N)» stays.
   - Where the answers come from: the watcher (`--watch`, started with every game) asks GET reports/mine at the game's
     start and every 10 minutes while it runs (only on a computer that has sent a report: computer.txt exists) and
     writes uaro-graphics\reports\answers.json; the driver reads that file.
2. **«Звернення» as a messenger.** Left: «Нове звернення», tabs «Відкриті · N» / «Закриті · N», cards (unread dot,
   title, status chip, time, the last message's start). Right: the title + status chip; a meta line (code · kind ·
   driver version · sent at); a stepper Надіслано → Прочитано → Потрібна інформація → Виправляємо → Виправлено →
   Закрито (the current one marked; «Потрібна інформація» only while it is the status or was); the chat — the
   player's bubbles right, the developer's left with «Розробник», screenshots as thumbnails in their bubble (a click
   opens it), a status change as a thin centred line, «Розробник прочитав · time»; a bar over the reply box: orange
   «Розробник чекає на вашу відповідь.» + the question while need_info, green «Виправлено в X…» with «Підтверджую» /
   «Ще є» once the fix is installed (existing rule); the reply box with «Додати знімок…», «Вставити · Ctrl+V»,
   «Надіслати · Ctrl+Enter». The new-report form stays as it is.
3. **Read receipts both ways.** The site stores when the player last saw (player_seen_at) and when the developer last
   opened the report (dev_seen_at). The player sees «Розробник прочитав · time»; the developer sees «✓✓ Гравець
   прочитав · time» or «✓ ще не бачив».
4. **The admin («Звіти графіки»).** Tabs «Від гравців» (auto = 0, «Тест графіки» included) / «Автоматичні» (auto = 1);
   chips «Усі відкриті», «Чекають на відповідь» (the existing waiting rule), «Чекають на гравця» (need_info),
   «Виправляємо», «Виправлено»; each row: code, kind, time of the last message, title, status, who waits, read
   receipt, map · card · version; the conversation as a chat; quick answers that fill the text and set the status
   («Надішліть звернення з Pause» → need_info, «Виправлено в … — оновіться й перевірте» → fixed, «Дякую, дивлюся» →
   fixing) in the player's language (utility.language from the report, else uk).

## Non-goals
No accounts or login (the computer key stays); no server push (polling); no new statuses (new/read/need_info/fixing/
fixed/closed). The player's label of «new» becomes «Надіслано» (the admin keeps «Нове»).

## Constraints
- Win32 app without new libraries; driver pictures through win/picture_draw (the panel's path); strings in en/uk/ru
  (src/core/settings/strings.cpp); lines ≤ 120; ship-beta.sh gate (build + host tests).
- Site: Laravel 11 + the Fluent admin (frontend/admin), changes additive (old apps keep working); nullable columns;
  no phpunit; deployed as before (artisan as web71 via stdin; vite build to the scratchpad, assets to web/admin).
- Betas only («работаем пока в бете»); one beta per part: notice (A), messenger (B), admin (C, site only).

## Calls made by the builder (the user may change them)
The poll every 10 minutes; the card's place (top centre) and size; «Пізніше» until the next game start;
Ctrl+Shift+F10 opens the answer while the card shows; «Тест графіки» in the players' tab.

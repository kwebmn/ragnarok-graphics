# The app's first screen as a game launcher — design

Status: direction approved by the owner on 2026-11-12 («Вариант А»); the calls marked **(предложено)** wait for the
owner's word. Concept page with mockups: https://claude.ai/artifact/E86nmjnrRPw7s2iqnwW2VF

## Why

The owner (12.11): the first screen of uaro-graphics.exe «очень сложный, новый пользователь может испугаться и не
разобраться… надо как-то быстрее вместить всё… хотелось бы ченжлог… учти, что это игра, а не приложение».
Today the first view is the settings app's «Гра» page: 12 sections on the left, Simple/Detailed, search, Reports,
language, Play / One more window / Report, a keys hint, «Start at once», the update channel, check and rollback, three
look lines with «Change ›», «★ New features», «Changed only», the graphics test, news, server, and the bottom bar
Apply / Apply and play / Undo / Close with the status line — about 30 things, Play a small blue button.

## Goals

- A newcomer sees a game and one obvious action: a big «ГРАТИ».
- Everything a player needs often is one click away: play, another window, how the game looks, what changed, a report.
- A changelog: every version's notes in players' words, «what is new for me» since the last game.
- Nothing of today's settings lost: the whole settings app behind the gear.

Non-goals: new settings; changes to the game or the driver; the site's API (news pictures need it — see «Open»).
Anti-goals: a launcher that hides the update or the server state; a second place where settings are edited
differently; animations that cost the game's PC anything while the game runs.

## The screen (variant A)

Full-window art of uaRO (the site's uaRO HD shots), darkened to the left and the bottom; over it:

1. **Top bar**: the uaRO wordmark; the server pill «Сервер працює · N онлайн» (red «Сервер недоступний» when the
   site's launcher API gives no count); the language «UA ▾».
2. **«Що нового»** (left column): first the update card «Оновлено до X» with its first two points and «Журнал змін →»
   (shown while there are versions newer than the last one seen, else the newest version's card); then the site's
   news cards (title, date, «Детальніше» opens the site).
3. **Dock** (bottom left), four icons with words: «Налаштування» (the settings app as it is now, with «← До гри»),
   «Звернення» (the reports window; a red count of unread answers), «Сайт», «Форум».
4. **Look line** over the button: «Ремастер · Новий інтерфейс · HD» and «авто для <card>» when the presets follow
   the card; click → the look picker.
5. **«ГРАТИ»** with a ▾ part: «Ще вікно гри», «Безпечний запуск» (one start with [renderer] safe_mode). The button
   shows the state: «ОНОВИТИ · N МБ» (an update offered: download, install, play), «Оновлення 45 % · 6 МБ/с»,
   «ГРАТИ», «ГРА ЗАПУЩЕНА · + ВІКНО» (the game runs: another window). Enter = the button.
6. **Version** small under the button: «3.0.18-beta.191 · бета · усе оновлено»; click → a menu: the channel
   (Реліз / Бета), «Перевірити оновлення», «Інша версія…» (today's rollback list). The automatic rollback offer after
   two failed starts stays and shows as a plate over the dock.

**Plates** over the dock for the states «Гра» shows today, each with its own button: the safe mode («Звичайний
режим»), uaRO Graphics switched off («Увімкнути знову»), the rollback offer after two failed starts («← Повернути X»).

## The look picker

A card like the game's windows over the launcher: «Авто для вашої відеокарти» (presets::suggest) on top, then the
three knobs of core/settings/choice_presets — «Картинка» (with pictures), «Інтерфейс», «Текстури» — the current one
outlined, «змінено N» when hand changes differ. «Перевірити відеокарту» (today's graphics test), «Усі налаштування ›».
A pick writes the ini at once and shows «Змінено · Скасувати» for 5 s **(предложено)**.

## «Журнал змін»

A screen over the dimmed art: «← До гри», filters «Усе» / «Лише релізи» / «Нове для мене · N». Left: versions by day
(«Сьогодні», «Вчора», dates), tags «бета» / «реліз», «нове» for versions after the last seen one
(cache\seen-version.txt). Right: the chosen version's points; a point that names a new setting gets «Спробувати»
(opens it, as «★ Нові можливості» does). Betas listed only on the beta channel; the release channel sees releases,
each with its betas' points inside **(предложено)**.

Data: uaro-graphics\notes\TEST.<lang>.md (dist/TEST.*.md) already holds 883 versions. Since 3.0.18-beta.56 the ship
script prepends bare points without «# uaRO Graphics X — що нового»: the ship script writes the heading and the date,
and the missing headings are put back from git (each «X: що нового» commit's added block and date).

## First run

Today's four persona cards (3.0.17-beta.10) as pictures over the launcher, the card's suggestion outlined
«для вашої відеокарти», «Пропустити — вибере сама програма».

## Look and feel

- The game's own palette (uaUI style A): card #10141C, gold #C9A55B, text #EEF2F8, muted #9AA3B5; the button gold.
- The art changes slowly between the fountain at dawn, day, sunset and winter (crossfade over 2 s every 20 s)
  **(предложено)**; still while the game runs or the window is in the background.
- Windows' frame dark (DWM dark title bar) rather than a frame of our own **(предложено)**: a custom frame was asked
  as «свои цвета и рамка»; the dark system frame gives the look without breaking snap, resize and accessibility.

## How it is built (builder's notes)

- uaro-graphics.exe is Win32 + common controls; the launcher is one custom-painted child view: composed in software
  into a 32-bit DIB (core/window_panel Picture, roundRect, images; JPEG by stb), words by GDI over it, icons from
  Segoe MDL2 Assets (Windows 10+; letters on 7/8), hit boxes of its own; repainted only on a change (hover, state,
  the crossfade's frames).
- Layout and state are core code with host tests (no Windows): the boxes at a client size and DPI, the button's state
  from the update and game states, the changelog's parse/group/filter, the look line's words.
- Images ship in the package (uaro-graphics\launcher\*.jpg, about 2 MB).
- The settings view is today's window; «Гра» leaves its sections (the launcher replaces it); «← До гри» returns.
- Every new word in the three languages (strings.cpp); UI names quoted from its uk column.

Builder's calls (made in this design, open to the owner's word):
- The window keeps today's size and place; the launcher fits any client size from 960x560 (at 96 DPI) up.
- Keys: Enter plays; Tab walks the button, ▾, the look line, «Журнал змін», the news, the dock, the version, with a
  visible focus ring; Esc closes the picker, the changelog, a menu.
- An update offered: the ▾ has «Грати без оновлення» too.
- «Сайт» opens https://uaro.kiev.ua/<lang>/, «Форум» https://uaro.kiev.ua/forum/.
- Fonts: Exo 2 for the words of the launcher and Cinzel for the wordmark, shipped with the app (OFL, loaded privately);
  Segoe UI where they fail.
- «Спробувати» in the changelog: a point's «quoted» name equal to a settings row's title in the player's language;
  the settings open searched for it.
- The release channel's changelog: a release lists the points of the betas numbered before it (3.0.18-beta.N under
  3.0.18), its own points first.
- The graphics test's results table moves to «Налаштування» → «Сервіс».

## Stages (a beta each)

1. Changelog data: headings back in dist/TEST.*.md, the ship script writes them; core parse/group/filter + tests.
2. The launcher view: art, top bar, «Що нового», dock, look line, button with its states, version; Enter; the
   settings view behind the gear with «← До гри».
3. The look picker (pictures, apply at once, 5 s undo).
4. «Журнал змін» screen.
5. First run cards with pictures; the rollback plate; polish.

## Open

- News pictures: the site's launcher API has none (title, excerpt, date, url) — text cards until the owner wants the
  API to give a cover (a site change, only on his word).
- The calls marked **(предложено)** above.

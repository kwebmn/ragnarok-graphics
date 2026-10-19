# Settings app: modes, fewer pages, a window that grows — design

Approved by the user on 2026-10-19 («нравится, приступай к плану и делай») as the mockup canvas
https://claude.ai/artifact/Dh93NArXW6RPYE7JQyqhBH (five 1100×720 artboards: Простой/Главное, Подробный/Графика,
Подробный/Нейросеть, Разработчика/Отладка, Поиск). This file is the canvas in words.

## Why

uaro-graphics.exe grew to 15 pages and ~300 settings. A player needs a handful; the rest must stay (nothing is thrown
away, nothing leaves the ini). Settings that belong together are spread over pages, and the window is a fixed
880×600 that cannot be stretched or maximized.

## What

1. **Three modes** in the header: Простой (Simple), Подробный (Detailed), Разработчика (Developer).
   - Every setting has a level: the lowest mode that shows it. A mode hides rows above it; a page left empty leaves
     the list. Hidden rows keep their values and their ini keys.
   - Default: a new install starts Simple; an install the app ran in before starts Detailed. The choice is kept.
2. **Simple mode** lists Главное, Эффекты, Геймпад.
   - Главное, «Картинка»: Качество (Низкое / Среднее / Высокое / Супер-лёгкое), NVIDIA DLSS, Генерация кадров,
     Кадров в секунду не больше + VSync, HDR.
   - Главное, «Прочее»: Улучшенный звук, Геймпад (uaRO Pad), Беречь батарею ноутбука; buttons «Рекомендуемые
     настройки», «Что нового в X».
   - Главное, «Состояние» card: driver version and update, Direct3D and the card, DLSS installed, the game running,
     «Проверить обновление».
   - Главное, «HD-текстуры» card: show the network's pictures (on/off), the pack (pictures, size), «Сделать пакет»,
     «Подробнее».
   - Эффекты in Simple mode: the set and the 10 groups, without the per-effect details.
3. **Detailed mode** has 8 pages instead of 15: Графика (Рендер + Качество + Сглаживание + Кадры, two columns when
   wide), Экран, Эффекты, Нейросеть (tabs: Что улучшать · Модели по группам · Пакет · Готовые текстуры), Звук,
   Геймпад, Игра и чат, Сервис (with the diagnostics and the report).
4. **Developer mode** adds, under «Только разработчику», the page Отладка: the log, the profiler, X-ray, the DLSS
   checks (jitter, motion), bridge_present, render_thread, shader variants, the state cache and draw merging, Remix;
   tools: uaRO Studio, Сжать пакет, Palettes.grf, Превью моделей; the in-game keys. The developer rows elsewhere
   (model preview, one map's textures, «Сжать», Palettes.grf) show only in this mode.
5. **The window** stretches and maximizes, keeps its size and place, is at least 880×600, starts at 1100×720 when the
   screen has room. Wide pages (Графика, Эффекты, Нейросеть) go in two columns.
6. **The header**: the mode switch, a search field (Ctrl+F) and «Сообщить о проблеме». The search lists the matching
   settings of every page of the mode with their page › group, changeable in place, and «На страницу».
7. **The status bar** at the bottom: the driver's version and update, the pack, the game running.
8. **Changed vs recommended**: a dot before a row whose value differs from the recommended one for this computer
   (the auto setup's value, else the shipped default), and «Сбросить страницу к рекомендуемым» on the pages made of
   such rows.
9. **«Что нового»** after an update: the notes of the installed version, once.

## Calls made while turning the canvas into a plan

- Apply / Apply and play / Undo / Close stay. The artboards drew no buttons and the Main one says «Настройки
  применяются сразу», but the pending changes are the safety net (one backup a session, Undo, the game's own keys
  waiting for it to close). Instant apply is a separate decision for the user.
- Качество on Главное sets the Service page's presets (Низкое = Speed, Среднее = Balance, Высокое = Maximum, without
  their fps limit) or super_light; it leaves the effects set alone - Эффекты is the next page of the same mode.
- The 38 model groups show in Detailed mode (the Нейросеть artboard is the Detailed one); the canvas note listed them
  under Developer.
- uaRO Studio is a button of Отладка, not a list entry of its own.
- A page outside the mode's list (Главное's «Подробнее», «Сообщить о проблеме» in Simple mode) opens without
  switching the mode, at the Detailed level, with no list entry selected.
- The search replaces the page while it has text (Esc goes back); the canvas drew it over a dimmed page.

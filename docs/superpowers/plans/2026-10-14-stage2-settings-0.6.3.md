# uaRO Graphics 0.6.3 — ввод и окно, dgVoodoo, обновление и удаление из утилиты (план)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** выпуск 0.6.3 через `update.bat`: в утилите разделы «Ввод и окно» (ini ROExt или uaRO Pad) и «dgVoodoo»
(`dgVoodoo.conf`); в «Сервисе» проверка обновления, «Обновить до …» и «Удалить uaRO Graphics…»; `release/version.txt`
на GitHub; мелочи финального ревью 0.6.2. Из ключей ROExt раздел показывает только названные в загруженной DLL: у
`dinput8.dll` клиента нет `MouseFreedom` и `KeyRemap`; `KeyRemap` не показывается вовсе — `dinput.dll` новой сборки
нет (§2.3, §12).

**Architecture:** ядро (тесты на Linux): таблица 32 ключей чужих файлов — ROExt, uaRO Pad, dgVoodoo — с правилами чтения
и списками значений (`file_settings`), ключи ROExt, названные в его DLL (`identifyModule`); номер сборки из
`version.txt` и сравнение сборок (`update_check`); значения ключа игры, принадлежащие ПК (`machineValues`), файл
настроек без логина и загрузка, оставляющая ПК его экран, звук и логин; разбор лога без старого масштаба. Модель (тесты
на подставном Windows): слова чужих файлов ждут «Применить», при запущенной игре — её закрытия (§4.1), `dgVoodoo.conf`
пишется в обе копии; раздел покажет ключ ROExt, лишь если его называет загруженная сборка; первый запуск показывает то,
что запишет «Применить»; состояние проверки обновления и запуск `update.bat` и `uninstall.bat`. Окно (Win32): разделы из
таблицы; «Сервис» с проверкой через WinHTTP в своём потоке; закрытие с недописанным полем спрашивает.

**Tech Stack:** C++17, MinGW-w64 i686 GCC 12, Win32 (+ WinHTTP), Direct3D 12, тесты на Linux.

**Spec:** `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` (§2.3, §2.4, §4.1, §6.1, §8, §11 —
строка 0.6.3, §12); мелочи финального ревью 0.6.2 (scratch `ledger-0.6.2.md`, «Final: minor (deferred)» M1–M7).

## Что и где

| Что | Откуда | Задача |
|---|---|---|
| разделы «Ввод и окно», «dgVoodoo»; тексты 0.6.3 | §6, §6.1 | 1 |
| лог: после выключения чёткого текста на лету нет старого масштаба | 0.6.2/M1 | 2 |
| файл настроек игры без логина; загрузка оставляет экран, звук, видеокарту и логин ПК | §8; 0.6.2/M2, M3 | 2, 3 |
| первый запуск: страницы показывают то, что запишет «Применить» | §4.3; 0.6.2/M5 | 3 |
| ключи ROExt, uaRO Pad и dgVoodoo с их значениями; какие ключи ROExt называет его DLL | §2.3, §2.4 | 4 |
| модель: чужие файлы ждут «Применить» и закрытия игры, `dgVoodoo.conf` в обе копии | §4.1, §2.4 | 5 |
| проверка обновления, запуск `update.bat` и `uninstall.bat` | §8 | 6 |
| разделы «Ввод и окно» и «dgVoodoo» (Win32) | §6.1 | 7 |
| «Сервис»: проверить, обновить, удалить; WinHTTP; `uninstall.bat` ждёт утилиту | §8, §12 | 8 |
| закрытие с недописанным полем; лог драйвера при откате окна | 0.6.2/M6, M7 | 9 |
| версия, `release/version.txt`, спецификация, чек-лист; названия колонок | §11, §12; 0.6.2/M4 | 10 |
| `KeyRemap` ROExt | §2.3, §12 | без задачи: нет `dinput.dll` новой сборки ROExt |

## Global Constraints

- `ddraw.dll` — PE32 i386, импорты только `KERNEL32`, `USER32`, `GDI32`, `msvcrt`, пролог горячего патча, откат на
  dgVoodoo. `uaro-graphics.exe` — GUI, asInvoker, PerMonitorV2; импортирует только функции из списка
  `tools/check_exe.py`, каждая есть в Windows 7 (новые: `winhttp.dll` — 10 функций, `kernel32!CreateThread`).
- §4.1: ini ROExt, `uaro-pad.ini` и `dgVoodoo.conf` утилита пишет только при закрытой игре; действуют со следующего
  запуска. §4.6: перед первой записью запуска — копия (она уже берёт ini модулей и обе копии `dgVoodoo.conf`).
- Правка чужого файла меняет только свои значения: комментарии, порядок и остальные ключи остаются (`applyIniEdits`).
- Тексты — на английском, украинском и русском (апостроф в украинском — ASCII `'`); отчёт и лог — по-английски.
- Сборка без предупреждений; строки — до 120 знаков (кроме строк таблиц Markdown); комментарии по-английски.
  `.bat` — CRLF.
- Версия — `0.6.3`. Коммиты — KWEB Team, сообщения по-русски, трейлер `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Чужой файл не такой, как у нас.** `dgVoodoo.conf` другой версии (ключа нет или он в другой секции), ini с
   повторённой секцией, значения в другом регистре, комментарий в строке. Раздел показывает значение программы по
   умолчанию, «Применить» дописывает ключ, чужое значение вне списка остаётся (тесты Tasks 4, 5).
2. **Проверка обновления без интернета, без TLS 1.2 (Windows 7), за прокси, с HTML вместо `version.txt`.** Окно не
   замирает (свой поток), строка версий говорит «не проверить (ошибка …)»; разбор ответа — тест Task 6, сеть — только
   Windows.
3. **`update.bat` и `uninstall.bat`, запущенные из утилиты.** Утилита закрывается без второго вопроса, скрипт ждёт её
   до 10 с; при запущенной игре `update.bat` отказывается сам. Только Windows: пункт 7 чек-листа.
4. **Высота страниц.** «dgVoodoo» с предупреждением о рендере — около 448 точек из 488 при 96 DPI; «Ввод и окно» при
   двух модулях — с переключателем. Только Windows: пункт 2 чек-листа.
5. **ROExt и uaRO Pad загружены вместе** (старый ROExt `dinput8.dll` рядом с uaRO Pad): раздел показывает один по
   выбору, «Экран» пишет размер только в ini того ROExt, что загружает игра (тест Task 5 — `.bak` не в счёт).

## Как записаны правки

Код шагов взят из коммитов эталонной реализации, план проверен на свежей копии `main` (каждый шаг применён, вывод
каждой команды сверен с Expected, итоговое дерево совпало с эталоном). Формы — как в прежних планах: «`путь`:» — файл
целиком; «В `путь` заменить: … на: …» — фрагмент встречается в файле ровно один раз в момент замены; замены одного шага
идут сверху вниз, в порядке записи. `.bat` на диске — CRLF, в блоках — LF.

Проверка задачи — `verify4.sh` (scratch): сборка Windows без предупреждений, `check_dll`, `check_exe`, сборка и тесты
ядра.

---

### Task 1: Разделы «Ввод и окно», «dgVoodoo» и тексты 0.6.3 (§6.1)

Список разделов получает «Ввод и окно» и «dgVoodoo» перед «Диагностикой» (§6). Все тексты 0.6.3 добавляются сразу, на
трёх языках: подписи ROExt, uaRO Pad и dgVoodoo со списками значений, проверка обновления, обновление и удаление.
Кнопка сброса в группе «uaRO Graphics» короче — «Сбросить к умолчаниям»: имя уже в заголовке. Записка файла настроек
игры говорит, что логина в нём нет и загрузка оставляет экран и звук этого ПК (0.6.2, M2, M3).

**Files:**
- Modify: `src/core/settings/schema.cpp`, `src/core/settings/schema.hpp`, `src/core/settings/strings.cpp`, `src/core/settings/strings.hpp`, `tests/test_schema.cpp`

**Interfaces:**
- Consumes: —
- Produces: `enum class Page : uint8_t { Render, Screen, Quality, Frames, Sound, Game, Chat, Input, DgVoodoo,
  Diagnostics, Service }`, `kPageCount = 11`; тексты `Text::PageInput` … `Text::ErrLaunchFile` (в конце перечня).

- [ ] **Step 1: Падающий тест**

В `tests/test_schema.cpp` заменить:
```cpp
}

TEST(schema_pages_follow_the_spec) {
    // Stage 2 spec §6.1 with 0.6.2: Sound, Game, and Chat and login after Frames - the last holds the whisper, login
    // and chat macro rows the Game page has no room for.
    CHECK_EQ(kPageCount, 9);
    CHECK(pageName(Page::Sound) == Text::PageSound);
    CHECK(pageName(Page::Game) == Text::PageGame);
    CHECK(pageName(Page::Chat) == Text::PageChat);
    CHECK_EQ(int(Page::Diagnostics), 7);
    CHECK_EQ(int(Page::Service), 8);
}
```
на:
```cpp
}

TEST(schema_pages_follow_the_spec) {
    // Stage 2 spec §6: Sound, Game, and Chat and login after Frames - the last holds the whisper, login and chat macro
    // rows the Game page has no room for (0.6.2) - then Input and window, and dgVoodoo (0.6.3).
    CHECK_EQ(kPageCount, 11);
    CHECK(pageName(Page::Sound) == Text::PageSound);
    CHECK(pageName(Page::Game) == Text::PageGame);
    CHECK(pageName(Page::Chat) == Text::PageChat);
    CHECK(pageName(Page::Input) == Text::PageInput);
    CHECK(pageName(Page::DgVoodoo) == Text::PageDgVoodoo);
    CHECK_EQ(int(Page::Input), 7);
    CHECK_EQ(int(Page::Diagnostics), 9);
    CHECK_EQ(int(Page::Service), 10);
}
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_schema.cpp:67:26: error: ‘Input’ is not a member of ‘uagfx::settings::Page’`.

- [ ] **Step 3: Реализация**

В `src/core/settings/schema.cpp` заменить:
```cpp
        case Page::Sound: return Text::PageSound;
        case Page::Game: return Text::PageGame;
        case Page::Chat: return Text::PageChat;
        case Page::Diagnostics: return Text::PageDiagnostics;
        case Page::Service: break;
    }
```
на:
```cpp
        case Page::Sound: return Text::PageSound;
        case Page::Game: return Text::PageGame;
        case Page::Chat: return Text::PageChat;
        case Page::Input: return Text::PageInput;
        case Page::DgVoodoo: return Text::PageDgVoodoo;
        case Page::Diagnostics: return Text::PageDiagnostics;
        case Page::Service: break;
    }
```

В `src/core/settings/schema.hpp` заменить:
```cpp
namespace uagfx::settings {

// The settings app's pages in the order of its list (stage 2 spec §6.1): Chat holds the whisper, login and chat macro
// rows the Game page has no room for; Input and dgVoodoo come with 0.6.3.
enum class Page : uint8_t { Render, Screen, Quality, Frames, Sound, Game, Chat, Diagnostics, Service };
constexpr int kPageCount = 9;
Text pageName(Page p);

enum class Control : uint8_t { Check, Slider, Choice, Number, Line };  // Line: a line of text
```
на:
```cpp
namespace uagfx::settings {

// The settings app's pages in the order of its list (stage 2 spec §6.1): Chat holds the whisper, login and chat macro
// rows the Game page has no room for; Input is ROExt's or uaRO Pad's settings, DgVoodoo dgVoodoo.conf's.
enum class Page : uint8_t { Render, Screen, Quality, Frames, Sound, Game, Chat, Input, DgVoodoo, Diagnostics, Service };
constexpr int kPageCount = 11;
Text pageName(Page p);

enum class Control : uint8_t { Check, Slider, Choice, Number, Line };  // Line: a line of text
```

В `src/core/settings/strings.cpp` заменить:
```cpp
     "Повернути налаштування з копії від {1}? Поточні спершу теж скопіюються.",
     "Вернуть настройки из копии от {1}? Нынешние сначала тоже скопируются."},
    {"uaRO Graphics", "uaRO Graphics", "uaRO Graphics"},
    {"Reset uaRO Graphics to its defaults", "Скинути uaRO Graphics до типових", "Сбросить uaRO Graphics к умолчаниям"},
    {"Return every uaRO Graphics setting to its default? The renderer and the language stay.",
     "Повернути всі налаштування uaRO Graphics до типових? Рендер і мова залишаться.",
     "Вернуть все настройки uaRO Graphics к умолчаниям? Рендер и язык останутся."},
```
на:
```cpp
     "Повернути налаштування з копії від {1}? Поточні спершу теж скопіюються.",
     "Вернуть настройки из копии от {1}? Нынешние сначала тоже скопируются."},
    {"uaRO Graphics", "uaRO Graphics", "uaRO Graphics"},
    {"Reset to defaults", "Скинути до типових", "Сбросить к умолчаниям"},
    {"Return every uaRO Graphics setting to its default? The renderer and the language stay.",
     "Повернути всі налаштування uaRO Graphics до типових? Рендер і мова залишаться.",
     "Вернуть все настройки uaRO Graphics к умолчаниям? Рендер и язык останутся."},
```

В `src/core/settings/strings.cpp` заменить:
```cpp
    {"Save to a file...", "Зберегти у файл…", "Сохранить в файл…"},
    {"Load from a file...", "Завантажити з файлу…", "Загрузить из файла…"},
    {"Reset the windows' layout", "Скинути розкладку вікон", "Сбросить раскладку окон"},
    {"The whole game key: hotbars, windows, options, macros. Another PC loads the file too.",
     "Увесь ключ гри: панелі, вікна, опції, макроси. Файл завантажується й на іншому ПК.",
     "Весь ключ игры: панели, окна, опции, макросы. Файл загружается и на другом ПК."},
    {"Load the game's settings from {1}? Everything the game saved (hotbars, windows, options, macros) is replaced; a "
     "copy of the present settings is made first.",
     "Завантажити налаштування гри з {1}? Усе, що зберегла гра (панелі, вікна, опції, макроси), заміниться; спершу "
```
на:
```cpp
    {"Save to a file...", "Зберегти у файл…", "Сохранить в файл…"},
    {"Load from a file...", "Завантажити з файлу…", "Загрузить из файла…"},
    {"Reset the windows' layout", "Скинути розкладку вікон", "Сбросить раскладку окон"},
    {"The game's key: hotbars, windows, options, macros - not the login. Loading keeps this PC's screen and sound.",
     "Ключ гри: панелі, вікна, опції, макроси — без логіна. Завантаження лишає екран і звук цього ПК.",
     "Ключ игры: панели, окна, опции, макросы — без логина. Загрузка оставляет экран и звук этого ПК."},
    {"Load the game's settings from {1}? Everything the game saved (hotbars, windows, options, macros) is replaced; a "
     "copy of the present settings is made first.",
     "Завантажити налаштування гри з {1}? Усе, що зберегла гра (панелі, вікна, опції, макроси), заміниться; спершу "
```

В `src/core/settings/strings.cpp` заменить:
```cpp
     "Такий файл додають подвійним клацанням (regedit).",
     "В {1} только часть ключа игры (например, выгрузка одной папки из regedit): загрузка заменила бы всё остальное. "
     "Такой файл добавляют двойным щелчком (regedit)."},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "a text for every Text");

```
на:
```cpp
     "Такий файл додають подвійним клацанням (regedit).",
     "В {1} только часть ключа игры (например, выгрузка одной папки из regedit): загрузка заменила бы всё остальное. "
     "Такой файл добавляют двойным щелчком (regedit)."},
    {"Input and window", "Ввід і вікно", "Ввод и окно"},
    {"dgVoodoo", "dgVoodoo", "dgVoodoo"},
    {"ROExt ({1})", "ROExt ({1})", "ROExt ({1})"},
    {"uaRO Pad ({1})", "uaRO Pad ({1})", "uaRO Pad ({1})"},
    {"No ROExt or uaRO Pad here: the game has its own mouse and window.",
     "Тут немає ні ROExt, ні uaRO Pad: у гри власні миша й вікно.",
     "Здесь нет ни ROExt, ни uaRO Pad: у игры свои мышь и окно."},
    {"Settings of", "Налаштування", "Настройки"},
    {"While the game runs these are grey: {1} reads its file as the game starts. Close the game to change them.",
     "Поки гра запущена, це сіре: {1} читає свій файл під час її запуску. Закрий гру, щоб змінювати.",
     "Пока игра запущена, это серое: {1} читает свой файл при её запуске. Закрой игру, чтобы менять."},
    {"Free mouse (MouseFreedom)", "Вільна миша (MouseFreedom)", "Свободная мышь (MouseFreedom)"},
    {"The mouse is not captured: the game's cursor follows the Windows cursor.",
     "Мишу не захоплено: курсор гри йде за курсором Windows.",
     "Мышь не захвачена: курсор игры идёт за курсором Windows."},
    {"Mouse buttons 3-5 as F24-F22", "Кнопки миші 3–5 як F24–F22", "Кнопки мыши 3–5 как F24–F22"},
    {"Skills can be put on them (RemapMouseButtons).", "На них можна покласти вміння (RemapMouseButtons).",
     "На них можно положить умения (RemapMouseButtons)."},
    {"Alt+F4 as a key, quit with Alt+PrintScreen", "Alt+F4 як клавіша, вихід — Alt+PrintScreen",
     "Alt+F4 как клавиша, выход — Alt+PrintScreen"},
    {"The window always on top", "Вікно завжди зверху", "Окно всегда сверху"},
    {"No borders, the window in its place (WindowLock)", "Без рамки, вікно на місці (WindowLock)",
     "Без рамки, окно на месте (WindowLock)"},
    {"Without its own size (Screen) ROExt makes the window as big as the screen without the taskbar.",
     "Без власного розміру («Екран») ROExt робить вікно на весь екран без панелі завдань.",
     "Без своего размера («Экран») ROExt делает окно на весь экран без панели задач."},
    {"Ctrl+W minimizes the window", "Ctrl+W згортає вікно", "Ctrl+W сворачивает окно"},
    {"CPU in the background (AutoFreeCPU)", "Процесор у фоні (AutoFreeCPU)", "Процессор в фоне (AutoFreeCPU)"},
    {"0: off; 1: on; more: a pause of that many ms while the window is inactive.",
     "0 — вимкнено, 1 — увімкнено, більше — пауза в стільки мс, поки вікно неактивне.",
     "0 — выключено, 1 — включено, больше — пауза во столько мс, пока окно неактивно."},
    {"Window X (WindowPosX)", "Вікно: X (WindowPosX)", "Окно: X (WindowPosX)"},
    {"Window Y (WindowPosY)", "Вікно: Y (WindowPosY)", "Окно: Y (WindowPosY)"},
    {"Only with ROExt's own window size (Screen page).", "Лише з власним розміром вікна ROExt (розділ «Екран»).",
     "Только со своим размером окна ROExt (раздел «Экран»)."},
    {"Code page (CodePage)", "Кодова сторінка (CodePage)", "Кодовая страница (CodePage)"},
    {"65001 (UTF-8) shows every language at once. Change it only if you know why.",
     "65001 (UTF-8) показує всі мови разом. Змінюй, лише якщо знаєш навіщо.",
     "65001 (UTF-8) показывает все языки сразу. Меняй, только если знаешь зачем."},
    {"don't change (-1)", "не змінювати (-1)", "не менять (-1)"},
    {"UTF-8 (65001)", "UTF-8 (65001)", "UTF-8 (65001)"},
    {"Cyrillic (1251)", "кирилиця (1251)", "кириллица (1251)"},
    {"Free mouse", "Вільна миша", "Свободная мышь"},
    {"Keep the cursor in the window", "Тримати курсор у вікні", "Держать курсор в окне"},
    {"While the game is active; Alt+Tab and Win let it go.", "Поки гра активна; Alt+Tab і Win його відпускають.",
     "Пока игра активна; Alt+Tab и Win его отпускают."},
    {"The click that activates the window acts in the game", "Клік, що активує вікно, діє в грі",
     "Щелчок, что активирует окно, действует в игре"},
    {"Middle mouse button", "Середня кнопка миші", "Средняя кнопка мыши"},
    {"Mouse button 4 (back)", "Кнопка миші 4 (назад)", "Кнопка мыши 4 (назад)"},
    {"Mouse button 5 (forward)", "Кнопка миші 5 (вперед)", "Кнопка мыши 5 (вперёд)"},
    {"nothing", "нічого", "ничего"},
    {"F1", "F1", "F1"},
    {"F2", "F2", "F2"},
    {"F3", "F3", "F3"},
    {"F4", "F4", "F4"},
    {"F5", "F5", "F5"},
    {"F6", "F6", "F6"},
    {"F7", "F7", "F7"},
    {"F8", "F8", "F8"},
    {"F9", "F9", "F9"},
    {"No title bar and borders (in a window)", "Без заголовка й рамки (у вікні)", "Без заголовка и рамки (в окне)"},
    {"The borderless window", "Вікно без рамки", "Окно без рамки"},
    {"in the centre", "по центру", "по центру"},
    {"where it was", "де було", "где было"},
    {"Pause in the background, ms", "Пауза у фоні, мс", "Пауза в фоне, мс"},
    {"While the game is behind other windows each frame sleeps this long; 0: never.",
     "Поки гра за іншими вікнами, кожен кадр спить стільки; 0 — ніколи.",
     "Пока игра за другими окнами, каждый кадр спит столько; 0 — никогда."},
    {"{1} (as in the file)", "{1} (як у файлі)", "{1} (как в файле)"},
    {"dgVoodoo works only with the dgVoodoo renderer (Renderer page): with uaRO Graphics these do nothing.",
     "dgVoodoo працює лише з рендером dgVoodoo (розділ «Рендер»): з uaRO Graphics це нічого не змінює.",
     "dgVoodoo работает только с рендером dgVoodoo (раздел «Рендер»): с uaRO Graphics это ничего не меняет."},
    {"Full screen (FullScreenMode)", "На весь екран (FullScreenMode)", "Во весь экран (FullScreenMode)"},
    {"Off: the game's full screen stays in a window.", "Вимкнено: повний екран гри лишається у вікні.",
     "Выключено: полный экран игры остаётся в окне."},
    {"Scaling", "Масштабування", "Масштабирование"},
    {"dgVoodoo decides", "вирішує dgVoodoo", "решает dgVoodoo"},
    {"centred", "по центру", "по центру"},
    {"stretched", "розтягнути", "растянуть"},
    {"centred, keep the aspect", "по центру, з пропорціями", "по центру, с пропорциями"},
    {"stretched, keep the aspect", "розтягнути з пропорціями", "растянуть с пропорциями"},
    {"stretched to 4:3", "розтягнути до 4:3", "растянуть до 4:3"},
    {"4:3 like a CRT", "4:3 як ЕПТ", "4:3 как ЭЛТ"},
    {"4:3 like a C64", "4:3 як C64", "4:3 как C64"},
    {"Keep the window's aspect", "Зберігати пропорції вікна", "Сохранять пропорции окна"},
    {"Capture the mouse", "Захоплювати мишу", "Захватывать мышь"},
    {"Free mouse (FreeMouse)", "Вільна миша (FreeMouse)", "Свободная мышь (FreeMouse)"},
    {"The mouse moves freely in the window when dgVoodoo scales the picture.",
     "Миша вільно рухається у вікні, коли dgVoodoo масштабує картинку.",
     "Мышь свободно движется в окне, когда dgVoodoo масштабирует картинку."},
    {"Brightness, %", "Яскравість, %", "Яркость, %"},
    {"Colour, %", "Колір, %", "Цвет, %"},
    {"Contrast, %", "Контраст, %", "Контраст, %"},
    {"Texture filtering", "Фільтрація текстур", "Фильтрация текстур"},
    {"as the game asks", "як просить гра", "как просит игра"},
    {"nearest point", "найближча точка", "ближайшая точка"},
    {"bilinear", "білінійна", "билинейная"},
    {"linear with mipmaps", "лінійна з міп-рівнями", "линейная с мип-уровнями"},
    {"trilinear", "трилінійна", "трилинейная"},
    {"anisotropic 2x", "анізотропна 2x", "анизотропная 2x"},
    {"anisotropic 4x", "анізотропна 4x", "анизотропная 4x"},
    {"anisotropic 8x", "анізотропна 8x", "анизотропная 8x"},
    {"anisotropic 16x", "анізотропна 16x", "анизотропная 16x"},
    {"Antialiasing", "Згладжування", "Сглаживание"},
    {"off", "вимкнено", "выключено"},
    {"2x", "2x", "2x"},
    {"3x", "3x", "3x"},
    {"4x", "4x", "4x"},
    {"8x", "8x", "8x"},
    {"16x", "16x", "16x"},
    {"Resolution", "Роздільність", "Разрешение"},
    {"the game's", "як у гри", "как у игры"},
    {"the screen's", "як у екрана", "как у экрана"},
    {"Always VSync", "Завжди VSync", "Всегда VSync"},
    {"dgVoodoo's watermark", "Водяний знак dgVoodoo", "Водяной знак dgVoodoo"},
    {"Check for an update", "Перевірити оновлення", "Проверить обновление"},
    {"Update to {1}", "Оновити до {1}", "Обновить до {1}"},
    {"Remove uaRO Graphics...", "Видалити uaRO Graphics…", "Удалить uaRO Graphics…"},
    {"checking GitHub...", "перевіряю GitHub…", "проверяю GitHub…"},
    {"the newest build", "найновіша збірка", "самая новая сборка"},
    {"GitHub has {1}", "на GitHub {1}", "на GitHub {1}"},
    {"GitHub cannot be checked (error {1}); update.bat still works",
     "GitHub не перевірити (помилка {1}); update.bat працює як раніше",
     "GitHub не проверить (ошибка {1}); update.bat работает как раньше"},
    {"Update uaRO Graphics to {1}? The settings app closes, and update.bat downloads and installs the build.",
     "Оновити uaRO Graphics до {1}? Утиліта закриється, а update.bat завантажить і встановить збірку.",
     "Обновить uaRO Graphics до {1}? Утилита закроется, а update.bat скачает и установит сборку."},
    {"Remove uaRO Graphics from this client? The settings app closes, and uninstall.bat puts dgVoodoo back; the "
     "backups of your settings stay.",
     "Видалити uaRO Graphics з цього клієнта? Утиліта закриється, а uninstall.bat поверне dgVoodoo; резервні копії "
     "налаштувань лишаться.",
     "Удалить uaRO Graphics из этого клиента? Утилита закроется, а uninstall.bat вернёт dgVoodoo; резервные копии "
     "настроек останутся."},
    {" Unsaved changes are lost.", " Незбережені зміни пропадуть.", " Несохранённые изменения пропадут."},
    {"Cannot start {1} (error {2}).", "Не вдалося запустити {1} (помилка {2}).",
     "Не удалось запустить {1} (ошибка {2})."},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "a text for every Text");

```

В `src/core/settings/strings.hpp` заменить:
```cpp
    ServiceGameSettings, GameSave, GameLoad, LayoutReset, GameSettingsNote, GameLoadAsk, LayoutResetAsk, GameSaved,
    GameLoaded, LayoutResetDone, ErrReadFile, FileFilterReg, DiagGpuValueCapped, SoundSetupDiffers,
    GameSettingsRunning, ErrPartialFile,
    Count
};

```
на:
```cpp
    ServiceGameSettings, GameSave, GameLoad, LayoutReset, GameSettingsNote, GameLoadAsk, LayoutResetAsk, GameSaved,
    GameLoaded, LayoutResetDone, ErrReadFile, FileFilterReg, DiagGpuValueCapped, SoundSetupDiffers,
    GameSettingsRunning, ErrPartialFile,
    PageInput, PageDgVoodoo, InputRoext, InputPad, InputNone, InputModule, FilesClosedOnly, RoextFreedom,
    MouseFreedomHint, RoextButtons, RoextButtonsHint, RoextAltF4, RoextOnTop, RoextLock, RoextLockHint, RoextMinimize,
    RoextFreeCpu, RoextFreeCpuHint, RoextPosX, RoextPosY, RoextPosHint, RoextCodePage, RoextCodePageHint, CodePageKeep,
    CodePageUtf8, CodePage1251, PadFreedom, PadLock, PadLockHint, PadFocusClick, PadMiddle, PadButton4, PadButton5,
    KeyNone, KeyF1, KeyF2, KeyF3, KeyF4, KeyF5, KeyF6, KeyF7, KeyF8, KeyF9, PadBorderless, PadPosition, PosCenter,
    PosKeep, PadSleep, PadSleepHint, FileValueRaw, DgvUaroNote, DgvFullscreen, DgvFullscreenHint, DgvScaling,
    ScaleUnspecified, ScaleCentered, ScaleStretched, ScaleCenteredAr, ScaleStretchedAr, Scale43, Scale43Crt, Scale43C64,
    DgvKeepAspect, DgvCaptureMouse, DgvFreeMouse, DgvFreeMouseHint, DgvBrightness, DgvColor, DgvContrast, DgvFiltering,
    AppDriven, FilterPoint, FilterBilinear, FilterLinearMip, FilterTrilinear, Aniso2, Aniso4, Aniso8, Aniso16,
    DgvAntialiasing, AaOff, Times2, Times3, Times4, Times8, Times16, DgvResolution, ResUnforced, ResMax, DgvVsync,
    DgvWatermark, UpdateCheck, UpdateRun, Uninstall, UpdateChecking, UpdateLatest, UpdateNewer, UpdateFailed, UpdateAsk,
    UninstallAsk, UnsavedLost, ErrLaunchFile,
    Count
};

```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `299 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `299 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_schema.cpp src/core/settings/schema.cpp src/core/settings/schema.hpp src/core/settings/strings.cpp src/core/settings/strings.hpp
git commit -q -F - <<'EOF'
Утилита: разделы «Ввод и окно», «dgVoodoo» и тексты 0.6.3

Список разделов по §6 этапа 2. Тексты 0.6.3 на трёх языках: ROExt, uaRO Pad,
dgVoodoo, проверка обновления, обновление и удаление. Записка файла настроек
игры — без логина, загрузка оставляет экран и звук ПК (ревью 0.6.2, M2, M3).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 2: Ядро: лог чёткого текста; файл настроек игры без логина; что принадлежит ПК (0.6.2, M1–M3)

Чёткий текст, выключенный на лету, пишет `crisp text: off (crisp_text = 0)`; разбор лога теперь забывает с ним и
масштаб, иначе «Диагностика» показывала `off …; k 1.750, …` (M1). `machineValues()` — значения ключа, принадлежащие
ПК: выбор `setup.exe` для экрана, звука и видеокарты, сохранённый логин и старый пароль. `settingsFileCopy` — ключ для
файла без логина и пароля (M3). `loadedPersonal` — личная копия после загрузки: значения файла, но значения ПК — как
сейчас, и ничего, равного общей копии (§4.2, M2).

**Files:**
- Modify: `src/core/settings/game_registry.cpp`, `src/core/settings/game_registry.hpp`, `src/core/settings/log_digest.cpp`, `tests/test_game_registry.cpp`, `tests/test_service.cpp`

**Interfaces:**
- Consumes: `effectiveCopy`, `setupValues` (`game_registry.hpp`); `digestLog`.
- Produces: `const std::vector<std::string>& machineValues()`, `RegCopy settingsFileCopy(const GameRegistry&)`,
  `RegCopy loadedPersonal(const GameRegistry&, const RegCopy& file)`.

- [ ] **Step 1: Падающие тесты**

В `tests/test_game_registry.cpp` заменить:
```cpp
    CHECK(effectiveValue(after, "UIRectInfo", "ITEMWNDINFO.Y") == RegValue::dword(50));
    CHECK(windowLayoutReset(GameRegistry{}).empty());  // no layout yet: the game makes its own
}
```
на:
```cpp
    CHECK(effectiveValue(after, "UIRectInfo", "ITEMWNDINFO.Y") == RegValue::dword(50));
    CHECK(windowLayoutReset(GameRegistry{}).empty());  // no layout yet: the game makes its own
}

TEST(game_registry_a_settings_file_leaves_the_login_out) {
    // 0.6.2 review M3: the saved login and a legacy password stay on this PC; the rest goes as the game reads it.
    GameRegistry r;
    r.common.exists = true;
    r.common.keys[""]["WIDTH"] = RegValue::dword(1920);
    r.common.keys[""]["PASSWORD"] = RegValue::sz("secret");
    r.personal.exists = true;
    r.personal.keys[""]["ID"] = RegValue::sz("player");
    r.personal.keys["Option"]["isEffectOn"] = RegValue::dword(0);
    const RegCopy c = settingsFileCopy(r);
    CHECK(c.find("", "WIDTH") != nullptr);
    CHECK(c.find("Option", "isEffectOn") != nullptr);
    CHECK(c.find("", "ID") == nullptr);
    CHECK(c.find("", "PASSWORD") == nullptr);
}

TEST(game_registry_loading_keeps_this_pc_s_screen_sound_and_login) {
    // 0.6.2 review M2: a file from another PC brings the player's settings, not that PC's screen, sound, video card or
    // login; and nothing equal to the common copy stays in the personal one (§4.2).
    GameRegistry r;
    r.common.exists = true;
    r.common.keys[""]["WIDTH"] = RegValue::dword(1920);
    r.common.keys[""]["FOG"] = RegValue::dword(1);
    r.personal.exists = true;
    r.personal.keys[""]["SOUNDMODE"] = RegValue::dword(2);
    r.personal.keys[""]["ID"] = RegValue::sz("here");
    RegCopy file;
    file.exists = true;
    file.keys[""]["WIDTH"] = RegValue::dword(2560);
    file.keys[""]["SOUNDMODE"] = RegValue::dword(0);
    file.keys[""]["GUIDDRIVER"] = RegValue::binary(std::vector<uint8_t>(16, 7));
    file.keys[""]["ID"] = RegValue::sz("there");
    file.keys[""]["FOG"] = RegValue::dword(1);  // the same as this PC's common copy
    file.keys["Option"]["isEffectOn"] = RegValue::dword(0);
    const RegCopy p = loadedPersonal(r, file);
    CHECK(p.exists);
    CHECK(p.find("Option", "isEffectOn") && *p.find("Option", "isEffectOn")->asDword() == 0u);
    CHECK(p.find("", "WIDTH") == nullptr);  // this PC's screen: the common copy's, as before
    CHECK(p.find("", "SOUNDMODE") && *p.find("", "SOUNDMODE")->asDword() == 2u);
    CHECK(p.find("", "GUIDDRIVER") == nullptr);
    CHECK(p.find("", "ID") && p.find("", "ID")->text == "here");
    CHECK(p.find("", "FOG") == nullptr);
}
```

В `tests/test_service.cpp` заменить:
```cpp
    CHECK_EQ(d.tail.size(), size_t(2));
    CHECK(d.tail[1].rfind("19:21:01.000", 0) == 0);
    CHECK(!digestLog("").found);
}

TEST(report_video_memory_at_the_32_bit_cap_is_a_floor) {
```
на:
```cpp
    CHECK_EQ(d.tail.size(), size_t(2));
    CHECK(d.tail[1].rfind("19:21:01.000", 0) == 0);
    CHECK(!digestLog("").found);
}

TEST(log_digest_a_live_crisp_text_off_has_no_scale) {
    // 0.6.2 review M1: turned off while the game runs, crisp text has no scale any more - the line of its last one
    // goes.
    const LogDigest d = digestLog(
        "19:20:01.001 INFO  uaRO Graphics 0.6.2 in C:\\Games\\uaRO\\uaRO.exe\r\n"
        "19:20:01.050 INFO  crisp text: on\r\n"
        "19:20:02.000 INFO  crisp text: k 1.750, 12 windows redrawn\r\n"
        "19:25:00.000 INFO  crisp text: off (crisp_text = 0)\r\n");
    CHECK(d.crisp == "off (crisp_text = 0)");
    CHECK(d.crispScale.empty());
}

TEST(report_video_memory_at_the_32_bit_cap_is_a_floor) {
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_game_registry.cpp:154:23: error: ‘settingsFileCopy’ was not declared in this scope`.

- [ ] **Step 3: Значения ПК и файл настроек**

В `src/core/settings/game_registry.cpp` заменить:
```cpp
#include "core/settings/game_registry.hpp"

#include <cctype>

namespace uagfx::settings {

```
на:
```cpp
#include "core/settings/game_registry.hpp"

#include <cctype>
#include <iterator>

namespace uagfx::settings {

```

В `src/core/settings/game_registry.cpp` заменить:
```cpp
    return out;
}

std::vector<RegWrite> windowLayoutReset(const GameRegistry& r) {
    std::vector<RegWrite> out;
    const RegCopy all = effectiveCopy(r);
```
на:
```cpp
    return out;
}

const std::vector<std::string>& machineValues() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> n = setupValues();
        for (const char* more : {"GUIDDRIVER", "GUIDDEVICE", "DEVICENAME", "DEVICECNT", "MODECNT", "ID", "PASSWORD"})
            n.push_back(more);
        return n;
    }();
    return names;
}

RegCopy settingsFileCopy(const GameRegistry& r) {
    RegCopy out = effectiveCopy(r);
    for (const char* name : {"ID", "PASSWORD"}) out.keys[""].erase(name);
    return out;
}

RegCopy loadedPersonal(const GameRegistry& r, const RegCopy& file) {
    RegCopy out = file;
    for (const std::string& name : machineValues()) {
        out.keys[""].erase(name);
        if (const RegValue* v = r.personal.find("", name)) out.keys[""][name] = *v;
    }
    for (auto& key : out.keys)
        for (auto v = key.second.begin(); v != key.second.end();) {
            const RegValue* common = r.common.find(key.first, v->first);
            v = common && *common == v->second ? key.second.erase(v) : std::next(v);
        }
    return out;
}

std::vector<RegWrite> windowLayoutReset(const GameRegistry& r) {
    std::vector<RegWrite> out;
    const RegCopy all = effectiveCopy(r);
```

В `src/core/settings/game_registry.hpp` заменить:
```cpp

std::optional<RegValue> effectiveValue(const GameRegistry& r, const std::string& subkey, const std::string& name);
bool gameKeyExists(const GameRegistry& r);  // in either copy
// The game's key as the game reads it: every key and value of either copy, the personal one winning (spec §8: the
// settings file holds it).
RegCopy effectiveCopy(const GameRegistry& r);

// A value of the key itself that both copies hold, differently (stage 2 spec §7): the personal one wins.
struct CopyDifference {
```
на:
```cpp

std::optional<RegValue> effectiveValue(const GameRegistry& r, const std::string& subkey, const std::string& name);
bool gameKeyExists(const GameRegistry& r);  // in either copy
// The game's key as the game reads it: every key and value of either copy, the personal one winning.
RegCopy effectiveCopy(const GameRegistry& r);
// The values of the key itself that belong to this PC, not to the player (0.6.2 review M2, M3): what setup.exe chose
// for the screen, the sound and the video card, and the saved login (and a legacy password).
const std::vector<std::string>& machineValues();
// The game's key for a settings file (spec §8): as the game reads it, without the saved login and a legacy password.
RegCopy settingsFileCopy(const GameRegistry& r);
// The personal copy after loading a settings file: the file's values but this PC's machine values as they are now, and
// none equal to the common copy (§4.2).
RegCopy loadedPersonal(const GameRegistry& r, const RegCopy& file);

// A value of the key itself that both copies hold, differently (stage 2 spec §7): the personal one wins.
struct CopyDifference {
```

- [ ] **Step 4: Падает только log_digest_a_live_crisp_text_off_has_no_scale**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `FAIL log_digest_a_live_crisp_text_off_has_no_scale`, `302 tests, 1 failed checks`.

- [ ] **Step 5: Разбор лога**

В `src/core/settings/log_digest.cpp` заменить:
```cpp
            d.renderer = msg.substr(10);
        } else if (msg.rfind("crisp text: ", 0) == 0) {
            const std::string what = msg.substr(12);
            if (what == "on" || what.rfind("off", 0) == 0)
                d.crisp = what;
            else if (what.rfind("k ", 0) == 0)
                d.crispScale = what;
        }
        const size_t frames = msg.find(" frames in ");
        const size_t open = msg.find(" (", frames == std::string::npos ? 0 : frames);
```
на:
```cpp
            d.renderer = msg.substr(10);
        } else if (msg.rfind("crisp text: ", 0) == 0) {
            const std::string what = msg.substr(12);
            if (what == "on" || what.rfind("off", 0) == 0) {
                d.crisp = what;
                d.crispScale.clear();  // a scale is the one of the crisp text after it (0.6.2 review M1)
            } else if (what.rfind("k ", 0) == 0) {
                d.crispScale = what;
            }
        }
        const size_t frames = msg.find(" frames in ");
        const size_t open = msg.find(" (", frames == std::string::npos ? 0 : frames);
```

- [ ] **Step 6: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `302 tests, 0 failed checks`.

- [ ] **Step 7: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `302 tests, 0 failed checks`.

- [ ] **Step 8: Коммит**

```bash
git add tests/test_game_registry.cpp tests/test_service.cpp src/core/settings/game_registry.cpp src/core/settings/game_registry.hpp src/core/settings/log_digest.cpp
git commit -q -F - <<'EOF'
Ядро: лог без старого масштаба; файл настроек игры без логина; значения ПК

Выключенный на лету чёткий текст уносит и свой масштаб (ревью 0.6.2, M1).
Файл настроек игры — без логина и старого пароля (M3); загрузка оставляет ПК
его экран, звук, видеокарту и логин и не кладёт в личную копию значения,
равные общей (§4.2, M2).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 3: Модель: первый запуск и перенос настроек игры (0.6.2, M2, M3, M5)

Без ключа игры «Применить» создаёт набор первого запуска (2D-звук, 48 звуков, §4.3), а страницы показывали значения
игры по умолчанию («выключен», 16): выбрать «выключен» было нельзя (M5). `readsAs()` — ключ, как его прочитает игра,
на первом запуске — с этим набором; от него считают `regWord` и `setRegWord`. «Сохранить» пишет `settingsFileCopy`,
«Загрузить» — `loadedPersonal` от ключа, прочитанного сейчас.

**Files:**
- Modify: `src/app/model.cpp`, `src/app/model.hpp`, `tests/test_model.cpp`

**Interfaces:**
- Consumes: Task 2 (`settingsFileCopy`, `loadedPersonal`); `firstRunWrites`, `applyWrites`.
- Produces: `settings::GameRegistry Model::readsAs() const` (private).

- [ ] **Step 1: Падающие тесты**

В `tests/test_model.cpp` заменить:
```cpp
    CHECK(m.preset() == Preset::Maximum);
}

TEST(model_skins_are_the_folders_of_skin_and_the_intro_a_file) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"skin"));
```
на:
```cpp
    CHECK(m.preset() == Preset::Maximum);
}

TEST(model_a_first_run_shows_what_apply_will_write) {
    // 0.6.2 review M5: with no game key at all Apply creates the first-run set (2D sound, 48 sounds, spec §4.3); the
    // pages show it, and «off» picked there is a change Apply writes over it.
    fake::Windows& w = fake::reset();
    app::Model m;
    CHECK(m.regWord(*findRegSetting("SOUNDMODE")) == "1");
    CHECK(m.regWord(*findRegSetting("NUMSAMPLETYPE")) == "0");
    m.setRegWord(*findRegSetting("SOUNDMODE"), "0");
    CHECK_EQ(m.pendingCount(), size_t(1));
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(effectiveDword("SOUNDMODE"), 0);
    CHECK(w.registry.personal.exists || w.registry.common.exists);
}

TEST(model_skins_are_the_folders_of_skin_and_the_intro_a_file) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"skin"));
```

В `tests/test_model.cpp` заменить:
```cpp
    CHECK(saved.find("", "PASSWORD") == nullptr);
}

TEST(model_game_settings_load_and_reset_wait_for_the_game_to_close) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
```
на:
```cpp
    CHECK(saved.find("", "PASSWORD") == nullptr);
}

TEST(model_a_settings_file_moves_the_player_s_settings_not_the_pc_s) {
    // 0.6.2 review M2, M3: saved without the login; loaded on another PC, that PC keeps its screen, sound and login.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.personal.exists = true;
    w.registry.personal.keys[""]["ID"] = RegValue::sz("player");
    w.registry.personal.keys["Option"]["isEffectOn"] = RegValue::dword(0);
    app::Model m;
    const std::wstring file = fake::path(L"mine.reg");
    CHECK(m.saveGameSettings(file, Lang::En).empty());
    CHECK(w.files[file].find("player") == std::string::npos);
    w.registry = GameRegistry{};  // another PC, with a screen and a login of its own
    fake::gameKey(w.registry.common, 2560, 1440, false);
    w.registry.personal.exists = true;
    w.registry.personal.keys[""]["ID"] = RegValue::sz("other");
    app::Model there;
    CHECK(there.loadGameSettings(file, Lang::En).empty());
    CHECK_EQ(effectiveDword("WIDTH"), 2560);
    const RegValue* id = w.registry.personal.find("", "ID");
    CHECK(id && id->text == "other");
    CHECK(w.registry.personal.find("Option", "isEffectOn") != nullptr);
}

TEST(model_game_settings_load_and_reset_wait_for_the_game_to_close) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: `FAIL model_a_first_run_shows_what_apply_will_write`, `FAIL model_a_settings_file_moves_the_player_s_settings_not_the_pc_s`, `304 tests, 7 failed checks`.

- [ ] **Step 3: Реализация**

В `src/app/model.cpp` заменить:
```cpp
    return Size{dword(now_.registry.common, "WIDTH"), dword(now_.registry.common, "HEIGHT")};
}

std::string Model::regWord(const RegSetting& s) const {
    const auto it = reg_.find(regKey(s));
    return it != reg_.end() ? it->second : regWordOf(s, now_.registry);
}

void Model::setRegWord(const RegSetting& s, const std::string& word) {
    if (!validRegWord(s, word)) return;
    if (word == regWordOf(s, now_.registry))
        reg_.erase(regKey(s));
    else
        reg_[regKey(s)] = word;
```
на:
```cpp
    return Size{dword(now_.registry.common, "WIDTH"), dword(now_.registry.common, "HEIGHT")};
}

GameRegistry Model::readsAs() const {
    if (gameKeyExists(now_.registry)) return now_.registry;
    GameRegistry r = now_.registry;
    const ScreenPlan p = screenPlan();
    applyWrites(r.personal, firstRunWrites(p.game.width, p.game.height, p.fullscreen));
    r.personal.exists = true;
    return r;
}

std::string Model::regWord(const RegSetting& s) const {
    const auto it = reg_.find(regKey(s));
    return it != reg_.end() ? it->second : regWordOf(s, readsAs());
}

void Model::setRegWord(const RegSetting& s, const std::string& word) {
    if (!validRegWord(s, word)) return;
    if (word == regWordOf(s, readsAs()))
        reg_.erase(regKey(s));
    else
        reg_[regKey(s)] = word;
```

В `src/app/model.cpp` заменить:
```cpp
    GameRegistry r;  // as it is now: the game wrote its hotbars at its last map change
    r.personal = readPersonal();
    r.common = readCommon();
    RegCopy all = effectiveCopy(r);
    // The client never writes PASSWORD and drops it when it reads it (registry.md): an old one stays out of a file a
    // player may pass on (0.6.2 review).
    all.keys[""].erase("PASSWORD");
    const std::string text = utf8ToRegFile(toRegFile(all, gameKeyPaths().personalRoot));
    return writeFileAtomic(file, text) ? L"" : wtrf(l, Text::ErrWriteFile, {utf8(file)});
}

```
на:
```cpp
    GameRegistry r;  // as it is now: the game wrote its hotbars at its last map change
    r.personal = readPersonal();
    r.common = readCommon();
    // Without the login and a legacy password: a player may pass the file on (0.6.2 review M3).
    const std::string text = utf8ToRegFile(toRegFile(settingsFileCopy(r), gameKeyPaths().personalRoot));
    return writeFileAtomic(file, text) ? L"" : wtrf(l, Text::ErrWriteFile, {utf8(file)});
}

```

В `src/app/model.cpp` заменить:
```cpp
    if (copy.keys.find("") == copy.keys.end()) return wtrf(l, Text::ErrPartialFile, {utf8(file)});
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = replacePersonal(copy);
    reg_.clear();  // the game's key is another one now: what the pages changed was for the old one
    screen_.reset();
    screenAsSetup_ = false;
```
на:
```cpp
    if (copy.keys.find("") == copy.keys.end()) return wtrf(l, Text::ErrPartialFile, {utf8(file)});
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    GameRegistry r;  // as it is now
    r.personal = readPersonal();
    r.common = readCommon();
    // This PC keeps its screen, sound, video card and login: the file brings the player's settings (0.6.2 review M2).
    const std::string failed = replacePersonal(loadedPersonal(r, copy));
    reg_.clear();  // the game's key is another one now: what the pages changed was for the old one
    screen_.reset();
    screenAsSetup_ = false;
```

В `src/app/model.hpp` заменить:
```cpp
    // The ini as it is on disk now - what Notepad or use-*.bat wrote since load() stays - or the shipped defaults
    // when there is none.
    std::string currentIni() const;
    std::wstring writeIni(const std::vector<settings::IniEdit>& edits, settings::Lang l);
    std::wstring backupOnce(settings::Lang l, bool prune = true);  // prune: only the 20 newest stay (spec §4.6)
    // Each value on its own (spec §4.5): "" or the ones that failed, "NAME (error N), ..."; the others are written.
```
на:
```cpp
    // The ini as it is on disk now - what Notepad or use-*.bat wrote since load() stays - or the shipped defaults
    // when there is none.
    std::string currentIni() const;
    // The game's key as the game will read it: on a first run the set Apply creates (spec §4.3), which the pages show
    // (0.6.2 review M5).
    settings::GameRegistry readsAs() const;
    std::wstring writeIni(const std::vector<settings::IniEdit>& edits, settings::Lang l);
    std::wstring backupOnce(settings::Lang l, bool prune = true);  // prune: only the 20 newest stay (spec §4.6)
    // Each value on its own (spec §4.5): "" or the ones that failed, "NAME (error N), ..."; the others are written.
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `304 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `304 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_model.cpp src/app/model.cpp src/app/model.hpp
git commit -q -F - <<'EOF'
Утилита: первый запуск показывает то, что запишет «Применить»; перенос настроек игры

Без ключа игры страницы показывают набор первого запуска (ревью 0.6.2, M5).
Сохранение — без логина, загрузка оставляет ПК его экран, звук и логин
(M2, M3).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 4: Ключи ROExt, uaRO Pad и dgVoodoo (§2.3, §2.4)

`src/core/settings/file_settings.*` — 32 ключа чужих файлов: 10 ROExt (что они делают — из `dinput8.ini` его старой
сборки; `WindowWidth`/`WindowHeight` — раздела «Экран»), 9 uaRO Pad (окно и мышь, остальное — в его меню) и 13
dgVoodoo со списками значений из комментариев его `dgVoodoo.conf` 2.55. Слово — значение, как его пишет файл; флажок
включён при своём «on» в любом регистре (у ROExt — любое число, кроме 0); значение вне списка остаётся как есть;
ползунок ограничен. Сборки ROExt читают разные ключи (у `dinput8.dll` клиента нет `MouseFreedom` и `KeyRemap`):
`identifyModule` запоминает те из 13 известных, что названы в DLL отдельной строкой между нулями — так сборка хранит
имена, которые читает; имя внутри другой строки (импорт `IsValidCodePage`) не в счёт.

**Files:**
- Create: `src/core/settings/file_settings.cpp`, `src/core/settings/file_settings.hpp`, `tests/test_file_settings.cpp`
- Modify: `src/core/settings/input_module.cpp`, `src/core/settings/input_module.hpp`, `tests/test_input_module.cpp`

**Interfaces:**
- Consumes: `Control`, `Text`; `identifyModule`, `ModuleInfo` (`input_module.hpp`); `readIni` (`ini_edit.hpp`) — в
  тестах.
- Produces: `enum class SettingsFile { Roext, UaroPad, DgVoodoo }`, `struct FileSetting`, `fileSettings()`,
  `fileKey(s)`, `findFileSetting(file, key)`, `fileProgram(file)`, `fileWordOf(s, ini)`, `validFileWord(s, word)`;
  `std::vector<std::string> ModuleInfo::keys` — ключи ROExt, названные в DLL, в порядке списка.

- [ ] **Step 1: Падающий тест**

`tests/test_file_settings.cpp`:
```cpp
// Other programs' settings files on the settings app's pages (stage 2 spec §2.3, §2.4; release 0.6.3).
#include <set>
#include <string>
#include <utility>

#include "core/settings/file_settings.hpp"
#include "core/settings/ini_edit.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

const FileSetting& setting(SettingsFile file, const char* key) { return *findFileSetting(file, key); }

}  // namespace

TEST(file_settings_read_the_values_as_the_programs_do) {
    const auto roext = readIni("[ROExt]\r\nMouseFreedom=1\r\nAutoFreeCPU=4\r\nCodePage=437\r\n");
    CHECK(fileWordOf(setting(SettingsFile::Roext, "roext.mousefreedom"), roext) == "1");
    CHECK(fileWordOf(setting(SettingsFile::Roext, "roext.remapaltf4"), roext) == "0");  // missing: ROExt's own
    CHECK(fileWordOf(setting(SettingsFile::Roext, "roext.autofreecpu"), roext) == "4");
    CHECK(fileWordOf(setting(SettingsFile::Roext, "roext.codepage"), roext) == "437");  // not in the list: kept
    const auto pad = readIni("[mouse]\r\nmiddle = F3          ; mouse button 3\r\n");
    CHECK(fileWordOf(setting(SettingsFile::UaroPad, "mouse.middle"), pad) == "F3");
    CHECK(fileWordOf(setting(SettingsFile::UaroPad, "mouse.freedom"), pad) == "1");
    const auto dgv = readIni("[General]\r\nFullScreenMode = TRUE\r\nScalingMode = Stretched_AR\r\nBrightness = 300\r\n"
                             "[DirectX]\r\nFiltering = 16\r\n");
    CHECK(fileWordOf(setting(SettingsFile::DgVoodoo, "general.fullscreenmode"), dgv) == "true");  // any case
    CHECK(fileWordOf(setting(SettingsFile::DgVoodoo, "general.scalingmode"), dgv) == "stretched_ar");
    CHECK(fileWordOf(setting(SettingsFile::DgVoodoo, "directx.filtering"), dgv) == "16");
    CHECK(fileWordOf(setting(SettingsFile::DgVoodoo, "general.brightness"), dgv) == "150");  // a slider is clamped
    CHECK(fileWordOf(setting(SettingsFile::DgVoodoo, "general.contrast"), dgv) == "100");
}

TEST(file_settings_take_only_their_words) {
    CHECK(validFileWord(setting(SettingsFile::Roext, "roext.mousefreedom"), "1"));
    CHECK(!validFileWord(setting(SettingsFile::Roext, "roext.mousefreedom"), "2"));
    CHECK(validFileWord(setting(SettingsFile::Roext, "roext.autofreecpu"), "1000"));
    CHECK(!validFileWord(setting(SettingsFile::Roext, "roext.autofreecpu"), "-1"));
    CHECK(validFileWord(setting(SettingsFile::Roext, "roext.windowposx"), "-50"));
    CHECK(!validFileWord(setting(SettingsFile::Roext, "roext.windowposx"), "5x"));
    CHECK(validFileWord(setting(SettingsFile::DgVoodoo, "general.fullscreenmode"), "false"));
    CHECK(!validFileWord(setting(SettingsFile::DgVoodoo, "general.fullscreenmode"), "1"));
    CHECK(validFileWord(setting(SettingsFile::DgVoodoo, "general.scalingmode"), "centered"));
    CHECK(!validFileWord(setting(SettingsFile::DgVoodoo, "general.scalingmode"), "zoomed"));
    CHECK(validFileWord(setting(SettingsFile::DgVoodoo, "general.brightness"), "105"));
    CHECK(!validFileWord(setting(SettingsFile::DgVoodoo, "general.brightness"), "155"));
    CHECK(validFileWord(setting(SettingsFile::UaroPad, "mouse.button5"), "F9"));
    CHECK(!validFileWord(setting(SettingsFile::UaroPad, "mouse.button5"), "F10"));
}

TEST(file_settings_each_once_and_whole) {
    std::set<std::pair<SettingsFile, std::string>> keys;
    for (const FileSetting& s : fileSettings()) {
        CHECK(keys.insert({s.file, fileKey(s)}).second);
        CHECK(findFileSetting(s.file, fileKey(s)) == &s);
        if (s.control == Control::Check) CHECK_EQ(s.tokens.size(), size_t(2));
        if (s.control == Control::Choice) CHECK_EQ(s.tokens.size(), s.choices.size());
        CHECK(validFileWord(s, fileWordOf(s, {})));  // the program's own value is one a page can show
    }
    CHECK_EQ(keys.size(), size_t(32));
    CHECK(findFileSetting(SettingsFile::Roext, "roext.nothing") == nullptr);
    CHECK(std::string(fileProgram(SettingsFile::DgVoodoo)) == "dgVoodoo");
}
```

В `tests/test_input_module.cpp` заменить:
```cpp
    CHECK(!loadedByGame("dinput.dll.bak"));
}

TEST(input_module_reads_roexts_window) {
    const RoextWindow a = roextWindow(readIni("[ROExt]\r\nWindowLock=1\r\nWindowWidth=1600\r\nWindowHeight=863\r\n"));
    CHECK(a.active);
```
на:
```cpp
    CHECK(!loadedByGame("dinput.dll.bak"));
}

TEST(input_module_lists_the_roext_keys_its_build_names) {
    // Spec §2.3: a build reads the keys it names, each a string of its own. The client's dinput8.dll names no
    // MouseFreedom; a name inside another string - an import - is none.
    const ModuleInfo old =
        identifyModule("dinput8.dll", dll({"ROExt", "MinimizeKey", "WindowLock", "IsValidCodePage"}));
    CHECK((old.keys == std::vector<std::string>{"WindowLock", "MinimizeKey"}));
    const ModuleInfo newer = identifyModule("dinput.dll", dll({"ROExt", "KeyRemap", "MouseFreedom"}));
    CHECK((newer.keys == std::vector<std::string>{"MouseFreedom", "KeyRemap"}));
    CHECK(identifyModule("dinput.dll", dll({"uaro-pad.ini", "MouseFreedom"})).keys.empty());  // ROExt's only
}

TEST(input_module_reads_roexts_window) {
    const RoextWindow a = roextWindow(readIni("[ROExt]\r\nWindowLock=1\r\nWindowWidth=1600\r\nWindowHeight=863\r\n"));
    CHECK(a.active);
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает; первой строкой — ошибка того из тестов, что собрался первым: `…/tests/test_file_settings.cpp:6:10: fatal error: core/settings/file_settings.hpp: No such file or directory`, `…/tests/test_input_module.cpp:50:16: error: ‘const struct uagfx::settings::ModuleInfo’ has no member named ‘keys’`.

- [ ] **Step 3: Реализация**

`src/core/settings/file_settings.cpp`:
```cpp
#include "core/settings/file_settings.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace uagfx::settings {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool integer(const std::string& word, long& n) {
    char* end = nullptr;
    n = std::strtol(word.c_str(), &end, 10);
    return !word.empty() && *end == '\0';
}

FileSetting check(SettingsFile f, const char* section, const char* key, std::vector<std::string> tokens,
                  const char* fallback, Text label, Text hint) {
    return FileSetting{f, section, key, Control::Check, std::move(tokens), {}, 0, 1, 1, fallback, label, hint};
}

FileSetting choice(SettingsFile f, const char* section, const char* key, std::vector<std::string> tokens,
                   std::vector<Text> choices, const char* fallback, Text label, Text hint) {
    return FileSetting{f, section, key, Control::Choice, std::move(tokens), std::move(choices), 0, 0, 0, fallback,
                       label, hint};
}

FileSetting number(SettingsFile f, const char* section, const char* key, Control control, int min, int max, int step,
                   const char* fallback, Text label, Text hint) {
    return FileSetting{f, section, key, control, {}, {}, min, max, step, fallback, label, hint};
}

}  // namespace

const std::vector<FileSetting>& fileSettings() {
    using F = SettingsFile;
    using T = Text;
    const std::vector<std::string> flag{"0", "1"};
    const std::vector<std::string> boolean{"false", "true"};
    const std::vector<std::string> keys{"none", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9"};
    const std::vector<Text> keyNames{T::KeyNone, T::KeyF1, T::KeyF2, T::KeyF3, T::KeyF4,
                                     T::KeyF5,   T::KeyF6, T::KeyF7, T::KeyF8, T::KeyF9};
    static const std::vector<FileSetting> list{
        // ROExt (spec §2.3; its old build's dinput8.ini says what they do): 0 is off, 1 on. A page shows the ones the
        // build the game loads names (ModuleInfo::keys); WindowWidth and WindowHeight are the Screen page's.
        check(F::Roext, "ROExt", "MouseFreedom", flag, "0", T::RoextFreedom, T::MouseFreedomHint),
        check(F::Roext, "ROExt", "RemapMouseButtons", flag, "0", T::RoextButtons, T::RoextButtonsHint),
        check(F::Roext, "ROExt", "RemapAltF4", flag, "0", T::RoextAltF4, T::RoextAltF4),
        check(F::Roext, "ROExt", "WindowOnTop", flag, "0", T::RoextOnTop, T::RoextOnTop),
        check(F::Roext, "ROExt", "WindowLock", flag, "0", T::RoextLock, T::RoextLockHint),
        check(F::Roext, "ROExt", "MinimizeKey", flag, "0", T::RoextMinimize, T::RoextMinimize),
        number(F::Roext, "ROExt", "AutoFreeCPU", Control::Number, 0, 1000, 1, "0", T::RoextFreeCpu,
               T::RoextFreeCpuHint),
        number(F::Roext, "ROExt", "WindowPosX", Control::Number, -10000, 10000, 1, "0", T::RoextPosX, T::RoextPosHint),
        number(F::Roext, "ROExt", "WindowPosY", Control::Number, -10000, 10000, 1, "0", T::RoextPosY, T::RoextPosHint),
        choice(F::Roext, "ROExt", "CodePage", {"-1", "65001", "1251"},
               {T::CodePageKeep, T::CodePageUtf8, T::CodePage1251}, "-1", T::RoextCodePage, T::RoextCodePageHint),
        // uaRO Pad (its uaro-pad.ini): its window and mouse; the rest is in its own menu in the game.
        check(F::UaroPad, "mouse", "freedom", flag, "1", T::PadFreedom, T::MouseFreedomHint),
        check(F::UaroPad, "mouse", "lock", flag, "0", T::PadLock, T::PadLockHint),
        check(F::UaroPad, "mouse", "focus_click", flag, "0", T::PadFocusClick, T::PadFocusClick),
        check(F::UaroPad, "window", "borderless", flag, "0", T::PadBorderless, T::PadBorderless),
        choice(F::UaroPad, "mouse", "middle", keys, keyNames, "none", T::PadMiddle, T::PadMiddle),
        choice(F::UaroPad, "mouse", "button4", keys, keyNames, "none", T::PadButton4, T::PadButton4),
        choice(F::UaroPad, "mouse", "button5", keys, keyNames, "none", T::PadButton5, T::PadButton5),
        choice(F::UaroPad, "window", "position", {"center", "keep"}, {T::PosCenter, T::PosKeep}, "center",
               T::PadPosition, T::PadPosition),
        number(F::UaroPad, "window", "background_sleep_ms", Control::Slider, 0, 100, 1, "10", T::PadSleep,
               T::PadSleepHint),
        // dgVoodoo (spec §2.4): the value lists of its own dgVoodoo.conf, version 2.55.
        check(F::DgVoodoo, "General", "FullScreenMode", boolean, "true", T::DgvFullscreen, T::DgvFullscreenHint),
        check(F::DgVoodoo, "General", "KeepWindowAspectRatio", boolean, "true", T::DgvKeepAspect, T::DgvKeepAspect),
        check(F::DgVoodoo, "General", "CaptureMouse", boolean, "true", T::DgvCaptureMouse, T::DgvCaptureMouse),
        check(F::DgVoodoo, "GeneralExt", "FreeMouse", boolean, "false", T::DgvFreeMouse, T::DgvFreeMouseHint),
        check(F::DgVoodoo, "DirectX", "ForceVerticalSync", boolean, "false", T::DgvVsync, T::DgvVsync),
        check(F::DgVoodoo, "DirectX", "dgVoodooWatermark", boolean, "true", T::DgvWatermark, T::DgvWatermark),
        choice(F::DgVoodoo, "General", "ScalingMode",
               {"unspecified", "centered", "stretched", "centered_ar", "stretched_ar", "stretched_4_3",
                "stretched_4_3_crt", "stretched_4_3_c64"},
               {T::ScaleUnspecified, T::ScaleCentered, T::ScaleStretched, T::ScaleCenteredAr, T::ScaleStretchedAr,
                T::Scale43, T::Scale43Crt, T::Scale43C64},
               "unspecified", T::DgvScaling, T::DgvScaling),
        choice(F::DgVoodoo, "DirectX", "Filtering",
               {"appdriven", "pointsampled", "bilinear", "linearmip", "trilinear", "2", "4", "8", "16"},
               {T::AppDriven, T::FilterPoint, T::FilterBilinear, T::FilterLinearMip, T::FilterTrilinear, T::Aniso2,
                T::Aniso4, T::Aniso8, T::Aniso16},
               "appdriven", T::DgvFiltering, T::DgvFiltering),
        choice(F::DgVoodoo, "DirectX", "Antialiasing", {"appdriven", "off", "2x", "4x", "8x", "16x"},
               {T::AppDriven, T::AaOff, T::Times2, T::Times4, T::Times8, T::Times16}, "appdriven", T::DgvAntialiasing,
               T::DgvAntialiasing),
        choice(F::DgVoodoo, "DirectX", "Resolution", {"unforced", "max", "2x", "3x", "4x"},
               {T::ResUnforced, T::ResMax, T::Times2, T::Times3, T::Times4}, "unforced", T::DgvResolution,
               T::DgvResolution),
        number(F::DgVoodoo, "General", "Brightness", Control::Slider, 50, 150, 5, "100", T::DgvBrightness,
               T::DgvBrightness),
        number(F::DgVoodoo, "General", "Color", Control::Slider, 50, 150, 5, "100", T::DgvColor, T::DgvColor),
        number(F::DgVoodoo, "General", "Contrast", Control::Slider, 50, 150, 5, "100", T::DgvContrast, T::DgvContrast),
    };
    return list;
}

std::string fileKey(const FileSetting& s) { return lower(s.section) + "." + lower(s.key); }

const FileSetting* findFileSetting(SettingsFile file, const std::string& key) {
    for (const FileSetting& s : fileSettings())
        if (s.file == file && fileKey(s) == lower(key)) return &s;
    return nullptr;
}

const char* fileProgram(SettingsFile file) {
    switch (file) {
        case SettingsFile::Roext: return "ROExt";
        case SettingsFile::UaroPad: return "uaRO Pad";
        case SettingsFile::DgVoodoo: break;
    }
    return "dgVoodoo";
}

std::string fileWordOf(const FileSetting& s, const std::map<std::string, std::string>& ini) {
    const auto it = ini.find(fileKey(s));
    const std::string value = it != ini.end() ? it->second : std::string(s.fallback);
    long n = 0;
    switch (s.control) {
        case Control::Check: {
            const bool on = lower(value) == lower(s.tokens[1]) || (s.tokens[1] == "1" && integer(value, n) && n != 0);
            return s.tokens[on ? 1 : 0];
        }
        case Control::Choice:
            for (const std::string& t : s.tokens)
                if (lower(t) == lower(value)) return t;
            return value;
        case Control::Slider:
            return integer(value, n) ? std::to_string(std::clamp(n, long(s.min), long(s.max))) : s.fallback;
        case Control::Number:
            return integer(value, n) && n >= s.min && n <= s.max ? std::to_string(n) : std::string(s.fallback);
        case Control::Line: break;
    }
    return value;
}

bool validFileWord(const FileSetting& s, const std::string& word) {
    long n = 0;
    switch (s.control) {
        case Control::Check: return word == s.tokens[0] || word == s.tokens[1];
        case Control::Choice: return std::find(s.tokens.begin(), s.tokens.end(), word) != s.tokens.end();
        case Control::Slider:
        case Control::Number:
            return integer(word, n) && n >= s.min && n <= s.max && (n - s.min) % std::max(s.step, 1) == 0;
        case Control::Line: break;
    }
    return false;
}

}  // namespace uagfx::settings
```

`src/core/settings/file_settings.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/settings/schema.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings {

// The settings file of another program a page edits (stage 2 spec §2.3, §2.4; release 0.6.3): ROExt's or uaRO Pad's
// ini next to uaRO.exe, dgVoodoo.conf.
enum class SettingsFile : uint8_t { Roext, UaroPad, DgVoodoo };

// One key of such a file. A page's word is the value as the file writes it - "1", "true", "F3" - else the program's
// own one; a value a list does not have stays as it is until the player picks another.
struct FileSetting {
    SettingsFile file;
    const char* section;  // as the program writes it: "ROExt", "mouse", "General"
    const char* key;
    Control control;                  // Check, Choice, Slider or Number
    std::vector<std::string> tokens;  // Check: off, on; Choice: the values in the order of `choices`
    std::vector<Text> choices;
    int min;  // Slider, Number
    int max;
    int step;
    const char* fallback;  // the program's own value while the key is missing
    Text label;
    Text hint;  // the label itself when there is none
};
const std::vector<FileSetting>& fileSettings();
std::string fileKey(const FileSetting& s);  // "general.fullscreenmode": as readIni gives it
const FileSetting* findFileSetting(SettingsFile file, const std::string& key);
const char* fileProgram(SettingsFile file);  // "ROExt", "uaRO Pad", "dgVoodoo": for the pages' notes
// What the program reads now, from readIni of its file: the value, else its own one. A check is on at its "on" token in
// any case, or any number but 0 when that token is "1"; a choice's value is the token it matches in any case, else
// itself; a slider's is clamped.
std::string fileWordOf(const FileSetting& s, const std::map<std::string, std::string>& ini);
bool validFileWord(const FileSetting& s, const std::string& word);

}  // namespace uagfx::settings
```

В `src/core/settings/input_module.cpp` заменить:
```cpp
bool contains(const std::vector<uint8_t>& data, const char* text) {
    const size_t n = std::strlen(text);
    return std::search(data.begin(), data.end(), text, text + n) != data.end();
}

// The first NUL-terminated ASCII string ".\<name>.ini" in the DLL.
```
на:
```cpp
bool contains(const std::vector<uint8_t>& data, const char* text) {
    const size_t n = std::strlen(text);
    return std::search(data.begin(), data.end(), text, text + n) != data.end();
}

// The ROExt keys of its builds: the old dinput8.dll's, MouseFreedom and KeyRemap of the newer one's dinput.ini.
const char* const kRoextKeys[] = {"MouseFreedom", "KeyRemap",     "RemapMouseButtons", "RemapAltF4", "WindowOnTop",
                                  "WindowLock",   "MinimizeKey",  "AutoFreeCPU",       "WindowPosX", "WindowPosY",
                                  "WindowWidth",  "WindowHeight", "CodePage"};

// Whether the DLL holds `text` as a string of its own, between NULs: the way a build keeps the key names it reads
// ("CodePage" inside the import IsValidCodePage is none).
bool namesString(const std::vector<uint8_t>& data, const char* text) {
    const size_t n = std::strlen(text);
    for (auto it = data.begin(); (it = std::search(it, data.end(), text, text + n)) != data.end(); ++it) {
        const size_t i = size_t(it - data.begin());
        if ((i == 0 || data[i - 1] == 0) && i + n < data.size() && data[i + n] == 0) return true;
    }
    return false;
}

// The first NUL-terminated ASCII string ".\<name>.ini" in the DLL.
```

В `src/core/settings/input_module.cpp` заменить:
```cpp
            const size_t dot = fileName.find('.');
            m.ini = fileName.substr(0, dot) + ".ini";
        }
    }
    return m;
}
```
на:
```cpp
            const size_t dot = fileName.find('.');
            m.ini = fileName.substr(0, dot) + ".ini";
        }
        for (const char* key : kRoextKeys)
            if (namesString(dll, key)) m.keys.push_back(key);
    }
    return m;
}
```

В `src/core/settings/input_module.hpp` заменить:
```cpp
struct ModuleInfo {
    ModuleKind kind = ModuleKind::Other;
    std::string ini;  // the file it reads: ROExt names it in the DLL (".\dinput8.ini"), else <its name>.ini
};
ModuleInfo identifyModule(const std::string& fileName, const std::vector<uint8_t>& dll);

```
на:
```cpp
struct ModuleInfo {
    ModuleKind kind = ModuleKind::Other;
    std::string ini;  // the file it reads: ROExt names it in the DLL (".\dinput8.ini"), else <its name>.ini
    std::vector<std::string> keys;  // ROExt: the keys of its ini this build names ("WindowLock"), in a fixed order
};
ModuleInfo identifyModule(const std::string& fileName, const std::vector<uint8_t>& dll);

```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `308 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `308 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_file_settings.cpp tests/test_input_module.cpp src/core/settings/file_settings.cpp src/core/settings/file_settings.hpp src/core/settings/input_module.cpp src/core/settings/input_module.hpp
git commit -q -F - <<'EOF'
Ядро: ключи ROExt, uaRO Pad и dgVoodoo

32 ключа чужих файлов: ROExt, uaRO Pad и dgVoodoo со значениями из их ini и
dgVoodoo.conf 2.55. Значение вне списка остаётся как есть. Какие ключи ROExt
читает его сборка, говорят строки её DLL.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 5: Модель: чужие файлы настроек (§4.1, §2.4)

Слово чужого файла ждёт «Применить», как ini; равное тому, что в файле, — не изменение. Пока игра запущена, все они
ждут её закрытия (программы читают файлы при её запуске, §4.1), ответ — `ApplyWaits`. ROExt и uaRO Pad — те, что
загружает игра (`.bak` не в счёт); размер окна из «Экрана» идёт в тот же ini ROExt одной записью. `dgVoodoo.conf`
пишется в обе копии: какую читает dgVoodoo из подпапки, не проверено (§2.4). Каждый файл читается в момент записи,
меняются только свои значения. «Восстановить» снимает и эти изменения. Раздел покажет ключ ROExt, лишь если его
называет сборка, которую загружает игра (`fileShown`); ключи uaRO Pad и dgVoodoo — все.

**Files:**
- Modify: `src/app/model.cpp`, `src/app/model.hpp`, `tests/test_model.cpp`

**Interfaces:**
- Consumes: Task 4 (`FileSetting`, `fileWordOf`, `validFileWord`, `fileKey`, `fileProgram`, `fileSettings`,
  `ModuleInfo::keys`).
- Produces: `std::string Model::fileWord(const FileSetting&) const`, `void Model::setFileWord(const FileSetting&, const
  std::string&)`, `bool Model::hasFile(SettingsFile) const`, `bool Model::fileShown(const FileSetting&) const`;
  `Snapshot::padModule`, `Snapshot::dgvoodooConf`, `Paths::dgvConf`, `Paths::dgvConfInside`.

- [ ] **Step 1: Падающие тесты**

В `tests/test_model.cpp` заменить:
```cpp
// The settings app's model (stage 2 spec §4) over the stand-ins of tests/fake_windows.cpp.
#include "app/model.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/reg_file.hpp"
```
на:
```cpp
// The settings app's model (stage 2 spec §4) over the stand-ins of tests/fake_windows.cpp.
#include "app/model.hpp"
#include "core/settings/file_settings.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/reg_file.hpp"
```

В `tests/test_model.cpp` заменить:
```cpp
    w.gameRunning = false;
    m.refreshRunning();
    CHECK(!m.screenWaits());
}

TEST(model_a_first_run_while_the_game_runs_writes_the_key_and_leaves_roexts_ini) {
```
на:
```cpp
    w.gameRunning = false;
    m.refreshRunning();
    CHECK(!m.screenWaits());
}

TEST(model_other_programs_files_wait_for_the_game_to_close) {
    // Spec §4.1, 0.6.3: uaRO Pad's ini and dgVoodoo.conf - both copies - change only while the game is closed, the
    // rest of each file (comments, other keys) as it was.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.files[fake::path(L"dinput.dll")] = std::string("MZ") + '\0' + "uaro-pad.ini" + '\0';
    w.files[fake::path(L"uaro-pad.ini")] = "[mouse]\r\nfreedom = 1 ; the mouse is free\r\nmiddle = none\r\n";
    w.files[fake::path(L"dgVoodoo.conf")] = "[General]\r\nFullScreenMode = true\r\n";
    w.folders.insert(fake::path(L"dgvoodoo"));
    w.files[fake::path(L"dgvoodoo\\dgVoodoo.conf")] = "[General]\r\nFullScreenMode = true\r\n";
    w.gameRunning = true;
    app::Model m;
    CHECK(m.hasFile(SettingsFile::UaroPad));
    CHECK(!m.hasFile(SettingsFile::Roext));
    const FileSetting& middle = *findFileSetting(SettingsFile::UaroPad, "mouse.middle");
    const FileSetting& full = *findFileSetting(SettingsFile::DgVoodoo, "general.fullscreenmode");
    CHECK(m.fileShown(middle) && m.fileShown(full));  // every key of uaRO Pad's and dgVoodoo's
    m.setFileWord(middle, "F3");
    m.setFileWord(full, "false");
    CHECK_EQ(m.pendingCount(), size_t(2));
    CHECK(!m.apply(Lang::En).empty());  // it says what waits
    CHECK_EQ(valueOf(w.files[fake::path(L"uaro-pad.ini")], "mouse.middle"), std::string("none"));
    CHECK_EQ(m.pendingCount(), size_t(2));
    w.gameRunning = false;
    CHECK(m.apply(Lang::En).empty());
    const std::string pad = w.files[fake::path(L"uaro-pad.ini")];
    CHECK_EQ(valueOf(pad, "mouse.middle"), std::string("F3"));
    CHECK(pad.find("; the mouse is free") != std::string::npos);
    CHECK_EQ(valueOf(w.files[fake::path(L"dgVoodoo.conf")], "general.fullscreenmode"), std::string("false"));
    CHECK_EQ(valueOf(w.files[fake::path(L"dgvoodoo\\dgVoodoo.conf")], "general.fullscreenmode"), std::string("false"));
    CHECK(m.fileWord(full) == "false");
    CHECK_EQ(m.pendingCount(), size_t(0));
}

TEST(model_roext_s_settings_come_from_the_ini_the_game_loads) {
    // Spec §2.3: the ROExt the game loads, the keys its build names; its window size stays the Screen page's, a
    // renamed copy is off.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.files[fake::path(L"dinput.dll")] =
        std::string("MZ") + '\0' + "ROExt" + '\0' + ".\\dinput.ini" + '\0' + "MouseFreedom" + '\0';
    const std::wstring ini = fake::path(L"dinput.ini");
    w.files[ini] = "[ROExt]\r\nMouseFreedom=1\r\nWindowLock=1\r\nWindowWidth=1600\r\nWindowHeight=900\r\n";
    app::Model m;
    CHECK(m.hasFile(SettingsFile::Roext));
    const FileSetting& freedom = *findFileSetting(SettingsFile::Roext, "roext.mousefreedom");
    CHECK(m.fileWord(freedom) == "1");
    CHECK(m.fileShown(freedom));
    CHECK(!m.fileShown(*findFileSetting(SettingsFile::Roext, "roext.minimizekey")));  // a key this build does not name
    m.setFileWord(freedom, "0");
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "roext.mousefreedom"), std::string("0"));
    CHECK_EQ(valueOf(w.files[ini], "roext.windowwidth"), std::string("1600"));
    w.files[fake::path(L"dinput.dll.bak")] = w.files[fake::path(L"dinput.dll")];
    w.files.erase(fake::path(L"dinput.dll"));
    app::Model renamed;
    CHECK(!renamed.hasFile(SettingsFile::Roext));
    CHECK(!renamed.fileShown(freedom));
}

TEST(model_a_restore_drops_what_the_pages_changed_in_other_programs_files) {
    // The files come back from the copy: a word for the file as it was is no change any more.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    backup(L"2026-10-01-120000", "[quality]\r\nsharpen = 0\r\n");
    app::Model m;
    m.setFileWord(*findFileSetting(SettingsFile::DgVoodoo, "general.fullscreenmode"), "false");
    CHECK_EQ(m.pendingCount(), size_t(1));
    CHECK(m.restore(m.now().backups.front(), Lang::En).empty());
    CHECK_EQ(m.pendingCount(), size_t(0));
}

TEST(model_a_first_run_while_the_game_runs_writes_the_key_and_leaves_roexts_ini) {
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_model.cpp:258:13: error: ‘class uagfx::app::Model’ has no member named ‘hasFile’`.

- [ ] **Step 3: Реализация**

В `src/app/model.cpp` заменить:
```cpp
    paths_.report = paths_.dir + L"\\uaro-graphics-report.txt";
    paths_.ddraw = paths_.dir + L"\\ddraw.dll";
    paths_.dgvoodoo = paths_.dir + L"\\dgvoodoo\\DDraw.dll";
    system_ = probeSystem();
    load();
}
```
на:
```cpp
    paths_.report = paths_.dir + L"\\uaro-graphics-report.txt";
    paths_.ddraw = paths_.dir + L"\\ddraw.dll";
    paths_.dgvoodoo = paths_.dir + L"\\dgvoodoo\\DDraw.dll";
    paths_.dgvConf = paths_.dir + L"\\dgVoodoo.conf";
    paths_.dgvConfInside = paths_.dir + L"\\dgvoodoo\\dgVoodoo.conf";
    system_ = probeSystem();
    load();
}
```

В `src/app/model.cpp` заменить:
```cpp
            s.roext = roextWindow(readIni(m.iniText));
            s.roextModule = int(s.modules.size());
        }
        s.modules.push_back(m);
    }
    for (const CompatEntry& e : readCompat(paths_.game))
```
на:
```cpp
            s.roext = roextWindow(readIni(m.iniText));
            s.roextModule = int(s.modules.size());
        }
        if (m.loaded && m.info.kind == ModuleKind::UaroPad && s.padModule < 0) s.padModule = int(s.modules.size());
        s.modules.push_back(m);
    }
    for (const CompatEntry& e : readCompat(paths_.game))
```

В `src/app/model.cpp` заменить:
```cpp
        s.ddraw = bytes.find("uaRO Graphics") != std::string::npos ? Ddraw::Ours : Ddraw::Other;
    if (readFile(paths_.version, bytes)) s.installedVersion = firstLine(bytes);
    s.dgvoodoo = fileExists(paths_.dgvoodoo);
    s.gameRunning = gameRunning(paths_.game);
    s.backups = listFolder(paths_.backups, true);
    std::reverse(s.backups.begin(), s.backups.end());
```
на:
```cpp
        s.ddraw = bytes.find("uaRO Graphics") != std::string::npos ? Ddraw::Ours : Ddraw::Other;
    if (readFile(paths_.version, bytes)) s.installedVersion = firstLine(bytes);
    s.dgvoodoo = fileExists(paths_.dgvoodoo);
    if (!readFile(paths_.dgvConf, s.dgvoodooConf)) readFile(paths_.dgvConfInside, s.dgvoodooConf);
    s.gameRunning = gameRunning(paths_.game);
    s.backups = listFolder(paths_.backups, true);
    std::reverse(s.backups.begin(), s.backups.end());
```

В `src/app/model.cpp` заменить:
```cpp
    for (const auto& d : regSetupDiffers()) setRegWord(*d.first, d.second);
}

std::vector<std::wstring> Model::skins() const { return listFolder(paths_.dir + L"\\skin", true); }

bool Model::hasIntro() const { return fileExists(paths_.dir + L"\\openning.bik"); }
```
на:
```cpp
    for (const auto& d : regSetupDiffers()) setRegWord(*d.first, d.second);
}

std::map<std::string, std::string> Model::fileIni(SettingsFile f) const {
    switch (f) {
        case SettingsFile::Roext:
            return now_.roextModule >= 0 ? readIni(now_.modules[size_t(now_.roextModule)].iniText)
                                         : std::map<std::string, std::string>{};
        case SettingsFile::UaroPad:
            return now_.padModule >= 0 ? readIni(now_.modules[size_t(now_.padModule)].iniText)
                                       : std::map<std::string, std::string>{};
        case SettingsFile::DgVoodoo: break;
    }
    return readIni(now_.dgvoodooConf);
}

std::string Model::fileWord(const FileSetting& s) const {
    const auto it = files_.find(std::string(fileProgram(s.file)) + ":" + fileKey(s));
    return it != files_.end() ? it->second : fileWordOf(s, fileIni(s.file));
}

void Model::setFileWord(const FileSetting& s, const std::string& word) {
    if (!validFileWord(s, word)) return;
    const std::string key = std::string(fileProgram(s.file)) + ":" + fileKey(s);
    if (word == fileWordOf(s, fileIni(s.file)))
        files_.erase(key);
    else
        files_[key] = word;
}

bool Model::hasFile(SettingsFile f) const {
    switch (f) {
        case SettingsFile::Roext: return now_.roextModule >= 0;
        case SettingsFile::UaroPad: return now_.padModule >= 0;
        case SettingsFile::DgVoodoo: break;
    }
    return true;
}

bool Model::fileShown(const FileSetting& s) const {
    if (s.file != SettingsFile::Roext) return true;
    if (now_.roextModule < 0) return false;
    const std::vector<std::string>& keys = now_.modules[size_t(now_.roextModule)].info.keys;
    return std::find(keys.begin(), keys.end(), s.key) != keys.end();
}

std::vector<std::wstring> Model::skins() const { return listFolder(paths_.dir + L"\\skin", true); }

bool Model::hasIntro() const { return fileExists(paths_.dir + L"\\openning.bik"); }
```

В `src/app/model.cpp` заменить:
```cpp

size_t Model::pendingCount() const {
    const bool screen = screen_ && !(*screen_ == screenNow().choice);
    return ini_.size() + reg_.size() + (screen ? 1 : 0) + (screenAsSetup_ ? 1 : 0);
}

std::string Model::currentIni() const {
```
на:
```cpp

size_t Model::pendingCount() const {
    const bool screen = screen_ && !(*screen_ == screenNow().choice);
    return ini_.size() + reg_.size() + files_.size() + (screen ? 1 : 0) + (screenAsSetup_ ? 1 : 0);
}

std::string Model::currentIni() const {
```

В `src/app/model.cpp` заменить:
```cpp
    return failed;
}

std::wstring Model::apply(Lang l) {
    refreshRunning();
    std::vector<IniEdit> ini = pendingIniEdits();
    std::vector<RegWrite> reg;
    std::vector<IniEdit> roext;
    const bool firstRun = !gameKeyExists(now_.registry);
    // The Screen page shows the plan for the renderer chosen: a switch of the renderer alone writes it too.
    const bool screen = (screen_ && !(*screen_ == screenNow().choice)) || backend() != now_.config.backend;
```
на:
```cpp
    return failed;
}

std::wstring Model::writeFiles(const std::map<SettingsFile, std::vector<IniEdit>>& edits, Lang l) {
    for (const auto& f : edits) {
        std::vector<std::pair<std::wstring, std::string>> targets;  // the path, its name for a message
        if (f.first == SettingsFile::DgVoodoo) {
            // dgVoodoo loaded from its own folder reads one of the copies, never checked which: both alike.
            targets.push_back({paths_.dgvConf, "dgVoodoo.conf"});
            if (dirExists(paths_.dir + L"\\dgvoodoo"))
                targets.push_back({paths_.dgvConfInside, "dgvoodoo\\dgVoodoo.conf"});
        } else {
            const int i = f.first == SettingsFile::Roext ? now_.roextModule : now_.padModule;
            const ModuleFile& m = now_.modules[size_t(i)];
            targets.push_back({m.iniPath, m.info.ini});
        }
        for (const auto& t : targets) {
            std::string text;  // as it is now, like the ini: what was written there meanwhile stays
            readFile(t.first, text);
            if (!writeFileAtomic(t.first, applyIniEdits(text, f.second)))
                return wtrf(l, Text::ErrWriteFile, {t.second});
        }
    }
    return L"";
}

std::wstring Model::apply(Lang l) {
    refreshRunning();
    std::vector<IniEdit> ini = pendingIniEdits();
    std::vector<RegWrite> reg;
    std::vector<IniEdit> roext;
    std::map<SettingsFile, std::vector<IniEdit>> files;
    const bool firstRun = !gameKeyExists(now_.registry);
    // The Screen page shows the plan for the renderer chosen: a switch of the renderer alone writes it too.
    const bool screen = (screen_ && !(*screen_ == screenNow().choice)) || backend() != now_.config.backend;
```

В `src/app/model.cpp` заменить:
```cpp
        else if (const auto w = planWrite(now_.registry, s->subkey, s->name, regValueOf(*s, kv.second)))
            reg.push_back(*w);
    }
    if (!ini.empty() || !reg.empty() || !roext.empty()) {
        std::wstring err = backupOnce(l);
        if (!err.empty()) return err;
        if (!reg.empty()) {
```
на:
```cpp
        else if (const auto w = planWrite(now_.registry, s->subkey, s->name, regValueOf(*s, kv.second)))
            reg.push_back(*w);
    }
    std::map<std::string, std::string> heldFiles;  // other programs read their files as the game starts (spec §4.1)
    for (const auto& kv : files_) {
        const FileSetting* s = nullptr;
        for (const FileSetting& f : fileSettings())
            if (std::string(fileProgram(f.file)) + ":" + fileKey(f) == kv.first) s = &f;
        if (!s || !hasFile(s->file)) continue;  // its program is gone: nothing to write
        if (now_.gameRunning)
            heldFiles.insert(kv);
        else
            files[s->file].push_back({s->section, s->key, kv.second});
    }
    for (const IniEdit& e : roext) files[SettingsFile::Roext].push_back(e);  // the screen's size, in the same write
    if (!ini.empty() || !reg.empty() || !files.empty()) {
        std::wstring err = backupOnce(l);
        if (!err.empty()) return err;
        if (!reg.empty()) {
```

В `src/app/model.cpp` заменить:
```cpp
                return wtrf(l, Text::ErrWriteRegistry, {failed});
            }
        }
        if (!roext.empty()) {
            const ModuleFile& m = now_.modules[size_t(now_.roextModule)];
            std::string text;  // as it is now, like the ini: what the player wrote there meanwhile stays
            readFile(m.iniPath, text);
            if (!writeFileAtomic(m.iniPath, applyIniEdits(text, roext))) {
                load();
                return wtrf(l, Text::ErrWriteFile, {m.info.ini});
            }
        }
        if (!ini.empty()) {
            err = writeIni(ini, l);
```
на:
```cpp
                return wtrf(l, Text::ErrWriteRegistry, {failed});
            }
        }
        err = writeFiles(files, l);
        if (!err.empty()) {
            load();
            return err;
        }
        if (!ini.empty()) {
            err = writeIni(ini, l);
```

В `src/app/model.cpp` заменить:
```cpp
    ini_.clear();
    if (!kept.empty()) ini_["renderer.backend"] = kept;
    reg_ = held;
    if (waits)
        screen_ = chosen;  // the next Apply with the game closed writes the rest of it
    else
```
на:
```cpp
    ini_.clear();
    if (!kept.empty()) ini_["renderer.backend"] = kept;
    reg_ = held;
    files_ = heldFiles;
    if (waits)
        screen_ = chosen;  // the next Apply with the game closed writes the rest of it
    else
```

В `src/app/model.cpp` заменить:
```cpp
    screenAsSetup_ = false;
    wroteNow_.clear();
    load();
    return waits || !held.empty() ? wtr(l, Text::ApplyWaits) : L"";
}

std::wstring Model::writeNow(const IniSetting& s, const std::string& token, Lang l) {
```
на:
```cpp
    screenAsSetup_ = false;
    wroteNow_.clear();
    load();
    return waits || !held.empty() || !heldFiles.empty() ? wtr(l, Text::ApplyWaits) : L"";
}

std::wstring Model::writeNow(const IniSetting& s, const std::string& token, Lang l) {
```

В `src/app/model.cpp` заменить:
```cpp
std::wstring Model::revert(Lang l) {
    ini_.clear();
    reg_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    if (wroteNow_.empty()) return L"";
```
на:
```cpp
std::wstring Model::revert(Lang l) {
    ini_.clear();
    reg_.clear();
    files_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    if (wroteNow_.empty()) return L"";
```

В `src/app/model.cpp` заменить:
```cpp
    const std::string failed = replacePersonal(personal);
    ini_.clear();
    reg_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    wroteNow_.clear();
```
на:
```cpp
    const std::string failed = replacePersonal(personal);
    ini_.clear();
    reg_.clear();
    files_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    wroteNow_.clear();
```

В `src/app/model.hpp` заменить:
```cpp
#include "app/registry.hpp"
#include "app/system.hpp"
#include "core/config.hpp"
#include "core/settings/game_registry.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/input_module.hpp"
```
на:
```cpp
#include "app/registry.hpp"
#include "app/system.hpp"
#include "core/config.hpp"
#include "core/settings/file_settings.hpp"
#include "core/settings/game_registry.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/input_module.hpp"
```

В `src/app/model.hpp` заменить:
```cpp
struct Paths {
    std::wstring dir;  // the client folder
    std::wstring ini, log, game, version, backups, report, ddraw, dgvoodoo;
};

// A dinput DLL in the client folder (stage 2 spec §2.3).
```
на:
```cpp
struct Paths {
    std::wstring dir;  // the client folder
    std::wstring ini, log, game, version, backups, report, ddraw, dgvoodoo;
    std::wstring dgvConf, dgvConfInside;  // dgVoodoo.conf next to uaRO.exe and in dgvoodoo\ (spec §2.4)
};

// A dinput DLL in the client folder (stage 2 spec §2.3).
```

В `src/app/model.hpp` заменить:
```cpp
    std::vector<ModuleFile> modules;
    settings::RoextWindow roext;  // of the ROExt the game loads; inactive when there is none
    int roextModule = -1;         // its index in modules
    std::vector<CompatFlag> compat;
    Ddraw ddraw = Ddraw::None;
    std::string installedVersion;  // uaro-graphics\version.txt
```
на:
```cpp
    std::vector<ModuleFile> modules;
    settings::RoextWindow roext;  // of the ROExt the game loads; inactive when there is none
    int roextModule = -1;         // its index in modules
    int padModule = -1;           // the uaRO Pad the game loads, -1 when none
    std::string dgvoodooConf;     // dgVoodoo.conf, else the copy in the dgvoodoo folder
    std::vector<CompatFlag> compat;
    Ddraw ddraw = Ddraw::None;
    std::string installedVersion;  // uaro-graphics\version.txt
```

В `src/app/model.hpp` заменить:
```cpp
    // personal values.
    std::vector<std::pair<const settings::RegSetting*, std::string>> regSetupDiffers() const;
    void regAsSetup();
    std::vector<std::wstring> skins() const;  // the folders of skin\ next to uaRO.exe (spec §6.1)
    bool hasIntro() const;                     // openning.bik next to uaRO.exe

```
на:
```cpp
    // personal values.
    std::vector<std::pair<const settings::RegSetting*, std::string>> regSetupDiffers() const;
    void regAsSetup();
    // Other programs' settings files on the pages (0.6.3, file_settings): the ROExt or uaRO Pad the game loads, and
    // dgVoodoo.conf. The player's word, else what the program reads now; while the game runs they wait (spec §4.1).
    std::string fileWord(const settings::FileSetting& s) const;
    void setFileWord(const settings::FileSetting& s, const std::string& word);
    bool hasFile(settings::SettingsFile f) const;          // ROExt's, uaRO Pad's: the game loads it; dgVoodoo's always
    bool fileShown(const settings::FileSetting& s) const;  // ROExt's: the build the game loads names it; others' all
    std::vector<std::wstring> skins() const;  // the folders of skin\ next to uaRO.exe (spec §6.1)
    bool hasIntro() const;                     // openning.bik next to uaRO.exe

```

В `src/app/model.hpp` заменить:
```cpp
    // Each value on its own (spec §4.5): "" or the ones that failed, "NAME (error N), ..."; the others are written.
    std::string writeValues(const std::vector<settings::RegWrite>& writes);
    std::vector<settings::IniEdit> pendingIniEdits() const;

    Paths paths_;
    SystemInfo system_;
    Snapshot now_;
    std::map<std::string, std::string> ini_;  // "section.key" -> the player's word
    std::map<std::string, std::string> reg_;  // regKey -> the player's word
    std::optional<settings::ScreenChoice> screen_;
    bool screenAsSetup_ = false;
    std::map<std::string, std::string> wroteNow_;  // "section.key" -> the word before the first writeNow
```
на:
```cpp
    // Each value on its own (spec §4.5): "" or the ones that failed, "NAME (error N), ..."; the others are written.
    std::string writeValues(const std::vector<settings::RegWrite>& writes);
    std::vector<settings::IniEdit> pendingIniEdits() const;
    std::map<std::string, std::string> fileIni(settings::SettingsFile f) const;  // readIni of it, as it was read
    // Each file as it is now with its edits; dgVoodoo.conf in both places (spec §2.4). "" or what went wrong.
    std::wstring writeFiles(const std::map<settings::SettingsFile, std::vector<settings::IniEdit>>& edits,
                            settings::Lang l);

    Paths paths_;
    SystemInfo system_;
    Snapshot now_;
    std::map<std::string, std::string> ini_;  // "section.key" -> the player's word
    std::map<std::string, std::string> reg_;  // regKey -> the player's word
    std::map<std::string, std::string> files_;  // "<program>:<section.key>" -> the player's word
    std::optional<settings::ScreenChoice> screen_;
    bool screenAsSetup_ = false;
    std::map<std::string, std::string> wroteNow_;  // "section.key" -> the word before the first writeNow
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `311 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `311 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_model.cpp src/app/model.cpp src/app/model.hpp
git commit -q -F - <<'EOF'
Утилита: чужие файлы настроек в модели — ждут «Применить» и закрытия игры

ini ROExt и uaRO Pad, dgVoodoo.conf в обе копии: только при закрытой игре
(§4.1), меняются только свои значения. «Восстановить» снимает и эти изменения.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 6: Проверка обновления: номер сборки и запуск скриптов (§8)

`src/core/settings/update_check.*` разбирает строку `version.txt` («0.6.3 build 2026-10-14 02:17») и сравнивает сборки:
новее — выше версия или та же, собранная позже. Разбор — `strtol`, не `sscanf`: тот потянул бы в утилиту функции,
которых нет в списке `check_exe`. Модель хранит состояние проверки (не было, идёт, последняя, новее, ошибка) и
запускает `update.bat` или `uninstall.bat` (подставной Windows запоминает, что запущено).

**Files:**
- Create: `src/core/settings/update_check.cpp`, `src/core/settings/update_check.hpp`, `tests/test_update_check.cpp`
- Modify: `src/app/model.cpp`, `src/app/model.hpp`, `tests/fake_windows.cpp`, `tests/fake_windows.hpp`, `tests/test_model.cpp`

**Interfaces:**
- Consumes: `launch` (`files.hpp`).
- Produces: `struct Build`, `parseBuild(text)`, `buildText(b)`, `newerBuild(candidate, installed)`;
  `enum class UpdateState { Never, Checking, Latest, Newer, Failed }`, `struct UpdateCheck`, `Model::updateCheck()`,
  `updateStarted()`, `updateFinished(error, body)`, `runScript(name, Lang)`.

- [ ] **Step 1: Падающие тесты**

В `tests/fake_windows.cpp` заменить:
```cpp

std::string utf8ToRegFile(const std::string& utf8) { return utf8; }

unsigned long launch(const std::wstring&, const std::wstring&) { return 0; }

void openInShell(const std::wstring&) {}

```
на:
```cpp

std::string utf8ToRegFile(const std::string& utf8) { return utf8; }

unsigned long launch(const std::wstring& file, const std::wstring&) {
    windows().launched.push_back(file);
    return 0;
}

void openInShell(const std::wstring&) {}

```

В `tests/fake_windows.hpp` заменить:
```cpp
    bool gameRunning = false;
    uagfx::app::SystemInfo system;
    int clock = 0;  // seconds after 2026-10-13 12:00:00: backup folders are named after it
};

// A fresh one: the client folder C:\uaRO, the main monitor 3840 x 2160 at 175 %, no game key, no files.
```
на:
```cpp
    bool gameRunning = false;
    uagfx::app::SystemInfo system;
    int clock = 0;  // seconds after 2026-10-13 12:00:00: backup folders are named after it
    std::vector<std::wstring> launched;  // what launch() started, full paths
};

// A fresh one: the client folder C:\uaRO, the main monitor 3840 x 2160 at 175 %, no game key, no files.
```

В `tests/test_model.cpp` заменить:
```cpp
    CHECK(w.registry.personal.exists || w.registry.common.exists);
}

TEST(model_skins_are_the_folders_of_skin_and_the_intro_a_file) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"skin"));
```
на:
```cpp
    CHECK(w.registry.personal.exists || w.registry.common.exists);
}

TEST(model_an_update_check_compares_github_s_build_with_the_installed_one) {
    // Spec §8, 0.6.3: release/version.txt against uaro-graphics\\version.txt; an answer that is no build is a failure.
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics\\version.txt")] = "0.6.2 build 2026-10-14 02:17\r\n";
    app::Model m;
    CHECK(m.updateCheck().state == app::UpdateState::Never);
    m.updateStarted();
    CHECK(m.updateCheck().state == app::UpdateState::Checking);
    m.updateFinished(0, "0.6.3 build 2026-10-20 10:00\r\n");
    CHECK(m.updateCheck().state == app::UpdateState::Newer);
    CHECK(m.updateCheck().remote == "0.6.3 build 2026-10-20 10:00");
    m.updateFinished(0, "0.6.2 build 2026-10-14 02:17\r\n");
    CHECK(m.updateCheck().state == app::UpdateState::Latest);
    m.updateFinished(12029, "");  // no connection
    CHECK(m.updateCheck().state == app::UpdateState::Failed);
    CHECK_EQ(m.updateCheck().error, 12029ul);
    m.updateFinished(0, "<html>404</html>");
    CHECK(m.updateCheck().state == app::UpdateState::Failed);
}

TEST(model_update_and_remove_start_their_scripts) {
    // Spec §8: «Update» starts update.bat, «Remove» uninstall.bat, both next to uaRO.exe.
    fake::Windows& w = fake::reset();
    app::Model m;
    CHECK(m.runScript(L"update.bat", Lang::En).empty());
    CHECK(!w.launched.empty() && w.launched.back() == fake::path(L"update.bat"));
}

TEST(model_skins_are_the_folders_of_skin_and_the_intro_a_file) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"skin"));
```

`tests/test_update_check.cpp`:
```cpp
// The update check of the settings app (stage 2 spec §8, release 0.6.3): release/version.txt against the installed one.
#include "core/settings/update_check.hpp"
#include "test.hpp"

using namespace uagfx::settings;

TEST(update_check_reads_version_txt) {
    const auto b = parseBuild("0.6.3 build 2026-10-14 02:17\r\n");
    CHECK(b && b->major == 0 && b->minor == 6 && b->patch == 3 && b->when == "2026-10-14 02:17");
    CHECK(b && buildText(*b) == "0.6.3 build 2026-10-14 02:17");
    const auto bom = parseBuild("\xEF\xBB\xBF" "0.7.0 build 2026-11-01 10:00");
    CHECK(bom && bom->minor == 7);
    CHECK(!parseBuild("<html>Not Found</html>"));
    CHECK(!parseBuild(""));
}

TEST(update_check_a_newer_build_is_a_higher_version_or_a_later_one) {
    const Build installed = *parseBuild("0.6.2 build 2026-10-14 02:17");
    CHECK(newerBuild(*parseBuild("0.6.3 build 2026-10-01 00:00"), installed));
    CHECK(newerBuild(*parseBuild("0.6.2 build 2026-10-14 05:00"), installed));  // the same version built again
    CHECK(!newerBuild(installed, installed));
    CHECK(!newerBuild(*parseBuild("0.6.1 build 2026-12-01 00:00"), installed));
    CHECK(newerBuild(*parseBuild("0.10.0 build 2026-10-01 00:00"), *parseBuild("0.9.9 build 2026-10-01 00:00")));
}
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает; первой строкой — ошибка того из тестов, что собрался первым: `…/tests/test_model.cpp:467:13: error: ‘class uagfx::app::Model’ has no member named ‘updateCheck’`, `…/tests/test_update_check.cpp:2:10: fatal error: core/settings/update_check.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

В `src/app/model.cpp` заменить:
```cpp
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::launchGame(Lang l) {
    const unsigned long e = launch(paths_.game, paths_.dir);
    refreshRunning();
```
на:
```cpp
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::runScript(const wchar_t* name, Lang l) {
    const unsigned long e = launch(paths_.dir + L"\\" + name, paths_.dir);
    return e ? wtrf(l, Text::ErrLaunchFile, {utf8(name), num(long(e))}) : L"";
}

void Model::updateFinished(unsigned long error, const std::string& body) {
    const auto remote = error ? std::nullopt : parseBuild(body);
    if (!remote) {
        update_ = UpdateCheck{UpdateState::Failed, "", error};
        return;
    }
    const auto installed = parseBuild(now_.installedVersion);  // none known: any build is the one to have
    const bool newer = !installed || newerBuild(*remote, *installed);
    update_ = UpdateCheck{newer ? UpdateState::Newer : UpdateState::Latest, buildText(*remote), 0};
}

std::wstring Model::launchGame(Lang l) {
    const unsigned long e = launch(paths_.game, paths_.dir);
    refreshRunning();
```

В `src/app/model.hpp` заменить:
```cpp
#include "core/settings/schema.hpp"
#include "core/settings/screen.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::app {

```
на:
```cpp
#include "core/settings/schema.hpp"
#include "core/settings/screen.hpp"
#include "core/settings/strings.hpp"
#include "core/settings/update_check.hpp"

namespace uagfx::app {

```

В `src/app/model.hpp` заменить:
```cpp
};

enum class Ddraw { Ours, Other, None };

// Everything the pages show, read from the client folder and the registry.
struct Snapshot {
```
на:
```cpp
};

enum class Ddraw { Ours, Other, None };

// The update check of the Service page (spec §8): not asked yet, asking GitHub, or its answer.
enum class UpdateState { Never, Checking, Latest, Newer, Failed };
struct UpdateCheck {
    UpdateState state = UpdateState::Never;
    std::string remote;       // GitHub's build, "0.6.3 build 2026-10-20 10:00"
    unsigned long error = 0;  // Failed: the system's error or the HTTP status; 0 when the answer was no build
};

// Everything the pages show, read from the client folder and the registry.
struct Snapshot {
```

В `src/app/model.hpp` заменить:
```cpp
    std::wstring loadGameSettings(const std::wstring& file, settings::Lang l);
    std::wstring resetWindowLayout(settings::Lang l);  // spec §8; only while the game is closed
    std::wstring launchGame(settings::Lang l);
    std::optional<settings::Lang> savedLanguage() const;
    void saveLanguage(settings::Lang l);  // the app's own key: no backup for it

```
на:
```cpp
    std::wstring loadGameSettings(const std::wstring& file, settings::Lang l);
    std::wstring resetWindowLayout(settings::Lang l);  // spec §8; only while the game is closed
    std::wstring launchGame(settings::Lang l);
    // «Update» and «Remove» (spec §8): update.bat or uninstall.bat next to uaRO.exe; the app closes after it.
    std::wstring runScript(const wchar_t* name, settings::Lang l);
    const UpdateCheck& updateCheck() const { return update_; }
    void updateStarted() { update_ = UpdateCheck{UpdateState::Checking, "", 0}; }
    void updateFinished(unsigned long error, const std::string& body);  // GitHub's answer, or why there is none
    std::optional<settings::Lang> savedLanguage() const;
    void saveLanguage(settings::Lang l);  // the app's own key: no backup for it

```

В `src/app/model.hpp` заменить:
```cpp
    bool screenAsSetup_ = false;
    std::map<std::string, std::string> wroteNow_;  // "section.key" -> the word before the first writeNow
    bool backedUp_ = false;
};

// What the Screen page says about a plan it cannot write.
```
на:
```cpp
    bool screenAsSetup_ = false;
    std::map<std::string, std::string> wroteNow_;  // "section.key" -> the word before the first writeNow
    bool backedUp_ = false;
    UpdateCheck update_;
};

// What the Screen page says about a plan it cannot write.
```

`src/core/settings/update_check.cpp`:
```cpp
#include "core/settings/update_check.hpp"

#include <cstdlib>
#include <tuple>

namespace uagfx::settings {

std::optional<Build> parseBuild(const std::string& text) {
    std::string line = text.rfind("\xEF\xBB\xBF", 0) == 0 ? text.substr(3) : text;
    line = line.substr(0, line.find_first_of("\r\n"));
    Build b;
    int* parts[] = {&b.major, &b.minor, &b.patch};
    const char* p = line.c_str();
    for (int i = 0; i < 3; ++i) {  // strtol, not sscanf: the settings app imports only what Windows 7 has (check_exe)
        if ((i && *p++ != '.') || *p < '0' || *p > '9') return std::nullopt;
        char* end = nullptr;
        *parts[i] = int(std::strtol(p, &end, 10));
        p = end;
    }
    const std::string rest = p;
    if (!rest.empty() && rest.rfind(" build ", 0) != 0) return std::nullopt;
    if (!rest.empty()) b.when = rest.substr(7);
    return b;
}

std::string buildText(const Build& b) {
    std::string out = std::to_string(b.major) + "." + std::to_string(b.minor) + "." + std::to_string(b.patch);
    return b.when.empty() ? out : out + " build " + b.when;
}

bool newerBuild(const Build& candidate, const Build& installed) {
    return std::tie(candidate.major, candidate.minor, candidate.patch, candidate.when) >
           std::tie(installed.major, installed.minor, installed.patch, installed.when);
}

}  // namespace uagfx::settings
```

`src/core/settings/update_check.hpp`:
```cpp
#pragma once
#include <optional>
#include <string>

namespace uagfx::settings {

// A build as release/version.txt on GitHub and uaro-graphics\version.txt say it: "0.6.3 build 2026-10-14 02:17"
// (stage 2 spec §8).
struct Build {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string when;  // "2026-10-14 02:17"
};
std::optional<Build> parseBuild(const std::string& text);  // its first line; nothing when it is not a build
std::string buildText(const Build& b);
bool newerBuild(const Build& candidate, const Build& installed);  // a higher version, or the same one built later

}  // namespace uagfx::settings
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `315 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `315 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/fake_windows.cpp tests/fake_windows.hpp tests/test_model.cpp tests/test_update_check.cpp src/app/model.cpp src/app/model.hpp src/core/settings/update_check.cpp src/core/settings/update_check.hpp
git commit -q -F - <<'EOF'
Утилита: проверка обновления — номер сборки, ответ GitHub, запуск скриптов

Строка version.txt и сравнение сборок; состояние проверки в модели; запуск
update.bat и uninstall.bat (§8).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 7: Разделы «Ввод и окно» и «dgVoodoo» (Win32, §6.1)

Раздел строится из таблицы Task 4: флажки — по два в ряд, списки (значение файла вне списка — отдельной строкой «как в
файле»), ползунки, поля чисел (у `WindowPosX`/`WindowPosY` — со знаком минус). «Ввод и окно» показывает ROExt или uaRO
Pad, которого загружает игра (у ROExt — только ключи его сборки); если обоих — переключатель вверху, на оба места нет. Без них — пояснение. «dgVoodoo» при
рендере uaRO Graphics предупреждает, что dgVoodoo не работает. Пока игра запущена, всё серое, внизу записка.

**Files:**
- Create: `src/app/page_files.cpp`
- Modify: `CMakeLists.txt`, `src/app/main_window.cpp`, `src/app/page.hpp`, `src/app/ui.cpp`, `src/app/ui.hpp`

**Interfaces:**
- Consumes: Task 5 (`fileWord`, `setFileWord`, `hasFile`, `fileShown`); Task 4.
- Produces: `std::unique_ptr<Page> makeFilesPage(PageHost&, settings::Page)`; `Panel::number(..., bool enabled, bool
  negative)`.

- [ ] **Step 1: Страница и подключение**

В `CMakeLists.txt` заменить:
```cmake
    configure_file(src/app/version_rc.h.in ${CMAKE_BINARY_DIR}/app/version_rc.h)
    add_executable(uaro-graphics WIN32 src/app/main.cpp src/app/main_window.cpp src/app/ui.cpp src/app/page.cpp
                   src/app/page_settings.cpp src/app/page_screen.cpp src/app/page_diagnostics.cpp
                   src/app/page_service.cpp src/app/app.rc)
    target_include_directories(uaro-graphics PRIVATE src/app dist ${CMAKE_BINARY_DIR}/app)
    target_compile_options(uaro-graphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-graphics PRIVATE uagfx_app comdlg32)
```
на:
```cmake
    configure_file(src/app/version_rc.h.in ${CMAKE_BINARY_DIR}/app/version_rc.h)
    add_executable(uaro-graphics WIN32 src/app/main.cpp src/app/main_window.cpp src/app/ui.cpp src/app/page.cpp
                   src/app/page_settings.cpp src/app/page_screen.cpp src/app/page_diagnostics.cpp
                   src/app/page_service.cpp src/app/page_files.cpp src/app/app.rc)
    target_include_directories(uaro-graphics PRIVATE src/app dist ${CMAKE_BINARY_DIR}/app)
    target_compile_options(uaro-graphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-graphics PRIVATE uagfx_app comdlg32)
```

В `src/app/main_window.cpp` заменить:
```cpp
        case settings::Page::Screen: current_ = makeScreenPage(*this); break;
        case settings::Page::Diagnostics: current_ = makeDiagnosticsPage(*this); break;
        case settings::Page::Service: current_ = makeServicePage(*this); break;
        default: current_ = makeSettingsPage(*this, settings::Page(page_)); break;
    }
    current_->build(*panel_);
```
на:
```cpp
        case settings::Page::Screen: current_ = makeScreenPage(*this); break;
        case settings::Page::Diagnostics: current_ = makeDiagnosticsPage(*this); break;
        case settings::Page::Service: current_ = makeServicePage(*this); break;
        case settings::Page::Input:
        case settings::Page::DgVoodoo: current_ = makeFilesPage(*this, settings::Page(page_)); break;
        default: current_ = makeSettingsPage(*this, settings::Page(page_)); break;
    }
    current_->build(*panel_);
```

В `src/app/page.hpp` заменить:
```cpp
std::unique_ptr<Page> makeScreenPage(PageHost& host);
std::unique_ptr<Page> makeDiagnosticsPage(PageHost& host);
std::unique_ptr<Page> makeServicePage(PageHost& host);

// The window text of a control, as UTF-8 (number fields).
std::string controlText(HWND ctl);
```
на:
```cpp
std::unique_ptr<Page> makeScreenPage(PageHost& host);
std::unique_ptr<Page> makeDiagnosticsPage(PageHost& host);
std::unique_ptr<Page> makeServicePage(PageHost& host);
std::unique_ptr<Page> makeFilesPage(PageHost& host, settings::Page which);  // Input, DgVoodoo

// The window text of a control, as UTF-8 (number fields).
std::string controlText(HWND ctl);
```

`src/app/page_files.cpp`:
```cpp
#include <algorithm>
#include <cstdlib>
#include <map>

#include "app/page.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kShowRoext = 1690;
constexpr int kShowPad = 1691;
constexpr int kFile = 1700;  // + 4 x the setting's index in fileSettings()

bool g_showPad = false;  // ROExt and uaRO Pad both loaded: the page shows uaRO Pad's (kept while the app runs)

// Input and window, dgVoodoo (stage 2 spec §2.3, §2.4, §6.1; release 0.6.3): other programs' settings files, the keys
// of file_settings one to one.
class FilesPage : public Page {
public:
    FilesPage(PageHost& host, settings::Page which) : Page(host), which_(which) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        p.heading(t(pageName(which_)));
        SettingsFile file = SettingsFile::DgVoodoo;
        if (which_ == settings::Page::DgVoodoo) {
            if (m.backend() == Backend::Uaro) p.note(t(Text::DgvUaroNote), Tone::Warn);
        } else {
            const bool roext = m.hasFile(SettingsFile::Roext);
            const bool pad = m.hasFile(SettingsFile::UaroPad);
            if (!roext && !pad) {
                p.note(t(Text::InputNone));
                return;
            }
            if (roext && pad) {  // the page has no room for both: one at a time
                p.radio(kShowRoext, t(Text::InputModule), L"ROExt", !g_showPad, true, L"");
                p.radio(kShowPad, L"", L"uaRO Pad", g_showPad, false, L"");
                p.gap(6);
            }
            file = pad && (!roext || g_showPad) ? SettingsFile::UaroPad : SettingsFile::Roext;
            const Snapshot& now = m.now();
            const ModuleFile& dll = now.modules[size_t(file == SettingsFile::Roext ? now.roextModule : now.padModule)];
            p.heading(tf(file == SettingsFile::Roext ? Text::InputRoext : Text::InputPad, {utf8(dll.file)}));
        }
        rows(p, file);
        p.note(t(Text::AfterRestart));
        if (m.now().gameRunning) p.note(tf(Text::FilesClosedOnly, {fileProgram(file)}), Tone::Warn);
    }

    void command(int id, int code) override {
        Model& m = host_.model();
        if ((id == kShowRoext || id == kShowPad) && code == BN_CLICKED) {
            g_showPad = id == kShowPad;
            host_.rebuild();
            return;
        }
        const FileSetting* s = settingOf(id);
        if (!s) return;
        HWND ctl = GetDlgItem(host_.window(), id);
        if (s->control == Control::Check && code == BN_CLICKED) {
            m.setFileWord(*s, s->tokens[SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0]);
        } else if (s->control == Control::Choice && (code == CBN_SELCHANGE || code == CBN_CLOSEUP)) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            const auto it = words_.find(id);
            if (it == words_.end() || sel < 0 || size_t(sel) >= it->second.size()) return;
            m.setFileWord(*s, it->second[size_t(sel)]);  // the file's own odd value is no word to write: nothing
        } else if (s->control == Control::Number && code == EN_KILLFOCUS) {
            const std::string text = controlText(ctl);
            if (validFileWord(*s, text))
                m.setFileWord(*s, text);
            else
                host_.rebuild();  // back to the value it had
        } else {
            return;
        }
        host_.changed();
    }

    void scroll(int id, int, int pos) override {
        const FileSetting* s = settingOf(id);
        if (!s || s->control != Control::Slider) return;
        const int step = std::max(s->step, 1);
        const int v = std::clamp(s->min + (pos - s->min + step / 2) / step * step, s->min, s->max);
        if (v != pos) SendMessageW(GetDlgItem(host_.window(), id), TBM_SETPOS, TRUE, v);
        const std::string word = std::to_string(v);
        SetDlgItemTextW(host_.window(), id + 1, wide(word).c_str());
        host_.model().setFileWord(*s, word);
        host_.changed();
    }

private:
    static const FileSetting* settingOf(int id) {
        if (id < kFile) return nullptr;
        const size_t i = size_t(id - kFile) / 4;
        return (id - kFile) % 4 == 0 && i < fileSettings().size() ? &fileSettings()[i] : nullptr;
    }

    // The keys of one file, in the order of fileSettings(), ROExt's those its build names: checks two to a row. The
    // programs read their files as the game starts, so while it runs everything is grey (spec §4.1).
    void rows(Panel& p, SettingsFile file) {
        Model& m = host_.model();
        const bool open = !m.now().gameRunning;
        const std::vector<FileSetting>& all = fileSettings();
        std::vector<CheckItem> checks;
        auto flush = [&] {
            if (!checks.empty()) p.checks(checks);
            checks.clear();
        };
        for (size_t i = 0; i < all.size(); ++i) {
            const FileSetting& s = all[i];
            if (s.file != file || !m.fileShown(s)) continue;
            const int id = kFile + int(i) * 4;
            const std::string word = m.fileWord(s);
            const std::wstring hint = s.hint == s.label ? L"" : t(s.hint);
            if (s.control == Control::Check) {
                checks.push_back({id, t(s.label), word == s.tokens[1], hint, open});
                continue;
            }
            flush();
            if (s.control == Control::Slider) {
                p.slider(id, t(s.label), s.min, s.max, s.step, std::atoi(word.c_str()), wide(word), hint, open);
            } else if (s.control == Control::Number) {
                p.number(id, t(s.label), wide(word), hint, open, s.min < 0);
            } else {
                std::vector<std::wstring> items;
                std::vector<std::string>& words = words_[id];
                words.clear();
                int selected = -1;
                for (size_t c = 0; c < s.tokens.size(); ++c) {
                    items.push_back(t(s.choices[c]));
                    words.push_back(s.tokens[c]);
                    if (word == s.tokens[c]) selected = int(c);
                }
                if (selected < 0) {  // a value the list has not: shown, kept until another one is picked
                    items.push_back(tf(Text::FileValueRaw, {word}));
                    words.push_back(word);
                    selected = int(items.size()) - 1;
                }
                p.choice(id, t(s.label), items, selected, hint, open);
            }
        }
        flush();
    }

    settings::Page which_;
    std::map<int, std::vector<std::string>> words_;  // a list's words in its order, by its id
};

}  // namespace

std::unique_ptr<Page> makeFilesPage(PageHost& host, settings::Page which) {
    return std::make_unique<FilesPage>(host, which);
}

}  // namespace uagfx::app
```

В `src/app/ui.cpp` заменить:
```cpp
    return c;
}

HWND Panel::number(int id, const std::wstring& text, const std::wstring& value, const std::wstring& hint, bool enabled) {
    label(text, at(kRow), enabled);
    HWND c = make(L"EDIT", value, ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_, at(90), at(kRow),
                  id, WS_EX_CLIENTEDGE);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow) + at(kGap);
```
на:
```cpp
    return c;
}

HWND Panel::number(int id, const std::wstring& text, const std::wstring& value, const std::wstring& hint, bool enabled,
                   bool negative) {
    label(text, at(kRow), enabled);
    HWND c = make(L"EDIT", value, (negative ? 0 : ES_NUMBER) | ES_AUTOHSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_,
                  at(90), at(kRow), id, WS_EX_CLIENTEDGE);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow) + at(kGap);
```

В `src/app/ui.hpp` заменить:
```cpp
    // A trackbar with its number on the right; the number is control id + 1.
    HWND slider(int id, const std::wstring& label, int lo, int hi, int step, int value, const std::wstring& valueText,
                const std::wstring& hint, bool enabled = true);
    HWND number(int id, const std::wstring& label, const std::wstring& value, const std::wstring& hint,
                bool enabled = true);
    // Two numbers with "×" between them: ids id and id + 1.
    HWND pair(int id, const std::wstring& label, int a, int b, const std::wstring& hint, bool enabled = true);
    void checks(const std::vector<CheckItem>& items);  // two to a row
```
на:
```cpp
    // A trackbar with its number on the right; the number is control id + 1.
    HWND slider(int id, const std::wstring& label, int lo, int hi, int step, int value, const std::wstring& valueText,
                const std::wstring& hint, bool enabled = true);
    // A number field; `negative` takes a minus too (a window's position on a monitor left of the main one).
    HWND number(int id, const std::wstring& label, const std::wstring& value, const std::wstring& hint,
                bool enabled = true, bool negative = false);
    // Two numbers with "×" between them: ids id and id + 1.
    HWND pair(int id, const std::wstring& label, int a, int b, const std::wstring& hint, bool enabled = true);
    void checks(const std::vector<CheckItem>& items);  // two to a row
```

- [ ] **Step 2: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `315 tests, 0 failed checks`.

- [ ] **Step 3: Коммит**

```bash
git add CMakeLists.txt src/app/main_window.cpp src/app/page.hpp src/app/page_files.cpp src/app/ui.cpp src/app/ui.hpp
git commit -q -F - <<'EOF'
Утилита: разделы «Ввод и окно» и «dgVoodoo»

Ключи ROExt (названные в его DLL) или uaRO Pad, которого загружает игра, и
dgVoodoo.conf — из таблицы, по два флажка в ряд; пока игра запущена, всё
серое (§4.1).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 8: «Сервис»: проверить обновление, обновить, удалить (Win32, §8)

Кнопки группы «uaRO Graphics»: «Сбросить к умолчаниям», «Проверить обновление» (серая, пока идёт проверка) или
«Обновить до …», когда на GitHub сборка новее, и «Удалить uaRO Graphics…»; строка версий дополняется ответом GitHub.
Запрос — WinHTTP к `raw.githubusercontent.com` в отдельном потоке, ответ приходит окну сообщением. «Обновить» и
«Удалить» спрашивают (и о несохранённом), запускают скрипт и закрывают утилиту без второго вопроса; `update.bat` уже
ждёт её закрытия до 10 с, `uninstall.bat` теперь тоже. В списке `check_exe` — WinHTTP и `CreateThread`.

**Files:**
- Create: `src/app/web.cpp`, `src/app/web.hpp`
- Modify: `CMakeLists.txt`, `dist/uninstall.bat`, `src/app/main_window.cpp`, `src/app/main_window.hpp`, `src/app/page.hpp`, `src/app/page_service.cpp`, `tools/check_exe.py`

**Interfaces:**
- Consumes: Task 6 (`updateCheck`, `updateStarted`, `updateFinished`, `runScript`, `parseBuild`).
- Produces: `bool checkUpdateAsync(HWND, UINT)` (`web.hpp`); `PageHost::checkUpdate()`, `PageHost::closeForScript()`.

- [ ] **Step 1: Сервис, WinHTTP, скрипты**

В `CMakeLists.txt` заменить:
```cmake
    configure_file(src/app/version_rc.h.in ${CMAKE_BINARY_DIR}/app/version_rc.h)
    add_executable(uaro-graphics WIN32 src/app/main.cpp src/app/main_window.cpp src/app/ui.cpp src/app/page.cpp
                   src/app/page_settings.cpp src/app/page_screen.cpp src/app/page_diagnostics.cpp
                   src/app/page_service.cpp src/app/page_files.cpp src/app/app.rc)
    target_include_directories(uaro-graphics PRIVATE src/app dist ${CMAKE_BINARY_DIR}/app)
    target_compile_options(uaro-graphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-graphics PRIVATE uagfx_app comdlg32)
    target_link_options(uaro-graphics PRIVATE -static -static-libgcc -static-libstdc++ -s -municode)
else()
    enable_testing()
```
на:
```cmake
    configure_file(src/app/version_rc.h.in ${CMAKE_BINARY_DIR}/app/version_rc.h)
    add_executable(uaro-graphics WIN32 src/app/main.cpp src/app/main_window.cpp src/app/ui.cpp src/app/page.cpp
                   src/app/page_settings.cpp src/app/page_screen.cpp src/app/page_diagnostics.cpp
                   src/app/page_service.cpp src/app/page_files.cpp src/app/web.cpp src/app/app.rc)
    target_include_directories(uaro-graphics PRIVATE src/app dist ${CMAKE_BINARY_DIR}/app)
    target_compile_options(uaro-graphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-graphics PRIVATE uagfx_app comdlg32 winhttp)
    target_link_options(uaro-graphics PRIVATE -static -static-libgcc -static-libstdc++ -s -municode)
else()
    enable_testing()
```

В `dist/uninstall.bat` заменить:
```bat
rem uaRO Graphics: removes our ddraw.dll and puts dgVoodoo back where it was.
setlocal
cd /d "%~dp0"
tasklist /fi "imagename eq uaro-graphics.exe" 2>nul | find /i "uaro-graphics.exe" >nul && (
    echo uaRO Graphics Settings is open: close it and run uninstall.bat again.
    echo.
    pause
    exit /b 1
)
(echo test) > "uaro-graphics-write-test.tmp" 2>nul || goto :noaccess
del "uaro-graphics-write-test.tmp"
if exist "ddraw.dll" (
```
на:
```bat
rem uaRO Graphics: removes our ddraw.dll and puts dgVoodoo back where it was.
setlocal
cd /d "%~dp0"
rem The settings app may have started this: give it up to 10 seconds to close, its exe goes too.
set /a waited=0
:waitapp
tasklist /fi "imagename eq uaro-graphics.exe" 2>nul | find /i "uaro-graphics.exe" >nul || goto :appclosed
if %waited% geq 10 (
    echo uaRO Graphics Settings is open: close it and run uninstall.bat again.
    echo.
    pause
    exit /b 1
)
timeout /t 1 /nobreak >nul
set /a waited+=1
goto :waitapp
:appclosed
(echo test) > "uaro-graphics-write-test.tmp" 2>nul || goto :noaccess
del "uaro-graphics-write-test.tmp"
if exist "ddraw.dll" (
```

В `src/app/main_window.cpp` заменить:
```cpp
#include <commdlg.h>

#include "app/mnemonic.hpp"

namespace uagfx::app {
namespace {
```
на:
```cpp
#include <commdlg.h>

#include "app/mnemonic.hpp"
#include "app/web.hpp"

namespace uagfx::app {
namespace {
```

В `src/app/main_window.cpp` заменить:
```cpp
constexpr int kUndo = 22;
constexpr int kCloseButton = 23;
constexpr UINT kRebuild = WM_APP + 1;
constexpr UINT_PTR kTimer = 1;

}  // namespace
```
на:
```cpp
constexpr int kUndo = 22;
constexpr int kCloseButton = 23;
constexpr UINT kRebuild = WM_APP + 1;
constexpr UINT kUpdateChecked = WM_APP + 2;  // GitHub's answer (web.hpp)
constexpr UINT_PTR kTimer = 1;

}  // namespace
```

В `src/app/main_window.cpp` заменить:
```cpp
    return ok ? std::wstring(file) : std::wstring();
}

bool MainWindow::confirmClose() { return model_->pendingCount() == 0 || ask(wtr(lang_, Text::AskUnsaved)); }

std::wstring MainWindow::windowLetters() const {
```
на:
```cpp
    return ok ? std::wstring(file) : std::wstring();
}

void MainWindow::checkUpdate() {
    model_->updateStarted();
    if (!checkUpdateAsync(hwnd_, kUpdateChecked)) model_->updateFinished(GetLastError(), "");
    rebuild();
}

void MainWindow::closeForScript() {
    closing_ = true;
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

bool MainWindow::confirmClose() { return model_->pendingCount() == 0 || ask(wtr(lang_, Text::AskUnsaved)); }

std::wstring MainWindow::windowLetters() const {
```

В `src/app/main_window.cpp` заменить:
```cpp
            }
            return 0;
        }
        case WM_CLOSE:
            if (confirmClose()) DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kTimer);
```
на:
```cpp
            }
            return 0;
        }
        case kUpdateChecked: {
            const std::unique_ptr<std::string> text(reinterpret_cast<std::string*>(lp));
            model_->updateFinished(static_cast<unsigned long>(wp), *text);
            rebuild();
            return 0;
        }
        case WM_CLOSE:
            if (closing_ || confirmClose()) DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kTimer);
```

В `src/app/main_window.hpp` заменить:
```cpp
    void message(const std::wstring& text, bool error) override;
    bool ask(const std::wstring& text) override;
    std::wstring pickFile(bool save) override;

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
```
на:
```cpp
    void message(const std::wstring& text, bool error) override;
    bool ask(const std::wstring& text) override;
    std::wstring pickFile(bool save) override;
    void checkUpdate() override;
    void closeForScript() override;

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
```

В `src/app/main_window.hpp` заменить:
```cpp
    void updateStatus();
    int box(const std::wstring& text, UINT type);
    bool confirmClose();
    std::wstring windowLetters() const;  // Alt+letter of the window's own buttons
    void doApply(bool play);
    RECT pageArea() const;
```
на:
```cpp
    void updateStatus();
    int box(const std::wstring& text, UINT type);
    bool confirmClose();
    bool closing_ = false;  // closeForScript: the player has answered already
    std::wstring windowLetters() const;  // Alt+letter of the window's own buttons
    void doApply(bool play);
    RECT pageArea() const;
```

В `src/app/page.hpp` заменить:
```cpp
    virtual bool ask(const std::wstring& text) = 0;  // Yes or No
    // A .reg file to save the game's settings to (save) or to load them from; "" when the player cancels.
    virtual std::wstring pickFile(bool save) = 0;
};

// One page of the list on the left (stage 2 spec §6.1). Its controls' ids start at 1000.
```
на:
```cpp
    virtual bool ask(const std::wstring& text) = 0;  // Yes or No
    // A .reg file to save the game's settings to (save) or to load them from; "" when the player cancels.
    virtual std::wstring pickFile(bool save) = 0;
    virtual void checkUpdate() = 0;     // asks GitHub; the page shows the answer when it comes (spec §8)
    virtual void closeForScript() = 0;  // update.bat or uninstall.bat runs: the app closes without asking again
};

// One page of the list on the left (stage 2 spec §6.1). Its controls' ids start at 1000.
```

В `src/app/page_service.cpp` заменить:
```cpp
constexpr int kGameSave = 1430;
constexpr int kGameLoad = 1431;
constexpr int kLayoutReset = 1432;

Text presetName(Preset p) {
    switch (p) {
```
на:
```cpp
constexpr int kGameSave = 1430;
constexpr int kGameLoad = 1431;
constexpr int kLayoutReset = 1432;
constexpr int kCheck = 1440;
constexpr int kUpdate = 1441;
constexpr int kUninstall = 1442;

Text presetName(Preset p) {
    switch (p) {
```

В `src/app/page_service.cpp` заменить:
```cpp
            p.note(t(Text::GameSettingsNote));
        p.gap();
        p.heading(t(Text::ServiceUaroGraphics));
        p.buttons({{kReset, t(Text::ResetButton)}});
        p.note(tf(Text::VersionsValue,
                  {kVersion, m.now().installedVersion.empty() ? utf8(t(Text::Unknown)) : m.now().installedVersion}),
               Tone::Normal);
    }

```
на:
```cpp
            p.note(t(Text::GameSettingsNote));
        p.gap();
        p.heading(t(Text::ServiceUaroGraphics));
        const UpdateCheck& u = m.updateCheck();
        std::vector<ButtonItem> own{{kReset, t(Text::ResetButton)}};
        if (u.state == UpdateState::Newer)
            own.push_back({kUpdate, tf(Text::UpdateRun, {versionOf(u.remote)})});
        else
            own.push_back({kCheck, t(Text::UpdateCheck), u.state != UpdateState::Checking});
        own.push_back({kUninstall, t(Text::Uninstall)});
        p.buttons(own);
        p.note(tf(Text::VersionsValue,
                  {kVersion, m.now().installedVersion.empty() ? utf8(t(Text::Unknown)) : m.now().installedVersion}) +
                   updateText(u),
               Tone::Normal);
    }

```

В `src/app/page_service.cpp` заменить:
```cpp
            const std::wstring err = m.resetWindowLayout(l);
            host_.message(err.empty() ? t(Text::LayoutResetDone) : err, !err.empty());
            host_.reload();
        }
    }

private:
    HWND list_ = nullptr;
};

```
на:
```cpp
            const std::wstring err = m.resetWindowLayout(l);
            host_.message(err.empty() ? t(Text::LayoutResetDone) : err, !err.empty());
            host_.reload();
        } else if (id == kCheck) {
            host_.checkUpdate();
        } else if (id == kUpdate || id == kUninstall) {  // spec §8: the script replaces or removes the app itself
            const bool update = id == kUpdate;
            std::wstring question = update ? tf(Text::UpdateAsk, {m.updateCheck().remote}) : t(Text::UninstallAsk);
            if (m.pendingCount() > 0) question += t(Text::UnsavedLost);
            if (!host_.ask(question)) return;
            const std::wstring err = m.runScript(update ? L"update.bat" : L"uninstall.bat", l);
            if (err.empty())
                host_.closeForScript();  // the script waits for it to close
            else
                host_.message(err, true);
        }
    }

private:
    // "0.6.3 build 2026-10-20 10:00" -> "0.6.3": the update button's text
    static std::string versionOf(const std::string& build) {
        const auto b = parseBuild(build);
        return b ? std::to_string(b->major) + "." + std::to_string(b->minor) + "." + std::to_string(b->patch) : build;
    }

    std::wstring updateText(const UpdateCheck& u) const {
        switch (u.state) {
            case UpdateState::Checking: return L" · " + t(Text::UpdateChecking);
            case UpdateState::Latest: return L" · " + t(Text::UpdateLatest);
            case UpdateState::Newer: return L" · " + tf(Text::UpdateNewer, {u.remote});
            case UpdateState::Failed: return L" · " + tf(Text::UpdateFailed, {u.error ? std::to_string(u.error) : "?"});
            case UpdateState::Never: break;
        }
        return L"";
    }

    HWND list_ = nullptr;
};

```

`src/app/web.cpp`:
```cpp
#include "app/web.hpp"

#include <winhttp.h>

#include <string>

namespace uagfx::app {
namespace {

// update.bat downloads the package from the same place (dist/update.bat).
constexpr wchar_t kHost[] = L"raw.githubusercontent.com";
constexpr wchar_t kPath[] = L"/kwebmn/ragnarok-graphics/main/release/version.txt";

struct Job {
    HWND window;
    UINT message;
};

unsigned long fetch(std::string& out) {
    HINTERNET session = WinHttpOpen(L"uaRO Graphics Settings", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return GetLastError();
    // GitHub takes TLS 1.2 only: Windows 7 has it with KB3140245, else the check fails and says so (spec §12).
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1;
    WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols);
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);
    unsigned long error = 0;
    HINTERNET connect = WinHttpConnect(session, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", kPath, nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                : nullptr;
    if (!request || !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        error = GetLastError();
    } else {
        DWORD status = 0;
        DWORD size = sizeof status;
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        if (status != 200) error = status;
        char buf[512];
        DWORD got = 0;
        while (!error && out.size() < 4096 && WinHttpReadData(request, buf, sizeof buf, &got) && got)
            out.append(buf, got);
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return error;
}

DWORD WINAPI worker(void* param) {
    const Job job = *static_cast<Job*>(param);
    delete static_cast<Job*>(param);
    auto* text = new std::string;
    const unsigned long error = fetch(*text);
    if (!PostMessageW(job.window, job.message, error, reinterpret_cast<LPARAM>(text))) delete text;  // window gone
    return 0;
}

}  // namespace

bool checkUpdateAsync(HWND window, UINT message) {
    auto* job = new Job{window, message};
    HANDLE thread = CreateThread(nullptr, 0, worker, job, 0, nullptr);
    if (!thread) {
        delete job;
        return false;
    }
    CloseHandle(thread);
    return true;
}

}  // namespace uagfx::app
```

`src/app/web.hpp`:
```cpp
#pragma once
#include <windows.h>

namespace uagfx::app {

// The update check (stage 2 spec §8): GitHub's release/version.txt over HTTPS (WinHTTP) on a thread of its own. Its
// answer comes to `window` as `message`: wParam - 0, the system's error or the HTTP status; lParam - the text, a new
// std::string the window deletes. False when the thread could not start (GetLastError says why).
bool checkUpdateAsync(HWND window, UINT message);

}  // namespace uagfx::app
```

В `tools/check_exe.py` заменить:
```python
    "comdlg32.dll": {
        "GetOpenFileNameW", "GetSaveFileNameW",
    },
    "gdi32.dll": {
        "CreateFontIndirectW", "DeleteObject", "GetDeviceCaps", "GetTextExtentPoint32W", "SelectObject",
        "SetBkMode", "SetTextColor",
    },
    "kernel32.dll": {
        "CloseHandle", "CreateDirectoryW", "CreateFileW", "CreateSemaphoreW", "CreateToolhelp32Snapshot",
        "DeleteCriticalSection", "DeleteFileW", "EnterCriticalSection", "FindClose", "FindFirstFileW",
        "FindNextFileW", "FindResourceW", "FlushFileBuffers", "FormatMessageA", "FreeLibrary", "GetCurrentProcess",
        "GetCurrentThreadId", "GetEnvironmentVariableW", "GetFileAttributesW", "GetFileSizeEx", "GetLastError",
```
на:
```python
    "comdlg32.dll": {
        "GetOpenFileNameW", "GetSaveFileNameW",
    },
    "winhttp.dll": {
        "WinHttpCloseHandle", "WinHttpConnect", "WinHttpOpen", "WinHttpOpenRequest", "WinHttpQueryHeaders",
        "WinHttpReadData", "WinHttpReceiveResponse", "WinHttpSendRequest", "WinHttpSetOption", "WinHttpSetTimeouts",
    },
    "gdi32.dll": {
        "CreateFontIndirectW", "DeleteObject", "GetDeviceCaps", "GetTextExtentPoint32W", "SelectObject",
        "SetBkMode", "SetTextColor",
    },
    "kernel32.dll": {
        "CloseHandle", "CreateDirectoryW", "CreateFileW", "CreateSemaphoreW", "CreateThread",
        "CreateToolhelp32Snapshot",
        "DeleteCriticalSection", "DeleteFileW", "EnterCriticalSection", "FindClose", "FindFirstFileW",
        "FindNextFileW", "FindResourceW", "FlushFileBuffers", "FormatMessageA", "FreeLibrary", "GetCurrentProcess",
        "GetCurrentThreadId", "GetEnvironmentVariableW", "GetFileAttributesW", "GetFileSizeEx", "GetLastError",
```

- [ ] **Step 2: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 212 functions …`, затем `315 tests, 0 failed checks`.

- [ ] **Step 3: Коммит**

```bash
git add CMakeLists.txt dist/uninstall.bat src/app/main_window.cpp src/app/main_window.hpp src/app/page.hpp src/app/page_service.cpp src/app/web.cpp src/app/web.hpp tools/check_exe.py
git commit -q -F - <<'EOF'
Утилита: «Сервис» — проверить обновление, обновить, удалить

Проверка — WinHTTP к raw.githubusercontent.com в своём потоке; «Обновить до …»
и «Удалить uaRO Graphics…» запускают update.bat и uninstall.bat и закрывают
утилиту; uninstall.bat ждёт её до 10 с. WinHTTP и CreateThread — в списке
check_exe (§8).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 9: Закрытие с недописанным полем; лог драйвера при откате окна (0.6.2, M6, M7)

Esc, крестик и Alt+F4 сначала забирают поле, в котором набирают текст, и только потом спрашивают о несохранённом:
раньше недописанный макрос пропадал молча (M6). Драйвер пишет в лог, куда встало окно, если поставить его в настоящих
точках не вышло; в комментариях — Windows 10 1703: per-monitor v2 появился в ней (M7). Только Windows: проверка —
сборка и пункт 10 чек-листа.

**Files:**
- Modify: `src/app/main_window.cpp`, `src/win/dpi.hpp`, `src/win/window_mode.cpp`

**Interfaces:**
- Consumes: —
- Produces: —

- [ ] **Step 1: Правки**

В `src/app/main_window.cpp` заменить:
```cpp
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

bool MainWindow::confirmClose() { return model_->pendingCount() == 0 || ask(wtr(lang_, Text::AskUnsaved)); }

std::wstring MainWindow::windowLetters() const {
    std::wstring taken;
```
на:
```cpp
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

bool MainWindow::confirmClose() {
    // A field being typed in counts: Esc, the title bar's X and Alt+F4 take it first (0.6.2 review M6).
    HWND focus = GetFocus();
    const int focusId = focus && IsChild(hwnd_, focus) ? GetDlgCtrlID(focus) : 0;
    if (focusId >= 1000 && current_) current_->command(focusId, EN_KILLFOCUS);
    return model_->pendingCount() == 0 || ask(wtr(lang_, Text::AskUnsaved));
}

std::wstring MainWindow::windowLetters() const {
    std::wstring taken;
```

В `src/win/dpi.hpp` заменить:
```cpp
SIZE clientPixels(HWND hwnd, bool physical);

// While it lives, this thread is per-monitor DPI aware: what it measures and places is in the screen's real pixels.
// Windows 10 1607 and newer; before that active() is false and nothing changes.
class RealPixels {
public:
    RealPixels();
```
на:
```cpp
SIZE clientPixels(HWND hwnd, bool physical);

// While it lives, this thread is per-monitor DPI aware: what it measures and places is in the screen's real pixels.
// Per-monitor v2 is Windows 10 1703 and newer; before that active() is false and nothing changes (0.6.2 review M7).
class RealPixels {
public:
    RealPixels();
```

В `src/win/window_mode.cpp` заменить:
```cpp

// Borderless over the monitor in its real pixels. uaRO.exe is not DPI-aware: in its coordinates a 3840 x 2160 monitor
// at 175 % is 2194 x 1234, and a window of that size came out 3839 x 2159 (0.6.1). False when it did not land on the
// monitor's pixels - Windows before 10 1607, or one that placed it otherwise - and `got` is where it went.
bool placeInPixels(HWND hwnd, int monitor, RECT& got) {
    RealPixels real;
    if (!real.active()) return false;
```
на:
```cpp

// Borderless over the monitor in its real pixels. uaRO.exe is not DPI-aware: in its coordinates a 3840 x 2160 monitor
// at 175 % is 2194 x 1234, and a window of that size came out 3839 x 2159 (0.6.1). False when it did not land on the
// monitor's pixels - Windows before 10 1703, or one that placed it otherwise - and `got` is where it went.
bool placeInPixels(HWND hwnd, int monitor, RECT& got) {
    RealPixels real;
    if (!real.active()) return false;
```

В `src/win/window_mode.cpp` заменить:
```cpp
                     displayModeToken(config.mode), got.right - got.left, got.bottom - got.top, got.left, got.top);
            return;
        }
        SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
```
на:
```cpp
                     displayModeToken(config.mode), got.right - got.left, got.bottom - got.top, got.left, got.top);
            return;
        }
        if (got.right != got.left)  // placed in real pixels, but it went elsewhere (0.6.2 review M7)
            LOG_INFO("window: real pixels gave %ldx%ld at %ld,%ld; placed in the game's points", got.right - got.left,
                     got.bottom - got.top, got.left, got.top);
        SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
```

- [ ] **Step 2: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 212 functions …`, затем `315 tests, 0 failed checks`.

- [ ] **Step 3: Коммит**

```bash
git add src/app/main_window.cpp src/win/dpi.hpp src/win/window_mode.cpp
git commit -q -F - <<'EOF'
Утилита и драйвер: закрытие с недописанным полем; лог при откате окна

Закрытие сначала забирает набранный текст и спрашивает о несохранённом
(ревью 0.6.2, M6); драйвер пишет, куда встало окно, если настоящие точки не
вышли, комментарии — Windows 10 1703 (M7).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 10: Выпуск 0.6.3 — версия, `release/version.txt`, спецификация, чек-лист (§11)

Версия 0.6.3. `tools/package.sh` кладёт рядом с пакетом `build-win/version.txt` — ту же строку, что в нём: при выпуске
она копируется в `release/version.txt`, его читает проверка обновления. Спецификация этапа 2: оба модуля по очереди
(§2.3), ключи ROExt — названные в его DLL, `KeyRemap` не показан, пока нет `dinput.dll` новой сборки, значения
dgVoodoo (§2.4), перенос настроек игры без логина и с экраном ПК, обновление и удаление (§8), строка 0.6.3 (§11);
колонки: игра передаёт `SPEAKERTYPE` в MSS 6.1a как есть, названия — по константам `AIL_3D_*` (§6.1, §12, M4).
Чек-лист 0.6.3.

**Files:**
- Modify: `dist/TEST.ru.md`, `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md`, `src/core/version.cpp`, `tests/test_version.cpp`, `tools/package.sh`

**Interfaces:**
- Consumes: —
- Produces: `kVersion = "0.6.3"`; `build-win/version.txt`.

- [ ] **Step 1: Падающий тест версии**

В `tests/test_version.cpp` заменить:
```cpp
#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_6_2) { CHECK(std::strcmp(uagfx::kVersion, "0.6.2") == 0); }
```
на:
```cpp
#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_6_3) { CHECK(std::strcmp(uagfx::kVersion, "0.6.3") == 0); }
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: `FAIL version_is_0_6_3`, `315 tests, 1 failed checks`.

- [ ] **Step 3: Версия, пакет, спецификация, чек-лист**

`dist/TEST.ru.md`:
```markdown
# uaRO Graphics 0.6.3 — проверка

Что нового:
- Раздел «Ввод и окно»: настройки ROExt или uaRO Pad — смотря что загружает игра (у тебя uaRO Pad). У ROExt — только
  ключи, которые читает его сборка.
- Раздел «dgVoodoo»: его полный экран, масштабирование, фильтрация, сглаживание, разрешение и прочее; работает только с
  рендером dgVoodoo.
- «Сервис»: «Проверить обновление» и «Обновить до …», «Удалить uaRO Graphics…» прямо из утилиты.
- Мелочи ревью 0.6.2: файл настроек игры без логина; загрузка оставляет экран и звук этого ПК; закрытие с недописанным
  макросом спрашивает; «Чёткий текст в прошлый раз» после выключения на лету без старого масштаба.

## Установка

Закрой игру и утилиту и дважды щёлкни `update.bat` в папке клиента.

## Проверка

1. `update.bat` в конце написал `uaRO Graphics 0.6.3 build … is installed`.
2. Запусти `uaro-graphics.exe`. Пройди «Ввод и окно», «dgVoodoo» и «Сервис» по-русски, по-украински и по-английски:
   тексты не обрезаны, не наезжают друг на друга, всё помещается в окно.
3. **Ввод и окно.** Заголовок «uaRO Pad (dinput.dll)». Поставь «Средняя кнопка мыши» — «F3» → «Применить и играть»:
   средняя кнопка в игре работает как F3. В `uaro-pad.ini` строка `middle = F3`, комментарии в файле на месте. Потом
   верни «ничего».
4. **Пока игра запущена** — «Ввод и окно» и «dgVoodoo» серые, внизу записка, что программа читает свой файл при
   запуске игры.
5. **dgVoodoo.** С рендером uaRO Graphics вверху жёлтая строка, что dgVoodoo не работает. Значения в списках — как в
   `dgVoodoo.conf` (по умолчанию «как просит игра»). Если хочешь проверить сам dgVoodoo: «Рендер» → dgVoodoo,
   «dgVoodoo» → «Сглаживание» — 4x → «Применить и играть»: края моделей ровнее. Потом верни рендер uaRO Graphics.
6. **Проверить обновление.** «Сервис» → «Проверить обновление»: в строке версий «проверяю GitHub…», потом «самая
   новая сборка».
7. **Обновить.** Закрой утилиту, открой `uaro-graphics\version.txt` в Блокноте, замени `0.6.3` на `0.6.2` и сохрани.
   Запусти утилиту: «Сервис» → «Проверить обновление» — «на GitHub 0.6.3 build …» и кнопка «Обновить до 0.6.3». Нажми
   её и согласись: утилита закрывается, открывается окно `update.bat` и ставит 0.6.3 заново.
8. **Удалить** — не нажимай, если не собираешься удалять. Если нажмёшь «Удалить uaRO Graphics…» и ответишь «Нет»,
   ничего не происходит.
9. **Файл настроек без логина.** «Сервис» → «Сохранить в файл…», открой файл в Блокноте: строки `"ID"` нет.
10. **Закрытие с недописанным.** «Чат и вход»: впиши что-нибудь в «Alt+5» и, не уходя из поля, нажми Esc — утилита
    спрашивает про несохранённые изменения. «Нет» — она остаётся; потом сотри текст.
11. **Чёткий текст на лету.** Пока игра идёт: «Качество» → сними «Чёткий текст» → «Применить». Через пару секунд
    «Диагностика» → «Проверить снова»: «Чёткий текст в прошлый раз» — ровно `off (crisp_text = 0)`. Потом включи
    обратно.

## Что прислать

Файл `uaro-graphics.log` и коротко по каждому пункту. Если утилита что-то показывает не так — снимок её окна
(Win+Shift+S). Кнопка «Отчёт для форума» в «Диагностике» копирует всё нужное в буфер обмена.

## Если что-то не так

Вернуть настройки: «Сервис» → «Резервные копии» → «Восстановить». Вернуть dgVoodoo без утилиты: `use-dgvoodoo.bat`.

## Удаление

Закрой утилиту и дважды щёлкни `uninstall.bat` (или «Сервис» → «Удалить uaRO Graphics…») — всё вернётся как было.
Папка `uaro-graphics-backup` с копиями настроек останется.
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
- имя ini — найденная в DLL строка `.\<имя>.ini`.

Активны DLL, которые игра действительно загрузит: `dinput.dll` в папке клиента и `dinput8.dll` в папке клиента (его
подгружает системный `dinput.dll`). Копии `.bak` неактивны. Если активны и ROExt, и uaRO Pad, утилита показывает оба.

Ключи `[ROExt]`:
- `MouseFreedom`, `RemapMouseButtons` (кнопки мыши 3–5 → F24–F22), `RemapAltF4` (Alt+F4 → Alt+PrintScreen),
```
на:
```markdown
- имя ini — найденная в DLL строка `.\<имя>.ini`.

Активны DLL, которые игра действительно загрузит: `dinput.dll` в папке клиента и `dinput8.dll` в папке клиента (его
подгружает системный `dinput.dll`). Копии `.bak` неактивны. Если активны и ROExt, и uaRO Pad, раздел показывает их
по очереди: переключатель вверху, на оба места нет (0.6.3).

Ключи `[ROExt]`:
- `MouseFreedom`, `RemapMouseButtons` (кнопки мыши 3–5 → F24–F22), `RemapAltF4` (Alt+F4 → Alt+PrintScreen),
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
- `WindowPosX`, `WindowPosY`, `WindowWidth`, `WindowHeight` (0 — не задано), `CodePage` (−1 — не менять), `MinimizeKey`
  (только старая сборка).

Что делает `KeyRemap` и как работает `MouseFreedom` новой сборки, неизвестно. Для выпуска 0.6.3 нужен `dinput.dll`
ROExt от пользователя (§12).

**uaRO Pad** — `dinput.dll`, в котором есть строка `uaro-pad.ini`. Он заменяет ROExt: по инструкции ROExt
переименовывают в `.bak`. Утилите нужны ключи окна и мыши:
```
на:
```markdown
- `WindowPosX`, `WindowPosY`, `WindowWidth`, `WindowHeight` (0 — не задано), `CodePage` (−1 — не менять), `MinimizeKey`
  (только старая сборка).

Сборки читают разные ключи: в `dinput8.dll` из архива клиента (8704 байта) нет `MouseFreedom` и `KeyRemap`, они есть
в оставшемся там `dinput.ini` новой сборки. Имена ключей, которые сборка читает, лежат в её DLL отдельными строками
между нулями, и «Ввод и окно» показывает только ключи, названные в загруженной DLL (0.6.3). Что делает `KeyRemap` и
как работает `MouseFreedom` новой сборки, неизвестно: её `dinput.dll` в архиве клиента нет (§12). Поэтому `KeyRemap`
раздел не показывает и не трогает, а `MouseFreedom` у сборки, которая его называет, — флажок «Свободная мышь». Что
делают остальные ключи, сказано в `dinput8.ini` старой сборки.

**uaRO Pad** — `dinput.dll`, в котором есть строка `uaro-pad.ini`. Он заменяет ROExt: по инструкции ROExt
переименовывают в `.bak`. Утилите нужны ключи окна и мыши:
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
  `Contrast`;
- `[GeneralExt]`: `FreeMouse`;
- `[DirectX]`: `Filtering`, `Antialiasing`, `Resolution`, `ForceVerticalSync`, `dgVoodooWatermark`.

## 3. Устройство

```
на:
```markdown
  `Contrast`;
- `[GeneralExt]`: `FreeMouse`;
- `[DirectX]`: `Filtering`, `Antialiasing`, `Resolution`, `ForceVerticalSync`, `dgVoodooWatermark`.

Значения — из комментариев `dgVoodoo.conf` версии 2.55 (0.6.3):
- `ScalingMode`: unspecified, centered, stretched, centered_ar, stretched_ar, stretched_4_3, stretched_4_3_crt,
  stretched_4_3_c64;
- `Filtering`: appdriven, pointsampled, bilinear, linearmip, trilinear или анизотропная 2, 4, 8, 16;
- `Antialiasing`: appdriven, off, 2x, 4x, 8x, 16x;
- `Resolution`: unforced, max, 2x, 3x, 4x;
- `Brightness`, `Color`, `Contrast`: 50–150 % с шагом 5 (весь допустимый диапазон dgVoodoo не проверялся).

Значение из файла, которого нет в списке, показывается как есть и остаётся, пока игрок не выберет другое. Пока выбран
рендер uaRO Graphics, раздел предупреждает, что dgVoodoo не работает.

## 3. Устройство

```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
| Сервис | §8 | — | 0.6.0–0.6.3 |

Пояснения к разделам:
- **Колонки.** Названия значений `SPEAKERTYPE` взяты из заголовков MSS 6 и требуют проверки (§12).
- **Шёпот.** У клиента ошибка загрузки: «от друзей» включает и «от незнакомых». Утилита это показывает, а не скрывает.
- **Раздел «Игра».** Значения, которые не хранятся в реестре (`/noctrl`, `/aura`, `/window` и т. п.), сюда не входят:
  их нельзя задать заранее.
```
на:
```markdown
| Сервис | §8 | — | 0.6.0–0.6.3 |

Пояснения к разделам:
- **Колонки.** Игра передаёт `SPEAKERTYPE` в `AIL_set_3D_speaker_type` Miles Sound System как есть, поэтому значения
  подписаны по константам `AIL_3D_*` его заголовков (§12).
- **Шёпот.** У клиента ошибка загрузки: «от друзей» включает и «от незнакомых». Утилита это показывает, а не скрывает.
- **Раздел «Игра».** Значения, которые не хранятся в реестре (`/noctrl`, `/aura`, `/window` и т. п.), сюда не входят:
  их нельзя задать заранее.
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
| Пресеты | заполняют поля, записывает их «Применить» (таблица ниже) | 0.6.0 |
| Резервные копии | список, «Восстановить», «Оставить 5 последних» (§4.6) | 0.6.0 |
| Сбросить uaRO Graphics | ini по умолчанию из `RCDATA`, кроме `backend` и `[utility]` | 0.6.0 |
| Сохранить настройки игры в файл / загрузить из файла | действующие значения обеих копий (весь ключ игры: панели навыков, раскладка окон, опции, макросы) — файл `.reg` с путём личной копии; на другом ПК его загружает утилита (или `regedit` двойным щелчком) | 0.6.2 |
| Сбросить раскладку окон | `.X = -1` у каждого окна `UIRectInfo` в личной копии (игра ставит их по местам при открытии), `M_CHATWNDY = -1` для чата | 0.6.2 |
| Проверить обновление / обновить | `release/version.txt` с GitHub (WinHTTP) против `uaro-graphics\version.txt`; «Обновить» запускает `update.bat` и закрывает утилиту | 0.6.3 |
| Удалить uaRO Graphics | `uninstall.bat` после подтверждения, утилита закрывается | 0.6.3 |
| Версии | утилита, драйвер | 0.6.0 |

На странице четыре группы: пресеты; резервные копии; настройки игры — «Сохранить в файл…», «Загрузить из файла…»,
```
на:
```markdown
| Пресеты | заполняют поля, записывает их «Применить» (таблица ниже) | 0.6.0 |
| Резервные копии | список, «Восстановить», «Оставить 5 последних» (§4.6) | 0.6.0 |
| Сбросить uaRO Graphics | ini по умолчанию из `RCDATA`, кроме `backend` и `[utility]` | 0.6.0 |
| Сохранить настройки игры в файл / загрузить из файла | действующие значения обеих копий (весь ключ игры: панели навыков, раскладка окон, опции, макросы) — файл `.reg` с путём личной копии; на другом ПК его загружает утилита (или `regedit` двойным щелчком). С 0.6.3 в файле нет логина (`ID`) и старого пароля (`PASSWORD`); загрузка оставляет этому ПК его экран, звук, видеокарту и логин, а значения, равные общей копии, в личную не кладёт (§4.2) | 0.6.2 |
| Сбросить раскладку окон | `.X = -1` у каждого окна `UIRectInfo` в личной копии (игра ставит их по местам при открытии), `M_CHATWNDY = -1` для чата | 0.6.2 |
| Проверить обновление / обновить | `release/version.txt` с GitHub (`raw.githubusercontent.com`, WinHTTP, в отдельном потоке) против `uaro-graphics\version.txt`: выше версия или та же, собранная позже; ответ — в строке версий; «Обновить до …» после подтверждения запускает `update.bat` и закрывает утилиту, `update.bat` ждёт её закрытия до 10 с | 0.6.3 |
| Удалить uaRO Graphics | `uninstall.bat` после подтверждения, утилита закрывается; `uninstall.bat` ждёт её закрытия до 10 с | 0.6.3 |
| Версии | утилита, драйвер | 0.6.0 |

На странице четыре группы: пресеты; резервные копии; настройки игры — «Сохранить в файл…», «Загрузить из файла…»,
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
| 0.6.0 | утилита: Рендер, Экран, Качество (наше), Кадры, Диагностика, Сервис (пресеты, резервные копии, сброс, версии); языки; применение на лету в драйвере; первый запуск без `setup.exe`; запись размера окна ROExt (§4.4); пакет и `.bat` |
| 0.6.1 | мелочи финальных ревью 0.5.2 (драйвер) и 0.6.0 (утилита): клавиатура и Alt+буква (§6), диагностика по §7, реестр по одному значению (§4.5), экран с ROExt ждёт закрытия игры (§4.1) |
| 0.6.2 | Звук, Игра, Чат и вход, качество игры в «Качестве», перенос настроек игры, сброс раскладки окон, пресеты с качеством игры; полный экран в настоящих точках монитора; мелочи финального ревью 0.6.1 |
| 0.6.3 | Ввод и окно (ROExt, uaRO Pad), dgVoodoo, обновление и удаление из утилиты, `release/version.txt` |

Одна спецификация, отдельный план на каждый выпуск. План следующего выпуска пишется после проверки предыдущего.

```
на:
```markdown
| 0.6.0 | утилита: Рендер, Экран, Качество (наше), Кадры, Диагностика, Сервис (пресеты, резервные копии, сброс, версии); языки; применение на лету в драйвере; первый запуск без `setup.exe`; запись размера окна ROExt (§4.4); пакет и `.bat` |
| 0.6.1 | мелочи финальных ревью 0.5.2 (драйвер) и 0.6.0 (утилита): клавиатура и Alt+буква (§6), диагностика по §7, реестр по одному значению (§4.5), экран с ROExt ждёт закрытия игры (§4.1) |
| 0.6.2 | Звук, Игра, Чат и вход, качество игры в «Качестве», перенос настроек игры, сброс раскладки окон, пресеты с качеством игры; полный экран в настоящих точках монитора; мелочи финального ревью 0.6.1 |
| 0.6.3 | Ввод и окно (ROExt, uaRO Pad), dgVoodoo, обновление и удаление из утилиты, `release/version.txt`; мелочи финального ревью 0.6.2; `KeyRemap` ROExt — когда будет его `dinput.dll` |

Одна спецификация, отдельный план на каждый выпуск. План следующего выпуска пишется после проверки предыдущего.

```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
| Полный экран клиента с нашим рендером не проверялся | первый пункт чек-листа 0.6.0; запасной путь — §5.4 |
| Игра может не увидеть ключ, которого нет в общей копии (первый запуск) | проверка на чистой системе или виртуальной машине; если не видит — утилита предлагает один раз запустить `setup.exe` |
| Microsoft обещает когда-нибудь убрать виртуализацию | тогда игра сама не сможет сохранять настройки; диагностика показывает, где лежат её значения |
| Новый ROExt: что делает `KeyRemap`, как `MouseFreedom` переводит указатель при окне, отличном от размера игры | до 0.6.3 нужен `dinput.dll` ROExt от пользователя; до тех пор «Экран» пишет только `WindowWidth` и `WindowHeight` |
| Названия значений `SPEAKERTYPE`, значение `DEVICECNT`/`MODECNT` | для 0.6.2 нужны `mss32.dll` и `setup.exe` из клиента пользователя; до тех пор колонки подписаны номерами с пометкой |
| Какую из двух копий `dgVoodoo.conf` читает dgVoodoo | пишутся обе одинаково |
| Окно утилиты при смене DPI или на Windows 7 | `WM_DPICHANGED`; Windows 7 проверяется по возможности в виртуальной машине |
| Проверка обновлений на Windows 7 (TLS 1.2) | при ошибке — «не удалось проверить», `update.bat` работает как раньше |
```
на:
```markdown
| Полный экран клиента с нашим рендером не проверялся | первый пункт чек-листа 0.6.0; запасной путь — §5.4 |
| Игра может не увидеть ключ, которого нет в общей копии (первый запуск) | проверка на чистой системе или виртуальной машине; если не видит — утилита предлагает один раз запустить `setup.exe` |
| Microsoft обещает когда-нибудь убрать виртуализацию | тогда игра сама не сможет сохранять настройки; диагностика показывает, где лежат её значения |
| Новый ROExt: что делает `KeyRemap`, как `MouseFreedom` переводит указатель при окне, отличном от размера игры | `dinput.dll` новой сборки нет: «Ввод и окно» показывает ключи, названные в загруженной DLL, кроме `KeyRemap`; «Экран» пишет только `WindowWidth` и `WindowHeight` |
| Названия значений `SPEAKERTYPE`, значение `DEVICECNT`/`MODECNT` | игра передаёт `SPEAKERTYPE` в `AIL_set_3D_speaker_type` открытого 3D-провайдера без изменений (декомпиляция `uaRO.exe`), `mss32.dll` клиента — MSS 6.1a, эту функцию экспортирует: колонки подписаны по константам `AIL_3D_*` (2 колонки, наушники, объёмный, 4 колонки, 5.1, 7.1). Знает ли MSS 6.1a 5.1 и 7.1, не проверено — у поля подсказка «на этом клиенте не проверены». `setup.exe` сжат PECompact, его строки без распаковки не прочесть: `DEVICECNT` и `MODECNT` не проверены (0.6.3, ревью 0.6.2 M4) |
| Какую из двух копий `dgVoodoo.conf` читает dgVoodoo | пишутся обе одинаково |
| Окно утилиты при смене DPI или на Windows 7 | `WM_DPICHANGED`; Windows 7 проверяется по возможности в виртуальной машине |
| Проверка обновлений на Windows 7 (TLS 1.2) | при ошибке — «не удалось проверить», `update.bat` работает как раньше |
```

В `src/core/version.cpp` заменить:
```cpp

namespace uagfx {

const char* const kVersion = "0.6.2";

}  // namespace uagfx
```
на:
```cpp

namespace uagfx {

const char* const kVersion = "0.6.3";

}  // namespace uagfx
```

В `tools/package.sh` заменить:
```sh
#!/usr/bin/env bash
# Builds the Windows DLL, checks it and packs build-win/uaro-graphics.zip: release/ keeps it under this one name, the
# link update.bat downloads. uaro-graphics/version.txt tells which build is installed.
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(sed -n 's/.*kVersion = "\(.*\)".*/\1/p' src/core/version.cpp)
```
на:
```sh
#!/usr/bin/env bash
# Builds the Windows DLL, checks it and packs build-win/uaro-graphics.zip: release/ keeps it under this one name, the
# link update.bat downloads. uaro-graphics/version.txt tells which build is installed; build-win/version.txt, the same
# line, goes to release/ too: the settings app's update check reads it.
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(sed -n 's/.*kVersion = "\(.*\)".*/\1/p' src/core/version.cpp)
```

В `tools/package.sh` заменить:
```sh
cp dist/install.bat dist/uninstall.bat dist/use-uaro.bat dist/use-dgvoodoo.bat dist/update.bat dist/TEST.ru.md "$stage/"
cp build-win/ddraw.dll build-win/uaro-graphics.exe dist/uaro-graphics.ini "$stage/uaro-graphics/"
printf '%s build %s\r\n' "$version" "$(date '+%Y-%m-%d %H:%M')" > "$stage/uaro-graphics/version.txt"
(cd "$stage" && zip -q -r "../uaro-graphics.zip" .)
python3 tools/check_package.py "$out"
echo "packed $out"
```
на:
```sh
cp dist/install.bat dist/uninstall.bat dist/use-uaro.bat dist/use-dgvoodoo.bat dist/update.bat dist/TEST.ru.md "$stage/"
cp build-win/ddraw.dll build-win/uaro-graphics.exe dist/uaro-graphics.ini "$stage/uaro-graphics/"
printf '%s build %s\r\n' "$version" "$(date '+%Y-%m-%d %H:%M')" > "$stage/uaro-graphics/version.txt"
cp "$stage/uaro-graphics/version.txt" build-win/version.txt
(cd "$stage" && zip -q -r "../uaro-graphics.zip" .)
python3 tools/check_package.py "$out"
echo "packed $out"
```

- [ ] **Step 4: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 212 functions …`, затем `315 tests, 0 failed checks`.

- [ ] **Step 5: Пакет**

Run: `tools/package.sh && unzip -p build-win/uaro-graphics.zip TEST.ru.md | head -1 && cat build-win/version.txt`
Expected: `packed build-win/uaro-graphics.zip`, `# uaRO Graphics 0.6.3 — проверка`, `0.6.3 build …`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_version.cpp dist/TEST.ru.md docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md src/core/version.cpp tools/package.sh
git commit -q -F - <<'EOF'
0.6.3: версия, version.txt для проверки обновления, спецификация, чек-лист

package.sh кладёт рядом с пакетом version.txt для release/. Спецификация этапа
2: оба модуля по очереди, ключи ROExt по его DLL, KeyRemap — когда будет
dinput.dll новой сборки, значения dgVoodoo, перенос настроек игры без логина,
обновление и удаление, строка 0.6.3, колонки — как их передаёт игра (ревью
0.6.2, M4). Чек-лист 0.6.3.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

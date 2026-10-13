# uaRO Graphics 0.6.2 — звук, игра, чат и вход, настройки игры в файл (план)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** выпуск 0.6.2 через `update.bat`: в утилите разделы «Звук», «Игра», «Чат и вход» и качество самой игры
в «Качестве» — настройки игры в реестре по правилам §4; в «Сервисе» — настройки игры в файл и из файла и сброс
раскладки окон; пресеты задают и качество игры; окно без рамки ровно по монитору; видеопамять 32-битной программы;
мелочи финального ревью 0.6.1.

**Architecture:** ядро (тесты на Linux): таблица 41 значения ключа игры — раздел, вид поля, как игра читает значение
(`docs/research/registry.md`), её значение по умолчанию, пишет ли его она сама; слово страницы ↔ значение реестра;
действующая копия ключа, сброс раскладки, `.reg` с любым из четырёх путей ключа, пресеты со словами реестра, текст
видеопамяти. Модель утилиты (тесты на подставном Windows): слова реестра ждут «Применить», как ini; что пишет игра,
ждёт её закрытия (§4.1); пометка setup.exe у звука (§4.2); файл настроек игры, сброс раскладки, «забыть логин»;
`screenWaits`. Окно (Win32): разделы строятся из таблицы, флажки и макросы — по два в ряд, буквы Alt только у
доступных; «Сервис» в четыре группы; выбор файла (comdlg32). Драйвер: окно без рамки в настоящих точках монитора; ini
без половины файла; строка лога о выключенном чётком тексте.

**Tech Stack:** C++17, MinGW-w64 i686 GCC 12, Win32 (+ comdlg32), Direct3D 12, тесты на Linux.

**Spec:** `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` (§4, §6.1, §7, §8, §11 — строка
0.6.2); разбор реестра `docs/research/registry.md`; мелочи финального ревью 0.6.1 (scratch `ledger-0.6.1.md`,
«Final: minor (deferred)» 5–11).

## Что и где

| Что | Откуда | Задача |
|---|---|---|
| разделы «Звук», «Игра», «Чат и вход»; тексты 0.6.2; `ApplyWaits` вместо «всё, кроме экрана» | §6.1; 0.6.1/7 | 1 |
| таблица значений ключа игры и правила их чтения | §6.1, registry.md | 2 |
| действующая копия, сброс раскладки, `.reg` с любого пути, пресеты с качеством игры | §8, §4.2 | 3 |
| слова реестра в модели: ждут «Применить», то, что пишет игра, — её закрытия | §4.1, §4.2 | 4 |
| настройки игры в файл и из файла, сброс раскладки окон | §8 | 5 |
| страницы «Качество» (игра), «Звук», «Игра», «Чат и вход»; пометка setup.exe у звука; буквы Alt только у доступных | §6.1, §4.2; 0.6.1/5 | 6 |
| «Сервис»: настройки игры, выбор файла, пресет с качеством игры | §8 | 7 |
| видеопамять 32-битной программы | §7 | 8 |
| окно без рамки в настоящих точках; ini без половины файла; строка лога чёткого текста | §5.4; 0.6.1/11 | 9 |
| «Экран» при ROExt и запущенной игре; старые номера в комментариях; комментарий кеша букв | §4.1; 0.6.1/6, 9, 11 | 10 |
| версия, спецификация, чек-лист | §11 | 11 |
| 0.6.1/10 — заголовок коммита `bcea604` называет не тот файл | — | без задачи: история `main` не переписывается |

## Global Constraints

- `ddraw.dll` — PE32 i386, импорты только `KERNEL32`, `USER32`, `GDI32`, `msvcrt`, пролог горячего патча, откат на
  dgVoodoo. `uaro-graphics.exe` — GUI, asInvoker, PerMonitorV2; импортирует только функции из списка
  `tools/check_exe.py`, каждая есть в Windows 7 (новые: `comdlg32!GetOpenFileNameW`, `GetSaveFileNameW`).
- §4.1: что игра пишет сама (весь `Option`, `UIRectInfo`, `ShortCutList`, `SPRITEMODE`, `TEXTUREMODE`,
  `TRILINEARFILTER`, `FOG`, `SHOWTIPSATSTARTUP`, `BINKMODE`, `CHECKSAVE`, `ID`), утилита пишет только при закрытой
  игре; устройство звука и `ISLIGHTMAP` — всегда; действует всё со следующего запуска.
- §4.2: запись — в личную копию; значение, равное общему, удаляет личное. §4.6: перед первой записью запуска — копия.
- Тексты — на английском, украинском и русском (апостроф в украинском — ASCII `'`); отчёт и лог — по-английски.
- Сборка без предупреждений; строки — до 120 знаков; комментарии по-английски, как в окружающем коде.
- Версия — `0.6.2`. Коммиты — KWEB Team, сообщения по-русски, трейлер `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Высота страниц.** При 96 DPI «Качество» занимает около 440 точек из 488, «Чат и вход» — около 464, «Сервис» —
   около 466. Длинные украинские и английские тексты и масштаб 125–200 % не должны выводить ряд за страницу на
   нижние кнопки. Это видно только на Windows: пункт 2 чек-листа.
2. **Текст, который игра читает через ANSI.** Макросы и имя скина с буквами не из кодовой страницы Windows (эмодзи,
   иероглифы) игра получит как `?`. Предел в 49 букв считается буквами (`validRegWord`, тест Task 2), а
   `EM_LIMITTEXT` — единицами UTF-16.
3. **Игра запустилась или закрылась при открытой странице.** Ряды сереют и оживают с перестройкой по таймеру.
   Слово, заданное до запуска игры, «Применить» держит в ожидании (`ApplyWaits`) и не теряет — тест Task 4.
4. **`.reg` с другого ПК или правленый руками.** Другой путь (HKLM, без WOW6432Node), CRLF, чужие ключи в том же
   файле. Личная копия заменяется, только если в файле есть ключ игры (тесты Tasks 3, 5).
5. **Окно без рамки в настоящих точках** на втором мониторе с другим масштабом и на Windows до 10 1607 (откат на
   точки игры), а также если игра потом сама двигает окно. Только Windows: пункт 3 чек-листа.

## Как записаны правки

Код шагов взят из коммитов эталонной реализации, план проверен на свежей копии `main` (каждый шаг применён, вывод
каждой команды сверен с Expected, итоговое дерево совпало с эталоном). Формы — как в прежних планах: «`путь`:» — файл
целиком; «В `путь` заменить: … на: …» — фрагмент встречается в файле ровно один раз в момент замены; замены одного
шага идут сверху вниз, в порядке записи.

Проверка задачи — `verify4.sh` (scratch): сборка Windows без предупреждений, `check_dll`, `check_exe`, сборка и тесты
ядра.

---

### Task 1: Разделы «Звук», «Игра», «Чат и вход» и тексты 0.6.2 (§6.1; 0.6.1, мелочь 7)

Список разделов получает «Звук», «Игра» и «Чат и вход». «Чат и вход» — отдельный раздел: со шёпотом, логином и
макросами «Игра» не помещается в 488 точек страницы. Все тексты 0.6.2 добавляются сразу, на трёх языках. Подписи
флажков короткие: два столбца по 324 точки. `ScreenWaits` становится `ApplyWaits`: при запущенной игре ждёт не только
экран, но и всё, что игра пишет сама (0.6.1, мелочь 7).

**Files:**
- Modify: `src/app/model.cpp`, `src/core/settings/schema.cpp`, `src/core/settings/schema.hpp`, `src/core/settings/strings.cpp`, `src/core/settings/strings.hpp`, `tests/test_schema.cpp`

**Interfaces:**
- Consumes: —
- Produces: `enum class Page : uint8_t { Render, Screen, Quality, Frames, Sound, Game, Chat, Diagnostics, Service }`,
  `kPageCount = 9`; тексты `Text::PageSound` … `Text::SoundSetupDiffers` (в конце перечня, перед `Count`);
  `Text::ApplyWaits` вместо `Text::ScreenWaits`.

- [ ] **Step 1: Падающий тест**

В `tests/test_schema.cpp` заменить:
```cpp
        CHECK(appliesNow(k) == (k != "renderer.backend" && k != "frames.allow_tearing"));
    }
}
```
на:
```cpp
        CHECK(appliesNow(k) == (k != "renderer.backend" && k != "frames.allow_tearing"));
    }
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

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_schema.cpp:64:26: error: ‘Sound’ is not a member of ‘uagfx::settings::Page’`.

- [ ] **Step 3: Реализация**

В `src/app/model.cpp` заменить:
```cpp
    screenAsSetup_ = false;
    wroteNow_.clear();
    load();
    return waits ? wtr(l, Text::ScreenWaits) : L"";
}

std::wstring Model::writeNow(const IniSetting& s, const std::string& token, Lang l) {
```
на:
```cpp
    screenAsSetup_ = false;
    wroteNow_.clear();
    load();
    return waits ? wtr(l, Text::ApplyWaits) : L"";
}

std::wstring Model::writeNow(const IniSetting& s, const std::string& token, Lang l) {
```

В `src/core/settings/schema.cpp` заменить:
```cpp
        case Page::Screen: return Text::PageScreen;
        case Page::Quality: return Text::PageQuality;
        case Page::Frames: return Text::PageFrames;
        case Page::Diagnostics: return Text::PageDiagnostics;
        case Page::Service: break;
    }
```
на:
```cpp
        case Page::Screen: return Text::PageScreen;
        case Page::Quality: return Text::PageQuality;
        case Page::Frames: return Text::PageFrames;
        case Page::Sound: return Text::PageSound;
        case Page::Game: return Text::PageGame;
        case Page::Chat: return Text::PageChat;
        case Page::Diagnostics: return Text::PageDiagnostics;
        case Page::Service: break;
    }
```

В `src/core/settings/schema.hpp` заменить:
```cpp

namespace uagfx::settings {

// The settings app's pages in the order of its list (stage 2 spec §6.1; Sound, Game, Input and dgVoodoo come with
// 0.6.1 and 0.6.2).
enum class Page : uint8_t { Render, Screen, Quality, Frames, Diagnostics, Service };
constexpr int kPageCount = 6;
Text pageName(Page p);

enum class Control : uint8_t { Check, Slider, Choice, Number };
```
на:
```cpp

namespace uagfx::settings {

// The settings app's pages in the order of its list (stage 2 spec §6.1): Chat holds the whisper, login and chat macro
// rows the Game page has no room for; Input and dgVoodoo come with 0.6.3.
enum class Page : uint8_t { Render, Screen, Quality, Frames, Sound, Game, Chat, Diagnostics, Service };
constexpr int kPageCount = 9;
Text pageName(Page p);

enum class Control : uint8_t { Check, Slider, Choice, Number };
```

В `src/core/settings/strings.cpp` заменить:
```cpp
    {"Cannot write {1}.", "Не вдалося записати {1}.", "Не удалось записать {1}."},
    {"Cannot write {1} to the registry.", "Не вдалося записати {1} до реєстру.", "Не удалось записать {1} в реестр."},
    {"Close the game first.", "Спершу закрий гру.", "Сначала закрой игру."},
    {"Saved, all but the screen: ROExt's settings change only while the game is closed. Close the game and press "
     "Apply again.",
     "Записано все, крім екрана: налаштування ROExt змінюються лише при закритій грі. Закрий гру й натисни "
     "«Застосувати» ще раз.",
     "Записано всё, кроме экрана: настройки ROExt меняются только при закрытой игре. Закрой игру и нажми «Применить» "
     "ещё раз."},
    {"Cannot make the backup ({1}): nothing was written.", "Не вдалося зробити резервну копію ({1}): нічого не записано.",
     "Не удалось сделать резервную копию ({1}): ничего не записано."},
    {"Screen: {1}", "Екран: {1}", "Экран: {1}"},
    {"Cannot start uaRO.exe (error {1}).", "Не вдалося запустити uaRO.exe (помилка {1}).",
     "Не удалось запустить uaRO.exe (ошибка {1})."},
    {"Cannot restore {1}.", "Не вдалося відновити {1}.", "Не удалось восстановить {1}."},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "a text for every Text");

```
на:
```cpp
    {"Cannot write {1}.", "Не вдалося записати {1}.", "Не удалось записать {1}."},
    {"Cannot write {1} to the registry.", "Не вдалося записати {1} до реєстру.", "Не удалось записать {1} в реестр."},
    {"Close the game first.", "Спершу закрий гру.", "Сначала закрой игру."},
    {"Saved, all but what changes only while the game is closed - it is kept: close the game and press Apply again.",
     "Записано все, крім того, що змінюється лише при закритій грі, — воно не загубилося: закрий гру й натисни "
     "«Застосувати» ще раз.",
     "Записано всё, кроме того, что меняется только при закрытой игре, — оно не потерялось: закрой игру и нажми "
     "«Применить» ещё раз."},
    {"Cannot make the backup ({1}): nothing was written.", "Не вдалося зробити резервну копію ({1}): нічого не записано.",
     "Не удалось сделать резервную копию ({1}): ничего не записано."},
    {"Screen: {1}", "Екран: {1}", "Экран: {1}"},
    {"Cannot start uaRO.exe (error {1}).", "Не вдалося запустити uaRO.exe (помилка {1}).",
     "Не удалось запустить uaRO.exe (ошибка {1})."},
    {"Cannot restore {1}.", "Не вдалося відновити {1}.", "Не удалось восстановить {1}."},
    {"Sound", "Звук", "Звук"},
    {"Game", "Гра", "Игра"},
    {"Chat and login", "Чат і вхід", "Чат и вход"},
    {"The game's own quality", "Якість самої гри", "Качество самой игры"},
    {"Sprites", "Спрайти", "Спрайты"},
    {"How sharp the game loads characters and monsters: high is their full size.",
     "Наскільки чітко гра завантажує персонажів і монстрів: висока — повний розмір.",
     "Насколько чётко игра загружает персонажей и монстров: высокое — полный размер."},
    {"Textures", "Текстури", "Текстуры"},
    {"The size of the ground and wall textures: high is full, half and low save video memory.",
     "Розмір текстур землі й стін: висока — повний, половина й низька заощаджують відеопам'ять.",
     "Размер текстур земли и стен: высокое — полный, половина и низкое экономят видеопамять."},
    {"high", "висока", "высокое"},
    {"half", "половина", "половина"},
    {"low", "низька", "низкое"},
    {"Mipmaps", "Міп-рівні", "Мип-уровни"},
    {"Smaller copies of textures for the distance (TRILINEARFILTER). With uaRO Graphics the 0.6.2 checklist tries "
     "them.",
     "Менші копії текстур для далеких місць (TRILINEARFILTER). З uaRO Graphics їх перевіряє чек-лист 0.6.2.",
     "Меньшие копии текстур для дальних мест (TRILINEARFILTER). С uaRO Graphics их проверяет чек-лист 0.6.2."},
    {"Ground shadows and light", "Тіні й світло на землі", "Тени и свет на земле"},
    {"The maps' baked shadows (ISLIGHTMAP); /lightmap switches them for one session only.",
     "Запечені тіні карт (ISLIGHTMAP); /lightmap перемикає їх лише на один сеанс.",
     "Запечённые тени карт (ISLIGHTMAP); /lightmap переключает их только на один сеанс."},
    {"Fog", "Туман", "Туман"},
    {"The maps' fog, as /fog.", "Туман карт, як /fog.", "Туман карт, как /fog."},
    {"The game rewrites the grey settings when it closes: change them while it is closed.",
     "Сірі налаштування гра перезапише при виході: змінюй їх, коли її закрито.",
     "Серые настройки игра перезапишет при выходе: меняй их, когда она закрыта."},
    {"Sound", "Звук", "Звук"},
    {"Off: no sound and no music. 2D: sounds without a direction. 3D: sounds from where they are (Miles 3D).",
     "Вимкнено: ні звуків, ні музики. 2D: звуки без напрямку. 3D: звуки звідти, де вони є (Miles 3D).",
     "Выключен: ни звуков, ни музыки. 2D: звуки без направления. 3D: звуки оттуда, где они есть (Miles 3D)."},
    {"off", "вимкнено", "выключен"},
    {"2D", "2D", "2D"},
    {"3D", "3D", "3D"},
    {"Sample rate", "Частота", "Частота"},
    {"A higher rate sounds better.", "Вища частота звучить краще.", "Выше частота — лучше звук."},
    {"22050 Hz", "22050 Гц", "22050 Гц"},
    {"11025 Hz", "11025 Гц", "11025 Гц"},
    {"8000 Hz", "8000 Гц", "8000 Гц"},
    {"Bits", "Розрядність", "Разрядность"},
    {"16-bit sound is cleaner.", "16-бітний звук чистіший.", "16-битный звук чище."},
    {"16-bit", "16 біт", "16 бит"},
    {"8-bit", "8 біт", "8 бит"},
    {"Sounds at once", "Звуків одночасно", "Звуков одновременно"},
    {"How many effects can play at the same time.", "Скільки ефектів може звучати разом.",
     "Сколько эффектов может звучать одновременно."},
    {"48", "48", "48"},
    {"32", "32", "32"},
    {"16", "16", "16"},
    {"Speakers", "Колонки", "Колонки"},
    {"3D only. The names come from the Miles Sound System 6 headers and are not checked on this client.",
     "Лише для 3D. Назви — із заголовків Miles Sound System 6, на цьому клієнті не перевірені.",
     "Только для 3D. Названия — из заголовков Miles Sound System 6, на этом клиенте не проверены."},
    {"2 speakers", "2 колонки", "2 колонки"},
    {"headphones", "навушники", "наушники"},
    {"surround", "об'ємний", "объёмный"},
    {"4 speakers", "4 колонки", "4 колонки"},
    {"5.1", "5.1", "5.1"},
    {"7.1", "7.1", "7.1"},
    {"Music", "Музика", "Музыка"},
    {"Music volume", "Гучність музики", "Громкость музыки"},
    {"Sound effects", "Звукові ефекти", "Звуковые эффекты"},
    {"Effects volume", "Гучність ефектів", "Громкость эффектов"},
    {"0-127, as /bv and /v in the game.", "0–127, як /bv і /v у грі.", "0–127, как /bv и /v в игре."},
    {"Effects (/effect)", "Ефекти (/effect)", "Эффекты (/effect)"},
    {"\"Miss\" (/miss)", "«Miss» (/miss)", "«Miss» (/miss)"},
    {"Cursor snaps to monsters: skills (/skillsnap)", "Курсор до монстрів: уміння (/skillsnap)",
     "Курсор к монстрам: умения (/skillsnap)"},
    {"Cursor snaps to monsters: attacks (/snap)", "Курсор до монстрів: атака (/snap)",
     "Курсор к монстрам: атака (/snap)"},
    {"Cursor snaps to items (/itemsnap)", "Курсор до предметів (/itemsnap)", "Курсор к предметам (/itemsnap)"},
    {"Camera locked on the character (/camera)", "Камера жорстко за персонажем (/camera)",
     "Камера жёстко за персонажем (/camera)"},
    {"Own homunculus AI (/hoai)", "Свій ШІ гомункула (/hoai)", "Свой ИИ гомункула (/hoai)"},
    {"Own mercenary AI (/merai)", "Свій ШІ найманця (/merai)", "Свой ИИ наёмника (/merai)"},
    {"Skin", "Скін", "Скин"},
    {"How the interface looks: the folders of skin\\ next to uaRO.exe.",
     "Вигляд інтерфейсу: теки skin\\ поруч із uaRO.exe.", "Вид интерфейса: папки skin\\ рядом с uaRO.exe."},
    {"no skin", "без скіна", "без скина"},
    {"{1} (no such folder)", "{1} (немає такої теки)", "{1} (нет такой папки)"},
    {"Tip of the day", "Підказка дня", "Подсказка дня"},
    {"Intro movie", "Заставка", "Заставка"},
    {"Translucent minimap", "Прозора мінікарта", "Прозрачная миникарта"},
    {"Move whole stacks without asking the amount", "Переносити стоси без питання про кількість",
     "Перемещать стопки без вопроса о количестве"},
    {"A window for whispers from strangers", "Вікно шепоту від незнайомих", "Окно шёпота от незнакомых"},
    {"A window for whispers from friends", "Вікно шепоту від друзів", "Окно шёпота от друзей"},
    {"A sound when a whisper comes", "Звук, коли приходить шепіт", "Звук, когда приходит шёпот"},
    {"The client turns the window for strangers on too while the one for friends is on.",
     "Клієнт вмикає й вікно від незнайомих, поки ввімкнене вікно від друзів.",
     "Клиент включает и окно от незнакомых, пока включено окно от друзей."},
    {"Remember the login", "Пам'ятати логін", "Запоминать логин"},
    {"Forget the login", "Забути логін", "Забыть логин"},
    {"Forget the saved login? The game asks for it next time.",
     "Забути збережений логін? Наступного разу гра його спитає.",
     "Забыть сохранённый логин? В следующий раз игра его спросит."},
    {"The login is forgotten.", "Логін забуто.", "Логин забыт."},
    {"Chat macros", "Макроси чату", "Макросы чата"},
    {"Up to 49 letters; empty is the game's own one, shown grey. A /command runs, any other text goes to the chat.",
     "До 49 літер; порожній — власний макрос гри, показаний сірим. /команда виконується, інший текст іде в чат.",
     "До 49 букв; пустой — макрос самой игры, показан серым. /команда выполняется, другой текст уходит в чат."},
    {"The game's settings", "Налаштування гри", "Настройки игры"},
    {"Save to a file...", "Зберегти у файл…", "Сохранить в файл…"},
    {"Load from a file...", "Завантажити з файлу…", "Загрузить из файла…"},
    {"Reset the windows' layout", "Скинути розкладку вікон", "Сбросить раскладку окон"},
    {"The whole game key: hotbars, windows, options, macros. Another PC loads the file too.",
     "Увесь ключ гри: панелі, вікна, опції, макроси. Файл завантажується й на іншому ПК.",
     "Весь ключ игры: панели, окна, опции, макросы. Файл загружается и на другом ПК."},
    {"Load the game's settings from {1}? Everything the game saved (hotbars, windows, options, macros) is replaced; a "
     "copy of the present settings is made first.",
     "Завантажити налаштування гри з {1}? Усе, що зберегла гра (панелі, вікна, опції, макроси), заміниться; спершу "
     "буде копія нинішніх налаштувань.",
     "Загрузить настройки игры из {1}? Всё, что сохранила игра (панели, окна, опции, макросы), заменится; сначала "
     "будет копия нынешних настроек."},
    {"Put every game window back in its place? Each takes it when it opens next time.",
     "Повернути всі вікна гри на свої місця? Кожне стане на місце, коли відкриється наступного разу.",
     "Вернуть все окна игры на свои места? Каждое встанет на место, когда откроется в следующий раз."},
    {"Saved: {1}", "Збережено: {1}", "Сохранено: {1}"},
    {"Loaded. The game takes it at its next start.", "Завантажено. Гра візьме це під час наступного запуску.",
     "Загружено. Игра возьмёт это при следующем запуске."},
    {"Done: the windows take their places when they open.", "Готово: вікна стануть на місця, коли відкриються.",
     "Готово: окна встанут на места, когда откроются."},
    {"{1} holds no settings of the game.", "У {1} немає налаштувань гри.", "В {1} нет настроек игры."},
    {"Game settings (*.reg)", "Налаштування гри (*.reg)", "Настройки игры (*.reg)"},
    {"{1}, at least {2} MB (a 32-bit program sees no more), driver {3}",
     "{1}, щонайменше {2} МБ (32-бітна програма більше не бачить), драйвер {3}",
     "{1}, не меньше {2} МБ (32-битная программа больше не видит), драйвер {3}"},
    {"setup.exe has: {1}. The game uses this app's values.", "У setup.exe: {1}. Гра бере значення цієї утиліти.",
     "В setup.exe: {1}. Игра берёт значения этой утилиты."},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "a text for every Text");

```

В `src/core/settings/strings.hpp` заменить:
```cpp
    ServicePresets, PresetMaximum, PresetBalance, PresetSpeed, PresetPlain, PresetNote, PresetCustom,
    ServiceBackups, BackupRestore, BackupPrune, BackupNote, BackupNone, BackupAsk, ServiceReset, ResetButton,
    ResetAsk, ServiceVersions, VersionsValue, Unknown,
    ErrWriteFile, ErrWriteRegistry, ErrGameRunning, ScreenWaits, ErrBackup, ErrScreen, ErrLaunch, ErrRestore,
    Count
};

```
на:
```cpp
    ServicePresets, PresetMaximum, PresetBalance, PresetSpeed, PresetPlain, PresetNote, PresetCustom,
    ServiceBackups, BackupRestore, BackupPrune, BackupNote, BackupNone, BackupAsk, ServiceReset, ResetButton,
    ResetAsk, ServiceVersions, VersionsValue, Unknown,
    ErrWriteFile, ErrWriteRegistry, ErrGameRunning, ApplyWaits, ErrBackup, ErrScreen, ErrLaunch, ErrRestore,
    PageSound, PageGame, PageChat,
    GameQualityHeading, GameSprites, GameSpritesHint, GameTextures, GameTexturesHint, LevelHigh, LevelHalf, LevelLow,
    GameMipmaps, GameMipmapsHint, GameLightmaps, GameLightmapsHint, GameFog, GameFogHint, GameClosedOnly,
    SoundMode, SoundModeHint, SoundOff, Sound2D, Sound3D, SoundRate, SoundRateHint, Rate22050, Rate11025, Rate8000,
    SoundBits, SoundBitsHint, Bits16, Bits8, SoundVoices, SoundVoicesHint, Voices48, Voices32, Voices16,
    SoundSpeakers, SoundSpeakersHint, Speakers2, SpeakersPhones, SpeakersSurround, Speakers4, Speakers51, Speakers71,
    SoundMusic, SoundMusicVolume, SoundEffects, SoundEffectsVolume, SoundVolumeHint,
    GameEffects, GameMiss, GameSkillSnap, GameAttackSnap, GameItemSnap, GameCamera, GameHomunAi, GameMercAi,
    GameSkin, GameSkinHint, SkinNone, SkinMissing, GameTips, GameIntro, GameMinimap, GameStacks,
    ChatWhisperStrangers, ChatWhisperFriends, ChatWhisperSound, ChatFriendsNote, ChatRememberLogin, ChatForgetLogin,
    ChatForgetAsk, ChatLoginForgotten, ChatMacrosHeading, ChatMacrosHint,
    ServiceGameSettings, GameSave, GameLoad, LayoutReset, GameSettingsNote, GameLoadAsk, LayoutResetAsk, GameSaved,
    GameLoaded, LayoutResetDone, ErrReadFile, FileFilterReg, DiagGpuValueCapped, SoundSetupDiffers,
    Count
};

```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `271 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 199 functions …`, затем `271 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_schema.cpp src/app/model.cpp src/core/settings/schema.cpp src/core/settings/schema.hpp src/core/settings/strings.cpp src/core/settings/strings.hpp
git commit -q -F - <<'EOF'
Утилита: разделы «Звук», «Игра», «Чат и вход» и тексты 0.6.2

Список разделов по §6.1 этапа 2. «Чат и вход» отдельно: шёпот, логин и
макросы не помещаются на страницу «Игры». Все тексты 0.6.2 на трёх языках.
ScreenWaits стал ApplyWaits: при запущенной игре ждёт не только экран
(ревью 0.6.1, мелочь 7).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 2: Значения ключа игры, которые правят страницы (§6.1, registry.md)

`src/core/settings/game_settings.*` — 41 значение: раздел, вид поля, как игра читает значение, её значение по
умолчанию и пишет ли его она сама (§4.1). Правила чтения — из `registry.md`: у спрайтов и текстур 2/1/0, всё прочее —
как 2; `ISLIGHTMAP` и `CHECKSAVE` включены только при ровно 1; музыка — обратный `bgmIsPaused`, заставка — обратный
`BINKMODE`; миникарта непрозрачна только при `0xFFFFFFFF`. Слово страницы — как у ini: «1», «2», «100», текст.
Макросы и имя скина — строки (`Control::Line`); предел макроса — 49 букв, не байт.

**Files:**
- Create: `src/core/settings/game_settings.cpp`, `src/core/settings/game_settings.hpp`, `tests/test_game_settings.cpp`
- Modify: `src/app/page_settings.cpp`, `src/core/settings/schema.hpp`

**Interfaces:**
- Consumes: `effectiveValue`, `RegValue` (`game_registry.hpp`); `Page`, `Text`.
- Produces: `enum class RegKind`, `struct RegSetting`, `regSettings()`, `findRegSetting(key)`, `regKey(s)`,
  `regWordOf(s, registry)`, `validRegWord(s, word)`, `regValueOf(s, word)`, `builtInMacro(index)`;
  `Control::Line` в `schema.hpp`.

- [ ] **Step 1: Падающий тест**

`tests/test_game_settings.cpp`:
```cpp
// The game's key on the settings app's pages (stage 2 spec §6.1, release 0.6.2; docs/research/registry.md).
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/settings/game_settings.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

const RegSetting& setting(const char* key) { return *findRegSetting(key); }

// The common copy holding these "subkey\name" values.
GameRegistry withValues(const std::vector<std::pair<std::string, RegValue>>& values) {
    GameRegistry r;
    r.common.exists = true;
    for (const auto& v : values) {
        const size_t slash = v.first.rfind('\\');
        const std::string subkey = slash == std::string::npos ? "" : v.first.substr(0, slash);
        const std::string name = slash == std::string::npos ? v.first : v.first.substr(slash + 1);
        r.common.keys[subkey][name] = v.second;
    }
    return r;
}

}  // namespace

TEST(game_settings_read_values_by_the_game_s_rules) {
    const GameRegistry r = withValues({{"SPRITEMODE", RegValue::dword(5)},  // 2 and any other value: full size
                                       {"TEXTUREMODE", RegValue::dword(1)},
                                       {"ISLIGHTMAP", RegValue::dword(2)},       // on only when exactly 1
                                       {"DIGITALRATETYPE", RegValue::dword(7)},  // 2 and up: 8000 Hz
                                       {"Option\\bgmIsPaused", RegValue::dword(1)},
                                       {"Option\\streamVolume", RegValue::dword(300)},  // kept raw, played at 127
                                       {"UIRectInfo\\M_MINIMAPARGB", RegValue::dword(0x12345678)},
                                       {"Option\\SKINNAME", RegValue::sz("default")}});
    CHECK(regWordOf(setting("SPRITEMODE"), r) == "2");
    CHECK(regWordOf(setting("TEXTUREMODE"), r) == "1");
    CHECK(regWordOf(setting("ISLIGHTMAP"), r) == "0");
    CHECK(regWordOf(setting("DIGITALRATETYPE"), r) == "2");
    CHECK(regWordOf(setting("Option\\bgmIsPaused"), r) == "0");  // the music is off
    CHECK(regWordOf(setting("Option\\streamVolume"), r) == "127");
    CHECK(regWordOf(setting("UIRectInfo\\M_MINIMAPARGB"), r) == "1");  // anything but opaque is translucent
    CHECK(regWordOf(setting("Option\\SKINNAME"), r) == "default");
}

TEST(game_settings_missing_values_are_the_game_s_own) {
    const GameRegistry r;
    CHECK(regWordOf(setting("SOUNDMODE"), r) == "0");  // no sound at all
    CHECK(regWordOf(setting("NUMSAMPLETYPE"), r) == "2");
    CHECK(regWordOf(setting("FOG"), r) == "1");
    CHECK(regWordOf(setting("Option\\soundVolume"), r) == "100");
    CHECK(regWordOf(setting("Option\\bgmIsPaused"), r) == "1");  // the music is on
    CHECK(regWordOf(setting("Option\\SKINNAME"), r) == "<Basic Skin>");
    CHECK(regWordOf(setting("ShortCutList\\0"), r) == "");
    CHECK(regWordOf(setting("CHECKSAVE"), r) == "0");
    CHECK(std::string(builtInMacro(0)) == "/!");  // the empty macro's text, shown grey
    CHECK(std::string(builtInMacro(9)) == "/...");
}

TEST(game_settings_write_what_the_game_reads) {
    CHECK(regValueOf(setting("Option\\bgmIsPaused"), "1") == RegValue::dword(0));  // music on: not paused
    CHECK(regValueOf(setting("UIRectInfo\\M_MINIMAPARGB"), "0") == RegValue::dword(0xFFFFFFFF));
    CHECK(regValueOf(setting("UIRectInfo\\M_MINIMAPARGB"), "1") == RegValue::dword(0xAAFFFFFF));
    CHECK(regValueOf(setting("SPRITEMODE"), "1") == RegValue::dword(1));
    CHECK(regValueOf(setting("BINKMODE"), "1") == RegValue::dword(0));  // the intro plays
    CHECK(regValueOf(setting("ShortCutList\\3"), "/lv") == RegValue::sz("/lv"));
}

TEST(game_settings_take_only_their_words) {
    CHECK(validRegWord(setting("SPRITEMODE"), "0"));
    CHECK(!validRegWord(setting("SPRITEMODE"), "3"));
    CHECK(!validRegWord(setting("Option\\isEffectOn"), "2"));
    CHECK(validRegWord(setting("Option\\soundVolume"), "127"));
    CHECK(!validRegWord(setting("Option\\soundVolume"), "128"));
    CHECK(validRegWord(setting("ShortCutList\\0"), std::string(49, 'a')));
    CHECK(!validRegWord(setting("ShortCutList\\0"), std::string(50, 'a')));
    std::string cyrillic;
    for (int i = 0; i < 49; ++i) cyrillic += "ж";  // letters, not bytes
    CHECK(validRegWord(setting("ShortCutList\\0"), cyrillic));
    CHECK(!validRegWord(setting("ShortCutList\\0"), "a\nb"));
    CHECK(!validRegWord(setting("Option\\SKINNAME"), ""));
}

TEST(game_settings_the_game_writes_its_own_at_exit) {
    // Spec §4.1: what the game writes itself changes only while it is closed; sound and ISLIGHTMAP any time.
    CHECK(setting("SPRITEMODE").gameWrites);
    CHECK(setting("Option\\isEffectOn").gameWrites);
    CHECK(setting("ShortCutList\\9").gameWrites);
    CHECK(setting("CHECKSAVE").gameWrites);
    CHECK(!setting("ISLIGHTMAP").gameWrites);
    CHECK(!setting("SOUNDMODE").gameWrites);
    CHECK(!setting("SPEAKERTYPE").gameWrites);
}

TEST(game_settings_each_once_on_a_page) {
    std::set<std::string> keys;
    for (const RegSetting& s : regSettings()) {
        CHECK(keys.insert(regKey(s)).second);
        CHECK(findRegSetting(regKey(s)) == &s);
        CHECK(s.page == Page::Quality || s.page == Page::Sound || s.page == Page::Game || s.page == Page::Chat);
        if (s.control == Control::Choice && s.kind == RegKind::Number) CHECK_EQ(s.values.size(), s.choices.size());
        CHECK(validRegWord(s, regWordOf(s, GameRegistry{})));  // the game's own value is one a page can show
    }
    CHECK_EQ(keys.size(), size_t(41));
    CHECK(findRegSetting("Option\\nothing") == nullptr);
}
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_game_settings.cpp:7:10: fatal error: core/settings/game_settings.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

В `src/app/page_settings.cpp` заменить:
```cpp
                case Control::Number:
                    p.number(id, t(s.label), wide(token), t(s.hint));
                    break;
            }
            if (!appliesNow(sectionKey(s))) p.note(t(Text::AfterRestart));
        }
```
на:
```cpp
                case Control::Number:
                    p.number(id, t(s.label), wide(token), t(s.hint));
                    break;
                case Control::Line:  // the game's key only: a key of uaro-graphics.ini is never a line of text
                    break;
            }
            if (!appliesNow(sectionKey(s))) p.note(t(Text::AfterRestart));
        }
```

`src/core/settings/game_settings.cpp`:
```cpp
#include "core/settings/game_settings.hpp"

#include <algorithm>
#include <cstdlib>

namespace uagfx::settings {
namespace {

RegSetting check(const char* subkey, const char* name, Page page, RegKind kind, uint32_t fallback, Text label,
                 Text hint, bool gameWrites) {
    return RegSetting{subkey, name, page, Control::Check, kind, fallback, {}, {}, 0, 1, 1, label, hint, gameWrites, ""};
}

RegSetting choice(const char* subkey, const char* name, Page page, uint32_t fallback, std::vector<uint32_t> values,
                  std::vector<Text> choices, Text label, Text hint, bool gameWrites) {
    return RegSetting{subkey, name, page, Control::Choice, RegKind::Number, fallback, std::move(values),
                      std::move(choices), 0, 0, 0, label, hint, gameWrites, ""};
}

RegSetting slider(const char* subkey, const char* name, Page page, uint32_t fallback, Text label, Text hint) {
    return RegSetting{subkey, name, page, Control::Slider, RegKind::Number, fallback, {}, {}, 0, 127, 1, label, hint,
                      true, ""};
}

RegSetting macro(const char* name) {
    return RegSetting{"ShortCutList", name, Page::Chat, Control::Line, RegKind::Line, 0, {}, {}, 0, 49, 0,
                      Text::ChatMacrosHeading, Text::ChatMacrosHint, true, ""};
}

size_t letters(const std::string& utf8) {  // a UTF-8 letter's later bytes are 10xxxxxx
    return size_t(
        std::count_if(utf8.begin(), utf8.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

}  // namespace

const std::vector<RegSetting>& regSettings() {
    using P = Page;
    using K = RegKind;
    static const std::vector<RegSetting> list{
        // Quality (the game's own): registry.md §1.
        choice("", "SPRITEMODE", P::Quality, 2, {2, 1, 0}, {Text::LevelHigh, Text::LevelHalf, Text::LevelLow},
               Text::GameSprites, Text::GameSpritesHint, true),
        choice("", "TEXTUREMODE", P::Quality, 2, {2, 1, 0}, {Text::LevelHigh, Text::LevelHalf, Text::LevelLow},
               Text::GameTextures, Text::GameTexturesHint, true),
        check("", "TRILINEARFILTER", P::Quality, K::Flag, 0, Text::GameMipmaps, Text::GameMipmapsHint, true),
        check("", "ISLIGHTMAP", P::Quality, K::FlagOne, 1, Text::GameLightmaps, Text::GameLightmapsHint, false),
        check("", "FOG", P::Quality, K::Flag, 1, Text::GameFog, Text::GameFogHint, true),
        // Sound: the device the game never writes; the Option values at exit.
        choice("", "SOUNDMODE", P::Sound, 0, {0, 1, 2}, {Text::SoundOff, Text::Sound2D, Text::Sound3D}, Text::SoundMode,
               Text::SoundModeHint, false),
        choice("", "DIGITALRATETYPE", P::Sound, 0, {0, 1, 2}, {Text::Rate22050, Text::Rate11025, Text::Rate8000},
               Text::SoundRate, Text::SoundRateHint, false),
        choice("", "DIGITALBITSTYPE", P::Sound, 0, {0, 1}, {Text::Bits16, Text::Bits8}, Text::SoundBits,
               Text::SoundBitsHint, false),
        choice("", "NUMSAMPLETYPE", P::Sound, 2, {0, 1, 2}, {Text::Voices48, Text::Voices32, Text::Voices16},
               Text::SoundVoices, Text::SoundVoicesHint, false),
        choice("", "SPEAKERTYPE", P::Sound, 0, {0, 1, 2, 3, 4, 5},
               {Text::Speakers2, Text::SpeakersPhones, Text::SpeakersSurround, Text::Speakers4, Text::Speakers51,
                Text::Speakers71},
               Text::SoundSpeakers, Text::SoundSpeakersHint, false),
        check("Option", "bgmIsPaused", P::Sound, K::NotFlag, 0, Text::SoundMusic, Text::SoundMusic, true),
        slider("Option", "streamVolume", P::Sound, 100, Text::SoundMusicVolume, Text::SoundVolumeHint),
        check("Option", "isSoundOn", P::Sound, K::Flag, 1, Text::SoundEffects, Text::SoundEffects, true),
        slider("Option", "soundVolume", P::Sound, 100, Text::SoundEffectsVolume, Text::SoundVolumeHint),
        // Game: registry.md §4, §1, §5.
        check("Option", "isEffectOn", P::Game, K::Flag, 1, Text::GameEffects, Text::GameEffects, true),
        check("Option", "m_bMakeMissEffect", P::Game, K::Flag, 1, Text::GameMiss, Text::GameMiss, true),
        check("Option", "m_monsterSnapOn_Skill", P::Game, K::Flag, 1, Text::GameSkillSnap, Text::GameSkillSnap, true),
        check("Option", "m_monsterSnapOn_NoSkill", P::Game, K::Flag, 0, Text::GameAttackSnap, Text::GameAttackSnap,
              true),
        check("Option", "m_isItemSnap", P::Game, K::Flag, 0, Text::GameItemSnap, Text::GameItemSnap, true),
        check("Option", "g_isFixedCamera", P::Game, K::Flag, 0, Text::GameCamera, Text::GameCamera, true),
        check("Option", "onHoUserAI", P::Game, K::Flag, 0, Text::GameHomunAi, Text::GameHomunAi, true),
        check("Option", "onMerUserAI", P::Game, K::Flag, 0, Text::GameMercAi, Text::GameMercAi, true),
        check("", "SHOWTIPSATSTARTUP", P::Game, K::Flag, 1, Text::GameTips, Text::GameTips, true),
        check("", "BINKMODE", P::Game, K::NotFlag, 0, Text::GameIntro, Text::GameIntro, true),
        check("UIRectInfo", "M_MINIMAPARGB", P::Game, K::Argb, 0xAAFFFFFF, Text::GameMinimap, Text::GameMinimap, true),
        check("UIRectInfo", "M_ISDRAGALL", P::Game, K::Flag, 1, Text::GameStacks, Text::GameStacks, true),
        RegSetting{"Option", "SKINNAME", P::Game, Control::Choice, K::Line, 0, {}, {}, 0, 127, 0, Text::GameSkin,
                   Text::GameSkinHint, true, "<Basic Skin>"},
        // Chat and login: registry.md §4, §1, §6.
        check("Option", "m_isShowWhisperWnd", P::Chat, K::Flag, 1, Text::ChatWhisperStrangers,
              Text::ChatWhisperStrangers, true),
        check("Option", "m_isShowWhisperWnd_Friend", P::Chat, K::Flag, 1, Text::ChatWhisperFriends,
              Text::ChatFriendsNote, true),
        check("Option", "m_isPlayWhisperOpenSound", P::Chat, K::Flag, 1, Text::ChatWhisperSound,
              Text::ChatWhisperSound, true),
        check("", "CHECKSAVE", P::Chat, K::FlagOne, 0, Text::ChatRememberLogin, Text::ChatRememberLogin, true),
        macro("0"), macro("1"), macro("2"), macro("3"), macro("4"), macro("5"), macro("6"), macro("7"), macro("8"),
        macro("9"),
    };
    return list;
}

std::string regKey(const RegSetting& s) {
    return *s.subkey ? std::string(s.subkey) + "\\" + s.name : std::string(s.name);
}

const RegSetting* findRegSetting(const std::string& key) {
    for (const RegSetting& s : regSettings())
        if (regKey(s) == key) return &s;
    return nullptr;
}

std::string regWordOf(const RegSetting& s, const GameRegistry& r) {
    const auto v = effectiveValue(r, s.subkey, s.name);
    if (s.kind == RegKind::Line) return v && v->isSz() ? v->text : s.fallbackText;
    const uint32_t n = v && v->asDword() ? *v->asDword() : s.fallback;
    switch (s.kind) {
        case RegKind::Flag: return n != 0 ? "1" : "0";
        case RegKind::FlagOne: return n == 1 ? "1" : "0";
        case RegKind::NotFlag: return n == 0 ? "1" : "0";
        case RegKind::Argb: return n != 0xFFFFFFFFu ? "1" : "0";
        case RegKind::Number:
        case RegKind::Line: break;
    }
    if (s.control == Control::Slider)
        return std::to_string(std::clamp(int64_t(n), int64_t(s.min), int64_t(s.max)));
    if (std::find(s.values.begin(), s.values.end(), n) != s.values.end()) return std::to_string(n);
    return std::to_string(*std::max_element(s.values.begin(), s.values.end()));  // the game's rule for the rest
}

bool validRegWord(const RegSetting& s, const std::string& word) {
    if (s.kind == RegKind::Line)
        return letters(word) <= size_t(s.max) && word.find_first_of(std::string("\r\n\0", 3)) == std::string::npos &&
               (s.control != Control::Choice || !word.empty());
    char* end = nullptr;
    const long n = std::strtol(word.c_str(), &end, 10);
    if (word.empty() || *end != '\0') return false;
    if (s.control == Control::Check) return word == "0" || word == "1";
    if (s.control == Control::Slider) return n >= s.min && n <= s.max;
    return n >= 0 && std::find(s.values.begin(), s.values.end(), uint32_t(n)) != s.values.end();
}

RegValue regValueOf(const RegSetting& s, const std::string& word) {
    const bool on = word == "1";
    switch (s.kind) {
        case RegKind::Flag:
        case RegKind::FlagOne: return RegValue::dword(on ? 1 : 0);
        case RegKind::NotFlag: return RegValue::dword(on ? 0 : 1);
        case RegKind::Argb: return RegValue::dword(on ? 0xAAFFFFFFu : 0xFFFFFFFFu);
        case RegKind::Line: return RegValue::sz(word);
        case RegKind::Number: break;
    }
    return RegValue::dword(uint32_t(std::strtoul(word.c_str(), nullptr, 10)));
}

const char* builtInMacro(int index) {
    static const char* const kMacros[] = {"/!", "/?", "/ho", "/lv", "/swt", "/ic", "/an", "/ag", "/$", "/..."};
    return index >= 0 && index < 10 ? kMacros[index] : "";
}

}  // namespace uagfx::settings
```

`src/core/settings/game_settings.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/settings/game_registry.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings {

// How a value of the game's key reads as a page's word (docs/research/registry.md §1, §4, §5, §6).
enum class RegKind : uint8_t {
    Number,   // a REG_DWORD as it is: a choice's stored number, a slider's value
    Flag,     // "1" when not 0
    FlagOne,  // "1" only when exactly 1: the game tests == 1 (ISLIGHTMAP, CHECKSAVE)
    NotFlag,  // "1" when 0: the music on is bgmIsPaused = 0, the intro on is BINKMODE = 0
    Argb,     // the minimap: "1" (translucent, 0xAAFFFFFF) unless 0xFFFFFFFF (opaque)
    Line,     // REG_SZ
};

// One value of the game's key a page edits (stage 2 spec §6.1, release 0.6.2).
struct RegSetting {
    const char* subkey;  // "" = the key itself
    const char* name;
    Page page;
    Control control;
    RegKind kind;
    uint32_t fallback;             // the game's own value when it is missing (registry.md)
    std::vector<uint32_t> values;  // Choice of a Number: the stored numbers in the order of `choices`
    std::vector<Text> choices;
    int min;  // Slider
    int max;  // Slider; Line: the most letters
    int step;
    Text label;
    Text hint;  // the label itself when there is none
    bool gameWrites;           // the game writes it (at exit, or from its windows): only while it is closed (§4.1)
    const char* fallbackText;  // Line: the game's own text when it is missing
};
const std::vector<RegSetting>& regSettings();
const RegSetting* findRegSetting(const std::string& key);  // "SPRITEMODE", "Option\\isEffectOn"
std::string regKey(const RegSetting& s);
// What the game reads now, as a page's word - "1", "2", "100", the text - with the game's own rules for odd values:
// a choice outside its list is its highest, a slider is clamped.
std::string regWordOf(const RegSetting& s, const GameRegistry& r);
bool validRegWord(const RegSetting& s, const std::string& word);
RegValue regValueOf(const RegSetting& s, const std::string& word);  // what a valid word is written as
// The game's own chat macro of Alt+1 ... Alt+0 (index 0 ... 9), used while its value is empty (registry.md §6).
const char* builtInMacro(int index);

}  // namespace uagfx::settings
```

В `src/core/settings/schema.hpp` заменить:
```cpp
constexpr int kPageCount = 9;
Text pageName(Page p);

enum class Control : uint8_t { Check, Slider, Choice, Number };

// One key of uaro-graphics.ini the pages edit one to one. The Screen page writes [display] mode, monitor,
// window_width and window_height from its own plan (spec §5.3).
```
на:
```cpp
constexpr int kPageCount = 9;
Text pageName(Page p);

enum class Control : uint8_t { Check, Slider, Choice, Number, Line };  // Line: a line of text

// One key of uaro-graphics.ini the pages edit one to one. The Screen page writes [display] mode, monitor,
// window_width and window_height from its own plan (spec §5.3).
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `277 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 199 functions …`, затем `277 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_game_settings.cpp src/app/page_settings.cpp src/core/settings/game_settings.cpp src/core/settings/game_settings.hpp src/core/settings/schema.hpp
git commit -q -F - <<'EOF'
Ядро: значения ключа игры, которые правят страницы

41 значение: раздел, вид поля, правила чтения игры по registry.md, её
значение по умолчанию и пишет ли его игра сама (§4.1). Слово страницы ↔
значение реестра; макросы — до 49 букв.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 3: Действующая копия, сброс раскладки, `.reg` с любого пути, пресеты с качеством игры (§8, §4.2)

`effectiveCopy` — ключ игры, как его читает игра (общая копия под личной): его сохраняет «Сохранить в файл».
`windowLayoutReset` — `.X = -1` у каждого окна `UIRectInfo` и `M_CHATWNDY = -1` (§8); без раскладки — ничего.
`fromRegFileAny` берёт файл с любым из четырёх путей ключа: личная копия и общая, с WOW6432Node и без. Это файл с
другого ПК или выгрузка `regedit`. Пресеты задают и качество игры (§8): «Максимум» и «Баланс» — спрайты и текстуры 2,
«Скорость» — текстуры 1; мип-уровни — никогда. `presetOf` сверяет эти слова, если их дали.

**Files:**
- Modify: `src/core/settings/game_registry.cpp`, `src/core/settings/game_registry.hpp`, `src/core/settings/presets.cpp`, `src/core/settings/presets.hpp`, `src/core/settings/reg_file.cpp`, `src/core/settings/reg_file.hpp`, `tests/test_game_registry.cpp`, `tests/test_reg_file.cpp`, `tests/test_service.cpp`

**Interfaces:**
- Consumes: `RegCopy`, `GameRegistry`, `RegWrite`, `planWrite` (`game_registry.hpp`); `toRegFile`, `fromRegFile`.
- Produces: `RegCopy effectiveCopy(const GameRegistry&)`, `std::vector<RegWrite> windowLayoutReset(const GameRegistry&)`,
  `const std::vector<std::string>& gameKeyRoots()`, `bool fromRegFileAny(const std::string& text, const
  std::vector<std::string>& roots, RegCopy& out)`, `presetRegWords(Preset)`, `std::optional<Preset> presetOf(const
  Config&, const std::map<std::string, std::string>& gameWords = {})`.

- [ ] **Step 1: Падающие тесты**

В `tests/test_game_registry.cpp` заменить:
```cpp
    CHECK(d.size() == 2 && d[1].name == "PROVIDERNAME" && d[1].personal == "Miles Fast 2D Positional Audio" &&
          d[1].common == "No Provider");
}
```
на:
```cpp
    CHECK(d.size() == 2 && d[1].name == "PROVIDERNAME" && d[1].personal == "Miles Fast 2D Positional Audio" &&
          d[1].common == "No Provider");
}

TEST(game_registry_the_effective_copy_is_what_the_game_reads) {
    GameRegistry r;
    r.common.exists = true;
    r.common.keys[""]["WIDTH"] = RegValue::dword(1920);
    r.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    r.personal.exists = true;
    r.personal.keys[""]["WIDTH"] = RegValue::dword(1680);
    r.personal.keys["Option"]["isEffectOn"] = RegValue::dword(0);
    const RegCopy c = effectiveCopy(r);
    CHECK(c.exists);
    CHECK(c.find("", "WIDTH") && *c.find("", "WIDTH")->asDword() == 1680u);  // the personal one wins
    CHECK(c.find("", "SOUNDMODE") && *c.find("", "SOUNDMODE")->asDword() == 1u);
    CHECK(c.find("Option", "isEffectOn") && *c.find("Option", "isEffectOn")->asDword() == 0u);
    CHECK(!effectiveCopy(GameRegistry{}).exists);
}

TEST(game_registry_a_window_layout_reset_puts_every_window_back) {
    // Spec §8: every window's .X becomes -1 - it does not fit, so the game puts the window in its place when it
    // opens - and the chat goes to the bottom left; the rest stays.
    GameRegistry r;
    r.personal.exists = true;
    r.personal.keys["UIRectInfo"]["ITEMWNDINFO.X"] = RegValue::dword(1700);
    r.personal.keys["UIRectInfo"]["ITEMWNDINFO.Y"] = RegValue::dword(50);
    r.personal.keys["UIRectInfo"]["M_CHATWNDY"] = RegValue::dword(900);
    r.common.exists = true;
    r.common.keys["UIRectInfo"]["SKILLLISTWNDINFO.X"] = RegValue::dword(300);
    GameRegistry after = r;
    applyWrites(after.personal, windowLayoutReset(r));
    CHECK(effectiveValue(after, "UIRectInfo", "ITEMWNDINFO.X") == RegValue::dword(0xFFFFFFFFu));
    CHECK(effectiveValue(after, "UIRectInfo", "SKILLLISTWNDINFO.X") == RegValue::dword(0xFFFFFFFFu));
    CHECK(effectiveValue(after, "UIRectInfo", "M_CHATWNDY") == RegValue::dword(0xFFFFFFFFu));
    CHECK(effectiveValue(after, "UIRectInfo", "ITEMWNDINFO.Y") == RegValue::dword(50));
    CHECK(windowLayoutReset(GameRegistry{}).empty());  // no layout yet: the game makes its own
}
```

В `tests/test_reg_file.cpp` заменить:
```cpp
    empty.exists = true;
    CHECK(toRegFile(empty, kRoot) == "Windows Registry Editor Version 5.00\r\n\r\n[" + std::string(kRoot) + "]\r\n\r\n");
}
```
на:
```cpp
    empty.exists = true;
    CHECK(toRegFile(empty, kRoot) == "Windows Registry Editor Version 5.00\r\n\r\n[" + std::string(kRoot) + "]\r\n\r\n");
}

TEST(reg_file_a_settings_file_reads_under_any_root_of_the_game_key) {
    RegCopy c;
    c.exists = true;
    c.keys[""]["WIDTH"] = RegValue::dword(1920);
    c.keys["Option"]["isEffectOn"] = RegValue::dword(0);
    CHECK_EQ(gameKeyRoots().size(), size_t(4));
    for (const std::string& root : gameKeyRoots()) {  // a file from another PC, or exported with regedit from HKLM
        RegCopy back;
        CHECK(fromRegFileAny(toRegFile(c, root), gameKeyRoots(), back));
        CHECK(back.exists && back.find("Option", "isEffectOn") && *back.find("Option", "isEffectOn")->asDword() == 0u);
    }
    RegCopy none;
    CHECK(!fromRegFileAny(toRegFile(c, "HKEY_CURRENT_USER\\Software\\Other"), gameKeyRoots(), none));
    CHECK(!fromRegFileAny("not a .reg file", gameKeyRoots(), none));
}
```

В `tests/test_service.cpp` заменить:
```cpp
    CHECK(off.crisp == "off (the game is not at 0x400000)");
    CHECK(off.crispScale.empty());  // the last start's own lines only
}
```
на:
```cpp
    CHECK(off.crisp == "off (the game is not at 0x400000)");
    CHECK(off.crispScale.empty());  // the last start's own lines only
}

TEST(presets_set_the_game_s_own_quality) {
    // Spec §8 from 0.6.2: Maximum and Balance - sprites and textures high; Speed - textures half; mipmaps never.
    using Words = std::vector<std::pair<std::string, std::string>>;
    CHECK(presetRegWords(Preset::Maximum) == (Words{{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}}));
    CHECK(presetRegWords(Preset::Balance) == (Words{{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}}));
    CHECK(presetRegWords(Preset::Speed) == (Words{{"TEXTUREMODE", "1"}}));
    CHECK(presetRegWords(Preset::Plain).empty());
    const Config balance = parseConfig(applyIniEdits("", presetEdits(Preset::Balance)));
    CHECK(presetOf(balance, {{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}}) == Preset::Balance);
    CHECK(!presetOf(balance, {{"SPRITEMODE", "2"}, {"TEXTUREMODE", "1"}}).has_value());
    CHECK(presetOf(balance) == Preset::Balance);  // without the game's words: uaro-graphics.ini alone
}
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает; первой строкой — ошибка того из тестов, что собрался первым: `…/tests/test_game_registry.cpp:118:23: error: ‘effectiveCopy’ was not declared in this scope`, `…/tests/test_reg_file.cpp:83:14: error: ‘gameKeyRoots’ was not declared in this scope`, `…/tests/test_service.cpp:91:11: error: ‘presetRegWords’ was not declared in this scope`.

- [ ] **Step 3: Реализация**

В `src/core/settings/game_registry.cpp` заменить:
```cpp
    }
}

std::vector<uint8_t> halDeviceGuid() {
    // {84E63DE0-46AA-11CF-816F-0000C020156E}: IID_IDirect3DHALDevice, the GUID's bytes in memory order.
    return {0xE0, 0x3D, 0xE6, 0x84, 0xAA, 0x46, 0xCF, 0x11, 0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E};
```
на:
```cpp
    }
}

RegCopy effectiveCopy(const GameRegistry& r) {
    RegCopy out = r.common;
    out.exists = r.personal.exists || r.common.exists;
    for (const auto& key : r.personal.keys)
        for (const auto& v : key.second) out.keys[key.first][v.first] = v.second;
    return out;
}

std::vector<RegWrite> windowLayoutReset(const GameRegistry& r) {
    std::vector<RegWrite> out;
    const RegCopy all = effectiveCopy(r);
    const auto layout = all.keys.find("UIRectInfo");
    if (layout == all.keys.end()) return out;
    const auto same = [](const std::string& a, const std::string& b) { return !NameLess{}(a, b) && !NameLess{}(b, a); };
    const RegValue away = RegValue::dword(0xFFFFFFFFu);
    std::vector<std::string> names{"M_CHATWNDY"};  // the chat's Y, there or not
    for (const auto& v : layout->second)
        if (v.first.size() > 2 && same(v.first.substr(v.first.size() - 2), ".X")) names.push_back(v.first);
    for (const std::string& name : names)
        if (const auto w = planWrite(r, "UIRectInfo", name, away)) out.push_back(*w);
    return out;
}

std::vector<uint8_t> halDeviceGuid() {
    // {84E63DE0-46AA-11CF-816F-0000C020156E}: IID_IDirect3DHALDevice, the GUID's bytes in memory order.
    return {0xE0, 0x3D, 0xE6, 0x84, 0xAA, 0x46, 0xCF, 0x11, 0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E};
```

В `src/core/settings/game_registry.hpp` заменить:
```cpp

std::optional<RegValue> effectiveValue(const GameRegistry& r, const std::string& subkey, const std::string& name);
bool gameKeyExists(const GameRegistry& r);  // in either copy

// A value of the key itself that both copies hold, differently (stage 2 spec §7): the personal one wins.
struct CopyDifference {
```
на:
```cpp

std::optional<RegValue> effectiveValue(const GameRegistry& r, const std::string& subkey, const std::string& name);
bool gameKeyExists(const GameRegistry& r);  // in either copy
// The game's key as the game reads it: every key and value of either copy, the personal one winning (spec §8: the
// settings file holds it).
RegCopy effectiveCopy(const GameRegistry& r);

// A value of the key itself that both copies hold, differently (stage 2 spec §7): the personal one wins.
struct CopyDifference {
```

В `src/core/settings/game_registry.hpp` заменить:
```cpp
// value or no common one to fall back on.
std::optional<RegWrite> planReset(const GameRegistry& r, const std::string& subkey, const std::string& name);
void applyWrites(RegCopy& personal, const std::vector<RegWrite>& writes);

// The IID of the Direct3D HAL device, as setup.exe stores it in GUIDDEVICE.
std::vector<uint8_t> halDeviceGuid();
```
на:
```cpp
// value or no common one to fall back on.
std::optional<RegWrite> planReset(const GameRegistry& r, const std::string& subkey, const std::string& name);
void applyWrites(RegCopy& personal, const std::vector<RegWrite>& writes);
// "Reset the windows' layout" (spec §8): every window's .X in UIRectInfo becomes -1 - a window that does not fit is put
// in its place when it opens (registry.md §5) - and the chat goes to the bottom left (M_CHATWNDY = -1). Nothing when
// there is no layout yet: the game makes its own.
std::vector<RegWrite> windowLayoutReset(const GameRegistry& r);

// The IID of the Direct3D HAL device, as setup.exe stores it in GUIDDEVICE.
std::vector<uint8_t> halDeviceGuid();
```

В `src/core/settings/presets.cpp` заменить:
```cpp
            {"display", "crisp_text", v.crispText ? "1" : "0"}};
}

std::optional<Preset> presetOf(const Config& c) {
    for (int i = 0; i < kPresetCount; ++i) {
        const Values v = valuesOf(Preset(i));
        if (c.highResolution == v.highResolution && c.renderScale == v.renderScale && c.sharpen == v.sharpen &&
            c.uiFilter == UiFilter::Sharp && c.crispText == v.crispText)
            return Preset(i);
    }
    return std::nullopt;
}
```
на:
```cpp
            {"display", "crisp_text", v.crispText ? "1" : "0"}};
}

std::vector<std::pair<std::string, std::string>> presetRegWords(Preset p) {
    switch (p) {
        case Preset::Maximum:
        case Preset::Balance: return {{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}};
        case Preset::Speed: return {{"TEXTUREMODE", "1"}};
        case Preset::Plain: break;
    }
    return {};
}

std::optional<Preset> presetOf(const Config& c, const std::map<std::string, std::string>& gameWords) {
    for (int i = 0; i < kPresetCount; ++i) {
        const Values v = valuesOf(Preset(i));
        bool same = c.highResolution == v.highResolution && c.renderScale == v.renderScale && c.sharpen == v.sharpen &&
                    c.uiFilter == UiFilter::Sharp && c.crispText == v.crispText;
        for (const auto& w : presetRegWords(Preset(i))) {
            const auto it = gameWords.find(w.first);
            same = same && (it == gameWords.end() || it->second == w.second);
        }
        if (same) return Preset(i);
    }
    return std::nullopt;
}
```

В `src/core/settings/presets.hpp` заменить:
```cpp
#pragma once
#include <optional>
#include <vector>

#include "core/config.hpp"
```
на:
```cpp
#pragma once
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"
```

В `src/core/settings/presets.hpp` заменить:
```cpp
// The uaro-graphics.ini edits of a preset (stage 2 spec §8): high_resolution, render_scale, sharpen, ui_filter,
// crisp_text.
std::vector<IniEdit> presetEdits(Preset p);
// The preset whose values these settings hold, if any.
std::optional<Preset> presetOf(const Config& c);

}  // namespace uagfx::settings
```
на:
```cpp
// The uaro-graphics.ini edits of a preset (stage 2 spec §8): high_resolution, render_scale, sharpen, ui_filter,
// crisp_text.
std::vector<IniEdit> presetEdits(Preset p);
// The game's own quality a preset sets from 0.6.2 (spec §8), as the words of the game's key settings (game_settings):
// Maximum and Balance - sprites and textures high; Speed - textures half; mipmaps never.
std::vector<std::pair<std::string, std::string>> presetRegWords(Preset p);
// The preset whose values these settings hold, if any; `gameWords` (setting -> word) is checked where it has the words
// a preset sets, and an empty one leaves the game's quality out.
std::optional<Preset> presetOf(const Config& c, const std::map<std::string, std::string>& gameWords = {});

}  // namespace uagfx::settings
```

В `src/core/settings/reg_file.cpp` заменить:
```cpp
    return true;
}

}  // namespace uagfx::settings
```
на:
```cpp
    return true;
}

const std::vector<std::string>& gameKeyRoots() {
    static const std::vector<std::string> roots{
        "HKEY_CURRENT_USER\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok",
        "HKEY_CURRENT_USER\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Gravity Soft\\Ragnarok",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Gravity Soft\\Ragnarok"};
    return roots;
}

bool fromRegFileAny(const std::string& text, const std::vector<std::string>& roots, RegCopy& out) {
    for (const std::string& root : roots) {
        RegCopy c;
        if (fromRegFile(text, root, c) && c.exists) {
            out = c;
            return true;
        }
    }
    return false;
}

}  // namespace uagfx::settings
```

В `src/core/settings/reg_file.hpp` заменить:
```cpp
#pragma once
#include <string>

#include "core/settings/game_registry.hpp"

```
на:
```cpp
#pragma once
#include <string>
#include <vector>

#include "core/settings/game_registry.hpp"

```

В `src/core/settings/reg_file.hpp` заменить:
```cpp
// Reads what toRegFile writes, and regedit's own files: the keys under `root` (others are skipped), "name"=value lines
// with "text", dword:, hex: and hex(N): values and '\' line continuations. false when it is not a .reg file.
bool fromRegFile(const std::string& text, const std::string& root, RegCopy& out);

}  // namespace uagfx::settings
```
на:
```cpp
// Reads what toRegFile writes, and regedit's own files: the keys under `root` (others are skipped), "name"=value lines
// with "text", dword:, hex: and hex(N): values and '\' line continuations. false when it is not a .reg file.
bool fromRegFile(const std::string& text, const std::string& root, RegCopy& out);
// The roots a file of the game's settings may have (spec §8): the personal copy and the common one, with and without
// WOW6432Node - a file from another PC, or exported with regedit.
const std::vector<std::string>& gameKeyRoots();
// The keys of a settings file under the first of `roots` it holds them for; false when it holds none.
bool fromRegFileAny(const std::string& text, const std::vector<std::string>& roots, RegCopy& out);

}  // namespace uagfx::settings
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `281 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 199 functions …`, затем `281 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_game_registry.cpp tests/test_reg_file.cpp tests/test_service.cpp src/core/settings/game_registry.cpp src/core/settings/game_registry.hpp src/core/settings/presets.cpp src/core/settings/presets.hpp src/core/settings/reg_file.cpp src/core/settings/reg_file.hpp
git commit -q -F - <<'EOF'
Ядро: действующая копия ключа, сброс раскладки окон, .reg с любого пути, пресеты с качеством игры

effectiveCopy — ключ, как его читает игра, для файла настроек.
windowLayoutReset — .X = -1 у окон UIRectInfo и M_CHATWNDY = -1 (§8).
fromRegFileAny — файл с любого из четырёх путей ключа. Пресеты задают
спрайты и текстуры игры; presetOf их сверяет.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 4: Модель: слова реестра на страницах (§4.1, §4.2)

Слово страницы ждёт «Применить», как ini; слово, равное действующему, — не изменение. «Применить» пишет каждое
значение отдельно (§4.5). Значения, которые игра пишет сама, при запущенной игре остаются в ожидании, остальное
записывается; ответ — `ApplyWaits` (§4.1). Пресет ставит и слова реестра, «Отменить» и «Восстановить» их снимают.
Скины — папки `skin\` рядом с `uaRO.exe`, заставка — `openning.bik`. «Забыть логин» — пустой `ID`, только при
закрытой игре, после копии.

**Files:**
- Modify: `src/app/model.cpp`, `src/app/model.hpp`, `tests/test_model.cpp`

**Interfaces:**
- Consumes: Task 2 (`RegSetting`, `regWordOf`, `validRegWord`, `regValueOf`, `findRegSetting`, `regKey`); Task 3
  (`presetRegWords`).
- Produces: `std::string Model::regWord(const RegSetting&) const`, `void Model::setRegWord(const RegSetting&, const
  std::string&)`, `bool Model::regOpen(const RegSetting&) const`, `std::vector<std::wstring> Model::skins() const`,
  `bool Model::hasIntro() const`, `std::wstring Model::forgetLogin(Lang)`.

- [ ] **Step 1: Падающие тесты**

В `tests/test_model.cpp` заменить:
```cpp
// The settings app's model (stage 2 spec §4) over the stand-ins of tests/fake_windows.cpp.
#include "app/model.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/reg_file.hpp"
#include "fake_windows.hpp"
```
на:
```cpp
// The settings app's model (stage 2 spec §4) over the stand-ins of tests/fake_windows.cpp.
#include "app/model.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/reg_file.hpp"
#include "fake_windows.hpp"
```

В `tests/test_model.cpp` заменить:
```cpp
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[roext], "roext.windowwidth"), std::string("1400"));
}
```
на:
```cpp
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[roext], "roext.windowwidth"), std::string("1400"));
}

TEST(model_game_settings_are_written_while_the_game_is_closed) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics.ini")] = "[renderer]\r\nbackend = uaro\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.setRegWord(*findRegSetting("SPRITEMODE"), "1");
    m.setRegWord(*findRegSetting("Option\\isEffectOn"), "0");
    m.setRegWord(*findRegSetting("ShortCutList\\0"), "/lv");
    CHECK_EQ(m.pendingCount(), size_t(3));
    CHECK(m.regWord(*findRegSetting("SPRITEMODE")) == "1");
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(effectiveDword("SPRITEMODE"), 1);
    const auto effect = effectiveValue(w.registry, "Option", "isEffectOn");
    CHECK(effect && effect->asDword() && *effect->asDword() == 0u);
    const auto macro = effectiveValue(w.registry, "ShortCutList", "0");
    CHECK(macro && macro->text == "/lv");
    CHECK_EQ(m.pendingCount(), size_t(0));
}

TEST(model_game_settings_the_game_writes_wait_while_it_runs) {
    // Spec §4.1: ISLIGHTMAP the game never writes - it goes now; SPRITEMODE it writes at exit - it waits.
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics.ini")] = "[renderer]\r\nbackend = uaro\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.setRegWord(*findRegSetting("ISLIGHTMAP"), "0");
    m.setRegWord(*findRegSetting("SPRITEMODE"), "0");
    w.gameRunning = true;
    m.refreshRunning();
    CHECK(!m.regOpen(*findRegSetting("SPRITEMODE")));
    CHECK(m.regOpen(*findRegSetting("ISLIGHTMAP")));
    CHECK(!m.apply(Lang::En).empty());  // it says what waits
    CHECK_EQ(effectiveDword("ISLIGHTMAP"), 0);
    CHECK_EQ(effectiveDword("SPRITEMODE"), -1);  // not there yet
    CHECK_EQ(m.pendingCount(), size_t(1));
    w.gameRunning = false;
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(effectiveDword("SPRITEMODE"), 0);
}

TEST(model_a_game_setting_as_setup_s_leaves_setup_in_charge) {
    // Spec §4.2: a value equal to the common copy's deletes the personal one.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    w.registry.personal.exists = true;
    w.registry.personal.keys[""]["SOUNDMODE"] = RegValue::dword(2);
    app::Model m;
    CHECK(m.regWord(*findRegSetting("SOUNDMODE")) == "2");
    m.setRegWord(*findRegSetting("SOUNDMODE"), "1");
    CHECK(m.apply(Lang::En).empty());
    CHECK(w.registry.personal.find("", "SOUNDMODE") == nullptr);
    CHECK_EQ(effectiveDword("SOUNDMODE"), 1);
}

TEST(model_presets_and_revert_take_the_game_s_quality_too) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.applyPreset(Preset::Speed);
    CHECK(m.regWord(*findRegSetting("TEXTUREMODE")) == "1");
    m.applyPreset(Preset::Maximum);
    CHECK(m.regWord(*findRegSetting("TEXTUREMODE")) == "2");
    CHECK(m.regWord(*findRegSetting("SPRITEMODE")) == "2");
    m.setRegWord(*findRegSetting("FOG"), "0");
    CHECK(m.canRevert());
    CHECK(m.revert(Lang::En).empty());
    CHECK(m.regWord(*findRegSetting("FOG")) == "1");
    CHECK_EQ(m.pendingCount(), size_t(0));
}

TEST(model_skins_are_the_folders_of_skin_and_the_intro_a_file) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"skin"));
    w.folders.insert(fake::path(L"skin\\default"));
    w.folders.insert(fake::path(L"skin\\Blue"));
    app::Model m;
    CHECK(m.skins() == (std::vector<std::wstring>{L"Blue", L"default"}));
    CHECK(!m.hasIntro());
    w.files[fake::path(L"openning.bik")] = "BIK";
    CHECK(m.hasIntro());
}

TEST(model_forget_login_clears_the_saved_one) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.personal.exists = true;
    w.registry.personal.keys[""]["ID"] = RegValue::sz("player");
    w.gameRunning = true;  // the game writes it from its login window: refused while it runs (spec §4.1)
    app::Model m;
    CHECK(!m.forgetLogin(Lang::En).empty());
    CHECK(w.registry.personal.find("", "ID")->text == "player");
    w.gameRunning = false;
    CHECK(m.forgetLogin(Lang::En).empty());
    CHECK(w.registry.personal.find("", "ID")->text == "");
}
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_model.cpp:250:7: error: ‘class uagfx::app::Model’ has no member named ‘setRegWord’`.

- [ ] **Step 3: Реализация**

В `src/app/model.cpp` заменить:
```cpp
    return Size{dword(now_.registry.common, "WIDTH"), dword(now_.registry.common, "HEIGHT")};
}

void Model::applyPreset(Preset p) {
    for (const IniEdit& e : presetEdits(p))
        if (const IniSetting* s = findSetting(e.section + "." + e.key)) setToken(*s, e.value);
}

size_t Model::pendingCount() const {
    const bool screen = screen_ && !(*screen_ == screenNow().choice);
    return ini_.size() + (screen ? 1 : 0) + (screenAsSetup_ ? 1 : 0);
}

std::string Model::currentIni() const {
```
на:
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
}

std::vector<std::wstring> Model::skins() const { return listFolder(paths_.dir + L"\\skin", true); }

bool Model::hasIntro() const { return fileExists(paths_.dir + L"\\openning.bik"); }

void Model::applyPreset(Preset p) {
    for (const IniEdit& e : presetEdits(p))
        if (const IniSetting* s = findSetting(e.section + "." + e.key)) setToken(*s, e.value);
    for (const auto& w : presetRegWords(p))
        if (const RegSetting* s = findRegSetting(w.first)) setRegWord(*s, w.second);
}

size_t Model::pendingCount() const {
    const bool screen = screen_ && !(*screen_ == screenNow().choice);
    return ini_.size() + reg_.size() + (screen ? 1 : 0) + (screenAsSetup_ ? 1 : 0);
}

std::string Model::currentIni() const {
```

В `src/app/model.cpp` заменить:
```cpp
        for (const char* name : {"ISFULLSCREENMODE", "WIDTH", "HEIGHT", "BITPERPIXEL"})
            if (const auto w = planReset(now_.registry, "", name)) reg.push_back(*w);
    }
    if (!ini.empty() || !reg.empty() || !roext.empty()) {
        std::wstring err = backupOnce(l);
        if (!err.empty()) return err;
```
на:
```cpp
        for (const char* name : {"ISFULLSCREENMODE", "WIDTH", "HEIGHT", "BITPERPIXEL"})
            if (const auto w = planReset(now_.registry, "", name)) reg.push_back(*w);
    }
    std::map<std::string, std::string> held;  // what the game writes itself waits for it to close (spec §4.1)
    for (const auto& kv : reg_) {  // after the screen's: the player's word wins over a first run's
        const RegSetting* s = findRegSetting(kv.first);
        if (!s) continue;
        if (!regOpen(*s))
            held.insert(kv);
        else if (const auto w = planWrite(now_.registry, s->subkey, s->name, regValueOf(*s, kv.second)))
            reg.push_back(*w);
    }
    if (!ini.empty() || !reg.empty() || !roext.empty()) {
        std::wstring err = backupOnce(l);
        if (!err.empty()) return err;
```

В `src/app/model.cpp` заменить:
```cpp
    const std::string kept = waits && backendWord != ini_.end() ? backendWord->second : "";
    ini_.clear();
    if (!kept.empty()) ini_["renderer.backend"] = kept;
    if (waits)
        screen_ = chosen;  // the next Apply with the game closed writes the rest of it
    else
```
на:
```cpp
    const std::string kept = waits && backendWord != ini_.end() ? backendWord->second : "";
    ini_.clear();
    if (!kept.empty()) ini_["renderer.backend"] = kept;
    reg_ = held;
    if (waits)
        screen_ = chosen;  // the next Apply with the game closed writes the rest of it
    else
```

В `src/app/model.cpp` заменить:
```cpp
    screenAsSetup_ = false;
    wroteNow_.clear();
    load();
    return waits ? wtr(l, Text::ApplyWaits) : L"";
}

std::wstring Model::writeNow(const IniSetting& s, const std::string& token, Lang l) {
```
на:
```cpp
    screenAsSetup_ = false;
    wroteNow_.clear();
    load();
    return waits || !held.empty() ? wtr(l, Text::ApplyWaits) : L"";
}

std::wstring Model::writeNow(const IniSetting& s, const std::string& token, Lang l) {
```

В `src/app/model.cpp` заменить:
```cpp

std::wstring Model::revert(Lang l) {
    ini_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    if (wroteNow_.empty()) return L"";
```
на:
```cpp

std::wstring Model::revert(Lang l) {
    ini_.clear();
    reg_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    if (wroteNow_.empty()) return L"";
```

В `src/app/model.cpp` заменить:
```cpp
    }
    const std::string failed = replacePersonal(personal);
    ini_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    wroteNow_.clear();
```
на:
```cpp
    }
    const std::string failed = replacePersonal(personal);
    ini_.clear();
    reg_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    wroteNow_.clear();
```

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

std::wstring Model::forgetLogin(Lang l) {
    refreshRunning();
    if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);  // the game writes it from its login window
    const auto w = planWrite(now_.registry, "", "ID", RegValue::sz(""));
    if (!w) return L"";
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = writePersonalValue(*w);
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::launchGame(Lang l) {
    const unsigned long e = launch(paths_.game, paths_.dir);
    refreshRunning();
```

В `src/app/model.hpp` заменить:
```cpp
#include "app/system.hpp"
#include "core/config.hpp"
#include "core/settings/game_registry.hpp"
#include "core/settings/input_module.hpp"
#include "core/settings/log_digest.hpp"
#include "core/settings/presets.hpp"
```
на:
```cpp
#include "app/system.hpp"
#include "core/config.hpp"
#include "core/settings/game_registry.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/input_module.hpp"
#include "core/settings/log_digest.hpp"
#include "core/settings/presets.hpp"
```

В `src/app/model.hpp` заменить:
```cpp
    bool screenAsSetupPending() const { return screenAsSetup_; }
    std::optional<settings::Size> setupScreen(bool& fullscreen) const;  // setup.exe's, when it differs from the game's

    void applyPreset(settings::Preset p);
    size_t pendingCount() const;  // the player's changes not written yet
    // Apply has something to write: the player's changes, or the game's key on a first run (spec §4.3).
```
на:
```cpp
    bool screenAsSetupPending() const { return screenAsSetup_; }
    std::optional<settings::Size> setupScreen(bool& fullscreen) const;  // setup.exe's, when it differs from the game's

    // The game's key on the pages (0.6.2): the player's word, else what the game reads now (game_settings).
    std::string regWord(const settings::RegSetting& s) const;
    void setRegWord(const settings::RegSetting& s, const std::string& word);
    // Whether a page may change it now: what the game writes itself waits for it to close (spec §4.1).
    bool regOpen(const settings::RegSetting& s) const { return !s.gameWrites || !now_.gameRunning; }
    std::vector<std::wstring> skins() const;  // the folders of skin\ next to uaRO.exe (spec §6.1)
    bool hasIntro() const;                     // openning.bik next to uaRO.exe

    void applyPreset(settings::Preset p);
    size_t pendingCount() const;  // the player's changes not written yet
    // Apply has something to write: the player's changes, or the game's key on a first run (spec §4.3).
```

В `src/app/model.hpp` заменить:
```cpp
    std::wstring resetDefaults(settings::Lang l);
    std::wstring removeCompatFlag(const CompatFlag& f, settings::Lang l);
    std::wstring resetRegistryValue(const std::string& name, settings::Lang l);
    std::wstring launchGame(settings::Lang l);
    std::optional<settings::Lang> savedLanguage() const;
    void saveLanguage(settings::Lang l);  // the app's own key: no backup for it
```
на:
```cpp
    std::wstring resetDefaults(settings::Lang l);
    std::wstring removeCompatFlag(const CompatFlag& f, settings::Lang l);
    std::wstring resetRegistryValue(const std::string& name, settings::Lang l);
    std::wstring forgetLogin(settings::Lang l);  // the saved login goes; only while the game is closed (§4.1)
    std::wstring launchGame(settings::Lang l);
    std::optional<settings::Lang> savedLanguage() const;
    void saveLanguage(settings::Lang l);  // the app's own key: no backup for it
```

В `src/app/model.hpp` заменить:
```cpp
    SystemInfo system_;
    Snapshot now_;
    std::map<std::string, std::string> ini_;  // "section.key" -> the player's word
    std::optional<settings::ScreenChoice> screen_;
    bool screenAsSetup_ = false;
    std::map<std::string, std::string> wroteNow_;  // "section.key" -> the word before the first writeNow
```
на:
```cpp
    SystemInfo system_;
    Snapshot now_;
    std::map<std::string, std::string> ini_;  // "section.key" -> the player's word
    std::map<std::string, std::string> reg_;  // regKey -> the player's word
    std::optional<settings::ScreenChoice> screen_;
    bool screenAsSetup_ = false;
    std::map<std::string, std::string> wroteNow_;  // "section.key" -> the word before the first writeNow
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `287 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 199 functions …`, затем `287 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_model.cpp src/app/model.cpp src/app/model.hpp
git commit -q -F - <<'EOF'
Утилита: слова реестра в модели — ждут «Применить», при запущенной игре — её закрытия

Слово страницы ждёт «Применить», как ini. Значения, которые игра пишет сама,
при запущенной игре остаются в ожидании (§4.1), остальное записывается.
Пресет ставит и качество игры. Скины, заставка, «забыть логин».

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 5: Модель: настройки игры в файл и из файла, сброс раскладки окон (§8)

«Сохранить» читает обе копии в момент сохранения: игра пишет панели навыков при смене карты. В файл идёт действующая
копия с путём личной (UTF-16, как у `regedit`). «Загрузить» — только при закрытой игре: сначала копия нынешних
настроек, потом личная копия заменяется файлом. Изменения страниц для старого ключа снимаются. «Сбросить раскладку» —
только при закрытой игре, после копии.

**Files:**
- Modify: `src/app/model.cpp`, `src/app/model.hpp`, `tests/test_model.cpp`

**Interfaces:**
- Consumes: Task 3 (`effectiveCopy`, `windowLayoutReset`, `gameKeyRoots`, `fromRegFileAny`).
- Produces: `std::wstring Model::saveGameSettings(const std::wstring& file, Lang) const`, `std::wstring
  Model::loadGameSettings(const std::wstring& file, Lang)`, `std::wstring Model::resetWindowLayout(Lang)`.

- [ ] **Step 1: Падающие тесты**

В `tests/test_model.cpp` заменить:
```cpp
    CHECK(m.forgetLogin(Lang::En).empty());
    CHECK(w.registry.personal.find("", "ID")->text == "");
}
```
на:
```cpp
    CHECK(m.forgetLogin(Lang::En).empty());
    CHECK(w.registry.personal.find("", "ID")->text == "");
}

TEST(model_game_settings_go_to_a_file_and_back) {
    // Spec §8: the file holds the whole key as the game reads it - both copies, the personal one winning - under the
    // personal copy's path; loading replaces the personal copy, a copy of the present one made first.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    w.registry.personal.exists = true;
    w.registry.personal.keys["Option"]["isEffectOn"] = RegValue::dword(0);
    w.registry.personal.keys["ShortcutItem\\uaRO"]["SITEM000"] = RegValue::sz("Red Potion");
    app::Model m;
    const std::wstring file = fake::path(L"mine.reg");
    CHECK(m.saveGameSettings(file, Lang::En).empty());
    RegCopy saved;
    CHECK(fromRegFile(w.files[file], app::gameKeyPaths().personalRoot, saved));
    CHECK(saved.find("", "SOUNDMODE") && saved.find("Option", "isEffectOn") &&
          saved.find("ShortcutItem\\uaRO", "SITEM000"));
    w.registry.personal = RegCopy{};  // another PC: nothing of the player's there yet
    app::Model other;
    CHECK(other.loadGameSettings(file, Lang::En).empty());
    const RegValue* item = w.registry.personal.find("ShortcutItem\\uaRO", "SITEM000");
    CHECK(item && item->text == "Red Potion");
    CHECK(w.registry.personal.find("Option", "isEffectOn") != nullptr);
    CHECK(!other.now().backups.empty());
}

TEST(model_game_settings_load_and_reset_wait_for_the_game_to_close) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.files[fake::path(L"mine.reg")] = toRegFile(effectiveCopy(w.registry), app::gameKeyPaths().personalRoot);
    w.gameRunning = true;  // the game writes most of it at exit (spec §4.1)
    app::Model m;
    CHECK(!m.loadGameSettings(fake::path(L"mine.reg"), Lang::En).empty());
    CHECK(!m.resetWindowLayout(Lang::En).empty());
    CHECK(!w.registry.personal.exists);
}

TEST(model_a_file_without_the_game_key_is_refused) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"other.reg")] =
        "Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software\\Other]\r\n\"A\"=dword:00000001\r\n";
    app::Model m;
    CHECK(!m.loadGameSettings(fake::path(L"other.reg"), Lang::En).empty());
    CHECK(!m.loadGameSettings(fake::path(L"none.reg"), Lang::En).empty());
    CHECK(!w.registry.personal.exists);
}

TEST(model_a_window_layout_reset_writes_the_personal_copy) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.personal.exists = true;
    w.registry.personal.keys["UIRectInfo"]["ITEMWNDINFO.X"] = RegValue::dword(1700);
    app::Model m;
    CHECK(m.resetWindowLayout(Lang::En).empty());
    const auto x = effectiveValue(w.registry, "UIRectInfo", "ITEMWNDINFO.X");
    CHECK(x && x->asDword() && *x->asDword() == 0xFFFFFFFFu);
    CHECK(!m.now().backups.empty());
}
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_model.cpp:353:13: error: ‘class uagfx::app::Model’ has no member named ‘saveGameSettings’`.

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

std::wstring Model::saveGameSettings(const std::wstring& file, Lang l) const {
    GameRegistry r;  // as it is now: the game wrote its hotbars at its last map change
    r.personal = readPersonal();
    r.common = readCommon();
    const std::string text = utf8ToRegFile(toRegFile(effectiveCopy(r), gameKeyPaths().personalRoot));
    return writeFileAtomic(file, text) ? L"" : wtrf(l, Text::ErrWriteFile, {utf8(file)});
}

std::wstring Model::loadGameSettings(const std::wstring& file, Lang l) {
    refreshRunning();
    if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);  // the game writes most of it at exit
    std::string bytes;
    RegCopy copy;
    if (!readFile(file, bytes) || !fromRegFileAny(regFileToUtf8(bytes), gameKeyRoots(), copy))
        return wtrf(l, Text::ErrReadFile, {utf8(file)});
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = replacePersonal(copy);
    reg_.clear();  // the game's key is another one now: what the pages changed was for the old one
    screen_.reset();
    screenAsSetup_ = false;
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::resetWindowLayout(Lang l) {
    refreshRunning();
    if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);  // the game writes UIRectInfo at exit
    GameRegistry r;
    r.personal = readPersonal();
    r.common = readCommon();
    const std::vector<RegWrite> writes = windowLayoutReset(r);
    if (writes.empty()) return L"";
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = writeValues(writes);
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::launchGame(Lang l) {
    const unsigned long e = launch(paths_.game, paths_.dir);
    refreshRunning();
```

В `src/app/model.hpp` заменить:
```cpp
    std::wstring removeCompatFlag(const CompatFlag& f, settings::Lang l);
    std::wstring resetRegistryValue(const std::string& name, settings::Lang l);
    std::wstring forgetLogin(settings::Lang l);  // the saved login goes; only while the game is closed (§4.1)
    std::wstring launchGame(settings::Lang l);
    std::optional<settings::Lang> savedLanguage() const;
    void saveLanguage(settings::Lang l);  // the app's own key: no backup for it
```
на:
```cpp
    std::wstring removeCompatFlag(const CompatFlag& f, settings::Lang l);
    std::wstring resetRegistryValue(const std::string& name, settings::Lang l);
    std::wstring forgetLogin(settings::Lang l);  // the saved login goes; only while the game is closed (§4.1)
    // Spec §8: the whole game key as the game reads it, to a .reg file under the personal copy's path; from one, it
    // replaces the personal copy (only while the game is closed, a copy of the present one made first).
    std::wstring saveGameSettings(const std::wstring& file, settings::Lang l) const;
    std::wstring loadGameSettings(const std::wstring& file, settings::Lang l);
    std::wstring resetWindowLayout(settings::Lang l);  // spec §8; only while the game is closed
    std::wstring launchGame(settings::Lang l);
    std::optional<settings::Lang> savedLanguage() const;
    void saveLanguage(settings::Lang l);  // the app's own key: no backup for it
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `291 tests, 0 failed checks`.

- [ ] **Step 5: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 199 functions …`, затем `291 tests, 0 failed checks`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_model.cpp src/app/model.cpp src/app/model.hpp
git commit -q -F - <<'EOF'
Утилита: настройки игры в файл и из файла, сброс раскладки окон

Сохранение — действующая копия ключа в момент сохранения, путём личной копии.
Загрузка и сброс раскладки — только при закрытой игре и после резервной копии
(§4.1, §4.6).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 6: Страницы «Качество» (игра), «Звук», «Игра», «Чат и вход» (§6.1, §4.2; 0.6.1, мелочь 5)

«Качество» получает часть «Качество самой игры». «Звук», «Игра» и «Чат и вход» строятся из таблицы Task 2: флажки — по
два в ряд, макросы — по два в ряд, пустой показывает серым встроенный макрос. Серым становится то, что игра пишет сама,
пока она запущена, и внизу записка; «Колонки» серые, если звук не 3D. Пометка setup.exe у звука (§4.2): если личная
копия устройства звука расходится с общей, показывается строка «В setup.exe: …» и кнопка «Как в setup.exe»; слова
ставит модель, записывает «Применить». Буквы Alt есть только у доступных полей (0.6.1, мелочь 5). У ползунка деления и
страница — десятая часть диапазона.

**Files:**
- Modify: `src/app/model.cpp`, `src/app/model.hpp`, `src/app/page_settings.cpp`, `src/app/ui.cpp`, `src/app/ui.hpp`, `tests/test_model.cpp`

**Interfaces:**
- Consumes: Task 2; Task 4 (`regWord`, `setRegWord`, `regOpen`, `skins`, `hasIntro`, `forgetLogin`).
- Produces: `std::vector<std::pair<const RegSetting*, std::string>> Model::regSetupDiffers() const`, `void
  Model::regAsSetup()`; `struct CheckItem`, `struct LineItem`, `Panel::checks`, `Panel::lines`.

- [ ] **Step 1: Падающий тест**

В `tests/test_model.cpp` заменить:
```cpp
    CHECK_EQ(effectiveDword("SOUNDMODE"), 1);
}

TEST(model_presets_and_revert_take_the_game_s_quality_too) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
```
на:
```cpp
    CHECK_EQ(effectiveDword("SOUNDMODE"), 1);
}

TEST(model_sound_differs_from_setup_exe_s_until_taken_as_in_it) {
    // Spec §4.2: where the personal copy's sound differs from setup.exe's (the common copy), the page says so; "as in
    // setup.exe" takes setup.exe's words, and Apply deletes the personal values.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    w.registry.common.keys[""]["NUMSAMPLETYPE"] = RegValue::dword(0);
    w.registry.personal.exists = true;
    w.registry.personal.keys[""]["SOUNDMODE"] = RegValue::dword(2);
    w.registry.personal.keys[""]["NUMSAMPLETYPE"] = RegValue::dword(0);       // the same as setup.exe's
    w.registry.personal.keys[""]["SPEAKERTYPE"] = RegValue::dword(1);         // setup.exe has none to compare
    w.registry.personal.keys["Option"]["soundVolume"] = RegValue::dword(50);  // not setup.exe's
    app::Model m;
    const auto differs = m.regSetupDiffers();
    CHECK_EQ(differs.size(), size_t(1));
    CHECK(!differs.empty() && regKey(*differs[0].first) == "SOUNDMODE" && differs[0].second == "1");
    m.regAsSetup();
    CHECK(m.regWord(*findRegSetting("SOUNDMODE")) == "1");
    CHECK(m.regSetupDiffers().empty());  // the player has taken it
    CHECK(m.apply(Lang::En).empty());
    CHECK(w.registry.personal.find("", "SOUNDMODE") == nullptr);
    CHECK(w.registry.personal.find("", "SPEAKERTYPE") != nullptr);
}

TEST(model_presets_and_revert_take_the_game_s_quality_too) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_model.cpp:313:28: error: ‘class uagfx::app::Model’ has no member named ‘regSetupDiffers’`.

- [ ] **Step 3: Реализация**

В `src/app/model.cpp` заменить:
```cpp
        reg_.erase(regKey(s));
    else
        reg_[regKey(s)] = word;
}

std::vector<std::wstring> Model::skins() const { return listFolder(paths_.dir + L"\\skin", true); }
```
на:
```cpp
        reg_.erase(regKey(s));
    else
        reg_[regKey(s)] = word;
}

std::vector<std::pair<const RegSetting*, std::string>> Model::regSetupDiffers() const {
    GameRegistry setup = now_.registry;
    setup.personal = RegCopy{};
    const std::vector<std::string>& names = setupValues();
    std::vector<std::pair<const RegSetting*, std::string>> out;
    for (const RegSetting& s : regSettings()) {
        if (*s.subkey || std::find(names.begin(), names.end(), s.name) == names.end()) continue;
        if (!now_.registry.personal.find("", s.name) || !now_.registry.common.find("", s.name)) continue;
        const std::string theirs = regWordOf(s, setup);
        if (regWord(s) != theirs) out.push_back({&s, theirs});
    }
    return out;
}

void Model::regAsSetup() {
    for (const auto& d : regSetupDiffers()) setRegWord(*d.first, d.second);
}

std::vector<std::wstring> Model::skins() const { return listFolder(paths_.dir + L"\\skin", true); }
```

В `src/app/model.hpp` заменить:
```cpp
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "app/registry.hpp"
```
на:
```cpp
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/registry.hpp"
```

В `src/app/model.hpp` заменить:
```cpp
    void setRegWord(const settings::RegSetting& s, const std::string& word);
    // Whether a page may change it now: what the game writes itself waits for it to close (spec §4.1).
    bool regOpen(const settings::RegSetting& s) const { return !s.gameWrites || !now_.gameRunning; }
    std::vector<std::wstring> skins() const;  // the folders of skin\ next to uaRO.exe (spec §6.1)
    bool hasIntro() const;                     // openning.bik next to uaRO.exe

```
на:
```cpp
    void setRegWord(const settings::RegSetting& s, const std::string& word);
    // Whether a page may change it now: what the game writes itself waits for it to close (spec §4.1).
    bool regOpen(const settings::RegSetting& s) const { return !s.gameWrites || !now_.gameRunning; }
    // Spec §4.2: the values setup.exe writes (the sound device) where the player's copy differs from setup.exe's own
    // (the common copy), each with setup.exe's word; "as in setup.exe" takes those words, and Apply then deletes the
    // personal values.
    std::vector<std::pair<const settings::RegSetting*, std::string>> regSetupDiffers() const;
    void regAsSetup();
    std::vector<std::wstring> skins() const;  // the folders of skin\ next to uaRO.exe (spec §6.1)
    bool hasIntro() const;                     // openning.bik next to uaRO.exe

```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `292 tests, 0 failed checks`.

- [ ] **Step 5: Панель и страницы (Win32)**

В `src/app/page_settings.cpp` заменить:
```cpp
#include <cstdio>
#include <cstdlib>

```
на:
```cpp
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

```

В `src/app/page_settings.cpp` заменить:
```cpp
constexpr int kUaro = 1001;
constexpr int kDgVoodoo = 1002;
constexpr int kRows = 1100;  // + 4 x the setting's index in iniSettings()

std::string hresult(long hr) {
    char buf[16];
```
на:
```cpp
constexpr int kUaro = 1001;
constexpr int kDgVoodoo = 1002;
constexpr int kRows = 1100;  // + 4 x the setting's index in iniSettings()
constexpr int kForgetLogin = 1490;
constexpr int kAsSetup = 1491;
constexpr int kReg = 1500;  // + 4 x the setting's index in regSettings()

std::string hresult(long hr) {
    char buf[16];
```

В `src/app/page_settings.cpp` заменить:
```cpp
    return buf;
}

// Render, Quality and Frames (stage 2 spec §6.1): uaro-graphics.ini keys one to one.
class SettingsPage : public Page {
public:
    SettingsPage(PageHost& host, settings::Page which) : Page(host), which_(which) {}

    void build(Panel& p) override {
        p.heading(t(pageName(which_)));
        if (which_ == settings::Page::Render)
            buildRender(p);
        else
            buildRows(p);
    }

    void command(int id, int code) override {
```
на:
```cpp
    return buf;
}

bool sameName(const std::string& a, const std::string& b) {  // a folder's name: any case
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

// Render, Quality and Frames (stage 2 spec §6.1): uaro-graphics.ini keys one to one. Quality, Sound, Game and Chat
// (0.6.2): the game's key.
class SettingsPage : public Page {
public:
    SettingsPage(PageHost& host, settings::Page which) : Page(host), which_(which) {}

    void build(Panel& p) override {
        p.heading(t(pageName(which_)));
        switch (which_) {
            case settings::Page::Render:
                buildRender(p);
                return;
            case settings::Page::Quality:
                buildRows(p);
                p.gap();
                p.heading(t(Text::GameQualityHeading));
                regRows(p, [](const RegSetting&) { return true; });
                break;
            case settings::Page::Sound:
                regRows(p, [](const RegSetting& s) { return !*s.subkey; });  // the device, as setup.exe sets it
                buildAsSetup(p);
                p.gap();
                regRows(p, [](const RegSetting& s) { return s.control == Control::Check; });  // music, effects
                regRows(p, [](const RegSetting& s) { return s.control == Control::Slider; });
                break;
            case settings::Page::Game:
                regRows(p, [](const RegSetting&) { return true; });
                break;
            case settings::Page::Chat:
                regRows(p, [](const RegSetting& s) { return *s.subkey && s.control == Control::Check; });  // whispers
                p.note(t(Text::ChatFriendsNote));
                regRows(p, [](const RegSetting& s) { return !*s.subkey; });  // remember the login
                buildForgetLogin(p);
                p.heading(t(Text::ChatMacrosHeading));
                regRows(p, [](const RegSetting& s) { return s.control == Control::Line; });
                p.note(t(Text::ChatMacrosHint));
                break;
            default:
                buildRows(p);
                return;
        }
        p.note(t(Text::AfterRestart));
        if (held_) p.note(t(Text::GameClosedOnly), Tone::Warn);
    }

    void command(int id, int code) override {
```

В `src/app/page_settings.cpp` заменить:
```cpp
        if ((id == kUaro || id == kDgVoodoo) && code == BN_CLICKED) {
            m.setToken(*findSetting("renderer.backend"), id == kUaro ? "uaro" : "dgvoodoo");
            host_.changed();
            return;
        }
        const IniSetting* s = settingOf(id);
```
на:
```cpp
        if ((id == kUaro || id == kDgVoodoo) && code == BN_CLICKED) {
            m.setToken(*findSetting("renderer.backend"), id == kUaro ? "uaro" : "dgvoodoo");
            host_.changed();
            return;
        }
        if (id == kForgetLogin && code == BN_CLICKED) {
            forgetLogin();
            return;
        }
        if (id == kAsSetup && code == BN_CLICKED) {
            m.regAsSetup();
            host_.changed();
            host_.rebuild();  // the lists show setup.exe's now
            return;
        }
        if (const RegSetting* r = regOf(id)) {
            regCommand(*r, GetDlgItem(host_.window(), id), code);
            return;
        }
        const IniSetting* s = settingOf(id);
```

В `src/app/page_settings.cpp` заменить:
```cpp
    }

    void scroll(int id, int code, int pos) override {
        const IniSetting* s = settingOf(id);
        if (!s || s->control != Control::Slider) return;
        Model& m = host_.model();
```
на:
```cpp
    }

    void scroll(int id, int code, int pos) override {
        if (const RegSetting* r = regOf(id)) {
            if (r->control != Control::Slider) return;
            const std::string word = std::to_string(std::clamp(pos, r->min, r->max));
            SetDlgItemTextW(host_.window(), id + 1, wide(word).c_str());
            host_.model().setRegWord(*r, word);
            host_.changed();
            return;
        }
        const IniSetting* s = settingOf(id);
        if (!s || s->control != Control::Slider) return;
        Model& m = host_.model();
```

В `src/app/page_settings.cpp` заменить:
```cpp

private:
    static const IniSetting* settingOf(int id) {
        if (id < kRows) return nullptr;
        const size_t i = size_t(id - kRows) / 4;
        return (id - kRows) % 4 == 0 && i < iniSettings().size() ? &iniSettings()[i] : nullptr;
    }

    void buildRender(Panel& p) {
```
на:
```cpp

private:
    static const IniSetting* settingOf(int id) {
        if (id < kRows || id >= kForgetLogin) return nullptr;
        const size_t i = size_t(id - kRows) / 4;
        return (id - kRows) % 4 == 0 && i < iniSettings().size() ? &iniSettings()[i] : nullptr;
    }

    static const RegSetting* regOf(int id) {
        if (id < kReg) return nullptr;
        const size_t i = size_t(id - kReg) / 4;
        return (id - kReg) % 4 == 0 && i < regSettings().size() ? &regSettings()[i] : nullptr;
    }

    void regCommand(const RegSetting& s, HWND ctl, int code) {
        Model& m = host_.model();
        if (s.control == Control::Check && code == BN_CLICKED) {
            m.setRegWord(s, SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED ? "1" : "0");
        } else if (s.control == Control::Choice && (code == CBN_SELCHANGE || code == CBN_CLOSEUP)) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            const size_t count = s.kind == RegKind::Line ? skinWords_.size() : s.values.size();
            if (sel < 0 || size_t(sel) >= count) return;
            m.setRegWord(s, s.kind == RegKind::Line ? skinWords_[size_t(sel)] : std::to_string(s.values[size_t(sel)]));
            // The speakers are for 3D only: the page again, once the list is closed.
            if (regKey(s) == "SOUNDMODE" && !SendMessageW(ctl, CB_GETDROPPEDSTATE, 0, 0)) host_.rebuild();
        } else if (s.control == Control::Line && code == EN_KILLFOCUS) {
            const std::string text = controlText(ctl);
            if (validRegWord(s, text))
                m.setRegWord(s, text);
            else
                host_.rebuild();  // back to the text it had
        } else {
            return;
        }
        host_.changed();
    }

    // The game's key on this page that `pick` takes, in the order of regSettings(): checks two to a row, the chat
    // macros as one block. What the game writes itself is grey while it runs (spec §4.1).
    void regRows(Panel& p, bool (*pick)(const RegSetting&)) {
        Model& m = host_.model();
        const std::vector<RegSetting>& all = regSettings();
        const bool threeD = m.regWord(*findRegSetting("SOUNDMODE")) == "2";
        std::vector<CheckItem> checks;
        std::vector<LineItem> macros;
        int letters = 0;
        bool macrosOpen = true;
        for (size_t i = 0; i < all.size(); ++i) {
            const RegSetting& s = all[i];
            const std::string key = regKey(s);
            if (s.page != which_ || !pick(s) || (key == "BINKMODE" && !m.hasIntro())) continue;  // no intro to skip
            const int id = kReg + int(i) * 4;
            const std::string word = m.regWord(s);
            const bool open = m.regOpen(s) && (key != "SPEAKERTYPE" || threeD);
            held_ = held_ || !m.regOpen(s);
            const std::wstring hint = s.hint == s.label ? L"" : t(s.hint);
            if (s.control == Control::Check) {
                checks.push_back({id, t(s.label), word == "1", hint, open});
                continue;
            }
            if (s.control == Control::Line) {  // a chat macro: "0" is Alt+1, ..., "9" is Alt+0
                const int n = std::atoi(s.name);
                macros.push_back({id, L"Alt+" + std::to_wstring((n + 1) % 10), wide(word), wide(builtInMacro(n))});
                letters = s.max;
                macrosOpen = open;
                continue;
            }
            if (!checks.empty()) p.checks(checks);
            checks.clear();
            if (s.control == Control::Slider) {
                p.slider(id, t(s.label), s.min, s.max, std::max(s.step, 1), std::atoi(word.c_str()), wide(word), hint,
                         open);
                continue;
            }
            std::vector<std::wstring> items;
            int selected = 0;
            if (s.kind == RegKind::Line) {
                items = skinItems(s, word, selected);
            } else {
                for (size_t c = 0; c < s.choices.size(); ++c) {
                    items.push_back(t(s.choices[c]));
                    if (word == std::to_string(s.values[c])) selected = int(c);
                }
            }
            p.choice(id, t(s.label), items, selected, hint, open);
        }
        if (!checks.empty()) p.checks(checks);
        if (!macros.empty()) p.lines(macros, letters, L"", macrosOpen);
    }

    // "No skin", the folders of skin\, and a name the game has that no folder holds: it shows no skin then.
    std::vector<std::wstring> skinItems(const RegSetting& s, const std::string& word, int& selected) {
        std::vector<std::wstring> items{t(Text::SkinNone)};
        skinWords_ = {s.fallbackText};
        selected = 0;
        for (const std::wstring& folder : host_.model().skins()) {
            items.push_back(folder);
            skinWords_.push_back(utf8(folder));
            if (sameName(skinWords_.back(), word)) selected = int(items.size()) - 1;
        }
        if (selected == 0 && !word.empty() && word != s.fallbackText) {
            items.push_back(tf(Text::SkinMissing, {word}));
            skinWords_.push_back(word);
            selected = int(items.size()) - 1;
        }
        return items;
    }

    // Spec §4.2: where this app's sound differs from setup.exe's, the page says so and offers setup.exe's.
    void buildAsSetup(Panel& p) {
        const auto differs = host_.model().regSetupDiffers();
        if (differs.empty()) return;
        std::string list;
        for (const auto& d : differs) {
            const RegSetting& s = *d.first;
            std::string value = d.second;
            for (size_t c = 0; c < s.choices.size(); ++c)
                if (d.second == std::to_string(s.values[c])) value = utf8(t(s.choices[c]));
            list += (list.empty() ? "" : ", ") + utf8(t(s.label)) + " — " + value;
        }
        p.note(tf(Text::SoundSetupDiffers, {list}), Tone::Warn);
        p.buttons({{kAsSetup, t(Text::ScreenAsSetup)}});
    }

    void buildForgetLogin(Panel& p) {
        const Snapshot& now = host_.model().now();
        const auto login = effectiveValue(now.registry, "", "ID");
        p.buttons({{kForgetLogin, t(Text::ChatForgetLogin)}},
                  login && login->isSz() && !login->text.empty() && !now.gameRunning);
    }

    void forgetLogin() {
        if (!host_.ask(t(Text::ChatForgetAsk))) return;
        const std::wstring err = host_.model().forgetLogin(host_.lang());
        host_.message(err.empty() ? t(Text::ChatLoginForgotten) : err, !err.empty());
        host_.reload();
    }

    void buildRender(Panel& p) {
```

В `src/app/page_settings.cpp` заменить:
```cpp
    }

    settings::Page which_;
};

}  // namespace
```
на:
```cpp
    }

    settings::Page which_;
    bool held_ = false;                   // a row is grey: the game runs and writes it itself
    std::vector<std::string> skinWords_;  // the skin list's words, in its order
};

}  // namespace
```

В `src/app/ui.cpp` заменить:
```cpp
#include "app/ui.hpp"

#include <cstring>
#include <utility>

```
на:
```cpp
#include "app/ui.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

```

В `src/app/ui.cpp` заменить:
```cpp
constexpr int kLabel = 200;  // the label column, at 96 DPI
constexpr int kRow = 26;
constexpr int kGap = 6;

}  // namespace

```
на:
```cpp
constexpr int kLabel = 200;  // the label column, at 96 DPI
constexpr int kRow = 26;
constexpr int kGap = 6;

// A grey control's text: no letter of its own (Alt+one would send the focus elsewhere), its '&' shown as it is.
std::wstring literal(const std::wstring& text) {
    std::wstring out;
    for (wchar_t ch : text) {
        out += ch;
        if (ch == L'&') out += L'&';
    }
    return out;
}

}  // namespace

```

В `src/app/ui.cpp` заменить:
```cpp
}

HWND Panel::check(int id, const std::wstring& text, bool on, const std::wstring& hint, bool enabled) {
    HWND c = make(L"BUTTON", withMnemonic(text, taken_), BS_AUTOCHECKBOX | WS_TABSTOP, area_.left, y_,
                  area_.right - area_.left, at(kRow), id);
    SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(c, enabled);
    tip(c, hint);
```
на:
```cpp
}

HWND Panel::check(int id, const std::wstring& text, bool on, const std::wstring& hint, bool enabled) {
    HWND c = make(L"BUTTON", enabled ? withMnemonic(text, taken_) : literal(text), BS_AUTOCHECKBOX | WS_TABSTOP,
                  area_.left, y_, area_.right - area_.left, at(kRow), id);
    SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(c, enabled);
    tip(c, hint);
```

В `src/app/ui.cpp` заменить:
```cpp
HWND Panel::radio(int id, const std::wstring& text0, const std::wstring& text, bool on, bool first,
                  const std::wstring& hint, bool enabled) {
    if (!text0.empty()) label(text0, at(kRow), false);  // the group's name: the buttons have letters of their own
    HWND c = make(L"BUTTON", withMnemonic(text, taken_), BS_AUTORADIOBUTTON | WS_TABSTOP, area_.left + at(kLabel), y_,
                  area_.right - area_.left - at(kLabel), at(kRow), id, 0, first);
    SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(c, enabled);
    tip(c, hint);
```
на:
```cpp
HWND Panel::radio(int id, const std::wstring& text0, const std::wstring& text, bool on, bool first,
                  const std::wstring& hint, bool enabled) {
    if (!text0.empty()) label(text0, at(kRow), false);  // the group's name: the buttons have letters of their own
    HWND c = make(L"BUTTON", enabled ? withMnemonic(text, taken_) : literal(text), BS_AUTORADIOBUTTON | WS_TABSTOP,
                  area_.left + at(kLabel), y_, area_.right - area_.left - at(kLabel), at(kRow), id, 0, first);
    SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(c, enabled);
    tip(c, hint);
```

В `src/app/ui.cpp` заменить:
```cpp

HWND Panel::choice(int id, const std::wstring& text, const std::vector<std::wstring>& items, int selected,
                   const std::wstring& hint, bool enabled) {
    label(text, at(kRow));
    HWND c = make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_,
                  area_.right - area_.left - at(kLabel), at(300), id);
    for (const std::wstring& item : items) SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
```
на:
```cpp

HWND Panel::choice(int id, const std::wstring& text, const std::vector<std::wstring>& items, int selected,
                   const std::wstring& hint, bool enabled) {
    label(text, at(kRow), enabled);
    HWND c = make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_,
                  area_.right - area_.left - at(kLabel), at(300), id);
    for (const std::wstring& item : items) SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
```

В `src/app/ui.cpp` заменить:
```cpp

HWND Panel::slider(int id, const std::wstring& text, int lo, int hi, int step, int value, const std::wstring& valueText,
                   const std::wstring& hint, bool enabled) {
    label(text, at(kRow + 4));
    const int width = area_.right - area_.left - at(kLabel) - at(70);
    HWND c = make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, area_.left + at(kLabel), y_, width,
                  at(kRow + 4), id);
    SendMessageW(c, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi));
    SendMessageW(c, TBM_SETTICFREQ, WPARAM(step), 0);
    SendMessageW(c, TBM_SETLINESIZE, 0, LPARAM(step));
    SendMessageW(c, TBM_SETPAGESIZE, 0, LPARAM(step * 2));
    SendMessageW(c, TBM_SETPOS, TRUE, LPARAM(value));
    EnableWindow(c, enabled);
    tip(c, hint);
```
на:
```cpp

HWND Panel::slider(int id, const std::wstring& text, int lo, int hi, int step, int value, const std::wstring& valueText,
                   const std::wstring& hint, bool enabled) {
    label(text, at(kRow + 4), enabled);
    const int width = area_.right - area_.left - at(kLabel) - at(70);
    HWND c = make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, area_.left + at(kLabel), y_, width,
                  at(kRow + 4), id);
    SendMessageW(c, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi));
    const int tenth = std::max(step, (hi - lo) / 10);  // ticks and a page: a tenth of the range, at least a step
    SendMessageW(c, TBM_SETTICFREQ, WPARAM(tenth), 0);
    SendMessageW(c, TBM_SETLINESIZE, 0, LPARAM(step));
    SendMessageW(c, TBM_SETPAGESIZE, 0, LPARAM(std::max(step * 2, tenth)));
    SendMessageW(c, TBM_SETPOS, TRUE, LPARAM(value));
    EnableWindow(c, enabled);
    tip(c, hint);
```

В `src/app/ui.cpp` заменить:
```cpp
}

HWND Panel::number(int id, const std::wstring& text, const std::wstring& value, const std::wstring& hint, bool enabled) {
    label(text, at(kRow));
    HWND c = make(L"EDIT", value, ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_, at(90), at(kRow),
                  id, WS_EX_CLIENTEDGE);
    EnableWindow(c, enabled);
```
на:
```cpp
}

HWND Panel::number(int id, const std::wstring& text, const std::wstring& value, const std::wstring& hint, bool enabled) {
    label(text, at(kRow), enabled);
    HWND c = make(L"EDIT", value, ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_, at(90), at(kRow),
                  id, WS_EX_CLIENTEDGE);
    EnableWindow(c, enabled);
```

В `src/app/ui.cpp` заменить:
```cpp
}

HWND Panel::pair(int id, const std::wstring& text, int a, int b, const std::wstring& hint, bool enabled) {
    label(text, at(kRow));
    const int x = area_.left + at(kLabel);
    HWND c = make(L"EDIT", std::to_wstring(a), ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, x, y_, at(80), at(kRow), id,
                  WS_EX_CLIENTEDGE);
```
на:
```cpp
}

HWND Panel::pair(int id, const std::wstring& text, int a, int b, const std::wstring& hint, bool enabled) {
    label(text, at(kRow), enabled);
    const int x = area_.left + at(kLabel);
    HWND c = make(L"EDIT", std::to_wstring(a), ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, x, y_, at(80), at(kRow), id,
                  WS_EX_CLIENTEDGE);
```

В `src/app/ui.cpp` заменить:
```cpp
    tip(d, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

HWND Panel::note(const std::wstring& text, Tone tone) {
```
на:
```cpp
    tip(d, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

void Panel::checks(const std::vector<CheckItem>& items) {
    const int half = (area_.right - area_.left - at(12)) / 2;
    for (size_t i = 0; i < items.size(); ++i) {
        const CheckItem& c = items[i];
        HWND h = make(L"BUTTON", c.enabled ? withMnemonic(c.text, taken_) : literal(c.text),
                      BS_AUTOCHECKBOX | WS_TABSTOP, area_.left + (i % 2 ? half + at(12) : 0), y_, half, at(kRow), c.id);
        SendMessageW(h, BM_SETCHECK, c.on ? BST_CHECKED : BST_UNCHECKED, 0);
        EnableWindow(h, c.enabled);
        tip(h, c.hint);
        if (i % 2 == 1 || i + 1 == items.size()) y_ += at(kRow) + at(kGap);
    }
}

void Panel::lines(const std::vector<LineItem>& items, int maxChars, const std::wstring& hint, bool enabled) {
    const int half = (area_.right - area_.left - at(12)) / 2;
    const int name = at(48);
    for (size_t i = 0; i < items.size(); ++i) {
        const LineItem& l = items[i];
        const int x = area_.left + (i % 2 ? half + at(12) : 0);
        make(L"STATIC", l.label, SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX, x, y_, name, at(kRow), 0);
        HWND e = make(L"EDIT", l.value, ES_AUTOHSCROLL | WS_TABSTOP, x + name, y_, half - name, at(kRow), l.id,
                      WS_EX_CLIENTEDGE);
        SendMessageW(e, EM_LIMITTEXT, WPARAM(maxChars), 0);
        SendMessageW(e, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(l.cue.c_str()));
        EnableWindow(e, enabled);
        tip(e, hint);
        if (i % 2 == 1 || i + 1 == items.size()) y_ += at(kRow) + at(kGap);
    }
}

HWND Panel::note(const std::wstring& text, Tone tone) {
```

В `src/app/ui.cpp` заменить:
```cpp
        SIZE s{};
        GetTextExtentPoint32W(dc, b.second.c_str(), int(b.second.size()), &s);
        const int w = s.cx + at(28);
        HWND c = make(L"BUTTON", withMnemonic(b.second, taken_), BS_PUSHBUTTON | WS_TABSTOP, x, y_, w, at(kRow + 4),
                      b.first);
        EnableWindow(c, enabled);
        if (!first) first = c;
        x += w + at(8);
```
на:
```cpp
        SIZE s{};
        GetTextExtentPoint32W(dc, b.second.c_str(), int(b.second.size()), &s);
        const int w = s.cx + at(28);
        HWND c = make(L"BUTTON", enabled ? withMnemonic(b.second, taken_) : literal(b.second),
                      BS_PUSHBUTTON | WS_TABSTOP, x, y_, w, at(kRow + 4), b.first);
        EnableWindow(c, enabled);
        if (!first) first = c;
        x += w + at(8);
```

В `src/app/ui.hpp` заменить:
```cpp

enum class Tone { Normal, Muted, Good, Warn, Bad };
COLORREF toneColor(Tone t);

// The controls of one page (stage 2 spec §6), laid out from the top in rows: a label on the left, the control on the
// right, notes under it across the page. They are children of the main window, so Tab goes through all of them.
```
на:
```cpp

enum class Tone { Normal, Muted, Good, Warn, Bad };
COLORREF toneColor(Tone t);

struct CheckItem {  // a check box of Panel::checks
    int id;
    std::wstring text;
    bool on;
    std::wstring hint;
    bool enabled;
};

struct LineItem {  // a text field of Panel::lines
    int id;
    std::wstring label;
    std::wstring value;
    std::wstring cue;  // shown grey while it is empty
};

// The controls of one page (stage 2 spec §6), laid out from the top in rows: a label on the left, the control on the
// right, notes under it across the page. They are children of the main window, so Tab goes through all of them.
```

В `src/app/ui.hpp` заменить:
```cpp
                bool enabled = true);
    // Two numbers with "×" between them: ids id and id + 1.
    HWND pair(int id, const std::wstring& label, int a, int b, const std::wstring& hint, bool enabled = true);
    HWND note(const std::wstring& text, Tone tone = Tone::Muted);
    HWND buttons(const std::vector<std::pair<int, std::wstring>>& list, bool enabled = true);
    HWND listBox(int id, int rows);
```
на:
```cpp
                bool enabled = true);
    // Two numbers with "×" between them: ids id and id + 1.
    HWND pair(int id, const std::wstring& label, int a, int b, const std::wstring& hint, bool enabled = true);
    void checks(const std::vector<CheckItem>& items);  // two to a row
    // Text fields two to a row, a short label before each (the chat macros); at most maxChars letters.
    void lines(const std::vector<LineItem>& items, int maxChars, const std::wstring& hint, bool enabled);
    HWND note(const std::wstring& text, Tone tone = Tone::Muted);
    HWND buttons(const std::vector<std::pair<int, std::wstring>>& list, bool enabled = true);
    HWND listBox(int id, int rows);
```

- [ ] **Step 6: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 199 functions …`, затем `292 tests, 0 failed checks`.

- [ ] **Step 7: Коммит**

```bash
git add tests/test_model.cpp src/app/model.cpp src/app/model.hpp src/app/page_settings.cpp src/app/ui.cpp src/app/ui.hpp
git commit -q -F - <<'EOF'
Утилита: страницы «Качество» (игра), «Звук», «Игра», «Чат и вход»

Ряды строятся из таблицы значений ключа игры. Флажки и макросы — по два в ряд.
Пока игра запущена, серым становится то, что она пишет сама. Пометка
setup.exe у звука и «Как в setup.exe» (§4.2). Буквы Alt — только у доступных
полей (ревью 0.6.1, мелочь 5).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 7: «Сервис»: настройки игры, выбор файла, пресет с качеством игры (§8)

«Сервис» разбит на четыре группы, иначе он не помещается: пресеты; резервные копии (список в 3 строки, записка в одну
строку); настройки игры — «Сохранить в файл…», «Загрузить из файла…», «Сбросить раскладку окон»; uaRO Graphics — сброс
и версии (с 0.6.3 там же обновление и удаление). Пока игра запущена, серыми становятся «Восстановить», «Загрузить» и
«Сбросить раскладку», а записка группы объясняет почему. Файл выбирается стандартным окном Windows: comdlg32, в
списке разрешённых импортов. Пресет на странице учитывает качество игры (`Model::preset`). У каждой кнопки ряда своё
«доступна», так что серая не занимает букву Alt.

**Files:**
- Modify: `CMakeLists.txt`, `src/app/main_window.cpp`, `src/app/main_window.hpp`, `src/app/model.cpp`, `src/app/model.hpp`, `src/app/page.hpp`, `src/app/page_screen.cpp`, `src/app/page_service.cpp`, `src/app/page_settings.cpp`, `src/app/ui.cpp`, `src/app/ui.hpp`, `src/core/settings/strings.cpp`, `src/core/settings/strings.hpp`, `tests/test_model.cpp`, `tools/check_exe.py`

**Interfaces:**
- Consumes: Task 3 (`presetOf`), Task 5 (`saveGameSettings`, `loadGameSettings`, `resetWindowLayout`).
- Produces: `std::optional<Preset> Model::preset() const`; `struct ButtonItem { int id; std::wstring text; bool
  enabled = true; }`, `HWND Panel::buttons(const std::vector<ButtonItem>&)`; `virtual std::wstring
  PageHost::pickFile(bool save)`.

- [ ] **Step 1: Падающий тест**

В `tests/test_model.cpp` заменить:
```cpp
    CHECK_EQ(m.pendingCount(), size_t(0));
}

TEST(model_skins_are_the_folders_of_skin_and_the_intro_a_file) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"skin"));
```
на:
```cpp
    CHECK_EQ(m.pendingCount(), size_t(0));
}

TEST(model_the_preset_shown_counts_the_game_s_quality) {
    // Spec §8: from 0.6.2 a preset sets the game's quality too; with it changed by hand the Service page names none.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.applyPreset(Preset::Speed);
    CHECK(m.preset() == Preset::Speed);
    m.setRegWord(*findRegSetting("TEXTUREMODE"), "2");
    CHECK(!m.preset());
    m.applyPreset(Preset::Maximum);
    CHECK(m.preset() == Preset::Maximum);
}

TEST(model_skins_are_the_folders_of_skin_and_the_intro_a_file) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"skin"));
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_model.cpp:346:13: error: ‘class uagfx::app::Model’ has no member named ‘preset’`.

- [ ] **Step 3: Реализация**

В `src/app/model.cpp` заменить:
```cpp
        if (const IniSetting* s = findSetting(e.section + "." + e.key)) setToken(*s, e.value);
    for (const auto& w : presetRegWords(p))
        if (const RegSetting* s = findRegSetting(w.first)) setRegWord(*s, w.second);
}

size_t Model::pendingCount() const {
```
на:
```cpp
        if (const IniSetting* s = findSetting(e.section + "." + e.key)) setToken(*s, e.value);
    for (const auto& w : presetRegWords(p))
        if (const RegSetting* s = findRegSetting(w.first)) setRegWord(*s, w.second);
}

std::optional<Preset> Model::preset() const {
    std::map<std::string, std::string> words;
    for (int i = 0; i < kPresetCount; ++i)
        for (const auto& w : presetRegWords(Preset(i)))
            if (const RegSetting* s = findRegSetting(w.first)) words[w.first] = regWord(*s);
    return presetOf(pendingConfig(), words);
}

size_t Model::pendingCount() const {
```

В `src/app/model.hpp` заменить:
```cpp
    bool hasIntro() const;                     // openning.bik next to uaRO.exe

    void applyPreset(settings::Preset p);
    size_t pendingCount() const;  // the player's changes not written yet
    // Apply has something to write: the player's changes, or the game's key on a first run (spec §4.3).
    bool canApply() const { return pendingCount() > 0 || !settings::gameKeyExists(now_.registry); }
```
на:
```cpp
    bool hasIntro() const;                     // openning.bik next to uaRO.exe

    void applyPreset(settings::Preset p);
    std::optional<settings::Preset> preset() const;  // the one the fields hold now, with the game's quality (spec §8)
    size_t pendingCount() const;  // the player's changes not written yet
    // Apply has something to write: the player's changes, or the game's key on a first run (spec §4.3).
    bool canApply() const { return pendingCount() > 0 || !settings::gameKeyExists(now_.registry); }
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `293 tests, 0 failed checks`.

- [ ] **Step 5: «Сервис», выбор файла, кнопки (Win32)**

В `CMakeLists.txt` заменить:
```cmake
                   src/app/page_service.cpp src/app/app.rc)
    target_include_directories(uaro-graphics PRIVATE src/app dist ${CMAKE_BINARY_DIR}/app)
    target_compile_options(uaro-graphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-graphics PRIVATE uagfx_app)
    target_link_options(uaro-graphics PRIVATE -static -static-libgcc -static-libstdc++ -s -municode)
else()
    enable_testing()
```
на:
```cmake
                   src/app/page_service.cpp src/app/app.rc)
    target_include_directories(uaro-graphics PRIVATE src/app dist ${CMAKE_BINARY_DIR}/app)
    target_compile_options(uaro-graphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-graphics PRIVATE uagfx_app comdlg32)
    target_link_options(uaro-graphics PRIVATE -static -static-libgcc -static-libstdc++ -s -municode)
else()
    enable_testing()
```

В `src/app/main_window.cpp` заменить:
```cpp
#include "app/main_window.hpp"

#include <commctrl.h>

#include "app/mnemonic.hpp"

```
на:
```cpp
#include "app/main_window.hpp"

#include <commctrl.h>
#include <commdlg.h>

#include "app/mnemonic.hpp"

```

В `src/app/main_window.cpp` заменить:
```cpp

bool MainWindow::ask(const std::wstring& text) { return box(text, MB_YESNO | MB_ICONQUESTION) == IDYES; }

bool MainWindow::confirmClose() { return model_->pendingCount() == 0 || ask(wtr(lang_, Text::AskUnsaved)); }

std::wstring MainWindow::windowLetters() const {
```
на:
```cpp

bool MainWindow::ask(const std::wstring& text) { return box(text, MB_YESNO | MB_ICONQUESTION) == IDYES; }

std::wstring MainWindow::pickFile(bool save) {
    wchar_t file[MAX_PATH] = {};
    if (save) std::wstring(L"uaro-game-settings.reg").copy(file, MAX_PATH - 1);
    std::wstring filter = wtr(lang_, Text::FileFilterReg);
    filter += L'\0';
    filter += L"*.reg";
    filter += L'\0';  // and c_str()'s own: the list ends with two
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.hwndOwner = hwnd_;
    o.lpstrFilter = filter.c_str();
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = L"reg";
    o.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    rebuilds_.boxOpened();  // its own message loop runs like a message box's
    const BOOL ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
    if (rebuilds_.boxClosed()) PostMessageW(hwnd_, kRebuild, 0, 0);
    return ok ? std::wstring(file) : std::wstring();
}

bool MainWindow::confirmClose() { return model_->pendingCount() == 0 || ask(wtr(lang_, Text::AskUnsaved)); }

std::wstring MainWindow::windowLetters() const {
```

В `src/app/main_window.hpp` заменить:
```cpp
    void reload() override;
    void message(const std::wstring& text, bool error) override;
    bool ask(const std::wstring& text) override;

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
```
на:
```cpp
    void reload() override;
    void message(const std::wstring& text, bool error) override;
    bool ask(const std::wstring& text) override;
    std::wstring pickFile(bool save) override;

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
```

В `src/app/page.hpp` заменить:
```cpp
    // A message box. The page stays until it is closed: its code still runs under the box.
    virtual void message(const std::wstring& text, bool error) = 0;
    virtual bool ask(const std::wstring& text) = 0;  // Yes or No
};

// One page of the list on the left (stage 2 spec §6.1). Its controls' ids start at 1000.
```
на:
```cpp
    // A message box. The page stays until it is closed: its code still runs under the box.
    virtual void message(const std::wstring& text, bool error) = 0;
    virtual bool ask(const std::wstring& text) = 0;  // Yes or No
    // A .reg file to save the game's settings to (save) or to load them from; "" when the player cancels.
    virtual std::wstring pickFile(bool save) = 0;
};

// One page of the list on the left (stage 2 spec §6.1). Its controls' ids start at 1000.
```

В `src/app/page_screen.cpp` заменить:
```cpp
            p.note(tf(Text::ScreenSetupDiffers, {num(setup->width), num(setup->height),
                                                 utf8(t(setupFull ? Text::ShowFullscreenWord : Text::ShowWindowWord))}),
                   Tone::Warn);
            p.buttons({{kAsSetup, t(Text::ScreenAsSetup)}}, !m.screenAsSetupPending());
        }
        if (roext) {
            p.note(tf(Text::ScreenRoext, {snap.modules[size_t(snap.roextModule)].info.ini}));
```
на:
```cpp
            p.note(tf(Text::ScreenSetupDiffers, {num(setup->width), num(setup->height),
                                                 utf8(t(setupFull ? Text::ShowFullscreenWord : Text::ShowWindowWord))}),
                   Tone::Warn);
            p.buttons({{kAsSetup, t(Text::ScreenAsSetup), !m.screenAsSetupPending()}});
        }
        if (roext) {
            p.note(tf(Text::ScreenRoext, {snap.modules[size_t(snap.roextModule)].info.ini}));
```

В `src/app/page_service.cpp` заменить:
```cpp
constexpr int kRestore = 1411;
constexpr int kKeepFive = 1412;
constexpr int kReset = 1420;

Text presetName(Preset p) {
    switch (p) {
```
на:
```cpp
constexpr int kRestore = 1411;
constexpr int kKeepFive = 1412;
constexpr int kReset = 1420;
constexpr int kGameSave = 1430;
constexpr int kGameLoad = 1431;
constexpr int kLayoutReset = 1432;

Text presetName(Preset p) {
    switch (p) {
```

В `src/app/page_service.cpp` заменить:
```cpp
    return name.substr(0, 10) + L" " + name.substr(11, 2) + L":" + name.substr(13, 2) + L":" + name.substr(15, 2);
}

// The Service page (stage 2 spec §8, 0.6.0 part): presets, backups, reset, versions.
class ServicePage : public Page {
public:
    explicit ServicePage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        p.heading(t(Text::ServicePresets));
        std::vector<std::pair<int, std::wstring>> presets;
        for (int i = 0; i < kPresetCount; ++i) presets.push_back({kPreset + i, t(presetName(Preset(i)))});
        p.buttons(presets);
        const auto now = presetOf(m.pendingConfig());
        p.note(tf(Text::PresetNote, {utf8(now ? t(presetName(*now)) : t(Text::PresetCustom))}));
        p.gap();
        p.heading(t(Text::ServiceBackups));
        list_ = p.listBox(kBackups, 4);
        for (const std::wstring& b : m.now().backups)
            SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(backupLabel(b).c_str()));
        if (m.now().backups.empty()) p.note(t(Text::BackupNone));
        p.buttons({{kRestore, t(Text::BackupRestore)}, {kKeepFive, t(Text::BackupPrune)}}, !m.now().backups.empty());
        if (m.now().gameRunning && !m.now().backups.empty()) {  // it writes what the game writes too (spec §4.1)
            EnableWindow(GetDlgItem(host_.window(), kRestore), FALSE);
            p.note(t(Text::ErrGameRunning), Tone::Warn);
        }
        p.note(t(Text::BackupNote));
        p.gap();
        p.heading(t(Text::ServiceReset));
        p.buttons({{kReset, t(Text::ResetButton)}});
        p.gap();
        p.heading(t(Text::ServiceVersions));
        p.note(tf(Text::VersionsValue,
                  {kVersion, m.now().installedVersion.empty() ? utf8(t(Text::Unknown)) : m.now().installedVersion}),
               Tone::Normal);
```
на:
```cpp
    return name.substr(0, 10) + L" " + name.substr(11, 2) + L":" + name.substr(13, 2) + L":" + name.substr(15, 2);
}

// The Service page (stage 2 spec §8): presets, backups, the game's settings to a file and back, the window layout
// reset, uaRO Graphics' own reset and versions. While the game runs, what it writes itself waits (spec §4.1): those
// buttons are grey and their group's note says why.
class ServicePage : public Page {
public:
    explicit ServicePage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        const bool running = m.now().gameRunning;
        const bool backups = !m.now().backups.empty();
        p.heading(t(Text::ServicePresets));
        std::vector<ButtonItem> presets;
        for (int i = 0; i < kPresetCount; ++i) presets.push_back({kPreset + i, t(presetName(Preset(i)))});
        p.buttons(presets);
        const auto now = m.preset();
        p.note(tf(Text::PresetNote, {utf8(now ? t(presetName(*now)) : t(Text::PresetCustom))}));
        p.gap();
        p.heading(t(Text::ServiceBackups));
        list_ = p.listBox(kBackups, 3);
        for (const std::wstring& b : m.now().backups)
            SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(backupLabel(b).c_str()));
        if (!backups) SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t(Text::BackupNone).c_str()));
        p.buttons({{kRestore, t(Text::BackupRestore), backups && !running},  // the game writes it too (spec §4.1)
                   {kKeepFive, t(Text::BackupPrune), backups}});
        if (running && backups)
            p.note(t(Text::ErrGameRunning), Tone::Warn);
        else
            p.note(t(Text::BackupNote));
        p.gap();
        p.heading(t(Text::ServiceGameSettings));
        p.buttons({{kGameSave, t(Text::GameSave)},
                   {kGameLoad, t(Text::GameLoad), !running},
                   {kLayoutReset, t(Text::LayoutReset), !running}});
        if (running)
            p.note(t(Text::GameSettingsRunning), Tone::Warn);
        else
            p.note(t(Text::GameSettingsNote));
        p.gap();
        p.heading(t(Text::ServiceUaroGraphics));
        p.buttons({{kReset, t(Text::ResetButton)}});
        p.note(tf(Text::VersionsValue,
                  {kVersion, m.now().installedVersion.empty() ? utf8(t(Text::Unknown)) : m.now().installedVersion}),
               Tone::Normal);
```

В `src/app/page_service.cpp` заменить:
```cpp
            const std::wstring err = m.resetDefaults(l);
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        }
    }

```
на:
```cpp
            const std::wstring err = m.resetDefaults(l);
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        } else if (id == kGameSave) {
            const std::wstring file = host_.pickFile(true);
            if (file.empty()) return;
            const std::wstring err = m.saveGameSettings(file, l);
            host_.message(err.empty() ? tf(Text::GameSaved, {utf8(file)}) : err, !err.empty());
        } else if (id == kGameLoad) {
            const std::wstring file = host_.pickFile(false);
            if (file.empty() || !host_.ask(tf(Text::GameLoadAsk, {utf8(file)}))) return;
            const std::wstring err = m.loadGameSettings(file, l);
            host_.message(err.empty() ? t(Text::GameLoaded) : err, !err.empty());
            host_.reload();
        } else if (id == kLayoutReset) {
            if (!host_.ask(t(Text::LayoutResetAsk))) return;
            const std::wstring err = m.resetWindowLayout(l);
            host_.message(err.empty() ? t(Text::LayoutResetDone) : err, !err.empty());
            host_.reload();
        }
    }

```

В `src/app/page_settings.cpp` заменить:
```cpp
    void buildForgetLogin(Panel& p) {
        const Snapshot& now = host_.model().now();
        const auto login = effectiveValue(now.registry, "", "ID");
        p.buttons({{kForgetLogin, t(Text::ChatForgetLogin)}},
                  login && login->isSz() && !login->text.empty() && !now.gameRunning);
    }

    void forgetLogin() {
```
на:
```cpp
    void buildForgetLogin(Panel& p) {
        const Snapshot& now = host_.model().now();
        const auto login = effectiveValue(now.registry, "", "ID");
        p.buttons({{kForgetLogin, t(Text::ChatForgetLogin), login && login->isSz() && !login->text.empty() &&
                                                                !now.gameRunning}});
    }

    void forgetLogin() {
```

В `src/app/ui.cpp` заменить:
```cpp
    return c;
}

HWND Panel::buttons(const std::vector<std::pair<int, std::wstring>>& list, bool enabled) {
    HWND first = nullptr;
    int x = area_.left;
    HDC dc = GetDC(owner_);
    HGDIOBJ old = SelectObject(dc, font_);
    for (const auto& b : list) {
        SIZE s{};
        GetTextExtentPoint32W(dc, b.second.c_str(), int(b.second.size()), &s);
        const int w = s.cx + at(28);
        HWND c = make(L"BUTTON", enabled ? withMnemonic(b.second, taken_) : literal(b.second),
                      BS_PUSHBUTTON | WS_TABSTOP, x, y_, w, at(kRow + 4), b.first);
        EnableWindow(c, enabled);
        if (!first) first = c;
        x += w + at(8);
    }
```
на:
```cpp
    return c;
}

HWND Panel::buttons(const std::vector<ButtonItem>& list) {
    HWND first = nullptr;
    int x = area_.left;
    HDC dc = GetDC(owner_);
    HGDIOBJ old = SelectObject(dc, font_);
    for (const auto& b : list) {
        SIZE s{};
        GetTextExtentPoint32W(dc, b.text.c_str(), int(b.text.size()), &s);
        const int w = s.cx + at(28);
        HWND c = make(L"BUTTON", b.enabled ? withMnemonic(b.text, taken_) : literal(b.text),
                      BS_PUSHBUTTON | WS_TABSTOP, x, y_, w, at(kRow + 4), b.id);
        EnableWindow(c, b.enabled);
        if (!first) first = c;
        x += w + at(8);
    }
```

В `src/app/ui.hpp` заменить:
```cpp
    bool on;
    std::wstring hint;
    bool enabled;
};

struct LineItem {  // a text field of Panel::lines
```
на:
```cpp
    bool on;
    std::wstring hint;
    bool enabled;
};

struct ButtonItem {  // a button of Panel::buttons
    int id;
    std::wstring text;
    bool enabled = true;
};

struct LineItem {  // a text field of Panel::lines
```

В `src/app/ui.hpp` заменить:
```cpp
    // Text fields two to a row, a short label before each (the chat macros); at most maxChars letters.
    void lines(const std::vector<LineItem>& items, int maxChars, const std::wstring& hint, bool enabled);
    HWND note(const std::wstring& text, Tone tone = Tone::Muted);
    HWND buttons(const std::vector<std::pair<int, std::wstring>>& list, bool enabled = true);
    HWND listBox(int id, int rows);
    HWND listView(int id, const std::vector<std::pair<std::wstring, int>>& columns, int rows);
    void gap(int at96 = 8);
```
на:
```cpp
    // Text fields two to a row, a short label before each (the chat macros); at most maxChars letters.
    void lines(const std::vector<LineItem>& items, int maxChars, const std::wstring& hint, bool enabled);
    HWND note(const std::wstring& text, Tone tone = Tone::Muted);
    HWND buttons(const std::vector<ButtonItem>& list);
    HWND listBox(int id, int rows);
    HWND listView(int id, const std::vector<std::pair<std::wstring, int>>& columns, int rows);
    void gap(int at96 = 8);
```

В `src/core/settings/strings.cpp` заменить:
```cpp
    {"Backups", "Резервні копії", "Резервные копии"},
    {"Restore", "Відновити", "Восстановить"},
    {"Keep the 5 newest", "Залишити 5 останніх", "Оставить 5 последних"},
    {"Before its first write in each run the app copies everything it may change; the 20 newest copies stay.",
     "Перед першим записом у кожному запуску утиліта копіює все, що може змінити; зберігаються 20 останніх копій.",
     "Перед первой записью в каждом запуске утилита копирует всё, что может изменить; хранятся 20 последних копий."},
    {"No backups yet.", "Копій ще немає.", "Копий ещё нет."},
    {"Restore the settings from the copy of {1}? The current ones are copied first.",
     "Повернути налаштування з копії від {1}? Поточні спершу теж скопіюються.",
     "Вернуть настройки из копии от {1}? Нынешние сначала тоже скопируются."},
    {"Reset", "Скидання", "Сброс"},
    {"Reset uaRO Graphics to its defaults", "Скинути uaRO Graphics до типових", "Сбросить uaRO Graphics к умолчаниям"},
    {"Return every uaRO Graphics setting to its default? The renderer and the language stay.",
     "Повернути всі налаштування uaRO Graphics до типових? Рендер і мова залишаться.",
     "Вернуть все настройки uaRO Graphics к умолчаниям? Рендер и язык останутся."},
    {"Versions", "Версії", "Версии"},
    {"Settings app {1} · driver {2}", "Утиліта {1} · драйвер {2}", "Утилита {1} · драйвер {2}"},
    {"unknown", "невідомо", "неизвестно"},
    {"Cannot write {1}.", "Не вдалося записати {1}.", "Не удалось записать {1}."},
```
на:
```cpp
    {"Backups", "Резервні копії", "Резервные копии"},
    {"Restore", "Відновити", "Восстановить"},
    {"Keep the 5 newest", "Залишити 5 останніх", "Оставить 5 последних"},
    {"Before its first write the app copies everything it may change; the 20 newest copies stay.",
     "Перед першим записом утиліта копіює все, що може змінити; зберігаються 20 останніх копій.",
     "Перед первой записью утилита копирует всё, что может изменить; хранятся 20 последних копий."},
    {"No backups yet.", "Копій ще немає.", "Копий ещё нет."},
    {"Restore the settings from the copy of {1}? The current ones are copied first.",
     "Повернути налаштування з копії від {1}? Поточні спершу теж скопіюються.",
     "Вернуть настройки из копии от {1}? Нынешние сначала тоже скопируются."},
    {"uaRO Graphics", "uaRO Graphics", "uaRO Graphics"},
    {"Reset uaRO Graphics to its defaults", "Скинути uaRO Graphics до типових", "Сбросить uaRO Graphics к умолчаниям"},
    {"Return every uaRO Graphics setting to its default? The renderer and the language stay.",
     "Повернути всі налаштування uaRO Graphics до типових? Рендер і мова залишаться.",
     "Вернуть все настройки uaRO Graphics к умолчаниям? Рендер и язык останутся."},
    {"Settings app {1} · driver {2}", "Утиліта {1} · драйвер {2}", "Утилита {1} · драйвер {2}"},
    {"unknown", "невідомо", "неизвестно"},
    {"Cannot write {1}.", "Не вдалося записати {1}.", "Не удалось записать {1}."},
```

В `src/core/settings/strings.cpp` заменить:
```cpp
     "{1}, не меньше {2} МБ (32-битная программа больше не видит), драйвер {3}"},
    {"setup.exe has: {1}. The game uses this app's values.", "У setup.exe: {1}. Гра бере значення цієї утиліти.",
     "В setup.exe: {1}. Игра берёт значения этой утилиты."},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "a text for every Text");

```
на:
```cpp
     "{1}, не меньше {2} МБ (32-битная программа больше не видит), драйвер {3}"},
    {"setup.exe has: {1}. The game uses this app's values.", "У setup.exe: {1}. Гра бере значення цієї утиліти.",
     "В setup.exe: {1}. Игра берёт значения этой утилиты."},
    {"Loading and the layout reset wait for the game to close: it rewrites its key when it exits.",
     "Завантаження й скидання розкладки — при закритій грі: вона перезаписує свій ключ при виході.",
     "Загрузка и сброс раскладки — при закрытой игре: она перезаписывает свой ключ при выходе."},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "a text for every Text");

```

В `src/core/settings/strings.hpp` заменить:
```cpp
    DiagLogErrorsNone,
    DiagLogErrorsSome, DiagLogFps, DiagLog, DiagLogNone, DiagFixed,
    ServicePresets, PresetMaximum, PresetBalance, PresetSpeed, PresetPlain, PresetNote, PresetCustom,
    ServiceBackups, BackupRestore, BackupPrune, BackupNote, BackupNone, BackupAsk, ServiceReset, ResetButton,
    ResetAsk, ServiceVersions, VersionsValue, Unknown,
    ErrWriteFile, ErrWriteRegistry, ErrGameRunning, ApplyWaits, ErrBackup, ErrScreen, ErrLaunch, ErrRestore,
    PageSound, PageGame, PageChat,
    GameQualityHeading, GameSprites, GameSpritesHint, GameTextures, GameTexturesHint, LevelHigh, LevelHalf, LevelLow,
```
на:
```cpp
    DiagLogErrorsNone,
    DiagLogErrorsSome, DiagLogFps, DiagLog, DiagLogNone, DiagFixed,
    ServicePresets, PresetMaximum, PresetBalance, PresetSpeed, PresetPlain, PresetNote, PresetCustom,
    ServiceBackups, BackupRestore, BackupPrune, BackupNote, BackupNone, BackupAsk, ServiceUaroGraphics, ResetButton,
    ResetAsk, VersionsValue, Unknown,
    ErrWriteFile, ErrWriteRegistry, ErrGameRunning, ApplyWaits, ErrBackup, ErrScreen, ErrLaunch, ErrRestore,
    PageSound, PageGame, PageChat,
    GameQualityHeading, GameSprites, GameSpritesHint, GameTextures, GameTexturesHint, LevelHigh, LevelHalf, LevelLow,
```

В `src/core/settings/strings.hpp` заменить:
```cpp
    ChatForgetAsk, ChatLoginForgotten, ChatMacrosHeading, ChatMacrosHint,
    ServiceGameSettings, GameSave, GameLoad, LayoutReset, GameSettingsNote, GameLoadAsk, LayoutResetAsk, GameSaved,
    GameLoaded, LayoutResetDone, ErrReadFile, FileFilterReg, DiagGpuValueCapped, SoundSetupDiffers,
    Count
};

```
на:
```cpp
    ChatForgetAsk, ChatLoginForgotten, ChatMacrosHeading, ChatMacrosHint,
    ServiceGameSettings, GameSave, GameLoad, LayoutReset, GameSettingsNote, GameLoadAsk, LayoutResetAsk, GameSaved,
    GameLoaded, LayoutResetDone, ErrReadFile, FileFilterReg, DiagGpuValueCapped, SoundSetupDiffers,
    GameSettingsRunning,
    Count
};

```

В `tools/check_exe.py` заменить:
```python
    },
    "comctl32.dll": {
        "InitCommonControlsEx",
    },
    "gdi32.dll": {
        "CreateFontIndirectW", "DeleteObject", "GetDeviceCaps", "GetTextExtentPoint32W", "SelectObject",
```
на:
```python
    },
    "comctl32.dll": {
        "InitCommonControlsEx",
    },
    "comdlg32.dll": {
        "GetOpenFileNameW", "GetSaveFileNameW",
    },
    "gdi32.dll": {
        "CreateFontIndirectW", "DeleteObject", "GetDeviceCaps", "GetTextExtentPoint32W", "SelectObject",
```

- [ ] **Step 6: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `293 tests, 0 failed checks`.

- [ ] **Step 7: Коммит**

```bash
git add tests/test_model.cpp src/app/model.cpp src/app/model.hpp CMakeLists.txt src/app/main_window.cpp src/app/main_window.hpp src/app/page.hpp src/app/page_screen.cpp src/app/page_service.cpp src/app/page_settings.cpp src/app/ui.cpp src/app/ui.hpp src/core/settings/strings.cpp src/core/settings/strings.hpp tools/check_exe.py
git commit -q -F - <<'EOF'
Утилита: «Сервис» — настройки игры в файл, раскладка окон, пресет с качеством игры

Четыре группы: пресеты, копии, настройки игры, uaRO Graphics. Выбор файла —
стандартным окном Windows (comdlg32 в списке разрешённых). Пока игра запущена,
загрузка, сброс раскладки и восстановление серые, записка группы говорит
почему. Пресет учитывает качество игры.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 8: Видеопамять 32-битной программы (§7)

DXGI сообщает 32-битной программе не больше 3072 МБ: RTX 5060 Ti в логе 0.1.1 показала 3072 МБ. Начиная с этого числа
«Диагностика» пишет «не меньше 3072 МБ (32-битная программа больше не видит)», а отчёт —
`at least 3072 MB (a 32-bit program sees no more)`.

**Files:**
- Modify: `src/app/page_diagnostics.cpp`, `src/core/settings/report.cpp`, `src/core/settings/report.hpp`, `tests/test_service.cpp`

**Interfaces:**
- Consumes: —
- Produces: `constexpr unsigned long long kVramSeenAtMost = 3072`, `std::string vramText(unsigned long long mb)`
  (`report.hpp`).

- [ ] **Step 1: Падающий тест**

В `tests/test_service.cpp` заменить:
```cpp
    CHECK_EQ(d.tail.size(), size_t(2));
    CHECK(d.tail[1].rfind("19:21:01.000", 0) == 0);
    CHECK(!digestLog("").found);
}

TEST(report_hides_user_names_and_lists_the_settings) {
```
на:
```cpp
    CHECK_EQ(d.tail.size(), size_t(2));
    CHECK(d.tail[1].rfind("19:21:01.000", 0) == 0);
    CHECK(!digestLog("").found);
}

TEST(report_video_memory_at_the_32_bit_cap_is_a_floor) {
    // DXGI tells a 32-bit program no more than 3072 MB (the RTX 5060 Ti of 0.1.1's check showed 3072 MB).
    CHECK(vramText(2048) == "2048 MB");
    CHECK(vramText(3071) == "3071 MB");
    CHECK(vramText(3072) == "at least 3072 MB (a 32-bit program sees no more)");
}

TEST(report_hides_user_names_and_lists_the_settings) {
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_service.cpp:51:11: error: ‘vramText’ was not declared in this scope`.

- [ ] **Step 3: Реализация**

В `src/core/settings/report.cpp` заменить:
```cpp
#include "core/settings/ini_edit.hpp"

namespace uagfx::settings {

std::string maskUserPaths(const std::string& text) {
    std::string out;
```
на:
```cpp
#include "core/settings/ini_edit.hpp"

namespace uagfx::settings {

std::string vramText(unsigned long long mb) {
    const std::string text = std::to_string(mb) + " MB";
    return mb >= kVramSeenAtMost ? "at least " + text + " (a 32-bit program sees no more)" : text;
}

std::string maskUserPaths(const std::string& text) {
    std::string out;
```

В `src/core/settings/report.hpp` заменить:
```cpp
    std::vector<std::string> problems;       // red and yellow lines of the Diagnostics page
    LogDigest log;
};
// The text, in English for the developers; Windows user names in paths are hidden.
std::string makeReport(const ReportFacts& f);
std::string maskUserPaths(const std::string& text);  // "\Users\<name>\" -> "\Users\...\"
```
на:
```cpp
    std::vector<std::string> problems;       // red and yellow lines of the Diagnostics page
    LogDigest log;
};
// DXGI tells a 32-bit program no more than 3072 MB of video memory: from there on the number is the least the card
// has, not its size.
constexpr unsigned long long kVramSeenAtMost = 3072;
std::string vramText(unsigned long long mb);  // "2048 MB"; "at least 3072 MB (a 32-bit program sees no more)"
// The text, in English for the developers; Windows user names in paths are hidden.
std::string makeReport(const ReportFacts& f);
std::string maskUserPaths(const std::string& text);  // "\Users\<name>\" -> "\Users\...\"
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `294 tests, 0 failed checks`.

- [ ] **Step 5: Строка «Видеокарта» и отчёт (Win32)**

В `src/app/page_diagnostics.cpp` заменить:
```cpp
                           (win10 ? L"" : t(Text::DiagWindowsOld)),
                       win10 ? Tone::Good : Tone::Warn, {}});
        if (sys.d3d12Probed) {
            out.push_back({t(Text::DiagGpu),
                           tf(Text::DiagGpuValue, {sys.d3d12.adapter, num(static_cast<long long>(sys.d3d12.vramMb)),
                                                   sys.gpuDriver.empty() ? utf8(t(Text::Unknown)) : sys.gpuDriver}),
                           Tone::Normal, {}});
            out.push_back({t(Text::DiagD3D12),
                           sys.d3d12.ok ? t(Text::DiagD3D12Ok) : tf(Text::DiagD3D12Failed, {hresult(sys.d3d12.hr)}),
```
на:
```cpp
                           (win10 ? L"" : t(Text::DiagWindowsOld)),
                       win10 ? Tone::Good : Tone::Warn, {}});
        if (sys.d3d12Probed) {
            const std::vector<std::string> gpu{sys.d3d12.adapter, num(static_cast<long long>(sys.d3d12.vramMb)),
                                               sys.gpuDriver.empty() ? utf8(t(Text::Unknown)) : sys.gpuDriver};
            out.push_back({t(Text::DiagGpu),
                           tf(sys.d3d12.vramMb >= kVramSeenAtMost ? Text::DiagGpuValueCapped : Text::DiagGpuValue, gpu),
                           Tone::Normal, {}});
            out.push_back({t(Text::DiagD3D12),
                           sys.d3d12.ok ? t(Text::DiagD3D12Ok) : tf(Text::DiagD3D12Failed, {hresult(sys.d3d12.hr)}),
```

В `src/app/page_diagnostics.cpp` заменить:
```cpp
        f.driverVersion = s.installedVersion;
        f.windows = num(long(sys.windows.major)) + "." + num(long(sys.windows.minor)) + " build " +
                    num(long(sys.windows.build));
        f.gpu = sys.d3d12Probed ? sys.d3d12.adapter + ", " + num(static_cast<long long>(sys.d3d12.vramMb)) +
                                      " MB, driver " + (sys.gpuDriver.empty() ? "unknown" : sys.gpuDriver) +
                                      ", Direct3D 12: " + (sys.d3d12.ok ? "OK" : "failed " + hresult(sys.d3d12.hr))
                                : "not checked (Windows before 10)";
        for (const MonitorInfo& mon : sys.monitors)
```
на:
```cpp
        f.driverVersion = s.installedVersion;
        f.windows = num(long(sys.windows.major)) + "." + num(long(sys.windows.minor)) + " build " +
                    num(long(sys.windows.build));
        f.gpu = sys.d3d12Probed ? sys.d3d12.adapter + ", " + vramText(sys.d3d12.vramMb) + ", driver " +
                                      (sys.gpuDriver.empty() ? "unknown" : sys.gpuDriver) +
                                      ", Direct3D 12: " + (sys.d3d12.ok ? "OK" : "failed " + hresult(sys.d3d12.hr))
                                : "not checked (Windows before 10)";
        for (const MonitorInfo& mon : sys.monitors)
```

- [ ] **Step 6: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `294 tests, 0 failed checks`.

- [ ] **Step 7: Коммит**

```bash
git add tests/test_service.cpp src/core/settings/report.cpp src/core/settings/report.hpp src/app/page_diagnostics.cpp
git commit -q -F - <<'EOF'
Диагностика: видеопамять 32-битной программы — нижняя граница

DXGI сообщает 32-битной программе не больше 3072 МБ. С этого числа строка
«Видеокарта» и отчёт говорят «не меньше».

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 9: Драйвер: окно без рамки ровно по монитору; ini; строка лога (§5.4; 0.6.1, мелочь 11)

Окно без рамки игра ставит в своих точках: при 175 % монитор 3840×2160 для неё 2194×1234, и окно выходит 3839×2159
(лог 0.6.1). Драйвер ставит его в настоящих точках: поток на это время per-monitor aware (`RealPixels`). Затем он
проверяет, где окно оказалось; если не там — ставит, как раньше. `readTextFile` при ошибке не оставляет половины
файла. Чёткий текст, выключенный на лету, пишет в лог ту же строку, что и при старте без него, — её видит «Чёткий
текст в прошлый раз» в «Диагностике» (0.6.1, мелочь 11). Код только для Windows: проверка — сборка и пункты 3 и 15
чек-листа.

**Files:**
- Modify: `src/win/context.cpp`, `src/win/dpi.cpp`, `src/win/dpi.hpp`, `src/win/paths.cpp`, `src/win/window_mode.cpp`

**Interfaces:**
- Consumes: —
- Produces: `class win::RealPixels` (`dpi.hpp`): пока живёт, поток per-monitor aware; `active()`.

- [ ] **Step 1: Правки**

В `src/win/context.cpp` заменить:
```cpp
    config = u.next;  // render_scale, high_resolution, sharpen, vsync and fps_limit are read at every present
    if (config.uiFilter != was.uiFilter) renderer->setUiSharp(config.uiFilter == UiFilter::Sharp);
    if (config.crispText != was.crispText) {
        if (config.crispText)
            crisp::install(true);
        else
            crisp::uninstall();
    }
    if (config.logLevel != was.logLevel) wlog::setLevel(config.logLevel);
    std::string line = "settings: uaro-graphics.ini changed:";
```
на:
```cpp
    config = u.next;  // render_scale, high_resolution, sharpen, vsync and fps_limit are read at every present
    if (config.uiFilter != was.uiFilter) renderer->setUiSharp(config.uiFilter == UiFilter::Sharp);
    if (config.crispText != was.crispText) {
        if (config.crispText) {
            crisp::install(true);
        } else {
            crisp::uninstall();
            LOG_INFO("crisp text: off (crisp_text = 0)");  // as at a start without it: the Diagnostics page reads it
        }
    }
    if (config.logLevel != was.logLevel) wlog::setLevel(config.logLevel);
    std::string line = "settings: uaro-graphics.ini changed:";
```

В `src/win/dpi.cpp` заменить:
```cpp
    return SIZE{std::max<LONG>(1, rc.right - rc.left), std::max<LONG>(1, rc.bottom - rc.top)};
}

}  // namespace uagfx::win
```
на:
```cpp
    return SIZE{std::max<LONG>(1, rc.right - rc.left), std::max<LONG>(1, rc.bottom - rc.top)};
}

RealPixels::RealPixels() {
    if (const SetContextFn set = setContext()) old_ = set(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

RealPixels::~RealPixels() {
    if (old_) setContext()(old_);
}

}  // namespace uagfx::win
```

В `src/win/dpi.hpp` заменить:
```cpp
// is shown in real pixels (stage 3 spec §2: measured pixel for pixel on the game's own window).
SIZE clientPixels(HWND hwnd, bool physical);

}  // namespace uagfx::win
```
на:
```cpp
// is shown in real pixels (stage 3 spec §2: measured pixel for pixel on the game's own window).
SIZE clientPixels(HWND hwnd, bool physical);

// While it lives, this thread is per-monitor DPI aware: what it measures and places is in the screen's real pixels.
// Windows 10 1607 and newer; before that active() is false and nothing changes.
class RealPixels {
public:
    RealPixels();
    ~RealPixels();
    RealPixels(const RealPixels&) = delete;
    RealPixels& operator=(const RealPixels&) = delete;
    bool active() const { return old_ != nullptr; }

private:
    DPI_AWARENESS_CONTEXT old_ = nullptr;
};

}  // namespace uagfx::win
```

В `src/win/paths.cpp` заменить:
```cpp
        ok = out.empty() || (ReadFile(f, &out[0], DWORD(out.size()), &got, nullptr) && got == out.size());
    }
    CloseHandle(f);
    return ok;
}

```
на:
```cpp
        ok = out.empty() || (ReadFile(f, &out[0], DWORD(out.size()), &got, nullptr) && got == out.size());
    }
    CloseHandle(f);
    if (!ok) out.clear();  // not half a file: a caller that does not look at the answer reads nothing
    return ok;
}

```

В `src/win/window_mode.cpp` заменить:
```cpp
#include <vector>

#include "core/window_plan.hpp"
#include "win/log.hpp"

namespace uagfx::win {
```
на:
```cpp
#include <vector>

#include "core/window_plan.hpp"
#include "win/dpi.hpp"
#include "win/log.hpp"

namespace uagfx::win {
```

В `src/win/window_mode.cpp` заменить:
```cpp
    return mi.rcMonitor;
}

const char* actionName(WindowAction a) {
    switch (a) {
        case WindowAction::Window: return "a normal window";
```
на:
```cpp
    return mi.rcMonitor;
}

// Borderless over the monitor in its real pixels. uaRO.exe is not DPI-aware: in its coordinates a 3840 x 2160 monitor
// at 175 % is 2194 x 1234, and a window of that size came out 3839 x 2159 (0.6.1). False when it did not land on the
// monitor's pixels - Windows before 10 1607, or one that placed it otherwise - and `got` is where it went.
bool placeInPixels(HWND hwnd, int monitor, RECT& got) {
    RealPixels real;
    if (!real.active()) return false;
    const RECT mon = monitorRect(monitor);  // EnumDisplayMonitors and GetMonitorInfo tell real pixels now
    SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    GetWindowRect(hwnd, &got);
    return got.left == mon.left && got.top == mon.top && got.right == mon.right && got.bottom == mon.bottom;
}

const char* actionName(WindowAction a) {
    switch (a) {
        case WindowAction::Window: return "a normal window";
```

В `src/win/window_mode.cpp` заменить:
```cpp
        SetWindowLongA(hwnd, GWL_STYLE, LONG(WS_POPUP) | visible);
        SetWindowLongA(hwnd, GWL_EXSTYLE,
                       ex & ~LONG(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE | WS_EX_TOPMOST));
        SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
```
на:
```cpp
        SetWindowLongA(hwnd, GWL_STYLE, LONG(WS_POPUP) | visible);
        SetWindowLongA(hwnd, GWL_EXSTYLE,
                       ex & ~LONG(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE | WS_EX_TOPMOST));
        RECT got{};
        if (placeInPixels(hwnd, config.monitor, got)) {
            LOG_INFO("window: %s (mode = %s), %ldx%ld real pixels at %ld,%ld", actionName(action),
                     displayModeToken(config.mode), got.right - got.left, got.bottom - got.top, got.left, got.top);
            return;
        }
        SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
```

- [ ] **Step 2: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `294 tests, 0 failed checks`.

- [ ] **Step 3: Коммит**

```bash
git add src/win/context.cpp src/win/dpi.cpp src/win/dpi.hpp src/win/paths.cpp src/win/window_mode.cpp
git commit -q -F - <<'EOF'
Драйвер: окно без рамки ровно по монитору; ini без половины файла; строка лога о чётком тексте

Окно без рамки ставится в настоящих точках монитора (поток на это время
per-monitor aware) и проверяется; иначе — как раньше. Было на точку меньше
при 175 %. readTextFile при ошибке не оставляет половины файла; выключенный
на лету чёткий текст пишет строку лога (ревью 0.6.1, мелочь 11).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 10: «Экран» при ROExt и запущенной игре; комментарии (§4.1; 0.6.1, мелочи 6, 9, 11)

При ROExt и запущенной игре ждать нужно только ini ROExt, а его пишет только план окна (0.6.1, мелочь 6).
`Model::screenWaits()` — то же условие, что у «Применить». «Экран» сереет только тогда, переключатели «Во весь экран» и
«В окне» доступны всегда. Ещё исправлены старые номера выпусков в комментариях (мелочь 9) и неточный комментарий кеша
букв (мелочь 11).

**Files:**
- Modify: `src/app/model.cpp`, `src/app/model.hpp`, `src/app/page_screen.cpp`, `src/core/glyph_cache.cpp`, `src/core/settings/screen.cpp`, `tests/test_model.cpp`

**Interfaces:**
- Consumes: `Model::screenPlan()`.
- Produces: `bool Model::screenWaits() const`.

- [ ] **Step 1: Падающий тест**

В `tests/test_model.cpp` заменить:
```cpp
    CHECK_EQ(valueOf(w.files[roext], "roext.windowwidth"), std::string("1680"));
}

TEST(model_a_first_run_while_the_game_runs_writes_the_key_and_leaves_roexts_ini) {
    // No key yet: the game waits in its setup.exe loop (registry.md §2) with ROExt loaded. The key goes now - it is
    // what lets the game start - and ROExt's ini waits for the game to close (spec §4.1; 0.6.1 review I1).
```
на:
```cpp
    CHECK_EQ(valueOf(w.files[roext], "roext.windowwidth"), std::string("1680"));
}

TEST(model_while_the_game_runs_only_a_screen_roext_sizes_waits) {
    // Spec §4.1, 0.6.1 minor 6: ROExt's ini waits while the game runs, and only a window's plan writes it - the full
    // screen does not: the Screen page greys the window's controls only.
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics.ini")] = "[renderer]\r\nbackend = uaro\r\n";
    w.files[fake::path(L"dinput.dll")] = std::string("MZ") + '\0' + "ROExt" + '\0' + ".\\dinput.ini" + '\0';
    w.files[fake::path(L"dinput.ini")] = "[ROExt]\r\nWindowLock=1\r\nWindowWidth=1920\r\nWindowHeight=1080\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.gameRunning = true;
    app::Model m;
    CHECK(m.screenWaits());  // the game's window: ROExt sizes it
    ScreenChoice c = m.screenChoice();
    c.show = Show::Fullscreen;
    m.setScreen(c);
    CHECK(!m.screenWaits());
    c.show = Show::Window;
    m.setScreen(c);
    w.gameRunning = false;
    m.refreshRunning();
    CHECK(!m.screenWaits());
}

TEST(model_a_first_run_while_the_game_runs_writes_the_key_and_leaves_roexts_ini) {
    // No key yet: the game waits in its setup.exe loop (registry.md §2) with ROExt loaded. The key goes now - it is
    // what lets the game start - and ROExt's ini waits for the game to close (spec §4.1; 0.6.1 review I1).
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "error" | head -1`
Expected: сборка тестов падает: `…/tests/test_model.cpp:233:13: error: ‘class uagfx::app::Model’ has no member named ‘screenWaits’`.

- [ ] **Step 3: Реализация**

В `src/app/model.cpp` заменить:
```cpp

ScreenPlan Model::screenPlan() const {
    return planScreen(screenChoice(), system_.monitors, backend(), now_.roext, system_.frame);
}

void Model::screenAsSetup() {
```
на:
```cpp

ScreenPlan Model::screenPlan() const {
    return planScreen(screenChoice(), system_.monitors, backend(), now_.roext, system_.frame);
}

bool Model::screenWaits() const {
    return now_.gameRunning && now_.roextModule >= 0 && screenPlan().writesRoext;
}

void Model::screenAsSetup() {
```

В `src/app/model.hpp` заменить:
```cpp
    void setScreen(const settings::ScreenChoice& c);
    settings::ScreenNow screenNow() const;
    settings::ScreenPlan screenPlan() const;
    void screenAsSetup();          // "as in setup.exe": the personal screen values go
    bool screenAsSetupPending() const { return screenAsSetup_; }
    std::optional<settings::Size> setupScreen(bool& fullscreen) const;  // setup.exe's, when it differs from the game's
```
на:
```cpp
    void setScreen(const settings::ScreenChoice& c);
    settings::ScreenNow screenNow() const;
    settings::ScreenPlan screenPlan() const;
    // The plan needs ROExt's ini, written only while the game is closed (spec §4.1): the Screen page greys what changes
    // the window. The full screen never needs it (0.6.1 minor 6).
    bool screenWaits() const;
    void screenAsSetup();          // "as in setup.exe": the personal screen values go
    bool screenAsSetupPending() const { return screenAsSetup_; }
    std::optional<settings::Size> setupScreen(bool& fullscreen) const;  // setup.exe's, when it differs from the game's
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: без предупреждений, `295 tests, 0 failed checks`.

- [ ] **Step 5: «Экран» и комментарии**

В `src/app/page_screen.cpp` заменить:
```cpp
        if (monitors.empty()) return;
        const int index = uaro && c.monitor >= 0 && size_t(c.monitor) < monitors.size() ? c.monitor : 0;
        const MonitorInfo& mon = monitors[size_t(index)];
        // ROExt sizes the window, and its ini is written only while the game is closed (spec §4.1): until then the
        // choice is grey (0.6.0 review M4).
        const Snapshot& snap = m.now();
        const bool roext = snap.roext.active && (snap.roext.lock || snap.roext.size.width > 0) && snap.roextModule >= 0;
        const bool open = !(roext && snap.gameRunning);

        p.radio(kFullscreen, t(Text::ScreenShow), t(Text::ScreenFullscreen), c.show == Show::Fullscreen, true, L"",
                open);
        p.radio(kWindow, L"", t(Text::ScreenWindow), c.show == Show::Window, false, L"", open);
        p.gap(6);
        std::vector<std::wstring> items;
        for (size_t i = 0; i < monitors.size(); ++i)
```
на:
```cpp
        if (monitors.empty()) return;
        const int index = uaro && c.monitor >= 0 && size_t(c.monitor) < monitors.size() ? c.monitor : 0;
        const MonitorInfo& mon = monitors[size_t(index)];
        // ROExt sizes the window, and its ini is written only while the game is closed (spec §4.1): until then what
        // changes the window is grey (0.6.0 review M4). The full screen never needs it (0.6.1 minor 6).
        const Snapshot& snap = m.now();
        const bool roext = snap.roext.active && (snap.roext.lock || snap.roext.size.width > 0) && snap.roextModule >= 0;
        const bool open = !m.screenWaits();

        p.radio(kFullscreen, t(Text::ScreenShow), t(Text::ScreenFullscreen), c.show == Show::Fullscreen, true, L"");
        p.radio(kWindow, L"", t(Text::ScreenWindow), c.show == Show::Window, false, L"");
        p.gap(6);
        std::vector<std::wstring> items;
        for (size_t i = 0; i < monitors.size(); ++i)
```

В `src/core/glyph_cache.cpp` заменить:
```cpp
    order_.push_front(Entry{key, std::move(g), c});
    try {
        map_[key] = order_.begin();
    } catch (...) {  // no memory for its index: the letter goes again, the cache stays as it was (0.5.2 review 2c)
        order_.pop_front();
        throw;
    }
```
на:
```cpp
    order_.push_front(Entry{key, std::move(g), c});
    try {
        map_[key] = order_.begin();
    } catch (...) {
        // No memory for its index: the letter goes again, and the cache stays whole without it - and without the glyph
        // it was to replace (0.5.2 review 2c).
        order_.pop_front();
        throw;
    }
```

В `src/core/settings/screen.cpp` заменить:
```cpp
    }
    const MonitorInfo& primary = monitors[0];
    const bool uaro = backend == Backend::Uaro;
    // dgVoodoo leaves our ini alone: its window and full screen stay on the main monitor until 0.6.2.
    const int index = uaro && c.monitor >= 0 && size_t(c.monitor) < monitors.size() ? c.monitor : 0;
    const MonitorInfo& m = monitors[size_t(index)];
    if (c.show == Show::Fullscreen) {
```
на:
```cpp
    }
    const MonitorInfo& primary = monitors[0];
    const bool uaro = backend == Backend::Uaro;
    // dgVoodoo leaves our ini alone: its window and full screen stay on the main monitor until 0.6.3.
    const int index = uaro && c.monitor >= 0 && size_t(c.monitor) < monitors.size() ? c.monitor : 0;
    const MonitorInfo& m = monitors[size_t(index)];
    if (c.show == Show::Fullscreen) {
```

- [ ] **Step 6: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `295 tests, 0 failed checks`.

- [ ] **Step 7: Коммит**

```bash
git add tests/test_model.cpp src/app/model.cpp src/app/model.hpp src/app/page_screen.cpp src/core/glyph_cache.cpp src/core/settings/screen.cpp
git commit -q -F - <<'EOF'
Утилита: «Экран» при ROExt сереет только для окна; комментарии

ini ROExt пишет только план окна: при запущенной игре серыми становятся поля
окна, полный экран доступен (ревью 0.6.1, мелочь 6). Старые номера выпусков
и комментарий кеша букв (мелочи 9, 11).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

### Task 11: Выпуск 0.6.2 — версия, спецификация, чек-лист (§11)

Версия 0.6.2. В спецификации этапа 2:
- раздел «Чат и вход»;
- пометка звука;
- полный экран на деле (§5.4);
- видеопамять;
- страница «Сервиса»;
- строка 0.6.2 в таблице выпусков.

В v1 §4.1 — полный экран в настоящих точках. Чек-лист 0.6.2. Пакет собирает `tools/package.sh`.

**Files:**
- Modify: `dist/TEST.ru.md`, `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md`, `docs/superpowers/specs/2026-10-13-uaro-graphics-design.md`, `src/core/version.cpp`, `tests/test_version.cpp`

**Interfaces:**
- Consumes: —
- Produces: `kVersion = "0.6.2"`.

- [ ] **Step 1: Падающий тест версии**

В `tests/test_version.cpp` заменить:
```cpp
#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_6_1) { CHECK(std::strcmp(uagfx::kVersion, "0.6.1") == 0); }
```
на:
```cpp
#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_6_2) { CHECK(std::strcmp(uagfx::kVersion, "0.6.2") == 0); }
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | grep -E "FAIL|tests,"`
Expected: `FAIL version_is_0_6_2`, `295 tests, 1 failed checks`.

- [ ] **Step 3: Версия, спецификации, чек-лист**

`dist/TEST.ru.md`:
```markdown
# uaRO Graphics 0.6.2 — проверка

Что нового:
- Новые разделы утилиты: «Звук», «Игра», «Чат и вход», а в «Качестве» — качество самой игры (спрайты, текстуры,
  мип-уровни, тени на земле, туман). Это настройки игры в реестре — без `setup.exe`.
- «Сервис»: настройки игры в файл и из файла (панели навыков, окна, опции, макросы — хоть на другой ПК), сброс
  раскладки окон игры. Пресеты задают и качество игры.
- Во весь экран окно теперь ровно по монитору (было на точку меньше).
- «Диагностика»: видеопамять больше 3 ГБ — «не меньше 3072 МБ»: 32-битная программа больше не видит.
- Мелочи ревью 0.6.1: у серых полей нет букв Alt; при запущенной игре с ROExt на «Экране» серое только то, что
  меняет окно; строка в логе, когда чёткий текст выключают в запущенной игре.

## Установка

Закрой игру и утилиту и дважды щёлкни `update.bat` в папке клиента.

## Проверка

1. `update.bat` в конце написал `uaRO Graphics 0.6.2 build … is installed`.
2. Запусти `uaro-graphics.exe`. Пройди «Качество», «Звук», «Игру», «Чат и вход» и «Сервис» по-русски, по-украински
   и по-английски: тексты не обрезаны, не наезжают друг на друга, всё помещается в окно.
3. **Во весь экран.** «Экран» → «Во весь экран» → «Применить и играть». В `uaro-graphics.log` строка
   `window: borderless over the monitor (mode = auto), 3840x2160 real pixels at 0,0`; справа и снизу нет полоски в
   точку. Потом верни «В окне».
4. **Качество игры.** При закрытой игре: «Качество» → «Текстуры» — «половина» → «Применить и играть»: земля и стены
   мягче. Потом верни «высокое».
5. **Мип-уровни** (с нашим рендером их ещё не проверяли). При закрытой игре: «Качество» → отметь «Мип-уровни» →
   «Применить и играть». Земля вдали ровнее, без мерцания; игра работает как обычно. Напиши, как выглядит; если
   хуже — сними флажок.
6. **Пока игра запущена** — «Качество», «Звук», «Игра», «Чат и вход»: то, что игра пишет сама, серое, внизу записка
   «Серые настройки игра перезапишет при выходе…». «Тени и свет на земле» и устройство звука (звук, частота,
   разрядность, звуков одновременно) не серые.
7. **Звук.** При закрытой игре: «Звук» → режим «3D» — «Колонки» становятся доступны, «2D» — снова серые.
   «Громкость музыки» 60 → «Применить и играть»: музыка тише. Потом верни как было.
8. **Игра.** «Игра» → сними «Эффекты (/effect)» → «Применить и играть»: эффектов в игре нет. В списке «Скин» — «без
   скина» и папки из `skin\`. Флажок «Заставка» есть, только если в папке клиента есть `openning.bik`. Верни как
   было.
9. **Чат и вход.** Пустые поля макросов показывают серым макрос самой игры. Впиши в «Alt+4» `/lv` → «Применить и
   играть»: в игре Alt+4 пишет `/lv`. При закрытой игре «Забыть логин» спрашивает, потом кнопка гаснет; в игре поле
   логина пустое.
10. **Настройки игры в файл.** «Сервис» → «Сохранить в файл…» → файл `.reg`. Пока игра запущена, «Загрузить из
    файла…» и «Сбросить раскладку окон» серые. При закрытой игре «Загрузить из файла…» этот же файл: вопрос, потом
    «Загружено». Игра стартует с теми же панелями и окнами.
11. **Раскладка окон.** Сдвинь в игре пару окон (инвентарь, умения), выйди. «Сервис» → «Сбросить раскладку окон» →
    в игре окна открываются на своих местах, чат — внизу слева.
12. **Пресеты.** «Сервис» → «Скорость»: в «Качестве» текстуры — «половина», под пресетами «Сейчас: Скорость».
    Поставь текстуры «высокое» — «Сейчас: свои значения». Потом «Отменить».
13. **setup.exe и звук** — если на «Звуке» есть жёлтая строка «В setup.exe: …»: «Как в setup.exe» ставит списки как
    в setup.exe, «Применить» записывает, строка пропадает.
14. **ROExt и запущенная игра.** Пока игра запущена: «Экран» → «Во весь экран» выбирается; у «В окне» поля размера
    серые и под ними «Сначала закрой игру».
15. **Чёткий текст на лету.** Пока игра идёт: «Качество» → сними «Чёткий текст» → «Применить». Через пару секунд
    «Диагностика» → «Проверить снова»: «Чёткий текст в прошлый раз» — `off (crisp_text = 0)`. Потом включи обратно.
16. **Видеопамять.** «Диагностика» → «Видеокарта»: RTX 5060 Ti, «не меньше 3072 МБ (32-битная программа больше не
    видит)».
17. **Буквы Alt.** Пока игра запущена, нажми Alt на «Игре»: у серых флажков буквы не подчёркнуты.

## Что прислать

Файл `uaro-graphics.log` и коротко по каждому пункту. Если утилита что-то показывает не так — снимок её окна
(Win+Shift+S). Кнопка «Отчёт для форума» в «Диагностике» копирует всё нужное в буфер обмена.

## Если что-то не так

Вернуть настройки: «Сервис» → «Резервные копии» → «Восстановить». Вернуть dgVoodoo без утилиты: `use-dgvoodoo.bat`.

## Удаление

Закрой утилиту и дважды щёлкни `uninstall.bat` — всё вернётся как было. Папка `uaro-graphics-backup` с копиями
настроек останется.
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
| реестр: значения, которые пишет игра (§2.1) | только при закрытой игре | со следующего запуска |
| ini ROExt, `uaro-pad.ini`, `dgVoodoo.conf` | только при закрытой игре | со следующего запуска |

- Пока игра запущена, поля второго рода серые, в строке состояния подсказка «закрой игру».
- Перед записью утилита ещё раз проверяет, не запустилась ли игра.
- Если клиентов несколько, они перезапишут друг друга, поэтому утилита проверяет все.

```
на:
```markdown
| реестр: значения, которые пишет игра (§2.1) | только при закрытой игре | со следующего запуска |
| ini ROExt, `uaro-pad.ini`, `dgVoodoo.conf` | только при закрытой игре | со следующего запуска |

- Пока игра запущена, поля второго рода серые, на странице подсказка «закрой игру».
- Перед записью утилита ещё раз проверяет, не запустилась ли игра.
- Если клиентов несколько, они перезапишут друг друга, поэтому утилита проверяет все.

```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
- **Запись** идёт в личную копию. Если новое значение равно общему, личное удаляется, и этим значением снова управляет
  `setup.exe`.
- **Расхождение копий.** Если личное значение отличается от общего, у поля пометка «setup.exe: …» и кнопка «как в
  setup.exe» (удалить личное).
- **Запуск игры от администратора.** Если у `uaRO.exe` стоит флажок `RUNASADMIN` (§7), игра личную копию не видит.
  Утилита предупреждает и предлагает снять флажок; флажок в HKCU снимается без прав администратора.
- **Утилита запущена от администратора.** Полоса предупреждения: игра, запущенная из такой утилиты, тоже получит права
```
на:
```markdown
- **Запись** идёт в личную копию. Если новое значение равно общему, личное удаляется, и этим значением снова управляет
  `setup.exe`.
- **Расхождение копий.** Если личное значение отличается от общего, у поля пометка «setup.exe: …» и кнопка «как в
  setup.exe» (удалить личное). У звука пометка одна на всё устройство (режим, частота, разрядность, звуки, колонки):
  «В setup.exe: …» и кнопка «Как в setup.exe» (0.6.2).
- **Запуск игры от администратора.** Если у `uaRO.exe` стоит флажок `RUNASADMIN` (§7), игра личную копию не видит.
  Утилита предупреждает и предлагает снять флажок; флажок в HKCU снимается без прав администратора.
- **Утилита запущена от администратора.** Полоса предупреждения: игра, запущенная из такой утилиты, тоже получит права
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown

  Пример у пользователя: «в окне · окно 3360×1890 · интерфейс 175 % · игра 1920×1080».

### 5.4 Если полноэкранный путь не заработает

Полный экран клиента с нашим рендером на ПК пользователя ещё не проверялся. Он стоит первым пунктом чек-листа 0.6.0.

Если он не заработает, «Во весь экран» для нашего рендера станет таким:
- `ISFULLSCREENMODE = 0`;
- ini `mode = borderless`.

Это окно игры без рамки на весь монитор. Ограничение: `u ≥ d`, иначе окно игры при создании не поместится (§5.3).

## 6. Окно утилиты

**Расположение.**
- Слева список разделов: Рендер, Экран, Качество, Кадры, Звук, Игра, Ввод и окно, dgVoodoo, Диагностика, Сервис.
- Справа страница выбранного раздела.
- Вверху справа язык: EN / UA / RU. По умолчанию — язык Windows, если он украинский или русский, иначе английский.
  Выбор хранится в `[utility] language`.
```
на:
```markdown

  Пример у пользователя: «в окне · окно 3360×1890 · интерфейс 175 % · игра 1920×1080».

### 5.4 Полный экран на деле

Проверен на ПК пользователя в 0.6.1 (4K, 175 %): окно без рамки на весь монитор, панель задач скрыта, мышь попадает,
Alt+Tab работает. Окно выходило на точку меньше монитора (3839×2159): игра не знает о масштабе Windows и ставит его в
своих точках (2194×1234). С 0.6.2 драйвер ставит окно без рамки в настоящих точках монитора (поток на это время
per-monitor aware) и проверяет, где оно оказалось; если Windows поставила его иначе — как раньше, в точках игры.

Запасной путь, если полный экран перестанет работать: `ISFULLSCREENMODE = 0` и ini `mode = borderless` — окно игры
без рамки на весь монитор. Ограничение: `u ≥ d`, иначе окно игры при создании не поместится (§5.3).

## 6. Окно утилиты

**Расположение.**
- Слева список разделов: Рендер, Экран, Качество, Кадры, Звук, Игра, Чат и вход, Ввод и окно, dgVoodoo, Диагностика,
  Сервис.
- Справа страница выбранного раздела.
- Вверху справа язык: EN / UA / RU. По умолчанию — язык Windows, если он украинский или русский, иначе английский.
  Выбор хранится в `[utility] language`.
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
| Звук | звук выкл / 2D / 3D (`SOUNDMODE`), частота 22050 / 11025 / 8000 Гц, 16 / 8 бит, одновременных звуков 48 / 32 / 16, колонки (только 3D, `SPEAKERTYPE`) | реестр | 0.6.2 |
| | музыка вкл и громкость 0–127, звуки вкл и громкость 0–127 | `Option` | 0.6.2 |
| Игра | эффекты (`/effect`), «Miss» (`/miss`), привязка курсора: умения (`/skillsnap`), атака (`/snap`), предметы (`/itemsnap`), камера жёстко за персонажем (`/camera`), свой ИИ гомункула (`/hoai`) и наёмника (`/merai`) | `Option` | 0.6.2 |
| | шёпот: окно от незнакомых / от друзей / сигнал | `Option` | 0.6.2 |
| | скин (папки `skin\`, «без скина»), подсказка дня, заставка (если есть `openning.bik`) | реестр | 0.6.2 |
| | запоминать логин и «забыть логин», макросы чата Alt+1…Alt+0 (до 49 символов, пусто — встроенный) | реестр | 0.6.2 |
| | миникарта прозрачная, перемещать стопки без вопроса о количестве | `UIRectInfo` | 0.6.2 |
| Ввод и окно | ROExt (§2.3, без размера окна — он в «Экране») или uaRO Pad, или пояснение, что нет ни того, ни другого | ini ROExt / `uaro-pad.ini` | 0.6.3 |
| dgVoodoo | ключи §2.4 | `dgVoodoo.conf` | 0.6.3 |
| Диагностика | §7 | — | 0.6.0 |
```
на:
```markdown
| Звук | звук выкл / 2D / 3D (`SOUNDMODE`), частота 22050 / 11025 / 8000 Гц, 16 / 8 бит, одновременных звуков 48 / 32 / 16, колонки (только 3D, `SPEAKERTYPE`) | реестр | 0.6.2 |
| | музыка вкл и громкость 0–127, звуки вкл и громкость 0–127 | `Option` | 0.6.2 |
| Игра | эффекты (`/effect`), «Miss» (`/miss`), привязка курсора: умения (`/skillsnap`), атака (`/snap`), предметы (`/itemsnap`), камера жёстко за персонажем (`/camera`), свой ИИ гомункула (`/hoai`) и наёмника (`/merai`) | `Option` | 0.6.2 |
| | скин (папки `skin\`, «без скина»), подсказка дня, заставка (если есть `openning.bik`) | реестр | 0.6.2 |
| | миникарта прозрачная, перемещать стопки без вопроса о количестве | `UIRectInfo` | 0.6.2 |
| Чат и вход | шёпот: окно от незнакомых / от друзей / сигнал | `Option` | 0.6.2 |
| | запоминать логин и «забыть логин», макросы чата Alt+1…Alt+0 (до 49 символов, пусто — встроенный) | реестр | 0.6.2 |
| Ввод и окно | ROExt (§2.3, без размера окна — он в «Экране») или uaRO Pad, или пояснение, что нет ни того, ни другого | ini ROExt / `uaro-pad.ini` | 0.6.3 |
| dgVoodoo | ключи §2.4 | `dgVoodoo.conf` | 0.6.3 |
| Диагностика | §7 | — | 0.6.0 |
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
- **Шёпот.** У клиента ошибка загрузки: «от друзей» включает и «от незнакомых». Утилита это показывает, а не скрывает.
- **Раздел «Игра».** Значения, которые не хранятся в реестре (`/noctrl`, `/aura`, `/window` и т. п.), сюда не входят:
  их нельзя задать заранее.

## 7. Диагностика

```
на:
```markdown
- **Шёпот.** У клиента ошибка загрузки: «от друзей» включает и «от незнакомых». Утилита это показывает, а не скрывает.
- **Раздел «Игра».** Значения, которые не хранятся в реестре (`/noctrl`, `/aura`, `/window` и т. п.), сюда не входят:
  их нельзя задать заранее.
- **Раздел «Чат и вход»** (0.6.2): со шёпотом, логином и макросами «Игра» не помещается в окно утилиты. Флажки на
  «Игре» и в «Чате» — по два в ряд, макросы — по два в ряд.

## 7. Диагностика

```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown

**Система:**
- Windows (версия, сборка);
- видеокарта, версия драйвера, видеопамять;
- создаётся ли устройство D3D12 уровня 11_0.

**Мониторы** (у каждого):
```
на:
```markdown

**Система:**
- Windows (версия, сборка);
- видеокарта, версия драйвера, видеопамять (32-битной программе Windows сообщает не больше 3072 МБ: тогда «не меньше
  3072 МБ», 0.6.2);
- создаётся ли устройство D3D12 уровня 11_0.

**Мониторы** (у каждого):
```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
| Проверить обновление / обновить | `release/version.txt` с GitHub (WinHTTP) против `uaro-graphics\version.txt`; «Обновить» запускает `update.bat` и закрывает утилиту | 0.6.3 |
| Удалить uaRO Graphics | `uninstall.bat` после подтверждения, утилита закрывается | 0.6.3 |
| Версии | утилита, драйвер | 0.6.0 |

**Пресеты:**

```
на:
```markdown
| Проверить обновление / обновить | `release/version.txt` с GitHub (WinHTTP) против `uaro-graphics\version.txt`; «Обновить» запускает `update.bat` и закрывает утилиту | 0.6.3 |
| Удалить uaRO Graphics | `uninstall.bat` после подтверждения, утилита закрывается | 0.6.3 |
| Версии | утилита, драйвер | 0.6.0 |

На странице четыре группы: пресеты; резервные копии; настройки игры — «Сохранить в файл…», «Загрузить из файла…»,
«Сбросить раскладку окон»; uaRO Graphics — сброс к умолчаниям и версии (с 0.6.3 там же обновление и удаление). Пока
игра запущена, «Восстановить», «Загрузить» и «Сбросить раскладку» серые, а записка группы говорит почему (§4.1).

**Пресеты:**

```

В `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` заменить:
```markdown
|---|---|
| 0.6.0 | утилита: Рендер, Экран, Качество (наше), Кадры, Диагностика, Сервис (пресеты, резервные копии, сброс, версии); языки; применение на лету в драйвере; первый запуск без `setup.exe`; запись размера окна ROExt (§4.4); пакет и `.bat` |
| 0.6.1 | мелочи финальных ревью 0.5.2 (драйвер) и 0.6.0 (утилита): клавиатура и Alt+буква (§6), диагностика по §7, реестр по одному значению (§4.5), экран с ROExt ждёт закрытия игры (§4.1) |
| 0.6.2 | Звук, Игра, качество игры в «Качестве», перенос настроек игры, сброс раскладки окон, пресеты с качеством игры |
| 0.6.3 | Ввод и окно (ROExt, uaRO Pad), dgVoodoo, обновление и удаление из утилиты, `release/version.txt` |

Одна спецификация, отдельный план на каждый выпуск. План следующего выпуска пишется после проверки предыдущего.
```
на:
```markdown
|---|---|
| 0.6.0 | утилита: Рендер, Экран, Качество (наше), Кадры, Диагностика, Сервис (пресеты, резервные копии, сброс, версии); языки; применение на лету в драйвере; первый запуск без `setup.exe`; запись размера окна ROExt (§4.4); пакет и `.bat` |
| 0.6.1 | мелочи финальных ревью 0.5.2 (драйвер) и 0.6.0 (утилита): клавиатура и Alt+буква (§6), диагностика по §7, реестр по одному значению (§4.5), экран с ROExt ждёт закрытия игры (§4.1) |
| 0.6.2 | Звук, Игра, Чат и вход, качество игры в «Качестве», перенос настроек игры, сброс раскладки окон, пресеты с качеством игры; полный экран в настоящих точках монитора; мелочи финального ревью 0.6.1 |
| 0.6.3 | Ввод и окно (ROExt, uaRO Pad), dgVoodoo, обновление и удаление из утилиты, `release/version.txt` |

Одна спецификация, отдельный план на каждый выпуск. План следующего выпуска пишется после проверки предыдущего.
```

В `docs/superpowers/specs/2026-10-13-uaro-graphics-design.md` заменить:
```markdown

### 4.1 Режимы экрана и показ
- **Окно**: размер окна — `window_width × window_height` из настроек (0 — размер, заданный клиентом).
- **Полный экран** = окно без рамки на весь выбранный монитор. `SetDisplayMode` задаёт только логическое разрешение,
  режим монитора не меняется. Эксклюзивный режим не используется, Alt+Tab мгновенный, потеря поверхностей
  (`DDERR_SURFACELOST`) никогда не сообщается.
- Flip(WAIT) и WaitForVerticalBlank+Blt → показ с vsync по настройке; Blt экрана загрузки → показ сразу.
- Настройки: VSync, ограничение FPS, разрешение разрывов (для G-SYNC/VRR).

```
на:
```markdown

### 4.1 Режимы экрана и показ
- **Окно**: размер окна — `window_width × window_height` из настроек (0 — размер, заданный клиентом).
- **Полный экран** = окно без рамки на весь выбранный монитор, в его настоящих точках (0.6.2; этап 2, §5.4).
  `SetDisplayMode` задаёт только логическое разрешение, режим монитора не меняется. Эксклюзивный режим не
  используется, Alt+Tab мгновенный, потеря поверхностей (`DDERR_SURFACELOST`) никогда не сообщается.
- Flip(WAIT) и WaitForVerticalBlank+Blt → показ с vsync по настройке; Blt экрана загрузки → показ сразу.
- Настройки: VSync, ограничение FPS, разрешение разрывов (для G-SYNC/VRR).

```

В `src/core/version.cpp` заменить:
```cpp

namespace uagfx {

const char* const kVersion = "0.6.1";

}  // namespace uagfx
```
на:
```cpp

namespace uagfx {

const char* const kVersion = "0.6.2";

}  // namespace uagfx
```

- [ ] **Step 4: Сборка и проверки**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: `OK: build-win/ddraw.dll: …`, `OK: build-win/uaro-graphics.exe: GUI, 201 functions …`, затем `295 tests, 0 failed checks`.

- [ ] **Step 5: Пакет**

Run: `tools/package.sh && unzip -p build-win/uaro-graphics.zip TEST.ru.md | head -1 && unzip -p build-win/uaro-graphics.zip uaro-graphics/version.txt`
Expected: `packed build-win/uaro-graphics.zip`, `# uaRO Graphics 0.6.2 — проверка`, `0.6.2 build …`.

- [ ] **Step 6: Коммит**

```bash
git add tests/test_version.cpp dist/TEST.ru.md docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md docs/superpowers/specs/2026-10-13-uaro-graphics-design.md src/core/version.cpp
git commit -q -F - <<'EOF'
0.6.2: версия, спецификация, чек-лист

Спецификация этапа 2: раздел «Чат и вход», пометка setup.exe у звука, полный
экран на деле (§5.4), видеопамять, страница «Сервиса», строка 0.6.2. v1:
полный экран в настоящих точках монитора. Чек-лист 0.6.2.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
```

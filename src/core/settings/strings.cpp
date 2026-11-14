#include "core/settings/strings.hpp"

#include <cctype>

namespace uagfx::settings {
namespace {

struct Entry {
    Text id;
    const char* en;
    const char* uk;
    const char* ru;
};

// In the order of Text.
constexpr Entry kTexts[] = {
    {Text::AppTitle, "uaRO",
     "uaRO",
     "uaRO"},
    {Text::PageRender, "Renderer", "Рендер", "Рендер"},
    {Text::PageScreen, "Screen", "Екран", "Экран"},
    {Text::PageQuality, "Quality", "Якість", "Качество"},
    {Text::PageFrames, "Frames", "Кадри", "Кадры"},
    {Text::PageDiagnostics, "Diagnostics", "Діагностика", "Диагностика"},
    {Text::PageService, "Service", "Сервіс", "Сервис"},
    {Text::Apply, "&Apply", "&Застосувати", "&Применить"},
    {Text::ApplyAndPlay, "Apply and &play", "Застосувати й &грати", "Применить и и&грать"},
    {Text::Revert, "&Undo", "&Скасувати", "&Отменить"},
    {Text::Close, "&Close", "За&крити", "За&крыть"},
    {Text::StatusRunning, "The game is running.", "Гра запущена.", "Игра запущена."},
    {Text::StatusClosed, "The game is not running.", "Гра не запущена.", "Игра не запущена."},
    {Text::StatusPending, "Not applied: {1}.", "Не застосовано: {1}.", "Не применено: {1}."},
    {Text::StatusApplied, "Applied.", "Застосовано.", "Применено."},
    {Text::ElevatedBanner,
     "The settings app runs as administrator: a game started from it would not see its settings. Close it and start "
     "it normally.",
     "Утиліту запущено від імені адміністратора: гра, запущена з неї, не побачить свої налаштування. Закрий її та "
     "запусти звичайно.",
     "Утилита запущена от имени администратора: игра, запущенная из неё, не увидит свои настройки. Закрой её и "
     "запусти обычно."},
    {Text::AfterRestart, "↻ after the game restarts", "↻ після перезапуску гри", "↻ после перезапуска игры"},
    {Text::LiveNote, "These settings work in a running game too, without a restart.",
     "Ці налаштування діють і в запущеній грі, без перезапуску.",
     "Эти настройки действуют и в запущенной игре, без перезапуска."},
    {Text::LiveNoteRunning, "The game is running: a slider applies as soon as you let it go.",
     "Гра запущена: повзунок застосовується, щойно його відпустиш.",
     "Игра запущена: ползунок применяется, как только его отпустишь."},
    {Text::AskUnsaved,
     "Some changes are not applied. Close without them?", "Деякі зміни не застосовано. Закрити без них?",
     "Часть изменений не применена. Закрыть без них?"},
    {Text::RenderHeading, "What draws the game", "Чим малювати гру", "Чем рисовать игру"},
    {Text::RenderNeedsWindows10, "Our renderer is not available: it needs Windows 10 or newer.",
     "Наш рендер недоступний: потрібна Windows 10 або новіша.", "Наш рендер недоступен: нужна Windows 10 или новее."},
    {Text::RenderNoD3D12, "Our renderer is not available: Direct3D 12 does not start ({1}).",
     "Наш рендер недоступний: Direct3D 12 не запускається ({1}).",
     "Наш рендер недоступен: Direct3D 12 не запускается ({1})."},
    {Text::InstalledOurs,
     "Installed: uaRO Graphics {1}.", "Встановлено: uaRO Graphics {1}.", "Установлено: uaRO Graphics {1}."},
    {Text::InstalledNoDriver, "uaRO Graphics' ddraw.dll is not in the game folder: run install.bat.",
     "ddraw.dll uaRO Graphics немає в теці гри: запусти install.bat.",
     "ddraw.dll uaRO Graphics нет в папке игры: запусти install.bat."},
    {Text::PauseNote, "In the game, Pause (or Scroll Lock) switches every improvement off and on, to compare.",
     "У грі клавіша Pause (або Scroll Lock) вимикає й вмикає всі покращення — для порівняння.",
     "В игре клавиша Pause (или Scroll Lock) выключает и включает все улучшения — для сравнения."},
    {Text::ScreenShow, "Show", "Показ", "Показ"},
    {Text::ScreenFullscreen, "Full screen", "На весь екран", "Во весь экран"},
    {Text::ScreenWindow, "In a window", "У вікні", "В окне"},
    {Text::ScreenMonitor, "Monitor", "Монітор", "Монитор"},
    {Text::MonitorItem, "{1}: {2}×{3}, scaling {4} %", "{1}: {2}×{3}, масштаб {4} %", "{1}: {2}×{3}, масштаб {4} %"},
    {Text::MonitorMain, ", main", ", основний", ", основной"},
    {Text::ScreenUi, "Interface size", "Розмір інтерфейсу", "Размер интерфейса"},
    {Text::ScreenUiHint, "Screen pixels per game pixel. More: a larger interface and less of the world.",
     "Скільки точок екрана на одну точку гри. Більше — більший інтерфейс і менше видно світу.",
     "Сколько точек экрана на одну точку игры. Больше — крупнее интерфейс и меньше видно мира."},
    {Text::ScreenUiExact, "exactly, %", "точно, %", "точно, %"},
    {Text::ScreenWindowSize, "Window size", "Розмір вікна", "Размер окна"},
    {Text::ScreenWindowSizeHint, "In the screen's real pixels; only sizes that fit the screen above the task bar.",
     "У справжніх точках екрана; лише ті, що вміщуються над панеллю завдань.",
     "В настоящих точках экрана; только те, что помещаются над панелью задач."},
    {Text::WindowItem, "{1}×{2} (game {3}×{4})", "{1}×{2} (гра {3}×{4})", "{1}×{2} (игра {3}×{4})"},
    {Text::ScreenResult, "Game {1}×{2} → screen {3}×{4}, interface ×{5}", "Гра {1}×{2} → екран {3}×{4}, інтерфейс ×{5}",
     "Игра {1}×{2} → экран {3}×{4}, интерфейс ×{5}"},
    {Text::ScreenCrispest, " — the crispest interface", " — найчіткіший інтерфейс", " — самый чёткий интерфейс"},
    {Text::ScreenTooBig, "It will not fit: the window is larger than the screen above the task bar.",
     "Не вміститься: вікно більше за екран над панеллю завдань.",
     "Не поместится: окно больше экрана над панелью задач."},
    {Text::ScreenTooSmall, "Too small: the game needs at least 640×480.", "Замало: грі потрібно щонайменше 640×480.",
     "Слишком мало: игре нужно не меньше 640×480."},
    {Text::ScreenBadNumber, "Not a valid size.", "Неправильний розмір.", "Неверный размер."},
    {Text::ScreenNowWindow, "Now: in a window · window {1}×{2} · interface {3} % · game {4}×{5}",
     "Зараз: у вікні · вікно {1}×{2} · інтерфейс {3} % · гра {4}×{5}",
     "Сейчас: в окне · окно {1}×{2} · интерфейс {3} % · игра {4}×{5}"},
    {Text::ScreenNowFullscreen,
     "Now: full screen · game {1}×{2} · interface {3} %", "Зараз: на весь екран · гра {1}×{2} · інтерфейс {3} %",
     "Сейчас: во весь экран · игра {1}×{2} · интерфейс {3} %"},
    {Text::ScreenRoext, "ROExt sets the window size ({1}): the app writes the same size there.",
     "Розмір вікна задає ROExt ({1}): утиліта запише туди той самий розмір.",
     "Размер окна задаёт ROExt ({1}): утилита запишет туда тот же размер."},
    {Text::ScreenFirstRun,
     "The game has no settings in the registry yet: Apply creates everything it needs, no setup.exe required.",
     "У реєстрі ще немає налаштувань гри: «Застосувати» створить усе потрібне, setup.exe не знадобиться.",
     "В реестре ещё нет настроек игры: «Применить» создаст всё нужное, setup.exe не понадобится."},
    {Text::ScreenAltTab, "In full screen the game minimises itself on Alt+Tab: that is how it works.",
     "На весь екран гра сама згортається при Alt+Tab — так вона влаштована.",
     "Во весь экран игра сама сворачивается при Alt+Tab — так она устроена."},
    {Text::QualityHighRes, "High resolution", "Висока роздільність", "Высокое разрешение"},
    {Text::QualityHighResHint, "Draw the game at the window's real pixels: with Windows scaling the world is sharper.",
     "Малювати гру в справжніх точках вікна: при масштабі Windows світ чіткіший.",
     "Рисовать игру в настоящих точках окна: при масштабе Windows мир чётче."},
    {Text::QualityDpiAware, "Window with Windows scaling (test)", "Вікно з масштабом Windows (тест)",
     "Окно с масштабом Windows (тест)"},
    {Text::QualityDpiAwareHint,
     "For Windows scaling of 125-175 %: the game's window is told to follow it, so it covers the monitor pixel for "
     "pixel and Windows shows the picture straight on it - even frames instead of jumps between 72 and 48. Takes "
     "effect after the game's restart. If the mouse misses, turn it off.",
     "Для масштабу Windows 125–175 %: вікно гри повідомляє Windows, що саме зважає на масштаб, тож закриває монітор "
     "точка в точку, і Windows показує картинку прямо на ньому — рівні кадри замість стрибків між 72 і 48. Діє після "
     "перезапуску гри. Якщо миша промахується — вимкніть.",
     "Для масштаба Windows 125–175 %: окно игры сообщает Windows, что само учитывает масштаб, поэтому закрывает "
     "монитор точка в точку, и Windows показывает картинку прямо на нём — ровные кадры вместо скачков между 72 и 48. "
     "Действует после перезапуска игры. Если мышь промахивается — выключите."},
    {Text::QualityRenderScale, "Render scale", "Масштаб рендера", "Масштаб рендера"},
    {Text::QualityRenderScaleHint,
     "Over 100 %: the frame is drawn larger and shrunk when shown - smoother edges, more work for the graphics card. "
     "Under 100 % (a large screen, a weaker card): drawn smaller and made larger sharply (a Catmull-Rom filter and "
     "CAS sharpening, like FSR 1) - less work, a little softer.",
     "Понад 100 %: кадр малюється більшим і зменшується при показі — гладші краї, відеокарта працює більше. Менше 100 "
     "% (великий екран, слабша карта): малюється меншим і збільшується різко (фільтр Catmull-Rom і різкість CAS, як "
     "FSR 1) — менше роботи, трохи м'якше.",
     "Больше 100 %: кадр рисуется больше и уменьшается при показе — глаже края, видеокарта работает больше. Меньше "
     "100 % (большой экран, слабая карта): рисуется меньше и увеличивается резко (фильтр Catmull-Rom и резкость CAS, "
     "как FSR 1) — меньше работы, чуть мягче."},
    // 2.5.16
    {Text::QualityWorldScale, "World resolution", "Роздільність світу", "Разрешение мира"},
    {Text::QualityWorldScaleHint,
     "Direct3D 11, for weaker cards: the world (the map, the characters, the effects) is drawn at this share of the "
     "picture's size and made larger plainly (sharpening it cost a weak card more than the smaller world saved); the "
     "interface and the text stay sharp. 100 % = as it is; 75 % = about half the pixels, softer.",
     "Direct3D 11, для слабших карт: світ (мапа, персонажі, ефекти) малюється в такій частці розміру картинки й "
     "просто збільшується (різкість коштувала слабкій карті більше, ніж заощаджував менший світ); інтерфейс і текст "
     "лишаються чіткими. 100 % — як є; 75 % — приблизно вдвічі менше точок, м'якше.",
     "Direct3D 11, для слабых карт: мир (карта, персонажи, эффекты) рисуется в такой доле размера картинки и просто "
     "увеличивается (резкость стоила слабой карте больше, чем экономил меньший мир); интерфейс и текст остаются "
     "чёткими. 100 % — как есть; 75 % — примерно вдвое меньше точек, мягче."},
    // 2.5.18
    {Text::QualityFullColour, "Full-colour textures", "Повноколірні текстури", "Полноцветные текстуры"},
    {Text::QualityFullColourHint,
     "The game makes every texture 16-bit as it loads it: glows, fire and the skills' particles get steps (16 shades "
     "of transparency). On: the textures are drawn in their pictures' own colours; the texture pack finds its "
     "pictures as before. Changed in the game, it applies to the textures loaded from then on.",
     "Гра робить кожну текстуру 16-бітною, коли завантажує її: світіння, вогонь і частки вмінь стають ступінчастими "
     "(16 відтінків прозорості). Увімкнено: текстури малюються у власних кольорах своїх картинок; пакет текстур "
     "знаходить свої картинки, як і раніше. Змінене в грі діє на текстури, завантажені після зміни.",
     "Игра делает каждую текстуру 16-битной при загрузке: свечения, огонь и частицы умений становятся ступенчатыми "
     "(16 оттенков прозрачности). Включено: текстуры рисуются в собственных цветах своих картинок; пакет текстур "
     "находит свои картинки, как и раньше. Изменённое в игре действует на текстуры, загруженные после изменения."},
    {Text::QualitySharpen, "Sharpening", "Різкість", "Резкость"},
    {Text::QualitySharpenHint, "Sharpening of the picture on screen; 0 = off. Screenshots are not sharpened.",
     "Різкість картинки на екрані; 0 — вимкнено. Скриншоти не змінюються.",
     "Резкость картинки на экране; 0 — выключено. Скриншоты не меняются."},
    {Text::QualityUiFilter, "Interface pictures", "Картинки інтерфейсу", "Картинки интерфейса"},
    {Text::QualityUiFilterHint,
     "How enlarged interface pictures look. With crisp text the windows come from their crisp twins.",
     "Як виглядають збільшені картинки інтерфейсу. З чітким текстом вікна беруться з чітких двійників.",
     "Как выглядят увеличенные картинки интерфейса. При чётком тексте окна берутся из чётких двойников."},
    {Text::UiSharp, "sharp", "чіткі", "чёткие"},
    {Text::UiSmooth, "smooth", "м'які", "мягкие"},
    {Text::QualityCrispText, "Crisp text", "Чіткий текст", "Чёткий текст"},
    {Text::QualityCrispTextHint,
     "The game's text, window frames and fills at the screen's real pixels when the game is enlarged.",
     "Текст, рамки й заливки вікон гри в справжніх точках екрана, коли гру збільшено.",
     "Текст, рамки и заливки окон игры в настоящих точках экрана, когда игра увеличена."},
    // 3.0.18-beta.178
    {Text::QualityCrispNumbers, "Crisp damage numbers", "Чіткі цифри урону", "Чёткие цифры урона"},
    {Text::QualityCrispNumbersHint,
     "Damage numbers, Miss, Guard, Critical and Lucky: the game's own pictures, enlarged sharp (xBR) instead of "
     "blurred. The game moves them as always.",
     "Цифри урону, Miss, Guard, Critical і Lucky: власні картинки гри, збільшені чітко (xBR), а не розмито. Рухає "
     "їх гра, як завжди.",
     "Цифры урона, Miss, Guard, Critical и Lucky: собственные картинки игры, увеличенные чётко (xBR), а не размыто. "
     "Двигает их игра, как всегда."},
    {Text::FramesVsync,
     "Vertical sync (VSync)", "Вертикальна синхронізація (VSync)", "Вертикальная синхронизация (VSync)"},
    {Text::FramesVsyncHint,
     "Frames are shown in step with the monitor: no tearing.", "Кадри показуються в такт із монітором: без розривів.",
     "Кадры показываются в такт с монитором: без разрывов."},
    {Text::FramesFpsLimit, "FPS limit", "Обмеження FPS", "Ограничение FPS"},
    {Text::FramesFpsLimitHint,
     "Frames per second at most, 0-1000; 0 = no limit.", "Кадрів на секунду не більше, 0–1000; 0 — без обмеження.",
     "Кадров в секунду не больше, 0–1000; 0 — без ограничения."},
    {Text::FramesTearing,
     "Tearing for G-SYNC and FreeSync", "Розриви для G-SYNC і FreeSync", "Разрывы для G-SYNC и FreeSync"},
    {Text::FramesTearingHint, "With VSync off, G-SYNC and FreeSync monitors show each frame as soon as it is ready.",
     "Без VSync монітори G-SYNC і FreeSync показують кожен кадр, щойно він готовий.",
     "Без VSync мониторы G-SYNC и FreeSync показывают каждый кадр, как только он готов."},
    {Text::DiagCheck, "Check", "Перевірка", "Проверка"},
    {Text::DiagResult, "Result", "Результат", "Результат"},
    {Text::DiagFix, "&Fix", "&Виправити", "&Исправить"},
    {Text::DiagOpenLog, "Open the log", "Відкрити лог", "Открыть лог"},
    {Text::DiagOpenFolder, "Open the folder", "Відкрити теку", "Открыть папку"},
    {Text::DiagReport, "Report for the forum", "Звіт для форуму", "Отчёт для форума"},
    {Text::DiagRecheck, "Check again", "Перевірити знову", "Проверить снова"},
    {Text::DiagVerboseLog, "Detailed log", "Докладний лог", "Подробный лог"},
    {Text::DiagVerboseLogHint, "The log writes more details: useful when something is wrong.",
     "Лог пише більше подробиць: потрібно, коли щось не так.",
     "Лог пишет больше подробностей: нужно, когда что-то не так."},
    {Text::DiagReportDone, "The report is on the clipboard and in uaro-graphics-report.txt.",
     "Звіт скопійовано в буфер обміну й збережено в uaro-graphics-report.txt.",
     "Отчёт скопирован в буфер обмена и сохранён в uaro-graphics-report.txt."},
    {Text::DiagWindows, "Windows", "Windows", "Windows"},
    {Text::DiagWindowsValue, "{1}.{2}, build {3}", "{1}.{2}, збірка {3}", "{1}.{2}, сборка {3}"},
    {Text::DiagWindowsOld,
     " — our renderer needs Windows 10", " — нашому рендеру потрібна Windows 10", " — нашему рендеру нужна Windows 10"},
    {Text::DiagGpu, "Graphics card", "Відеокарта", "Видеокарта"},
    {Text::DiagGpuValue, "{1}, {2} MB, driver {3}", "{1}, {2} МБ, драйвер {3}", "{1}, {2} МБ, драйвер {3}"},
    {Text::DiagD3D12, "Direct3D 12", "Direct3D 12", "Direct3D 12"},
    {Text::DiagD3D12Ok, "works", "працює", "работает"},
    {Text::DiagD3D12Failed, "does not start ({1})", "не запускається ({1})", "не запускается ({1})"},
    {Text::DiagMonitor, "Monitor {1}", "Монітор {1}", "Монитор {1}"},
    {Text::DiagMonitorValue, "{1}×{2}, {3} Hz, scaling {4} %, HDR: {5}", "{1}×{2}, {3} Гц, масштаб {4} %, HDR: {5}",
     "{1}×{2}, {3} Гц, масштаб {4} %, HDR: {5}"},
    {Text::DiagMonitorWork, "Monitor {1}: work area", "Монітор {1}: робоча область", "Монитор {1}: рабочая область"},
    {Text::DiagMonitorWorkValue,
     "{1}×{2} without the task bar", "{1}×{2} без панелі завдань", "{1}×{2} без панели задач"},
    {Text::Yes, "yes", "так", "да"},
    {Text::No, "no", "ні", "нет"},
    {Text::DiagDdraw, "ddraw.dll", "ddraw.dll", "ddraw.dll"},
    {Text::DiagDdrawOther, "not uaRO Graphics: run install.bat", "не uaRO Graphics: запусти install.bat",
     "не uaRO Graphics: запусти install.bat"},
    {Text::DiagDdrawNone, "missing: run install.bat", "немає: запусти install.bat", "нет: запусти install.bat"},
    {Text::DiagModuleRoext, "ROExt (reads {1})", "ROExt (читає {1})", "ROExt (читает {1})"},
    {Text::DiagModuleOther, "an unknown module", "невідомий модуль", "неизвестный модуль"},
    {Text::DiagModuleOff, " — off", " — вимкнено", " — отключён"},
    {Text::DiagInput, "Input module", "Модуль вводу", "Модуль ввода"},
    {Text::DiagInputNone,
     "none: neither ROExt nor uaRO Pad", "немає: ні ROExt, ні uaRO Pad", "нет: ни ROExt, ни uaRO Pad"},
    {Text::DiagFolder, "Game folder", "Тека гри", "Папка игры"},
    {Text::DiagFolderOk, "writable", "можна писати", "можно писать"},
    {Text::DiagFolderReadOnly,
     "not writable: the settings cannot be saved", "не можна писати: налаштування не збережуться",
     "нельзя писать: настройки не сохранятся"},
    {Text::DiagFolderProgramFiles,
     "in Program Files: Windows redirects the game's files; better move the client, e.g. to C:\\Games",
     "у Program Files: Windows підміняє файли гри; краще перенести клієнт, наприклад у C:\\Games",
     "в Program Files: Windows подменяет файлы игры; лучше перенести клиент, например в C:\\Games"},
    {Text::DiagApp, "Settings app", "Утиліта", "Утилита"},
    {Text::DiagElevated,
     "runs as administrator: start it normally", "запущена від імені адміністратора: запусти звичайно",
     "запущена от имени администратора: запусти обычно"},
    {Text::DiagGame, "Game", "Гра", "Игра"},
    {Text::DiagGameRunning, "running", "запущена", "запущена"},
    {Text::DiagGameClosed, "not running", "не запущена", "не запущена"},
    {Text::DiagCompat, "Flag {1} ({2})", "Прапорець {1} ({2})", "Флажок {1} ({2})"},
    {Text::DiagCompatUser, "this user", "ваш користувач", "ваш пользователь"},
    {Text::DiagCompatAll, "all users", "усі користувачі", "все пользователи"},
    {Text::CompatRunAsAdmin, "the game does not see its personal settings in the registry",
     "гра не бачить свої особисті налаштування в реєстрі", "игра не видит свои личные настройки в реестре"},
    {Text::CompatOldWindows,
     "an old Windows mode: in the way of Direct3D 12 and scaling", "режим старої Windows: заважає Direct3D 12 і "
     "масштабу",
     "режим старой Windows: мешает Direct3D 12 и масштабу"},
    {Text::CompatDpi,
     "scaling overridden: the picture is not at the real pixels", "масштаб перевизначено: картинка не в справжніх "
     "точках",
     "масштаб переопределён: картинка не в настоящих точках"},
    {Text::CompatColors, "old colours or 640×480", "старі кольори або 640×480", "старые цвета или 640×480"},
    {Text::CompatOther, "a Windows compatibility flag", "прапорець сумісності Windows", "флажок совместимости Windows"},
    {Text::CompatAllUsersHint, "; remove it in uaRO.exe's properties as administrator",
     "; знімається у властивостях uaRO.exe з правами адміністратора",
     "; снимается в свойствах uaRO.exe с правами администратора"},
    {Text::DiagRegistry, "Game settings in the registry", "Налаштування гри в реєстрі", "Настройки игры в реестре"},
    {Text::DiagRegistryValue, "there; this app writes them", "є; їх пише ця утиліта", "есть; их пишет эта утилита"},
    {Text::DiagRegistryNone,
     "none: Apply on the Screen page creates them", "немає: «Застосувати» на сторінці «Екран» створить їх",
     "нет: «Применить» на странице «Экран» создаст их"},
    {Text::DiagLogRenderer, "Renderer last time", "Рендер минулого разу", "Рендер в прошлый раз"},
    {Text::DiagLogCrisp, "Crisp text last time", "Чіткий текст минулого разу", "Чёткий текст в прошлый раз"},
    {Text::DiagLogErrors, "Errors in the log", "Помилки в лозі", "Ошибки в логе"},
    {Text::DiagLogErrorsNone, "none", "немає", "нет"},
    {Text::DiagLogErrorsSome, "{1}, the last: {2}", "{1}, остання: {2}", "{1}, последняя: {2}"},
    {Text::DiagLogFps, "Frames per second", "Кадрів на секунду", "Кадров в секунду"},
    {Text::DiagLog, "Log", "Лог", "Лог"},
    {Text::DiagLogNone, "none yet: start the game", "ще немає: запусти гру", "ещё нет: запусти игру"},
    {Text::DiagFixed, "Fixed.", "Виправлено.", "Исправлено."},
    {Text::ServicePresets, "Presets", "Пресети", "Пресеты"},
    {Text::PresetMaximum, "Maximum", "Максимум", "Максимум"},
    {Text::PresetBalance, "Balance", "Баланс", "Баланс"},
    {Text::PresetSpeed, "Speed", "Швидкість", "Скорость"},
    {Text::PresetPlain, "No improvements", "Без покращень", "Без улучшений"},
    {Text::PresetNote, "A preset fills in the Quality and Smoothing pages; Apply writes it. Now: {1}.",
     "Пресет заповнює сторінки «Якість» і «Згладжування»; записує «Застосувати». Зараз: {1}.",
     "Пресет заполняет страницы «Качество» и «Сглаживание»; записывает «Применить». Сейчас: {1}."},
    {Text::PresetCustom, "your own values", "свої значення", "свои значения"},
    {Text::ServiceBackups, "Backups", "Резервні копії", "Резервные копии"},
    {Text::BackupRestore, "Restore", "Відновити", "Восстановить"},
    {Text::BackupPrune, "Keep the 5 newest", "Залишити 5 останніх", "Оставить 5 последних"},
    {Text::BackupNote, "Before its first write the app copies everything it may change; the 20 newest copies stay.",
     "Перед першим записом утиліта копіює все, що може змінити; зберігаються 20 останніх копій.",
     "Перед первой записью утилита копирует всё, что может изменить; хранятся 20 последних копий."},
    {Text::BackupNone, "No backups yet.", "Копій ще немає.", "Копий ещё нет."},
    {Text::BackupAsk, "Restore the settings from the copy of {1}? The current ones are copied first.",
     "Повернути налаштування з копії від {1}? Поточні спершу теж скопіюються.",
     "Вернуть настройки из копии от {1}? Нынешние сначала тоже скопируются."},
    {Text::ServiceUaroGraphics, "uaRO Graphics", "uaRO Graphics", "uaRO Graphics"},
    {Text::ResetButton,
     "Reset uaRO Graphics to its defaults", "Скинути uaRO Graphics до типових", "Сбросить uaRO Graphics к умолчаниям"},
    {Text::ResetAsk, "Return every uaRO Graphics setting to its default? The renderer and the language stay.",
     "Повернути всі налаштування uaRO Graphics до типових? Рендер і мова залишаться.",
     "Вернуть все настройки uaRO Graphics к умолчаниям? Рендер и язык останутся."},
    {Text::VersionsValue, "Settings app {1} · driver {2}", "Утиліта {1} · драйвер {2}", "Утилита {1} · драйвер {2}"},
    {Text::Unknown, "unknown", "невідомо", "неизвестно"},
    {Text::ErrWriteFile, "Cannot write {1}.", "Не вдалося записати {1}.", "Не удалось записать {1}."},
    {Text::ErrWriteRegistry,
     "Cannot write {1} to the registry.", "Не вдалося записати {1} до реєстру.", "Не удалось записать {1} в реестр."},
    {Text::ErrGameRunning, "Close the game first.", "Спершу закрий гру.", "Сначала закрой игру."},
    {Text::ApplyWaits,
     "Saved, all but what changes only while the game is closed - it is kept: close the game and press Apply again.",
     "Записано все, крім того, що змінюється лише при закритій грі, — воно не загубилося: закрий гру й натисни "
     "«Застосувати» ще раз.",
     "Записано всё, кроме того, что меняется только при закрытой игре, — оно не потерялось: закрой игру и нажми "
     "«Применить» ещё раз."},
    {Text::ErrBackup,
     "Cannot make the backup ({1}): nothing was written.", "Не вдалося зробити резервну копію ({1}): нічого не "
     "записано.",
     "Не удалось сделать резервную копию ({1}): ничего не записано."},
    {Text::ErrScreen, "Screen: {1}", "Екран: {1}", "Экран: {1}"},
    {Text::ErrLaunch,
     "Cannot start uaRO.exe (error {1}). The settings app must be in the uaRO client folder, next to uaRO.exe (or in "
     "its uaro-graphics folder).",
     "Не вдалося запустити uaRO.exe (помилка {1}). Утиліта має бути в теці клієнта uaRO, поруч з uaRO.exe (або в її "
     "теці uaro-graphics).",
     "Не удалось запустить uaRO.exe (ошибка {1}). Утилита должна быть в папке клиента uaRO, рядом с uaRO.exe (или в "
     "её папке uaro-graphics)."},
    {Text::ErrRestore, "Cannot restore {1}.", "Не вдалося відновити {1}.", "Не удалось восстановить {1}."},
    {Text::PageSound, "Sound", "Звук", "Звук"},
    {Text::PageGame, "Game", "Гра", "Игра"},
    {Text::PageChat, "Chat and login", "Чат і вхід", "Чат и вход"},
    {Text::GameQualityHeading, "The game's own quality", "Якість самої гри", "Качество самой игры"},
    {Text::GameSprites, "Sprites", "Спрайти", "Спрайты"},
    {Text::GameSpritesHint, "How sharp the game loads characters and monsters: high is their full size.",
     "Наскільки чітко гра завантажує персонажів і монстрів: висока — повний розмір.",
     "Насколько чётко игра загружает персонажей и монстров: высокое — полный размер."},
    {Text::GameTextures, "Textures", "Текстури", "Текстуры"},
    {Text::GameTexturesHint, "The size of the ground and wall textures: high is full, half and low save video memory.",
     "Розмір текстур землі й стін: висока — повний, половина й низька заощаджують відеопам'ять.",
     "Размер текстур земли и стен: высокое — полный, половина и низкое экономят видеопамять."},
    {Text::LevelHigh, "high", "висока", "высокое"},
    {Text::LevelHalf, "half", "половина", "половина"},
    {Text::LevelLow, "low", "низька", "низкое"},
    {Text::GameMipmaps, "Mipmaps", "Міп-рівні", "Мип-уровни"},
    {Text::GameMipmapsHint,
     "Smaller copies of textures for the distance (TRILINEARFILTER). With uaRO Graphics the 0.6.2 checklist tries "
     "them.",
     "Менші копії текстур для далеких місць (TRILINEARFILTER). З uaRO Graphics їх перевіряє чек-лист 0.6.2.",
     "Меньшие копии текстур для дальних мест (TRILINEARFILTER). С uaRO Graphics их проверяет чек-лист 0.6.2."},
    {Text::GameLightmaps, "Ground shadows and light", "Тіні й світло на землі", "Тени и свет на земле"},
    {Text::GameLightmapsHint, "The maps' baked shadows (ISLIGHTMAP); /lightmap switches them for one session only.",
     "Запечені тіні карт (ISLIGHTMAP); /lightmap перемикає їх лише на один сеанс.",
     "Запечённые тени карт (ISLIGHTMAP); /lightmap переключает их только на один сеанс."},
    {Text::GameFog, "Fog", "Туман", "Туман"},
    {Text::GameFogHint, "The maps' fog, as /fog.", "Туман карт, як /fog.", "Туман карт, как /fog."},
    {Text::GameClosedOnly, "The game rewrites the grey settings when it closes: change them while it is closed.",
     "Сірі налаштування гра перезапише при виході: змінюй їх, коли її закрито.",
     "Серые настройки игра перезапишет при выходе: меняй их, когда она закрыта."},
    {Text::SoundMode, "Sound", "Звук", "Звук"},
    {Text::SoundModeHint,
     "Off: no sound and no music. 2D: sounds without a direction. 3D: sounds from where they are (Miles 3D).",
     "Вимкнено: ні звуків, ні музики. 2D: звуки без напрямку. 3D: звуки звідти, де вони є (Miles 3D).",
     "Выключен: ни звуков, ни музыки. 2D: звуки без направления. 3D: звуки оттуда, где они есть (Miles 3D)."},
    {Text::SoundOff, "off", "вимкнено", "выключен"},
    {Text::Sound2D, "2D", "2D", "2D"},
    {Text::Sound3D, "3D", "3D", "3D"},
    {Text::SoundRate, "Sample rate", "Частота", "Частота"},
    {Text::SoundRateHint, "A higher rate sounds better.", "Вища частота звучить краще.", "Выше частота — лучше звук."},
    {Text::Rate22050, "22050 Hz", "22050 Гц", "22050 Гц"},
    {Text::Rate11025, "11025 Hz", "11025 Гц", "11025 Гц"},
    {Text::Rate8000, "8000 Hz", "8000 Гц", "8000 Гц"},
    {Text::SoundBits, "Bits", "Розрядність", "Разрядность"},
    {Text::SoundBitsHint, "16-bit sound is cleaner.", "16-бітний звук чистіший.", "16-битный звук чище."},
    {Text::Bits16, "16-bit", "16 біт", "16 бит"},
    {Text::Bits8, "8-bit", "8 біт", "8 бит"},
    {Text::SoundVoices, "Sounds at once", "Звуків одночасно", "Звуков одновременно"},
    {Text::SoundVoicesHint, "How many effects can play at the same time.", "Скільки ефектів може звучати разом.",
     "Сколько эффектов может звучать одновременно."},
    {Text::Voices48, "48", "48", "48"},
    {Text::Voices32, "32", "32", "32"},
    {Text::Voices16, "16", "16", "16"},
    {Text::SoundSpeakers, "Speakers", "Колонки", "Колонки"},
    {Text::SoundSpeakersHint,
     "3D only. The names come from the Miles Sound System 6 headers and are not checked on this client.",
     "Лише для 3D. Назви — із заголовків Miles Sound System 6, на цьому клієнті не перевірені.",
     "Только для 3D. Названия — из заголовков Miles Sound System 6, на этом клиенте не проверены."},
    {Text::Speakers2, "2 speakers", "2 колонки", "2 колонки"},
    {Text::SpeakersPhones, "headphones", "навушники", "наушники"},
    {Text::SpeakersSurround, "surround", "об'ємний", "объёмный"},
    {Text::Speakers4, "4 speakers", "4 колонки", "4 колонки"},
    {Text::Speakers51, "5.1", "5.1", "5.1"},
    {Text::Speakers71, "7.1", "7.1", "7.1"},
    {Text::SoundMusic, "Music", "Музика", "Музыка"},
    {Text::SoundMusicVolume, "Music volume", "Гучність музики", "Громкость музыки"},
    {Text::SoundEffects, "Sound effects", "Звукові ефекти", "Звуковые эффекты"},
    {Text::SoundEffectsVolume, "Effects volume", "Гучність ефектів", "Громкость эффектов"},
    {Text::SoundVolumeHint,
     "0-127, as /bv and /v in the game.", "0–127, як /bv і /v у грі.", "0–127, как /bv и /v в игре."},
    {Text::GameEffects, "Effects (/effect)", "Ефекти (/effect)", "Эффекты (/effect)"},
    {Text::GameMiss, "\"Miss\" (/miss)", "«Miss» (/miss)", "«Miss» (/miss)"},
    {Text::GameSkillSnap, "Cursor snaps to monsters: skills (/skillsnap)", "Курсор до монстрів: уміння (/skillsnap)",
     "Курсор к монстрам: умения (/skillsnap)"},
    {Text::GameAttackSnap, "Cursor snaps to monsters: attacks (/snap)", "Курсор до монстрів: атака (/snap)",
     "Курсор к монстрам: атака (/snap)"},
    {Text::GameItemSnap,
     "Cursor snaps to items (/itemsnap)", "Курсор до предметів (/itemsnap)", "Курсор к предметам (/itemsnap)"},
    {Text::GameCamera, "Camera locked on the character (/camera)", "Камера жорстко за персонажем (/camera)",
     "Камера жёстко за персонажем (/camera)"},
    {Text::GameHomunAi, "Own homunculus AI (/hoai)", "Свій ШІ гомункула (/hoai)", "Свой ИИ гомункула (/hoai)"},
    {Text::GameMercAi, "Own mercenary AI (/merai)", "Свій ШІ найманця (/merai)", "Свой ИИ наёмника (/merai)"},
    {Text::GameSkin, "Skin", "Скін", "Скин"},
    {Text::GameSkinHint, "How the interface looks: the folders of skin\\ next to uaRO.exe.",
     "Вигляд інтерфейсу: теки skin\\ поруч із uaRO.exe.", "Вид интерфейса: папки skin\\ рядом с uaRO.exe."},
    {Text::SkinNone, "no skin", "без скіна", "без скина"},
    {Text::SkinMissing, "{1} (no such folder)", "{1} (немає такої теки)", "{1} (нет такой папки)"},
    {Text::GameTips, "Tip of the day", "Підказка дня", "Подсказка дня"},
    {Text::GameIntro, "Intro movie", "Заставка", "Заставка"},
    {Text::GameMinimap, "Translucent minimap", "Прозора мінікарта", "Прозрачная миникарта"},
    {Text::GameStacks, "Move whole stacks without asking the amount", "Переносити стоси без питання про кількість",
     "Перемещать стопки без вопроса о количестве"},
    {Text::ChatWhisperStrangers,
     "A window for whispers from strangers", "Вікно шепоту від незнайомих", "Окно шёпота от незнакомых"},
    {Text::ChatWhisperFriends,
     "A window for whispers from friends", "Вікно шепоту від друзів", "Окно шёпота от друзей"},
    {Text::ChatWhisperSound,
     "A sound when a whisper comes", "Звук, коли приходить шепіт", "Звук, когда приходит шёпот"},
    {Text::ChatFriendsNote, "The client turns the window for strangers on too while the one for friends is on.",
     "Клієнт вмикає й вікно від незнайомих, поки ввімкнене вікно від друзів.",
     "Клиент включает и окно от незнакомых, пока включено окно от друзей."},
    {Text::ChatRememberLogin, "Remember the login", "Пам'ятати логін", "Запоминать логин"},
    {Text::ChatForgetLogin, "Forget the login", "Забути логін", "Забыть логин"},
    {Text::ChatForgetAsk, "Forget the saved login? The game asks for it next time.",
     "Забути збережений логін? Наступного разу гра його спитає.",
     "Забыть сохранённый логин? В следующий раз игра его спросит."},
    {Text::ChatLoginForgotten, "The login is forgotten.", "Логін забуто.", "Логин забыт."},
    {Text::ChatMacrosHeading, "Chat macros", "Макроси чату", "Макросы чата"},
    {Text::ChatMacrosHint,
     "Up to 49 letters; empty is the game's own one, shown grey. A /command runs, any other text goes to the chat.",
     "До 49 літер; порожній — власний макрос гри, показаний сірим. /команда виконується, інший текст іде в чат.",
     "До 49 букв; пустой — макрос самой игры, показан серым. /команда выполняется, другой текст уходит в чат."},
    // 3.0.11-beta.179: the login screen's shelf
    {Text::ChatForgetAccounts, "Forget all accounts", "Забути всі акаунти", "Забыть все аккаунты"},
    {Text::ChatForgetAccountsAsk,
     "Take all the saved accounts of the login screen's shelf (logins and passwords) off this computer? The accounts "
     "on the server stay.",
     "Прибрати з цього комп'ютера всі збережені акаунти полиці екрана входу (логіни й паролі)? Акаунти на сервері "
     "залишаться.",
     "Убрать с этого компьютера все сохранённые аккаунты полки экрана входа (логины и пароли)? Аккаунты на сервере "
     "останутся."},
    {Text::ChatAccountsForgotten, "The accounts are forgotten.", "Акаунти забуто.", "Аккаунты забыты."},
    {Text::ServiceGameSettings, "The game's settings", "Налаштування гри", "Настройки игры"},
    {Text::GameSave, "Save to a file...", "Зберегти у файл…", "Сохранить в файл…"},
    {Text::GameLoad, "Load from a file...", "Завантажити з файлу…", "Загрузить из файла…"},
    {Text::LayoutReset, "Reset the windows' layout", "Скинути розкладку вікон", "Сбросить раскладку окон"},
    {Text::GameSettingsNote, "The whole game key: hotbars, windows, options, macros. Another PC loads the file too.",
     "Увесь ключ гри: панелі, вікна, опції, макроси. Файл завантажується й на іншому ПК.",
     "Весь ключ игры: панели, окна, опции, макросы. Файл загружается и на другом ПК."},
    {Text::GameLoadAsk,
     "Load the game's settings from {1}? Everything the game saved (hotbars, windows, options, macros) is replaced; a "
     "copy of the present settings is made first.",
     "Завантажити налаштування гри з {1}? Усе, що зберегла гра (панелі, вікна, опції, макроси), заміниться; спершу "
     "буде копія нинішніх налаштувань.",
     "Загрузить настройки игры из {1}? Всё, что сохранила игра (панели, окна, опции, макросы), заменится; сначала "
     "будет копия нынешних настроек."},
    {Text::LayoutResetAsk, "Put every game window back in its place? Each takes it when it opens next time.",
     "Повернути всі вікна гри на свої місця? Кожне стане на місце, коли відкриється наступного разу.",
     "Вернуть все окна игры на свои места? Каждое встанет на место, когда откроется в следующий раз."},
    {Text::GameSaved, "Saved: {1}", "Збережено: {1}", "Сохранено: {1}"},
    {Text::GameLoaded,
     "Loaded. The game takes it at its next start.", "Завантажено. Гра візьме це під час наступного запуску.",
     "Загружено. Игра возьмёт это при следующем запуске."},
    {Text::LayoutResetDone,
     "Done: the windows take their places when they open.", "Готово: вікна стануть на місця, коли відкриються.",
     "Готово: окна встанут на места, когда откроются."},
    {Text::ErrReadFile,
     "{1} holds no settings of the game.", "У {1} немає налаштувань гри.", "В {1} нет настроек игры."},
    {Text::FileFilterReg, "Game settings (*.reg)", "Налаштування гри (*.reg)", "Настройки игры (*.reg)"},
    {Text::DiagGpuValueCapped, "{1}, at least {2} MB (a 32-bit program sees no more), driver {3}",
     "{1}, щонайменше {2} МБ (32-бітна програма більше не бачить), драйвер {3}",
     "{1}, не меньше {2} МБ (32-битная программа больше не видит), драйвер {3}"},
    {Text::GameSettingsRunning,
     "Loading and the layout reset wait for the game to close: it rewrites its key when it exits.",
     "Завантаження й скидання розкладки — при закритій грі: вона перезаписує свій ключ при виході.",
     "Загрузка и сброс раскладки — при закрытой игре: она перезаписывает свой ключ при выходе."},
    {Text::ErrPartialFile,
     "{1} holds only a part of the game's key (a regedit export of one folder, say): loading it would replace all the "
     "rest. Such a file is added with a double-click (regedit).",
     "У {1} лише частина ключа гри (наприклад, вивантаження однієї теки з regedit): завантаження замінило б усе інше. "
     "Такий файл додають подвійним клацанням (regedit).",
     "В {1} только часть ключа игры (например, выгрузка одной папки из regedit): загрузка заменила бы всё остальное. "
     "Такой файл добавляют двойным щелчком (regedit)."},
    {Text::PageSmoothing, "Smoothing", "Згладжування", "Сглаживание"},
    {Text::SmoothMsaa, "Edge smoothing (MSAA)", "Згладжування країв (MSAA)", "Сглаживание краёв (MSAA)"},
    {Text::SmoothMsaaHint,
     "Smooths the jagged edges of models, walls and the ground. 4× is a good balance; 8× needs more video memory.",
     "Згладжує зубчасті краї моделей, стін і землі. 4× — добрий баланс; 8× потребує більше відеопам'яті.",
     "Сглаживает зубчатые края моделей, стен и земли. 4× — хороший баланс; 8× требует больше видеопамяти."},
    {Text::SmoothA2c, "Smooth edges of leaves and fences", "Рівні краї листя й огорож", "Ровные края листвы и оград"},
    {Text::SmoothA2cHint,
     "With MSAA: the cut-out parts of models get soft edges instead of jagged ones (alpha to coverage).",
     "Разом з MSAA: вирізані частини моделей отримують м'які краї замість зубчастих (alpha to coverage).",
     "Вместе с MSAA: у вырезанных частей моделей края мягкие, а не зубчатые (alpha to coverage)."},
    {Text::SmoothAniso, "Anisotropic filtering", "Анізотропна фільтрація", "Анизотропная фильтрация"},
    {Text::SmoothAnisoHint,
     "Keeps the ground and walls sharp at a slant and into the distance. Best with the game's mipmaps on the Quality "
     "page.",
     "Земля й стіни лишаються чіткими під кутом і вдалині. Найкраще разом з міп-рівнями гри в «Якості».",
     "Земля и стены остаются чёткими под углом и вдали. Лучше всего вместе с мип-уровнями игры в «Качестве»."},
    {Text::SmoothOff, "Off", "Вимкнено", "Выключено"},
    {Text::Factor2, "2×", "2×", "2×"},
    {Text::Factor4, "4×", "4×", "4×"},
    {Text::Factor8, "8×", "8×", "8×"},
    {Text::Factor16, "16×", "16×", "16×"},
    {Text::RenderBridge, "64-bit renderer",
     "64-бітний рендер",
     "64-битный рендер"},
    {Text::RenderBridgeHint,
     "Draws in a separate 64-bit process, uaro-render64.exe: DLSS and frame generation need it. The picture should "
     "look the same.",
     "Малює в окремому 64-бітному процесі uaro-render64.exe: він потрібен для DLSS і генерації кадрів. Картинка має "
     "бути та сама.",
     "Рисует в отдельном 64-битном процессе uaro-render64.exe: он нужен для DLSS и генерации кадров. Картинка "
     "должна быть та же."},
    {Text::RenderThread, "Direct3D 11 on a thread of its own (test)", "Direct3D 11 у власному потоці (перевірка)",
     "Direct3D 11 в своём потоке (проверка)"},
    {Text::RenderThreadHint,
     "With Direct3D 11 the game's thread only writes down what to draw, and a thread of its own draws it on another "
     "core of the processor - as the 64-bit renderer does for Direct3D 12. More frames where the processor holds the "
     "game back: an older computer, many effects.",
     "З Direct3D 11 потік гри лише записує, що малювати, а малює власний потік на іншому ядрі процесора — як "
     "64-бітний рендер для Direct3D 12. Більше кадрів там, де гру стримує процесор: старіший комп'ютер, багато "
     "ефектів.",
     "С Direct3D 11 поток игры только записывает, что рисовать, а рисует собственный поток на другом ядре процессора "
     "— как 64-битный рендер для Direct3D 12. Больше кадров там, где игру сдерживает процессор: старый компьютер, "
     "много эффектов."},
    {Text::SmoothDlss, "NVIDIA DLSS", "NVIDIA DLSS", "NVIDIA DLSS"},
    {Text::SmoothDlssHint,
     "A technology of NVIDIA for GeForce RTX cards, in the 64-bit renderer (it starts by itself). DLAA smooths at "
     "full resolution; Quality ... Ultra draw the world smaller and rebuild it. Instead of MSAA.",
     "Технологія NVIDIA для карт GeForce RTX, у 64-бітному рендері (вмикається сам). DLAA згладжує в повній "
     "роздільності; «Якість» … «Ультра» малюють світ меншим і відновлюють його. Замість MSAA.",
     "Технология NVIDIA для карт GeForce RTX, в 64-битном рендере (включается сам). DLAA сглаживает в полном "
     "разрешении; «Качество» … «Ультра» рисуют мир меньше и восстанавливают его. Вместо MSAA."},
    {Text::DlssDlaa, "DLAA (smoothing only)", "DLAA (лише згладжування)", "DLAA (только сглаживание)"},
    {Text::DlssQuality, "Quality", "Якість", "Качество"},
    {Text::DlssBalanced, "Balanced", "Баланс", "Баланс"},
    {Text::DlssPerformance, "Performance", "Продуктивність", "Производительность"},
    {Text::DlssUltra, "Ultra performance", "Ультрапродуктивність", "Ультрапроизводительность"},
    {Text::FramesGeneration,
     "NVIDIA DLSS frame generation", "Генерація кадрів NVIDIA DLSS", "Генерация кадров NVIDIA DLSS"},
    {Text::FramesGenerationHint,
     "Adds made-up frames between the game's: GeForce RTX 40 and 50 (3x and 4x: RTX 50), only with DLSS on the "
     "Smoothing page. Smoother movement of the world, a little more delay; off in menus by itself. The frames shown "
     "stay under the monitor's refresh, so the game's own are at most the refresh divided by the factor (144 Hz: 2x - "
     "69 a second, 4x - 34); the cursor and the windows move with the game's own frames - 2x suits 144 Hz.",
     "Додає вигадані кадри між кадрами гри: GeForce RTX 40 і 50 (3x і 4x — RTX 50), лише з DLSS у «Згладжуванні». "
     "Плавніший рух світу, трохи більша затримка; у меню вимикається сама. Показаних кадрів не більше за частоту "
     "монітора, тож власних кадрів гри щонайбільше вона, поділена на множник (144 Гц: 2x — 69 на секунду, 4x — 34); "
     "курсор і вікна рухаються з кадрами гри — для 144 Гц підходить 2x.",
     "Добавляет придуманные кадры между кадрами игры: GeForce RTX 40 и 50 (3x и 4x — RTX 50), только с DLSS в "
     "«Сглаживании». Плавнее движение мира, чуть больше задержка; в меню выключается сама. Показанных кадров не "
     "больше частоты монитора, поэтому собственных кадров игры не больше неё, делённой на множитель (144 Гц: 2x — 69 "
     "в секунду, 4x — 34); курсор и окна движутся с кадрами игры — для 144 Гц подходит 2x."},
    {Text::Times3, "3×", "3×", "3×"},
    {Text::AiEnable,
     "HD textures from the texture pack (uaro-textures.zip)", "HD-текстури з пакета текстур (uaro-textures.zip)",
     "HD-текстуры из пакета текстур (uaro-textures.zip)"},
    {Text::AiEnableHint,
     "The game's textures redrawn larger and sharper by a neural network (Real-ESRGAN): the ready pictures of the "
     "texture pack uaro-textures.zip, which the Texture pack page makes once on a graphics card with Direct3D 12 - a "
     "player's card makes nothing. Pause shows the game's own. Direct3D 12 draws them in the 64-bit renderer, "
     "Direct3D 11 in the game itself.",
     "Текстури гри, перемальовані нейромережею (Real-ESRGAN) більшими й чіткішими: готові картинки пакета текстур "
     "uaro-textures.zip, який один раз створює вкладка «Пакет текстур» на відеокарті з Direct3D 12, — відеокарта "
     "гравця нічого не створює. Pause показує власні текстури гри. Direct3D 12 малює їх у 64-бітному рендері, "
     "Direct3D 11 — у самій грі.",
     "Текстуры игры, перерисованные нейросетью (Real-ESRGAN) крупнее и чётче: готовые картинки пакета текстур "
     "uaro-textures.zip, который один раз создаёт вкладка «Пакет текстур» на видеокарте с Direct3D 12, — видеокарта "
     "игрока ничего не создаёт. Pause показывает родные текстуры игры. Direct3D 12 рисует их в 64-битном рендере, "
     "Direct3D 11 — в самой игре."},
    {Text::AiBudget, "GPU time a frame, ms", "Час відеокарти на кадр, мс", "Время видеокарты на кадр, мс"},
    {Text::AiBudgetHint,
     "How much of each frame the neural network may take, 1-20 ms: more makes textures faster, less keeps the "
     "frame rate.",
     "Скільки часу кожного кадру може брати нейромережа, 1–20 мс: більше — текстури швидше, менше — FPS вищий.",
     "Сколько времени каждого кадра может брать нейросеть, 1–20 мс: больше — текстуры быстрее, меньше — FPS выше."},
    {Text::AiDenoise, "Grain smoothing", "Згладжування зерна", "Сглаживание зерна"},
    {Text::AiDenoiseHint,
     "How much of the textures' grain the realistic model smooths away: 0 keeps all of it (sand and grass stay "
     "crisp), 100 makes them smooth as if painted. Walls and floors look alike at any value. Textures made at "
     "another value are made again.",
     "Скільки зерна текстур згладжує реалістична модель: 0 — лишає все (пісок і трава чіткі), 100 — гладкі, наче "
     "намальовані. Стіни й підлога майже однакові за будь-якого значення. Текстури, зроблені з іншим значенням, "
     "робляться знову.",
     "Сколько зерна текстур сглаживает реалистичная модель: 0 — оставляет всё (песок и трава чёткие), 100 — "
     "гладкие, будто нарисованные. Стены и пол почти одинаковы при любом значении. Текстуры, сделанные с другим "
     "значением, делаются заново."},
    {Text::RenderRemix, "RTX Remix (experiment)", "RTX Remix (експеримент)", "RTX Remix (эксперимент)"},
    {Text::RenderRemixHint,
     "Draws through Direct3D 9 for NVIDIA RTX Remix: path tracing, and DLSS 5 once NVIDIA adds it to Remix "
     "(announced for 23 October 2026). Needs RTX Remix's d3d9.dll and .trex folder next to uaRO.exe and rtx.conf "
     "from uaro-graphics\\remix copied there; Alt+X opens Remix's menu. Without them the game draws with "
     "Direct3D 12 as usual.",
     "Малює через Direct3D 9 для NVIDIA RTX Remix: трасування шляхів, а DLSS 5 — коли NVIDIA додасть його в Remix "
     "(обіцяно 23 жовтня 2026). Потрібні d3d9.dll і тека .trex з RTX Remix поруч з uaRO.exe і туди ж rtx.conf з "
     "uaro-graphics\\remix; Alt+X відкриває меню Remix. Без них гра малює через Direct3D 12, як завжди.",
     "Рисует через Direct3D 9 для NVIDIA RTX Remix: трассировка путей, а DLSS 5 — когда NVIDIA добавит его в Remix "
     "(обещано 23 октября 2026). Нужны d3d9.dll и папка .trex из RTX Remix рядом с uaRO.exe и туда же rtx.conf "
     "из uaro-graphics\\remix; Alt+X открывает меню Remix. Без них игра рисует через Direct3D 12, как обычно."},
    {Text::PageAi, "AI textures", "Нейромережа", "Нейросеть"},
    {Text::PageAiCache, "Ready textures", "Готові текстури", "Готовые текстуры"},
    {Text::AiPreset, "Set", "Набір", "Набор"},
    {Text::AiPresetHint,
     "Recommended works on the ground and buildings: the safest choice. Everything takes characters and effects "
     "too. Change any setting below and the set becomes Custom.",
     "«Рекомендований» покращує землю й будівлі — найнадійніший вибір. «Усе» бере ще персонажів та ефекти. "
     "Змініть будь-що нижче — і набір стане «Свій».",
     "«Рекомендуемый» улучшает землю и здания — самый надёжный выбор. «Всё» берёт ещё персонажей и эффекты. "
     "Измените что-нибудь ниже — и набор станет «Свой»."},
    {Text::AiPresetRecommended,
     "Recommended: ground and buildings", "Рекомендований: земля й будівлі", "Рекомендуемый: земля и здания"},
    {Text::AiPresetAll,
     "Everything: characters and effects too", "Усе: і персонажі, і ефекти", "Всё: и персонажи, и эффекты"},
    {Text::AiPresetCustom, "Custom", "Свої", "Свои"},
    {Text::AiWhat, "What to improve:", "Що покращувати:", "Что улучшать:"},
    {Text::AiGround, "Map ground", "Земля карт", "Земля карт"},
    {Text::AiGroundHint,
     "Sand, grass, paving, roads: everything you walk on.", "Пісок, трава, плити, дороги: усе, чим ходять.",
     "Песок, трава, плиты, дороги: всё, по чему ходят."},
    {Text::AiObjects, "Buildings, trees and water", "Будівлі, дерева й вода", "Здания, деревья и вода"},
    {Text::AiObjectsHint, "Everything else that stands on a map: houses, walls, trees, rocks, water.",
     "Усе інше, що стоїть на карті: будинки, стіни, дерева, каміння, вода.",
     "Всё остальное, что стоит на карте: дома, стены, деревья, камни, вода."},
    {Text::AiSprites, "Monsters", "Монстри", "Монстры"},
    {Text::AiSpritesHint,
     "Monsters, pets, homunculi and mercenaries: their animation frames from the texture pack, one by one as the game "
     "draws them. Small pixel pictures: the network may change their look - try it and see.",
     "Монстри, вихованці, гомункули й найманці: кадри їхніх анімацій із пакета текстур, по одному, щойно гра їх "
     "малює. Це маленькі піксельні картинки: нейромережа може змінити їхній вигляд — спробуйте й подивіться.",
     "Монстры, питомцы, гомункулы и наёмники: кадры их анимаций из пакета текстур, по одному, как только игра их "
     "рисует. Это маленькие пиксельные картинки: нейросеть может изменить их вид — попробуйте и посмотрите."},
    {Text::AiEffects, "Skill effects", "Ефекти вмінь", "Эффекты умений"},
    {Text::AiEffectsHint,
     "Glows, sparks and light added onto the picture. Usually small and quick: the difference is little.",
     "Сяйва, іскри й світло, що додаються до картинки. Зазвичай маленькі й швидкі: різниця невелика.",
     "Свечения, искры и свет, которые добавляются к картинке. Обычно маленькие и быстрые: разница небольшая."},
    {Text::AiModelWorld, "Model for maps", "Модель для карт", "Модель для карт"},
    {Text::AiModelWorldHint,
     "For the ground and buildings. Realistic keeps the texture's own detail; Drawn makes it smooth and clean like a "
     "painting and is twice as fast.",
     "Для землі й будівель. «Реалістична» зберігає власні деталі текстури; «Мальована» робить її гладкою й чистою, "
     "як малюнок, і вдвічі швидша.",
     "Для земли и зданий. «Реалистичная» сохраняет собственные детали текстуры; «Рисованная» делает её гладкой и "
     "чистой, как рисунок, и вдвое быстрее."},
    {Text::AiModelSprites, "Model for characters", "Модель для персонажів", "Модель для персонажей"},
    {Text::AiModelSpritesHint,
     "For characters, monsters, NPCs, items, effects and icons. Drawn (made for anime) keeps clean lines and flat "
     "colours.",
     "Для персонажів, монстрів, NPC, речей, ефектів та іконок. «Мальована» (зроблена для аніме) зберігає чисті лінії "
     "й рівні кольори.",
     "Для персонажей, монстров, NPC, вещей, эффектов и иконок. «Рисованная» (сделана для аниме) сохраняет чистые "
     "линии и ровные цвета."},
    {Text::AiModelGeneral, "Realistic (general-x4v3)", "Реалістична (general-x4v3)", "Реалистичная (general-x4v3)"},
    {Text::AiModelAnime, "Drawn (animevideov3)", "Мальована (animevideov3)", "Рисованная (animevideov3)"},
    {Text::AiScale, "Result size", "Розмір результату", "Размер результата"},
    {Text::AiScaleHint,
     "×4: a 256×256 texture becomes 1024×1024, about 5.6 MB of video memory with its smaller copies. ×2: 512×512, "
     "about 1.4 MB - for cards with 4 GB or less (the pack's pictures are taken at half their size).",
     "×4: текстура 256×256 стає 1024×1024 — близько 5,6 МБ відеопам'яті з меншими копіями. ×2: 512×512, близько 1,4 "
     "МБ — для відеокарт із 4 ГБ і менше (картинки пакета беруться вдвічі меншими).",
     "×4: текстура 256×256 становится 1024×1024 — около 5,6 МБ видеопамяти с уменьшенными копиями. ×2: 512×512, около "
     "1,4 МБ — для видеокарт с 4 ГБ и меньше (картинки пакета берутся вдвое меньшими)."},
    {Text::AiScale4, "×4 - the sharpest", "×4 — найчіткіші", "×4 — самые чёткие"},
    {Text::AiScale2, "×2 - a quarter of the memory", "×2 — учетверо менше пам'яті", "×2 — вчетверо меньше памяти"},
    {Text::AiSizes, "Texture sides, from - to", "Сторони текстур, від — до", "Стороны текстур, от — до"},
    {Text::AiSizesHint,
     "Which textures of the world it takes, by their sides in pixels (16-1024). From 16: shadows, the frame under "
     "the cursor, the small things of a map too. Over 512 needs much video memory: the network takes about 0.5 GB "
     "for 1024×1024.",
     "Які текстури світу брати — за їхніми сторонами в пікселях (16–1024). Від 16 — також тіні, рамка під курсором, "
     "дрібниці карти. Понад 512 потребує багато відеопам'яті: нейромережі треба близько 0,5 ГБ на 1024×1024.",
     "Какие текстуры мира брать — по их сторонам в пикселях (16–1024). От 16 — также тени, рамка под курсором, "
     "мелочи карты. Больше 512 требует много видеопамяти: нейросети нужно около 0,5 ГБ на 1024×1024."},
    {Text::AiAhead, "Prepare a map's known textures in advance", "Заздалегідь готувати знайомі текстури карти",
     "Заранее готовить знакомые текстуры карты"},
    {Text::AiAheadHint,
     "A map's textures the texture pack knows are taken from it as soon as the map loads, not when they come into "
     "view: fewer of them sharpen before your eyes.",
     "Текстури карти, які знає пакет текстур, беруться з нього щойно карта завантажилась, а не коли потраплять у "
     "кадр: менше «проявлення» на очах.",
     "Текстуры карты, которые знает пакет текстур, берутся из него сразу после загрузки карты, а не когда попадут в "
     "кадр: меньше «проявления» на глазах."},
    {Text::AiOffNote,
     "Tick the box above and the network starts improving textures while you play. The Ready textures page shows "
     "and deletes what it has made.",
     "Поставте галочку вище — і нейромережа почне покращувати текстури, поки ви граєте. Сторінка «Готові текстури» "
     "показує й видаляє зроблене.",
     "Поставьте галочку выше — и нейросеть начнёт улучшать текстуры, пока вы играете. Страница «Готовые текстуры» "
     "показывает и удаляет сделанное."},
    {Text::AiCacheSummary, "The texture pack uaro-textures.zip: {1} pictures, {2} (the file: {3}).",
     "Пакет текстур uaro-textures.zip: картинок — {1}, {2} (файл: {3}).",
     "Пакет текстур uaro-textures.zip: картинок — {1}, {2} (файл: {3})."},
    {Text::AiCacheNone, "There is no texture pack (uaro-textures.zip) yet: the Texture pack page makes it.",
     "Пакета текстур (uaro-textures.zip) ще немає: його створює вкладка «Пакет текстур».",
     "Пакета текстур (uaro-textures.zip) пока нет: его создаёт вкладка «Пакет текстур»."},
    {Text::AiCacheUnreadable, "uaro-textures.zip cannot be read: {1}",
     "uaro-textures.zip не читається: {1}",
     "uaro-textures.zip не читается: {1}"},
    {Text::AiCacheMap, "Map", "Карта", "Карта"},
    {Text::AiCacheColGround, "Ground", "Земля", "Земля"},
    {Text::AiCacheColObjects, "Buildings", "Будівлі", "Здания"},
    {Text::AiCacheColSprites, "Characters", "Персонажі", "Персонажи"},
    {Text::AiCacheColEffects, "Effects", "Ефекти", "Эффекты"},
    {Text::AiCacheColTotal, "Total", "Усього", "Всего"},
    {Text::AiCacheColSize, "Size", "Розмір", "Размер"},
    {Text::AiCacheAllMaps, "All maps", "Усі карти", "Все карты"},
    {Text::AiCacheNoMap, "no map (the builder's; icons)",
     "без карти (від збирача; іконки)",
     "без карты (от сборщика; иконки)"},
    {Text::AiCacheDeleteKinds,
     "Delete from the ticked maps:", "Видаляти з позначених карт:", "Удалять с отмеченных карт:"},
    {Text::AiCacheDelete, "Delete ticked", "Видалити позначене", "Удалить отмеченное"},
    {Text::AiCacheDeleteOld, "Delete the old upscaled.grf ({1})",
     "Видалити старий upscaled.grf ({1})",
     "Удалить старый upscaled.grf ({1})"},
    {Text::AiCacheDeleteAll, "Delete all", "Видалити все", "Удалить всё"},
    {Text::AiCacheOldNote,
     "The pictures the pack's builder made are of no map; the maps are noted when the game makes pictures itself "
     "(making new ones while playing). Deleted pictures are made again by the Texture pack page. upscaled.grf "
     "(0.11-0.47) is not used any more.",
     "Картинки, які створив збирач пакета, без карти; карти записуються, коли картинки створює сама гра (створення "
     "нових під час гри). Видалені картинки знову створює вкладка «Пакет текстур». upscaled.grf (0.11-0.47) більше не "
     "використовується.",
     "Картинки, которые создал сборщик пакета, без карты; карты записываются, когда картинки создаёт сама игра "
     "(создание новых во время игры). Удалённые картинки снова создаёт вкладка «Пакет текстур». upscaled.grf "
     "(0.11-0.47) больше не используется."},
    {Text::AiCacheRunning,
     "The game or the pack's builder is running: pictures can be deleted once it is closed (it keeps the pack open).",
     "Гра або збирач пакета запущені: видаляти можна, коли вони закриті (вони тримають пакет відкритим).",
     "Игра или сборщик пакета запущены: удалять можно, когда они закрыты (они держат пакет открытым)."},
    {Text::AiCacheAskDelete,
     "Delete the ticked pictures from the texture pack ({1})? The Texture pack page makes them again.",
     "Видалити позначені картинки з пакета текстур ({1} шт.)? Вкладка «Пакет текстур» створить їх знову.",
     "Удалить отмеченные картинки из пакета текстур ({1} шт.)? Вкладка «Пакет текстур» создаст их снова."},
    {Text::AiCacheAskAll, "Delete the whole texture pack ({1})? The Texture pack page makes it again.",
     "Видалити весь пакет текстур ({1})? Вкладка «Пакет текстур» створить його знову.",
     "Удалить весь пакет текстур ({1})? Вкладка «Пакет текстур» создаст его снова."},
    {Text::AiCacheDeleted, "Deleted pictures: {1}. The texture pack is {2} now.",
     "Видалено картинок: {1}. Пакет текстур тепер {2}.",
     "Удалено картинок: {1}. Пакет текстур теперь {2}."},
    {Text::AiCacheDeletedAll, "The texture pack is deleted.",
     "Пакет текстур видалено.",
     "Пакет текстур удалён."},
    {Text::AiCacheNothing,
     "Nothing to delete: tick maps in the table first.", "Нічого видаляти: спершу позначте карти в таблиці.",
     "Нечего удалять: сначала отметьте карты в таблице."},
    {Text::AiCacheError, "The texture pack could not be changed: {1}",
     "Не вдалося змінити пакет текстур: {1}",
     "Не удалось изменить пакет текстур: {1}"},
    {Text::SizeMb, "{1} MB", "{1} МБ", "{1} МБ"},
    {Text::SizeGb, "{1} GB", "{1} ГБ", "{1} ГБ"},
    {Text::AiIcons, "Interface: icons, pictures, loading screen", "Інтерфейс: іконки, картинки, заставка",
     "Интерфейс: иконки, картинки, заставка"},
    {Text::AiIconsHint,
     "Items and skills in the windows, an item's picture in its description, cards' and NPCs' illustrations: smooth "
     "at the window's real pixels instead of in squares (needs crisp text, Quality page). The loading screen and the "
     "login's backgrounds too. The model for characters draws them; the mouse cursor comes with the characters' box.",
     "Речі й уміння у вікнах, картинка речі в її описі, ілюстрації карт і NPC: гладко в реальних пікселях вікна, а "
     "не квадратами (потрібен чіткий текст, сторінка «Якість»). А також заставка завантаження й тло входу. Їх малює "
     "модель для персонажів; курсор миші — разом із галочкою персонажів.",
     "Вещи и умения в окнах, картинка вещи в её описании, иллюстрации карт и NPC: гладко в настоящих пикселях окна, "
     "а не квадратами (нужен чёткий текст, страница «Качество»). А также заставка загрузки и фон входа. Их рисует "
     "модель для персонажей; курсор мыши — вместе с галочкой персонажей."},
    {Text::AiCacheColIcons, "Interface", "Інтерфейс", "Интерфейс"},
    {Text::AiUiPieces, "Window frames and buttons", "Рамки й кнопки вікон", "Рамки и кнопки окон"},
    {Text::AiUiPiecesHint,
     "With the interface's box: the windows' small pieces too - buttons, tabs, slots, arrows. The words on buttons are "
     "pictures and get smoothed too: should they look blurred, untick this and keep the icons.",
     "Разом із галочкою інтерфейсу: дрібні частини вікон — кнопки, вкладки, комірки, стрілки. Написи на кнопках — це "
     "картинки, вони теж згладжуються: якщо здадуться розмитими, зніміть цю галочку, іконки лишаться.",
     "Вместе с галочкой интерфейса: мелкие части окон — кнопки, вкладки, ячейки, стрелки. Надписи на кнопках — это "
     "картинки, они тоже сглаживаются: если покажутся размытыми, снимите эту галочку, иконки останутся."},
    {Text::AiPlayers, "Players", "Гравці", "Игроки"},
    {Text::AiPlayersHint,
     "Characters of players, your own too: bodies, heads, headgear, weapons, shields, garments - every frame by "
     "itself.",
     "Персонажі гравців, ваш теж: тіла, голови, головні убори, зброя, щити, плащі — кожен кадр окремо.",
     "Персонажи игроков, ваш тоже: тела, головы, головные уборы, оружие, щиты, плащи — каждый кадр отдельно."},
    {Text::AiNpcs, "NPCs", "NPC", "NPC"},
    {Text::AiNpcsHint, "The game's characters in towns and dungeons: Kafra, merchants, guards.",
     "Персонажі гри в містах і підземеллях: Кафра, торговці, вартові.",
     "Персонажи игры в городах и подземельях: Кафра, торговцы, стражники."},
    {Text::AiItems, "Items on the ground", "Речі на землі", "Вещи на земле"},
    {Text::AiItemsHint, "What lies on the ground: drops, loot.", "Те, що лежить на землі: здобич, луп.",
     "То, что лежит на земле: добыча, лут."},
    {Text::PageEffects, "Effects", "Ефекти", "Эффекты"},
    {Text::FxGlow, "Halo around fire and skills", "Ореол довкола вогню й умінь", "Ореол вокруг огня и умений"},
    {Text::FxGlowHint,
     "Fire, lightning, the light of skills, portals and lamps shine with a soft halo, as in modern games. The rest "
     "of the picture keeps its brightness.",
     "Вогонь, блискавки, світло вмінь, портали й ліхтарі світяться м'яким ореолом, як у сучасних іграх. Решта "
     "картинки не змінює яскравості.",
     "Огонь, молнии, свет умений, порталы и фонари светятся мягким ореолом, как в современных играх. Остальная "
     "картинка не меняет яркости."},
    {Text::FxGlowStrength, "Glow brightness", "Яскравість світіння", "Яркость свечения"},
    {Text::FxGlowStrengthHint, "How bright the halo is: 100 % as intended, 200 % twice as bright, 0 % none.",
     "Наскільки яскравий ореол: 100 % — як задумано, 200 % — удвічі яскравіше, 0 % — без нього.",
     "Насколько ярок ореол: 100 % — как задумано, 200 % — вдвое ярче, 0 % — без него."},
    {Text::FxGlowSize, "Halo size", "Розмір ореолу", "Размер ореола"},
    {Text::FxGlowSizeHint, "How far the glow spreads around an effect: less is a tight glow, more a wide haze.",
     "Як далеко світіння розходиться навколо ефекту: менше — щільне сяйво, більше — широкий серпанок.",
     "Как далеко свечение расходится вокруг эффекта: меньше — плотное сияние, больше — широкая дымка."},
    {Text::FxWater, "Modern water", "Сучасна вода", "Современная вода"},
    {Text::FxWaterHint,
     "The maps' water drawn anew: waves, reflections of the shore, trees and characters, the sun's glints, the bed "
     "seen through the water and darker where it is deep, foam along the shore. Lava stays as it is.",
     "Вода на картах малюється заново: хвилі, відображення берега, дерев і персонажів, відблиски сонця, дно крізь "
     "воду, темніше там, де глибоко, піна вздовж берега. Лава лишається як є.",
     "Вода на картах рисуется заново: волны, отражения берега, деревьев и персонажей, блики солнца, дно сквозь воду, "
     "темнее там, где глубоко, пена вдоль берега. Лава остаётся как есть."},
    {Text::FxWaterWaves, "Waves", "Хвилі", "Волны"},
    {Text::FxWaterWavesHint, "How rough the water is: 0 % a still mirror, 100 % wind-driven ripples.",
     "Наскільки неспокійна вода: 0 % — гладке дзеркало, 100 % — брижі від вітру.",
     "Насколько неспокойна вода: 0 % — гладкое зеркало, 100 % — рябь от ветра."},
    {Text::FxWaterReflections, "Reflections", "Відображення", "Отражения"},
    {Text::FxWaterReflectionsHint, "How much the water reflects the sky and what stands at its edge.",
     "Наскільки вода відображає небо і те, що стоїть на її березі.",
     "Насколько вода отражает небо и то, что стоит у её берега."},
    {Text::FxWaterClarity, "Clarity", "Прозорість", "Прозрачность"},
    {Text::FxWaterClarityHint, "How deep one sees into the water: less is murky, more is clear to the bottom.",
     "Як глибоко видно у воду: менше — каламутна, більше — прозора до дна.",
     "Как глубоко видно в воду: меньше — мутная, больше — прозрачная до дна."},
    {Text::FxGlowLight, "Light on the surroundings", "Світло на оточення", "Свет на окружение"},
    {Text::FxGlowLightHint,
     "How much the light of effects falls on the ground, walls and characters near them, in its own colour: a "
     "fireball lights the street. 0 % is the halo alone.",
     "Наскільки світло ефектів падає на землю, стіни й персонажів поруч, у своєму кольорі: вогняна куля освітлює "
     "вулицю. 0 % — лише ореол.",
     "Насколько свет эффектов падает на землю, стены и персонажей рядом, в своём цвете: огненный шар освещает "
     "улицу. 0 % — только ореол."},
    {Text::FxAo, "Shadows in corners and at walls", "Тіні в кутах і біля стін", "Тени в углах и у стен"},
    {Text::FxAoHint,
     "Soft shade where surfaces meet: at the foot of walls, in the corners of buildings, under trees and rocks, as in "
     "modern games (ambient occlusion). The characters and the effects keep their light.",
     "М'яка тінь там, де поверхні сходяться: біля стін, у кутах будівель, під деревами й каменями, як у сучасних "
     "іграх (ambient occlusion). Персонажі й ефекти не затіняються.",
     "Мягкая тень там, где поверхности сходятся: у стен, в углах зданий, под деревьями и камнями, как в современных "
     "играх (ambient occlusion). Персонажи и эффекты не затеняются."},
    {Text::FxAoStrength, "Shadow strength", "Сила тіней", "Сила теней"},
    {Text::FxAoStrengthHint,
     "How dark the shade is in the deepest corners.", "Наскільки темна тінь у найглибших кутах.",
     "Насколько темна тень в самых глубоких углах."},
    {Text::FxAoSize, "Shadow size", "Розмір тіней", "Размер теней"},
    {Text::FxAoSizeHint,
     "How far the shade reaches from a corner: less hugs the edges, more spreads under trees and between buildings.",
     "Як далеко тінь сягає від кута: менше — тонко вздовж країв, більше — широко під деревами й між будівлями.",
     "Как далеко тень заходит от угла: меньше — тонко вдоль краёв, больше — широко под деревьями и между "
     "зданиями."},
    {Text::FxGrade, "Colour", "Колір", "Цветокоррекция"},
    {Text::FxGradeHint,
     "The picture's colours to taste: richer or calmer, more contrast, brighter, warmer or colder, darker corners. "
     "The interface keeps its colours.",
     "Кольори картинки на смак: соковитіші чи спокійніші, контрастніші, яскравіші, тепліші чи холодніші, темніші "
     "кути. Інтерфейс не змінюється.",
     "Цвета картинки по вкусу: сочнее или спокойнее, контрастнее, ярче, теплее или холоднее, темнее по углам. "
     "Интерфейс не меняется."},
    {Text::FxGradeSaturation, "Saturation", "Насиченість", "Насыщенность"},
    {Text::FxGradeSaturationHint, "100 % as the game, more is richer colour, less paler, 0 % black and white.",
     "100 % — як у грі, більше — соковитіші кольори, менше — блідіші, 0 % — чорно-біла.",
     "100 % — как в игре, больше — сочнее цвета, меньше — бледнее, 0 % — чёрно-белая."},
    {Text::FxGradeContrast, "Contrast", "Контраст", "Контраст"},
    {Text::FxGradeContrastHint,
     "100 % as the game; more deepens the shadows and lifts the light parts, without losing them.",
     "100 % — як у грі; більше — глибші тіні й світліші світлі місця, без втрати деталей.",
     "100 % — как в игре; больше — глубже тени и светлее светлые места, без потери деталей."},
    {Text::FxGradeBrightness, "Brightness", "Яскравість", "Яркость"},
    {Text::FxGradeBrightnessHint, "100 % as the game; more lifts the middle tones, the brightest stay as they are.",
     "100 % — як у грі; більше — світліші середні тони, найсвітліші лишаються як є.",
     "100 % — как в игре; больше — светлее средние тона, самые светлые остаются как есть."},
    {Text::FxGradeWarmth, "Warmth", "Тон", "Тон"},
    {Text::FxGradeWarmthHint, "50 % as the game; less is colder (bluer), more warmer (yellower).",
     "50 % — як у грі; менше — холодніше (синіше), більше — тепліше (жовтіше).",
     "50 % — как в игре; меньше — холоднее (синее), больше — теплее (желтее)."},
    {Text::FxGradeVignette, "Vignette", "Віньєтка", "Виньетка"},
    {Text::FxGradeVignetteHint, "The corners of the screen darker, as in films; 0 % none.",
     "Кути екрана темніші, як у кіно; 0 % — без неї.", "Углы экрана темнее, как в кино; 0 % — без неё."},
    {Text::FxSmoothLight, "Map light without steps", "Світло карти без сходинок", "Свет карты без ступенек"},
    {Text::FxSmoothLightHint,
     "The map's baked light - the pools of torches and lamps, the shadows of houses and trees on the ground - smooth "
     "instead of blotches with steps and dots: the game keeps it in a very coarse grid that shows its cells when "
     "enlarged.",
     "Запечене світло карти — плями від смолоскипів і ліхтарів, тіні будинків і дерев на землі — плавне замість "
     "плям зі сходинками й крапками: гра тримає його в дуже грубій сітці, яка при збільшенні показує свої клітинки.",
     "Запечённый свет карты — пятна от факелов и фонарей, тени домов и деревьев на земле — плавный вместо пятен со "
     "ступеньками и точками: игра хранит его в очень грубой сетке, которая при увеличении показывает свои клетки."},
    {Text::FxTorch, "Live torches", "Живі смолоскипи", "Живые факелы"},
    {Text::FxTorchHint,
     "The maps' torches drawn anew: a flame that burns, sways and throws sparks instead of the game's blurred drop, a "
     "soft glow in the air around it instead of the red blotch, and a warm flickering light on the ground and the "
     "walls.",
     "Смолоскипи на картах малюються наново: полум'я, що горить, гойдається й сипле іскрами, замість розмитої краплі "
     "гри; м'яке сяйво в повітрі довкола замість червоної плями; тепле мерехтливе світло на землі й стінах.",
     "Факелы на картах рисуются заново: пламя, которое горит, колышется и сыплет искрами, вместо размытой капли "
     "игры; мягкое свечение в воздухе вокруг вместо красной кляксы; тёплый мерцающий свет на земле и стенах."},
    {Text::FxTorchGlow, "Glow around the fire", "Сяйво довкола вогню", "Свечение вокруг огня"},
    {Text::FxTorchGlowHint, "How bright the soft glow in the air around the flame is; 0 % none.",
     "Наскільки яскраве м'яке сяйво в повітрі довкола полум'я; 0 % — без нього.",
     "Насколько яркое мягкое свечение в воздухе вокруг пламени; 0 % — без него."},
    {Text::FxTorchLight, "Light around", "Світло довкола", "Свет вокруг"},
    {Text::FxTorchLightHint,
     "How much warm light the torch throws on the ground, the walls and whoever stands near; 0 % none.",
     "Скільки теплого світла смолоскип кидає на землю, стіни й тих, хто поруч; 0 % — без нього.",
     "Сколько тёплого света факел бросает на землю, стены и тех, кто рядом; 0 % — без него."},
    {Text::FxTorchFlicker, "Flicker", "Мерехтіння", "Мерцание"},
    {Text::FxTorchFlickerHint,
     "How much the fire and its light flicker: 0 % burns evenly, 50 % calmly, 100 % a lively fire throwing its light "
     "about the walls.",
     "Наскільки мерехтять вогонь і його світло: 0 % — горить рівно, 50 % — спокійно, 100 % — жвавий вогонь, що "
     "кидає світло по стінах.",
     "Насколько мерцают огонь и его свет: 0 % — горит ровно, 50 % — спокойно, 100 % — живой огонь, который бросает "
     "свет по стенам."},
    {Text::FxTorchFlame, "New flame", "Новий вогонь", "Новый огонь"},
    {Text::FxTorchFlameHint,
     "A live flame instead of the game's blurred picture: tongues of fire tear upwards and throw sparks, each torch "
     "burning its own way. Off: the game's own flame, with the new glow and light around it.",
     "Живе полум'я замість розмитої картинки гри: язики вогню рвуться вгору й сиплять іскрами, кожен смолоскип горить "
     "по-своєму. Вимкнено: вогонь гри, а сяйво й світло довкола — нові.",
     "Живое пламя вместо размытой картинки игры: языки огня рвутся вверх и сыплют искрами, каждый факел горит "
     "по-своему. Выключено: огонь игры, а свечение и свет вокруг — новые."},
    {Text::FxTorchBrightness, "Flame brightness", "Яскравість вогню", "Яркость огня"},
    {Text::FxTorchBrightnessHint,
     "How bright and dense the new flame burns: less is softer and more see-through, more is hotter.",
     "Наскільки яскраво й щільно горить новий вогонь: менше — м'якше й прозоріше, більше — гарячіше.",
     "Насколько ярко и плотно горит новый огонь: меньше — мягче и прозрачнее, больше — жарче."},
    {Text::FxSmoke, "Chimney smoke", "Дим із труб", "Дым из труб"},
    {Text::FxSmokeHint,
     "Each puff the chimneys send up drawn as billowing smoke - wisps and holes that change as it rises - instead of "
     "the same small picture blown up.",
     "Кожен клуб диму з труб малюється клубчастим димом — пасма й просвіти, що змінюються, поки він піднімається, — "
     "замість тієї самої маленької розтягнутої картинки.",
     "Каждый клуб дыма из труб рисуется клубящимся дымом — пряди и просветы, которые меняются, пока он поднимается, "
     "— вместо одной и той же маленькой растянутой картинки."},
    {Text::FxSmokeDensity, "Smoke density", "Густота диму", "Густота дыма"},
    {Text::FxSmokeDensityHint, "How thick the smoke is: less is thin and see-through, more is dense.",
     "Наскільки густий дим: менше — тонкий і прозорий, більше — щільний.",
     "Насколько густой дым: меньше — тонкий и прозрачный, больше — плотный."},
    {Text::FxLamps, "Lamps' light", "Світло ламп", "Свет ламп"},
    {Text::FxLampsHint,
     "The lights the maps' makers placed - lamps, braziers, crystals - light the characters and the walls near them "
     "too, not only the ground, and the fire-coloured ones flicker. Read from the map files; the first visit to a map "
     "with torches teaches the driver how they lie.",
     "Світло, яке розставили автори карт, — лампи, жаровні, кристали — освітлює й персонажів та стіни поруч, а не "
     "лише землю, а вогняне мерехтить. Береться з файлів карт; перший візит на карту зі смолоскипами вчить драйвер, "
     "як вони лягають.",
     "Свет, который расставили авторы карт, — лампы, жаровни, кристаллы — освещает и персонажей и стены рядом, а не "
     "только землю, а огненный мерцает. Берётся из файлов карт; первый заход на карту с факелами учит драйвер, как "
     "они ложатся."},
    {Text::FxLampsLight, "Lamps' brightness", "Яскравість ламп", "Яркость ламп"},
    {Text::FxLampsLightHint, "How much light the lamps add: less is a hint, more is bright.",
     "Скільки світла додають лампи: менше — ледь помітно, більше — яскраво.",
     "Сколько света добавляют лампы: меньше — едва заметно, больше — ярко."},
    {Text::FxFireLight, "Fire of skills lights around", "Вогонь умінь світить довкола", "Огонь умений светит вокруг"},
    {Text::FxFireLightHint,
     "Fire walls, fire bolts, meteors and other fire-coloured effects light the ground, the walls and the characters "
     "around them a little, as a torch does - with the depth of the scene, never washing it out.",
     "Вогняні стіни, вогняні стріли, метеори та інші вогняні ефекти трохи освітлюють землю, стіни й персонажів "
     "довкола, як смолоскип, — з глибиною сцени, без засвічення.",
     "Огненные стены, огненные стрелы, метеоры и другие огненные эффекты немного освещают землю, стены и персонажей "
     "вокруг, как факел, — с глубиной сцены, без пересвета."},
    {Text::FxFireLightStrength, "Fire light", "Світло вогню", "Свет огня"},
    {Text::FxFireLightStrengthHint,
     "How much the fire of skills lights around it.", "Наскільки вогонь умінь освітлює довкола.",
     "Насколько огонь умений освещает всё вокруг."},
    {Text::FxAura, "Softer level 99 aura", "М'якша аура 99 рівня", "Мягкая аура 99 уровня"},
    {Text::FxAuraHint,
     "The level 99 aura shines softer and does not dazzle around the characters. The game shows the simplified "
     "aura (the circle); its /aura command turns on the full one (the ring and bubbles too) - after a teleport or a "
     "new map.",
     "Аура 99 рівня світить м'якше й не сліпить довкола персонажів. Гра показує спрощену ауру (коло); її команда "
     "/aura вмикає повну (ще кільце й бульбашки) — після телепорту чи нової карти.",
     "Аура 99 уровня светит мягче и не слепит вокруг персонажей. Игра показывает упрощённую ауру (круг); её команда "
     "/aura включает полную (ещё кольцо и пузырьки) — после телепорта или новой карты."},
    {Text::FxAuraBrightness, "Aura brightness", "Яскравість аури", "Яркость ауры"},
    {Text::FxAuraBrightnessHint, "How bright the aura is, of what the game draws: 100 % as in the game, 0 % none.",
     "Наскільки яскрава аура від того, як її малює гра: 100 % — як у грі, 0 % — немає.",
     "Насколько ярка аура от того, как её рисует игра: 100 % — как в игре, 0 % — нет."},
    {Text::FxAuraPortals, "Aura does not hide portals", "Аура не закриває портали",
     "Аура не закрывает порталы"},  // 3.0.13-beta.15
    {Text::FxAuraPortalsHint,
     "A level 99 aura over a warp portal goes faint, the portal seen through it - with many auras at a portal it was "
     "lost under them.",
     "Аура 99 рівня над порталом стає блідою, і портал крізь неї видно: коли біля порталу багато аур, він губився під "
     "ними.",
     "Аура 99 уровня над порталом становится бледной, и портал сквозь неё виден: когда у портала много аур, он "
     "терялся под ними."},
    {Text::FxSilhouette, "Character's silhouette behind buildings", "Силует персонажа за будинками",
     "Силуэт персонажа за зданиями"},  // 3.0.13-beta.30
    {Text::FxSilhouetteHint,
     "Where a house, a tree or a wall hides your character, its shape shows over them in light blue - the buildings "
     "stay as they are.",
     "Там, де будинок, дерево чи стіна закривають вашого персонажа, його обрис видно поверх них блакитним - будинки "
     "лишаються як є.",
     "Там, где дом, дерево или стена закрывают вашего персонажа, его силуэт виден поверх них голубым - здания "
     "остаются как есть."},
    {Text::FxCrowdEffects, "Others' skill effects fainter", "Чужі ефекти навичок блідіші",
     "Чужие эффекты навыков бледнее"},  // 3.0.13-beta.20
    {Text::FxCrowdEffectsHint,
     "Skill effects on other players and their monsters at a third of their strength; yours and your party's as "
     "bright as ever - you see your own in a guild war or on the market.",
     "Ефекти навичок на інших гравцях і їхніх монстрах — на третину сили; ваші й вашої групи — як завжди яскраві: "
     "свої видно і на війні гільдій, і на ринку.",
     "Эффекты навыков на других игроках и их монстрах — на треть силы; ваши и вашей группы — как всегда яркие: "
     "свои видно и на войне гильдий, и на рынке."},
    {Text::FxMage, "Mages' new effects", "Нові ефекти магів", "Новые эффекты магов"},  // 3.0.15-beta.11
    {Text::FxMageHint,
     "The Mage's, Wizard's and Sage's skills in their own look: crisp, in the element's colours, no white blots; "
     "the boxes under it turn each part on and off.",
     "Навички мага, візарда й сейджа у власному вигляді: чіткі, в кольорах стихії, без білих плям; галочки під "
     "ним вмикають і вимикають кожну частину.",
     "Навыки мага, визарда и сейджа в собственном виде: чёткие, в цветах стихии, без белых пятен; галочки под "
     "ним включают и выключают каждую часть."},
    {Text::FxMageCasting, "Casting", "Каст", "Каст"},
    {Text::FxMageCastingHint,
     "A magic circle on the ground in the element's colour and a low crisp flame, in place of the column up the "
     "screen; at a ground skill's target the game's rings with a circle under them.",
     "Магічне коло на землі в кольорі стихії й невисоке чітке полум'я замість стовпа на весь екран; у точці цілі "
     "наземної навички - кільця гри з колом під ними.",
     "Магический круг на земле в цвете стихии и невысокое чёткое пламя вместо столба на весь экран; в точке цели "
     "наземного навыка - кольца игры с кругом под ними."},
    {Text::FxMageFire, "Fire", "Вогонь", "Огонь"},
    {Text::FxMageFireHint, "Fire Bolt, Fire Ball, Fire Wall, Fire Pillar, Meteor Storm, Sightrasher, Volcano.",
     "Fire Bolt, Fire Ball, Fire Wall, Fire Pillar, Meteor Storm, Sightrasher, Volcano.",
     "Fire Bolt, Fire Ball, Fire Wall, Fire Pillar, Meteor Storm, Sightrasher, Volcano."},
    {Text::FxMageWater, "Water and ice", "Вода й лід", "Вода и лёд"},
    {Text::FxMageWaterHint,
     "Cold Bolt, Frost Diver, Ice Wall, Storm Gust, Water Ball, Frost Nova, Deluge. Storm Gust's snowflakes drawn "
     "at the screen's resolution.",
     "Cold Bolt, Frost Diver, Ice Wall, Storm Gust, Water Ball, Frost Nova, Deluge. Сніжинки Storm Gust намальовано "
     "в роздільності екрана.",
     "Cold Bolt, Frost Diver, Ice Wall, Storm Gust, Water Ball, Frost Nova, Deluge. Снежинки Storm Gust нарисованы "
     "в разрешении экрана."},
    {Text::FxMageWind, "Wind", "Вітер", "Ветер"},
    {Text::FxMageWindHint, "Lightning Bolt, Thunderstorm, Jupitel Thunder, Lord of Vermilion, Violent Gale.",
     "Lightning Bolt, Thunderstorm, Jupitel Thunder, Lord of Vermilion, Violent Gale.",
     "Lightning Bolt, Thunderstorm, Jupitel Thunder, Lord of Vermilion, Violent Gale."},
    {Text::FxMageEarth, "Earth", "Земля", "Земля"},
    {Text::FxMageEarthHint, "Earth Spike, Heaven's Drive, Quagmire, Stone Curse, Land Protector.",
     "Earth Spike, Heaven's Drive, Quagmire, Stone Curse, Land Protector.",
     "Earth Spike, Heaven's Drive, Quagmire, Stone Curse, Land Protector."},
    {Text::FxMageSpirit, "Ghost and the rest", "Привид та інше", "Призрак и прочее"},
    {Text::FxMageSpiritHint, "Soul Strike, Napalm Beat, Safety Wall, Sight, Energy Coat, Dispel, Magic Rod.",
     "Soul Strike, Napalm Beat, Safety Wall, Sight, Energy Coat, Dispel, Magic Rod.",
     "Soul Strike, Napalm Beat, Safety Wall, Sight, Energy Coat, Dispel, Magic Rod."},
    {Text::FxMageSoft, "Soft look", "М'який вигляд", "Мягкий вид"},
    {Text::FxMageSoftHint,
     "All the classes' new skill effects soft: the game's own glow and see-through light, tinted a little in the "
     "element's colours and brighter, instead of crisp dense layers - Safety Wall, Sanctuary, Pneuma and the rest "
     "look lighter.",
     "Усі нові ефекти навичок м'якими: власне світіння гри й прозоре світло, трохи підфарбоване в кольори стихії й "
     "яскравіше, замість чітких щільних шарів — Safety Wall, Sanctuary, Pneuma та інші виглядають легше.",
     "Все новые эффекты навыков мягкими: собственное свечение игры и прозрачный свет, немного подкрашенный в цвета "
     "стихии и ярче, вместо чётких плотных слоёв — Safety Wall, Sanctuary, Pneuma и остальные выглядят легче."},
    // 3.0.18-beta.181: Storm Gust as the game's, its snow at real resolution
    {Text::FxCrispStormGust, "Storm Gust sharp, as the game's", "Storm Gust чітко, як у грі",
     "Storm Gust чётко, как в игре"},
    {Text::FxCrispStormGustHint,
     "Storm Gust in the game's own colours and light, its snowflakes drawn anew at the screen's resolution: each one "
     "sharp, in its place, size and brightness, instead of a stretched blot. Its cloud, whirl, lightning and ice stay "
     "the game's. Off: the mages' new look for it (when on), its snowflakes sharp there too.",
     "Storm Gust у власних кольорах і світлі гри, його сніжинки намальовано заново в роздільності екрана: кожна "
     "чітка, на своєму місці, свого розміру й яскравості, замість розтягнутої плями. Хмара, вихор, блискавки й лід — "
     "як у грі. Вимкнено: новий вигляд магів для нього (якщо увімкнено), сніжинки там теж чіткі.",
     "Storm Gust в собственных цветах и свете игры, его снежинки нарисованы заново в разрешении экрана: каждая "
     "чёткая, на своём месте, своего размера и яркости, вместо растянутого пятна. Облако, вихрь, молнии и лёд — как "
     "в игре. Выключено: новый вид магов для него (если включён), снежинки там тоже чёткие."},
    // 3.0.18-beta.183: every class's skills, one choice
    {Text::FxSkillLooks, "Skills' look", "Вигляд умінь", "Вид умений"},
    {Text::FxSkillLooksHint,
     "Every class's skills: as the game draws them; the game's own made sharp (the skills' snowflakes and sparkles "
     "drawn anew at the screen's resolution); redrawn in the classes' new looks; or redrawn and soft - the game's own "
     "light, tinted. The sparkles are sharp in the redrawn looks too.",
     "Уміння всіх професій: як їх малює гра; власний вигляд гри, тільки чіткий (сніжинки й іскри вмінь намальовано "
     "заново в роздільності екрана); перемальовані в новому вигляді професій; або перемальовані й м'які — власне "
     "світло гри, підфарбоване. Іскри чіткі й у перемальованих.",
     "Умения всех профессий: как их рисует игра; собственный вид игры, только чёткий (снежинки и искры умений "
     "нарисованы заново в разрешении экрана); перерисованные в новом виде профессий; или перерисованные и мягкие — "
     "собственный свет игры, подкрашенный. Искры чёткие и в перерисованных."},
    {Text::FxSkillLooksOriginal, "Original", "Оригінал", "Оригинал"},
    {Text::FxSkillLooksImproved, "Improved original", "Покращений оригінал", "Улучшенный оригинал"},
    {Text::FxSkillLooksRedrawn, "Redrawn", "Перемальовані", "Перерисованные"},
    {Text::FxSkillLooksSoft, "Redrawn, soft", "Перемальовані, м'які", "Перерисованные, мягкие"},
    {Text::FxSword, "Swordsmen's new effects", "Нові ефекти мечників", "Новые эффекты мечников"},
    {Text::FxSwordHint,
     "Swordsman, Knight, Crusader: Bash, Magnum Break, Bowling Bash, Brandish Spear, Grand Cross, Shield Boomerang.",
     "Мечник, лицар, хрестоносець: Bash, Magnum Break, Bowling Bash, Brandish Spear, Grand Cross, Shield Boomerang.",
     "Мечник, рыцарь, крестоносец: Bash, Magnum Break, Bowling Bash, Brandish Spear, Grand Cross, Shield Boomerang."},
    {Text::FxAcolyte, "Acolytes' new effects", "Нові ефекти аколітів", "Новые эффекты аколитов"},
    {Text::FxAcolyteHint,
     "Acolyte, Priest, Monk: Heal, Blessing, Magnus Exorcismus, Sanctuary, Kyrie, Asura Strike, the spirit spheres.",
     "Аколіт, прист, монах: Heal, Blessing, Magnus Exorcismus, Sanctuary, Kyrie, Asura Strike, сфери духу.",
     "Аколит, прист, монах: Heal, Blessing, Magnus Exorcismus, Sanctuary, Kyrie, Asura Strike, сферы духа."},
    {Text::FxArcher, "Archers' new effects", "Нові ефекти лучників", "Новые эффекты лучников"},
    {Text::FxArcherHint,
     "Archer, Hunter, Bard, Dancer: arrows, traps, Blitz Beat, Falcon Assault, the songs and dances.",
     "Лучник, мисливець, бард, танцівниця: стріли, пастки, Blitz Beat, Falcon Assault, пісні й танці.",
     "Лучник, охотник, бард, танцовщица: стрелы, ловушки, Blitz Beat, Falcon Assault, песни и танцы."},
    {Text::FxThief, "Thieves' new effects", "Нові ефекти злодіїв", "Новые эффекты воров"},
    {Text::FxThiefHint,
     "Thief, Assassin, Rogue: Sonic Blow, Grimtooth, Venom Dust, Enchant Poison, EDP, Steal, the strips.",
     "Злодій, асасин, розбійник: Sonic Blow, Grimtooth, Venom Dust, Enchant Poison, EDP, Steal, роздягання.",
     "Вор, ассасин, разбойник: Sonic Blow, Grimtooth, Venom Dust, Enchant Poison, EDP, Steal, раздевания."},
    {Text::FxMerchant, "Merchants' new effects", "Нові ефекти торговців", "Новые эффекты торговцев"},
    {Text::FxMerchantHint,
     "Merchant, Blacksmith, Alchemist: Mammonite, Cart Revolution, Over Thrust, Maximize Power, Acid Demonstration.",
     "Торговець, коваль, алхімік: Mammonite, Cart Revolution, Over Thrust, Maximize Power, Acid Demonstration.",
     "Торговец, кузнец, алхимик: Mammonite, Cart Revolution, Over Thrust, Maximize Power, Acid Demonstration."},
    {Text::FxOther, "Other classes' new effects", "Нові ефекти інших класів", "Новые эффекты других классов"},
    {Text::FxOtherHint,
     "Taekwon, Star Gladiator, Soul Linker, Ninja: the kicks, the links, the ninja's fire, water and thunder.",
     "Теквондо, зоряний гладіатор, соул-лінкер, ніндзя: удари ногами, лінки, вогонь, вода й грім ніндзя.",
     "Теквондо, звёздный гладиатор, соул-линкер, ниндзя: удары ногами, линки, огонь, вода и гром ниндзя."},
    {Text::FxHit, "New hits", "Нові удари", "Новые удары"},
    {Text::FxHitHint,
     "The sparks of the weapons' hits, the combos', the dark and holy hits: crisp, in the hit's colour.",
     "Іскри ударів зброєю, комбо, темних і святих ударів: чіткі, в кольорі удару.",
     "Искры ударов оружием, комбо, тёмных и святых ударов: чёткие, в цвете удара."},
    {Text::FxItem, "New potions, buffs and items", "Нові зілля, бафи й предмети", "Новые зелья, бафы и предметы"},
    {Text::FxItemHint,
     "Potions drunk, Concentration and Berserk potions, food, Kaizel, Kaahi, thrown items, cooking.",
     "Випиті зілля, Concentration і Berserk, їжа, Kaizel, Kaahi, кинуті предмети, готування.",
     "Выпитые зелья, Concentration и Berserk, еда, Kaizel, Kaahi, брошенные предметы, готовка."},
    {Text::FxStatus, "New states", "Нові стани", "Новые состояния"},
    {Text::FxStatusHint,
     "Frozen, stone, sleep, stun, curse, blind, poison and the body's colours of a state.",
     "Заморожений, камінь, сон, оглушення, прокляття, сліпота, отрута й кольори тіла від стану.",
     "Заморозка, камень, сон, оглушение, проклятие, слепота, яд и цвета тела от состояния."},
    {Text::FxTravel, "New teleport, levels and refining", "Нові телепорт, рівні й заточка",
     "Новые телепорт, уровни и заточка"},
    {Text::FxTravelHint,
     "Teleport, coming onto and leaving a map, Level Up, Job Level Up, a job changed, the refiner's success and "
     "failure, the MVP's.",
     "Телепорт, поява й вихід з карти, Level Up, Job Level Up, зміна професії, успіх і провал заточки, MVP.",
     "Телепорт, появление и уход с карты, Level Up, Job Level Up, смена профессии, успех и провал заточки, MVP."},
    {Text::FxMonster, "New monsters' and homunculi's effects", "Нові ефекти монстрів і гомункулів",
     "Новые эффекты монстров и гомункулов"},
    {Text::FxMonsterHint,
     "The monsters' skills (Dark Breath, Summon Slave, Blood Drain) and the homunculi's (Moonlight, Castling, "
     "Bloodlust).",
     "Навички монстрів (Dark Breath, Summon Slave, Blood Drain) і гомункулів (Moonlight, Castling, Bloodlust).",
     "Навыки монстров (Dark Breath, Summon Slave, Blood Drain) и гомункулов (Moonlight, Castling, Bloodlust)."},
    {Text::FxFireflies, "The maps' fireflies", "Світлячки карт", "Светлячки карт"},
    {Text::FxFirefliesHint,  // 3.0.7-beta.3: lamps at their places, the fireflies themselves «Світлячки в повітрі»
     "Each of the maps' fireflies lights the ground, the walls and the characters around its place like a lamp, in a "
     "colour of its own. Also those the maps' makers placed that the game never showed (once the map's file is read). "
     "Off: the game's own fireflies as the game draws them.",
     "Кожен світлячок карти освітлює землю, стіни й персонажів довкола свого місця, як лампа, своїм кольором. Також "
     "ті, що їх поставили автори карт, але гра ніколи не показувала (коли файл карти прочитано). Вимкнено — світлячки "
     "гри такі, як їх малює гра.",
     "Каждый светлячок карты освещает землю, стены и персонажей вокруг своего места, как лампа, своим цветом. Также "
     "те, что поставили авторы карт, но игра никогда не показывала (когда файл карты прочитан). Выключено — "
     "светлячки игры такие, как их рисует игра."},
    {Text::FxFirefliesBrightness, "Fireflies' brightness", "Яскравість світлячків", "Яркость светлячков"},
    {Text::FxFirefliesBrightnessHint,
     "How bright the fireflies and their light are.", "Наскільки яскраві світлячки та їхнє світло.",
     "Насколько ярки светлячки и их свет."},
    {Text::FxMapGlows, "Hidden lights of the maps", "Приховані вогники карт", "Скрытые огоньки карт"},
    {Text::FxMapGlowsHint,
     "Glows, pools of light and dust the maps' makers placed but the game never showed - Manuk, Splendide, "
     "Nidhoggur's nest, the towers - drawn from the map's file: floating balls of light, light on the ground.",
     "Сяйва, світлові кола й пил, що їх поставили автори карт, але гра ніколи не показувала, — Манук, Сплендід, "
     "гніздо Нідхьоґґа, вежі, — малюються за файлом карти: кулі світла в повітрі, світло на землі.",
     "Свечения, световые круги и пыль, которые поставили авторы карт, но игра никогда не показывала, — Манук, "
     "Сплендид, гнездо Нидхёгга, башни, — рисуются по файлу карты: шары света в воздухе, свет на земле."},
    {Text::FxMapGlowsBrightness, "Hidden lights' brightness", "Яскравість вогників", "Яркость огоньков"},
    {Text::FxMapGlowsBrightnessHint,
     "How bright the maps' hidden lights and their light are.", "Наскільки яскраві приховані вогники та їхнє світло.",
     "Насколько ярки скрытые огоньки и их свет."},
    {Text::FxRays, "Beams of light in the forest", "Промені світла в лісі", "Лучи света в лесу"},
    {Text::FxRaysHint,
     "The beams of light falling through the trees (Umbala, Geffenia, Glast Heim) are soft at their ends, shimmer as "
     "if through moving leaves, have dust drifting in them and light the ground where they fall.",
     "Промені світла крізь дерева (Умбала, Гефенія, Гласт Хейм) м'які на кінцях, мерехтять, наче крізь листя, у них "
     "кружляє пил, а там, де вони падають, світиться земля.",
     "Лучи света сквозь деревья (Умбала, Гефения, Гласт Хейм) мягкие на концах, мерцают, будто сквозь листву, в них "
     "кружится пыль, а там, где они падают, светится земля."},
    {Text::FxRaysBrightness, "Beams' brightness", "Яскравість променів", "Яркость лучей"},
    {Text::FxRaysBrightnessHint, "How bright the beams and the light they leave on the ground are.",
     "Наскільки яскраві промені та світло, яке вони лишають на землі.",
     "Насколько ярки лучи и свет, который они оставляют на земле."},
    {Text::FxLava, "Living lava", "Жива лава", "Живая лава"},
    {Text::FxLavaHint,
     "Lava (the Magma Dungeon) flows: its dark crust drifts, its cracks glow and pulse, and its heat lights the walls "
     "and the characters around it in red and orange.",
     "Лава (Магма-підземелля) тече: темна кірка пливе, тріщини світяться й пульсують, а жар освітлює стіни й "
     "персонажів довкола червоним і помаранчевим.",
     "Лава (Магма-подземелье) течёт: тёмная корка плывёт, трещины светятся и пульсируют, а жар освещает стены и "
     "персонажей вокруг красным и оранжевым."},
    {Text::FxLavaBrightness, "Lava's brightness", "Яскравість лави", "Яркость лавы"},
    {Text::FxLavaBrightnessHint,
     "How brightly the lava glows and lights around it.", "Наскільки яскраво лава сяє та освітлює довкола.",
     "Насколько ярко лава светится и освещает всё вокруг."},
    {Text::FxActorShadows, "Shadows from the sun and fire", "Тіні від сонця й вогню", "Тени от солнца и огня"},
    {Text::FxActorShadowsHint,
     "Characters and monsters cast soft shadows the shape of their sprites on the ground, instead of the game's round "
     "spot under the feet: by day from the sun; underground and at night from the two nearest torches and lamps - "
     "short by a fire, longer farther off, fading with its light and trembling with its flame.",
     "Персонажі й монстри відкидають м'які тіні за формою спрайта на землю замість круглої плями гри під ногами: удень "
     "від сонця; під землею й уночі — від двох найближчих смолоскипів і ліхтарів: коротка біля вогню, довша далі, "
     "блідне з його світлом і тремтить із полум'ям.",
     "Персонажи и монстры отбрасывают мягкие тени по форме спрайта на землю вместо круглого пятна игры под ногами: "
     "днём от солнца; под землёй и ночью — от двух ближайших факелов и фонарей: короткая у огня, длиннее дальше, "
     "бледнеет с его светом и дрожит вместе с пламенем."},
    {Text::FxActorShadowsStrength, "Shadows' darkness", "Темрява тіней", "Темнота теней"},
    {Text::FxActorShadowsStrengthHint,
     "How dark the characters' shadows are.", "Наскільки темні тіні персонажів.", "Насколько темны тени персонажей."},
    {Text::FxActorShadowsSpot, "The game's round shadow", "Кругла тінь гри", "Круглая тень игры"},  // 2.6.15
    {Text::FxActorShadowsSpotHint,
     "The game's round spot under the characters and the monsters as well as their own shadows. Off: no two shadows - "
     "the spot only where none of theirs falls (underground with no fire near).",
     "Кругла пляма гри під персонажами й монстрами разом з їхніми тінями. Вимкнено: двох тіней немає — пляма лише "
     "там, де їхня тінь не падає (під землею без вогню поруч).",
     "Круглое пятно игры под персонажами и монстрами вместе с их тенями. Выключено: двух теней нет — пятно только "
     "там, где их тень не падает (под землёй без огня рядом)."},
    {Text::FxHeat, "Heat haze", "Марево від жару", "Марево от жара"},
    {Text::FxHeatHint,
     "Over the torches' flames, burning skills and lava, and far off in the desert of Morroc, the picture shimmers as "
     "the hot air rises.",
     "Над полум'ям смолоскипів, вогняними вміннями й лавою, а вдалині — у пустелі Морока, картинка тремтить, наче "
     "гаряче повітря здіймається.",
     "Над пламенем факелов, огненными умениями и лавой, а вдали — в пустыне Морокка, картинка дрожит, будто "
     "поднимается горячий воздух."},
    {Text::FxHeatStrength, "Heat haze's strength", "Сила марева", "Сила марева"},
    {Text::FxHeatStrengthHint, "How much the hot air shimmers.", "Наскільки сильно тремтить гаряче повітря.",
     "Насколько сильно дрожит горячий воздух."},
    {Text::FxSkillColours, "Other skills in their colours", "Інші вміння своїм кольором", "Другие умения своим цветом"},
    {Text::FxSkillColoursHint,
     "Not only fire: ice and water light in cold blue, lightning in brief white flashes, holy skills in gold, poison "
     "in green - each skill in the colour of its own picture.",
     "Не лише вогонь: лід і вода світять холодним блакитним, блискавки — короткими білими спалахами, святі вміння — "
     "золотим, отрута — зеленим: кожне вміння кольором своєї картинки.",
     "Не только огонь: лёд и вода светят холодным голубым, молнии — короткими белыми вспышками, святые умения — "
     "золотым, яд — зелёным: каждое умение цветом своей картинки."},
    {Text::FxSkillsSoft, "Skills that do not blind", "Уміння не сліплять", "Умения не слепят"},
    {Text::FxSkillsSoftHint,
     "Skills and other effects of the game (not the maps' torches, smoke and the like) shine softer, so that big ones "
     "- Storm Gust, Magnus Exorcismus, Meteor Storm - do not blind.",
     "Уміння та інші ефекти гри (не смолоскипи, дим тощо на картах) світять м'якше, щоб великі — Storm Gust, Magnus "
     "Exorcismus, Meteor Storm — не сліпили.",
     "Умения и другие эффекты игры (не факелы, дым и т. п. на картах) светят мягче, чтобы большие — Storm Gust, Magnus "
     "Exorcismus, Meteor Storm — не слепили."},
    {Text::FxSkillBrightness, "Skills' brightness", "Яскравість умінь", "Яркость умений"},
    {Text::FxSkillBrightnessHint, "How bright the skills are, of what the game draws: 100 % as in the game.",
     "Наскільки яскраві вміння від того, як їх малює гра: 100 % — як у грі.",
     "Насколько ярки умения от того, как их рисует игра: 100 % — как в игре."},
    {Text::FxPortals, "Living portals", "Живі портали", "Живые порталы"},
    {Text::FxPortalsHint, "The warps glow softly: their light falls on what is around them.",
     "Варпи м'яко світяться: їхнє світло падає на все довкола.",
     "Варпы мягко светятся: их свет падает на всё вокруг."},
    {Text::FxPortalsStrength, "Portals' strength", "Сила порталів", "Сила порталов"},
    {Text::FxPortalsStrengthHint,
     "How bright the warps' light and the new portal are.", "Наскільки яскраві світло варпів і новий портал.",
     "Насколько ярки свет варпов и новый портал."},
    {Text::FxWeather, "Precipitation", "Опади", "Осадки"},  // 2.6.22: the Weather is the choice above
    {Text::FxWeatherHint,
     "Snow, rain, falling leaves or sand in the wind - by the Weather choice at the top (Auto - the map's own: snow "
     "over Lutie, leaves falling in Payon's woods, sand in the wind about Morroc, a fine rain over Niflheim and "
     "Einbroch).",
     "Сніг, дощ, листопад чи піщаний вітер — за вибором «Погода» вгорі (Авто — своя погода карти: сніг над Лютією, "
     "листопад у лісах Пайона, піщаний вітер довкола Морока, дрібний дощ над Нифльгеймом та Ейнброхом).",
     "Снег, дождь, листопад или песчаный ветер — по выбору «Погода» вверху (Авто — своя погода карты: снег над "
     "Лютией, листопад в лесах Пайона, песчаный ветер вокруг Морокка, мелкий дождь над Нифльхеймом и Эйнброхом)."},
    {Text::FxWeatherAmount, "Weather's amount", "Кількість опадів", "Количество осадков"},
    {Text::FxWeatherAmountHint, "How much snow, rain, leaves or sand there is.", "Скільки снігу, дощу, листя чи піску.",
     "Сколько снега, дождя, листьев или песка."},
    {Text::QualityWindowArt,
     "Window buttons, frames and labels", "Кнопки, рамки й написи вікон", "Кнопки, рамки и надписи окон"},
    {Text::QualityWindowArtHint,
     "How the windows' buttons, tabs, frames and the labels drawn on them are enlarged. Sharp (xBR) keeps letters "
     "one pixel thick whole; the AI is softer but melts them (the icons and pictures stay the AI's either way, with AI "
     "textures on). Needs crisp text.",
     "Як збільшуються кнопки, вкладки, рамки вікон і написи, намальовані на них. Чіткі (xBR) зберігають літери "
     "завтовшки в одну точку; нейромережа м'якша, але розмиває їх (іконки й картинки з ШІ-текстурами в будь-якому "
     "разі збільшує вона). Потрібен чіткий текст.",
     "Как увеличиваются кнопки, вкладки, рамки окон и надписи, нарисованные на них. Чёткие (xBR) сохраняют буквы "
     "толщиной в одну точку; нейросеть мягче, но размывает их (иконки и картинки с ИИ-текстурами в любом случае "
     "увеличивает она). Нужен чёткий текст."},
    {Text::WindowArtXbr, "Sharp (xBR)", "Чіткі (xBR)", "Чёткие (xBR)"},
    {Text::WindowArtAi, "AI (with AI textures)", "Нейромережа (з ШІ-текстурами)", "Нейросеть (с ИИ-текстурами)"},
    {Text::WindowArtPixels, "As in the game (pixels)", "Як у грі (пікселі)", "Как в игре (пиксели)"},
    {Text::AiGenerate, "Make new ones while playing (debugging)", "Створювати нові під час гри (налагодження)",
     "Создавать новые во время игры (отладка)"},
    {Text::AiGenerateHint,
     "Direct3D 12: the network makes in the game what the texture pack does not have yet, a little every frame, and "
     "writes it into the pack. With the pack's HD textures off it makes every picture anew and keeps none - a model "
     "tried without touching the pack. For the pack's maker; players take the ready pack.",
     "Direct3D 12: нейромережа створює в грі те, чого ще немає в пакеті текстур, потроху кожен кадр, і дописує це в "
     "пакет. З вимкненими HD-текстурами з пакета вона створює всі картинки наново й нічого не зберігає - так модель "
     "пробується, не чіпаючи пакета. Для того, хто збирає пакет; гравці беруть готовий пакет.",
     "Direct3D 12: нейросеть создаёт в игре то, чего ещё нет в пакете текстур, понемногу каждый кадр, и дописывает "
     "это в пакет. С выключенными HD-текстурами из пакета она создаёт все картинки заново и ничего не сохраняет - так "
     "модель пробуется, не трогая пакет. Для того, кто собирает пакет; игроки берут готовый пакет."},
    {Text::AiPackNote,
     "The textures come from the texture pack uaro-textures.zip in the game's folder. What it lacks the game's log "
     "says (\"not in the pack\").",
     "Текстури беруться з пакета текстур uaro-textures.zip у теці гри. Чого в ньому бракує, пише журнал гри (\"not in "
     "the pack\").",
     "Текстуры берутся из пакета текстур uaro-textures.zip в папке игры. Чего в нём не хватает, пишет лог игры (\"not "
     "in the pack\")."},
    {Text::PagePack, "Texture pack", "Пакет текстур", "Пакет текстур"},
    {Text::PackIntro,
     "The texture pack - uaro-textures.zip in the game's folder - holds the AI's pictures of the game's textures, made "
     "once on this graphics card. Give it to other players: their game shows the pictures at once and their graphics "
     "card makes nothing. Making it takes from tens of minutes to a few hours; it can stop and go on later.",
     "Пакет текстур - uaro-textures.zip у теці гри - містить картинки ШІ для текстур гри, створені один раз на цій "
     "відеокарті. Передай його іншим гравцям: їхня гра одразу показує картинки, а відеокарта нічого не створює. "
     "Створення триває від десятків хвилин до кількох годин; його можна зупинити й продовжити пізніше.",
     "Пакет текстур - uaro-textures.zip в папке игры - содержит картинки ИИ для текстур игры, созданные один раз на "
     "этой видеокарте. Передай его другим игрокам: их игра сразу показывает картинки, а видеокарта ничего не "
     "создаёт. Создание занимает от десятков минут до нескольких часов; его можно остановить и продолжить позже."},
    {Text::PackHas, "uaro-textures.zip: {1} pictures, {2}.", "uaro-textures.zip: {1} картинок, {2}.",
     "uaro-textures.zip: {1} картинок, {2}."},
    {Text::PackNone, "There is no texture pack in the game's folder yet.", "У теці гри ще немає пакета текстур.",
     "В папке игры ещё нет пакета текстур."},
    {Text::PackWhat, "What goes into the pack:", "Що входить у пакет:", "Что входит в пакет:"},
    {Text::PackWorld, "Maps", "Карти", "Карты"},
    {Text::PackWorldHint,
     "The ground, buildings, trees and water of every map.", "Земля, будівлі, дерева й вода всіх карт.",
     "Земля, здания, деревья и вода всех карт."},
    {Text::PackEffects, "Effects", "Ефекти", "Эффекты"},
    {Text::PackEffectsHint,
     "The light and sparks of skills and maps.", "Світло й іскри вмінь і карт.", "Свет и искры умений и карт."},
    {Text::PackMonsters, "Monsters", "Монстри", "Монстры"},
    {Text::PackMonstersHint, "Monsters, homunculi and mercenaries: every frame of their sprites.",
     "Монстри, гомункули й найманці: кожен кадр їхніх спрайтів.",
     "Монстры, гомункулы и наёмники: каждый кадр их спрайтов."},
    {Text::PackPlayers, "Characters", "Персонажі", "Персонажи"},
    {Text::PackPlayersHint, "Bodies, heads, headgear, weapons and shields - the most frames of all.",
     "Тіла, голови, головні убори, зброя й щити - найбільше кадрів.",
     "Тела, головы, головные уборы, оружие и щиты - больше всего кадров."},
    {Text::PackNpcs, "NPCs", "NPC", "NPC"},
    {Text::PackNpcsHint, "The NPCs' sprites.", "Спрайти NPC.", "Спрайты NPC."},
    {Text::PackItems, "Items", "Предмети", "Предметы"},
    {Text::PackItemsHint, "Items lying on the ground.", "Предмети, що лежать на землі.", "Предметы, лежащие на земле."},
    {Text::PackInterface, "Interface", "Інтерфейс", "Интерфейс"},
    {Text::PackInterfaceHint,
     "Item and skill icons, the windows' pictures.", "Іконки предметів і вмінь, картинки вікон.",
     "Иконки предметов и умений, картинки окон."},
    {Text::PackQuality, "Picture quality", "Якість картинок", "Качество картинок"},
    {Text::PackQualityHint,
     "WebP. Balanced is some ten times smaller than lossless and looks the same in the game; lossless keeps every "
     "pixel and is several times larger.",
     "WebP. Збалансована приблизно вдесятеро менша за якість без втрат і в грі виглядає так само; без втрат зберігає "
     "кожну точку й займає в кілька разів більше.",
     "WebP. Сбалансированное примерно в десять раз меньше качества без потерь и в игре выглядит так же; без потерь "
     "сохраняет каждую точку и занимает в несколько раз больше."},
    {Text::PackQualitySmall, "Smaller file (85)", "Менший файл (85)", "Файл поменьше (85)"},
    {Text::PackQualityBalanced, "Balanced (92)", "Збалансована (92)", "Сбалансированное (92)"},
    {Text::PackQualityLossless, "Lossless", "Без втрат", "Без потерь"},
    {Text::PackMake, "Make the pack", "Створити пакет", "Создать пакет"},
    {Text::PackStop, "Stop", "Зупинити", "Остановить"},
    {Text::PackAnew, "Make anew", "Створити заново", "Создать заново"},
    {Text::PackCopy, "Copy the result", "Скопіювати результат", "Скопировать результат"},
    {Text::PackAskAnew, "Throw away the {1} pictures of the pack and make it anew?",
     "Викинути {1} картинок пакета й створити його заново?", "Выбросить {1} картинок пакета и создать его заново?"},
    {Text::PackNoHost, "uaro-graphics\\uaro-render64.exe is missing: install uaRO Graphics again.",
     "Немає uaro-graphics\\uaro-render64.exe: встанови uaRO Graphics ще раз.",
     "Нет uaro-graphics\\uaro-render64.exe: установи uaRO Graphics ещё раз."},
    {Text::PackNothing, "Tick at least one kind.", "Познач хоча б один вид.", "Отметь хотя бы один вид."},
    {Text::PackStarting, "Starting the networks on the graphics card...", "Запуск нейромереж на відеокарті...",
     "Запуск нейросетей на видеокарте..."},
    {Text::PackReading,
     "Reading the game's files: {1} of {2}.", "Читання файлів гри: {1} з {2}.", "Чтение файлов игры: {1} из {2}."},
    {Text::PackBuilding, "Pictures: {1} of {2}, {3} a second; about {4} left.",
     "Картинки: {1} з {2}, {3} за секунду; лишилося близько {4}.",
     "Картинки: {1} из {2}, {3} в секунду; осталось около {4}."},
    {Text::PackBuildingNoEta, "Pictures: {1} of {2}.", "Картинки: {1} з {2}.", "Картинки: {1} из {2}."},
    {Text::PackFile, "Now: {1}", "Зараз: {1}", "Сейчас: {1}"},
    {Text::PackDone, "Done: the pack holds {1} pictures, {2}.", "Готово: у пакеті {1} картинок, {2}.",
     "Готово: в пакете {1} картинок, {2}."},
    {Text::PackStopped, "Stopped: {1} pictures made this time. Make the pack goes on from here.",
     "Зупинено: цього разу створено {1} картинок. «Створити пакет» продовжить з цього місця.",
     "Остановлено: в этот раз создано {1} картинок. «Создать пакет» продолжит с этого места."},
    {Text::PackFailed, "Failed: {1}", "Помилка: {1}", "Ошибка: {1}"},
    {Text::PackGone, "The builder closed before it finished. Make the pack goes on from where it stopped.",
     "Збирач закрився, не закінчивши. «Створити пакет» продовжить з того місця, де він зупинився.",
     "Сборщик закрылся, не закончив. «Создать пакет» продолжит с того места, где он остановился."},
    {Text::PackGameRunning,
     "The game is running: it shares the graphics card with the builder and may stutter meanwhile.",
     "Гра запущена: вона ділить відеокарту зі збирачем і тим часом може підгальмовувати.",
     "Игра запущена: она делит видеокарту со сборщиком и в это время может подтормаживать."},
    {Text::PackBackground, "The pack is made in the background: this window can be closed, the work goes on.",
     "Пакет створюється у фоні: це вікно можна закрити, робота триває.",
     "Пакет создаётся в фоне: это окно можно закрыть, работа продолжится."},
    {Text::TimeHoursMinutes, "{1} h {2} min", "{1} год {2} хв", "{1} ч {2} мин"},
    {Text::TimeMinutes, "{1} min", "{1} хв", "{1} мин"},
    {Text::TimeSeconds, "{1} s", "{1} с", "{1} с"},
    {Text::RenderApi, "Graphics API", "Графічний API", "Графический API"},
    {Text::RenderApiHint,
     "How uaRO Graphics draws. Direct3D 12 has every feature (DLSS, effects, AI textures). Direct3D 11 is for graphics "
     "cards without Direct3D 12 (Radeon HD 5000-7000, GeForce GTX 400-600 with old drivers, Intel HD 4000) and "
     "Windows 7 and 8: the game, the window's size, crisp text, the effects and the AI textures of the texture pack, "
     "without DLSS. Auto takes Direct3D 12 and else 11.",
     "Як малює uaRO Graphics. Direct3D 12 має всі можливості (DLSS, ефекти, ШІ-текстури). Direct3D 11 - для "
     "відеокарт без Direct3D 12 (Radeon HD 5000-7000, GeForce GTX 400-600 зі старими драйверами, Intel HD 4000) і "
     "Windows 7 та 8: гра, розмір вікна, чіткий текст, ефекти й ШІ-текстури з пакета текстур, без DLSS. "
     "«Авто» бере Direct3D 12, а без нього - 11.",
     "Как рисует uaRO Graphics. Direct3D 12 умеет всё (DLSS, эффекты, ИИ-текстуры). Direct3D 11 - для видеокарт без "
     "Direct3D 12 (Radeon HD 5000-7000, GeForce GTX 400-600 со старыми драйверами, Intel HD 4000) и Windows 7 и 8: "
     "игра, размер окна, чёткий текст, эффекты и ИИ-текстуры из пакета текстур, без DLSS. «Авто» берёт "
     "Direct3D 12, а без него - 11."},
    {Text::RenderApiD3d12,
     "Direct3D 12 (all features)", "Direct3D 12 (усі можливості)", "Direct3D 12 (все возможности)"},
    {Text::RenderApiD3d11, "Direct3D 11 (older graphics cards, test)", "Direct3D 11 (старші відеокарти, тест)",
     "Direct3D 11 (старые видеокарты, тест)"},
    {Text::RenderApiAuto, "Auto: 12, else 11", "Авто: 12, інакше 11", "Авто: 12, иначе 11"},
    {Text::RenderUseD3d11,
     "This computer has no Direct3D 12, but Direct3D 11 works: choose Direct3D 11 or Auto as the graphics API.",
     "На цьому комп'ютері немає Direct3D 12, але Direct3D 11 працює: виберіть графічний API Direct3D 11 або «Авто».",
     "На этом компьютере нет Direct3D 12, но Direct3D 11 работает: выберите графический API Direct3D 11 или «Авто»."},
    {Text::DiagD3D11, "Direct3D 11", "Direct3D 11", "Direct3D 11"},
    {Text::DiagD3D11Ok,
     "works, feature level {1}", "працює, рівень можливостей {1}", "работает, уровень возможностей {1}"},
    {Text::DiagD3D11Failed, "does not work ({1})", "не працює ({1})", "не работает ({1})"},
    {Text::AiD3d11Note,
     "With Direct3D 11 (the Render page) the game shows the ready pictures of the texture pack; making new ones while "
     "playing needs Direct3D 12.",
     "З Direct3D 11 (вкладка «Рендер») гра показує готові картинки пакета текстур; створення нових під час гри "
     "потребує Direct3D 12.",
     "С Direct3D 11 (вкладка «Рендер») игра показывает готовые картинки пакета текстур; создание новых во время игры "
     "требует Direct3D 12."},
    {Text::AiCacheAskOld,
     "Delete upscaled.grf ({1})? It is not used any more: the textures come from the texture pack.",
     "Видалити upscaled.grf ({1})? Він більше не використовується: текстури беруться з пакета текстур.",
     "Удалить upscaled.grf ({1})? Он больше не используется: текстуры берутся из пакета текстур."},
    {Text::AiCacheOldDeleted, "upscaled.grf is deleted.",
     "upscaled.grf видалено.",
     "upscaled.grf удалён."},
    {Text::ServiceUpdate, "Updates",
     "Оновлення",
     "Обновление"},
    {Text::UpdateCheck, "Check for updates",
     "Перевірити оновлення",
     "Проверить обновления"},
    {Text::UpdateNow, "Update now",
     "Оновити зараз",
     "Обновить сейчас"},
    {Text::UpdateChecking, "Asking GitHub for the newest version...",
     "Запитую на GitHub найновішу версію...",
     "Спрашиваю у GitHub новейшую версию..."},
    {Text::UpdateNewer, "A newer version is out: {1} (this one is {2}). Update now installs it.",
     "Вийшла новіша версія: {1} (ця — {2}). «Оновити зараз» встановить її.",
     "Вышла новая версия: {1} (эта — {2}). «Обновить сейчас» установит её."},
    {Text::UpdateLatest, "{1} is the newest version.",
     "{1} — найновіша версія.",
     "{1} — новейшая версия."},
    {Text::UpdateFailed, "Could not check: {1}",
     "Не вдалося перевірити: {1}",
     "Не удалось проверить: {1}"},
    {Text::UpdateAsk,
     "The settings app closes, the update installs in a window of its own and the app opens again. Go on?",
     "Утиліта закриється, оновлення встановиться в окремому вікні, і вона відкриється знову. Продовжити?",
     "Утилита закроется, обновление установится в отдельном окне, и она откроется снова. Продолжить?"},
    {Text::UpdateGameRunning, "The game is open: the update waits until it closes.",
     "Гра відкрита: оновлення дочекається, поки вона закриється.",
     "Игра открыта: обновление дождётся, пока она закроется."},
    {Text::UpdateGameHung,
     "The game runs but has no window - it hung while starting. Close it, and the update goes on.",
     "Гра запущена, але без вікна — вона зависла під час запуску. Закрийте її, і оновлення продовжиться.",
     "Игра запущена, но без окна — она зависла при запуске. Закройте её, и обновление продолжится."},
    {Text::UpdateCloseHung, "Close the hung game", "Закрити завислу гру", "Закрыть зависшую игру"},
    {Text::UpdateCloseHungFailed,
     "Windows did not let the game close. Close uaRO.exe in Task Manager (Ctrl+Shift+Esc).",
     "Windows не дав закрити гру. Закрийте uaRO.exe у Диспетчері завдань (Ctrl+Shift+Esc).",
     "Windows не дал закрыть игру. Закройте uaRO.exe в Диспетчере задач (Ctrl+Shift+Esc)."},
    {Text::UpdateNote,
     "Updates come from GitHub (kwebmn/ragnarok-graphics): the settings app downloads, checks and installs them by "
     "itself, the settings stay. The previous version is kept to go back to.",
     "Оновлення приходять із GitHub (kwebmn/ragnarok-graphics): утиліта сама завантажує, перевіряє й встановлює їх, "
     "налаштування залишаються. Попередня версія зберігається, щоб до неї можна було повернутися.",
     "Обновления приходят с GitHub (kwebmn/ragnarok-graphics): утилита сама скачивает, проверяет и устанавливает их, "
     "настройки остаются. Предыдущая версия сохраняется, чтобы к ней можно было вернуться."},
    {Text::UpdateError, "The update could not start: {1}",
     "Оновлення не запустилося: {1}",
     "Обновление не запустилось: {1}"},
    {Text::AutoHeading, "Set up for this computer",
     "Налаштувати під цей комп'ютер",
     "Настроить под этот компьютер"},
    {Text::AutoButton, "Set up automatically",
     "Налаштувати автоматично",
     "Настроить автоматически"},
    {Text::AutoNote,
     "Looks at the graphics card, its video memory and the screen and picks what they carry; the texture pack is used "
     "when uaro-textures.zip lies next to uaRO.exe. On a new computer: press it, then start the game.",
     "Дивиться на відеокарту, її відеопам'ять та екран і вибирає те, що вони потягнуть; пакет текстур вмикається, "
     "якщо uaro-textures.zip лежить поруч з uaRO.exe. На новому комп'ютері: натисніть, потім запустіть гру.",
     "Смотрит на видеокарту, её видеопамять и экран и выбирает то, что они потянут; пакет текстур включается, если "
     "uaro-textures.zip лежит рядом с uaRO.exe. На новом компьютере: нажмите, потом запустите игру."},
    {Text::AutoFound, "Graphics card: {1}, {2} MB of video memory; screen {3}.",
     "Відеокарта: {1}, {2} МБ відеопам'яті; екран {3}.",
     "Видеокарта: {1}, {2} МБ видеопамяти; экран {3}."},
    {Text::AutoTierNoDriver,
     "No graphics driver is installed (Windows' basic display adapter) or the card is too old: the game's own look is "
     "set. Install the graphics card's driver from its maker's site and press again.",
     "Драйвер відеокарти не встановлено (базовий відеоадаптер Windows) або карта надто стара: встановлено власний "
     "вигляд гри. Встановіть драйвер відеокарти з сайту виробника й натисніть ще раз.",
     "Драйвер видеокарты не установлен (базовый видеоадаптер Windows) или карта слишком старая: установлен "
     "собственный вид игры. Установите драйвер видеокарты с сайта производителя и нажмите ещё раз."},
    {Text::AutoTierOld,
     "The card has no Direct3D 12: Direct3D 11, the picture at the game's size, no smoothing, the Low effects.",
     "Карта не має Direct3D 12: Direct3D 11, картинка в розмірі гри, без згладжування, ефекти «Низькі».",
     "У карты нет Direct3D 12: Direct3D 11, картинка в размере игры, без сглаживания, эффекты «Низкие»."},
    {Text::AutoTierLow, "A small graphics card: the light settings, the heavy effects off.",
     "Слабка відеокарта: легкі налаштування, важкі ефекти вимкнено.",
     "Слабая видеокарта: лёгкие настройки, тяжёлые эффекты выключены."},
    {Text::AutoTierMiddle, "A middle graphics card: the balanced settings.",
     "Середня відеокарта: збалансовані налаштування.",
     "Средняя видеокарта: сбалансированные настройки."},
    {Text::AutoTierHigh, "A strong graphics card: the picture drawn at {1}% and smoothed.",
     "Потужна відеокарта: картинка малюється на {1}% і згладжується.",
     "Мощная видеокарта: картинка рисуется на {1}% и сглаживается."},
    {Text::AutoAiAll, "AI textures: on, every kind (from the texture pack).",
     "ШІ-текстури: увімкнено, усі види (з пакета текстур).",
     "ИИ-текстуры: включены, все виды (из пакета текстур)."},
    {Text::AutoAiSprites,
     "AI textures: characters, monsters, items and icons only - the card has under 1 GB of video memory.",
     "ШІ-текстури: лише персонажі, монстри, предмети та іконки — у карти менше 1 ГБ відеопам'яті.",
     "ИИ-текстуры: только персонажи, монстры, предметы и иконки — у карты меньше 1 ГБ видеопамяти."},
    {Text::AutoAiNoPack, "AI textures: off - uaro-textures.zip is not next to uaRO.exe.",
     "ШІ-текстури: вимкнено — поруч з uaRO.exe немає uaro-textures.zip.",
     "ИИ-текстуры: выключены — рядом с uaRO.exe нет uaro-textures.zip."},
    {Text::AutoAiOff, "AI textures: off.",
     "ШІ-текстури: вимкнено.",
     "ИИ-текстуры: выключены."},
    {Text::AutoAsk, "Write these settings and start the game?",
     "Записати ці налаштування й запустити гру?",
     "Записать эти настройки и запустить игру?"},
    {Text::AutoPending, "The settings are on the pages now; Apply writes them.",
     "Налаштування вже на сторінках; «Застосувати» запише їх.",
     "Настройки уже на страницах; «Применить» запишет их."},
    {Text::RenderGpu, "Graphics card",
     "Відеокарта",
     "Видеокарта"},
    {Text::RenderGpuHint,
     "Which graphics card draws the game when the computer has several (a laptop's integrated one and its NVIDIA or "
     "AMD). Automatic takes the fast one - Windows' choice for high performance. The game, the 64-bit renderer and "
     "the texture pack's builder use it. With one card the row only shows it.",
     "Яка відеокарта малює гру, якщо їх кілька (вбудована в ноутбуці та NVIDIA чи AMD). «Автоматично» бере швидшу — "
     "вибір Windows для високої продуктивності. Нею користуються гра, 64-бітний рендер і збирач пакета текстур. З "
     "однією відеокартою рядок лише показує її.",
     "Какая видеокарта рисует игру, если их несколько (встроенная в ноутбуке и NVIDIA или AMD). «Автоматически» берёт "
     "быструю — выбор Windows для высокой производительности. Ею пользуются игра, 64-битный рендер и сборщик пакета "
     "текстур. С одной видеокартой строка только показывает её."},
    {Text::RenderGpuAuto, "Automatic: {1}",
     "Автоматично: {1}",
     "Автоматически: {1}"},
    {Text::RenderGpuItem, "{1}, {2} MB",
     "{1}, {2} МБ",
     "{1}, {2} МБ"},
    {Text::RenderGpuMissing, "{1} (not found)",
     "{1} (не знайдено)",
     "{1} (не найдена)"},
    {Text::ReportHeading, "Report to the developers",
     "Звіт розробникам",
     "Отчёт разработчикам"},
    {Text::ReportNote,
     "Sends this computer's graphics card, Windows, monitors, uaro-graphics.ini and the logs of uaRO Graphics to "
     "uaro.kiev.ua - no login, no character; the Windows user name is hidden. The developers keep it 90 days.",
     "Надсилає на uaro.kiev.ua відеокарту, Windows, монітори цього комп'ютера, uaro-graphics.ini та журнали uaRO "
     "Graphics — без логіна й персонажа; ім'я користувача Windows приховано. Розробники зберігають його 90 днів.",
     "Отправляет на uaro.kiev.ua видеокарту, Windows, мониторы этого компьютера, uaro-graphics.ini и логи uaRO "
     "Graphics — без логина и персонажа; имя пользователя Windows скрыто. Разработчики хранят его 90 дней."},
    {Text::ReportAuto, "Send a report by itself when something goes wrong",
     "Надсилати звіт самостійно, якщо щось пішло не так",
     "Отправлять отчёт самостоятельно, если что-то пошло не так"},
    {Text::ReportAutoHint,
     "After a game in which the renderer would not start, the graphics card was lost or the game closed without "
     "closing the renderer, a report goes to the developers at uaro.kiev.ua (at most one in 6 hours). Takes effect at "
     "the next start of the game.",
     "Після гри, у якій рендер не запустився, відеокарта відпала або гра закрилася, не закривши рендер, звіт іде "
     "розробникам на uaro.kiev.ua (не частіше одного на 6 годин). Діє з наступного запуску гри.",
     "После игры, в которой рендер не запустился, видеокарта отвалилась или игра закрылась, не закрыв рендер, отчёт "
     "уходит разработчикам на uaro.kiev.ua (не чаще одного в 6 часов). Действует со следующего запуска игры."},
    {Text::PackMakeHeading, "Make the pack on this computer",
     "Зробити пакет на цьому комп'ютері",
     "Сделать пакет на этом компьютере"},
    {Text::FramesFpsCounter, "Frame counter",
     "Лічильник кадрів",
     "Счётчик кадров"},
    {Text::FramesFpsCounterHint,
     "Frames a second, the frame time and the graphics card's work a frame (GPU, DLSS apart) in the game window's top "
     "left corner. The frames a second stay at the limit (60) whatever is on - the GPU's milliseconds show what an "
     "effect costs: watch them as you switch it (the console, Ctrl+~, has each effect's own). Ctrl+Shift+F11 in the "
     "game shows and hides it at any time.",
     "Кадри за секунду, час кадру й робота відеокарти за кадр (GPU, окремо DLSS) у лівому верхньому куті вікна гри. "
     "Кадри за секунду лишаються на межі (60), хоч би що було ввімкнено, — мілісекунди GPU показують, скільки коштує "
     "ефект: дивіться на них, перемикаючи його (у консолі, Ctrl+~, — ціна кожного ефекту окремо). Ctrl+Shift+F11 у грі "
     "показує й ховає його будь-коли.",
     "Кадры в секунду, время кадра и работа видеокарты за кадр (GPU, отдельно DLSS) в левом верхнем углу окна игры. "
     "Кадры в секунду остаются на пределе (60), что бы ни было включено, — миллисекунды GPU показывают, сколько стоит "
     "эффект: смотрите на них, переключая его (в консоли, Ctrl+~, — цена каждого эффекта отдельно). Ctrl+Shift+F11 в "
     "игре показывает и прячет его в любой момент."},
    {Text::ServiceProfiles, "Profiles",
     "Профілі",
     "Профили"},
    {Text::ProfileNote,
     "A profile keeps every setting of uaro-graphics.ini under a name - one for playing, one for a weak laptop, one "
     "for recording - and brings them all back at a press.",
     "Профіль зберігає всі налаштування uaro-graphics.ini під назвою — для гри, для слабкого ноутбука, для запису — і "
     "повертає їх одним натисканням.",
     "Профиль хранит все настройки uaro-graphics.ini под именем — для игры, для слабого ноутбука, для записи — и "
     "возвращает их одним нажатием."},
    {Text::ProfileName, "Name",
     "Назва",
     "Имя"},
    {Text::ProfileNameCue, "for example: Maximum for 4K",
     "наприклад: Максимум для 4K",
     "например: Максимум для 4K"},
    {Text::ProfileSave, "Save as a profile",
     "Зберегти як профіль",
     "Сохранить как профиль"},
    {Text::ProfileLoad, "Load",
     "Завантажити",
     "Загрузить"},
    {Text::ProfileDelete, "Delete",
     "Видалити",
     "Удалить"},
    {Text::ProfileNone, "(no profiles yet)",
     "(профілів ще немає)",
     "(профилей пока нет)"},
    {Text::ProfileSaved, "The profile \"{1}\" is saved.",
     "Профіль «{1}» збережено.",
     "Профиль «{1}» сохранён."},
    {Text::ProfileLoaded,
     "The profile \"{1}\" is in uaro-graphics.ini now: most settings take effect at once, the renderer's at the next "
     "start of the game.",
     "Профіль «{1}» тепер в uaro-graphics.ini: більшість налаштувань діє одразу, налаштування рендера — з наступного "
     "запуску гри.",
     "Профиль «{1}» теперь в uaro-graphics.ini: большинство настроек действует сразу, настройки рендера — со "
     "следующего запуска игры."},
    {Text::ProfileAskLoad, "Replace the settings with the profile \"{1}\"? The present ones go to a backup first.",
     "Замінити налаштування профілем «{1}»? Поточні спершу підуть у резервну копію.",
     "Заменить настройки профилем «{1}»? Текущие сначала уйдут в резервную копию."},
    {Text::ProfileAskDelete, "Delete the profile \"{1}\"?",
     "Видалити профіль «{1}»?",
     "Удалить профиль «{1}»?"},
    {Text::ProfileBadName, "A profile's name: letters, digits, spaces, - and _, up to 40.",
     "Назва профілю: літери, цифри, пробіли, - і _, до 40.",
     "Имя профиля: буквы, цифры, пробелы, - и _, до 40."},
    {Text::PackBodyColours, "Clothes colours",
     "Кольори одягу",
     "Цвета одежды"},
    {Text::PackBodyColoursHint,
     "The characters' bodies in every clothes dye too (players who dyed their clothes): 4 to 9 times the bodies' "
     "pictures - a bigger pack and a longer make.",
     "Тіла персонажів також у кожному кольорі фарби одягу (гравці, що пофарбували одяг): у 4–9 разів більше картинок "
     "тіл — більший пакет і довше створення.",
     "Тела персонажей также в каждом цвете краски одежды (игроки, покрасившие одежду): в 4–9 раз больше картинок тел "
     "— больше пакет и дольше создание."},
    {Text::AiCompress, "Compress in video memory",
     "Стискати у відеопам'яті",
     "Сжимать в видеопамяти"},
    {Text::AiCompressHint,
     "Direct3D 11: the AI textures compressed (BC1, BC3) - a quarter to an eighth of the video memory, a little less "
     "exact. Automatic: on for a card under 3 GB.",
     "Direct3D 11: ШІ-текстури стиснуті (BC1, BC3) — від чверті до восьмої частини відеопам'яті, трохи менш точно. "
     "Автоматично: увімкнено для карти до 3 ГБ.",
     "Direct3D 11: ИИ-текстуры сжаты (BC1, BC3) — от четверти до восьмой части видеопамяти, чуть менее точно. "
     "Автоматически: включено для карты до 3 ГБ."},
    {Text::AiCompressAuto, "Automatic",
     "Автоматично",
     "Автоматически"},
    {Text::AiCompressOn, "On",
     "Увімкнено",
     "Включено"},
    {Text::AiCompressOff, "Off",
     "Вимкнено",
     "Выключено"},
    {Text::QualityUiNative, "Interface at the screen's pixels",
     "Інтерфейс у пікселях екрана",
     "Интерфейс в пикселях экрана"},
    {Text::QualityUiNativeHint,
     "With a render scale over 100 % on Direct3D 12 without DLSS: only the world is drawn larger and made smaller; "
     "the windows and the text are drawn at the screen's own pixels - sharper. A test: say how it looks.",
     "З масштабом рендера понад 100 % на Direct3D 12 без DLSS: більшим малюється й зменшується лише світ; вікна й "
     "текст малюються в пікселях самого екрана — різкіше. Перевірка: скажіть, як виглядає.",
     "С масштабом рендера больше 100 % на Direct3D 12 без DLSS: больше рисуется и уменьшается только мир; окна и "
     "текст рисуются в пикселях самого экрана — резче. Проверка: скажите, как выглядит."},
    {Text::FxNightStrength, "Night strength",
     "Сила ночі",
     "Сила ночи"},
    {Text::FxNightStrengthHint,
     "How dark the night the Clock brings is - the world darker and colder, the torches, lamps, skills and fireflies "
     "shining out of it, the interface as it is: 0 % no night, 100 % a deep night.",
     "Наскільки темна ніч, яку приносить «Годинник», — світ темніший і холодніший, факели, лампи, уміння та "
     "світлячки світять із темряви, інтерфейс як є: 0 % — без ночі, 100 % — глибока ніч.",
     "Насколько темна ночь, которую приносят «Часы», — мир темнее и холоднее, факелы, лампы, умения и светлячки "
     "светят из темноты, интерфейс как есть: 0 % — без ночи, 100 % — глубокая ночь."},
    {Text::FxFoliage, "Leaves in the wind",
     "Листя на вітрі",
     "Листва на ветру"},
    {Text::FxFoliageHint, "The leaves of trees and bushes sway a little, their tops more than their trunks.",
     "Листя дерев і кущів трохи гойдається, верхівки більше за стовбури.",
     "Листва деревьев и кустов немного колышется, верхушки больше стволов."},
    {Text::FxFoliageStrength, "How much",
     "Наскільки",
     "Насколько"},
    {Text::FxFoliageStrengthHint, "100 % a light breeze, 200 % a strong wind.",
     "100 % — легкий вітерець, 200 % — сильний вітер.",
     "100 % — лёгкий ветерок, 200 % — сильный ветер."},
    {Text::QualityHdr, "HDR",
     "HDR",
     "HDR"},
    {Text::QualityHdrHint,
     "Direct3D 12 on a monitor with Windows' HDR on: the picture in HDR10, white at 200 nits, the fires and sparks a "
     "little brighter than white. Not with DLSS frame generation. A test.",
     "Direct3D 12 на моніторі з увімкненим HDR у Windows: картинка в HDR10, білий 200 ніт, вогні й іскри трохи "
     "яскравіші за білий. Не з генерацією кадрів DLSS. Перевірка.",
     "Direct3D 12 на мониторе с включённым HDR в Windows: картинка в HDR10, белый 200 нит, огни и искры чуть ярче "
     "белого. Не с генерацией кадров DLSS. Проверка."},
    {Text::FxSoftParticles,
     "Effects not cut at the ground", "Ефекти не обрізаються біля землі", "Эффекты не обрезаются у земли"},
    {Text::FxSoftParticlesHint,
     "Skills, sparks and portals fade softly where they touch the ground and the walls instead of being cut off along "
     "a hard line.",
     "Уміння, іскри й портали м’яко згасають там, де торкаються землі та стін, а не обрізаються різкою лінією.",
     "Умения, искры и порталы мягко гаснут там, где касаются земли и стен, а не обрезаются резкой линией."},
    {Text::FxRimLight, "Lit edge on the fire's side", "Світлий край з боку вогню", "Светлый край со стороны огня"},
    {Text::FxRimLightHint,
     "Torches, lamps and burning skills light the edge of a character that faces them more, so the flat sprites look "
     "rounder near a fire.",
     "Факели, лампи й палаючі вміння сильніше освітлюють край персонажа, повернутий до них, тож пласкі спрайти біля "
     "вогню виглядають об’ємніше.",
     "Факелы, лампы и горящие умения сильнее освещают край персонажа, обращённый к ним, поэтому плоские спрайты у "
     "огня выглядят объёмнее."},
    {Text::FxRimStrength, "Rim light strength",
     "Сила контрового світла",
     "Сила контрового света"},
    {Text::FxRimStrengthHint, "How bright the lit edge is.",
     "Наскільки яскравий освітлений край.",
     "Насколько ярок освещённый край."},
    {Text::FxMotes, "Pollen and motes in the air", "Пилок і порошинки в повітрі", "Пыльца и пылинки в воздухе"},
    {Text::FxMotesHint,
     "Where the map has no weather of its own, tiny motes drift in the air: pollen over the fields and the towns, "
     "dust in the dungeons and the houses. Direct3D 12.",
     "Де на карті немає власної погоди, у повітрі пливуть дрібні порошинки: пилок над полями й містами, пил у "
     "підземеллях і будинках. Direct3D 12.",
     "Где на карте нет своей погоды, в воздухе плывут мелкие частицы: пыльца над полями и городами, пыль в "
     "подземельях и домах. Direct3D 12."},
    {Text::FxMotesAmount, "How many motes",
     "Скільки порошинок",
     "Сколько частиц"},
    {Text::FxMotesAmountHint, "How many motes float in the air.",
     "Скільки порошинок пливе в повітрі.",
     "Сколько частиц плывёт в воздухе."},
    {Text::FxRelief, "Ground relief",
     "Рельєф землі",
     "Рельеф земли"},
    {Text::FxReliefHint,
     "The cracks, stones and grass of the ground are lit from one side and look raised - best with the AI textures.",
     "Тріщини, каміння й трава на землі освітлені з одного боку й виглядають рельєфними — найкраще з ШІ-текстурами.",
     "Трещины, камни и трава на земле освещены с одной стороны и выглядят рельефными — лучше всего с ИИ-текстурами."},
    {Text::FxReliefStrength, "Relief strength",
     "Сила рельєфу",
     "Сила рельефа"},
    {Text::FxReliefStrengthHint, "How deep the ground looks.",
     "Наскільки глибокою виглядає земля.",
     "Насколько глубокой выглядит земля."},
    {Text::FxFloorReflections,
     "Reflections on smooth floors", "Відбиття на гладкій підлозі", "Отражения на гладком полу"},
    {Text::FxFloorReflectionsHint,
     "Indoors (dungeons, castles, houses) and in the rain the floor reflects the characters and what stands on it; a "
     "dry field does not shine. Direct3D 12.",
     "У приміщеннях (підземелля, замки, будинки) і під дощем підлога відбиває персонажів і те, що на ній стоїть; сухе "
     "поле не блищить. Direct3D 12.",
     "В помещениях (подземелья, замки, дома) и под дождём пол отражает персонажей и то, что на нём стоит; сухое поле "
     "не блестит. Direct3D 12."},
    {Text::FxFloorReflectStrength, "Floor shine",
     "Блиск підлоги",
     "Блеск пола"},
    {Text::FxFloorReflectStrengthHint, "How much the floor reflects; in the rain more.",
     "Наскільки підлога відбиває; під дощем більше.",
     "Насколько пол отражает; под дождём больше."},
    {Text::FxShafts,  // 3.0.7-beta.5: the lamps' a part of their own (shafts_lamps)
     "Glowing air at fire", "Сяюче повітря біля вогню", "Светящийся воздух у огня"},
    {Text::FxShaftsHint,
     "The air glows around torches, burning skills, lava and portals, as in a light mist (walls in front hide it); on "
     "the fields rays of light stream from the sky past the edge of the map. Direct3D 12.",
     "Повітря світиться навколо факелів, палаючих умінь, лави й порталів, як у легкому серпанку (стіни попереду його "
     "ховають); на полях промені світла ллються з неба за краєм карти. Direct3D 12.",
     "Воздух светится вокруг факелов, горящих умений, лавы и порталов, как в лёгкой дымке (стены впереди его "
     "прячут); на полях лучи света льются с неба за краем карты. Direct3D 12."},
    {Text::FxShaftsStrength, "Light in the air strength",
     "Сила світла в повітрі",
     "Сила света в воздухе"},
    {Text::FxShaftsStrengthHint, "How bright the glowing air and the rays are.",
     "Наскільки яскраві повітря, що світиться, і промені.",
     "Насколько ярки светящийся воздух и лучи."},
    {Text::FxDayCycle, "The sun moves by the clock", "Сонце рухається за годинником", "Солнце движется по часам"},
    {Text::FxDayCycleHint,
     "The sun moves by the Clock's hour: the characters' shadows turn and are long in the morning and the evening, "
     "short at noon; the light is pink in the morning and golden in the evening.",
     "Сонце рухається за годиною «Годинника»: тіні персонажів повертаються, довгі вранці й увечері, короткі опівдні; "
     "світло рожеве вранці й золоте ввечері.",
     "Солнце движется по часу «Часов»: тени персонажей поворачиваются, длинные утром и вечером, короткие в полдень; "
     "свет розовый утром и золотой вечером."},
    {Text::SmoothTaa, "TAA (any graphics card)",
     "TAA (будь-яка відеокарта)",
     "TAA (любая видеокарта)"},
    {Text::SmoothTaaHint,
     "Temporal anti-aliasing of the renderer's own for cards without DLSS (AMD, Intel): each frame is blended with "
     "the last, so edges and thin lines stop shimmering. Direct3D 12; DLSS goes first; MSAA is not used with it.",
     "Власне часове згладжування рендера для карт без DLSS (AMD, Intel): кожен кадр змішується з попереднім, тож краї "
     "й тонкі лінії перестають мерехтіти. Direct3D 12; DLSS має перевагу; MSAA разом із ним не використовується.",
     "Собственное временное сглаживание рендера для карт без DLSS (AMD, Intel): каждый кадр смешивается с предыдущим, "
     "поэтому края и тонкие линии перестают мерцать. Direct3D 12; DLSS в приоритете; MSAA вместе с ним не "
     "используется."},
    {Text::FramesKeepFps, "Keep the frame rate",
     "Тримати частоту кадрів",
     "Держать частоту кадров"},
    {Text::FramesKeepFpsHint,
     "When the game runs below this many frames a second for ten seconds because of the graphics card, the costliest "
     "effect is turned off for the rest of the game (the log says which), then the next if needed. Slow for the "
     "processor's part (a crowd, a town), the effects stay: turning them off would not help. The settings file is not "
     "changed. 0 = never.",
     "Якщо гра десять секунд іде повільніше за стільки кадрів на секунду через відеокарту, найдорожчий ефект "
     "вимикається до кінця гри (лог каже, який), за потреби наступний. Коли гальмує процесор (натовп, місто), ефекти "
     "лишаються: вимикати їх марно. Файл налаштувань не змінюється. 0 — ніколи.",
     "Если игра десять секунд идёт медленнее стольких кадров в секунду из-за видеокарты, самый дорогой эффект "
     "выключается до конца игры (лог говорит, какой), при необходимости следующий. Когда тормозит процессор (толпа, "
     "город), эффекты остаются: выключать их бесполезно. Файл настроек не меняется. 0 — никогда."},
    // 3.0.18-beta.186: the game's own light skill effects when the frames run short
    {Text::FramesLightSkills, "Lighter skill effects when frames run short", "Легші ефекти вмінь, коли бракує кадрів",
     "Облегчённые эффекты умений, когда не хватает кадров"},
    {Text::FramesLightSkillsHint,
     "Below the frame rate kept above with many skills on the screen (a fight, a guild war), the game's own lighter "
     "skill effects - as its /mineffect - for the new effects; its full ones again when the frames are back or the "
     "skills gone for 20 s. Your own /mineffect stays as you set it.",
     "Коли кадрів менше, ніж утримувати вище, а на екрані багато вмінь (бій, війна гільдій), — власні легші ефекти "
     "вмінь гри, як від її /mineffect, для нових ефектів; повні — знову, коли кадри повернулися або вмінь немає 20 с. "
     "Ваш власний /mineffect лишається, як ви його поставили.",
     "Когда кадров меньше, чем держать выше, а на экране много умений (бой, война гильдий), — собственные облегчённые "
     "эффекты умений игры, как от её /mineffect, для новых эффектов; полные — снова, когда кадры вернулись или умений "
     "нет 20 с. Ваш собственный /mineffect остаётся, как вы его поставили."},
    {Text::LightSkillsOnNote, "Skill effects lighter: {1} fps of {2}", "Ефекти вмінь легші: {1} к/с із {2}",
     "Эффекты умений облегчены: {1} к/с из {2}"},
    {Text::FxCostMs, " · ~{1} ms",
     " · ~{1} мс",
     " · ~{1} мс"},
    {Text::FxBloom, "Bloom of bright places",
     "Сяйво яскравих місць",
     "Свечение ярких мест"},
    {Text::FxBloomHint,
     "The brightest parts of the picture - sunlit tiles, snow, windows, fire - glow softly around. Direct3D 12.",
     "Найяскравіші частини картинки — освітлена сонцем плитка, сніг, вікна, вогонь — м’яко світяться навколо. "
     "Direct3D 12.",
     "Самые яркие части картинки — освещённая солнцем плитка, снег, окна, огонь — мягко светятся вокруг. Direct3D 12."},
    {Text::FxBloomStrength, "Bloom strength",
     "Сила сяйва",
     "Сила свечения"},
    {Text::FxBloomStrengthHint, "How much the bright places glow.",
     "Наскільки сяють яскраві місця.",
     "Насколько светятся яркие места."},
    {Text::FxRegionLooks, "Colours of each region",
     "Кольори кожного регіону",
     "Цвета каждого региона"},
    {Text::FxRegionLooksHint,
     "Each region gets a colour mood of its own: Payon warm and green, Niflheim cold and pale, Morroc sun-bleached, "
     "Lutie blue and clean, Glast Heim grim, Einbroch smoky.",
     "Кожен регіон має власний настрій кольорів: Пайон теплий і зелений, Ніфльгейм холодний і блідий, Моррок "
     "вигорілий на сонці, Лютіє блакитний і чистий, Гласт Гейм похмурий, Ейнброх задимлений.",
     "У каждого региона своё настроение цветов: Пайон тёплый и зелёный, Нифльхейм холодный и бледный, Морок "
     "выгоревший на солнце, Лютие голубой и чистый, Гласт Хейм мрачный, Эйнброх задымлённый."},
    {Text::FxRegionLooksStrength, "Region colours strength",
     "Сила кольорів регіону",
     "Сила цветов региона"},
    {Text::FxRegionLooksStrengthHint, "How strong the region's colour mood is.",
     "Наскільки сильний настрій кольорів регіону.",
     "Насколько сильно настроение цветов региона."},
    {Text::FxFootprints, "Footprints in snow and sand", "Сліди на снігу й піску", "Следы на снегу и песке"},
    {Text::FxFootprintsHint,
     "Characters leave footprints in the snow and the sand that slowly fade. Needs Shadows from the sun and fire. "
     "Direct3D 12.",
     "Персонажі лишають сліди на снігу й піску, що поволі зникають. Потрібні «Тіні від сонця й вогню». Direct3D 12.",
     "Персонажи оставляют следы на снегу и песке, которые понемногу исчезают. Нужны «Тени от солнца и огня». "
     "Direct3D 12."},
    {Text::FxStorms, "Thunderstorms",
     "Грози",
     "Грозы"},
    {Text::FxStormsHint,
     "Where it rains (with Precipitation on), lightning flashes now and then and lights up everything for a moment. "
     "Direct3D 12.",
     "Де йде дощ (з увімкненими «Опадами»), час від часу спалахує блискавка й на мить освітлює все. Direct3D 12.",
     "Где идёт дождь (с включёнными «Осадками»), время от времени сверкает молния и на миг освещает всё. Direct3D 12."},
    {Text::FxGrass, "Grass",
     "Трава",
     "Трава"},
    {Text::FxGrassHint,
     "Blades of grass grow on the green ground of the fields and towns, sway in the wind and lie down where "
     "characters walk. Costly for the graphics card. Direct3D 12.",
     "На зеленій землі полів і міст ростуть травинки, гойдаються на вітрі й приминаються там, де йдуть персонажі. "
     "Дорого для відеокарти. Direct3D 12.",
     "На зелёной земле полей и городов растут травинки, качаются на ветру и приминаются там, где идут персонажи. "
     "Дорого для видеокарты. Direct3D 12."},
    {Text::FxGrassAmount, "How thick the grass is",
     "Наскільки густа трава",
     "Насколько густая трава"},
    {Text::FxGrassAmountHint, "How many blades grow.",
     "Скільки травинок росте.",
     "Сколько травинок растёт."},
    {Text::FxSkillSparks, "Sparks of skills",
     "Іскри вмінь",
     "Искры умений"},
    {Text::FxSkillSparksHint,
     "Embers rise from burning skills (Fire Wall, Fire Bolt, Meteor Storm), and glints of their colour spread from "
     "the other bright skills. Needs the skills' light. Direct3D 12.",
     "Від палаючих умінь (Fire Wall, Fire Bolt, Meteor Storm) здіймаються жаринки, а від інших яскравих умінь "
     "розлітаються іскорки їхнього кольору. Потрібне світло вмінь. Direct3D 12.",
     "От горящих умений (Fire Wall, Fire Bolt, Meteor Storm) поднимаются угольки, а от других ярких умений "
     "разлетаются искорки их цвета. Нужен свет умений. Direct3D 12."},
    {Text::FxSkillSparksAmount, "Sparks brightness",
     "Яскравість іскор",
     "Яркость искр"},
    {Text::FxSkillSparksAmountHint, "How bright the sparks are.",
     "Наскільки яскраві іскри.",
     "Насколько ярки искры."},
    {Text::FxMapFade, "Smooth map change",
     "Плавна зміна карти",
     "Плавная смена карты"},
    {Text::FxMapFadeHint,
     "After a warp or a new map the world comes in smoothly from black instead of at once. Direct3D 12.",
     "Після телепорту чи нової карти світ плавно проявляється з темряви, а не одразу. Direct3D 12.",
     "После телепорта или новой карты мир плавно проявляется из темноты, а не сразу. Direct3D 12."},
    {Text::FxRoundLight, "Volume from fire and lamps", "Об'єм від вогню й ламп", "Объём от огня и ламп"},
    {Text::FxRoundLightHint,
     "Torches, lamps and fire light a character as if it were round: the side towards the fire brighter, the far one "
     "darker.",
     "Факели, лампи й вогонь освітлюють персонажа як округлого: бік до вогню світліший, дальній темніший.",
     "Факелы, лампы и огонь освещают персонажа как округлого: сторона к огню светлее, дальняя темнее."},
    {Text::FramesSavePower, "Save power in the background",
     "Економити енергію у фоні",
     "Экономить энергию в фоне"},
    {Text::FramesSavePowerHint,
     "While another window is in front or the game is minimised it draws 15 frames a second; on the login and the "
     "character select 60.",
     "Поки попереду інше вікно або гру згорнуто, вона малює 15 кадрів на секунду; на вході й виборі персонажа — 60.",
     "Пока впереди другое окно или игра свёрнута, она рисует 15 кадров в секунду; на входе и выборе персонажа — 60."},
    {Text::DiagProfile, "Measure where the game spends its time",
     "Заміряти, на що гра витрачає час",
     "Замерять, на что игра тратит время"},
    {Text::DiagProfileHint,
     "For the developers: about a thousand times a second the driver notes where uaRO.exe is busy, and it cuts each "
     "frame into its parts - the game's world, its interface, each effect, the world made larger, the picture into "
     "the window - with the card's and the processor's time of each; every 10 seconds all of it goes into the log. "
     "Costs a little speed - turn it off afterwards.",
     "Для розробників: приблизно тисячу разів на секунду драйвер помічає, чим зайнятий uaRO.exe, і розкладає кожен "
     "кадр на частини — світ гри, інтерфейс, кожен ефект, збільшення світу, вивід у вікно — з часом відеокарти й "
     "процесора на кожну; кожні 10 секунд усе це йде в лог. Трохи сповільнює — потім вимкніть.",
     "Для разработчиков: примерно тысячу раз в секунду драйвер отмечает, чем занят uaRO.exe, и раскладывает каждый "
     "кадр на части — мир игры, интерфейс, каждый эффект, увеличение мира, вывод в окно — со временем видеокарты и "
     "процессора на каждую; каждые 10 секунд всё это идёт в лог. Немного замедляет — потом выключите."},
    {Text::GfxSoundOurs, "Sound through uaRO Graphics",
     "Звук через uaRO Graphics",
     "Звук через uaRO Graphics"},
    {Text::GfxSoundRate, "Sound quality",
     "Якість звуку",
     "Качество звука"},
    {Text::GfxSoundRateHint,
     "The game mixes its sound at 22050 Hz at most - the music, recorded at 44100 Hz, loses its highs. 44100 or 48000 "
     "Hz plays it as it was recorded.",
     "Гра змішує звук щонайбільше на 22050 Гц - музика, записана на 44100 Гц, втрачає високі частоти. 44100 чи 48000 "
     "Гц грають її такою, як записано.",
     "Игра смешивает звук максимум на 22050 Гц - музыка, записанная на 44100 Гц, теряет высокие частоты. 44100 или "
     "48000 Гц играют её такой, как записано."},
    {Text::GfxSoundRateGame, "The game's (setup.exe)",
     "Як у грі (setup.exe)",
     "Как в игре (setup.exe)"},
    {Text::GfxSoundRate44, "44100 Hz",
     "44100 Гц",
     "44100 Гц"},
    {Text::GfxSoundRate48, "48000 Hz",
     "48000 Гц",
     "48000 Гц"},
    {Text::GfxSoundVoices, "The most sounds at once",
     "Найбільше звуків одночасно",
     "Больше всего звуков одновременно"},
    {Text::GfxSoundVoicesHint,
     "48 sounds at once whatever setup.exe says (16 or 32 there): skills in a crowd are not cut short.",
     "48 звуків одночасно, хоч би що стояло в setup.exe (там 16 чи 32): уміння в натовпі не обриваються.",
     "48 звуков одновременно, что бы ни стояло в setup.exe (там 16 или 32): умения в толпе не обрываются."},
    {Text::GfxSoundBackground, "Volume in the background",
     "Гучність у фоні",
     "Громкость в фоне"},
    {Text::GfxSoundBackgroundHint,
     "While another window is in front or the game is minimised: 0 is silence, 100 as loud as in front.",
     "Коли попереду інше вікно або гру згорнуто: 0 - тиша, 100 - так само гучно, як попереду.",
     "Когда впереди другое окно или игра свёрнута: 0 - тишина, 100 - так же громко, как впереди."},
    {Text::GfxSoundLimiter, "Even out the loudness in a crowd",
     "Вирівнювати гучність у натовпі",
     "Выравнивать громкость в толпе"},
    {Text::GfxSoundLimiterHint,
     "Past six sounds at once each is made a little quieter, so that many skills together do not crackle.",
     "Понад шість звуків одночасно кожен стає трохи тихішим, щоб багато вмінь разом не хрипіли.",
     "Больше шести звуков одновременно - каждый становится чуть тише, чтобы много умений вместе не хрипели."},
    {Text::GfxSoundStereo, "Left and right",
     "Ліворуч і праворуч",
     "Слева и справа"},
    {Text::GfxSoundStereoHint,
     "A sound beside your character comes from that side of the screen: how far to the side. 0 is the game's "
     "(everything in the middle).",
     "Звук збоку від персонажа лунає з того боку екрана: наскільки сильно. 0 - як у грі (усе посередині).",
     "Звук сбоку от персонажа звучит с той стороны экрана: насколько сильно. 0 - как в игре (всё посередине)."},
    {Text::GfxSoundStereoFlip, "Swap left and right", "Поміняти лівий і правий канали",
     "Поменять левый и правый каналы"},
    {Text::GfxSoundStereoFlipHint, "For headphones or speakers connected the other way round.",
     "Для навушників чи колонок, підключених навпаки.", "Для наушников или колонок, подключённых наоборот."},
    {Text::GfxSoundSteal, "Never lose a new sound",
     "Не губити нові звуки",
     "Не терять новые звуки"},
    {Text::GfxSoundStealHint,
     "When every sound channel is busy the quietest (the farthest) stops for the new one - the game dropped the new "
     "one.",
     "Коли всі звукові канали зайняті, найтихіший (найдальший) зупиняється заради нового - гра відкидала новий.",
     "Когда все звуковые каналы заняты, самый тихий (самый дальний) останавливается ради нового - игра отбрасывала "
     "новый."},
    {Text::GfxSoundCrossfade, "Smooth change of music",
     "Плавна зміна музики",
     "Плавная смена музыки"},
    {Text::GfxSoundCrossfadeHint,
     "Moving to another map the old music fades out and the new one in over 1.5 seconds instead of breaking off.",
     "Під час переходу на іншу мапу стара музика затихає, а нова наростає за 1,5 секунди, а не обривається.",
     "При переходе на другую карту старая музыка затихает, а новая нарастает за 1,5 секунды, а не обрывается."},
    {Text::FxRipples, "Ripples from steps",
     "Кола від кроків",
     "Круги от шагов"},
    {Text::FxRipplesHint,
     "Rings spread on the water where characters and monsters walk through it (with the modern water, Direct3D 12).",
     "По воді розходяться кола там, де крізь неї йдуть персонажі й монстри (із сучасною водою, Direct3D 12).",
     "По воде расходятся круги там, где через неё идут персонажи и монстры (с современной водой, Direct3D 12)."},
    {Text::FxGlint, "Cards on the ground sparkle",
     "Картки на землі виблискують",
     "Карты на земле поблёскивают"},
    {Text::FxGlintHint,
     "A card that dropped flashes a small star now and then, so that it is not missed among the loot.",
     "Картка, що випала, час від часу спалахує маленькою зіркою, щоб її не пропустити серед здобичі.",
     "Выпавшая карта время от времени вспыхивает маленькой звёздочкой, чтобы её не пропустить среди добычи."},
    {Text::FxHitFlash, "Flash on a hit",
     "Спалах від удару",
     "Вспышка при ударе"},
    {Text::FxHitFlashHint,
     "A monster that is hit lights up white for a moment, as in today's 2D games. Needs the game's sound on: the hit "
     "is known by its sound.",
     "Монстр, по якому влучили, на мить спалахує білим, як у сучасних 2D-іграх. Потрібен увімкнений звук гри: удар "
     "впізнається за звуком.",
     "Монстр, по которому попали, на мгновение вспыхивает белым, как в современных 2D-играх. Нужен включённый звук "
     "игры: удар узнаётся по звуку."},
    // 3.0.16-beta.7
    {Text::FxDeathFlash, "Flash on a death", "Спалах при смерті", "Вспышка при смерти"},
    {Text::FxDeathFlashHint,
     "A monster that dies lights up white, brighter and longer than from a hit. Needs the game's sound on: the death "
     "is known by its sound.",
     "Монстр, що гине, спалахує білим — яскравіше й довше, ніж від удару. Потрібен увімкнений звук гри: смерть "
     "впізнається за звуком.",
     "Умирающий монстр вспыхивает белым — ярче и дольше, чем от удара. Нужен включённый звук игры: смерть узнаётся "
     "по звуку."},
    // 3.0.16-beta.13 (the owner's picks 1, 5, 6, 15, 19, 20 of «20 идей»)
    {Text::FxBodyTints, "Body tints", "Відтінок тіла", "Оттенок тела"},
    {Text::FxBodyTintsHint,
     "Your character's body takes a soft colour while a state is on: a warm red under Fury, cool steel under Steel "
     "Body. Off at first.",
     "Тіло вашого персонажа набуває м'якого кольору, поки діє стан: теплий червоний під Fury, холодний сталевий під "
     "Steel Body. Спочатку вимкнено.",
     "Тело вашего персонажа приобретает мягкий цвет, пока действует состояние: тёплый красный под Fury, холодный "
     "стальной под Steel Body. Сначала выключено."},
    // 3.0.16-beta.16 (the owner: «тени от факелов и ламп»)
    {Text::FxTorchShadows, "Shadows from torches and lamps", "Тіні від факелів і ламп", "Тени от факелов и ламп"},
    {Text::FxTorchShadowsHint,
     "Walls, columns, houses, hills and characters stop the light of the nearest torches and lamps: what stands "
     "behind them stays in shadow, the shadows soft at their edges. Off at first.",
     "Стіни, колони, будинки, пагорби й персонажі затримують світло найближчих факелів і ламп: те, що за ними, "
     "лишається в тіні, краї тіней м'які. Спочатку вимкнено.",
     "Стены, колонны, дома, холмы и персонажи задерживают свет ближайших факелов и ламп: то, что за ними, остаётся "
     "в тени, края теней мягкие. Сначала выключено."},
    {Text::FxTorchShadowsStrength, "Shadow strength", "Сила тіні", "Сила тени"},
    {Text::FxTorchShadowsStrengthHint,
     "How much of a torch's or a lamp's light a shadow takes away: 100 % all of it, less leaves some light, as if "
     "bounced off the walls.",
     "Скільки світла факела чи лампи забирає тінь: 100 % — усе, менше — лишає трохи світла, ніби відбитого від "
     "стін.",
     "Сколько света факела или лампы забирает тень: 100 % — весь, меньше — оставляет немного света, будто "
     "отражённого от стен."},
    {Text::FxBodyStamp, "Effects behind the character", "Ефекти за героєм", "Эффекты за героем"},
    {Text::FxBodyStampHint,
     "An effect behind a character - the far half of a ring round it, a fire at its back - hides behind its figure, "
     "the part in front of it is drawn over as before. Off at first.",
     "Ефект позаду персонажа — дальня половина кола навколо нього, вогонь за спиною — ховається за його фігурою, а те, "
     "що попереду, малюється поверх, як і раніше. Спочатку вимкнено.",
     "Эффект позади персонажа — дальняя половина круга вокруг него, огонь за спиной — прячется за его фигурой, а то, "
     "что впереди, рисуется поверх, как и раньше. Сначала выключено."},
    {Text::FxSharpSprites, "Sharp character pixels", "Чіткі пікселі героїв", "Чёткие пиксели героев"},
    {Text::FxSharpSpritesHint,
     "Close up, the sprites of characters, monsters and NPCs are drawn as clean squares of their pixels with a soft "
     "edge - no blur, no flicker. Far off they look as before. Off at first.",
     "Зблизька спрайти персонажів, монстрів і NPC малюються чистими квадратиками своїх пікселів з м'яким краєм — без "
     "розмиття й мерехтіння. Здалеку — як і раніше. Спочатку вимкнено.",
     "Вблизи спрайты персонажей, монстров и NPC рисуются чистыми квадратиками своих пикселей с мягким краем — без "
     "размытия и мерцания. Издалека — как и раньше. Сначала выключено."},
    {Text::FxTorchLeafLight, "Light through the leaves", "Світло крізь листя", "Свет сквозь листву"},
    {Text::FxTorchLeafLightHint,
     "A tree's leaves between a torch or a lamp and the ground do not cast a black shadow: a part of the light comes "
     "through, green, in spots. Off at first.",
     "Листя дерева між факелом чи лампою і землею не дає чорної тіні: частина світла проходить крізь нього, зеленими "
     "плямами. Спочатку вимкнено.",
     "Листва дерева между факелом или лампой и землёй не даёт чёрной тени: часть света проходит сквозь неё, зелёными "
     "пятнами. Сначала выключено."},
    {Text::FxHallucinationWave, "Hallucination wave", "Хвиля від Hallucination", "Волна от Hallucination"},
    {Text::FxHallucinationWaveHint,
     "While your character is under Hallucination, the picture of the world sways in slow waves, as in newer "
     "clients. The interface stays still. Off at first.",
     "Поки на персонажі Hallucination, картинка світу гойдається повільними хвилями, як у новіших клієнтах. "
     "Інтерфейс лишається нерухомим. Спочатку вимкнено.",
     "Пока на персонаже Hallucination, картинка мира колышется медленными волнами, как в новых клиентах. "
     "Интерфейс остаётся неподвижным. Сначала выключено."},
    {Text::InterfaceSkillNeeds, "What a skill lacks", "Чого бракує навичці", "Чего не хватает навыку"},
    {Text::InterfaceSkillNeedsHint,
     "A skill on the bar that cannot go for its gem or its spirit spheres is dimmed with a red plate: ×0 when the "
     "gem (Holy Water, a stone) is not in the bag, 3/5 the spheres you have and it needs. Off at first.",
     "Навичка на панелі, якій бракує каменя чи сфер, тьмяніє з червоною плашкою: ×0, коли каменя (святої води, "
     "каменя ніндзя) немає в сумці, 3/5 — скільки сфер є і скільки треба. Спочатку вимкнено.",
     "Навык на панели, которому не хватает камня или сфер, тускнеет с красной плашкой: ×0, когда камня (святой "
     "воды, камня ниндзя) нет в сумке, 3/5 — сколько сфер есть и сколько нужно. Сначала выключено."},
    {Text::InterfaceComboWindow, "Combo window", "Вікно комбо", "Окно комбо"},
    {Text::InterfaceComboWindowHint,
     "The Monk's combo on the skill bar: the skill that chains now (Chain Combo after Triple Attack, then Combo "
     "Finish, Tiger Fist, Chain Crush, Asura) is outlined in gold with its time shrinking along its bottom; Asura "
     "glows blue when it can go - five spheres and Fury on. Off at first.",
     "Комбо монаха на панелі навичок: навичка, яку можна вдарити зараз (Chain Combo після Triple Attack, далі Combo "
     "Finish, Tiger Fist, Chain Crush, Asura), обведена золотом, а її час збігає смужкою внизу; Asura світиться "
     "синім, коли готова — п'ять сфер і Fury. Спочатку вимкнено.",
     "Комбо монаха на панели навыков: навык, который можно ударить сейчас (Chain Combo после Triple Attack, дальше "
     "Combo Finish, Tiger Fist, Chain Crush, Asura), обведён золотом, а его время убывает полоской внизу; Asura "
     "светится синим, когда готова — пять сфер и Fury. Сначала выключено."},
    {Text::InterfaceBrokenGear, "Broken gear", "Зламане спорядження", "Сломанное снаряжение"},
    {Text::InterfaceBrokenGearHint,
     "When a weapon or armour breaks (a Blacksmith's skill, a monster, Acid Terror) a short sound and a red Broken "
     "with its name in the character block until it is repaired. Off at first.",
     "Коли ламається зброя чи броня (навичка коваля, монстр, Acid Terror) — короткий сигнал і червоне Зламано: з її "
     "назвою в блоці персонажа, доки її не полагодять. Спочатку вимкнено.",
     "Когда ломается оружие или броня (навык кузнеца, монстр, Acid Terror) — короткий сигнал и красное Сломано с "
     "её названием в блоке персонажа, пока её не починят. Сначала выключено."},
    {Text::InterfacePetHunger, "Pet and homunculus hunger", "Голод пета й гомункула", "Голод пета и гомункула"},
    {Text::InterfacePetHungerHint,
     "The pet's and the homunculus's fullness in the character block - amber when hungry (25 % and less), red when "
     "very (10 %), with a short sound then: feed it before it runs away. Off at first.",
     "Ситість пета й гомункула в блоці персонажа — бурштинова, коли голодний (25 % і менше), червона, коли дуже (10 "
     "%), і тоді короткий сигнал: нагодуйте, доки не втік. Спочатку вимкнено.",
     "Сытость пета и гомункула в блоке персонажа — янтарная, когда голоден (25 % и меньше), красная, когда очень "
     "(10 %), и тогда короткий сигнал: покормите, пока не сбежал. Сначала выключено."},
    {Text::InterfaceGroundTimers, "Time rings on the ground", "Таймери на землі", "Таймеры на земле"},
    {Text::InterfaceGroundTimersHint,
     "Your own Safety Wall, Pneuma, Sanctuary and traps get a ring on the ground in the skill's colour that runs down "
     "as their time goes, pulsing in the last 3 seconds; a broken wall or a sprung trap takes it away. Off at first.",
     "Ваші Safety Wall, Pneuma, Sanctuary і пастки отримують кільце на землі в кольорі навички, яке зменшується, "
     "поки спливає їхній час, і блимає в останні 3 секунди; зламана стіна чи спрацьована пастка його прибирає. "
     "Спочатку вимкнено.",
     "Ваши Safety Wall, Pneuma, Sanctuary и ловушки получают кольцо на земле в цвете навыка, которое уменьшается, "
     "пока истекает их время, и мигает в последние 3 секунды; сломанная стена или сработавшая ловушка его убирает. "
     "Сначала выключено."},
    {Text::HotbarNoItem, "No {1} in the bag", "Немає {1} у сумці", "Нет {1} в сумке"},
    {Text::HotbarNoSpheres, "Spheres: needs {1}, you have {2}", "Сфери: потрібно {1}, є {2}",
     "Сферы: нужно {1}, есть {2}"},
    {Text::CharBroken, "Broken: {1}", "Зламано: {1}", "Сломано: {1}"},
    {Text::CharPet, "Pet", "Пет", "Пет"},
    {Text::CharHomun, "Homunculus", "Гомункул", "Гомункул"},
    {Text::FxWetGround, "Wet ground and puddles", "Мокра земля й калюжі", "Мокрая земля и лужи"},
    {Text::FxWetGroundHint,
     "On maps where it rains the ground is a little darker and puddles here and there mirror the houses, the trees "
     "and the characters (with Precipitation on, Direct3D 12).",
     "На мапах, де йде дощ, земля трохи темніша, а калюжі подекуди віддзеркалюють будинки, дерева й персонажів (з "
     "увімкненими «Опадами», Direct3D 12).",
     "На картах, где идёт дождь, земля чуть темнее, а лужи кое-где отражают дома, деревья и персонажей (с включёнными "
     "«Осадками», Direct3D 12)."},
    {Text::GfxSoundEngine, "Surround sound (new engine)",
     "Об'ємний звук (новий рушій)",
     "Объёмный звук (новый движок)"},
    {Text::GfxSoundEngineHint,
     "The sounds of monsters, skills and hits played by uaRO Graphics itself: around you on any speakers (stereo, "
     "5.1, 7.1), quieter and duller far off, with the echo of the place. Windows 10 and later; without it the game's "
     "own sound.",
     "Звуки монстрів, умінь і ударів грає сам uaRO Graphics: навколо вас на будь-яких колонках (стерео, 5.1, 7.1), "
     "далекі тихіші й глухіші, з відлунням місця. Windows 10 і новіші; без цього — власний звук гри.",
     "Звуки монстров, умений и ударов играет сам uaRO Graphics: вокруг вас на любых колонках (стерео, 5.1, 7.1), "
     "дальние тише и глуше, с эхом места. Windows 10 и новее; без этого — собственный звук игры."},
    {Text::GfxSoundReverb, "Echo of the place",
     "Відлуння місця",
     "Эхо места"},
    {Text::GfxSoundReverbHint,
     "With the new engine: caves and dungeons echo long, houses short, towns a little, fields hardly. 0 is none.",
     "З новим рушієм: печери й підземелля відлунюють довго, будинки коротко, міста трохи, поля майже ні. 0 — без "
     "відлуння.",
     "С новым движком: пещеры и подземелья отдаются долгим эхом, дома коротким, города немного, поля почти нет. 0 — "
     "без эха."},
    {Text::GfxSoundHeadphones, "Headphones: Windows' spatial sound",
     "Навушники: просторовий звук Windows",
     "Наушники: пространственный звук Windows"},
    {Text::GfxSoundHeadphonesHint,
     "With the new engine, when Windows Sonic for Headphones or Dolby Atmos is turned on in Windows (the speaker's "
     "properties, Spatial sound): the sounds placed in three dimensions - behind, in front, beside - in headphones "
     "too.",
     "З новим рушієм, коли у Windows увімкнено Windows Sonic для навушників або Dolby Atmos (властивості динаміка, "
     "«Просторовий звук»): звуки розставлено в трьох вимірах — позаду, попереду, збоку — і в навушниках.",
     "С новым движком, когда в Windows включён Windows Sonic для наушников или Dolby Atmos (свойства динамика, "
     "«Пространственный звук»): звуки расставлены в трёх измерениях — сзади, спереди, сбоку — и в наушниках."},
    {Text::GfxSoundDuck, "Music makes way in a fight",
     "Музика поступається в бою",
     "Музыка уступает в бою"},
    {Text::GfxSoundDuckHint,
     "While many sounds play at once (a fight, a crowd's skills) the music goes quieter, down to 65 %, and comes back "
     "over two seconds.",
     "Коли одночасно звучить багато звуків (бій, уміння натовпу), музика тихішає до 65 % і за дві секунди "
     "повертається.",
     "Когда одновременно звучит много звуков (бой, умения толпы), музыка становится тише, до 65 %, и за две секунды "
     "возвращается."},
    {Text::AutoUiWhole, "The interface at {1} % - a whole number of times the game's pixels, the crispest letters.",
     "Інтерфейс {1} % - ціле збільшення пікселів гри, найчіткіші літери.",
     "Интерфейс {1} % - целое увеличение пикселей игры, самые чёткие буквы."},
    {Text::AutoDlaa, "Smoothing: NVIDIA DLAA (the picture at its own size).",
     "Згладжування: NVIDIA DLAA (картинка у власному розмірі).",
     "Сглаживание: NVIDIA DLAA (картинка в собственном размере)."},
    {Text::AutoDlssQuality, "Smoothing: NVIDIA DLSS Quality.",
     "Згладжування: NVIDIA DLSS Quality.",
     "Сглаживание: NVIDIA DLSS Quality."},
    {Text::AutoSoundEngine, "Sound: around you on any speakers; with Windows Sonic in headphones too.",
     "Звук: навколо вас на будь-яких колонках; з Windows Sonic і в навушниках.",
     "Звук: вокруг вас на любых колонках; с Windows Sonic и в наушниках."},
    {Text::PagePad, "Controller",
     "Контролер",
     "Контроллер"},  // 3.0.11-beta.89: was «Геймпад»
    {Text::PadHeading, "The controller in the game",
     "Контролер у грі",
     "Контроллер в игре"},
    {Text::PadIntro,
     "Xbox controllers and PlayStation ones (DualShock 4, DualSense - over USB or Bluetooth) play the game: the left "
     "stick walks, A attacks, the other buttons use the skill bar's cells, the right stick picks the target, D-pad "
     "down talks to the nearest NPC. While you play with one, the interface shows its buttons and the target stands in "
     "a gold ring.",
     "Контролери Xbox і PlayStation (DualShock 4, DualSense - через USB або Bluetooth) керують грою: лівий стік веде "
     "персонажа, A атакує, інші кнопки тиснуть клітинки панелі навичок, правий стік вибирає ціль, хрестовина вниз "
     "говорить із найближчим NPC. Поки граєте з контролером, інтерфейс показує його кнопки, а ціль стоїть у золотому "
     "кільці.",
     "Контроллеры Xbox и PlayStation (DualShock 4, DualSense - по USB или Bluetooth) управляют игрой: левый стик ведёт "
     "персонажа, A атакует, остальные кнопки жмут ячейки панели навыков, правый стик выбирает цель, крестовина вниз "
     "говорит с ближайшим NPC. Пока играете с контроллером, интерфейс показывает его кнопки, а цель стоит в золотом "
     "кольце."},
    {Text::PadStateOff, "Off: the game starts without the pad.",
     "Вимкнено: гра запускається без геймпада.",
     "Выключен: игра запускается без геймпада."},
    {Text::PadStateOn, "On: uaRO Pad {1}.",
     "Увімкнено: uaRO Pad {1}.",
     "Включено: uaRO Pad {1}."},
    {Text::PadStateOld,
     "An old uaRO Pad dinput.dll is still in the game's folder: close the game and open this app again - it takes it "
     "away.",
     "У теці гри ще старий dinput.dll від uaRO Pad: закрий гру й відкрий утиліту знову - вона його прибере.",
     "В папке игры ещё старый dinput.dll от uaRO Pad: закрой игру и открой утилиту снова - она его уберёт."},
    {Text::PadFailed, "Could not change {1}: close the game, or start uaro-graphics.exe as administrator.",
     "Не вдалося змінити {1}: закрий гру або запусти uaro-graphics.exe від імені адміністратора.",
     "Не удалось изменить {1}: закрой игру или запусти uaro-graphics.exe от имени администратора."},
    {Text::PadController, "Controller",
     "Контролер",
     "Контроллер"},
    {Text::PadType, "Controller",
     "Геймпад",
     "Геймпад"},
    {Text::PadTypeHint, "Any: an Xbox controller if one is connected, else a PlayStation one.",
     "Будь-який: геймпад Xbox, якщо він підключений, інакше PlayStation.",
     "Любой: геймпад Xbox, если он подключён, иначе PlayStation."},
    {Text::PadTypeAuto, "Any (Xbox first)",
     "Будь-який (спершу Xbox)",
     "Любой (сначала Xbox)"},
    {Text::PadTypeSony, "PlayStation (DualShock 4, DualSense)",
     "PlayStation (DualShock 4, DualSense)",
     "PlayStation (DualShock 4, DualSense)"},
    {Text::PadLightbar, "Light bar",
     "Світлова панель",
     "Световая панель"},
    {Text::PadLightbarHint, "The colour of a PlayStation controller's light, over USB and Bluetooth.",
     "Колір світла геймпада PlayStation, через USB і Bluetooth.",
     "Цвет света геймпада PlayStation, по USB и Bluetooth."},
    {Text::ColourBlue, "Blue",
     "Синій",
     "Синий"},
    {Text::ColourRed, "Red",
     "Червоний",
     "Красный"},
    {Text::ColourGreen, "Green",
     "Зелений",
     "Зелёный"},
    {Text::ColourPurple, "Purple",
     "Фіолетовий",
     "Фиолетовый"},
    {Text::ColourOrange, "Orange",
     "Помаранчевий",
     "Оранжевый"},
    {Text::ColourWhite, "White",
     "Білий",
     "Белый"},
    {Text::ColourOff, "Off",
     "Вимкнено",
     "Выключено"},
    {Text::ColourOwn, "Own: {1}",
     "Свій: {1}",
     "Свой: {1}"},
    {Text::PadTouchpad, "Touchpad click",
     "Натиск на тачпад",
     "Нажатие на тачпад"},
    {Text::PadTouchpadHint, "What pressing a PlayStation controller's touchpad does.",
     "Що робить натиск на тачпад геймпада PlayStation.",
     "Что делает нажатие на тачпад геймпада PlayStation."},
    {Text::TouchNothing, "Nothing",
     "Нічого",
     "Ничего"},
    {Text::TouchBack, "Back: the pad off and on",
     "Back: вимкнути й увімкнути геймпад",
     "Back: выключить и включить геймпад"},
    {Text::TouchStart, "Start: the pad's menu",
     "Start: меню геймпада",
     "Start: меню геймпада"},
    {Text::PadRumble, "Vibration",
     "Вібрація",
     "Вибрация"},
    {Text::PadRumbleHint, "Short pulses when the target changes or a skill is refused.",
     "Короткі поштовхи, коли змінюється ціль або вміння не спрацювало.",
     "Короткие толчки, когда меняется цель или умение не сработало."},
    {Text::PadStartsOn, "On when the game starts",
     "Увімкнений на старті гри",
     "Включён при старте игры"},
    {Text::PadStartsOnHint, "Off: the pad waits for Back (Share, Create) to switch on.",
     "Вимкнено: геймпад чекає на Back (Share, Create), щоб увімкнутися.",
     "Выключено: геймпад ждёт Back (Share, Create), чтобы включиться."},
    {Text::PadWalkRadius, "How far the stick walks",
     "Дальність ходу стіком",
     "Дальность хода стиком"},  // 3.0.11-beta.89: was the pad's menu's language (it follows this app's now)
    {Text::PadWalkRadiusHint,
     "How far ahead of the character the left stick walks, a share of the window's shorter side (as before: 22 %).",
     "Наскільки далеко перед персонажем іде лівий стік, частка коротшої сторони вікна (як було: 22 %).",
     "Насколько далеко перед персонажем идёт левый стик, доля короткой стороны окна (как было: 22 %)."},
    {Text::PadSettingsNote,
     "Saved at once (uaro-graphics\\uaro-pad.ini and uaro-pad-binds.ini); the game takes the buttons when the "
     "character enters, the rest when it starts. The controller's menu in the game (Start) changes the same.",
     "Зберігається одразу (uaro-graphics\\uaro-pad.ini та uaro-pad-binds.ini); кнопки гра бере, коли входить "
     "персонаж, решту - під час запуску. Меню контролера в грі (Start) змінює те саме.",
     "Сохраняется сразу (uaro-graphics\\uaro-pad.ini и uaro-pad-binds.ini); кнопки игра берёт, когда входит "
     "персонаж, остальное - при запуске. Меню контроллера в игре (Start) меняет то же самое."},
    {Text::PadNpcHint, "Hint in NPC windows",
     "Підказка у вікнах NPC",
     "Подсказка в окнах NPC"},
    {Text::PadNpcHintHint,
     "While an NPC window is open, a line at the bottom of the game says what the buttons do there.",
     "Поки відкрите вікно NPC, рядок унизу гри підказує, що там роблять кнопки.",
     "Пока открыто окно NPC, строка внизу игры подсказывает, что там делают кнопки."},
    {Text::PadSeen, "The last controller in the game: {1}, {2}.", "Останній контролер у грі: {1}, {2}.",
     "Последний контроллер в игре: {1}, {2}."},  // 3.0.11-beta.89
    {Text::PadSeenNone, "No controller has been connected in the game yet.", "У грі контролер ще не підключали.",
     "В игре контроллер ещё не подключали."},
    {Text::PadOnInterface, "On the interface", "На інтерфейсі", "На интерфейсе"},
    {Text::PadButtons, "Buttons", "Кнопки", "Кнопки"},
    {Text::PadWho, "Character", "Персонаж", "Персонаж"},
    {Text::PadWhoHint, "Each character keeps its own buttons; those without their own take the default ones.",
     "Кожен персонаж має свої кнопки; ті, хто без своїх, беруть типові.",
     "У каждого персонажа свои кнопки; те, у кого своих нет, берут кнопки по умолчанию."},
    {Text::PadWhoDefault, "Default (characters without their own)", "Типові (персонажі без своїх)",
     "По умолчанию (персонажи без своих)"},
    {Text::PadButtonHint, "What this button does in the game: a skill bar's cell, attack, picking up, talking...",
     "Що ця кнопка робить у грі: клітинка панелі навичок, атака, підбирання, розмова...",
     "Что эта кнопка делает в игре: ячейка панели навыков, атака, подбор, разговор..."},
    {Text::PadMark, "The old target mark", "Стара позначка цілі", "Старая метка цели"},
    {Text::PadMarkHint,
     "A state the game shows on the pad's target (a poison tint and the like) - only while «Target's ring on the "
     "ground» is off; with the ring, the ring alone.",
     "Стан, який гра показує на цілі контролера (отруйний відтінок тощо), - лише коли «Кільце цілі на землі» "
     "вимкнене; з кільцем - саме кільце.",
     "Состояние, которое игра показывает на цели контроллера (ядовитый оттенок и т. п.), - только когда «Кольцо цели "
     "на земле» выключено; с кольцом - одно кольцо."},
    {Text::PadMarkOff, "None", "Немає", "Нет"},
    {Text::AiModelAnime6b, "anime 6B (drawn: finer lines, keeps details; 5-7 times slower)",
     "anime 6B (мальоване: тонші лінії, зберігає деталі; у 5-7 разів повільніша)",
     "anime 6B (рисованное: тоньше линии, сохраняет детали; в 5-7 раз медленнее)"},
    {Text::AiGroupsHeading, "Models by group",
     "Моделі за групами",
     "Модели по группам"},
    {Text::AiGroupMonsters, "Monsters and pets",
     "Монстри й улюбленці",
     "Монстры и питомцы"},
    {Text::AiGroupNpcs, "NPCs",
     "NPC",
     "NPC"},
    {Text::AiGroupItems, "Items on the ground",
     "Предмети на землі",
     "Предметы на земле"},
    {Text::AiGroupModelHint,
     "The network that makes this group's pictures: general - realistic, keeps grain (sand, stone), twice anime's "
     "work; anime - drawn, smooth and fast; anime 6B - drawn, finer lines and details, about 14 times anime's work.",
     "Нейромережа, що робить картинки цієї групи: general - реалістична, зберігає зерно (пісок, камінь), удвічі "
     "більше роботи за anime; anime - мальована, гладка й швидка; anime 6B - мальована, тонші лінії й деталі, "
     "приблизно в 14 разів більше роботи за anime.",
     "Нейросеть, которая делает картинки этой группы: general - реалистичная, сохраняет зерно (песок, камень), вдвое "
     "больше работы, чем anime; anime - рисованная, гладкая и быстрая; anime 6B - рисованная, тоньше линии и детали, "
     "примерно в 14 раз больше работы, чем anime."},
    {Text::AiModelSet, "Models",
     "Моделі",
     "Модели"},
    {Text::AiModelSetHint,
     "Fast: anime everywhere. Balanced: general for the world, anime for the rest, anime 6B for the loading screens. "
     "Maximum: general for the world, anime 6B for the rest - a long build of the pack.",
     "Швидкий: anime усюди. Збалансований: general для світу, anime для решти, anime 6B для заставок. Максимум: "
     "general для світу, anime 6B для решти - довге збирання пакета.",
     "Быстрый: anime везде. Сбалансированный: general для мира, anime для остального, anime 6B для заставок. "
     "Максимум: general для мира, anime 6B для остального - долгая сборка пакета."},
    {Text::AiModelSetFast, "Fast",
     "Швидкий",
     "Быстрый"},
    {Text::AiModelSetBalanced, "Balanced",
     "Збалансований",
     "Сбалансированный"},
    {Text::AiModelSetMaximum, "Maximum",
     "Максимум",
     "Максимум"},
    {Text::AiModelSetCustom, "Own",
     "Свій",
     "Свой"},
    {Text::AiGroupInPack, "In the pack: {1}",
     "У пакеті: {1}",
     "В пакете: {1}"},
    {Text::AiGroupInPackOther, "In the pack: {1} (⚠ - made by another model than the group's now)",
     "У пакеті: {1} (⚠ - зроблено іншою моделлю, ніж у групи тепер)",
     "В пакете: {1} (⚠ - сделаны другой моделью, чем у группы сейчас)"},
    {Text::AiNoPackYet, "No texture pack in the game's folder yet.",
     "У теці гри ще немає пакета текстур.",
     "В папке игры ещё нет пакета текстур."},
    {Text::AiUnknownSprites,
     "{1} pictures of the pack have no group of these yet (made before 2.5.28): the next build of the pack writes "
     "their groups without making them again.",
     "{1} картинок пакета ще не мають групи з цих (зроблені до 2.5.28): наступне збирання пакета запише їхні групи, "
     "не роблячи їх наново.",
     "{1} картинок пакета ещё не имеют группы из этих (сделаны до 2.5.28): следующая сборка пакета запишет их группы, "
     "не делая их заново."},
    {Text::AiRedo, "Make again ⚠ ({1})",
     "Створити наново ⚠ ({1})",
     "Пересоздать ⚠ ({1})"},
    {Text::AiRedoHint,
     "On the Texture pack page: the pictures another model made than their group's now are made again by it, each in "
     "its place in the pack. Apply the models first.",
     "На вкладці «Пакет текстур»: картинки, зроблені іншою моделлю, ніж тепер у їхньої групи, робляться нею наново, "
     "кожна на своє місце в пакеті. Спершу застосуй моделі.",
     "На вкладке «Пакет текстур»: картинки, сделанные другой моделью, чем теперь у их группы, делаются ею заново, "
     "каждая на своё место в пакете. Сначала примени модели."},
    {Text::AiPreviewHeading, "Preview",
     "Попередній перегляд",
     "Предпросмотр"},
    {Text::AiPreviewGroup, "Group",
     "Група",
     "Группа"},
    {Text::AiPreviewShow, "Show an example",
     "Показати приклад",
     "Показать пример"},
    {Text::AiPreviewHint,
     "A picture of the group from the game's files - its middle, 128 a side at most - as the game has it and through "
     "each model, made on this graphics card in a few seconds.",
     "Картинка групи з файлів гри - її середина, до 128 точок - як у грі й через кожну модель, зроблена на цій "
     "відеокарті за кілька секунд.",
     "Картинка группы из файлов игры - её середина, до 128 точек - как в игре и через каждую модель, сделанная на "
     "этой видеокарте за несколько секунд."},
    {Text::AiPreviewWorking, "The example is being made on the graphics card...",
     "Приклад робиться на відеокарті...",
     "Пример делается на видеокарте..."},
    {Text::AiPreviewFailed, "The example was not made: {1}",
     "Приклад не зроблено: {1}",
     "Пример не сделан: {1}"},
    {Text::AiPreviewFrom, "From {1} ({2}x{3})",
     "З {1} ({2}x{3})",
     "Из {1} ({2}x{3})"},
    {Text::AiPreviewGame, "the game",
     "у грі",
     "в игре"},
    {Text::PackRedo, "Make again what another model made",
     "Створити наново те, що зробила інша модель",
     "Пересоздать то, что сделала другая модель"},
    {Text::PackRedoHint,
     "Of the groups chosen: the pictures another model made than their group's on the AI page now are made again, "
     "each in its place in the pack.",
     "З вибраних груп: картинки, зроблені іншою моделлю, ніж тепер у їхньої групи на вкладці «Нейромережа», робляться "
     "наново, кожна на своє місце в пакеті.",
     "Из выбранных групп: картинки, сделанные другой моделью, чем теперь у их группы на вкладке «Нейросеть», делаются "
     "заново, каждая на своё место в пакете."},
    {Text::PackNetsGroups, "Models (the AI page): {1}.",
     "Моделі (вкладка «Нейромережа»): {1}.",
     "Модели (вкладка «Нейросеть»): {1}."},
    {Text::AiPreviewZoom, "Zoom",
     "Масштаб",
     "Масштаб"},
    {Text::AiPreviewWhole, "Whole",
     "Цілком",
     "Целиком"},
    {Text::AiPreviewZoomHint,
     "The pictures' middle that many times larger, pixel by pixel, two to a row - the same part of each, to compare "
     "the lines and details.",
     "Середина картинок у стільки разів більша, точка в точку, по дві в ряд - та сама частина кожної, щоб порівняти "
     "лінії й деталі.",
     "Середина картинок во столько раз крупнее, точка в точку, по две в ряд - одна и та же часть каждой, чтобы "
     "сравнить линии и детали."},
    {Text::AiPreviewOpen, "Open the viewer",
     "Відкрити перегляд",
     "Открыть просмотр"},
    {Text::AiPreviewWindowHint, "the wheel zooms, a drag moves, a double click shows it whole",
     "коліщатко - збільшення, перетягування - зсув, подвійний клік - цілком",
     "колесо - увеличение, перетаскивание - сдвиг, двойной щелчок - целиком"},
    {Text::AiSmall, "Small pictures by a model of their own",
     "Малі картинки - своєю моделлю",
     "Маленькие картинки - своей моделью"},
    {Text::AiSmallHint,
     "A picture smaller than this on both sides (the game's pixels: an icon is 24, an item on the ground about 23, a "
     "character's frame 30-80) is made by the model below, whatever its group has - anime 6B redraws the smallest "
     "ones its own way.",
     "Картинка, менша за це з обох боків (точки гри: іконка - 24, предмет на землі - близько 23, кадр персонажа - "
     "30-80), робиться моделлю нижче, хоч би яку мала її група - anime 6B перемальовує найменші по-своєму.",
     "Картинка меньше этого по обеим сторонам (точки игры: иконка - 24, предмет на земле - около 23, кадр персонажа - "
     "30-80) делается моделью ниже, какая бы ни была у её группы - anime 6B перерисовывает самые маленькие по-своему."},
    {Text::AiSmallSide, "Smaller than, points",
     "Менше ніж, точок",
     "Меньше чем, точек"},
    {Text::AiSmallModel, "Their model",
     "Їхня модель",
     "Их модель"},
    {Text::FxTreeVolume, "Rounded tree crowns",
     "Об'ємні крони дерев",
     "Объёмные кроны деревьев"},
    {Text::FxTreeVolumeHint,
     "A tree's crown is lit as a round shape: the side towards the sun lighter, the far side and the middle darker. "
     "Flat leaf pictures look like volume; the light turns with the sun.",
     "Крона дерева освітлена як куля: бік до сонця світліший, дальній бік і середина темніші. Пласкі картинки листя "
     "виглядають об'ємними; світло повертається разом із сонцем.",
     "Крона дерева освещена как шар: бок к солнцу светлее, дальний бок и середина темнее. Плоские картинки листвы "
     "выглядят объёмными; свет поворачивается вместе с солнцем."},
    {Text::FxLeafLight, "Sunlight through the leaves",
     "Світло крізь листя",
     "Свет сквозь листву"},
    {Text::FxLeafLightHint,
     "The leaves on the crown's edge towards the sun glow warm green, lit through - the most in the morning and the "
     "evening.",
     "Листя на краю крони з боку сонця світиться теплим зеленим, просвічене наскрізь, найбільше вранці й увечері.",
     "Листья на краю кроны со стороны солнца светятся тёплым зелёным, просвеченные насквозь, сильнее всего утром и "
     "вечером."},
    {Text::FxTreeTints, "Trees of different shades", "Дерева різних відтінків", "Деревья разных оттенков"},
    {Text::FxTreeTintsHint,
     "Each tree gets a slightly different shade and brightness, so a wood is not one crown copied.",
     "Кожне дерево трохи іншого відтінку й яскравості, тож ліс не складається з однієї скопійованої крони.",
     "Каждое дерево чуть другого оттенка и яркости, так что лес не состоит из одной скопированной кроны."},
    {Text::FxFlowers, "Flowers in the grass",
     "Квіти в траві",
     "Цветы в траве"},
    {Text::FxFlowersHint,
     "Here and there among the blades a flower sways with them, its colours the region's: Prontera's meadows, Payon's "
     "reds, Amatsu's pinks. Needs Grass.",
     "Подекуди серед травинок гойдається квітка, кольори свого краю: луки Пронтери, червоні Пайону, рожеві Амацу. "
     "Потрібна «Трава».",
     "Кое-где среди травинок качается цветок, цвета своего края: луга Пронтеры, красные Пайона, розовые Амацу. Нужна "
     "«Трава»."},
    {Text::FxDew, "Morning dew",
     "Ранкова роса",
     "Утренняя роса"},
    {Text::FxDewHint,
     "From dawn until about noon (the computer's clock) drops on the tips of the grass flash in the sun now and then. "
     "Needs Grass.",
     "Від світанку до полудня (за годинником комп'ютера) краплі на кінчиках трави час від часу спалахують на сонці. "
     "Потрібна «Трава».",
     "От рассвета до полудня (по часам компьютера) капли на кончиках травы время от времени вспыхивают на солнце. "
     "Нужна «Трава»."},
    {Text::FxMorningFog, "Morning fog",
     "Ранковий туман",
     "Утренний туман"},
    {Text::FxMorningFogHint,
     "A low fog lies over the fields and the towns at dawn: thick from 6 to 8, gone by half past ten (the computer's "
     "clock).",
     "На світанку над полями й містами стелиться низький туман: густий з 6 до 8, зникає до пів на одинадцяту (за "
     "годинником комп'ютера).",
     "На рассвете над полями и городами стелется низкий туман: густой с 6 до 8, тает к половине одиннадцатого (по "
     "часам компьютера)."},
    {Text::FxSandDrift, "Drifting sand",
     "Позмінка піску",
     "Позёмка песка"},
    {Text::FxSandDriftHint,
     "In the desert about Morroc and Veins the wind drives thin streams of sand over the ground (Direct3D 12).",
     "У пустелі довкола Морока й Вейнсу вітер жене по землі тонкі струмені піску (Direct3D 12).",
     "В пустыне вокруг Морока и Вейнса ветер гонит по земле тонкие струйки песка (Direct3D 12)."},
    {Text::FxWetWalls, "Water running down the walls", "Вода стікає стінами", "Вода стекает по стенам"},
    {Text::FxWetWallsHint,
     "In damp caves and dungeons thin trickles of water run down the walls where the light falls on them "
     "(Direct3D 12).",
     "У вологих печерах і підземеллях там, де на стіни падає світло, ними стікають тонкі струмки води (Direct3D 12).",
     "Во влажных пещерах и подземельях там, где на стены падает свет, по ним стекают тонкие струйки воды (Direct3D "
     "12)."},
    {Text::FxSwamp, "Swamps",
     "Болота",
     "Болота"},
    {Text::FxSwampHint,
     "In the swamps of Umbala, Brasilis and Dewata the water is murky, bubbles swell and burst on it, and a low "
     "greenish mist hangs over the ground. Needs Modern water.",
     "У болотах Умбали, Бразиліса й Девати вода каламутна, на ній здуваються й лопаються бульбашки, а над землею "
     "висить низький зеленуватий серпанок. Потрібна «Сучасна вода».",
     "В болотах Умбалы, Бразилиса и Деваты вода мутная, на ней вздуваются и лопаются пузыри, а над землёй висит "
     "низкая зеленоватая дымка. Нужна «Современная вода»."},
    {Text::FxButterflies, "Butterflies",
     "Метелики",
     "Бабочки"},
    {Text::FxButterfliesHint,
     "By day a few butterflies flutter over the fields and the green towns, their shadows on the ground "
     "(Direct3D 12).",
     "Удень над полями й зеленими містами пурхає кілька метеликів, їхні тіні біжать землею (Direct3D 12).",
     "Днём над полями и зелёными городами порхает несколько бабочек, их тени бегут по земле (Direct3D 12)."},
    {Text::FxBirds, "Birds",
     "Птахи",
     "Птицы"},
    {Text::FxBirdsHint,
     "By day a flock pecks on the ground and flies up and away when someone comes near; another comes later elsewhere "
     "(Direct3D 12).",
     "Удень зграйка клює щось на землі й злітає, коли хтось підходить близько; згодом інша сідає деінде (Direct3D 12).",
     "Днём стайка клюёт что-то на земле и вспархивает, когда кто-то подходит близко; позже другая садится в другом "
     "месте (Direct3D 12)."},
    {Text::FxLeafFall, "Leaves from the trees",
     "Листя з дерев",
     "Листья с деревьев"},
    {Text::FxLeafFallHint,
     "Now and then a leaf comes off a tree in sight, turns as it falls and lies on the ground a while - golden in "
     "autumn, sakura petals in Amatsu (Direct3D 12).",
     "Час від часу з дерева поруч зривається листок, кружляє, падає й трохи лежить на землі — восени золотий, в Амацу "
     "пелюстки сакури (Direct3D 12).",
     "Время от времени с дерева рядом срывается лист, кружится, падает и немного лежит на земле — осенью золотой, в "
     "Амацу лепестки сакуры (Direct3D 12)."},
    {Text::FxEmbers, "Embers from the fires",
     "Іскри з вогнищ",
     "Угольки от огня"},
    {Text::FxEmbersHint, "Sparks rise from the torches and the fires, sway, cool and go out in the air (Direct3D 12).",
     "Від смолоскипів і вогнищ здіймаються іскри, кружляють, холонуть і гаснуть у повітрі (Direct3D 12).",
     "От факелов и костров поднимаются искры, кружатся, остывают и гаснут в воздухе (Direct3D 12)."},
    {Text::FxSplashes, "Splashes from steps", "Бризки від кроків", "Брызги от шагов"},
    {Text::FxSplashesHint, "A step into shallow water throws up drops that fall back (Direct3D 12).",
     "Крок у мілку воду підкидає краплі, що падають назад (Direct3D 12).",
     "Шаг в мелкую воду подбрасывает капли, которые падают обратно (Direct3D 12)."},
    {Text::FxCaveDrips, "Drips from the ceiling", "Краплі зі стелі", "Капли с потолка"},
    {Text::FxCaveDripsHint,
     "In damp caves and dungeons a drop falls from above now and then and rings spread where it lands (Direct3D 12).",
     "У вологих печерах і підземеллях час від часу згори падає крапля, і там, де вона впала, розходяться кола "
     "(Direct3D 12).",
     "Во влажных пещерах и подземельях время от времени сверху падает капля, и там, где она упала, расходятся круги "
     "(Direct3D 12)."},
    {Text::FxUnderwater, "Bubbles and light under water", "Бульбашки й світло під водою", "Пузыри и свет под водой"},
    {Text::FxUnderwaterHint,
     "On the undersea maps (Byalan's dungeon) strings of bubbles rise and light from above falls in swaying bands "
     "(Direct3D 12).",
     "На підводних мапах (підземелля Бʼялану) здіймаються ланцюжки бульбашок, а згори хвилястими смугами падає світло "
     "(Direct3D 12).",
     "На подводных картах (подземелье Бьялана) поднимаются цепочки пузырьков, а сверху колышущимися полосами падает "
     "свет (Direct3D 12)."},
    {Text::FxMaterials,
     "Materials: stone, brick, bark", "Матеріали: камінь, цегла, кора", "Материалы: камень, кирпич, кора"},
    {Text::FxMaterialsHint,
     "Each texture of the ground, the houses and the rocks gets its relief, height and gloss, made by the card from "
     "its picture: the sun lights stones, bricks and bark from their side, the hollows are darker, smooth stone "
     "glints, the repeats of the ground's tiles hide under large patches of shade, steep cliffs get a stone grain "
     "of their own over the game's stretched stripes. With the remaster also depth (Direct3D 12).",
     "Кожна текстура землі, будинків і скель отримує рельєф, висоту й блиск, які відеокарта робить із її картинки: "
     "сонце освітлює камінь, цеглу й кору збоку, западини темніші, гладкий камінь виблискує, повтори плиток землі "
     "ховаються під великими плямами відтінку, круті скелі отримують власну кам'яну фактуру поверх "
     "розтягнутих смуг гри. З ремастером ще й глибина (Direct3D 12).",
     "Каждая текстура земли, домов и скал получает рельеф, высоту и блеск, которые видеокарта делает из её картинки: "
     "солнце освещает камень, кирпич и кору сбоку, впадины темнее, гладкий камень блестит, повторы плиток земли "
     "прячутся под крупными пятнами оттенка, крутые скалы получают свою каменную фактуру поверх "
     "растянутых полос игры. С ремастером ещё и глубина (Direct3D 12)."},
    {Text::FxMaterialsStrength, "Strength of the materials",
     "Сила матеріалів",
     "Сила материалов"},
    {Text::FxMaterialsStrengthHint, "How much the relief and the gloss show.",
     "Наскільки помітні рельєф і блиск.",
     "Насколько заметны рельеф и блеск."},
    {Text::FxTier, "Mode",
     "Режим",
     "Режим"},
    {Text::FxTierHint, "Automatic: Remaster on a card with 6 GB of its own memory or more, else Classic+.",
     "Автоматично: «Ремастер» на відеокарті з 6 ГБ власної пам'яті й більше, інакше «Класика+».",
     "Автоматически: «Ремастер» на видеокарте с 6 ГБ собственной памяти и больше, иначе «Классика+»."},
    {Text::FxTierAuto, "Automatic",
     "Автоматично",
     "Автоматически"},
    {Text::FxTierClassic, "Classic+ (any card)",
     "Класика+ (будь-яка відеокарта)",
     "Классика+ (любая видеокарта)"},
    {Text::FxTierRemaster, "Remaster (a strong card)",
     "Ремастер (сильна відеокарта)",
     "Ремастер (сильная видеокарта)"},
    {Text::FxGroundSeams,
     "Grass and road seams without steps", "Стики трави й доріг без сходинок", "Стыки травы и дорог без ступенек"},
    {Text::FxGroundSeamsHint,
     "Where grass meets a road or sand, the line between them no longer runs in steps along the ground's cells: it "
     "wanders, one texture reaching into the other. On fields and in towns; the tiled floors of dungeons keep their "
     "straight edges (Direct3D 12). With Own ground on, its textures blend the same way.",
     "Там, де трава зустрічається з дорогою чи піском, межа між ними більше не йде сходинками вздовж клітинок землі: "
     "вона петляє, одна текстура заходить в іншу. На полях і в містах; плиткова підлога підземель зберігає рівні "
     "краї (Direct3D 12). Зі «Своя земля» так само змішуються текстури своєї землі.",
     "Там, где трава встречается с дорогой или песком, граница между ними больше не идёт ступеньками вдоль клеток "
     "земли: она петляет, одна текстура заходит в другую. На полях и в городах; плиточный пол подземелий сохраняет "
     "ровные края (Direct3D 12). Со «Своя земля» так же смешиваются текстуры своей земли."},
    {Text::ConsoleTitle, "uaRO Graphics - effects",
     "uaRO Graphics — ефекти",
     "uaRO Graphics — эффекты"},
    {Text::ConsoleKeys,
     "Up/Down choose · Space on/off, a group's effects · Left/Right value, the tab · letters search · hold Tab to "
     "compare · Insert ★ to «Quick» · Ctrl+Shift+F11 frame counter · ↻ at the game's next start · ~ or Esc close",
     "↑↓ вибір · Пробіл увімк/вимк, ефекти групи · ←→ значення, вкладка · літери — пошук · затисніть Tab — "
     "порівняти · Insert — ★ у «Швидко» · Ctrl+Shift+F11 — лічильник кадрів · ↻ — після перезапуску гри · ~ або Esc "
     "— закрити",
     "↑↓ выбор · Пробел вкл/выкл, эффекты группы · ←→ значение, вкладка · буквы — поиск · зажмите Tab — сравнить · "
     "Insert — ★ в «Быстро» · Ctrl+Shift+F11 — счётчик кадров · ↻ — после перезапуска игры · ~ или Esc — закрыть"},
    {Text::FxSmoothHills, "Hills without steps of light", "Пагорби без сходинок світла", "Холмы без ступенек света"},
    {Text::FxSmoothHillsHint,
     "The game lights its ground cell by cell, and a hill shows as steps of light. The ground is lit by the sun over "
     "smooth slopes instead: the sunny side of a hill brighter, the far side in a soft shade (Direct3D 12).",
     "Гра освітлює землю клітинка за клітинкою, і пагорб видно сходинками світла. Натомість сонце освітлює плавні "
     "схили: сонячний бік пагорба світліший, дальній у м'якій тіні (Direct3D 12).",
     "Игра освещает землю клетка за клеткой, и холм видно ступеньками света. Вместо этого солнце освещает плавные "
     "склоны: солнечная сторона холма светлее, дальняя в мягкой тени (Direct3D 12)."},
    {Text::FxSmoothHillsStrength, "Strength of the hills' light",
     "Сила світла пагорбів",
     "Сила света холмов"},
    {Text::FxSmoothHillsStrengthHint, "How much the slopes' light and shade show.",
     "Наскільки помітні світло й тінь на схилах.",
     "Насколько заметны свет и тень на склонах."},
    {Text::FxWorldReflections, "World reflections in water",
     "Відображення світу у воді",
     "Отражения мира в воде"},
    {Text::FxWorldReflectionsHint,
     "The water reflects the houses, rocks and trees along its shores - also what is above the screen's top edge, "
     "which the picture alone cannot show. Needs Modern water; Remaster mode only (Direct3D 12).",
     "Вода відбиває будинки, скелі й дерева на березі — також те, що вище за верхній край екрана і чого сама картинка "
     "показати не може. Потрібна «Сучасна вода»; лише в режимі «Ремастер» (Direct3D 12).",
     "Вода отражает дома, скалы и деревья на берегу — также то, что выше верхнего края экрана и чего сама картинка "
     "показать не может. Нужна «Современная вода»; только в режиме «Ремастер» (Direct3D 12)."},
    {Text::FxSpriteLight, "Volume from the sun", "Об'єм від сонця", "Объём от солнца"},
    {Text::FxSpriteLightHint,
     "Characters and monsters are lit by the sun as figures: the side of the body towards it a little lighter and "
     "warmer, the far side in a soft shade - no longer flat stickers.",
     "Персонажів і монстрів сонце освітлює як об'ємні фігури: бік тіла до сонця трохи світліший і тепліший, дальній "
     "у м'якій тіні — вони вже не пласкі наліпки.",
     "Персонажей и монстров солнце освещает как объёмные фигуры: сторона тела к солнцу чуть светлее и теплее, дальняя "
     "в мягкой тени — они больше не плоские наклейки."},
    {Text::FxSpriteLightStrength, "Strength of the characters' light",
     "Сила світла персонажів",
     "Сила света персонажей"},
    {Text::FxSpriteLightStrengthHint, "How much the sun's side and the shade show on the characters.",
     "Наскільки помітні сонячний бік і тінь на персонажах.",
     "Насколько заметны солнечная сторона и тень на персонажах."},
    {Text::FxSeasons, "Seasons on the map",
     "Пори року на карті",
     "Времена года на карте"},
    {Text::FxSeasonsHint,
     "By the computer's calendar: snow in winter and the grass's colour by the season, each its own box below. Snowy "
     "maps keep snow on their roofs all year; the desert and the tropics have no seasons (Direct3D 12).",
     "За календарем комп'ютера: сніг узимку й колір трави за сезоном, кожне своєю галочкою нижче. Сніжні карти "
     "тримають сніг на дахах увесь рік; пустеля й тропіки без пір року (Direct3D 12).",
     "По календарю компьютера: снег зимой и цвет травы по сезону, каждое своей галочкой ниже. Снежные карты держат "
     "снег на крышах весь год; пустыня и тропики без времён года (Direct3D 12)."},
    {Text::FxRainWet, "Wet walls and roofs", "Мокрі стіни й дахи", "Мокрые стены и крыши"},
    {Text::FxRainWetHint,
     "Where it rains, the walls, roofs and models get wet too, not only the ground: darker, with a sheen where you "
     "look along them and water running down the walls. Needs Precipitation on (Direct3D 12).",
     "Де йде дощ, мокнуть не лише земля, а й стіни, дахи й моделі: темніші, з полиском, коли дивишся вздовж них, і "
     "водою, що стікає стінами. Потрібні «Опади» (Direct3D 12).",
     "Где идёт дождь, мокнет не только земля, но и стены, крыши и модели: темнее, с блеском, когда смотришь вдоль "
     "них, и водой, стекающей по стенам. Нужны «Осадки» (Direct3D 12)."},
    {Text::FxNightWindows, "Windows lit at night", "Вікна світяться вночі", "Окна светятся ночью"},
    {Text::FxNightWindowsHint,
     "As the night comes the houses' windows light up warm, not all of them. The panes are found in the walls' "
     "pictures by themselves; needs the Materials and the night (Direct3D 12).",
     "З настанням ночі вікна будинків загоряються теплим світлом, не всі. Шибки знаходяться в картинках стін самі; "
     "потрібні «Матеріали: камінь, цегла, кора» й ніч (Direct3D 12).",
     "С наступлением ночи окна домов загораются тёплым светом, не все. Стёкла находятся в картинках стен сами; нужны "
     "«Материалы: камень, кирпич, кора» и ночь (Direct3D 12)."},
    {Text::FxSharedWind, "One wind for everything",
     "Один вітер для всього",
     "Один ветер для всего"},
    {Text::FxSharedWindHint,
     "The grass, drifting sand, rain and snow all go one way with one wind, which slowly turns and blows stronger and "
     "weaker - instead of each going its own way (Direct3D 12).",
     "Трава, піщана поземка, дощ і сніг рухаються в один бік з одним вітром, що поволі змінює напрям і то посилюється, "
     "то слабне, — а не кожне куди саме хоче (Direct3D 12).",
     "Трава, песчаная позёмка, дождь и снег движутся в одну сторону с одним ветром, который медленно меняет "
     "направление и то усиливается, то стихает, — а не каждое куда само хочет (Direct3D 12)."},
    {Text::FxLitDust, "Dust in beams of light", "Пил у променях світла", "Пыль в лучах света"},
    {Text::FxLitDustHint,
     "Indoors and in dungeons the dust in the air shows where light falls - in the beams from the windows, by the "
     "torches - tinted by that light, and hardly in the dark corners. Needs Pollen and motes in the air, Dust indoors "
     "(Direct3D 12).",
     "У приміщеннях і підземеллях пил у повітрі видно там, де падає світло, — у променях із вікон, біля факелів, — у "
     "кольорі цього світла, а в темних кутах ледь-ледь. Потрібні «Пилок і порошинки в повітрі» й «Пил у приміщеннях» "
     "(Direct3D 12).",
     "В помещениях и подземельях пыль в воздухе видна там, где падает свет, — в лучах из окон, у факелов, — в цвете "
     "этого света, а в тёмных углах едва-едва. Нужны «Пыльца и пылинки в воздухе» и «Пыль в помещениях» "
     "(Direct3D 12)."},
    {Text::FxPreset, "Effects quality",
     "Якість ефектів",
     "Качество эффектов"},
    {Text::FxPresetHint,
     "Which effects are on, as the graphics settings of any game: Low for built-in graphics and cards under 4 GB, "
     "Medium for 4 to 8 GB, High for 8 GB and more. Auto picks by your card; Custom keeps every effect as you set "
     "it inside the groups.",
     "Які ефекти ввімкнені — як налаштування графіки в будь-якій грі: «Низькі» — для вбудованої графіки й карт до "
     "4 ГБ, «Середні» — від 4 до 8 ГБ, «Високі» — від 8 ГБ. «Авто» обирає за вашою картою; «Свої» залишають кожен "
     "ефект таким, як ви його поставили всередині груп.",
     "Какие эффекты включены — как настройки графики в любой игре: «Низкие» — для встроенной графики и карт до "
     "4 ГБ, «Средние» — от 4 до 8 ГБ, «Высокие» — от 8 ГБ. «Авто» выбирает по вашей карте; «Свои» оставляют каждый "
     "эффект таким, как вы его поставили внутри групп."},
    {Text::FxPresetAuto, "Auto: {1}",
     "Авто: {1}",
     "Авто: {1}"},
    {Text::FxPresetOff, "Off: the game's own picture",
     "Вимкнено: власна картинка гри",
     "Выключено: собственная картинка игры"},
    {Text::FxPresetWeak, "Low",
     "Низькі",
     "Низкие"},
    {Text::FxPresetMedium, "Medium",
     "Середні",
     "Средние"},
    {Text::FxPresetStrong, "High",
     "Високі",
     "Высокие"},
    {Text::FxPresetCustom, "Custom",
     "Свої",
     "Свои"},
    {Text::FxSetIntro,
     "Pick the settings for your graphics card, then turn groups on or off and make them stronger or weaker; a "
     "group's button opens it to its effects, each its own.",
     "Оберіть налаштування для своєї відеокарти, потім вмикайте чи вимикайте групи й робіть їх сильнішими чи "
     "слабшими; кнопка групи відкриває її ефекти, кожен окремо.",
     "Выберите настройки для своей видеокарты, потом включайте или выключайте группы и делайте их сильнее или "
     "слабее; кнопка группы открывает её эффекты, каждый отдельно."},
    {Text::FxGroupTime, "Day, night and seasons", "День, ніч і пори року", "День, ночь и времена года"},  // 2.6.22
    {Text::FxGroupTimeHint,
     "What the Clock and the Season at the top change: the sun moving, lit windows at night, the morning fog, "
     "the server's night, the seasons on the map.",
     "Що змінюють «Годинник» і «Пора року» вгорі: рух сонця, світло у вікнах уночі, ранковий туман, ніч "
     "сервера, пори року на карті.",
     "Что меняют «Часы» и «Время года» вверху: движение солнца, свет в окнах ночью, утренний туман, ночь "
     "сервера, времена года на карте."},
    {Text::FxGroupWeather, "Weather: rain, snow, sand", "Погода: дощ, сніг, пісок", "Погода: дождь, снег, песок"},
    {Text::FxGroupWeatherHint,
     "What the Weather at the top brings: snow, rain, leaves or sand in the air, thunderstorms, wet ground, "
     "walls and roofs, drifting sand, glittering snow.",
     "Що приносить «Погода» вгорі: сніг, дощ, листя чи пісок у повітрі, грози, мокрі земля, стіни й дахи, "
     "позмінка піску, іскристий сніг.",
     "Что приносит «Погода» вверху: снег, дождь, листья или песок в воздухе, грозы, мокрые земля, стены и "
     "крыши, позёмка песка, искрящийся снег."},
    {Text::FxGroupCharacters, "Characters and monsters", "Персонажі та монстри", "Персонажи и монстры"},
    {Text::FxGroupCharactersHint,
     "Their shadows, volume and light, footprints, breath in the frost, the flash of a hit, the level 99 aura.",
     "Їхні тіні, об'єм і світло, сліди, пара на морозі, спалах від удару, аура 99 рівня.",
     "Их тени, объём и свет, следы, пар на морозе, вспышка при ударе, аура 99 уровня."},
    {Text::FxGroupSkillEffects, "Skills", "Уміння", "Умения"},
    {Text::FxGroupSkillEffectsHint,
     "The skills softer so the big ones do not blind, their sparks, the light of their fire around.",
     "Уміння м'якші, щоб великі не сліпили, їхні іскри, світло їхнього вогню довкола.",
     "Умения мягче, чтобы большие не слепили, их искры, свет их огня вокруг."},
    {Text::FxGroupFire, "Fire and smoke", "Вогонь і дим", "Огонь и дым"},
    {Text::FxGroupFireHint, "Torches, embers over the fires, chimney smoke, lava, the heat haze over fire.",
     "Смолоскипи, іскри над вогнищами, дим із труб, лава, марево над вогнем.",
     "Факелы, угольки над огнём, дым из труб, лава, марево над огнём."},
    {Text::FxGroupLights, "Light", "Світло", "Свет"},
    {Text::FxGroupLightsHint,
     "Lamps' light, the maps' hidden lights and fireflies, portals, the halo around fire and skills, the "
     "glowing air, beams of light in the forests.",
     "Світло ламп, приховані вогники й світлячки карт, портали, ореол довкола вогню й умінь, сяюче повітря, "
     "промені світла в лісах.",
     "Свет ламп, скрытые огоньки и светлячки карт, порталы, ореол вокруг огня и умений, светящийся воздух, "
     "лучи света в лесах."},
    {Text::FxGroupGround, "Ground and walls", "Земля і стіни", "Земля и стены"},
    {Text::FxGroupGroundHint,
     "Relief and materials of the ground, the houses and the rocks, seams of grass and roads, hills, shadows "
     "in corners, reflections on floors, cards sparkling on the ground.",
     "Рельєф і матеріали землі, будинків і скель, стики трави й доріг, пагорби, тіні в кутах, відбиття на "
     "підлозі, картки, що виблискують на землі.",
     "Рельеф и материалы земли, домов и скал, стыки травы и дорог, холмы, тени в углах, отражения на полу, "
     "карты, поблёскивающие на земле."},
    {Text::FxGroupPlants, "Grass and trees", "Трава й дерева", "Трава и деревья"},
    {Text::FxGroupPlantsHint,
     "Grass, flowers and dew, leaves in the wind, the trees' crowns, light through the leaves, their shades of "
     "colour, falling leaves.",
     "Трава, квіти й роса, листя на вітрі, крони дерев, світло крізь листя, відтінки дерев, листопад.",
     "Трава, цветы и роса, листва на ветру, кроны деревьев, свет сквозь листву, оттенки деревьев, падающие листья."},
    {Text::FxGroupWaters, "Water", "Вода", "Вода"},
    {Text::FxGroupWatersHint,
     "The modern water and all on it: ripples and splashes from steps, the world under water, swamps, reflections.",
     "Сучасна вода і все на ній: кола й бризки від кроків, світ під водою, болота, відображення.",
     "Современная вода и всё на ней: круги и брызги от шагов, мир под водой, болота, отражения."},
    {Text::FxGroupStrength, "Strength",
     "Сила",
     "Сила"},
    {Text::FxGroupStrengthHint, "How strong the group's effects are: 100 % as made, less is gentler, more is stronger.",
     "Наскільки сильні ефекти групи: 100 % — як задумано, менше — м'якше, більше — сильніше.",
     "Насколько сильны эффекты группы: 100 % — как задумано, меньше — мягче, больше — сильнее."},
    {Text::FxDetails,  // 2.6.22
     "Effects: {1} of {2} on ▸", "Ефекти: увімкнено {1} з {2} ▸", "Эффекты: включено {1} из {2} ▸"},
    {Text::FxDetailsHide, "Hide the effects ▾",
     "Сховати ефекти ▾",
     "Скрыть эффекты ▾"},
    {Text::FxDetailsNote,
     "Each effect can be switched inside its group (the button under it). A switch of one makes the quality Custom, "
     "starting from what it had on; a group's box still turns all of its effects off.",
     "Кожен ефект можна перемкнути всередині його групи (кнопка під нею). Перемкнений ефект робить якість «Свої», "
     "починаючи з того, що вона мала увімкненим; галочка групи й далі вимикає всі її ефекти.",
     "Каждый эффект можно переключить внутри его группы (кнопка под ней). Переключённый эффект делает качество «Свои», "
     "начиная с того, что оно держало включённым; галочка группы по-прежнему выключает все её эффекты."},
    {Text::FxGroupGameFixes, "Game fixes", "Виправлення гри", "Исправления игры"},  // 2.6.22
    {Text::FxGroupGameFixesHint,
     "What mends the game's old picture rather than adds to it: the map's light without steps, the effects "
     "not cut at the ground, the smooth change of maps, no game fog under the open sky. In every set.",
     "Те, що лагодить стару картинку гри, а не додає до неї: світло карти без сходинок, ефекти не обрізаються "
     "біля землі, плавна зміна карти, без туману гри під відкритим небом. У кожному наборі.",
     "То, что чинит старую картинку игры, а не добавляет к ней: свет карты без ступенек, эффекты не "
     "обрезаются у земли, плавная смена карты, без тумана игры под открытым небом. В каждом наборе."},
    {Text::StatsSend, "Send anonymous statistics of the settings",
     "Надсилати анонімну статистику налаштувань",
     "Отправлять анонимную статистику настроек"},
    {Text::StatsSendHint,
     "Helps make the graphics better for everyone: which effects and textures are on and how long, what is turned "
     "off after a try, the graphics card's name and memory, the frames a second and the card's milliseconds. No name, "
     "no login, no files - a random number for this computer. Sent by this app to uaro.kiev.ua once a day. Apart "
     "from it, whatever is chosen here, the update check tells uaro.kiev.ua every 12 hours only that this install is "
     "there: another random number, the version and the channel (to count the installs) - and once how its first "
     "game went: whether it got to a map, its frames, whether it closed by an error.",
     "Допомагає зробити графіку кращою для всіх: які ефекти й текстури ввімкнені й скільки часу, що вимикають після "
     "спроби, назва й пам'ять відеокарти, кадри за секунду й мілісекунди карти. Ні імені, ні логіна, ні файлів — "
     "випадкове число для цього комп'ютера. Надсилає ця утиліта на uaro.kiev.ua раз на день. Окремо від неї, хоч "
     "би що вибрано тут, перевірка оновлень раз на 12 годин каже uaro.kiev.ua лише, що ця установка є: інше "
     "випадкове число, версію й канал (щоб рахувати установки), — і один раз, як пройшла перша гра: чи дійшла до "
     "карти, скільки кадрів, чи закрилася з помилкою.",
     "Помогает сделать графику лучше для всех: какие эффекты и текстуры включены и сколько времени, что выключают "
     "после пробы, название и память видеокарты, кадры в секунду и миллисекунды карты. Ни имени, ни логина, ни "
     "файлов — случайное число для этого компьютера. Отправляет эта утилита на uaro.kiev.ua раз в день. Отдельно "
     "от неё, что бы ни было выбрано здесь, проверка обновлений раз в 12 часов сообщает uaro.kiev.ua только, что "
     "эта установка есть: другое случайное число, версию и канал (чтобы считать установки), — и один раз, как прошла "
     "первая игра: дошла ли до карты, сколько кадров, закрылась ли с ошибкой."},
    {Text::StatsAsk,
     "Help make uaRO Graphics better?\n\nWith your leave the settings app sends, once a day, anonymous statistics "
     "of how the graphics are played: which effects and textures are on and for how long, what is turned off after "
     "a try, the graphics card and its frames a second. No name, no login, no files.\n\nThe best settings for each "
     "graphics card are then made from it. You can change your mind on the Diagnostics page.",
     "Допомогти зробити uaRO Graphics кращим?\n\nЗ вашого дозволу утиліта раз на день надсилає анонімну "
     "статистику того, як грають з графікою: які ефекти й текстури ввімкнені й скільки часу, що вимикають після "
     "спроби, відеокарта та її кадри за секунду. Ні імені, ні логіна, ні файлів.\n\nЗ неї складаються найкращі "
     "налаштування для кожної відеокарти. Передумати можна на сторінці «Діагностика».",
     "Помочь сделать uaRO Graphics лучше?\n\nС вашего разрешения утилита раз в день отправляет анонимную "
     "статистику того, как играют с графикой: какие эффекты и текстуры включены и сколько времени, что выключают "
     "после пробы, видеокарта и её кадры в секунду. Ни имени, ни логина, ни файлов.\n\nИз неё составляются лучшие "
     "настройки для каждой видеокарты. Передумать можно на странице «Диагностика»."},
    // 2.2.2
    {Text::StatsSendNow, "Send the statistics now", "Надіслати статистику зараз", "Отправить статистику сейчас"},
    {Text::StatsState,
     "Statistics: last sent {1}; waiting to go: {2} KB.", "Статистика: востаннє надіслано {1}; чекає: {2} КБ.",
     "Статистика: последняя отправка {1}; ждёт отправки: {2} КБ."},
    {Text::StatsNever, "never", "ще ні разу", "ещё ни разу"},
    {Text::StatsSentNow, "The statistics went to uaro.kiev.ua: {1} lines. Thank you!",
     "Статистику надіслано на uaro.kiev.ua: рядків — {1}. Дякуємо!",
     "Статистика отправлена на uaro.kiev.ua: строк — {1}. Спасибо!"},
    {Text::StatsNothing,
     "Nothing to send yet: the game writes the statistics while it is played with a map on the screen.",
     "Поки нічого надсилати: гра записує статистику, коли в неї грають з картою на екрані.",
     "Пока нечего отправлять: игра записывает статистику, когда в неё играют с картой на экране."},
    {Text::StatsFailed, "The statistics did not go: {1}. They stay and go next time.",
     "Статистику не надіслано: {1}. Вона лишається й піде наступного разу.",
     "Статистика не отправлена: {1}. Она остаётся и уйдёт в следующий раз."},
    // 2.2.5
    {Text::PackMapHeading, "One map's textures", "Текстури однієї карти", "Текстуры одной карты"},
    {Text::PackMapNote,
     "To try a model on one place: the pictures of one map's textures made again by the model chosen here, or taken "
     "out of the pack - the game then shows its own textures there. Other maps with the same textures change too. "
     "Close the game first: the change shows at its next start.",
     "Щоб спробувати модель на одному місці: картинки текстур однієї карти створити наново вибраною тут моделлю або "
     "прибрати з пакета — тоді гра показує там свої текстури. Інші карти з тими самими текстурами теж зміняться. "
     "Спершу закрийте гру: зміни видно після її наступного запуску.",
     "Чтобы попробовать модель на одном месте: картинки текстур одной карты пересоздать выбранной здесь моделью или "
     "убрать из пакета — тогда игра показывает там свои текстуры. Другие карты с теми же текстурами тоже изменятся. "
     "Сначала закройте игру: изменения видны после её следующего запуска."},
    {Text::PackMapName, "Map", "Карта", "Карта"},
    {Text::PackMapCue, "e.g. cmd_fild02", "напр. cmd_fild02", "напр. cmd_fild02"},
    {Text::PackMapNameHint,
     "The map's name as its files have it (/where in the game says it); the last map you played is filled in.",
     "Назва карти, як у її файлах (її каже /where у грі); підставлено останню карту, де ви грали.",
     "Название карты, как в её файлах (его говорит /where в игре); подставлена последняя карта, где вы играли."},
    {Text::PackMapGround, "Its ground", "Її земля", "Её земля"},
    {Text::PackMapGroundHint,
     "Sand, grass, paving, roads: the ground's textures.", "Пісок, трава, плити, дороги: текстури землі.",
     "Песок, трава, плиты, дороги: текстуры земли."},
    {Text::PackMapModels, "Its models", "Її моделі", "Её модели"},
    {Text::PackMapModelsHint,
     "Buildings, trees, rocks and the like placed on the map.", "Будівлі, дерева, скелі тощо, поставлені на карті.",
     "Здания, деревья, скалы и прочее, поставленное на карте."},
    {Text::PackMapModel, "Model", "Модель", "Модель"},
    {Text::PackMapModelHint, "The network that makes these pictures again; the pack's other pictures keep theirs.",
     "Мережа, що створює ці картинки наново; інші картинки пакета лишаються своїми.",
     "Сеть, которая пересоздаёт эти картинки; остальные картинки пакета остаются своими."},
    {Text::PackMapRemake, "Make again", "Створити наново", "Пересоздать"},
    {Text::PackMapDrop, "Take out of the pack", "Прибрати з пакета", "Убрать из пакета"},
    {Text::PackMapAskRemake,
     "Make the pictures of the textures of {1} again by the model {2}? The ones in the pack are replaced.",
     "Створити наново картинки текстур карти {1} моделлю {2}? Ті, що в пакеті, буде замінено.",
     "Пересоздать картинки текстур карты {1} моделью {2}? Те, что в пакете, будут заменены."},
    {Text::PackMapAskDrop,
     "Take the pictures of the textures of {1} out of the pack? The game will show its own textures there.",
     "Прибрати з пакета картинки текстур карти {1}? Гра покаже там свої текстури.",
     "Убрать из пакета картинки текстур карты {1}? Игра покажет там свои текстуры."},
    {Text::PackMapBadName, "Write the map's name as its files have it, e.g. cmd_fild02.",
     "Напишіть назву карти, як у її файлах, напр. cmd_fild02.",
     "Напишите название карты, как в её файлах, напр. cmd_fild02."},
    {Text::PackDropped,
     "{1} pictures taken out of the pack; {2} in it.", "Прибрано з пакета картинок: {1}; у ньому: {2}.",
     "Убрано из пакета картинок: {1}; в нём: {2}."},
    {Text::PackWriting, "The pack is being written, or its last build was cut short ({1}): a build finishes it.",
     "Пакет зараз записується або його останнє збирання обірвалося ({1}): збирання його допише.",
     "Пакет сейчас записывается или его последняя сборка оборвалась ({1}): сборка его допишет."},
    // 2.3: uaRO Studio
    {Text::StudioTitle, "uaRO Studio", "uaRO Studio", "uaRO Studio"},
    {Text::StudioNoGame, "uaRO Studio works with a game: start it from the uaro-graphics folder in your uaRO folder.",
     "uaRO Studio працює з грою: запустіть її з теки uaro-graphics у теці uaRO.",
     "uaRO Studio работает с игрой: запустите её из папки uaro-graphics в папке uaRO."},
    {Text::StudioCopyFailed, "uaRO Studio could not start from its own copy: {1}",
     "uaRO Studio не змогла запуститися зі своєї копії: {1}",
     "uaRO Studio не смогла запуститься из своей копии: {1}"},
    {Text::StudioPause, "Pause", "Пауза", "Пауза"},
    {Text::StudioResume, "Resume", "Продовжити", "Продолжить"},
    {Text::StudioPaused, "Paused: the engine, the pack and the game's files are free (update the driver now).",
     "Пауза: рушій, пакет і файли гри вільні (тепер можна оновити драйвер).",
     "Пауза: движок, пакет и файлы игры свободны (теперь можно обновить драйвер)."},
    {Text::StudioEngine, "Engine: {1}", "Рушій: {1}", "Движок: {1}"},
    {Text::StudioEngineNone, "Engine: not started yet", "Рушій: ще не запущено", "Движок: ещё не запущен"},
    {Text::StudioEngineNewer, "a newer one takes over after this job", "новіший заступить після цієї роботи",
     "новый заступит после этой работы"},
    {Text::StudioJobsIdle, "No jobs", "Робіт немає", "Работ нет"},
    {Text::StudioJobRunning, "{1}: {2}", "{1}: {2}", "{1}: {2}"},
    {Text::StudioJobsWaiting, "{1} more waiting", "ще чекають: {1}", "ещё ждут: {1}"},
    {Text::StudioStop, "Stop", "Стоп", "Стоп"},
    {Text::StudioTabTextures, "Textures", "Текстури", "Текстуры"},
    {Text::StudioTabPack, "Pack", "Пакет", "Пакет"},
    {Text::StudioTabLog, "Log", "Журнал", "Журнал"},
    {Text::StudioNavAll, "All textures", "Усі текстури", "Все текстуры"},
    {Text::StudioNavMaps, "Maps", "Карти", "Карты"},
    {Text::StudioNavFolders, "Folders", "Теки", "Папки"},
    {Text::StudioNavStates, "States", "Стани", "Состояния"},
    {Text::StudioNavMissing, "No picture yet", "Ще без картинки", "Ещё без картинки"},
    {Text::StudioNavInPack, "In the pack", "У пакеті", "В пакете"},
    {Text::StudioNavOwn, "The game's own always", "Завжди своя гри", "Всегда своя игры"},
    {Text::StudioNavFlagged, "Flagged", "З позначками", "С отметками"},
    {Text::StudioNavOverrides, "Material changed", "Матеріал змінено", "Материал изменён"},
    {Text::StudioNavOrphans, "Pictures without a texture", "Картинки без текстури", "Картинки без текстуры"},
    {Text::StudioNavModels, "By model", "За моделлю", "По модели"},
    {Text::StudioNavReports, "What the game drew", "Що гра намалювала", "Что игра нарисовала"},
    {Text::StudioFilterCue,
     "Filter: a file's name or a key", "Фільтр: ім'я файлу або ключ", "Фильтр: имя файла или ключ"},
    {Text::StudioColPicture, "Game / pack", "Гра / пакет", "Игра / пакет"},
    {Text::StudioColFile, "File", "Файл", "Файл"},
    {Text::StudioColState, "State", "Стан", "Состояние"},
    {Text::StudioColModel, "Model", "Модель", "Модель"},
    {Text::StudioColFlags, "Flags", "Позначки", "Отметки"},
    {Text::StudioColDetail, "Detail kept", "Збережено деталей", "Сохранено деталей"},
    {Text::StudioColSize, "Size", "Розмір", "Размер"},
    {Text::StudioColDate, "Made", "Створено", "Создано"},
    {Text::StudioStateMissing, "no picture", "без картинки", "без картинки"},
    {Text::StudioStateInPack, "in the pack", "у пакеті", "в пакете"},
    {Text::StudioStateOwn, "the game's own", "своя гри", "своя игры"},
    {Text::StudioStateOrphan, "no texture", "без текстури", "без текстуры"},
    {Text::StudioFlagSmoothed, "smoothed", "замилена", "замылена"},
    {Text::StudioFlagWashed, "tone shifted", "тон зсунуто", "тон сдвинут"},
    {Text::StudioFlagBanded, "bands", "смуги", "полосы"},
    {Text::StudioFlagRinging, "halos", "ореоли", "ореолы"},
    {Text::StudioFlagFringe, "pink fringe", "рожева облямівка", "розовая кайма"},
    {Text::StudioFlagAlpha, "alpha spoiled", "прозорість зіпсовано", "прозрачность испорчена"},
    {Text::StudioFlagSeams, "seams", "шви", "швы"},
    {Text::StudioRowsCount, "{1} of {2} pictures", "{1} з {2} картинок", "{1} из {2} картинок"},
    {Text::StudioCatalogEmpty,
     "The catalog is being made: the game's archives are read once (a few minutes), then kept.",
     "Каталог складається: архіви гри читаються один раз (кілька хвилин), далі він зберігається.",
     "Каталог составляется: архивы игры читаются один раз (несколько минут), дальше он хранится."},
    {Text::StudioViewPair, "Side by side", "Поруч", "Рядом"},
    {Text::StudioViewWipe, "Wipe", "Шторка", "Шторка"},
    {Text::StudioViewTiled, "Tiled 3\u00D73", "Плиткою 3\u00D73", "Плиткой 3\u00D73"},
    {Text::StudioViewMap, "As on the map", "Як на карті", "Как на карте"},
    {Text::StudioViewDiff, "Difference", "Різниця", "Разница"},
    {Text::StudioViewSmooth, "Smooth", "Згладжено", "Сглаженно"},
    {Text::StudioVsPack, "The pack's picture", "Картинка пакета", "Картинка пакета"},
    {Text::StudioVsMade, "Made again", "Створена наново", "Пересозданная"},
    {Text::StudioInfoFile, "File: {1}", "Файл: {1}", "Файл: {1}"},
    {Text::StudioInfoArchive, "Archive: {1}", "Архів: {1}", "Архив: {1}"},
    {Text::StudioInfoShadowed,
     "Also in (not shown, the first archive wins): {1}", "Також у (не показується, перший архів перемагає): {1}",
     "Также в (не показывается, первый архив побеждает): {1}"},
    {Text::StudioInfoKey, "Key: {1}", "Ключ: {1}", "Ключ: {1}"},
    {Text::StudioInfoLow,
     "At lower texture details: {1}", "За нижчої деталізації текстур: {1}", "При меньшей детализации текстур: {1}"},
    {Text::StudioInfoPicture, "Picture in the pack: {1}, {2}\u00D7{3}", "Картинка в пакеті: {1}, {2}\u00D7{3}",
     "Картинка в пакете: {1}, {2}\u00D7{3}"},
    {Text::StudioInfoNoPicture,
     "No picture in the pack: the game shows its own texture (or makes one, with [ai] generate).",
     "Картинки в пакеті немає: гра показує свою текстуру (або створює, з [ai] generate).",
     "Картинки в пакете нет: игра показывает свою текстуру (или создаёт, с [ai] generate)."},
    {Text::StudioInfoMade, "Made {1} by {2} ({3})", "Створено {1} моделлю {2} ({3})", "Создано {1} моделью {2} ({3})"},
    {Text::StudioInfoFrom, "Made of {1} from {2}", "Створено з {1} з {2}", "Создано из {1} из {2}"},
    {Text::StudioInfoMaps, "Maps ({1}): {2}", "Карти ({1}): {2}", "Карты ({1}): {2}"},
    {Text::StudioInfoAtlas, "The maps lay it as an atlas of {1}\u00D7{2} tiles: each tile is made on its own.",
     "Карти кладуть її як атлас {1}\u00D7{2} плиток: кожна плитка створюється окремо.",
     "Карты кладут её как атлас {1}\u00D7{2} плиток: каждая плитка создаётся отдельно."},
    {Text::StudioInfoOnePicture,
     "The maps lay it as one picture.", "Карти кладуть її однією картинкою.", "Карты кладут её одной картинкой."},
    {Text::StudioInfoColour, "Colour: {1}", "Колір: {1}", "Цвет: {1}"},
    {Text::StudioInfoMetrics,
     "Detail kept {1}%, tone shift {2}, bands {3}%, halos {4}%, pink fringe {5}%, alpha {6}%, seams {7}",
     "Збережено деталей {1}%, зсув тону {2}, смуги {3}%, ореоли {4}%, рожева облямівка {5}%, альфа {6}%, шви {7}",
     "Сохранено деталей {1}%, сдвиг тона {2}, полосы {3}%, ореолы {4}%, розовая кайма {5}%, альфа {6}%, швы {7}"},
    {Text::StudioInfoMaterial,
     "Material: gloss {1}, relief {2}, height {3}", "Матеріал: блиск {1}, рельєф {2}, висота {3}",
     "Материал: блеск {1}, рельеф {2}, высота {3}"},
    {Text::StudioInfoOwn, "The game's own always: no picture is made or shown for it.",
     "Завжди своя гри: картинка для неї не створюється й не показується.",
     "Всегда своя игры: картинка для неё не создаётся и не показывается."},
    {Text::StudioInfoOrphan, "A picture of the pack no texture of the game names (its texture changed or is gone): {1}",
     "Картинка пакета, якої не називає жодна текстура гри (її текстура змінилася або зникла): {1}",
     "Картинка пакета, которую не называет ни одна текстура игры (её текстура изменилась или пропала): {1}"},
    {Text::StudioMakeAgain, "Make again\u2026", "Створити наново\u2026", "Пересоздать\u2026"},
    {Text::StudioKeep, "Keep", "Залишити", "Оставить"},
    {Text::StudioDiscard, "Discard", "Відкинути", "Отбросить"},
    {Text::StudioTakeOut, "Take out of the pack", "Прибрати з пакета", "Убрать из пакета"},
    {Text::StudioOwnAlways, "The game's own always", "Завжди своя гри", "Всегда своя игры"},
    {Text::StudioOwnNot, "Back to the pack's", "Знову з пакета", "Снова из пакета"},
    {Text::StudioMakeTitle, "Make again: {1}", "Створити наново: {1}", "Пересоздать: {1}"},
    {Text::StudioMakeModel, "Model", "Модель", "Модель"},
    {Text::StudioMakeScale, "Size", "Розмір", "Размер"},
    {Text::StudioMakeDenoise, "Denoise (general)", "Шумоподавлення (general)", "Шумоподавление (general)"},
    {Text::StudioMakeOwnDenoise, "Its own, not the settings'", "Своє, не з налаштувань", "Своё, не из настроек"},
    {Text::StudioMakeBefore, "Before the network", "Перед мережею", "Перед сетью"},
    {Text::StudioMakeDeblock,
     "Smooth a JPG's 8\u00D78 blocks", "Згладити блоки JPG 8\u00D78", "Сгладить блоки JPG 8\u00D78"},
    {Text::StudioMakeKey, "Repair the colour key (magenta)", "Полагодити колірний ключ (пурпурний)",
     "Починить цветовой ключ (пурпурный)"},
    {Text::StudioMakeAfter, "After the network", "Після мережі", "После сети"},
    {Text::StudioMakeTone, "Tone as the original's", "Тон як в оригіналу", "Тон как у оригинала"},
    {Text::StudioMakeGrain, "The original's grain back", "Повернути зерно оригіналу", "Вернуть зерно оригинала"},
    {Text::StudioMakeSharpen, "Sharpen", "Різкість", "Резкость"},
    {Text::StudioMakeAlpha, "Clean the alpha (no pink fringe)", "Чиста прозорість (без рожевої облямівки)",
     "Чистая прозрачность (без розовой каймы)"},
    {Text::StudioMakeBlend, "A second model where the original has edges", "Друга модель там, де в оригіналі краї",
     "Вторая модель там, где у оригинала края"},
    {Text::StudioMakeOriginal, "Mix in the original", "Домішати оригінал", "Подмешать оригинал"},
    {Text::StudioMakeTiles, "Atlas tiles one by one ({1}\u00D7{2}): no tile takes its neighbour's edge",
     "Плитки атласу окремо ({1}\u00D7{2}): жодна не бере краю сусідньої",
     "Плитки атласа по одной ({1}\u00D7{2}): ни одна не берёт края соседней"},
    {Text::StudioMakeGo, "Make", "Створити", "Создать"},
    {Text::StudioCancel, "Cancel", "Скасувати", "Отмена"},
    {Text::StudioNone, "none", "немає", "нет"},
    {Text::StudioMakeMany, "Make again: {1} textures", "Створити наново: текстур {1}", "Пересоздать: текстур {1}"},
    {Text::StudioJobMake, "Make again {1}", "Створення {1}", "Пересоздание {1}"},
    {Text::StudioJobKeep, "Keep {1}", "Залишити {1}", "Оставить {1}"},
    {Text::StudioJobTakeOut, "Take out {1}", "Прибрати {1}", "Убрать {1}"},
    {Text::StudioJobOwn, "The game's own {1}", "Своя гри {1}", "Своя игры {1}"},
    {Text::StudioJobFailed, "{1} failed: {2}", "{1} не вдалося: {2}", "{1} не удалось: {2}"},
    {Text::StudioPackPictures, "Pictures: {1}; pictures no texture names: {2}; textures without a picture: {3}",
     "Картинок: {1}; картинок, яких не називає жодна текстура: {2}; текстур без картинки: {3}",
     "Картинок: {1}; картинок, которых не называет ни одна текстура: {2}; текстур без картинки: {3}"},
    {Text::StudioPackSize, "Size: {1}; held by pictures replaced or taken out: {2}",
     "Розмір: {1}; зайнято картинками, заміненими або прибраними: {2}",
     "Размер: {1}; занято картинками, заменёнными или убранными: {2}"},
    {Text::StudioPackByGroup, "By group: {1}", "За групою: {1}", "По группе: {1}"},
    {Text::StudioPackByModel, "By model: {1}", "За моделлю: {1}", "По модели: {1}"},
    {Text::StudioPackByKind, "By kind: {1}", "За видом: {1}", "По виду: {1}"},
    {Text::StudioPackRead, "Read again", "Прочитати знову", "Прочитать заново"},
    {Text::StudioPackCompact, "Compact", "Стиснути", "Сжать"},
    {Text::StudioPackCompactNote,
     "Compact: the pack copied into a new file without the space replaced and taken-out pictures hold. "
     "Close the game first: it holds the pack open.",
     "Стиснути: пакет копіюється в новий файл без місця, яке тримають замінені й прибрані картинки. "
     "Спершу закрийте гру: вона тримає пакет відкритим.",
     "Сжать: пакет копируется в новый файл без места, которое держат заменённые и убранные картинки. "
     "Сначала закройте игру: она держит пакет открытым."},
    {Text::StudioJobCompact, "Compact the pack", "Стиснути пакет", "Сжать пакет"},
    {Text::StudioPackGameOpen,
     "close the game first: it holds the pack open", "спершу закрийте гру: вона тримає пакет відкритим",
     "сначала закройте игру: она держит пакет открытым"},
    {Text::StudioPackNone, "No pack yet: uaro-textures.zip is made on the settings app's Texture pack page.",
     "Пакета ще немає: uaro-textures.zip створюється на сторінці «Пакет текстур» утиліти.",
     "Пакета ещё нет: uaro-textures.zip создаётся на странице «Пакет текстур» утилиты."},
    {Text::StudioLogDone, "{1}: done", "{1}: готово", "{1}: готово"},
    {Text::DiagXray, "X-ray of the AI pictures", "Рентген картинок ШІ", "Рентген картинок ИИ"},
    {Text::DiagXrayHint,
     "Ctrl+Shift+F9 in the game as well: a texture tinted green comes from the pack, blue was made in the game, red "
     "has no picture, grey stays the game's (soft light, its kind off, too big, the game's own always).",
     "Також Ctrl+Shift+F9 у грі: текстура зеленим - з пакета, синім - створена в грі, червоним - без картинки, сірим - "
     "лишається грою (м'яке світло, вид вимкнено, завелика, завжди своя гри).",
     "Также Ctrl+Shift+F9 в игре: текстура зелёным - из пакета, синим - создана в игре, красным - без картинки, "
     "серым - остаётся игры (мягкий свет, вид выключен, слишком большая, всегда своя игры)."},
    {Text::StudioViewMaterial, "Material", "Матеріал", "Материал"},
    {Text::StudioMatGloss, "Gloss", "Блиск", "Блеск"},
    {Text::StudioMatRelief, "Relief", "Рельєф", "Рельеф"},
    {Text::StudioMatHeight, "Height", "Висота", "Высота"},
    {Text::StudioMatSave, "Save the material", "Зберегти матеріал", "Сохранить материал"},
    {Text::StudioJobMaterial, "Material {1}", "Матеріал {1}", "Материал {1}"},
    // 2.4: the parts of the effects
    {Text::FxWaterFoam, "Foam along the shore", "Піна вздовж берега", "Пена вдоль берега"},
    {Text::FxWaterFoamHint, "White lines of foam roll in where the water meets the shore, a thin rim at its very edge.",
     "Білі смуги піни набігають там, де вода сходиться з берегом, біля самого краю тонка облямівка.",
     "Белые полосы пены набегают там, где вода сходится с берегом, у самого края тонкая кромка."},
    {Text::FxWaterGlints, "Sun glints on the water", "Відблиски сонця на воді", "Блики солнца на воде"},
    {Text::FxWaterGlintsHint, "The sun sparkles on the waves; in dungeons and caves hardly at all.",
     "Сонце виблискує на хвилях; у підземеллях і печерах ледь-ледь.",
     "Солнце сверкает на волнах; в подземельях и пещерах едва-едва."},
    {Text::FxCaustics, "Light on the bed", "Світло на дні", "Свет на дне"},
    {Text::FxCausticsHint,
     "Where the water is shallow, the waves gather the sun into a moving net of light on the bed.",
     "Де вода мілка, хвилі збирають сонце в рухливу сітку світла на дні.",
     "Где вода мелкая, волны собирают солнце в бегущую сетку света на дне."},
    {Text::FxWaterfalls, "Waterfalls", "Водоспади", "Водопады"},
    {Text::FxWaterfallsHint,
     "The waterfalls the maps' makers placed and the game never shows (Brasilis) fall and foam.",
     "Водоспади, які поставили автори карт, а гра ніколи не показує (Бразиліс), падають і піняться.",
     "Водопады, которые поставили авторы карт, а игра никогда не показывает (Бразилис), падают и пенятся."},
    {Text::FxStepDust, "Dust from steps", "Пил з-під ніг", "Пыль из-под ног"},
    {Text::FxStepDustHint,
     "Away from snow and sand, each step under the open sky lifts a little dust, on paved squares too.",
     "Поза снігом і піском кожен крок під відкритим небом здіймає трохи пилу, також на бруківці.",
     "Вне снега и песка каждый шаг под открытым небом поднимает немного пыли, в том числе на брусчатке."},
    {Text::FxSeasonSnow, "Snow in winter", "Сніг узимку", "Снег зимой"},
    {Text::FxSeasonSnowHint, "From December to February snow lies on the roofs, the tops of the rocks and the ground.",
     "З грудня по лютий сніг лежить на дахах, верхівках скель і землі.",
     "С декабря по февраль снег лежит на крышах, верхушках скал и земле."},
    {Text::FxSeasonGrass, "Grass by the season", "Трава за сезоном", "Трава по сезону"},
    {Text::FxSeasonGrassHint,
     "From September to February the green ground goes drier, towards straw; from March to May it is greener.",
     "З вересня по лютий зелена земля сухіша, ближче до соломи; з березня по травень — зеленіша.",
     "С сентября по февраль зелёная земля суше, ближе к соломе; с марта по май — зеленее."},
    {Text::FxPollen, "Pollen outdoors", "Пилок надворі", "Пыльца на улице"},
    {Text::FxPollenHint,
     "Over the fields and the towns without weather of their own, pollen floats in the air; less of it at night.",
     "Над полями й містами без своєї погоди в повітрі пливе пилок; уночі його менше.",
     "Над полями и городами без своей погоды в воздухе плывёт пыльца; ночью её меньше."},
    {Text::FxIndoorDust, "Dust indoors", "Пил у приміщеннях", "Пыль в помещениях"},
    {Text::FxIndoorDustHint, "In the dungeons and the houses fine dust floats in the air.",
     "У підземеллях і будинках у повітрі пливе дрібний пил.",
     "В подземельях и домах в воздухе плывёт мелкая пыль."},
    {Text::FxRainSplashes, "Rain splashes", "Бризки дощу", "Брызги дождя"},
    {Text::FxRainSplashesHint, "Where it rains, rings spread on the ground where the drops land (Direct3D 12).",
     "Де йде дощ, на землі розходяться кола там, де падають краплі (Direct3D 12).",
     "Где идёт дождь, на земле расходятся круги там, куда падают капли (Direct3D 12)."},
    {Text::FxDesertHeat, "Heat over the desert", "Марево над пустелею", "Марево в пустыне"},
    {Text::FxDesertHeatHint,
     "Far off in the desert around Morroc the picture shimmers in the hot air; the heat over fire and lava stays.",
     "Удалині в пустелі довкола Моррока картинка тремтить у гарячому повітрі; марево над вогнем і лавою лишається.",
     "Вдали в пустыне вокруг Морокка картинка дрожит в горячем воздухе; марево над огнём и лавой остаётся."},
    {Text::FxSkyRays, "Rays from the sky", "Промені з неба", "Лучи с неба"},
    {Text::FxSkyRaysHint,
     "On the fields rays of light pour down from the sky past the map's edge; the light around the fires stays.",
     "На полях промені світла ллються з неба за краєм карти; світло довкола вогню лишається.",
     "На полях лучи света льются с неба за краем карты; свет вокруг огня остаётся."},
    {Text::FxTreeSeasons, "Autumn and winter crowns", "Осінні та зимові крони", "Осенние и зимние кроны"},
    {Text::FxTreeSeasonsHint,
     "From September to November some crowns turn golden and red in patches; in Lutie rime lies on their tops.",
     "З вересня по листопад частина крон плямами жовтіє й червоніє; у Лютії на верхівках лежить іній.",
     "С сентября по ноябрь часть крон пятнами желтеет и краснеет; в Лютие на верхушках лежит иней."},
    {Text::FxDragonflies, "Dragonflies", "Бабки", "Стрекозы"},
    {Text::FxDragonfliesHint, "By day a dragonfly or two hovers over the water and darts off (Direct3D 12).",
     "Удень одна-дві бабки зависають над водою й різко перелітають (Direct3D 12).",
     "Днём над водой зависают и резко перелетают одна-две стрекозы (Direct3D 12)."},
    // 2.4.2: the game's key in setup.exe's copy alone, while the game runs: taken over once it is closed
    {Text::DiagRegistryOld, "setup.exe's alone so far: this app takes them over once the game is closed",
     "поки лише від setup.exe: утиліта забере їх, коли гру буде закрито",
     "пока только от setup.exe: утилита заберёт их, когда игра будет закрыта"},
    // 2.4.2: DLSS at the player's own size, its network
    {Text::DlssCustom, "Own size", "Свій розмір", "Свой размер"},
    {Text::DlssScale, "Own size: share of the screen", "Свій розмір: частка екрана", "Свой размер: доля экрана"},
    {Text::DlssScaleHint,
     "With DLSS on Own size: the world is drawn at this share of the screen, and DLSS enlarges it to the screen. 100 % "
     "is DLAA. A size DLSS does not take is brought to the nearest one it does.",
     "Для DLSS «Свій розмір»: світ малюється в такій частці екрана, а DLSS збільшує його до екрана. 100 % — це DLAA. "
     "Розмір, якого DLSS не бере, зводиться до найближчого, який бере.",
     "Для DLSS «Свой размер»: мир рисуется в такой доле экрана, а DLSS увеличивает его до экрана. 100 % — это DLAA. "
     "Размер, который DLSS не берёт, сводится к ближайшему, который берёт."},
    {Text::DlssWidth, "Own size: width", "Свій розмір: ширина", "Свой размер: ширина"},
    {Text::DlssHeight, "Own size: height", "Свій розмір: висота", "Свой размер: высота"},
    {Text::DlssSizeHint,
     "In pixels, by hand, over the share: 0 is by the share. One of them alone: the other one follows the screen's "
     "proportions.",
     "У пікселях, вручну, замість частки: 0 — за часткою. Якщо задано лише одне, друге — за пропорціями екрана.",
     "В пикселях, вручную, вместо доли: 0 — по доле. Если задано только одно, второе — по пропорциям экрана."},
    {Text::DlssModel, "DLSS model", "Модель DLSS", "Модель DLSS"},
    {Text::DlssModelHint,
     "The network DLSS draws with, in any mode. Auto is NVIDIA's for the mode: K for DLAA, Quality and Balanced, M "
     "for Performance, L for Ultra performance.",
     "Мережа, якою малює DLSS, у будь-якому режимі. «Авто» — вибір NVIDIA для режиму: K для DLAA, «Якості» й "
     "«Балансу», M для «Продуктивності», L для «Ультрапродуктивності».",
     "Сеть, которой рисует DLSS, в любом режиме. «Авто» — выбор NVIDIA для режима: K для DLAA, «Качества» и "
     "«Баланса», M для «Производительности», L для «Ультрапроизводительности»."},
    {Text::DlssModelAuto, "Auto", "Авто", "Авто"},
    {Text::DlssModelK, "K - the best picture", "K — найкраща картинка", "K — лучшая картинка"},
    {Text::DlssModelJ,
     "J - less smearing, more flicker", "J — менше шлейфів, більше мерехтіння", "J — меньше шлейфов, больше мерцания"},
    {Text::DlssModelL,
     "L - sharper and steadier, heavier", "L — різкіша й стабільніша, важча", "L — резче и стабильнее, тяжелее"},
    {Text::DlssModelM, "M - near L, faster", "M — близька до L, швидша", "M — близко к L, быстрее"},
    {Text::DlssCustomNow,
     "Own size on this screen: {1}×{2} of {3}×{4} ({5} %); DLSS may bring it to the nearest size it takes.",
     "Свій розмір на цьому екрані: {1}×{2} з {3}×{4} ({5} %); DLSS може звести його до найближчого, який бере.",
     "Свой размер на этом экране: {1}×{2} из {3}×{4} ({5} %); DLSS может свести его к ближайшему, который берёт."},
    // 2.5.19
    {Text::ServicePalettes,
     "Game palettes (Palettes.grf)", "Палітри гри (Palettes.grf)", "Палитры игры (Palettes.grf)"},
    {Text::PalettesNote,
     "Makes Palettes.grf with only the palettes the server lets be worn: hair styles 0-{1}, clothes colours 0-{2}, "
     "and every hair colour there is - should the server allow more, the game has them at once. Costumes and the "
     "Doram race are unknown to the 2007 client and stay out. The same palette is kept once: Kamishi's pack (219 "
     "thousand palettes, 153 MB) becomes about 23,600 in 4-5 MB. A palette the client does not find is drawn in the "
     "sprite's own colours, without an error.",
     "Збирає Palettes.grf лише з палітрами, які дозволяє сервер: зачіски 0–{1}, кольори одягу 0–{2} і всі кольори "
     "волосся, які є, — якщо сервер дозволить більше, у грі вони з'являться одразу. Костюмів і Дорамів клієнт 2007 "
     "року не знає — вони не беруться. Однакова палітра зберігається один раз: пак Kamishi (219 тисяч палітр, "
     "153 МБ) стає приблизно 23 600 у 4–5 МБ. Палітру, якої клієнт не знайде, він малює рідними кольорами спрайта, "
     "без помилки.",
     "Собирает Palettes.grf только с палитрами, которые разрешает сервер: причёски 0–{1}, цвета одежды 0–{2} и все "
     "цвета волос, какие есть, — если сервер разрешит больше, в игре они появятся сразу. Костюмов и Дорамов клиент "
     "2007 года не знает — они не берутся. Одинаковая палитра хранится один раз: пак Kamishi (219 тысяч палитр, "
     "153 МБ) становится примерно 23 600 в 4–5 МБ. Палитру, которой клиент не найдёт, он рисует родными цветами "
     "спрайта, без ошибки."},
    {Text::PalettesFromFile, "Make from a GRF or ZIP...", "Зібрати з GRF або ZIP…", "Собрать из GRF или ZIP…"},
    {Text::PalettesFromFolder, "Make from a folder...", "Зібрати з теки…", "Собрать из папки…"},
    {Text::PalettesWorking, "Making Palettes.grf...", "Збирається Palettes.grf…", "Собирается Palettes.grf…"},
    {Text::PalettesDone, "Made {1}: {2} palettes ({3} different), {4} MB in {5} s.",
     "Зібрано {1}: палітр {2} (різних {3}), {4} МБ за {5} с.",
     "Собрано {1}: палитр {2} (разных {3}), {4} МБ за {5} с."},
    {Text::PalettesLeft,
     "Left out: hair past the limits {1}, clothes past them {2}, costumes and the Doram race {3}, names the client "
     "never asks for {4}, empty {5}, files that are no palettes {6}.",
     "Не взято: волосся понад межі {1}, одяг понад межі {2}, костюми й Дорами {3}, імена, яких клієнт не питає, "
     "{4}, порожні {5}, файли, що не палітри, {6}.",
     "Не взято: волосы сверх пределов {1}, одежда сверх пределов {2}, костюмы и Дорамы {3}, имена, которые клиент не "
     "спрашивает, {4}, пустые {5}, файлы, которые не палитры, {6}."},
    {Text::PalettesFailed, "Not made: {1}", "Не зібрано: {1}", "Не собрано: {1}"},
    {Text::PalettesSameFile,
     "The source and the result are one file: save the result elsewhere and put it in place afterwards.",
     "Джерело й результат — один файл: збережіть результат деінде, а потім покладіть на місце.",
     "Источник и результат — один файл: сохраните результат в другое место, а потом положите на место."},
    {Text::PalettesFolderTitle, "The folder with the palettes (data\\palette, or a folder it is in)",
     "Тека з палітрами (data\\palette або тека, у якій вона)",
     "Папка с палитрами (data\\palette или папка, в которой она лежит)"},
    {Text::FileFilterGrfZip, "GRF or ZIP archives", "Архіви GRF або ZIP", "Архивы GRF или ZIP"},
    {Text::FileFilterGrf, "GRF archive", "Архів GRF", "Архив GRF"},
    // 2.5.20
    {Text::QualitySuperLight, "Super-light mode", "Супер-легкий режим", "Супер-лёгкий режим"},
    {Text::QualitySuperLightHint,
     "For the weakest graphics cards, over the settings below while it is on: no effect and no AI picture, the world "
     "at half the picture's size, no anisotropy, sharpening or smoothing; with Direct3D 11 also the world's textures "
     "as their colour alone (a clay look in the map's colours), a 16-bit world, mip levels without blending. The "
     "interface and the characters stay as they are. Turned off, everything comes back as it was.",
     "Для найслабших відеокарт, поверх налаштувань нижче, поки ввімкнено: жодного ефекту й жодної картинки ШІ, світ "
     "у половину розміру картинки, без анізотропії, різкості й згладжування; з Direct3D 11 ще й текстури світу самим "
     "їхнім кольором (наче глина в кольорах карти), 16-бітний світ, мип-рівні без змішування. Інтерфейс і персонажі "
     "лишаються як є. Вимкнете — усе повернеться як було.",
     "Для самых слабых видеокарт, поверх настроек ниже, пока включено: ни одного эффекта и ни одной картинки ИИ, мир "
     "в половину размера картинки, без анизотропии, резкости и сглаживания; с Direct3D 11 ещё и текстуры мира одним "
     "их цветом (как глина в цветах карты), 16-битный мир, мип-уровни без смешивания. Интерфейс и персонажи "
     "остаются как есть. Выключите — всё вернётся как было."},
    {Text::FramesBattery, "Save the laptop's battery", "Берегти батарею ноутбука", "Беречь батарею ноутбука"},
    {Text::FramesBatteryHint,
     "While a laptop runs on its battery: the frames limited (below), the effects at most the weak set's, the world "
     "at 75 % (Direct3D 11), 5 frames when the game is minimised. On the mains again, everything as before.",
     "Поки ноутбук працює від батареї: кадри обмежено (нижче), ефекти не сильніші за слабкий набір, світ у 75 % "
     "(Direct3D 11), 5 кадрів, коли гру згорнуто. Від мережі — усе як раніше.",
     "Пока ноутбук работает от батареи: кадры ограничены (ниже), эффекты не сильнее слабого набора, мир в 75 % "
     "(Direct3D 11), 5 кадров, когда игра свёрнута. От сети — всё как раньше."},
    {Text::FramesBatteryFps, "Frames on the battery", "Кадрів від батареї", "Кадров от батареи"},
    {Text::FramesBatteryFpsHint,
     "Frames a second at most while the laptop runs on its battery (the battery saver above). 30 is about half the "
     "work of 60.",
     "Кадрів за секунду найбільше, поки ноутбук працює від батареї (збереження батареї вище). 30 — це приблизно "
     "половина роботи від 60.",
     "Кадров в секунду не больше, пока ноутбук работает от батареи (экономия батареи выше). 30 — это примерно "
     "половина работы от 60."},
    // 2.5.23
    {Text::AiDyes, "Dyed clothes and hair", "Пофарбований одяг і волосся", "Крашеная одежда и волосы"},
    {Text::AiDyesHint,
     "Clothes and hair in any colour the game gives them: the pack's picture in the sprite's own colours, coloured "
     "as the dye. Off: a dyed character as the game draws it, without AI.",
     "Одяг і волосся будь-якого кольору, який дає гра: картинка пакета у власних кольорах спрайта, перефарбована "
     "під фарбу. Вимкнено: пофарбований персонаж такий, як його малює гра, без ШІ.",
     "Одежда и волосы любого цвета, который даёт игра: картинка пакета в родных цветах спрайта, перекрашенная под "
     "краску. Выключено: крашеный персонаж такой, как его рисует игра, без ИИ."},
    {Text::FramesReflex, "NVIDIA Reflex", "NVIDIA Reflex", "NVIDIA Reflex"},
    {Text::FramesReflexHint,
     "With DLSS: each frame waits until the graphics card is about to be free, and the game reads the mouse only "
     "then - the cursor follows the mouse sooner, the frames a second stay nearly the same. Frame generation turns it "
     "on anyway. Applies at the next start of the game.",
     "З DLSS: кожен кадр чекає, доки відеокарта майже звільниться, і гра читає мишу лише тоді — курсор раніше йде за "
     "мишею, кадрів на секунду майже стільки ж. Генерація кадрів вмикає його й так. Діє з наступного запуску гри.",
     "С DLSS: каждый кадр ждёт, пока видеокарта почти освободится, и игра читает мышь только тогда — курсор раньше "
     "идёт за мышью, кадров в секунду почти столько же. Генерация кадров включает его и так. Действует со "
     "следующего запуска игры."},
    {Text::AiModelGameAi,
     "Game AI 2.0 (game textures, real and drawn; not for small icons; downloaded apart, 29 MB; 3 times slower than "
     "anime 6B)",
     "Game AI 2.0 (ігрові текстури, реалістичні й мальовані; не для дрібних іконок; завантажується окремо, 29 МБ; "
     "утричі повільніша за anime 6B)",
     "Game AI 2.0 (игровые текстуры, реалистичные и рисованные; не для мелких иконок; скачивается отдельно, 29 МБ; "
     "втрое медленнее anime 6B)"},
    {Text::AiGameAiMissing,
     "Game AI is not on this computer yet: it comes apart from the package (29 MB). Until it is downloaded, pictures "
     "by it are not made.",
     "Game AI ще немає на цьому комп'ютері: вона йде окремо від пакета (29 МБ). Доки її не завантажено, картинки нею "
     "не робляться.",
     "Game AI ещё нет на этом компьютере: она идёт отдельно от пакета (29 МБ). Пока её не скачали, картинки ею не "
     "делаются."},
    {Text::AiGetModels, "Download Game AI (29 MB)", "Завантажити Game AI (29 МБ)", "Скачать Game AI (29 МБ)"},
    {Text::AiGetModelsFailed, "Game AI could not be downloaded: {1}",
     "Не вдалося завантажити Game AI: {1}",
     "Не удалось скачать Game AI: {1}"},
    {Text::PackUnread, "Of it {1} is pictures made again or taken out: the game never reads them, they only take room.",
     "З нього {1} — картинки, зроблені заново або прибрані: гра їх ніколи не читає, вони лише займають місце.",
     "Из него {1} — картинки, сделанные заново или убранные: игра их никогда не читает, они только занимают место."},
    {Text::PackCompact, "Compact", "Стиснути", "Сжать"},
    {Text::PackCompactNote,
     "Compact: the pack copied into a new file without that room. Close the game first: it holds the pack open.",
     "Стиснути: пакет копіюється в новий файл без цього місця. Спершу закрийте гру: вона тримає пакет відкритим.",
     "Сжать: пакет копируется в новый файл без этого места. Сначала закройте игру: она держит пакет открытым."},
    {Text::PackCompactAsk,
     "Compact uaro-textures.zip? It is copied whole without the {1} of old pictures: the disk needs {2} free for a "
     "while. Close the game first.",
     "Стиснути uaro-textures.zip? Його буде скопійовано цілком без {1} старих картинок: на диску на час копіювання "
     "потрібно {2} вільного місця. Спершу закрийте гру.",
     "Сжать uaro-textures.zip? Он будет скопирован целиком без {1} старых картинок: на диске на время копирования "
     "нужно {2} свободного места. Сначала закройте игру."},
    {Text::PackCompacting,
     "Compacting: {1} of {2} pictures…", "Стискання: {1} з {2} картинок…", "Сжатие: {1} из {2} картинок…"},
    {Text::PackCompacted,
     "Compacted: the pack is {1} smaller.", "Стиснуто: пакет менший на {1}.", "Сжато: пакет меньше на {1}."},
    {Text::PackCompactFailed, "Not compacted: {1}", "Не стиснуто: {1}", "Не сжато: {1}"},
    {Text::AiSectionGround, "World: the ground", "Світ: земля", "Мир: земля"},
    {Text::AiGroupGroundTown, "Towns", "Міста", "Города"},
    {Text::AiGroupGroundField, "Fields", "Поля", "Поля"},
    {Text::AiGroupGroundDungeon, "Dungeons", "Підземелля", "Подземелья"},
    {Text::AiGroupGroundIndoor, "Floors indoors", "Підлога в приміщеннях", "Полы помещений"},
    {Text::AiGroupGroundOther, "Other maps (guild castles, events, arenas)",
     "Інші карти (замки гільдій, події, арени)",
     "Прочие карты (замки гильдий, события, арены)"},
    {Text::AiSectionModels, "World: models", "Світ: моделі", "Мир: модели"},
    {Text::AiGroupBuildingsTown, "Town buildings", "Будівлі міст", "Здания городов"},
    {Text::AiGroupModelsField, "Fields' models (rocks, fences, ruins)",
     "Моделі полів (скелі, огорожі, руїни)",
     "Модели полей (скалы, заборы, руины)"},
    {Text::AiGroupModelsDungeon, "Dungeons' models", "Моделі підземель", "Модели подземелий"},
    {Text::AiGroupFurniture,
     "Furniture and things indoors", "Меблі й речі в приміщеннях", "Мебель и вещи в помещениях"},
    {Text::AiGroupModelsOther, "Other maps' models", "Моделі інших карт", "Модели прочих карт"},
    {Text::AiGroupTrees, "Trees", "Дерева", "Деревья"},
    {Text::AiGroupPlants, "Grass, bushes and flowers", "Трава, кущі й квіти", "Трава, кусты и цветы"},
    {Text::AiGroupWater, "Water", "Вода", "Вода"},
    {Text::AiSectionCharacters, "Characters", "Персонажі", "Персонажи"},
    {Text::AiGroupBodies, "Bodies", "Тіла", "Тела"},
    {Text::AiGroupMounted,
     "Bodies on mounts (Peco Peco, gryphons ...)", "Тіла верхи (Пеко, грифони ...)", "Тела верхом (Пеко, грифоны ...)"},
    {Text::AiGroupHeads, "Heads and hair", "Голови й зачіски", "Головы и причёски"},
    {Text::AiGroupWeapons, "Weapons", "Зброя", "Оружие"},
    {Text::AiGroupShields, "Shields", "Щити", "Щиты"},
    {Text::AiGroupHeadgear, "Headgear", "Головні убори", "Головные уборы"},
    {Text::AiGroupGarments, "Garments", "Плащі", "Плащи"},
    {Text::AiSectionMonsters, "Monsters and NPCs", "Монстри й NPC", "Монстры и NPC"},
    {Text::AiGroupBosses, "Bosses and MVPs", "Боси й MVP", "Боссы и MVP"},
    {Text::AiGroupHomunculi, "Homunculi", "Гомункули", "Гомункулы"},
    {Text::AiSectionItems, "Items", "Предмети", "Предметы"},
    {Text::AiGroupItemIcons, "Item icons", "Іконки предметів", "Иконки предметов"},
    {Text::AiGroupItemPictures, "Item pictures", "Картинки предметів", "Картинки предметов"},
    {Text::AiGroupCardPictures, "Card pictures", "Картинки карток", "Картинки карточек"},
    {Text::AiSectionEffects, "Effects", "Ефекти", "Эффекты"},
    {Text::AiGroupSkillAnimations, "Skill animations", "Анімації вмінь", "Анимации умений"},
    {Text::AiGroupEffectSprites, "Effect sprites", "Спрайти ефектів", "Спрайты эффектов"},
    {Text::AiSectionInterface, "Interface", "Інтерфейс", "Интерфейс"},
    {Text::AiGroupEmotions, "Emotions", "Емоції", "Эмоции"},
    {Text::AiGroupCursor, "Cursor", "Курсор", "Курсор"},
    {Text::AiGroupWindows, "Windows", "Вікна", "Окна"},
    {Text::AiGroupMenuIcons, "Menu icons", "Іконки меню", "Иконки меню"},
    {Text::AiGroupPortraits, "NPC portraits", "Портрети NPC", "Портреты NPC"},
    {Text::AiGroupMaps, "World map and minimaps", "Мапа світу й мінімапи", "Карта мира и миникарты"},
    {Text::AiGroupLoading, "Loading screens", "Заставки завантаження", "Экраны загрузки"},
    {Text::AiGroupLogin,
     "Login, character select and creation", "Вхід, вибір і створення персонажа", "Вход, выбор и создание персонажа"},
    // 2.6: the modes and the list's new entries
    {Text::ModeSimple, "Simple", "Простий", "Простой"},
    {Text::ModeDetailed, "Detailed", "Докладний", "Подробный"},
    {Text::ModeDeveloper, "Developer", "Розробника", "Разработчика"},
    {Text::ViewMain, "Main", "Головне", "Главное"},
    {Text::ViewGraphics, "Graphics", "Графіка", "Графика"},
    {Text::ViewGameChat, "Game and chat", "Гра й чат", "Игра и чат"},
    {Text::PageDebug, "Debugging", "Налагодження", "Отладка"},
    {Text::AiTabWhat, "What to improve", "Що покращувати", "Что улучшать"},
    {Text::AiTabModels, "Models by group", "Моделі за групами", "Модели по группам"},
    // 2.6: the Debug page's test switches
    {Text::DebugDlssJitter, "DLSS jitter inverted", "Джитер DLSS навпаки", "Джиттер DLSS наоборот"},
    {Text::DebugDlssJitterHint,
     "The sign of the camera's sub-pixel shift DLSS is given. Turn it on when the picture with DLSS comes out "
     "blurred.",
     "Знак зсуву камери на частку пікселя, який отримує DLSS. Увімкни, якщо картинка з DLSS розмита.",
     "Знак сдвига камеры на долю пикселя, который получает DLSS. Включи, если картинка с DLSS размыта."},
    {Text::DebugDlssMotion, "DLSS motion inverted", "Рух для DLSS навпаки", "Движение для DLSS наоборот"},
    {Text::DebugDlssMotionHint,
     "The sign of the motion vectors DLSS is given. Turn it on when moving things smear behind them with DLSS.",
     "Знак векторів руху, які отримує DLSS. Увімкни, якщо з DLSS за рухомим тягнеться слід.",
     "Знак векторов движения, которые получает DLSS. Включи, если с DLSS за движущимся тянется след."},
    {Text::DebugPresent, "Picture into the window", "Картинка у вікно", "Картинка в окно"},
    {Text::DebugPresentHint,
     "How the 64-bit renderer's picture reaches the game's window: straight into it, or through a surface Windows "
     "composes over it.",
     "Як картинка 64-бітного рендера потрапляє у вікно гри: прямо в нього чи через поверхню, яку Windows накладає "
     "зверху.",
     "Как картинка 64-битного рендера попадает в окно игры: прямо в него или через поверхность, которую Windows "
     "накладывает сверху."},
    {Text::DebugPresentAuto, "Auto", "Авто", "Авто"},
    {Text::DebugPresentWindow, "Window", "Вікно", "Окно"},
    {Text::DebugPresentComposition, "Composition", "Композиція", "Композиция"},
    {Text::DebugDpiScale,
     "Composition follows Windows scaling", "Композиція з масштабом Windows", "Композиция с масштабом Windows"},
    {Text::DebugDpiScaleHint,
     "The composition surface is scaled with the game's window when Windows scales it (125 %, 150 %).",
     "Поверхню композиції масштабовано разом із вікном гри, коли Windows його масштабує (125 %, 150 %).",
     "Поверхность композиции масштабируется вместе с окном игры, когда Windows его масштабирует (125 %, 150 %)."},
    {Text::DebugVariants, "A shader for each state", "Шейдер на кожен стан", "Шейдер на каждое состояние"},
    {Text::DebugVariantsHint,
     "Each state of the game's draws gets a shader made for it - Direct3D 11 since 2.5.16, Direct3D 12 since "
     "3.0.5-beta.3; a weak card draws them several times faster. Off: one shader for every state. Takes effect at "
     "the next start of the game.",
     "Кожен стан малювання гри отримує свій шейдер — Direct3D 11 з 2.5.16, Direct3D 12 з 3.0.5-beta.3; слабка "
     "відеокарта малює їх у кілька разів швидше. Вимкнено: один шейдер на все. Діє з наступного запуску гри.",
     "Каждое состояние рисования игры получает свой шейдер — Direct3D 11 с 2.5.16, Direct3D 12 с 3.0.5-beta.3; "
     "слабая видеокарта рисует их в несколько раз быстрее. Выключено: один шейдер на всё. Действует со следующего "
     "запуска игры."},
    {Text::DebugRecordScene, "Record a scene", "Записати сцену", "Записать сцену"},
    {Text::DebugRecordSceneHint,
     "The game's pictures as the driver hands them to the renderer, from the start to 15 s after the first map, into "
     "uaro-graphics\\scene-record.bin - the video card check without the game will play such scenes. The log tells "
     "its size. Takes effect at the next start; turn it off after.",
     "Картинки гри так, як драйвер передає їх рендеру, від запуску до 15 с після першої карти, у "
     "uaro-graphics\\scene-record.bin — перевірка відеокарти без гри програватиме такі сцени. Розмір — у журналі. "
     "Діє з наступного запуску; потім вимкніть.",
     "Картинки игры так, как драйвер передаёт их рендеру, от запуска до 15 с после первой карты, в "
     "uaro-graphics\\scene-record.bin — проверка видеокарты без игры будет проигрывать такие сцены. Размер — в "
     "журнале. Действует со следующего запуска; потом выключите."},
    {Text::DebugStateCache,
     "Direct3D 11: only what changed", "Direct3D 11: лише те, що змінилося", "Direct3D 11: только то, что изменилось"},
    {Text::DebugStateCacheHint,
     "Direct3D 11 is told only what changed between the game's draws. Off: everything at every draw, should a picture "
     "come out wrong.",
     "Direct3D 11 отримує лише те, що змінилося між малюваннями гри. Вимкнено: усе щоразу - якщо картинка вийде "
     "неправильною.",
     "Direct3D 11 получает только то, что изменилось между рисованиями игры. Выключено: всё каждый раз - если "
     "картинка выйдет неправильной."},
    {Text::DebugMergeDraws,
     "Direct3D 11: merge draws", "Direct3D 11: злиття малювань", "Direct3D 11: слияние рисований"},
    {Text::DebugMergeDrawsHint,
     "Draws of the game that follow one another in the same state go as one. Off: each by itself.",
     "Малювання гри поспіль з одним станом ідуть одним викликом. Вимкнено: кожне окремо.",
     "Рисования игры подряд с одним состоянием идут одним вызовом. Выключено: каждое отдельно."},
    {Text::DebugFastPick,
     "Fast pick under the mouse", "Швидкий пошук клітинки під мишею", "Быстрый поиск клетки под мышью"},
    {Text::DebugFastPickHint,
     "The cell under the mouse found along the mouse's ray. Off: the game's own search over the map's pieces.",
     "Клітинку під мишею знайдено вздовж променя миші. Вимкнено: власний пошук гри по шматках мапи.",
     "Клетка под мышью находится вдоль луча мыши. Выключено: собственный поиск игры по кускам карты."},
    {Text::DebugFastFtol, "Fast number rounding", "Швидке округлення чисел", "Быстрое округление чисел"},
    {Text::DebugFastFtolHint,
     "The game's conversion of numbers to whole ones by one instruction (fisttp). Off: the game's own.",
     "Перетворення чисел гри на цілі однією інструкцією (fisttp). Вимкнено: власне гри.",
     "Преобразование чисел игры в целые одной инструкцией (fisttp). Выключено: собственное игры."},
    {Text::DebugFastTextures, "Fast texture rows", "Швидкі рядки текстур", "Быстрые строки текстур"},
    {Text::DebugFastTexturesHint, "The game's texture rows copied with SSE2. Off: the game's own copy.",
     "Рядки текстур гри копіюються через SSE2. Вимкнено: власне копіювання гри.",
     "Строки текстур игры копируются через SSE2. Выключено: собственное копирование игры."},
    // 3.0.11-beta.128 (the network plan's step 5; the owner: «делать сеть, оно бывает глючит»)
    {Text::DebugQuickAcks, "Quick TCP acknowledgements", "Швидкі підтвердження TCP", "Быстрые подтверждения TCP"},
    {Text::DebugQuickAcksHint,
     "The game's connection acknowledges each of the server's packets at once - Windows waits up to 200 ms for a "
     "lone one, so the server notices its lost packet later and the world stands still longer. Off: Windows' own way.",
     "З'єднання гри підтверджує кожен пакет сервера одразу — Windows чекає до 200 мс на поодинокий, тож сервер "
     "пізніше помічає свій втрачений пакет і світ довше стоїть. Вимкнено: як заведено у Windows.",
     "Соединение игры подтверждает каждый пакет сервера сразу — Windows ждёт до 200 мс на одиночный, поэтому сервер "
     "позже замечает свой потерянный пакет и мир дольше стоит. Выключено: как принято в Windows."},
    {Text::DeveloperOnly, "Developer only", "Лише розробнику", "Только разработчику"},
    {Text::DebugTools, "Tools", "Інструменти", "Инструменты"},
    {Text::DebugStudio, "Open uaRO Studio", "Відкрити uaRO Studio", "Открыть uaRO Studio"},
    {Text::DebugCompact, "Compact the texture pack", "Стиснути пакет текстур", "Сжать пакет текстур"},
    {Text::DebugPalettes, "Build Palettes.grf", "Зібрати Palettes.grf", "Собрать Palettes.grf"},
    {Text::DebugPreview, "Model preview", "Попередній перегляд моделей", "Превью моделей"},
    {Text::DebugKeys,
     "In the game: Ctrl+~ the console; Ctrl+Shift+F10 «New report» with a snapshot; Ctrl+Shift+F11 the frame "
     "counter; Ctrl+Shift+F12 a full-size snapshot; "
     "Ctrl+Shift+F9 X-ray; Ctrl+Shift+F8 the texture under the cursor into uaRO Studio, Ctrl+Shift+F7 its picture off "
     "and on.",
     "У грі: Ctrl+~ - консоль; Ctrl+Shift+F10 - «Нове звернення» зі знімком; Ctrl+Shift+F11 - лічильник кадрів; "
     "Ctrl+Shift+F12 - знімок у повному розмірі; "
     "Ctrl+Shift+F9 - рентген; Ctrl+Shift+F8 - текстура під курсором в uaRO Studio, Ctrl+Shift+F7 - її картинка "
     "вимк./увімк.",
     "В игре: Ctrl+~ - консоль; Ctrl+Shift+F10 - «Новое обращение» со снимком; Ctrl+Shift+F11 - счётчик кадров; "
     "Ctrl+Shift+F12 - снимок в полном размере; "
     "Ctrl+Shift+F9 - рентген; Ctrl+Shift+F8 - текстура под курсором в uaRO Studio, Ctrl+Shift+F7 - её картинка "
     "выкл./вкл."},
    // 2.6: the Main page
    {Text::LookHead, "How the game looks", "Як виглядає гра", "Как выглядит игра"},
    {Text::LookNote,
     "Three choices, each on its own: the picture, the interface, the textures. Every setting stays on its page.",
     "Три окремі вибори: картинка, інтерфейс, текстури. Кожне налаштування й далі на своїй сторінці.",
     "Три отдельных выбора: картинка, интерфейс, текстуры. Каждая настройка по-прежнему на своей странице."},
    {Text::LookPicture, "Picture", "Картинка", "Картинка"},
    {Text::LookAuto, "Auto (by the card's speed)", "Авто (за швидкістю відеокарти)", "Авто (по скорости видеокарты)"},
    {Text::LookGame, "As in the game", "Як у грі", "Как в игре"},
    {Text::LookBetter, "Improved", "Покращена", "Улучшенная"},
    {Text::LookRemaster, "Remaster", "Ремастер", "Ремастер"},
    {Text::LookUi, "Interface", "Інтерфейс", "Интерфейс"},
    {Text::LookUiNew, "New", "Новий", "Новый"},
    {Text::LookUiScreen, "New screen, the game's windows", "Новий екран, вікна гри", "Новый экран, окна игры"},
    {Text::LookUiClassic, "Classic", "Класичний", "Классический"},
    {Text::LookTextures, "Textures", "Текстури", "Текстуры"},
    {Text::LookTexHd, "HD", "HD", "HD"},
    {Text::LookTexLight, "HD, light (weak cards)", "HD економно (слабкі відеокарти)",
     "HD экономно (слабые видеокарты)"},
    {Text::LookTexOriginal, "Original", "Оригінальні", "Оригинальные"},
    {Text::LookChanged, "{1} · changed {2}", "{1} · змінено {2}", "{1} · изменено {2}"},
    {Text::LookHint,
     "Choosing writes only this choice's settings; what you changed in them by hand is replaced (until you apply, "
     "«Undo» brings it back).",
     "Вибір пише лише свої налаштування; змінене в них вручну буде замінено (до «Застосувати» його повертає "
     "«Скасувати»).",
     "Выбор пишет только свои настройки; изменённое в них вручную будет заменено (до «Применить» его возвращает "
     "«Отменить»)."},
    {Text::PlayAnotherWindow, "One more game window", "Ще вікно гри", "Ещё окно игры"},
    {Text::PlayReport, "Report a problem", "Повідомити про помилку", "Сообщить об ошибке"},
    {Text::PlayLookChange, "Change \xE2\x80\xBA", "Змінити \xE2\x80\xBA", "Изменить \xE2\x80\xBA"},
    {Text::NewListHead, "New to you: {1}", "Нове для вас: {1}", "Новое для вас: {1}"},
    {Text::NewListNone, "Nothing new since you last looked", "Нічого нового відтоді, як ви дивилися",
     "Ничего нового с тех пор, как вы смотрели"},
    {Text::NewListNote,
     "What came with the recent versions; what changes the look is off at first. Switch it here and look in the game.",
     "Що з'явилося в останніх версіях; те, що змінює вигляд, спочатку вимкнено. Увімкніть тут і подивіться в грі.",
     "Что появилось в последних версиях; то, что меняет вид, сначала выключено. Включите здесь и посмотрите в игре."},
    {Text::ChangedListHead, "Set otherwise than recommended: {1}", "Змінено щодо рекомендованого: {1}",
     "Изменено относительно рекомендуемого: {1}"},
    {Text::ChangedListNone, "Everything as recommended", "Усе як рекомендовано", "Всё как рекомендовано"},
    {Text::ChangedListNote, "The dot beside a row puts its recommended value back.",
     "Крапка біля рядка повертає рекомендоване значення.", "Точка рядом со строкой возвращает рекомендуемое значение."},
    {Text::PlayNewFeatures, "\xE2\x98\x85 New features ({1})", "\xE2\x98\x85 Нові можливості ({1})",
     "\xE2\x98\x85 Новые возможности ({1})"},
    {Text::PlayChangedOnly, "Changed only", "Лише змінені", "Только изменённые"},
    {Text::KindAllNew, "I want everything new", "Хочу все нове", "Хочу всё новое"},
    {Text::KindNewLookOldUi, "New picture, the old interface", "Нова картинка, інтерфейс як раніше",
     "Новая картинка, интерфейс как раньше"},
    {Text::KindWeakPc, "A weak computer, but sharp", "Слабкий комп'ютер, але чітко", "Слабый компьютер, но чётко"},
    {Text::KindClassic, "As in 2004", "Як у 2004-му", "Как в 2004-м"},
    {Text::KindHead, "How do you want to play?", "Як ви хочете грати?", "Как вы хотите играть?"},
    {Text::KindNote, "One press sets the picture, the interface and the textures; change them any time on «Main».",
     "Одне натискання ставить картинку, інтерфейс і текстури; змінити можна будь-коли на «Головне».",
     "Одно нажатие ставит картинку, интерфейс и текстуры; изменить можно когда угодно на «Главное»."},
    {Text::KindSkip, "Leave as it is", "Залишити як є", "Оставить как есть"},
    {Text::SeenHead, "Updated to {1}", "Оновлено до {1}", "Обновлено до {1}"},
    {Text::SeenNote, "What came with it is under the buttons below; the new switches are off until you turn them on.",
     "Що з'явилося — під кнопками нижче; нові перемикачі вимкнено, доки ви їх не ввімкнете.",
     "Что появилось — под кнопками ниже; новые переключатели выключены, пока вы их не включите."},
    {Text::SeenClose, "Got it", "Зрозуміло", "Понятно"},
    {Text::PlayNewFeaturesShort, "\xE2\x98\x85 New features", "\xE2\x98\x85 Нові можливості",
     "\xE2\x98\x85 Новые возможности"},
    {Text::ConsoleHintLine, "uaRO Graphics: Alt+D - quick settings (the picture, the interface, the textures).",
     "uaRO Graphics: Alt+D — швидкі налаштування (картинка, інтерфейс, текстури).",
     "uaRO Graphics: Alt+D — быстрые настройки (картинка, интерфейс, текстуры)."},
    {Text::CardSuggest, "For {1} ({2}): picture «{3}», textures «{4}»", "Для {1} ({2}): картинка «{3}», текстури «{4}»",
     "Для {1} ({2}): картинка «{3}», текстуры «{4}»"},
    {Text::CardMemory, "{1} GB", "{1} ГБ", "{1} ГБ"},
    {Text::CardIntegrated, "integrated", "вбудована", "встроенная"},
    {Text::CardByGame, "Chosen by how this card drew the game.", "Підібрано за тим, як ця відеокарта малювала гру.",
     "Подобрано по тому, как эта видеокарта рисовала игру."},
    {Text::CardByMemory,
     "Chosen by the card's memory; after a game it is chosen by how the card drew it.",
     "Підібрано за пам'яттю відеокарти; після гри — за тим, як вона її малювала.",
     "Подобрано по памяти видеокарты; после игры — по тому, как она её рисовала."},
    {Text::CardApply, "Choose for my card", "Підібрати під відеокарту", "Подобрать под видеокарту"},
    {Text::CardFits, "Already as for your card", "Уже як для вашої відеокарти", "Уже как для вашей видеокарты"},
    {Text::MoreRowsShow, "{1} more settings \xE2\x80\xBA", "Ще {1} налаштувань \xE2\x80\xBA",
     "Ещё {1} настроек \xE2\x80\xBA"},
    {Text::MoreRowsHide, "Hide the extra settings", "Сховати додаткові", "Скрыть дополнительные"},
    {Text::ConsoleTabQuick, "Quick", "Швидко", "Быстро"},
    {Text::ConsoleTabAll, "All", "Усе", "Всё"},
    {Text::ConsoleOften, "Switched most", "Часто перемикають", "Часто переключают"},
    {Text::ConsolePinned, "\xE2\x98\x85 Pinned by you", "\xE2\x98\x85 Закріплені вами",
     "\xE2\x98\x85 Закреплённые вами"},
    {Text::ConsoleActions, "Actions", "Дії", "Действия"},
    {Text::ConsoleTabNew, "New", "Нове", "Новое"},
    {Text::ConsoleNewCount, "\xE2\x98\x85 New: {1} - the «New» tab", "\xE2\x98\x85 Нового: {1} - вкладка «Нове»",
     "\xE2\x98\x85 Нового: {1} - вкладка «Новое»"},
    {Text::ConsoleNewNone, "Nothing new since you looked", "Нічого нового відтоді, як ви дивилися",
     "Ничего нового с тех пор, как вы смотрели"},
    {Text::ConsoleNewNote, "New since you looked - try them here", "Нове відтоді, як ви дивилися, - спробуйте тут",
     "Новое с тех пор, как вы смотрели, - попробуйте здесь"},
    {Text::MainIntro,
     "What a game needs. Everything else is in the Detailed mode.", "Головне для гри. Усе інше - у докладному режимі.",
     "Главное для игры. Всё остальное - в подробном режиме."},
    {Text::MainPicture, "Picture", "Картинка", "Картинка"},
    {Text::MainQualityLow, "Low", "Низька", "Низкое"},
    {Text::MainQualityMedium, "Medium", "Середня", "Среднее"},
    {Text::MainQualityHigh, "High", "Висока", "Высокое"},
    {Text::MainQualitySuperLight, "Super-light", "Надлегка", "Супер-лёгкое"},
    {Text::MainQualityOwn, "Your own", "Своя", "Своё"},
    {Text::MainQualityHint,
     "The detail and the smoothing in one choice (the Service page's presets); Super-light for weak graphics cards. "
     "The effects are on the Effects page.",
     "Деталізація і згладжування одним вибором (пресети сторінки «Сервіс»); «Надлегка» - для слабких відеокарт. "
     "Ефекти - на сторінці «Ефекти».",
     "Детализация и сглаживание одним выбором (пресеты страницы «Сервис»); «Супер-лёгкое» - для слабых видеокарт. "
     "Эффекты - на странице «Эффекты»."},
    {Text::MainOther, "Other", "Інше", "Прочее"},
    {Text::MainRecommended, "Recommended settings", "Рекомендовані налаштування", "Рекомендуемые настройки"},
    {Text::MainWhatsNew, "What's new in {1}", "Що нового в {1}", "Что нового в {1}"},
    {Text::MainState, "State", "Стан", "Состояние"},
    {Text::MainDriver, "Driver {1}", "Драйвер {1}", "Драйвер {1}"},
    {Text::MainUpdateNone, "no updates", "оновлень немає", "обновлений нет"},
    {Text::MainUpdateNew, "{1} is out", "вийшла {1}", "вышла {1}"},
    {Text::MainNoD3d, "No Direct3D 11: the game's own look only", "Немає Direct3D 11: лише власний вигляд гри",
     "Нет Direct3D 11: только собственный вид игры"},
    {Text::MainDlss, "DLSS and frame generation are installed", "DLSS і генерацію кадрів встановлено",
     "DLSS и генерация кадров установлены"},
    {Text::MainNoDlss, "DLSS is not installed",
     "DLSS не встановлено",
     "DLSS не установлен"},
    {Text::MainHd, "HD textures", "HD-текстури", "HD-текстуры"},
    {Text::MainPack, "Pack: {1} pictures, {2}", "Пакет: {1} картинок, {2}", "Пакет: {1} картинок, {2}"},
    {Text::MainNoPack, "No texture pack yet: «Make the pack» makes it on this computer.",
     "Пакета текстур ще немає: «Зробити пакет» зробить його на цьому комп'ютері.",
     "Пакета текстур пока нет: «Сделать пакет» сделает его на этом компьютере."},
    {Text::MainMakePack, "Make the pack", "Зробити пакет", "Сделать пакет"},
    {Text::MainMore, "More", "Докладніше", "Подробнее"},
    {Text::NotesNone,
     "No notes for {1} were found next to uaRO.exe (TEST.*.md).", "Поруч з uaRO.exe немає нотаток до {1} (TEST.*.md).",
     "Рядом с uaRO.exe нет заметок к {1} (TEST.*.md)."},
    // 2.6: the header's search
    {Text::SearchCue, "Search settings   Ctrl+F", "Пошук у налаштуваннях   Ctrl+F", "Поиск по настройкам   Ctrl+F"},
    {Text::SearchFound, "«{2}»: {1} found", "«{2}»: знайдено {1}", "«{2}»: найдено {1}"},
    {Text::SearchNothing, "Nothing found for «{1}».", "За «{1}» нічого не знайдено.", "По «{1}» ничего не найдено."},
    {Text::SearchMany, "Only the first {1} are shown: add a word.", "Показано лише перші {1}: додайте слово.",
     "Показаны только первые {1}: добавьте слово."},
    {Text::SearchHow,
     "Change them right here; the button under each opens its page. Enter opens the first one's page, Esc goes back.",
     "Змінюйте прямо тут; кнопка під кожним відкриває його сторінку. Enter відкриває сторінку першого, Esc повертає "
     "назад.",
     "Меняйте прямо здесь; кнопка под каждым открывает его страницу. Enter открывает страницу первого, Esc "
     "возвращает назад."},
    {Text::SearchOtherMode, "In the {1} mode: its button switches the app to it",
     "У режимі {1}: кнопка перемкне застосунок у нього", "В режиме {1}: кнопка переключит приложение в него"},
    // 3.0.11-beta.302: «Інтерфейс» in sections
    {Text::InterfaceSecMain, "Main", "Головне", "Главное"},
    {Text::InterfaceSecPanels, "Panels on the screen", "Панелі на екрані", "Панели на экране"},
    {Text::InterfaceSecMinimap, "Minimap", "Мінікарта", "Миникарта"},
    {Text::InterfaceSecFight, "Fight", "Бій", "Бой"},
    {Text::InterfaceSecGround, "Things on the ground", "Речі на землі", "Вещи на земле"},
    {Text::InterfaceSecLogin, "Login and characters", "Вхід і персонажі", "Вход и персонажи"},
    {Text::InterfaceSecHomun, "Homunculus and mercenary", "Гомункул і найманець", "Гомункул и наёмник"},
    {Text::InterfaceSecNet, "Connection", "З'єднання", "Соединение"},
    {Text::InterfaceSecWindows, "Windows in the new look", "Вікна в новому вигляді", "Окна в новом виде"},
    {Text::InterfaceWindowsShow, "Show each window ({1})", "Показати кожне вікно ({1})", "Показать каждое окно ({1})"},
    {Text::InterfaceWindowsHide, "Hide the windows' list", "Сховати список вікон", "Скрыть список окон"},
    {Text::InterfacePadNote, "The controller's settings for the interface are on the Controller page.",
     "Налаштування контролера для інтерфейсу - на сторінці «Контролер».",
     "Настройки контроллера для интерфейса - на странице «Контроллер»."},
    // 2.6: the status bar, the "changed" dots, the page's reset, "What's new"
    {Text::DotHintReset, "Recommended for this computer: {1} - a click sets it",
     "Рекомендовано для цього комп'ютера: {1} - клік поверне", "Рекомендуется для этого компьютера: {1} - клик вернёт"},
    {Text::DotNew, "New since the last update", "Нове після останнього оновлення", "Новое после последнего обновления"},
    {Text::StripNoPack, "No texture pack", "Пакета текстур немає", "Пакета текстур нет"},
    {Text::DotHint, "Recommended for this computer: {1}", "Рекомендовано для цього комп'ютера: {1}",
     "Рекомендуется для этого компьютера: {1}"},
    {Text::DotOn, "on", "увімкнено", "включено"},
    {Text::DotOff, "off", "вимкнено", "выключено"},
    {Text::DotLegend, "● - not the value recommended for this computer (its hint says which is).",
     "● - не те значення, що рекомендоване для цього комп'ютера (підказка каже, яке).",
     "● - не то значение, что рекомендуется для этого компьютера (подсказка говорит, какое)."},
    {Text::ResetPage, "Reset the page to the recommended values", "Скинути сторінку до рекомендованих",
     "Сбросить страницу к рекомендуемым"},
    {Text::ResetPageAsk,
     "Set every row of this page to the value recommended for this computer? Nothing is written until Apply.",
     "Поставити кожному рядку цієї сторінки значення, рекомендоване для цього комп'ютера? Нічого не записується до "
     "«Застосувати».",
     "Поставить каждой строке этой страницы значение, рекомендуемое для этого компьютера? Ничего не записывается до "
     "«Применить»."},
    {Text::WhatsNewTitle,
     "What's new in uaRO Graphics {1}", "Що нового в uaRO Graphics {1}", "Что нового в uaRO Graphics {1}"},
    {Text::ConsoleGroupOff, "its group is off", "його група вимкнена", "его группа выключена"},  // 2.6.8
    {Text::FxNoGameFog, "No game fog under the open sky", "Без туману гри під відкритим небом",
     "Без тумана игры под открытым небом"},  // 2.6.10
    {Text::FxNoGameFogHint,
     "On the fields and in the towns the game's own fog is left out - the green or white veil that thickens as the "
     "camera goes back. The dungeons keep their dark fog.",
     "На полях і в містах власний туман гри не малюється — зелена чи біла пелена, що густішає, коли камера "
     "віддаляється. Підземелля зберігають свій темний туман.",
     "На полях и в городах собственный туман игры не рисуется — зелёная или белая пелена, которая густеет, когда "
     "камера отдаляется. Подземелья сохраняют свой тёмный туман."},
    {Text::FxNoServerNight, "No darkening by the server's night",
     "Без затемнення від ночі сервера",
     "Без затемнения от ночи сервера"},  // 2.6.13
    {Text::FxNoServerNightHint,
     "The night the server brings now and then does not darken the picture nor tint it blue - the time of day is the "
     "Clock's (\"From the server\" follows the server's night with the graphics' own). Off: the server's night as "
     "the game shows it.",
     "Ніч, яку час від часу вмикає сервер, не затемнює картинку й не фарбує її в синє — час доби задає «Годинник» "
     "(«Від сервера» йде за ніччю сервера, але своєю ніччю графіки). Вимкнено: ніч сервера, як її показує гра.",
     "Ночь, которую время от времени включает сервер, не затемняет картинку и не красит её в синий — время суток "
     "задают «Часы» («От сервера» идут за ночью сервера, но своей ночью графики). Выключено: ночь сервера, как её "
     "показывает игра."},
    {Text::FxClock, "Clock", "Годинник", "Часы"},  // 2.6.11
    {Text::FxClockHint,
     "The time of day for all the effects - the sun and its light, the night, the butterflies, birds and fireflies, "
     "the dew, the morning fog: the computer's clock, the server's day and night, or a moment of your choice that "
     "stays. Deep night: nearly black whatever the night's strength (but 0 %), the lights shining out of it.",
     "Час доби для всіх ефектів — сонце та його світло, ніч, метелики, птахи й світлячки, роса, ранковий туман: "
     "годинник комп'ютера, день і ніч сервера або обраний вами момент, що не змінюється. Глибока ніч — майже чорна "
     "за будь-якої сили ночі (крім 0 %), вогні світять із неї.",
     "Время суток для всех эффектов — солнце и его свет, ночь, бабочки, птицы и светлячки, роса, утренний туман: "
     "часы компьютера, день и ночь сервера или выбранный вами момент, который не меняется. Глубокая ночь — почти "
     "чёрная при любой силе ночи (кроме 0 %), огни светят из неё."},
    {Text::FxClockAuto, "Auto (the computer's clock)", "Авто (годинник комп'ютера)", "Авто (часы компьютера)"},
    {Text::FxClockServer,
     "From the server (its day and night)", "Від сервера (його день і ніч)", "От сервера (его день и ночь)"},
    {Text::FxClockDawn, "Dawn 6:30", "Світанок 6:30", "Рассвет 6:30"},
    {Text::FxClockMorning, "Morning 9:00", "Ранок 9:00", "Утро 9:00"},
    {Text::FxClockDay, "Day 13:00", "День 13:00", "День 13:00"},
    {Text::FxClockSunset, "Sunset 18:30", "Захід 18:30", "Закат 18:30"},
    {Text::FxClockNight, "Night 0:00", "Ніч 0:00", "Ночь 0:00"},
    {Text::FxClockDeepNight, "Deep night 3:00", "Глибока ніч 3:00", "Глубокая ночь 3:00"},  // 2.7.16
    {Text::FxMoonLight, "Moonlight", "Місячне світло", "Лунный свет"},  // 2.7.18
    {Text::FxMoonLightHint,
     "At night the moon is the night's light: the map stays bright under it, silver-blue and muted - plainly night, "
     "and the road, the grass and the monsters in sight. None by day.",
     "Уночі місяць — головне світло ночі: карта під ним світла, сріблясто-синя й приглушена — видно, що ніч, а "
     "дорогу, траву й монстрів добре видно. Удень його немає.",
     "Ночью луна — главный свет ночи: карта под ней светлая, серебристо-синяя и приглушённая — видно, что ночь, а "
     "дорогу, траву и монстров хорошо видно. Днём её нет."},
    {Text::FxMoonStrength, "Moon strength", "Сила місяця", "Сила луны"},
    {Text::FxMoonStrengthHint,
     "How bright the moonlight is.", "Наскільки яскраве місячне світло.", "Насколько ярок лунный свет."},
    {Text::FxFirefliesLight, "Fireflies light the ground", "Світлячки світять", "Светлячки светят"},
    {Text::FxFirefliesLightHint,
     "Each firefly in the grass at night lights the ground under it a little, greenish, pulsing with its glow.",
     "Кожен світлячок у траві вночі трохи освітлює землю під собою, зеленкувато, пульсуючи разом зі своїм вогником.",
     "Каждый светлячок в траве ночью немного освещает землю под собой, зеленовато, пульсируя вместе со своим "
     "огоньком."},
    {Text::FxMoonBeams, "Moon beams", "Місячні промені", "Лунные лучи"},  // 2.7.20
    {Text::FxMoonBeamsHint,
     "Broad slanting beams of moonlight lie on the ground of the fields and the towns, as through gaps in the clouds, "
     "and drift slowly; they keep to the map, not to the camera.",
     "Широкі косі промені місячного світла лягають на землю полів і міст, наче крізь розриви хмар, і повільно "
     "пливуть; вони тримаються карти, а не камери.",
     "Широкие косые лучи лунного света ложатся на землю полей и городов, будто сквозь разрывы облаков, и медленно "
     "плывут; они держатся карты, а не камеры."},
    {Text::FxLightOrbs, "Orbs of light", "Кулі світла", "Шары света"},  // 2.7.25
    {Text::FxLightOrbsHint,
     "Ruwach's blue orb circling the hero a small bright ball at the head of its blue trail, as a comet's.",
     "Блакитна куля Ruwach, що кружляє довкола героя, — маленька яскрава кулька на початку свого блакитного шлейфу, "
     "як у комети.",
     "Голубой шар Ruwach, кружащий вокруг героя, — маленький яркий шарик в начале своего голубого шлейфа, как у "
     "кометы."},
    {Text::FxMapGlowOrbs, "Orbs in the air", "Кулі вогників у повітрі", "Шары огоньков в воздухе"},  // 3.0.7-beta.1
    {Text::FxMapGlowOrbsHint,
     "The hidden lights' balls of light floating in the air. Off: only the light they throw on the ground, the walls "
     "and the characters.",
     "Кулі світла, що висять у повітрі над прихованими вогниками. Вимкнено — лише світло, яке вони кидають на землю, "
     "стіни й персонажів.",
     "Шары света, висящие в воздухе над скрытыми огоньками. Выключено — только свет, который они бросают на землю, "
     "стены и персонажей."},
    {Text::FxFireflyDots, "Fireflies in the air", "Світлячки в повітрі", "Светлячки в воздухе"},  // 3.0.7-beta.3
    {Text::FxFireflyDotsHint,
     "The fireflies themselves - glowing dots drifting and flashing in the air. Off: only the light they throw, the "
     "game's own fireflies left out too.",
     "Самі світлячки — вогники, що пливуть і спалахують у повітрі. Вимкнено — лише світло від них, світлячків гри "
     "теж не видно.",
     "Сами светлячки — огоньки, что плывут и вспыхивают в воздухе. Выключено — только свет от них, светлячков игры "
     "тоже не видно."},
    {Text::FxShaftsLamps, "At lamps and lights", "Біля ламп і вогників", "У ламп и огоньков"},  // 3.0.7-beta.5
    {Text::FxShaftsLampsHint,
     "The air glows around the maps' lamps, hidden lights and fireflies too - balls of light in the air, often where "
     "no lamp is seen.",
     "Повітря світиться ще й навколо ламп карт, прихованих вогників і світлячків — кулі світла в повітрі, часто там, "
     "де лампи не видно.",
     "Воздух светится ещё и вокруг ламп карт, скрытых огоньков и светлячков — шары света в воздухе, часто там, где "
     "лампы не видно."},
    {Text::FxPortalNew, "New portal", "Новий портал", "Новый портал"},  // 2.7.48
    {Text::FxPortalNewHint,
     "The warps drawn anew at full resolution as the game's own: the white band of each ring and its crown of light "
     "spikes rising and turning blue, sharp and smooth, gently breathing; the haze on the floor without its steps. "
     "Off: the game's own portal.",
     "Варпи намальовані заново в повній роздільності, як у гри: білий пояс кожного кільця й корона світлових "
     "променів, що здіймаються й синіють, чіткі й плавні, ледь дихають; серпанок на підлозі без сходинок. "
     "Вимкнено — портал гри.",
     "Варпы нарисованы заново в полном разрешении, как у игры: белый пояс каждого кольца и корона световых лучей, "
     "что поднимаются и синеют, чёткие и плавные, чуть дышат; дымка на полу без ступенек. Выключено — портал игры."},
    {Text::FxNightFireflies, "Fireflies in the grass at night", "Світлячки в траві вночі", "Светлячки в траве ночью"},
    {Text::FxNightFirefliesHint,
     "From dusk to dawn fireflies drift low over the grass where butterflies fly by day, each lit a while and dark a "
     "while (Direct3D 12).",
     "Від сутінків до світанку над травою, де вдень літають метелики, низько пливуть світлячки — то світяться, то "
     "гаснуть (Direct3D 12).",
     "От сумерек до рассвета над травой, где днём летают бабочки, низко плывут светлячки — то светятся, то гаснут "
     "(Direct3D 12)."},
    {Text::FxSeason, "Season", "Пора року", "Время года"},  // 2.6.22
    {Text::FxSeasonHint,
     "The season for all the effects that go by the year - winter's snow on the roofs, the rocks and the fields, "
     "autumn's yellow and red trees and dry grass, spring's greener ground: the computer's calendar, or a season of "
     "your choice that stays.",
     "Пора року для всіх ефектів, що залежать від неї, — зимовий сніг на дахах, скелях і полях, осінні жовті й "
     "червоні дерева та суха трава, весняна зеленіша земля: календар комп'ютера або обрана вами пора року, що не "
     "змінюється.",
     "Время года для всех эффектов, которые от него зависят, — зимний снег на крышах, скалах и полях, осенние жёлтые "
     "и красные деревья и сухая трава, весенняя более зелёная земля: календарь компьютера или выбранное вами время "
     "года, которое не меняется."},
    {Text::FxSeasonAuto, "Auto (the computer's calendar)", "Авто (календар комп'ютера)", "Авто (календарь компьютера)"},
    {Text::FxSeasonSpring, "Spring", "Весна", "Весна"},
    {Text::FxSeasonSummer, "Summer", "Літо", "Лето"},
    {Text::FxSeasonAutumn, "Autumn", "Осінь", "Осень"},
    {Text::FxSeasonWinter, "Winter", "Зима", "Зима"},
    {Text::FxWeatherKind, "Weather", "Погода", "Погода"},  // 2.6.22
    {Text::FxWeatherKindHint,
     "The weather for all the effects that go by it - the snow, rain, leaves or sand in the air (Precipitation), the "
     "wet ground, walls and roofs, the rain's splashes and the storms: the map's own, clear skies everywhere, or a "
     "weather of your choice under every open sky (never indoors).",
     "Погода для всіх ефектів, що залежать від неї, — сніг, дощ, листя чи пісок у повітрі («Опади»), мокра земля, "
     "стіни й дахи, бризки дощу та грози: своя погода карти, ясне небо всюди або обрана вами погода під кожним "
     "відкритим небом (ніколи в приміщеннях).",
     "Погода для всех эффектов, которые от неё зависят, — снег, дождь, листья или песок в воздухе («Осадки»), мокрая "
     "земля, стены и крыши, брызги дождя и грозы: своя погода карты, ясное небо везде или выбранная вами погода под "
     "каждым открытым небом (никогда в помещениях)."},
    {Text::FxWeatherKindAuto, "Auto (the map's own)", "Авто (своя погода карти)", "Авто (своя погода карты)"},
    {Text::FxWeatherKindClear, "Clear", "Ясно", "Ясно"},
    {Text::FxWeatherKindRain, "Rain", "Дощ", "Дождь"},
    {Text::FxWeatherKindSnow, "Snow", "Сніг", "Снег"},
    {Text::FxWeatherKindLeaves, "Falling leaves", "Листопад", "Листопад"},
    {Text::FxWeatherKindSand, "Sand in the wind", "Піщаний вітер", "Песчаный ветер"},
    // 2.6.22: the groups by what the eye is on, the console's search
    {Text::FxGroupCreatures, "Creatures", "Живність", "Живность"},
    {Text::FxGroupCreaturesHint, "Butterflies and dragonflies, birds, fireflies in the grass at night.",
     "Метелики й бабки, птахи, світлячки в траві вночі.",
     "Бабочки и стрекозы, птицы, светлячки в траве ночью."},
    {Text::FxGroupCaves, "Caves and dungeons", "Печери й підземелля", "Пещеры и подземелья"},
    {Text::FxGroupCavesHint, "Water running down the walls, drips from the ceiling, dust in the beams of light.",
     "Вода, що стікає стінами, краплі зі стелі, пил у променях світла.",
     "Вода, стекающая по стенам, капли с потолка, пыль в лучах света."},
    {Text::FxGroupSky, "Air", "Повітря", "Воздух"},  // 2.7.17: the sky past the edge gone
    {Text::FxGroupSkyHint, "Pollen and motes in the air, one wind for everything.",
     "Пилок і порошинки в повітрі, один вітер для всього.",
     "Пыльца и пылинки в воздухе, один ветер для всего."},
    {Text::FxGroupScreen, "Whole screen", "Весь екран", "Весь экран"},
    {Text::FxGroupScreenHint,
     "The colours of the whole picture: colour grading, each region's colours, the glow of bright places.",
     "Кольори всієї картинки: корекція кольору, кольори кожного регіону, сяйво яскравих місць.",
     "Цвета всей картинки: цветокоррекция, цвета каждого региона, свечение ярких мест."},
    {Text::ConsoleThisMap, "Only what this map has", "Лише те, що є на цій карті", "Только то, что есть на этой карте"},
    {Text::ConsoleSearch, "Search: {1}", "Пошук: {1}", "Поиск: {1}"},
    {Text::ConsoleNothing, "Nothing found - Backspace erases, Esc clears",
     "Нічого не знайдено — Backspace стирає, Esc очищує",
     "Ничего не найдено — Backspace стирает, Esc сбрасывает"},
    {Text::DebugSeams, "Ground seams view", "Стики землі: перегляд", "Стыки земли: просмотр"},  // 2.7.8
    {Text::DebugSeamsHint,
     "To find what blinks: the strips of Grass and road seams painted - their own band cyan, the neighbour's strip "
     "magenta; over everything - without the depth test. Off: the picture as usual.",
     "Щоб знайти, що блимає: смужки «Стиків трави й доріг» зафарбовані — свій край клітинки блакитним, смужка сусіда "
     "пурпуровим; «поверх усього» — без перевірки глибини. Вимкнено — картинка як звичайно.",
     "Чтобы найти, что мигает: полоски «Стыков травы и дорог» закрашены — свой край клетки голубым, полоска соседа "
     "пурпурным; «поверх всего» — без проверки глубины. Выключено — картинка как обычно."},
    {Text::DebugSeamsOff, "Off", "Вимк", "Выкл"},
    {Text::DebugSeamsColour, "Coloured", "Кольором", "Цветом"},
    {Text::DebugSeamsOnTop, "Coloured, over everything", "Кольором поверх усього", "Цветом поверх всего"},
    {Text::DebugWeakCard, "Take the card as weak", "Вважати відеокарту слабкою", "Считать видеокарту слабой"},
    {Text::DebugWeakCardHint,
     "To try how a weak card is judged by its speed: the card counts as one with little memory of its own - «Auto» "
     "starts with the weak set, the grass thinned; after a minute of room on a map the medium set on trial, kept after "
     "three (uaro-graphics\\card-level.txt). Off: the card's own memory decides.",
     "Щоб перевірити, як слабку відеокарту оцінює її швидкість: карта вважається такою, що має мало своєї пам'яті, — "
     "«Авто» починає зі слабкого набору, трава проріджена; після хвилини із запасом на карті — середній набір на "
     "пробу, після трьох він лишається (uaro-graphics\\card-level.txt). Вимкнено — вирішує пам'ять самої карти.",
     "Чтобы проверить, как слабую видеокарту оценивает её скорость: карта считается такой, у которой мало своей "
     "памяти, — «Авто» начинает со слабого набора, трава прорежена; после минуты с запасом на карте — средний набор на "
     "пробу, после трёх он остаётся (uaro-graphics\\card-level.txt). Выключено — решает память самой карты."},
    {Text::PadEnabled, "Gamepad (uaRO Pad)",
     "Геймпад (uaRO Pad)",
     "Геймпад (uaRO Pad)"},
    {Text::PadEnabledHint,
     "Xbox and PlayStation controllers play the game; with no controller connected nothing changes. Built into uaRO "
     "Graphics. From the next start of the game.",
     "Геймпади Xbox і PlayStation керують грою; без підключеного геймпада нічого не змінюється. Вбудований в uaRO "
     "Graphics. Діє з наступного запуску гри.",
     "Геймпады Xbox и PlayStation управляют игрой; без подключённого геймпада ничего не меняется. Встроен в uaRO "
     "Graphics. Действует со следующего запуска игры."},
    {Text::UpdateOut, "Update {1} is out",
     "Вийшло оновлення {1}",
     "Вышло обновление {1}"},
    {Text::UpdateRestartNote, "The game closes, updates and starts again - about 20 seconds.",
     "Гра закриється, оновиться й запуститься знову — це близько 20 секунд.",
     "Игра закроется, обновится и запустится снова — это около 20 секунд."},
    {Text::UpdatePlayWithout, "Play without the update",
     "Грати без оновлення",
     "Играть без обновления"},
    {Text::UpdateDo, "Update",
     "Оновити",
     "Обновить"},
    {Text::UpdatingTo, "Updating to {1}",
     "Оновлюємо до {1}",
     "Обновляем до {1}"},
    {Text::UpdateLoadedGame, "{1} of {2} MB downloaded. Then the game starts by itself.",
     "Завантажено {1} з {2} МБ. Потім гра запуститься сама.",
     "Скачано {1} из {2} МБ. Потом игра запустится сама."},
    {Text::UpdateLoadedApp, "{1} of {2} MB downloaded. Then the settings app opens again.",
     "Завантажено {1} з {2} МБ. Потім утиліта відкриється знову.",
     "Скачано {1} из {2} МБ. Потом утилита откроется снова."},
    {Text::UpdateWaitGame, "Waiting for the game to close...",
     "Чекаємо, поки гра закриється…",
     "Ждём, пока игра закроется…"},
    {Text::UpdateInstalling, "Installing...",
     "Встановлюємо…",
     "Устанавливаем…"},
    {Text::UpdateLoadingDlss, "NVIDIA DLSS: {1} of {2} MB downloaded",
     "NVIDIA DLSS: завантажено {1} з {2} МБ",
     "NVIDIA DLSS: скачано {1} из {2} МБ"},
    {Text::UpdateLoadingModels, "Game AI: {1} of {2} MB downloaded",
     "Game AI: завантажено {1} з {2} МБ",
     "Game AI: скачано {1} из {2} МБ"},
    {Text::UpdateNothingChanged, "It did not work: {1}. Nothing changed in the game's folder.",
     "Не вдалося: {1}. У теці гри нічого не змінилося.",
     "Не удалось: {1}. В папке игры ничего не изменилось."},
    {Text::ButtonCancel, "Cancel",
     "Скасувати",
     "Отмена"},
    {Text::RollbackButton, "Back to the previous version ({1})",
     "Повернути попередню версію ({1})",
     "Вернуть предыдущую версию ({1})"},
    {Text::RollbackAsk,
     "Go back to uaRO Graphics {1}? The settings app closes and opens again; the newer version is not offered until a "
     "later one is out.",
     "Повернути uaRO Graphics {1}? Утиліта закриється й відкриється знову; новішу версію не пропонуватиме, доки не "
     "вийде наступна.",
     "Вернуть uaRO Graphics {1}? Утилита закроется и откроется снова; более новую версию не будет предлагать, пока не "
     "выйдет следующая."},
    // 3.0.17-beta.1 (the concept «простой старт»: the rollback in sight, a recent version to choose)
    {Text::RollbackShort, "\xE2\x86\x90 Back to {1}", "\xE2\x86\x90 Повернути {1}", "\xE2\x86\x90 Вернуть {1}"},
    {Text::RollbackFailedStarts,
     "The game did not start twice since the update to {1}. Go back to {2}? The settings stay.",
     "Гра двічі не запустилася після оновлення до {1}. Повернути {2}? Налаштування лишаться.",
     "Игра дважды не запустилась после обновления до {1}. Вернуть {2}? Настройки останутся."},
    {Text::RollbackOtherHead, "Another version", "Інша версія", "Другая версия"},
    {Text::RollbackOtherNote,
     "The channel's recent versions, if a newer one does not work for you. The settings stay.",
     "Останні версії вашого каналу — якщо новіша у вас не працює. Налаштування лишаться.",
     "Последние версии вашего канала — если новая у вас не работает. Настройки останутся."},
    {Text::RollbackToButton, "Install {1}", "Встановити {1}", "Установить {1}"},
    {Text::RollbackToAsk,
     "Install uaRO Graphics {1} in place of {2}? The settings stay; {2} is not offered again until a newer version "
     "is out.",
     "Встановити uaRO Graphics {1} замість {2}? Налаштування лишаться; {2} більше не пропонуватиме, доки не вийде "
     "новіша.",
     "Установить uaRO Graphics {1} вместо {2}? Настройки останутся; {2} больше не будет предлагать, пока не выйдет "
     "более новая."},
    {Text::RollingBack, "Going back to {1}",
     "Повертаємо {1}",
     "Возвращаем {1}"},
    {Text::RepairButton, "Repair the files",
     "Відновити файли",
     "Восстановить файлы"},
    {Text::RepairAsk,
     "Download the newest uaRO Graphics again and replace the files that are damaged or missing? The settings stay; "
     "the settings app closes and opens again.",
     "Завантажити найновіший uaRO Graphics ще раз і замінити пошкоджені або відсутні файли? Налаштування залишаться; "
     "утиліта закриється й відкриється знову.",
     "Скачать новейший uaRO Graphics ещё раз и заменить повреждённые или отсутствующие файлы? Настройки останутся; "
     "утилита закроется и откроется снова."},
    {Text::Repairing, "Repairing the files",
     "Відновлюємо файли",
     "Восстанавливаем файлы"},
    {Text::UninstallButton, "Remove uaRO Graphics",
     "Видалити uaRO Graphics",
     "Удалить uaRO Graphics"},
    {Text::UninstallAsk,
     "Remove uaRO Graphics from {1}? The game draws by itself again, as before the install; dgVoodoo comes back if it "
     "was there.",
     "Видалити uaRO Graphics із теки {1}? Гра знову малюватиме сама, як до встановлення; dgVoodoo повернеться, якщо "
     "був.",
     "Удалить uaRO Graphics из папки {1}? Игра снова будет рисовать сама, как до установки; dgVoodoo вернётся, если "
     "был."},
    {Text::UninstallKeep, "Keep the settings and the texture pack",
     "Залишити налаштування й пакет текстур",
     "Оставить настройки и пакет текстур"},
    {Text::UninstallDo, "Remove",
     "Видалити",
     "Удалить"},
    {Text::Uninstalling, "Removing uaRO Graphics",
     "Видаляємо uaRO Graphics",
     "Удаляем uaRO Graphics"},
    {Text::UninstallDone, "uaRO Graphics is removed.",
     "uaRO Graphics видалено.",
     "uaRO Graphics удалён."},
    {Text::UninstallKept, "The settings and the texture pack stay in the uaro-graphics folder.",
     "Налаштування й пакет текстур залишилися в теці uaro-graphics.",
     "Настройки и пакет текстур остались в папке uaro-graphics."},
    {Text::InstallDone, "uaRO Graphics {1} is installed. Start the game as usual.",
     "uaRO Graphics {1} встановлено. Запускайте гру як завжди.",
     "uaRO Graphics {1} установлен. Запускайте игру как обычно."},
    {Text::DlssDownload, "Download DLSS (34 MB)",
     "Завантажити DLSS (34 МБ)",
     "Скачать DLSS (34 МБ)"},
    {Text::UninstallFailed, "Not everything could be removed: {1}",
     "Не все вдалося видалити: {1}",
     "Не всё удалось удалить: {1}"},
    {Text::SetupTitle, "Installing uaRO Graphics",
     "Встановлення uaRO Graphics",
     "Установка uaRO Graphics"},
    {Text::SetupChoose, "Choose the uaRO client folder (the one with uaRO.exe):",
     "Виберіть теку клієнта uaRO (де лежить uaRO.exe):",
     "Выберите папку клиента uaRO (где лежит uaRO.exe):"},
    {Text::SetupNoGame, "There is no uaRO.exe in that folder. Run the setup again and choose the uaRO client folder.",
     "У цій теці немає uaRO.exe. Запустіть встановлення ще раз і виберіть теку клієнта uaRO.",
     "В этой папке нет uaRO.exe. Запустите установку ещё раз и выберите папку клиента uaRO."},
    {Text::SetupDamaged, "This setup is damaged (no package inside it). Download it again.",
     "Це встановлення пошкоджене (у ньому немає пакета). Завантажте його ще раз.",
     "Эта установка повреждена (в ней нет пакета). Скачайте её ещё раз."},
    {Text::ViewPlay, "Play",
     "Гра",
     "Игра"},
    {Text::LauncherDirect, "Start the game at once",
     "Запускати гру одразу",
     "Запускать игру сразу"},
    {Text::LauncherDirectHint,
     "uaRO.exe and the game's old shortcut start the game itself, not the page «Play»; the question about an update "
     "stays.",
     "uaRO.exe і старий ярлик гри запускають саму гру, а не сторінку «Гра»; питання про оновлення лишається.",
     "uaRO.exe и старый ярлык игры запускают саму игру, а не страницу «Игра»; вопрос об обновлении остаётся."},
    {Text::PlayNews, "News",
     "Новини",
     "Новости"},
    {Text::PlayAsking, "Asking uaro.kiev.ua for the news...",
     "Запитую новини на uaro.kiev.ua…",
     "Спрашиваю новости у uaro.kiev.ua…"},
    {Text::PlayNoSite, "uaro.kiev.ua does not answer: no news and no online count now. «{1}» works.",
     "uaro.kiev.ua не відповідає: новин і онлайну зараз не видно. «{1}» працює.",
     "uaro.kiev.ua не отвечает: новостей и онлайна сейчас не видно. «{1}» работает."},
    {Text::PlayNoNews, "No news yet.",
     "Новин поки немає.",
     "Новостей пока нет."},
    {Text::PlayPinned, "pinned · {1}",
     "закріплено · {1}",
     "закреплено · {1}"},
    {Text::PlayMore, "More →",
     "Докладніше →",
     "Подробнее →"},
    {Text::PlayServer, "Server",
     "Сервер",
     "Сервер"},
    {Text::PlayOnline, "Up · online: {1}",
     "Працює · онлайн: {1}",
     "Работает · онлайн: {1}"},
    {Text::PlayUpdateLine, "uaRO Graphics {1} · {2} MB",
     "uaRO Graphics {1} · {2} МБ",
     "uaRO Graphics {1} · {2} МБ"},
    {Text::PlayUpdateNote, "After the update the game starts by itself.",
     "Після оновлення гра запуститься сама.",
     "После обновления игра запустится сама."},
    {Text::PlayButton, "Play",
     "Грати",
     "Играть"},
    {Text::PlayUpdateButton, "Update and play",
     "Оновити й грати",
     "Обновить и играть"},
    {Text::AutoTierIntegratedWeak,
     "Integrated graphics, weak: the super-light mode - no effects, 60 frames, the battery spared on a laptop.",
     "Вбудована графіка, слабка: надлегкий режим — без ефектів, 60 кадрів, на ноутбуці економія батареї.",
     "Встроенная графика, слабая: сверхлёгкий режим — без эффектов, 60 кадров, на ноутбуке экономия батареи."},
    {Text::AutoTierIntegratedStrong,
     "Integrated graphics, strong: low quality, weak effects, 2x smoothing, 45 frames kept.",
     "Вбудована графіка, сильна: низька якість, слабкі ефекти, згладжування 2×, тримати 45 кадрів.",
     "Встроенная графика, сильная: низкое качество, слабые эффекты, сглаживание 2×, держать 45 кадров."},
    {Text::RenderApiSystem, "Off - the game's own look",
     "Вимкнено — вигляд самої гри",
     "Выключено — вид самой игры"},
    {Text::RenderSafeMode, "Safe mode",
     "Безпечний режим",
     "Безопасный режим"},
    {Text::RenderSafeModeHint,
     "Direct3D 11 without effects, the neural network, DLSS and the 64-bit renderer - when the game does not start or "
     "crashes. It comes by itself after two failed starts in a row.",
     "Direct3D 11 без ефектів, нейромережі, DLSS і 64-бітного рендера — якщо гра не запускається або вилітає. "
     "Вмикається й сама після двох невдалих запусків поспіль.",
     "Direct3D 11 без эффектов, нейросети, DLSS и 64-битного рендера — если игра не запускается или вылетает. "
     "Включается и сама после двух неудачных запусков подряд."},
    {Text::AutoPicked, "Set up for {1}",
     "Підібрано для {1}",
     "Подобрано для {1}"},
    {Text::AutoPickedMore, "What was picked",
     "Як підібрано",
     "Как подобрано"},
    {Text::MachineChanged,
     "The graphics card or the monitor changed: {1}, {2}. Pick the settings for them again? Your present settings "
     "change.",
     "Відеокарта або монітор змінилися: {1}, {2}. Підібрати налаштування під них заново? Ваші теперішні налаштування "
     "зміняться.",
     "Видеокарта или монитор сменились: {1}, {2}. Подобрать настройки под них заново? Ваши нынешние настройки "
     "изменятся."},
    {Text::SafeModeAuto,  // 3.0.11-beta.59: or closed by an error in their first minutes
     "The game starts in the safe mode: its last starts did not get to the picture or closed by an error in their "
     "first minutes. Direct3D 12, the effects, the neural network, DLSS and the 64-bit renderer are off.",
     "Гра запускається в безпечному режимі: останні запуски не дійшли до картинки або закрилися з помилкою в перші "
     "хвилини. Вимкнено Direct3D 12, ефекти, нейромережу, DLSS і 64-бітний рендер.",
     "Игра запускается в безопасном режиме: последние запуски не дошли до картинки или закрылись с ошибкой в первые "
     "минуты. Выключены Direct3D 12, эффекты, нейросеть, DLSS и 64-битный рендер."},
    {Text::SafeModeAsked,
     "The safe mode is on: Direct3D 11 without effects, the neural network, DLSS and the 64-bit renderer.",
     "Безпечний режим увімкнено: Direct3D 11 без ефектів, нейромережі, DLSS і 64-бітного рендера.",
     "Безопасный режим включён: Direct3D 11 без эффектов, нейросети, DLSS и 64-битного рендера."},
    {Text::SafeModeOff, "Normal mode",
     "Звичайний режим",
     "Обычный режим"},
    {Text::TroubleHeading, "If the game does not start",
     "Якщо гра не запускається",
     "Если игра не запускается"},
    {Text::SwitchOff, "Switch uaRO Graphics off",
     "Вимкнути uaRO Graphics",
     "Выключить uaRO Graphics"},
    {Text::SwitchOn, "Switch uaRO Graphics on",
     "Увімкнути uaRO Graphics",
     "Включить uaRO Graphics"},
    {Text::SwitchOffNote, "The game then draws by itself, as without uaRO Graphics; the gamepad and the launcher stay.",
     "Тоді гра малює сама, як без uaRO Graphics; геймпад і лаунчер лишаються.",
     "Тогда игра рисует сама, как без uaRO Graphics; геймпад и лаунчер остаются."},
    {Text::SwitchedOff, "uaRO Graphics is switched off: the game draws by itself.",
     "uaRO Graphics вимкнено: гра малює сама.",
     "uaRO Graphics выключен: игра рисует сама."},
    {Text::StartRenderer, "Starting the renderer...",
     "Запускаємо рендер…",
     "Запускаем рендер…"},
    {Text::StartDlss, "Starting NVIDIA DLSS...",
     "Запускаємо NVIDIA DLSS…",
     "Запускаем NVIDIA DLSS…"},
    {Text::StartShadersLoad, "Loading shaders...",
     "Завантажуємо шейдери…",
     "Загружаем шейдеры…"},
    {Text::StartShadersBuild, "Compiling shaders: {1} of {2}",
     "Збираємо шейдери: {1} з {2}",
     "Собираем шейдеры: {1} из {2}"},
    {Text::StartAiTextures, "Starting AI textures...",
     "Запускаємо ШІ-текстури…",
     "Запускаем ИИ-текстуры…"},
    {Text::StartFirstHint, "First start of this version - the next ones skip this",
     "Перший запуск цієї версії — наступні його пропустять",
     "Первый запуск этой версии — следующие его пропустят"},
    {Text::ReportsButton, "Reports",
     "Звернення",
     "Обращения"},
    {Text::ReportsButtonUnread, "Reports ({1})",
     "Звернення ({1})",
     "Обращения ({1})"},
    {Text::ReportsTitle, "uaRO - Reports",
     "uaRO — Звернення",
     "uaRO — Обращения"},
    {Text::ReportsNew, "+ New report",
     "+ Нове звернення",
     "+ Новое обращение"},
    {Text::ReportsRefresh, "Refresh",
     "Оновити",
     "Обновить"},
    {Text::ReportsLoading, "Asking uaro.kiev.ua…",
     "Запит до uaro.kiev.ua…",
     "Запрос к uaro.kiev.ua…"},
    {Text::ReportsOffline, "No connection with uaro.kiev.ua: {1}",
     "Немає зв'язку з uaro.kiev.ua: {1}",
     "Нет связи с uaro.kiev.ua: {1}"},
    {Text::ReportsWaiting, "Waiting for the connection: {1} - they go by themselves.",
     "Чекають на зв'язок: {1} — підуть самі.",
     "Ждут связи: {1} — уйдут сами."},
    {Text::ReportsEmpty,
     "No reports yet. Something looks wrong in the game? Press «+ New report» here or Ctrl+Shift+F10 in the game - a "
     "screenshot comes with it.",
     "Звернень ще немає. Щось не так у грі? Натисніть «+ Нове звернення» тут або Ctrl+Shift+F10 у грі — знімок екрана "
     "додасться сам.",
     "Обращений ещё нет. Что-то не так в игре? Нажмите «+ Новое обращение» здесь или Ctrl+Shift+F10 в игре — снимок "
     "экрана добавится сам."},
    {Text::ReportsPick, "Pick a report on the left to see the answers.",
     "Виберіть звернення ліворуч, щоб побачити відповіді.",
     "Выберите обращение слева, чтобы увидеть ответы."},
    {Text::ReportStatusNew, "Sent",  // 3.0.9-beta.5: the player's word (the admin's stays «Нове»)
     "Надіслано",
     "Отправлено"},
    {Text::ReportStatusRead, "Read",
     "Прочитано",
     "Прочитано"},
    {Text::ReportStatusNeedInfo, "Needs information",
     "Потрібна інформація",
     "Нужна информация"},
    {Text::ReportStatusFixing, "Being fixed",
     "Виправляємо",
     "Исправляем"},
    {Text::ReportStatusFixed, "Fixed in {1}",
     "Виправлено в {1}",
     "Исправлено в {1}"},
    {Text::ReportStatusClosed, "Closed",
     "Закрито",
     "Закрыто"},
    {Text::ReportKindGraphics, "Graphics",
     "Графіка",
     "Графика"},
    {Text::ReportKindCrash, "Crashes or does not start",
     "Вильот або не запускається",
     "Вылет или не запускается"},
    {Text::ReportKindSlow, "Slow",
     "Гальмує",
     "Тормозит"},
    {Text::ReportKindOther, "Interface and other",  // 3.0.16-beta.26: windows, items, NPCs - not only «other»
     "Інтерфейс та інше",
     "Интерфейс и другое"},
    {Text::ReportTitleCrash, "The game crashed",
     "Гра завершилась аварійно",
     "Игра завершилась аварийно"},
    {Text::ReportTitleStartFailed, "The game did not start",
     "Гра не запустилась",
     "Игра не запустилась"},
    {Text::ReportTitleGpuLost, "The graphics card stopped answering",
     "Відеокарта перестала відповідати",
     "Видеокарта перестала отвечать"},
    {Text::ReportTitleHang, "The game froze",
     "Гра зависла",
     "Игра зависла"},
    {Text::ReportTitleNone, "No description",
     "Без опису",
     "Без описания"},
    {Text::ReportYou, "You",
     "Ви",
     "Вы"},
    {Text::ReportDev, "Developer",
     "Розробник",
     "Разработчик"},
    {Text::ReportEventStatus, "Status: {1}",
     "Статус: {1}",
     "Статус: {1}"},
    {Text::ReportAutoSent, "Sent by itself after the game ended badly: {1}.",
     "Надіслано автоматично після збою гри: {1}.",
     "Отправлено автоматически после сбоя игры: {1}."},
    {Text::ReportReplyLabel, "Your answer",
     "Ваша відповідь",
     "Ваш ответ"},
    {Text::ReportAddShot, "Add a screenshot…",
     "Додати знімок…",
     "Добавить снимок…"},
    {Text::ReportAttached, "Screenshots added: {1}",
     "Додано знімків: {1}",
     "Добавлено снимков: {1}"},
    {Text::ReportSendButton, "Send",
     "Надіслати",
     "Отправить"},
    {Text::ReportCancel, "Cancel",
     "Скасувати",
     "Отмена"},
    {Text::ReportConfirmNote,
     "The developers fixed this in version {1}, which you have. Check in the game: gone - «Confirm», not - «Still "
     "there».",
     "Розробники виправили це у версії {1}, вона у вас уже стоїть. Перевірте в грі: зникло — «Підтверджую», ні — «Ще "
     "є».",
     "Разработчики исправили это в версии {1}, она у вас уже установлена. Проверьте в игре: исчезло — «Подтверждаю», "
     "нет — «Ещё есть»."},
    {Text::ReportFixedWait, "Fixed in version {1} - update uaRO, then check.",
     "Виправлено у версії {1} — оновіть uaRO, і тоді перевірте.",
     "Исправлено в версии {1} — обновите uaRO, затем проверьте."},
    {Text::ReportConfirm, "Confirm",
     "Підтверджую",
     "Подтверждаю"},
    {Text::ReportStillThere, "Still there",
     "Ще є",
     "Ещё есть"},
    {Text::ReportSentOk, "Sent.",
     "Надіслано.",
     "Отправлено."},
    {Text::ReportNotSent, "Not sent: {1}",
     "Не надіслано: {1}",
     "Не отправлено: {1}"},
    {Text::ReportNeedWords, "Write a few words or add a screenshot.",
     "Напишіть кілька слів або додайте знімок.",
     "Напишите несколько слов или добавьте снимок."},
    {Text::ReportShotFailed, "Cannot read the picture: {1}",
     "Не вдалося прочитати зображення: {1}",
     "Не удалось прочитать изображение: {1}"},
    {Text::ReportNewHeading, "New report",
     "Нове звернення",
     "Новое обращение"},
    {Text::ReportNoGameShots,
     "No game screenshots of the last week (Print Screen in the game saves them to its ScreenShot folder).",
     "Знімків гри за тиждень немає (Print Screen у грі зберігає їх у теку ScreenShot).",
     "Снимков игры за неделю нет (Print Screen в игре сохраняет их в папку ScreenShot)."},
    {Text::ReportAddFile, "Add a file…",
     "Додати файл…",
     "Добавить файл…"},
    {Text::ReportPasteNone, "The clipboard holds no picture.",
     "У буфері обміну немає зображення.",
     "В буфере обмена нет изображения."},
    {Text::ReportTooMany, "6 screenshots at most.",
     "Не більше 6 знімків.",
     "Не больше 6 снимков."},
    {Text::ReportMapLine, "Map: {1}",
     "Карта: {1}",
     "Карта: {1}"},
    {Text::ReportNewSent, "Report {1} is sent - the answer appears here.",
     "Звернення {1} надіслано — відповідь з'явиться тут.",
     "Обращение {1} отправлено — ответ появится здесь."},
    {Text::ReportNewRefused, "uaro.kiev.ua did not take the report: {1}",
     "uaro.kiev.ua не прийняв звернення: {1}",
     "uaro.kiev.ua не принял обращение: {1}"},
    {Text::ReportPictures, "Pictures",
     "Зображення",
     "Изображения"},
    {Text::ReportDiscard, "The report is not sent. Close without it?",
     "Звернення не надіслано. Закрити без нього?",
     "Обращение не отправлено. Закрыть без него?"},
    {Text::ReportsOpen, "My reports…",
     "Мої звернення…",
     "Мои обращения…"},
    {Text::StartReportHint,
     "Something wrong with the picture? Ctrl+Shift+F10 in the game - a screenshot and a report to the developers",
     "Щось не так з картинкою? Ctrl+Shift+F10 у грі — знімок і звернення до розробників",
     "Что-то не так с картинкой? Ctrl+Shift+F10 в игре — снимок и обращение к разработчикам"},
    {Text::PlayPatchLine, "New game files: {1} ({2} MB in all)",
     "Нові файли гри: {1} (разом {2} МБ)",
     "Новые файлы игры: {1} (всего {2} МБ)"},
    {Text::PlayPatchNote,
     "They are downloaded and put into the game's folder before it starts - DATA.INI stays as it is.",
     "Їх буде завантажено й покладено в теку гри перед запуском — DATA.INI не змінюється.",
     "Они будут скачаны и положены в папку игры перед запуском — DATA.INI не меняется."},
    {Text::PatchApplying, "Updating the game's files",
     "Оновлюємо файли гри",
     "Обновляем файлы игры"},
    {Text::PatchFailed, "The game's files were not updated: {1}. The game starts as it is.",
     "Файли гри не оновлено: {1}. Гра запуститься як є.",
     "Файлы игры не обновлены: {1}. Игра запустится как есть."},
    {Text::PackFromClient, "The HD texture pack comes with the game's full client on uaro.kiev.ua.",
     "Пакет HD-текстур іде разом із повним клієнтом гри на uaro.kiev.ua.",
     "Пакет HD-текстур идёт вместе с полным клиентом игры на uaro.kiev.ua."},
    {Text::CompareOffNote, "uaRO Graphics is off (Pause / Scroll Lock) - press it again to turn it on",
     "uaRO Graphics вимкнено (Pause / Scroll Lock) — натисніть ще раз, щоб увімкнути",
     "uaRO Graphics выключено (Pause / Scroll Lock) — нажмите ещё раз, чтобы включить"},
    {Text::ReportAutoAsk,
     "Send the developers a report by itself when the game crashes?\n\nAfter a game in which the renderer would not "
     "start, the graphics card was lost or the game closed without closing the renderer, a report goes to "
     "uaro.kiev.ua: this computer's graphics card, Windows, monitors, uaro-graphics.ini and the logs of uaRO Graphics "
     "- no login, no character; the Windows user name is hidden. At most one in 6 hours.\n\nIt shows in the Reports "
     "window, where you can add what you were doing. You can change your mind on the Diagnostics page.",
     "Надсилати розробникам звіт самостійно, коли гра вилітає?\n\nПісля гри, у якій рендер не запустився, відеокарта "
     "відпала або гра закрилася, не закривши рендер, звіт іде на uaro.kiev.ua: відеокарта, Windows, монітори цього "
     "комп'ютера, uaro-graphics.ini та журнали uaRO Graphics — без логіна й персонажа; ім'я користувача Windows "
     "приховано. Не частіше одного на 6 годин.\n\nВін з'явиться у вікні «Звернення», де можна дописати, що ви робили. "
     "Передумати можна на сторінці «Діагностика».",
     "Отправлять разработчикам отчёт самостоятельно, когда игра вылетает?\n\nПосле игры, в которой рендер не "
     "запустился, видеокарта отвалилась или игра закрылась, не закрыв рендер, отчёт уходит на uaro.kiev.ua: "
     "видеокарта, Windows, мониторы этого компьютера, uaro-graphics.ini и логи uaRO Graphics — без логина и персонажа; "
     "имя пользователя Windows скрыто. Не чаще одного в 6 часов.\n\nОн появится в окне «Обращения», где можно "
     "дописать, что вы делали. Передумать можно на странице «Диагностика»."},
    {Text::UpdateChannel, "Update channel",
     "Канал оновлень",
     "Канал обновлений"},
    {Text::UpdateChannelRelease, "Release",
     "Реліз",
     "Релиз"},
    {Text::UpdateChannelBeta, "Beta (testing)",
     "Бета (тестування)",
     "Бета (тестирование)"},
    {Text::UpdateChannelHint,
     "Release: the versions every player gets. Beta: the testers' builds, before everyone - they may have bugs, tell "
     "about them in the Reports window. Back on Release with a beta, the settings app offers the release back.",
     "Реліз — версії, які отримують усі гравці. Бета — збірки для тестувальників, раніше за всіх; у них можуть бути "
     "помилки — пишіть про них у «Звернення». Після повернення на «Реліз» із бетою утиліта запропонує повернутися на "
     "реліз.",
     "Релиз — версии, которые получают все игроки. Бета — сборки для тестировщиков, раньше всех; в них могут быть "
     "ошибки — пишите о них в «Обращения». После возвращения на «Релиз» с бетой утилита предложит вернуться на "
     "релиз."},
    {Text::ServiceClientLibs, "The game's libraries",
     "Бібліотеки гри",
     "Библиотеки игры"},
    {Text::ClientLibsNote,
     "Our own in place of the game's libraries of the 2000s; one turned off puts the game's own back. It changes once "
     "the game is closed.",
     "Свої замість старих бібліотек гри 2000-х; вимкнена повертає оригінал гри. Діє, коли гра закрита.",
     "Свои вместо старых библиотек игры 2000-х; выключенная возвращает оригинал игры. Действует, когда игра закрыта."},
    {Text::ClientCps, "Files' compression: our zlib (cps.dll)",
     "Стиснення файлів гри — свій zlib (cps.dll)",
     "Сжатие файлов игры — свой zlib (cps.dll)"},
    {Text::ClientCpsHint, "The game reads its archives through it. Off: the game's cps.dll of the 2000s.",
     "Через неї гра читає свої архіви. Вимкнено — старий cps.dll гри.",
     "Через неё игра читает свои архивы. Выключено — старый cps.dll игры."},
    {Text::ClientBink, "Bink video: none (binkw32.dll)",
     "Відео Bink — без нього (binkw32.dll)",
     "Видео Bink — без него (binkw32.dll)"},
    {Text::ClientBinkHint,
     "The game has no Bink video to show (its opening is not in the client): ours holds none of RAD's code. Off: "
     "RAD's binkw32.dll.",
     "Відео Bink гра не показує (вступного ролика в клієнті немає): наша заміна без коду RAD. Вимкнено — binkw32.dll "
     "від RAD.",
     "Видео Bink игра не показывает (вступительного ролика в клиенте нет): наша замена без кода RAD. Выключено — "
     "binkw32.dll от RAD."},
    {Text::ClientIjl, "JPEG pictures: ours (ijl15.dll)",
     "Картинки JPEG — свої (ijl15.dll)",
     "Картинки JPEG — свои (ijl15.dll)"},
    {Text::ClientIjlHint,
     "The game's JPEG pictures (the loading screens) are read through it and its Print Screen shots written. Off: "
     "Intel JPEG Library 1.5 of 2000.",
     "Через неї гра читає картинки JPEG (екрани завантаження) і записує знімки Print Screen. Вимкнено — Intel JPEG "
     "Library 1.5 2000 року.",
     "Через неё игра читает картинки JPEG (экраны загрузки) и записывает снимки Print Screen. Выключено — Intel JPEG "
     "Library 1.5 2000 года."},
    {Text::ClientMss, "Game sound: ours (mss32.dll)",
     "Звук гри — свій (mss32.dll)",
     "Звук игры — свой (mss32.dll)"},
    {Text::ClientMssHint,
     "The game's effects and music play through it, over Windows' XAudio2 (Windows 10 and later; on older Windows the "
     "game's own stays). Off: RAD's Miles Sound System 6.",
     "Через неї звучать ефекти й музика гри, через XAudio2 Windows (Windows 10 і новіші; на старіших лишається "
     "оригінал). Вимкнено — Miles Sound System 6 від RAD.",
     "Через неё звучат эффекты и музыка игры, через XAudio2 Windows (Windows 10 и новее; на более старых остаётся "
     "оригинал). Выключено — Miles Sound System 6 от RAD."},
    {Text::ClientRoext, "Window, mouse and keys: ROExt's work in the driver (dinput8.dll)",
     "Вікно, миша й клавіші — робота ROExt у драйвері (dinput8.dll)",
     "Окно, мышь и клавиши — работа ROExt в драйвере (dinput8.dll)"},
    {Text::ClientRoextHint,
     "The driver does what ROExt did (its settings on «Game and chat»), its dinput8.dll is no longer needed. Off: "
     "ROExt's dinput8.dll and its dinput8.ini.",
     "Драйвер робить те, що робив ROExt (його налаштування — на «Гра й чат»), його dinput8.dll більше не потрібна. "
     "Вимкнено — dinput8.dll ROExt і його dinput8.ini.",
     "Драйвер делает то, что делал ROExt (его настройки — на «Игра и чат»), его dinput8.dll больше не нужна. "
     "Выключено — dinput8.dll ROExt и его dinput8.ini."},
    {Text::RoextHeading, "The game's window, mouse and keys (ROExt)",
     "Вікно, миша й клавіші гри (ROExt)",
     "Окно, мышь и клавиши игры (ROExt)"},
    {Text::RoextNote,
     "What ROExt did, done by the driver; its dinput8.ini's settings were brought here. They change at the game's "
     "next start.",
     "Те, що робив ROExt, тепер робить драйвер; налаштування з його dinput8.ini перенесено сюди. Діють із наступного "
     "запуску гри.",
     "То, что делал ROExt, теперь делает драйвер; настройки из его dinput8.ini перенесены сюда. Действуют со "
     "следующего запуска игры."},
    {Text::RoextLock, "A borderless window locked in place",
     "Вікно без рамки, закріплене на місці",
     "Окно без рамки, закреплённое на месте"},
    {Text::RoextLockHint,
     "The game's window without a border, at its place: over the whole screen but the taskbar, or of the size the "
     "«Screen» page set.",
     "Вікно гри без рамки на своєму місці: на весь екран, крім панелі задач, або розміру, заданого на сторінці "
     "«Екран».",
     "Окно игры без рамки на своём месте: на весь экран, кроме панели задач, или размера, заданного на странице "
     "«Экран»."},
    {Text::RoextOnTop, "The game's window above the others",
     "Вікно гри поверх інших",
     "Окно игры поверх других"},
    {Text::RoextOnTopHint, "It stays above every other window, also when another one is active.",
     "Воно лишається над усіма іншими вікнами, навіть коли активне інше.",
     "Оно остаётся над всеми другими окнами, даже когда активно другое."},
    {Text::RoextMouse, "Mouse buttons 3-5 as F24, F23, F22",
     "Кнопки миші 3–5 як F24, F23, F22",
     "Кнопки мыши 3–5 как F24, F23, F22"},
    {Text::RoextMouseHint,
     "The middle button and the two side ones become keys F24, F23 and F22: skills can be put on them in the game's "
     "keys.",
     "Середня й дві бокові кнопки стають клавішами F24, F23 і F22: на них можна поставити вміння в клавішах гри.",
     "Средняя и две боковые кнопки становятся клавишами F24, F23 и F22: на них можно поставить умения в клавишах "
     "игры."},
    {Text::RoextAltF4, "Alt+F4 as a key of the game, Alt+Print Screen closes it",
     "Alt+F4 — клавіша гри, закриває Alt+Print Screen",
     "Alt+F4 — клавиша игры, закрывает Alt+Print Screen"},
    {Text::RoextAltF4Hint, "Alt+F4 then works in the game as F4, and the game is closed with Alt+Print Screen.",
     "Тоді Alt+F4 у грі працює як F4, а закривають гру через Alt+Print Screen.",
     "Тогда Alt+F4 в игре работает как F4, а закрывают игру через Alt+Print Screen."},
    {Text::RoextMinimize, "Ctrl+W minimizes the game",
     "Ctrl+W згортає гру",
     "Ctrl+W сворачивает игру"},
    {Text::RoextMinimizeHint, "The game's window goes to the taskbar on Ctrl+W.",
     "На Ctrl+W вікно гри згортається на панель задач.",
     "По Ctrl+W окно игры сворачивается на панель задач."},
    {Text::RoextFreeCpu, "Free the processor behind other windows",
     "Звільняти процесор за іншими вікнами",
     "Освобождать процессор за другими окнами"},
    {Text::RoextFreeCpuHint,
     "While another window is active and the game has nothing to do, it waits this long for input (0 - never). Scroll "
     "Lock never reaches the game, as with ROExt.",
     "Поки активне інше вікно і грі нічого робити, вона стільки чекає на введення (0 — ніколи). Scroll Lock до гри, як "
     "і з ROExt, не доходить.",
     "Пока активно другое окно и игре нечего делать, она столько ждёт ввода (0 — никогда). Scroll Lock до игры, как и "
     "с ROExt, не доходит."},
    {Text::RoextCodePage, "The game's text code page",
     "Кодова сторінка тексту гри",
     "Кодовая страница текста игры"},
    {Text::RoextCodePageHint,
     "-1: as Windows has it. 1251: Cyrillic on a Windows of another language; 65001: UTF-8. Leave it unless you know "
     "why.",
     "-1 — як у Windows. 1251 — кирилиця на Windows іншої мови; 65001 — UTF-8. Не змінюйте без потреби.",
     "-1 — как в Windows. 1251 — кириллица на Windows другого языка; 65001 — UTF-8. Не меняйте без нужды."},
    {Text::RoextWindowX, "The window's place: X", "Місце вікна: X", "Место окна: X"},
    {Text::RoextWindowY, "The window's place: Y", "Місце вікна: Y", "Место окна: Y"},
    {Text::RoextWindowPlaceHint,
     "Where the game's window of the «Screen» page's size opens (its top left corner), as ROExt had it.",
     "Де відкривається вікно гри розміру зі сторінки «Екран» (його лівий верхній кут), як було в ROExt.",
     "Где открывается окно игры размера со страницы «Экран» (его левый верхний угол), как было в ROExt."},
    {Text::MainKeys, "Keys in the game", "Клавіші в грі", "Клавиши в игре"},
    {Text::MainKeysList, "Ctrl+~ (the key left of 1) - the effects console: effects on and off right in the game\n"
     "Ctrl+Shift+F10 - «New report» to the developers, with a screenshot\n"
     "Ctrl+Shift+F11 - the frame counter\n"
     "Ctrl+Shift+F12 - a screenshot at full size (the ScreenShot folder)\n"
     "Pause or Scroll Lock - every improvement off, and on again: to compare with the original look\n"
     "Arrows (the chat line empty) - walk; two at once - diagonally; Ctrl+arrows - the camera\n"
     "Ctrl+Home - the camera as at the game's start\n"
     "Space (the chat line empty) - attack, else pick up, else talk to the NPC near\n"
     "` - the walk mode: W A S D walk, Tab the target, Space attack, F pick up, R talk, C sit, Q / E the camera, "
     "1-9 the bar, Enter the chat\n"
     "Arrows in a window - move its frame (after the first arrow); Enter presses, Esc closes, PgUp / PgDn the tabs",
     "Ctrl+~ (клавіша ліворуч від 1) — консоль ефектів: вмикати й вимикати ефекти просто в грі\n"
     "Ctrl+Shift+F10 — «Нове звернення» до розробників зі знімком екрана\n"
     "Ctrl+Shift+F11 — лічильник кадрів\n"
     "Ctrl+Shift+F12 — знімок екрана в повному розмірі (тека ScreenShot)\n"
     "Pause або Scroll Lock — усі покращення вимкнути й увімкнути знову: порівняти з оригінальним виглядом\n"
     "Стрілки (рядок чату порожній) — ходьба; дві разом — навскіс; Ctrl+стрілки — камера\n"
     "Ctrl+Home — камера як на початку гри\n"
     "Пробіл (рядок чату порожній) — атака, інакше підібрати, інакше заговорити з NPC поруч\n"
     "` — режим ходьби: W A S D — ходьба, Tab — ціль, пробіл — атака, F — підібрати, R — NPC, C — сісти, "
     "Q / E — камера, 1–9 — панель, Enter — чат\n"
     "Стрілки у вікні — ведуть його рамку (після першої стрілки); Enter натискає, Esc закриває, PgUp / PgDn — "
     "вкладки",
     "Ctrl+~ (клавиша слева от 1) — консоль эффектов: включать и выключать эффекты прямо в игре\n"
     "Ctrl+Shift+F10 — «Новое обращение» к разработчикам со снимком экрана\n"
     "Ctrl+Shift+F11 — счётчик кадров\n"
     "Ctrl+Shift+F12 — снимок экрана в полном размере (папка ScreenShot)\n"
     "Pause или Scroll Lock — все улучшения выключить и включить снова: сравнить с оригинальным видом\n"
     "Стрелки (строка чата пуста) — ходьба; две вместе — по диагонали; Ctrl+стрелки — камера\n"
     "Ctrl+Home — камера как в начале игры\n"
     "Пробел (строка чата пуста) — атака, иначе подобрать, иначе заговорить с NPC рядом\n"
     "` — режим ходьбы: W A S D — ходьба, Tab — цель, пробел — атака, F — подобрать, R — NPC, C — сесть, "
     "Q / E — камера, 1–9 — панель, Enter — чат\n"
     "Стрелки в окне — ведут его рамку (после первой стрелки); Enter нажимает, Esc закрывает, PgUp / PgDn — "
     "вкладки"},
    {Text::PlayKeysLine,
     "In the game: Ctrl+~ the effects console, Ctrl+Shift+F10 a report, Pause the original look. Every key is on "
     "«Main».",
     "У грі: Ctrl+~ — консоль ефектів, Ctrl+Shift+F10 — звернення, Pause — оригінальний вигляд. Усі клавіші — на "
     "«Головне».",
     "В игре: Ctrl+~ — консоль эффектов, Ctrl+Shift+F10 — обращение, Pause — оригинальный вид. Все клавиши — на "
     "«Главное»."},
    {Text::ServiceUpdates, "The client's graphics updates", "Оновлення графіки клієнта", "Обновления графики клиента"},
    {Text::ClientUpdates, "Mount the updates of the updates folder", "Підключати оновлення з теки updates",
     "Подключать обновления из папки updates"},
    {Text::ClientUpdatesHint,
     "The administrator's GRF archives come with the client's update from the site into the updates folder and go "
     "over the game's own "
     "archives: their files win, the newer archive (by name) over the older. Off: the game's own archives alone. It "
     "changes at the game's next start.",
     "GRF-архіви адміністратора приходять з оновленням клієнта із сайту в теку updates і перекривають власні архіви "
     "гри: їхні "
     "файли важливіші, новіший архів (за назвою) — над старішим. Вимкнено — лише власні архіви гри. Діє з наступного "
     "запуску гри.",
     "GRF-архивы администратора приходят с обновлением клиента с сайта в папку updates и перекрывают собственные "
     "архивы "
     "игры: их файлы важнее, более новый архив (по имени) — над более старым. Выключено — только собственные архивы "
     "игры. Действует со следующего запуска игры."},
    {Text::UpdatesNone,
     "The updates folder holds no archive.", "У теці updates архівів немає.", "В папке updates архивов нет."},
    {Text::UpdatesSome, "Archives in the updates folder: {1} ({2}), the newer first: {3}",
     "Архівів у теці updates: {1} ({2}), новіші першими: {3}",
     "Архивов в папке updates: {1} ({2}), более новые первыми: {3}"},
    {Text::ViewHelp, "Help", "Довідка", "Справка"},
    {Text::HelpIntro,
     "What every function of the program does, in the menu's order. The same descriptions show when the mouse stays "
     "over a row on its page.",
     "Що робить кожна функція програми — у порядку меню. Ті самі описи з'являються, якщо затримати мишу над рядком на "
     "його сторінці.",
     "Что делает каждая функция программы — в порядке меню. Те же описания появляются, если задержать мышь над "
     "строкой на её странице."},
    {Text::HelpFeatures, "What the program does", "Що вміє програма", "Что умеет программа"},
    {Text::HelpFeaturesList,
     "• uaRO Graphics draws the game anew on your video card: a sharp picture at any size, smoothing, light and "
     "effects, the AI textures.\n"
     "• «Play» opens before the game: the server's news and who is online, the updates, the «Play» button.\n"
     "• The updates come by themselves: the program's on its channel («Release» or «Beta (testing)»), the game's new "
     "files and the administrator's graphics updates from the site.\n"
     "• «Reports»: describe what went wrong, add screenshots - the developers answer there; in the game "
     "Ctrl+Shift+F10.\n"
     "• The gamepad: Xbox and PlayStation work at once, its buttons on «Gamepad».\n"
     "• The game's libraries: our own in place of the old ones of the 2000s, each can be turned off on «Service».\n"
     "• «Graphics test»: for 105 seconds the game goes through every effects set by itself and sends the developers "
     "how it runs on your computer - on «Play» or in the game's console (~).",
     "• uaRO Graphics малює гру наново на вашій відеокарті: чітка картинка будь-якого розміру, згладжування, світло й "
     "ефекти, ШІ-текстури.\n"
     "• «Гра» відкривається перед грою: новини сервера й хто онлайн, оновлення, кнопка «Грати».\n"
     "• Оновлення приходять самі: програми — за її каналом («Реліз» або «Бета (тестування)»), нові файли гри й "
     "оновлення графіки від адміністратора — із сайту.\n"
     "• «Звернення»: опишіть, що не так, додайте знімки — розробники відповідають там же; у грі Ctrl+Shift+F10.\n"
     "• Геймпад: Xbox і PlayStation працюють одразу, кнопки — на «Геймпад».\n"
     "• Бібліотеки гри: свої замість старих, 2000-х років; кожну можна вимкнути на «Сервіс».\n"
     "• «Тест графіки»: 105 секунд гра сама проходить усі набори ефектів і надсилає розробникам, як вона йде на "
     "вашому комп'ютері — на «Гра» або в консолі гри (~).",
     "• uaRO Graphics рисует игру заново на вашей видеокарте: чёткая картинка любого размера, сглаживание, свет и "
     "эффекты, ИИ-текстуры.\n"
     "• «Игра» открывается перед игрой: новости сервера и кто онлайн, обновления, кнопка «Играть».\n"
     "• Обновления приходят сами: программы — по её каналу («Релиз» или «Бета (тестирование)»), новые файлы игры и "
     "обновления графики от администратора — с сайта.\n"
     "• «Обращения»: опишите, что не так, добавьте снимки — разработчики отвечают там же; в игре Ctrl+Shift+F10.\n"
     "• Геймпад: Xbox и PlayStation работают сразу, кнопки — на «Геймпад».\n"
     "• Библиотеки игры: свои вместо старых, 2000-х годов; каждую можно выключить на «Сервис».\n"
     "• «Тест графики»: 105 секунд игра сама проходит все наборы эффектов и отправляет разработчикам, как она идёт "
     "на вашем компьютере — на «Игра» или в консоли игры (~)."},
    {Text::HelpViewPlay,
     "The server's news and who is online, the update channel and the updates, the «Play» button, the graphics test "
     "and its last result.",
     "Новини сервера й хто онлайн, канал оновлень і оновлення, кнопка «Грати», тест графіки та його останній "
     "результат.",
     "Новости сервера и кто онлайн, канал обновлений и обновления, кнопка «Играть», тест графики и его последний "
     "результат."},
    {Text::HelpViewMain,
     "The most needed settings of the other pages on one page; the recommended settings in one press.",
     "Найпотрібніші налаштування інших сторінок на одній; рекомендовані налаштування — одним натиском.",
     "Самые нужные настройки других страниц на одной; рекомендуемые настройки — одним нажатием."},
    {Text::HelpViewGraphics,
     "How the picture is drawn: the renderer, sharpness, smoothing (DLSS on NVIDIA), frames a second.",
     "Як малюється картинка: рендер, чіткість, згладжування (DLSS на NVIDIA), кадри за секунду.",
     "Как рисуется картинка: рендер, чёткость, сглаживание (DLSS на NVIDIA), кадры в секунду."},
    {Text::HelpViewScreen, "Full screen or a window, the window's size and the interface's.",
     "Повний екран чи вікно, розмір вікна й інтерфейсу.",
     "Полный экран или окно, размер окна и интерфейса."},
    {Text::HelpViewEffects,
     "The world's effects - light, weather, water, grass, sky - in groups, each its own switch; in the game, Ctrl+~.",
     "Ефекти світу — світло, погода, вода, трава, небо — групами, у кожного свій вимикач; у грі — Ctrl+~.",
     "Эффекты мира — свет, погода, вода, трава, небо — группами, у каждого свой выключатель; в игре — Ctrl+~."},
    {Text::HelpViewAi,
     "The AI textures: the game's textures made sharper by neural networks - what to improve, the models, the pack.",
     "ШІ-текстури: текстури гри, зроблені чіткішими нейромережами, — що покращувати, моделі, пакет.",
     "ИИ-текстуры: текстуры игры, сделанные чётче нейросетями, — что улучшать, модели, пакет."},
    {Text::HelpViewSound,
     "The game's sound: its own settings and ours - the output device, the sound around the player, the volume "
     "behind other windows.",
     "Звук гри: її власні налаштування й наші — пристрій виводу, звук навколо гравця, гучність за іншими вікнами.",
     "Звук игры: её собственные настройки и наши — устройство вывода, звук вокруг игрока, громкость за другими "
     "окнами."},
    {Text::HelpViewPad, "The gamepad: on or off, its buttons and sticks in the game.",
     "Геймпад: увімкнений чи ні, його кнопки й стіки в грі.",
     "Геймпад: включён или нет, его кнопки и стики в игре."},
    {Text::HelpViewGameChat,
     "The game's own options (effects, camera, snapping), the chat, and the window, mouse and keys of ROExt.",
     "Власні опції гри (ефекти, камера, прилипання), чат, а також вікно, миша й клавіші ROExt.",
     "Собственные опции игры (эффекты, камера, прилипание), чат, а также окно, мышь и клавиши ROExt."},
    {Text::HelpViewService,
     "Updates, profiles and backups, the game's libraries and graphics updates, the checks and «Reports».",
     "Оновлення, профілі й резервні копії, бібліотеки гри й оновлення графіки, перевірки та «Звернення».",
     "Обновления, профили и резервные копии, библиотеки игры и обновления графики, проверки и «Обращения»."},
    {Text::HelpViewDebug, "The developer's tools: the log, uaRO Studio, the texture pack's tools, test switches.",
     "Інструменти розробника: журнал, uaRO Studio, інструменти пакета текстур, тестові вимикачі.",
     "Инструменты разработчика: журнал, uaRO Studio, инструменты пакета текстур, тестовые выключатели."},
    {Text::HelpEffectsMore, "Each group holds its effects, each with a switch of its own, on the page itself.",
     "У кожній групі — її ефекти, у кожного свій вимикач, на самій сторінці.",
     "В каждой группе — её эффекты, у каждого свой выключатель, на самой странице."},
    {Text::ScreenShowHint,
     "Full screen: the game takes the whole monitor. In a window: the game in a window of the chosen size, next to "
     "other programs.",
     "На весь екран — гра займає весь монітор. У вікні — гра у вікні вибраного розміру, поруч з іншими програмами.",
     "Во весь экран — игра занимает весь монитор. В окне — игра в окне выбранного размера, рядом с другими "
     "программами."},
    {Text::ScreenMonitorHint, "The monitor the game opens on, when there is more than one.",
     "Монітор, на якому відкривається гра, коли їх кілька.",
     "Монитор, на котором открывается игра, когда их несколько."},
    {Text::SoundMusicHint,
     "The game's music. The same as /bgm in the chat.",
     "Музика гри. Те саме, що /bgm у чаті.",
     "Музыка игры. То же, что /bgm в чате."},
    {Text::SoundEffectsHint,
     "The game's sounds: skills, hits, monsters, the interface. The same as /sound in the chat.",
     "Звуки гри: уміння, удари, монстри, інтерфейс. Те саме, що /sound у чаті.",
     "Звуки игры: умения, удары, монстры, интерфейс. То же, что /sound в чате."},
    {Text::GameEffectsHint,
     "The skills' effects on the screen. Off: most of them are not shown. The same as /effect in the chat.",
     "Ефекти умінь на екрані. Вимкнено — більшість із них не показується. Те саме, що /effect у чаті.",
     "Эффекты умений на экране. Выключено — большинство из них не показывается. То же, что /effect в чате."},
    {Text::GameMissHint,
     "The word Miss over the target when an attack misses. The same as /miss in the chat.",
     "Напис Miss над ціллю, коли удар не влучає. Те саме, що /miss у чаті.",
     "Надпись Miss над целью, когда удар не попадает. То же, что /miss в чате."},
    {Text::GameSkillSnapHint,
     "While you aim a skill, the cursor jumps onto a monster near it. The same as /skillsnap in the chat.",
     "Поки цілишся вмінням, курсор сам стає на монстра поруч. Те саме, що /skillsnap у чаті.",
     "Пока целишься умением, курсор сам встаёт на монстра рядом. То же, что /skillsnap в чате."},
    {Text::GameAttackSnapHint,
     "For an attack the cursor jumps onto a monster near it. The same as /snap in the chat.",
     "Для атаки курсор сам стає на монстра поруч. Те саме, що /snap у чаті.",
     "Для атаки курсор сам встаёт на монстра рядом. То же, что /snap в чате."},
    {Text::GameItemSnapHint,
     "The cursor jumps onto an item on the ground near it. The same as /itemsnap in the chat.",
     "Курсор сам стає на предмет на землі поруч. Те саме, що /itemsnap у чаті.",
     "Курсор сам встаёт на предмет на земле рядом. То же, что /itemsnap в чате."},
    {Text::GameCameraHint,
     "On: the camera keeps the character in the middle all the time. Off: it follows smoothly, a little behind. The "
     "same as /camera in the chat.",
     "Увімкнено — камера весь час тримає персонажа в центрі. Вимкнено — йде за ним плавно, трохи із запізненням. Те "
     "саме, що /camera у чаті.",
     "Включено — камера всё время держит персонажа в центре. Выключено — следует за ним плавно, чуть с "
     "запаздыванием. То же, что /camera в чате."},
    {Text::GameHomunAiHint,
     "The homunculus acts by your own AI, AI\\USER_AI\\AI.lua, instead of the game's AI\\AI.lua. The same as /hoai "
     "in the chat.",
     "Гомункул діє за вашим ШІ — AI\\USER_AI\\AI.lua — замість ігрового AI\\AI.lua. Те саме, що /hoai у чаті.",
     "Гомункул действует по вашему ИИ — AI\\USER_AI\\AI.lua — вместо игрового AI\\AI.lua. То же, что /hoai в чате."},
    {Text::GameMercAiHint,
     "The mercenary acts by your own AI, AI\\USER_AI\\AI_M.lua, instead of the game's AI\\AI_M.lua. The same as "
     "/merai in the chat.",
     "Найманець діє за вашим ШІ — AI\\USER_AI\\AI_M.lua — замість ігрового AI\\AI_M.lua. Те саме, що /merai у чаті.",
     "Наёмник действует по вашему ИИ — AI\\USER_AI\\AI_M.lua — вместо игрового AI\\AI_M.lua. То же, что /merai в "
     "чате."},
    {Text::GameTipsHint,
     "A window with a tip opens once a session, when you enter the first map.",
     "Вікно з порадою відкривається раз за сесію, коли входиш на першу карту.",
     "Окно с советом открывается раз за сессию, при входе на первую карту."},
    {Text::GameIntroHint,
     "The intro movie before the login (openning.bik). The uaRO client has no such movie, so the box changes "
     "nothing.",
     "Вступне відео перед входом (openning.bik). У клієнті uaRO такого відео немає, тож галочка нічого не змінює.",
     "Вступительное видео перед входом (openning.bik). В клиенте uaRO такого видео нет, так что галочка ничего не "
     "меняет."},
    {Text::GameMinimapHint,
     "The minimap half see-through, so that the world shows under it. The same as its own button in the game.",
     "Мінікарта напівпрозора, крізь неї видно світ. Те саме, що її власна кнопка в грі.",
     "Миникарта полупрозрачная, сквозь неё видно мир. То же, что её собственная кнопка в игре."},
    {Text::GameStacksHint,
     "In the storage and the cart a stack moves whole, without the question how many. The same as the box in those "
     "windows.",
     "На складі й у візку стос переноситься цілим, без питання про кількість. Те саме, що галочка в цих вікнах.",
     "На складе и в тележке стопка переносится целиком, без вопроса о количестве. То же, что галочка в этих окнах."},
    {Text::ChatWhisperStrangersHint,
     "A private message (a whisper) from someone not among your friends opens a window of its own. Off: in the chat "
     "only.",
     "Особисте повідомлення (шепіт) від того, кого немає серед друзів, відкриває окреме вікно. Вимкнено — лише в "
     "чаті.",
     "Личное сообщение (шёпот) от того, кого нет среди друзей, открывает отдельное окно. Выключено — только в чате."},
    {Text::ChatWhisperSoundHint,
     "A short sound when a whisper comes, so that it is not missed.",
     "Короткий звук, коли приходить шепіт, щоб його не проґавити.",
     "Короткий звук, когда приходит шёпот, чтобы его не пропустить."},
    {Text::ChatRememberLoginHint,
     "The login window keeps your login for the next time; the password is never kept.",
     "Вікно входу пам'ятає ваш логін до наступного разу; пароль не зберігається.",
     "Окно входа помнит ваш логин до следующего раза; пароль не сохраняется."},
    {Text::WindowsTitleName, "The character in the window's title",
     "Персонаж у заголовку вікна",
     "Персонаж в заголовке окна"},
    {Text::WindowsTitleNameHint,
     "The window's title and its taskbar button show the character: the levels, the experience to the next level, HP "
     "and SP, the weight from 50 % and the map - for example, Lena · 99/70 · 45.2 % · HP 85 % · SP 40 % · prt_fild08 "
     "- uaRO HD. With several game windows open each one is seen in Alt+Tab.",
     "Заголовок вікна й кнопка на панелі задач показують персонажа: рівні, досвід до наступного рівня, HP і SP, вагу "
     "від 50 % і карту — наприклад, Lena · 99/70 · 45,2 % · HP 85 % · SP 40 % · prt_fild08 - uaRO HD. Коли відкрито "
     "кілька вікон гри, кожне видно в Alt+Tab.",
     "Заголовок окна и кнопка на панели задач показывают персонажа: уровни, опыт до следующего уровня, HP и SP, вес "
     "от 50 % и карту — например, Lena · 99/70 · 45,2 % · HP 85 % · SP 40 % · prt_fild08 - uaRO HD. Когда открыто "
     "несколько окон игры, каждое видно в Alt+Tab."},
    {Text::WindowsRememberPlace, "Each character's window at its own place",
     "Вікно кожного персонажа — на своєму місці",
     "Окно каждого персонажа — на своём месте"},
    {Text::WindowsRememberPlaceHint,
     "Drag a character's window where you want it: when that character comes into the game again, its window goes "
     "there by itself. For a window with a frame (not «A borderless window locked in place»).",
     "Перетягніть вікно персонажа, куди зручно: коли цей персонаж знову ввійде в гру, вікно стане туди саме. Для вікна "
     "з рамкою (не «Вікно без рамки, закріплене на місці»).",
     "Перетащите окно персонажа, куда удобно: когда этот персонаж снова войдёт в игру, окно встанет туда само. Для "
     "окна с рамкой (не «Окно без рамки, закреплённое на месте»)."},
    {Text::WindowsOthersLight, "The other game windows light",
     "Інші вікна гри — легкі",
     "Другие окна игры — лёгкие"},
    {Text::WindowsOthersLightHint,  // 3.0.11-beta.197: 5 frames (2 wholly behind another game window), as light_mode
     "With several game windows open, the ones you are not playing in draw without effects, HD pictures and DLSS, at "
     "half the resolution and 5 frames a second (2 wholly behind another game window). The window you switch to is "
     "whole again at once.",
     "Коли відкрито кілька вікон гри, ті, у яких ви зараз не граєте, малюють без ефектів, HD-картинок і DLSS, у "
     "половинній роздільності й 5 кадрів на секунду (2 — цілком за іншим вікном гри). Вікно, на яке ви перейшли, "
     "одразу знову повне.",
     "Когда открыто несколько окон игры, те, в которых вы сейчас не играете, рисуют без эффектов, HD-картинок и DLSS, "
     "в половинном разрешении и 5 кадров в секунду (2 — целиком за другим окном игры). Окно, на которое вы перешли, "
     "сразу снова полное."},
    {Text::WindowsOthersMusic, "Music in the other game windows",
     "Музика в інших вікнах гри",
     "Музыка в других окнах игры"},
    {Text::WindowsOthersMusicHint,
     "Off: with several game windows open, only the one you played in last plays its music. Their sounds stay, as "
     "quiet as «Volume in the background» says.",
     "Вимкнено: коли відкрито кілька вікон гри, музику грає лише те, у якому ви грали останнім. Їхні звуки лишаються, "
     "тихіші, як каже «Гучність у фоні».",
     "Выключено: когда открыто несколько окон игры, музыку играет только то, в котором вы играли последним. Их звуки "
     "остаются, тише, как говорит «Громкость в фоне»."},
    {Text::WindowsHeading, "Several game windows", "Кілька вікон гри", "Несколько окон игры"},
    {Text::TitleWeight, "weight {1} %", "вага {1} %", "вес {1} %"},
    {Text::WindowsKeys, "Window keys", "Клавіші вікон", "Клавиши окон"},
    {Text::WindowsKeysHint,
     "Ctrl+Tab goes back to the game window you played in before this one, Ctrl+Alt+1...8 straight to the window of "
     "that number: the order the windows were started in, the first one 1. Ctrl+Alt+0 shows or hides the windows' "
     "panel. The game does not get these keys.",
     "Ctrl+Tab повертає до вікна гри, у якому ви грали перед цим, Ctrl+Alt+1…8 — одразу до вікна з цим номером: це "
     "порядок, у якому вікна запускали, перше — 1. Ctrl+Alt+0 показує або ховає панель вікон. Гра цих натискань не "
     "отримує.",
     "Ctrl+Tab возвращает к окну игры, в котором вы играли перед этим, Ctrl+Alt+1…8 — сразу к окну с этим номером: "
     "это порядок, в котором окна запускали, первое — 1. Ctrl+Alt+0 показывает или прячет панель окон. Игра этих "
     "нажатий не получает."},
    {Text::WindowsKeysAll, "Ctrl+Tab and Ctrl+Alt+1...8", "Ctrl+Tab і Ctrl+Alt+1…8", "Ctrl+Tab и Ctrl+Alt+1…8"},
    {Text::WindowsKeysBack, "Ctrl+Tab alone", "Лише Ctrl+Tab", "Только Ctrl+Tab"},
    {Text::WindowsKeysOff, "Off", "Вимкнено", "Выключено"},
    {Text::WindowsPanel, "The windows' panel in the game", "Панель вікон у грі", "Панель окон в игре"},
    {Text::WindowsPanelHint,
     "With two or more game windows open, a panel in the game's picture shows them: a click on a window's card brings "
     "it to the front, the current one is blue, the thin bars are HP and SP (red under 25 %). Drag it by its top row; "
     "the button beside its cross lays the cards in a row or a column; the cross or Ctrl+Alt+0 hides it, Ctrl+Alt+0 "
     "or this switch shows it again.",
     "Коли відкрито два вікна гри або більше, панель у картинці гри показує їх: клік по картці вікна виводить його "
     "вперед, поточне — синє, тонкі смужки — HP і SP (червоні нижче 25 %). Панель тягнуть за верхній рядок; кнопка "
     "біля хрестика кладе картки в ряд або стовпчиком; хрестик або Ctrl+Alt+0 ховає її, Ctrl+Alt+0 або цей "
     "перемикач показує знову.",
     "Когда открыто два окна игры или больше, панель в картинке игры показывает их: клик по карточке окна выводит "
     "его вперёд, текущее — синее, тонкие полоски — HP и SP (красные ниже 25 %). Панель тянут за верхнюю строку; "
     "кнопка рядом с крестиком кладёт карточки в ряд или столбиком; крестик или Ctrl+Alt+0 прячет её, Ctrl+Alt+0 "
     "или этот переключатель показывает снова."},
    {Text::WindowsPanelLayout, "The panel's layout", "Вигляд панелі", "Вид панели"},
    {Text::WindowsPanelLayoutHint,
     "The windows' cards side by side or one under another, or small icons with their numbers alone (the name shows "
     "under the mouse).",
     "Картки вікон поруч або одна під одною чи маленькі іконки лише з номерами (ім'я видно під мишею).",
     "Карточки окон рядом или одна под другой либо маленькие иконки только с номерами (имя видно под мышью)."},
    {Text::WindowsPanelRow, "In a row", "В ряд", "В ряд"},
    {Text::WindowsPanelColumn, "In a column", "Стовпчиком", "Столбиком"},
    {Text::WindowsPanelSize, "The panel's size", "Розмір панелі", "Размер панели"},
    {Text::WindowsPanelSizeHint,
     "The panel grows with the game's interface («Interface size»); this makes it larger or smaller still.",
     "Панель росте разом з інтерфейсом гри («Розмір інтерфейсу»); тут її можна зробити ще більшою або меншою.",
     "Панель растёт вместе с интерфейсом игры («Размер интерфейса»); здесь её можно сделать ещё крупнее или "
     "мельче."},
    {Text::PanelTitle, "Windows", "Вікна", "Окна"},
    {Text::PanelWindow, "window {1}", "вікно {1}", "окно {1}"},
    {Text::PanelHeading, "The windows' panel", "Панель вікон", "Панель окон"},
    {Text::PanelOpacity, "The panel's opacity", "Непрозорість панелі", "Непрозрачность панели"},
    {Text::PanelOpacityHint, "Less: the game shows through the panel more.",
     "Менше — гру крізь панель видно більше.", "Меньше — игру сквозь панель видно больше."},
    {Text::WindowsOthersSounds, "Sounds in the other game windows", "Звуки в інших вікнах гри",
     "Звуки в других окнах игры"},
    {Text::WindowsOthersSoundsHint,
     "Off: with several game windows open, only the one you play in now is heard; the others' sounds fade out.",
     "Вимкнено: коли відкрито кілька вікон гри, чути лише те, у якому ви зараз граєте; звуки інших стихають.",
     "Выключено: когда открыто несколько окон игры, слышно только то, в котором вы сейчас играете; звуки других "
     "стихают."},
    {Text::PanelMenuLayout, "Layout", "Вигляд", "Вид"},
    {Text::PanelMenuSize, "Size", "Розмір", "Размер"},
    {Text::PanelMenuOpacity, "Opacity", "Непрозорість", "Непрозрачность"},
    {Text::PanelMenuHide, "Hide the panel · Ctrl+Alt+0", "Сховати панель · Ctrl+Alt+0", "Спрятать панель · Ctrl+Alt+0"},
    {Text::WindowsPanelIcons, "Icons", "Іконки", "Иконки"},
    // 3.0.11-beta.1: the new interface's minimap prototype (stage 1)
    // 3.0.11-beta.301 (the review: it turns on the whole new interface, not a minimap prototype)
    {Text::InterfaceMinimap, "New interface", "Новий інтерфейс", "Новый интерфейс"},
    {Text::InterfaceMinimapHint,
     "The game's interface redrawn: the minimap and its big map, the character's block, the target's card, the party, "
     "the buffs, the hotbar, the chat, the windows (items, stats, skills, equipment, storage, shops, NPC talk), the "
     "login and the character select. Each part has its own switch below. Off - the game's own interface.",
     "Інтерфейс гри наново: мінікарта і велика карта, блок персонажа, картка цілі, група, бафи, панель клавіш, чат, "
     "вікна (речі, характеристики, навички, спорядження, склад, крамниці, розмова з NPC), вхід і вибір персонажа. "
     "Кожна частина має свій перемикач нижче. Вимкнено - власний інтерфейс гри.",
     "Интерфейс игры заново: миникарта и большая карта, блок персонажа, карточка цели, группа, бафы, панель клавиш, "
     "чат, окна (вещи, характеристики, навыки, снаряжение, склад, магазины, разговор с NPC), вход и выбор персонажа. "
     "У каждой части свой переключатель ниже. Выключено - собственный интерфейс игры."},
    {Text::InterfaceHeading, "The new interface", "Новий інтерфейс", "Новый интерфейс"},  // 3.0.11-beta.2
    {Text::ViewInterface, "Interface", "Інтерфейс", "Интерфейс"},  // 3.0.11-beta.3: its own item
    {Text::HelpViewInterface,
     "The game's new interface, built part by part: each part has its switch here to try it out before it stays.",
     "Новий інтерфейс гри, який ми робимо частинами: кожна частина має тут свій перемикач, щоб спробувати її, перш ніж "
     "вона залишиться.",
     "Новый интерфейс игры, который мы делаем по частям: у каждой части здесь свой переключатель, чтобы попробовать "
     "её, прежде чем она останется."},
    {Text::InterfaceNote,
     "Parts of the new interface to try. Each switch works at once, in a running game too; off brings the game's own "
     "part back. In the game the console (~) has them too.",
     "Частини нового інтерфейсу, щоб спробувати. Кожен перемикач діє одразу, і в запущеній грі; вимкнено - частина гри "
     "повертається. У грі вони є і в консолі (~).",
     "Части нового интерфейса, чтобы попробовать. Каждый переключатель действует сразу, и в запущенной игре; "
     "выключено - часть игры возвращается. В игре они есть и в консоли (~)."},
    // 3.0.4-beta.1: the output device ([sound] output)
    {Text::SoundOutput, "Sound output device",
     "Пристрій виводу звуку",
     "Устройство вывода звука"},
    {Text::SoundOutputHint,
     "Where the game's music and sounds play. «As in Windows»: the device Windows sends sound to. A device chosen "
     "here: while it is off (headphones not connected), the sound plays as in Windows and comes back to it when it "
     "is on again. Applies at once, in a running game too.",
     "Куди грають музика й звуки гри. «Як у Windows» — на пристрій, куди звук спрямовує Windows. Вибраний тут "
     "пристрій: поки він вимкнений (навушники не підключені), звук іде як у Windows, а коли пристрій знову "
     "з'явиться — повертається на нього. Діє одразу, і в запущеній грі.",
     "Куда играют музыка и звуки игры. «Как в Windows» — на устройство, куда звук направляет Windows. Выбранное "
     "здесь устройство: пока оно выключено (наушники не подключены), звук идёт как в Windows, а когда устройство "
     "снова появится — возвращается на него. Действует сразу, и в запущенной игре."},
    {Text::SoundOutputWindows, "As in Windows",
     "Як у Windows",
     "Как в Windows"},
    {Text::SoundOutputWindowsNow, "As in Windows (now: {1})",
     "Як у Windows (зараз: {1})",
     "Как в Windows (сейчас: {1})"},
    {Text::SoundOutputAway, "{1} — not connected",
     "{1} — не підключено",
     "{1} — не подключено"},
    {Text::SoundOutputGone, "A device no longer in Windows",
     "Пристрій, якого вже немає у Windows",
     "Устройство, которого уже нет в Windows"},
    {Text::SoundOutputAwayNote,
     "The chosen device is not connected now: the game's sound plays as in Windows until it is back.",
     "Вибраний пристрій зараз не підключено: звук гри йде як у Windows, доки його не підключать.",
     "Выбранное устройство сейчас не подключено: звук игры идёт как в Windows, пока его не подключат."},
    // 3.0.4-beta.3: notifications while the game is behind ([notify])
    {Text::NotifyHeading, "Notifications", "Сповіщення", "Уведомления"},
    {Text::NotifyOn, "Notifications while the game is behind",
     "Сповіщення, коли гра у фоні",
     "Уведомления, когда игра в фоне"},
    {Text::NotifyOnHint,  // 3.0.7-beta.6: the new kinds
     "When the game window is behind others or minimised: its taskbar button flashes and Windows shows what wants "
     "you - a whisper, an invitation, a trade, an attack, a buff ending, the party, the chat, the character's death, "
     "the server gone. A click on the notification brings the game back. With several game windows each tells of its "
     "own; the party's and the guild's news the main one alone.",
     "Коли вікно гри за іншими вікнами чи згорнуте: його кнопка на панелі завдань блимає, а Windows показує, що "
     "сталося, — шепіт, запрошення, обмін, атака, кінець бафу, група, чат, смерть персонажа, втрата зв'язку. Клік по "
     "сповіщенню повертає гру. З кількома вікнами гри кожне повідомляє про своє, а новини групи й гільдії — лише "
     "головне.",
     "Когда окно игры за другими окнами или свёрнуто: его кнопка на панели задач мигает, а Windows показывает, что "
     "случилось, — шёпот, приглашение, обмен, атака, конец бафа, группа, чат, смерть персонажа, потеря связи. Щелчок "
     "по уведомлению возвращает игру. С несколькими окнами игры каждое сообщает о своём, а новости группы и гильдии — "
     "только главное."},
    {Text::NotifyWhisper, "Whispers", "Шепіт", "Шёпот"},
    {Text::NotifyParty, "Party invitations", "Запрошення до групи", "Приглашения в группу"},
    {Text::NotifyTrade, "Trade offers", "Пропозиції обміну", "Предложения обмена"},
    {Text::NotifyGuild, "Guild invitations", "Запрошення до гільдії", "Приглашения в гильдию"},
    {Text::NotifyFriend, "Friend requests", "Запити в друзі", "Запросы в друзья"},
    {Text::NotifyDeath, "The character's death", "Смерть персонажа", "Смерть персонажа"},
    {Text::NotifyKindHint, "Told while the game is behind other windows.",
     "Сповіщати, поки гра у фоні.",
     "Уведомлять, пока игра в фоне."},
    {Text::NotifyWhisperText, "Whisper from {1}: {2}", "Шепіт від {1}: {2}", "Шёпот от {1}: {2}"},
    {Text::NotifyPartyText, "Invitation to the party {1}", "Запрошення до групи {1}", "Приглашение в группу {1}"},
    {Text::NotifyTradeText, "{1} offers a trade", "{1} пропонує обмін", "{1} предлагает обмен"},
    {Text::NotifyGuildText, "Invitation to the guild {1}", "Запрошення до гільдії {1}", "Приглашение в гильдию {1}"},
    {Text::NotifyFriendText, "{1} asks to be your friend", "{1} хоче додати вас у друзі",
     "{1} хочет добавить вас в друзья"},
    {Text::NotifyDeathText, "The character died", "Персонаж загинув", "Персонаж погиб"},
    // 3.0.7-beta.6 (the user chose 1, 5, 6, 7, 8, 9, 10 «и чтобы это в конфигураторе настраивалось»)
    {Text::NotifyAttack, "An attack on the character", "Атака на персонажа", "Атака на персонажа"},
    {Text::NotifyAttackHint,
     "When the character comes under attack (misses too): once, then again only after half a minute without hits.",
     "Коли персонажа починають атакувати (і промахи теж): один раз, а знову — лише після пів хвилини без ударів.",
     "Когда персонажа начинают атаковать (и промахи тоже): один раз, а снова — только после полуминуты без ударов."},
    {Text::NotifyBuffEnding, "A buff ending", "Кінець бафу", "Конец бафа"},
    {Text::NotifyBuffEndingHint,
     "A few seconds before one of the character's buffs (Blessing, Increase AGI, food...) runs out - its name. Short "
     "ones (under half a minute) are not told.",
     "За кілька секунд до кінця бафу персонажа (Blessing, Increase AGI, їжа…) — його назва. Про короткі (менше пів "
     "хвилини) не сповіщає.",
     "За несколько секунд до конца бафа персонажа (Blessing, Increase AGI, еда…) — его название. О коротких (меньше "
     "полуминуты) не уведомляет."},
    {Text::NotifyBuffSeconds, "Seconds before a buff ends", "За скільки секунд до кінця бафу",
     "За сколько секунд до конца бафа"},
    {Text::NotifyBuffSecondsHint, "How long before a buff runs out to tell of it.",
     "За скільки секунд до кінця бафу сповіщати.", "За сколько секунд до конца бафа уведомлять."},
    {Text::NotifyPartyDeath, "A death in the party", "Смерть у групі", "Смерть в группе"},
    {Text::NotifyPartyDeathHint,
     "When one of the party near the character dies. With several game windows only the main one tells.",
     "Коли гине хтось із групи поруч із персонажем. З кількома вікнами гри повідомляє лише головне.",
     "Когда погибает кто-то из группы рядом с персонажем. С несколькими окнами игры сообщает только главное."},
    {Text::NotifyPartyLowHp, "Low HP in the party", "Мало HP у групі", "Мало HP в группе"},
    {Text::NotifyPartyLowHpHint,
     "When one of the party near the character falls under the threshold. With several game windows only the main "
     "one tells.",
     "Коли в когось із групи поруч із персонажем HP падає нижче порогу. З кількома вікнами гри повідомляє лише "
     "головне.",
     "Когда у кого-то из группы рядом с персонажем HP падает ниже порога. С несколькими окнами игры сообщает только "
     "главное."},
    {Text::NotifyPartyHp, "Party HP threshold", "Поріг HP групи", "Порог HP группы"},
    {Text::NotifyPartyHpHint, "Under how many per cent of HP to tell of one of the party.",
     "Нижче скількох відсотків HP сповіщати про когось із групи.",
     "Ниже скольких процентов HP уведомлять о ком-то из группы."},
    {Text::NotifyPartyChat, "The party's chat", "Чат групи", "Чат группы"},
    {Text::NotifyGuildChat, "The guild's chat", "Чат гільдії", "Чат гильдии"},
    {Text::NotifyChatHint,
     "What the others write there (not your own). With several game windows only the main one tells.",
     "Що пишуть інші (не ви самі). З кількома вікнами гри повідомляє лише головне.",
     "Что пишут другие (не вы сами). С несколькими окнами игры сообщает только главное."},
    {Text::NotifyMention, "Your name in the chat", "Згадка імені в чаті", "Упоминание имени в чате"},
    {Text::NotifyMentionHint, "When someone near writes the character's name in the common chat.",
     "Коли хтось поруч пише ім'я персонажа в загальному чаті.",
     "Когда кто-то рядом пишет имя персонажа в общем чате."},
    {Text::NotifyDisconnect, "The connection to the server lost", "Втрата зв'язку із сервером",
     "Потеря связи с сервером"},
    {Text::NotifyDisconnectHint, "When the server closes the connection - the game is no longer online.",
     "Коли сервер розриває з'єднання — гра вже не в мережі.",
     "Когда сервер разрывает соединение — игра уже не в сети."},
    {Text::NotifySound, "Notification sound", "Звук сповіщень", "Звук уведомлений"},
    {Text::NotifySoundHint, "Windows' own sound with each notification. Off: silent.",
     "Звук Windows разом із кожним сповіщенням. Вимкнено — без звуку.",
     "Звук Windows вместе с каждым уведомлением. Выключено — без звука."},
    {Text::NotifyAttackText, "The character is under attack", "Персонажа атакують", "Персонажа атакуют"},
    {Text::NotifyAttackHurtText, "The character is under attack: -{1} HP", "Персонажа атакують: -{1} HP",
     "Персонажа атакуют: -{1} HP"},
    {Text::NotifyBuffEndingText, "{1} ends in {2} s", "{1} закінчується за {2} с", "{1} заканчивается через {2} с"},
    {Text::NotifyPartyDeathText, "{1} of the party died", "У групі загинув персонаж {1}",
     "В группе погиб персонаж {1}"},
    {Text::NotifyPartyLowHpText, "{1} of the party: {2}% HP", "У персонажа {1} з групи {2}% HP",
     "У персонажа {1} из группы {2}% HP"},
    {Text::NotifyPartyChatText, "Party - {1}: {2}", "Група — {1}: {2}", "Группа — {1}: {2}"},
    {Text::NotifyGuildChatText, "Guild - {1}: {2}", "Гільдія — {1}: {2}", "Гильдия — {1}: {2}"},
    {Text::NotifyMentionText, "Your name in the chat - {1}: {2}", "Ваше ім'я в чаті — {1}: {2}",
     "Ваше имя в чате — {1}: {2}"},
    {Text::NotifyDisconnectText, "The connection to the server is lost", "Зв'язок із сервером втрачено",
     "Связь с сервером потеряна"},
    // 3.0.7-beta.7 (the user: «надо сделать чтобы когда хп и сп восстановились»)
    {Text::NotifyRecovered, "HP and SP back", "HP і SP відновилися", "HP и SP восстановились"},
    {Text::NotifyRecoveredHint,
     "When the character's HP and SP are both back (sitting, a rest) after one of them fell. Once, again after "
     "another fall.",
     "Коли HP і SP персонажа обидва відновилися (сидячи, після відпочинку), після того як щось із них просіло. Один "
     "раз, знову — після наступного просідання.",
     "Когда HP и SP персонажа оба восстановились (сидя, после отдыха), после того как что-то из них просело. Один "
     "раз, снова — после следующего проседания."},
    {Text::NotifyRecoveredPercent, "Back to", "Відновлення до", "Восстановление до"},
    {Text::NotifyRecoveredPercentHint, "Of their most: 100 % - full HP and SP.",
     "Від найбільшого: 100 % — повні HP і SP.", "От максимума: 100 % — полные HP и SP."},
    {Text::NotifyRecoveredText, "HP and SP back to {1}%", "HP і SP відновилися до {1} %",
     "HP и SP восстановились до {1} %"},
    // 3.0.4-beta.4: «Грати» starts a set of game windows
    {Text::WindowsStartCount, "Game windows started by «Play»",
     "Вікон гри за кнопкою «Грати»",
     "Окон игры по кнопке «Играть»"},
    {Text::WindowsStartCountHint,
     "1 - one game, as before. 2-8: a set for several characters (a priest, a sage...) - «Play» starts the game that "
     "many times, one every second and a half; you log in to each yourself, no login or password is kept.",
     "1 — одна гра, як і раніше. 2–8: набір для кількох персонажів (прист, сейдж...) — «Грати» запускає гру стільки "
     "разів, по одній кожні півтори секунди; у кожну ви входите самі, логіни й паролі не зберігаються.",
     "1 — одна игра, как и раньше. 2–8: набор для нескольких персонажей (прист, сейдж...) — «Играть» запускает игру "
     "столько раз, по одной каждые полторы секунды; в каждую вы входите сами, логины и пароли не хранятся."},
    // 3.0.4-beta.5: the camera and the photo mode
    {Text::CameraHeading, "Camera", "Камера", "Камера"},
    {Text::CameraIndoor, "Free camera in buildings",
     "Вільна камера в будинках",
     "Свободная камера в зданиях"},
    {Text::CameraIndoorHint,
     "Inside buildings - inns, shops, guild halls, castles - the game locks the camera: it barely turns and does not "
     "zoom out. With this box it turns, tilts and zooms out there as it does outdoors.",
     "У будинках — тавернах, крамницях, гільдіях, замках — гра тримає камеру: вона ледь повертається і не "
     "віддаляється. З цим прапорцем там вона повертається, нахиляється й віддаляється, як надворі.",
     "В зданиях — тавернах, лавках, гильдиях, замках — игра держит камеру: она едва поворачивается и не отдаляется. С "
     "этим флажком там она поворачивается, наклоняется и отдаляется, как на улице."},
    {Text::CameraPhotoNote,
     "Ctrl+Alt+F12 - the photo mode: the game's interface, the cursor and the windows' panel hidden for a clean shot "
     "(Ctrl+Shift+F12); the same keys again bring them back.",
     "Ctrl+Alt+F12 — режим фото: інтерфейс гри, курсор і панель вікон сховані для чистого знімка (Ctrl+Shift+F12); "
     "ті самі клавіші ще раз повертають їх.",
     "Ctrl+Alt+F12 — режим фото: интерфейс игры, курсор и панель окон скрыты для чистого снимка (Ctrl+Shift+F12); те "
     "же клавиши ещё раз возвращают их."},
    // 3.0.4-beta.6: the time left on the buff icons
    {Text::BuffsHeading, "Buffs", "Баффи", "Баффы"},
    {Text::BuffTimers, "Time left on the buff icons",
     "Час на значках баффів",
     "Время на значках баффов"},
    {Text::BuffTimersHint,
     "How long Blessing, Increase AGI, the potions and the rest still last - on each icon at the screen's right edge. "
     "The times come from the server's own data; a buff cast by another player counts from its level, an unknown one "
     "from its longest time.",
     "Скільки ще триватимуть Blessing, Increase AGI, зілля та інше — на кожному значку біля правого краю екрана. Час "
     "береться з даних самого сервера; бафф від іншого гравця рахується за його рівнем, невідомий — за найдовшим "
     "часом.",
     "Сколько ещё продлятся Blessing, Increase AGI, зелья и остальное — на каждом значке у правого края экрана. Время "
     "берётся из данных самого сервера; бафф от другого игрока считается по его уровню, неизвестный — по самому "
     "долгому времени."},
    // 3.0.4-beta.7: the maps' own sounds
    {Text::GfxSoundAmbience, "Sounds of the world",
     "Звуки світу",
     "Звуки мира"},
    {Text::GfxSoundAmbienceHint,
     "Birds in the fields, brooks, the sea's waves, the wind in the desert, insects at night, drops in the caves - the "
     "sounds the game's makers placed on 563 maps, which this old client never played. Each is heard near its place, "
     "louder as you come closer, from its side. With the game's effects volume.",
     "Птахи в полях, струмки, морські хвилі, вітер у пустелі, комахи, краплі в печерах — звуки, які розробники гри "
     "розставили на 563 картах, а цей старий клієнт їх ніколи не грав. Кожен чути біля його місця, гучніше ближче, з "
     "його боку. Разом із гучністю звуків ефектів гри.",
     "Птицы в полях, ручьи, морские волны, ветер в пустыне, насекомые, капли в пещерах — звуки, которые разработчики "
     "игры расставили на 563 картах, а этот старый клиент их никогда не играл. Каждый слышно у его места, громче "
     "ближе, с его стороны. Вместе с громкостью звуков эффектов игры."},
    {Text::GfxSoundAmbienceVolume, "Sounds of the world: volume",
     "Звуки світу: гучність",
     "Звуки мира: громкость"},
    {Text::GfxSoundAmbienceVolumeHint, "Their share of the game's effects volume.",
     "Їхня частка від гучності звуків ефектів гри.",
     "Их доля от громкости звуков эффектов игры."},
    // 3.0.4-beta.10: the own world's prototype
    {Text::FxOwnGround, "Own ground",  // 3.0.4-beta.26 (the user chose «Своя земля в «Ремастер»»): no «prototype»
     "Своя земля",
     "Своя земля"},
    {Text::FxOwnGroundHint,
     "The ground drawn by uaRO Graphics itself from the map's file instead of the game: smooth hills, the textures in "
     "full colour, soft passes from one to the next, and when the camera goes far out the land, the water and the "
     "houses on past the game's far edge. Direct3D 12.",
     "Землю малює сам uaRO Graphics із файла карти замість гри: плавні пагорби, текстури в повному кольорі, м'які "
     "переходи між ними, а коли камера далеко — земля, вода й будинки і за дальнім краєм гри. Direct3D 12.",
     "Землю рисует сам uaRO Graphics из файла карты вместо игры: плавные холмы, текстуры в полном цвете, мягкие "
     "переходы между ними, а когда камера далеко — земля, вода и дома и за дальним краем игры. Direct3D 12."},
    {Text::KeepFpsOffNote, "{1} is off for the rest of the game: {2} frames a second, under {3} (Keep the frame rate)",
     "{1}: вимкнено до кінця гри — {2} кадр/с, менше ніж {3} («Тримати частоту кадрів»)",
     "{1}: выключено до конца игры — {2} кадр/с, меньше {3} («Держать частоту кадров»)"},
    {Text::KeepFpsThinNote,
     "The grass thinned for the rest of the game: {1} frames a second, under {2} (Keep the frame rate)",
     "Траву проріджено до кінця гри — {1} кадр/с, менше ніж {2} («Тримати частоту кадрів»)",
     "Трава прорежена до конца игры — {1} кадр/с, меньше {2} («Держать частоту кадров»)"},
    // 3.0.11-beta.58 (the autopilot): any set of the ladder - {1} the set tried, {2} the one now
    {Text::CardRaisedNote, "The card keeps up: the effects set «{1}» on trial",
     "Відеокарта встигає: набір ефектів «{1}» — на пробу",
     "Видеокарта успевает: набор эффектов «{1}» — на пробу"},
    {Text::CardLoweredNote, "The set «{1}» is too much for this card: «{2}» back",
     "Набір «{1}» заважкий для цієї відеокарти — повернуто «{2}»",
     "Набор «{1}» тяжёл для этой видеокарты — возвращён «{2}»"},
    {Text::CardSlowNote, "The card cannot keep up with the set «{1}»: «{2}» now",
     "Відеокарта не встигає з набором «{1}» — тепер «{2}»",
     "Видеокарта не успевает с набором «{1}» — теперь «{2}»"},
    {Text::AutopilotNote, "Effects for this graphics card: «{1}» · aiming at {2} frames a second",
     "Ефекти під цю відеокарту: «{1}» · ціль — {2} кадрів за секунду",
     "Эффекты под эту видеокарту: «{1}» · цель — {2} кадров в секунду"},
    {Text::AutopilotChange, "Change", "Змінити", "Изменить"},
    {Text::FxOwnFarModels, "Houses and trees far off", "Будинки й дерева вдалині", "Дома и деревья вдали"},
    {Text::FxOwnFarModelsHint,
     "With the own ground, the map's houses, trees and rocks drawn by uaRO Graphics from the map's files past the edge "
     "where the game cuts them off - the land is not bare when the camera goes far out. Their colours are taken "
     "from the game's own drawing of them near.",
     "Зі своєю землею будинки, дерева й скелі карти малює сам uaRO Graphics з файлів карти й за краєм, де гра їх "
     "обрізає, — коли камера далеко, земля не порожня. Їхні кольори беруться з того, як гра малює їх поблизу.",
     "Со своей землёй дома, деревья и скалы карты рисует сам uaRO Graphics из файлов карты и за краем, где игра их "
     "обрезает, — когда камера далеко, земля не пустая. Их цвета берутся из того, как игра рисует их вблизи."},
    {Text::CameraZoomLimit, "Camera no further out than the character shows",
     "Камера не далі, ніж видно персонажа",
     "Камера не дальше, чем виден персонаж"},
    {Text::CameraZoomLimitHint,
     "The game lets the camera go out to 2040 but draws its world only to 1500: further out the character, the "
     "monsters and the effects vanish. With this box the camera stops at 1400.",
     "Гра дає віддалити камеру до 2040, а свій світ малює лише до 1500: далі зникають персонаж, монстри й ефекти. З "
     "цим прапорцем камера зупиняється на 1400.",
     "Игра даёт отдалить камеру до 2040, а свой мир рисует только до 1500: дальше пропадают персонаж, монстры и "
     "эффекты. С этим флажком камера останавливается на 1400."},
    {Text::UpdateBackNote,
     "The channel is «Release», and the beta {2} is installed. «Back to the release {1}» puts the release in; the "
     "settings stay.",
     "Вибрано канал «Реліз», а встановлено бету {2}. «Повернутися на реліз {1}» встановить реліз, налаштування "
     "залишаться.",
     "Выбран канал «Релиз», а установлена бета {2}. «Вернуться на релиз {1}» установит релиз, настройки останутся."},
    {Text::UpdateBackButton, "Back to the release {1}", "Повернутися на реліз {1}", "Вернуться на релиз {1}"},
    {Text::PlayBackButton, "Back to the release and play", "Повернутися на реліз і грати",
     "Вернуться на релиз и играть"},
    {Text::MainUpdateBack, "back to the release {1} possible", "можна повернутися на реліз {1}",
     "можно вернуться на релиз {1}"},
    {Text::UpdateBackDo, "Go back", "Повернутися", "Вернуться"},
    {Text::UpdateBackOut, "The channel is «Release»: back to the release {1}?",
     "Вибрано канал «Реліз»: повернутися на реліз {1}?", "Выбран канал «Релиз»: вернуться на релиз {1}?"},
    {Text::UpdateBackStay, "Stay on the beta", "Лишитися на беті", "Остаться на бете"},
    {Text::UpdateBackRestart, "The game closes, goes back to the release and starts again - about 20 seconds.",
     "Гра закриється, повернеться на реліз і запуститься знову — це близько 20 секунд.",
     "Игра закроется, вернётся на релиз и запустится снова — это около 20 секунд."},
    {Text::ReportOriginalsSending, "Sending the shots in full quality…", "Надсилаю знімки в повній якості…",
     "Отправляю снимки в полном качестве…"},
    {Text::ReportOriginalsFailed,
     "Report {1} is sent; the shots in full quality did not go ({2}) - it has the smaller ones.",
     "Звернення {1} надіслано; знімки в повній якості не пішли ({2}) — у ньому лишилися зменшені.",
     "Обращение {1} отправлено; снимки в полном качестве не ушли ({2}) — в нём остались уменьшенные."},
    {Text::TestRow, "Graphics test: 105 s, the result goes to the developers",
     "Тест графіки: 105 с, результат піде розробникам", "Тест графики: 105 с, результат уйдёт разработчикам"},
    {Text::TestRunning, "Graphics test: {1} ({2} of {3}), {4} s left. Keep still; ~ stops it",
     "Тест графіки: {1} ({2} з {3}), ще {4} с. Не рухайтеся; ~ — зупинити",
     "Тест графики: {1} ({2} из {3}), ещё {4} с. Не двигайтесь; ~ — остановить"},
    {Text::TestNeedsMap, "Graphics test: enter a map first", "Тест графіки: спершу увійдіть на карту",
     "Тест графики: сначала войдите на карту"},
    {Text::TestStopped, "Graphics test stopped - nothing was sent", "Тест графіки зупинено — нічого не надіслано",
     "Тест графики остановлен — ничего не отправлено"},
    {Text::TestSending, "Graphics test done - sending the result…", "Тест графіки завершено — надсилаю результат…",
     "Тест графики завершён — отправляю результат…"},
    {Text::TestSent, "Graphics test sent: {1}. Thank you! It is in the settings app's «Reports» too.",
     "Тест графіки надіслано: {1}. Дякуємо! Він є й у «Зверненнях» утиліти.",
     "Тест графики отправлен: {1}. Спасибо! Он есть и в «Обращениях» утилиты."},
    {Text::TestNotSent, "Graphics test not sent ({1}); the result is in uaro-graphics\\graphics-test.txt",
     "Тест графіки не надіслано ({1}); результат — у uaro-graphics\\graphics-test.txt",
     "Тест графики не отправлен ({1}); результат — в uaro-graphics\\graphics-test.txt"},
    {Text::TestNoApp, "Graphics test done; the result is in uaro-graphics\\graphics-test.txt",
     "Тест графіки завершено; результат — у uaro-graphics\\graphics-test.txt",
     "Тест графики завершён; результат — в uaro-graphics\\graphics-test.txt"},
    {Text::TestTitle, "Graphics test", "Тест графіки", "Тест графики"},
    {Text::TestColSet, "Set", "Набір", "Набор"},
    {Text::TestColFps, "FPS", "FPS", "FPS"},
    {Text::TestColSlow, "slowest tenth, FPS", "найповільніша десята, FPS", "самая медленная десятая, FPS"},
    {Text::TestColCard, "card, ms", "відеокарта, мс", "видеокарта, мс"},
    {Text::TestColDearest, "dearest on the card, ms", "найдорожче на відеокарті, мс", "дороже всего на видеокарте, мс"},
    {Text::TestSetOff, "No effects", "Без ефектів", "Без эффектов"},
    {Text::TestSetOwn, "Your settings", "Ваші налаштування", "Ваши настройки"},
    {Text::TestLimit, "FPS limit {1}", "Обмеження FPS {1}", "Ограничение FPS {1}"},
    {Text::TestNoLimit, "no FPS limit", "без обмеження FPS", "без ограничения FPS"},
    {Text::TestVsync, "VSync", "VSync", "VSync"},
    {Text::TestNotMeasured, "not measured", "не виміряно", "не измерено"},
    {Text::ReportKindTest, "Graphics test", "Тест графіки", "Тест графики"},
    {Text::TestWaitMap, "Graphics test: it begins 10 s after you enter a map",
     "Тест графіки почнеться за 10 с після входу на карту", "Тест графики начнётся через 10 с после входа на карту"},
    {Text::TestCountdown, "Graphics test begins in {1} s: stand somewhere quiet and keep still. ~ cancels it",
     "Тест графіки почнеться за {1} с: станьте в спокійному місці й не рухайтеся. ~ — скасувати",
     "Тест графики начнётся через {1} с: встаньте в спокойном месте и не двигайтесь. ~ — отменить"},
    {Text::TestCancelled, "Graphics test cancelled", "Тест графіки скасовано", "Тест графики отменён"},
    {Text::PlayTestNote,
     "105 seconds: the game itself shows each effects set and sends the developers how it runs on this computer. The "
     "game starts, and the test begins 10 seconds after you enter a map - stand somewhere quiet.",
     "105 секунд: гра сама показує кожен набір ефектів і надсилає розробникам, як він іде на цьому комп'ютері. Гра "
     "запуститься, а тест почнеться за 10 секунд після входу на карту — станьте в спокійному місці.",
     "105 секунд: игра сама показывает каждый набор эффектов и отправляет разработчикам, как он идёт на этом "
     "компьютере. Игра запустится, а тест начнётся через 10 секунд после входа на карту — встаньте в спокойном месте."},
    {Text::PlayTestButton, "Run the graphics test", "Запустити тест графіки", "Запустить тест графики"},
    {Text::PlayTestRunning, "The game is running: open its console (~) and choose «Graphics test».",
     "Гра вже запущена: відкрийте в ній консоль (~) і виберіть «Тест графіки».",
     "Игра уже запущена: откройте в ней консоль (~) и выберите «Тест графики»."},
    {Text::PlayTestLast, "The last test, {1}:", "Останній тест, {1}:", "Последний тест, {1}:"},
    {Text::PlayTestSent, "Sent to the developers: {1}.", "Надіслано розробникам: {1}.",
     "Отправлено разработчикам: {1}."},
    {Text::PlayTestNotSent, "Not sent: {1}.", "Не надіслано: {1}.", "Не отправлено: {1}."},
    {Text::PlayTestNone, "No graphics test on this computer yet.", "На цьому комп'ютері тесту графіки ще не було.",
     "На этом компьютере теста графики ещё не было."},
    {Text::TestSetMediumNoRelief, "Medium without relief", "Середні без рельєфу", "Средние без рельефа"},
    {Text::TestSetMediumNoLight, "Medium without smooth light", "Середні без згладжування світла",
     "Средние без сглаживания света"},
    {Text::TestCosts, "The card's cost within «High», ms: {1}", "Ціна у «Високих» на відеокарті, мс: {1}",
     "Цена в «Высоких» на видеокарте, мс: {1}"},
    {Text::TestCostsMedium, "The card's cost within «Medium», ms: {1}", "Ціна у «Середніх» на відеокарті, мс: {1}",
     "Цена в «Средних» на видеокарте, мс: {1}"},
    // 3.0.9-beta.4: the developer's answers where the player is
    {Text::AnswerStrip, "An answer to the report {1} · {2} — {3}", "Є відповідь на звернення {1} · {2} — {3}",
     "Есть ответ на обращение {1} · {2} — {3}"},
    {Text::AnswerWaits, "the developer waits for your answer.", "розробник чекає на вашу відповідь.",
     "разработчик ждёт вашего ответа."},
    {Text::AnswerUpdate, "update and check.", "оновіться й перевірте.", "обновитесь и проверьте."},
    {Text::AnswerNew, "a new answer from the developer.", "нова відповідь розробника.", "новый ответ разработчика."},
    {Text::AnswerOpen, "Open", "Відкрити", "Открыть"},
    {Text::CardTitle, "Answer to the report", "Відповідь на звернення", "Ответ на обращение"},
    {Text::CardLater, "Later", "Пізніше", "Позже"},
    {Text::CardFrom, "Developer · {1}", "Розробник · {1}", "Разработчик · {1}"},
    {Text::CardWaits, "The developer waits for your answer.", "Розробник чекає на вашу відповідь.",
     "Разработчик ждёт вашего ответа."},
    {Text::CardUpdate, "Fixed in {1} — update and check.", "Виправлено в {1} — оновіться й перевірте.",
     "Исправлено в {1} — обновитесь и проверьте."},
    {Text::CardNew, "Open it to read the answer.", "Відкрийте, щоб прочитати відповідь.",
     "Откройте, чтобы прочитать ответ."},
    {Text::NotifyAnswer, "Answers to reports", "Відповіді на звернення", "Ответы на обращения"},
    {Text::NotifyAnswerHint,
     "The developer answered your report: told while the game is behind other windows; in front, a card in the game "
     "tells of it.",
     "Розробник відповів на ваше звернення: сповіщення, коли гра за іншими вікнами; коли гра попереду, про це скаже "
     "картка в самій грі.",
     "Разработчик ответил на ваше обращение: уведомление, когда игра за другими окнами; когда игра впереди, об этом "
     "скажет карточка в самой игре."},
    {Text::NotifyAnswerText, "Answer to the report {1}: {2}", "Відповідь на звернення {1}: {2}",
     "Ответ на обращение {1}: {2}"},
    // 3.0.9-beta.5: «Звернення» as a messenger
    {Text::TabOpen, "Open · {1}", "Відкриті · {1}", "Открытые · {1}"},
    {Text::TabClosed, "Closed · {1}", "Закриті · {1}", "Закрытые · {1}"},
    {Text::TalkDevRead, "The developer read it · {1}", "Розробник прочитав · {1}", "Разработчик прочитал · {1}"},
    {Text::TalkSend, "Send · Ctrl+Enter", "Надіслати · Ctrl+Enter", "Отправить · Ctrl+Enter"},
    {Text::TalkPaste, "Paste · Ctrl+V", "Вставити · Ctrl+V", "Вставить · Ctrl+V"},
    {Text::TalkFacts, "{1} · driver {2} · sent {3}", "{1} · драйвер {2} · надіслано {3}",
     "{1} · драйвер {2} · отправлено {3}"},
    {Text::ListWaitsDev, "Waits for the developer", "Чекає на розробника", "Ждёт разработчика"},
    {Text::TalkWaitsBar, "The developer waits for your answer: {1}", "Розробник чекає на вашу відповідь: {1}",
     "Разработчик ждёт вашего ответа: {1}"},
    {Text::TalkAutomatic, "automatic", "автоматичне", "автоматическое"},
    {Text::StepFixed, "Fixed", "Виправлено", "Исправлено"},
    // 3.0.9-beta.6: «Нове звернення» friendlier
    {Text::NewTitle, "What happened?", "Що трапилося?", "Что случилось?"},
    {Text::NewSubtitle,
     "Tell it in your own words - the developer answers here, and we show the answer in the game too.",
     "Опишіть своїми словами — розробник відповість тут, а відповідь ми покажемо й у грі.",
     "Опишите своими словами — разработчик ответит здесь, а ответ мы покажем и в игре."},
    {Text::NewPlaceholder, "For example: a black screen after login · slow in Prontera · pink strips on the bridge",
     "Наприклад: після входу чорний екран · у Пронтері гальмує · на мосту рожеві смуги",
     "Например: после входа чёрный экран · в Пронтере тормозит · на мосту розовые полосы"},
    {Text::NewKindLabel, "What is it about? (you may leave it)", "Про що це? (можна не вибирати)",
     "О чём это? (можно не выбирать)"},
    {Text::NewShots, "Screenshots · {1} of 6", "Знімки · {1} з 6", "Снимки · {1} из 6"},
    {Text::NewShotGame, "+ Game screenshot…", "+ Знімок гри…", "+ Снимок игры…"},
    {Text::NewShotRemove, "Remove", "Прибрати", "Убрать"},
    {Text::NewPrivacy, "The game's settings and logs go with it - no login, no character, no user name.",
     "Разом підуть налаштування й журнали гри — без логіна, персонажа й імені користувача.",
     "Вместе уйдут настройки и журналы игры — без логина, персонажа и имени пользователя."},
    {Text::NewPrivacyMore, "What exactly?", "Що саме?", "Что именно?"},
    {Text::NewSendHint, "Ctrl+Enter - send", "Ctrl+Enter — надіслати", "Ctrl+Enter — отправить"},
    {Text::NewMapFromGame, "Map: {1} - from the game", "Карта: {1} — додано з гри", "Карта: {1} — добавлено из игры"},
    {Text::SentTitle, "Thank you! The report is sent", "Дякуємо! Звернення надіслано", "Спасибо! Обращение отправлено"},
    {Text::SentBody,
     "The developer answers here. When the answer comes, you see it in the game too - as a card at the top of the "
     "screen.",
     "Розробник відповість тут. Коли відповідь прийде, ви побачите її й у грі — карткою вгорі екрана.",
     "Разработчик ответит здесь. Когда ответ придёт, вы увидите его и в игре — карточкой вверху экрана."},
    {Text::SentOpen, "Open the conversation", "Відкрити переписку", "Открыть переписку"},
    {Text::SentDone, "Done", "Готово", "Готово"},
    {Text::QueuedTitle, "Kept - it goes by itself", "Збережено — піде само", "Сохранено — уйдёт само"},
    {Text::QueuedBody,
     "There is no connection with uaro.kiev.ua now. The report waits on this computer and goes by itself once there "
     "is internet - nothing to do.",
     "Зараз немає зв'язку з uaro.kiev.ua. Звернення чекає на цьому комп'ютері й піде само, щойно з'явиться інтернет — "
     "нічого робити не треба.",
     "Сейчас нет связи с uaro.kiev.ua. Обращение ждёт на этом компьютере и уйдёт само, как только появится интернет — "
     "ничего делать не нужно."},
    {Text::PickerTitle, "Game screenshots of the last week", "Знімки гри за тиждень", "Снимки игры за неделю"},
    {Text::PickerAdd, "Add", "Додати", "Добавить"},
    // 3.0.11-beta.5: the game's reference pack ({1} the server's data day, {2} its commit, {3} KB)
    {Text::ReferenceThere,
     "Game reference: the server's data of {1} (commit {2}), {3} KB, from uaro.kiev.ua - it updates by itself when "
     "the game starts.",
     "Довідник гри: дані сервера від {1} (коміт {2}), {3} КБ з uaro.kiev.ua - оновлюється сам під час запуску гри.",
     "Справочник игры: данные сервера от {1} (коммит {2}), {3} КБ с uaro.kiev.ua - обновляется сам при запуске "
     "игры."},
    {Text::ReferenceNone,
     "Game reference: not here yet - it comes from uaro.kiev.ua when the game starts with the new minimap on.",
     "Довідник гри ще не завантажено: він приходить з uaro.kiev.ua під час запуску гри з увімкненою новою мінікартою.",
     "Справочник игры ещё не загружен: он приходит с uaro.kiev.ua при запуске игры с включённой новой миникартой."},
    // 3.0.11-beta.6: the minimap's hints from the game's reference
    {Text::InterfaceMinimapNpcs, "NPC marks on the minimap", "Мітки NPC на мінікарті", "Метки NPC на миникарте"},
    {Text::InterfaceMinimapNpcsHint,
     "Dots in the colours of the site's map: healers, shops, smiths, armour, inns, guides, job masters, ships (the "
     "Kafras - a switch of their own). The pointer over a dot names it. From the game's reference (uaro.kiev.ua).",
     "Крапки в кольорах карти сайту: лікарі, крамниці, ковалі, броня, заїжджі двори, провідники, майстри професій, "
     "кораблі (Кафра - окремий перемикач). Курсор на крапці називає NPC. Дані - з довідника гри (uaro.kiev.ua).",
     "Точки в цветах карты сайта: лекари, лавки, кузнецы, броня, постоялые дворы, проводники, мастера профессий, "
     "корабли (Кафра - отдельный переключатель). Курсор на точке называет NPC. Данные - из справочника игры "
     "(uaro.kiev.ua)."},
    {Text::InterfaceMinimapWarps, "Warps on the minimap", "Переходи на мінікарті", "Переходы на миникарте"},
    {Text::InterfaceMinimapWarpsHint,
     "Red rings where a warp leads to another map; the pointer over a ring names that map.",
     "Червоні кільця там, де перехід веде на іншу карту; курсор на кільці називає ту карту.",
     "Красные кольца там, где переход ведёт на другую карту; курсор на кольце называет ту карту."},
    {Text::InterfaceMinimapMobs, "Monsters under the minimap", "Монстри під мінікартою", "Монстры под миникартой"},
    {Text::InterfaceMinimapMobsHint,
     "The map's monsters and how many of them the server keeps here; ★ - a boss and its respawn after death.",
     "Монстри цієї карти і скільки їх тримає тут сервер; ★ - бос і його респавн після смерті.",
     "Монстры этой карты и сколько их держит здесь сервер; ★ - босс и его респавн после смерти."},
    // 3.0.11-beta.119 (R-5Z6J2S): the map's layers
    {Text::InterfaceMinimapKafra, "Kafra on the maps", "Кафра на картах", "Кафра на картах"},
    {Text::InterfaceMinimapKafraHint,
     "The Kafras' pink K on the minimap and the big map, even with the other NPCs hidden. The maps' own buttons "
     "switch it too: the round ones in the minimap's top left, the chips under the big map.",
     "Рожеві літери K Кафри на мінікарті й великій карті, навіть коли інших NPC сховано. Перемикають і кнопки самих "
     "карт: круглі в лівому верхньому куті мінікарти, плашки під великою картою.",
     "Розовые буквы K Кафры на миникарте и большой карте, даже когда другие NPC скрыты. Переключают и кнопки самих "
     "карт: круглые в левом верхнем углу миникарты, плашки под большой картой."},
    {Text::InterfaceMinimapParty, "The party on the maps", "Група на картах", "Группа на картах"},
    {Text::InterfaceMinimapPartyHint,
     "The party members' faces on the minimap and the big map; the maps' own buttons switch them too.",
     "Обличчя учасників групи на мінікарті й великій карті; перемикають і кнопки самих карт.",
     "Лица участников группы на миникарте и большой карте; переключают и кнопки самих карт."},
    {Text::MinimapLayerWarps, "Portals", "Портали", "Порталы"},
    {Text::MinimapLayerNpcs, "NPC", "NPC", "NPC"},
    {Text::MinimapLayerKafra, "Kafra", "Кафра", "Кафра"},
    {Text::MinimapLayerParty, "Party and guild", "Група й гільдія", "Группа и гильдия"},  // .219: both
    // 3.0.11-beta.120: the maps' picture
    {Text::InterfaceMinimapStyle, "The maps' picture", "Картинка карт", "Картинка карт"},
    {Text::InterfaceMinimapStyleHint,
     "What the minimap and the big map show under their marks: the game's own picture of the map, or a scheme drawn "
     "from the server's walkable cells - the floor light (the part you can walk to lighter still), the walls dark. "
     "The maps' own button switches it too: the last round one in the minimap's top left, «Scheme» under the big map.",
     "Що показують мінікарта й велика карта під мітками: картинку карти з самої гри або схему, намальовану з клітинок, "
     "якими можна ходити на сервері, - підлога світла (куди можна дійти - ще світліша), стіни темні. Перемикає й "
     "кнопка самих карт: остання кругла в лівому верхньому куті мінікарти, «Схема» під великою картою.",
     "Что показывают миникарта и большая карта под метками: картинку карты из самой игры или схему, нарисованную из "
     "клеток, по которым можно ходить на сервере, - пол светлый (куда можно дойти - ещё светлее), стены тёмные. "
     "Переключает и кнопка самих карт: последняя круглая в левом верхнем углу миникарты, «Схема» под большой картой."},
    {Text::MinimapStylePicture, "The game's picture", "Картинка гри", "Картинка игры"},
    {Text::MinimapStyleScheme, "Scheme", "Схема", "Схема"},
    {Text::MinimapLayerScheme, "Scheme", "Схема", "Схема"},
    // 3.0.11-beta.122: the round trip's badge
    {Text::InterfaceNetBadge, "Ping on the minimap", "Пінг на мінікарті", "Пинг на миникарте"},
    {Text::InterfaceNetBadgeHint,
     "The time to the game's server and back on a small plate under the minimap's bar, with a graph of the last "
     "minute (a bar each second), measured by Windows on the game's own connection: green under 120 ms, amber under "
     "250 ms, red slower or when packets were lost; «stall» in red - the connection waited for a lost packet (the "
     "world jumps after it: that was the network, not the game).",
     "Час до сервера гри й назад на невеликій плашці під смугою мінікарти, з графіком за останню хвилину (стовпчик "
     "щосекунди), як його міряє Windows на з'єднанні самої гри: зелений - до 120 мс, бурштиновий - до 250 мс, "
     "червоний - повільніше або коли пакети губилися; «обрив» червоним - з'єднання чекало на загублений пакет (світ "
     "після нього стрибає: це була мережа, не гра).",
     "Время до сервера игры и обратно на небольшой плашке под полосой миникарты, с графиком за последнюю минуту "
     "(столбик каждую секунду), как его меряет Windows на соединении самой игры: зелёный - до 120 мс, янтарный - до "
     "250 мс, красный - медленнее или когда пакеты терялись; «обрыв» красным - соединение ждало потерянный пакет "
     "(мир после него прыгает: это была сеть, не игра)."},
    {Text::NetMs, "{1} ms", "{1} мс", "{1} мс"},
    {Text::NetStall, "stall", "обрив", "обрыв"},
    // 3.0.11-beta.123: the position fixes glided to
    {Text::InterfaceSmoothFixes, "Smooth position fixes", "Плавні поправки позиції", "Плавные поправки позиции"},
    {Text::InterfaceSmoothFixesHint,
     "The server often puts a monster or your character a cell or two from where the game, a moment behind it, shows "
     "them - before each attack of a monster that chased you, when one casts on the move, after a lag. The game "
     "jumps them there; with this on they glide there in a fifth of a second. Farther moves (a teleport, a "
     "knock-back) stay instant.",
     "Сервер часто ставить монстра чи вашого персонажа за клітинку-дві від місця, де гра, що трохи відстає, їх "
     "показує, - перед кожною атакою монстра, який вас наздогнав, коли він кастує на ходу, після лагу. Гра ривком "
     "переносить їх туди; з цим перемикачем вони доїжджають туди за п'яту частку секунди. Далекі переміщення "
     "(телепорт, відкидання) лишаються миттєвими.",
     "Сервер часто ставит монстра или вашего персонажа в клетке-двух от места, где игра, немного отставая, их "
     "показывает, - перед каждой атакой догнавшего вас монстра, когда он кастует на ходу, после лага. Игра рывком "
     "переносит их туда; с этим переключателем они доезжают туда за пятую долю секунды. Дальние перемещения "
     "(телепорт, отбрасывание) остаются мгновенными."},
    // 3.0.11-beta.130 (the network plan's step 5; the owner «делаем»)
    {Text::InterfacePredictWalk, "Walk without waiting for the server (an experiment)",
     "Хода без очікування сервера (експеримент)", "Ходьба без ожидания сервера (эксперимент)"},
    {Text::InterfacePredictWalkHint,
     "The character starts walking at the click instead of after the server's answer - a ping sooner. When the "
     "server does not let it walk (a cast, a stun) or answers too late, the character is put back. Off at first.",
     "Персонаж починає йти одразу після кліку, а не після відповіді сервера, — на один пінг раніше. Якщо сервер не "
     "дозволить піти (каст, оглушення) або відповість надто пізно, персонажа поверне назад. Спочатку вимкнено.",
     "Персонаж начинает идти сразу после клика, а не после ответа сервера, — на один пинг раньше. Если сервер не "
     "даст пойти (каст, оглушение) или ответит слишком поздно, персонажа вернёт назад. Сначала выключено."},
    // 3.0.11-beta.125 (the owner: «когда играю арчером, есть стрелы, и не видно сколько осталось»)
    {Text::InterfaceHotbarAmmo, "Ammo left on the skill bar", "Залишок стріл на панелі навичок",
     "Остаток стрел на панели навыков"},
    {Text::InterfaceHotbarAmmoHint,
     "For archers and everyone who wears ammo: a small plate over the skill bar's left end with the ammo's picture "
     "and how many are left - amber under 50, red at 10 or fewer, with a short warning when it falls under those "
     "and when none are left.",
     "Для лучників і всіх, хто носить боєприпаси: невелика плашка над лівим краєм панелі навичок з картинкою "
     "боєприпасу і скільки залишилося — бурштинова, коли менше 50, червона, коли 10 чи менше, з коротким "
     "попередженням, коли їх стає менше, і коли вони скінчилися.",
     "Для лучников и всех, кто носит боеприпасы: небольшая плашка над левым краем панели навыков с картинкой "
     "боеприпаса и сколько осталось — янтарная, когда меньше 50, красная, когда 10 или меньше, с коротким "
     "предупреждением, когда их становится меньше, и когда они закончились."},
    // 3.0.16-beta.8 (the owner: «для магов и пристов показывать количество гемов, а для монков ... сферы»)
    {Text::InterfaceHotbarSupplies, "Gems and spheres on the skill bar", "Камені та сфери на панелі навичок",
     "Камни и сферы на панели навыков"},
    {Text::InterfaceHotbarSuppliesHint,
     "Next to the ammo's plate: what the skills on your bar spend - gems, Holy Water, the Sage's and the Ninja's "
     "stones, the Alchemist's bottles - with how many are in the bag (red edge at 10 or fewer, a short chime then and "
     "when none are left), and the Monk's spirit spheres or the Gunslinger's coins as dots. Off at first.",
     "Поруч із плашкою стріл: що витрачають навички з вашої панелі — камені, святу воду, камені мудреця й ніндзя, "
     "пляшки алхіміка — і скільки їх у сумці (червоний край, коли 10 чи менше, короткий сигнал тоді й коли вони "
     "скінчилися), а сфери монаха чи монети ганслінгера — крапками. Спочатку вимкнено.",
     "Рядом с плашкой стрел: что тратят навыки с вашей панели — камни, святую воду, камни мудреца и ниндзя, бутылки "
     "алхимика — и сколько их в сумке (красный край, когда 10 или меньше, короткий сигнал тогда и когда они "
     "закончились), а сферы монаха или монеты ганслингера — точками. Сначала выключено."},
    {Text::HotbarAmmoLow, "{1} running low", "{1} закінчуються", "{1} заканчиваются"},
    {Text::HotbarAmmoOut, "{1} ran out", "{1} скінчилися", "{1} закончились"},
    // 3.0.11-beta.129 (the owner «делай»)
    {Text::InterfaceAmmoSound, "A sound when the arrows run low", "Звук, коли стріли закінчуються",
     "Звук, когда стрелы заканчиваются"},
    {Text::InterfaceAmmoSoundHint,
     "A short soft chime when 10 arrows (bullets, shuriken) are left and a lower one when none are left - as loud as "
     "the game's sounds, none while the game's sound is off.",
     "Короткий тихий сигнал, коли лишається 10 стріл (куль, сюрикенів), і нижчий, коли вони скінчилися, — гучність як "
     "у звуків гри, з вимкненим звуком гри сигналу немає.",
     "Короткий тихий сигнал, когда остаётся 10 стрел (пуль, сюрикенов), и более низкий, когда они закончились, — "
     "громкость как у звуков игры, при выключенном звуке игры сигнала нет."},
    {Text::InterfaceElementColours, "Element things in their colour",
     "Стихійні речі своїм кольором",
     "Стихийные вещи своим цветом"},
    {Text::InterfaceElementColoursHint,
     "A weapon or armour with an element (Fire Brand, a forged wind knife, an armour with Pasana Card) shows its "
     "element's colour - its name in the lists and the tip, a line in its cell.",
     "Зброя чи броня зі стихією (Fire Brand, кований ніж вітру, "
     "броня з Pasana Card) має колір своєї стихії — назва в списках "
     "і підказці, риска в клітинці.",
     "Оружие или броня со стихией (Fire Brand, кованый нож ветра, "
     "броня с Pasana Card) имеет цвет своей стихии — название в "
     "списках и подсказке, черта в ячейке."},
    // 3.0.11-beta.139 (the owner: «пишет что 60% но хп воостанавливается»): the server's overweight states
    {Text::CharOverweight1, "Overweight: HP and SP do not recover", "Перевантаження: HP і SP не відновлюються",
     "Перегруз: HP и SP не восстанавливаются"},
    {Text::CharOverweight2, "Overweight 90 %: no attacks or skills", "Перевантаження 90 %: атаки й уміння заборонені",
     "Перегруз 90 %: атаки и умения запрещены"},
    // 3.0.11-beta.65: the concept's «Подсветка, куда можно дойти» - an experiment
    {Text::InterfaceMinimapReach, "Light where you can walk (an experiment)",
     "Підсвітити, куди можна дійти (експеримент)", "Подсветить, куда можно дойти (эксперимент)"},
    {Text::InterfaceMinimapReachHint,
     "The new minimap and its big map dim what you cannot reach on foot from where you stand - other parts of the "
     "map (floors, islands, ledges) by the server's own map data. An experiment: people, monsters and traps are not "
     "counted; after a teleport it shows once the game knows your cell.",
     "Нова мінікарта та її велика карта притемнюють усе, куди звідси не дійти пішки, — інші частини карти (поверхи, "
     "острівці, уступи) за даними самого сервера. Експеримент: люди, монстри й пастки не враховані; після телепорту "
     "підсвітка з'являється, щойно гра знає вашу клітинку.",
     "Новая миникарта и её большая карта затемняют всё, куда отсюда не дойти пешком, — другие части карты (этажи, "
     "островки, уступы) по данным самого сервера. Эксперимент: люди, монстры и ловушки не учтены; после телепорта "
     "подсветка появляется, как только игра знает вашу клетку."},
    // 3.0.11-beta.72 (the owner: «сами картинки карт маленькие и не аккуратные»)
    {Text::InterfaceMinimapHd, "Sharp map pictures", "Чіткі картинки карт", "Чёткие картинки карт"},
    {Text::InterfaceMinimapHdHint,
     "The new minimap and its big map take the map's picture four times bigger when the game has one - from the AI "
     "texture pack (uaro-textures.zip) or the old client's texture_x4.zip. Off: the game's own picture, 512 x 512.",
     "Нова мінікарта та її велика карта беруть картинку карти вчетверо більшу, якщо вона є в грі, — з пака текстур "
     "ШІ (uaro-textures.zip) або з texture_x4.zip старого клієнта. Вимкнено: власна картинка гри, 512 x 512.",
     "Новая миникарта и её большая карта берут картинку карты в четыре раза больше, если она есть в игре, — из пака "
     "текстур ИИ (uaro-textures.zip) или из texture_x4.zip старого клиента. Выключено: собственная картинка игры, "
     "512 x 512."},
    // 3.0.11-beta.74 (the owner: «иконки скилов, предметов, и всего остального также по возможности грузить в текстур
    // которые обработаны»)
    {Text::InterfacePackIcons, "Icons from the AI texture pack", "Іконки з пака текстур ШІ",
     "Иконки из пака текстур ИИ"},
    {Text::InterfacePackIconsHint,
     "The new interface's icons and pictures - skills, items, status effects, monsters - drawn from the AI texture "
     "pack's pictures, four times bigger than the game's, wherever the pack has them (uaro-textures.zip). Off: the "
     "game's own small ones.",
     "Іконки й картинки нового інтерфейсу — навички, предмети, ефекти, монстри — малюються з картинок пака текстур ШІ, "
     "вчетверо більших за ігрові, там, де пак їх має (uaro-textures.zip). Вимкнено: власні маленькі картинки гри.",
     "Иконки и картинки нового интерфейса — навыки, предметы, эффекты, монстры — рисуются из картинок пака текстур "
     "ИИ, в четыре раза больше игровых, там, где пак их имеет (uaro-textures.zip). Выключено: собственные маленькие "
     "картинки игры."},
    // 3.0.11-beta.75 (the owner: «некоторые интерфейсы хотелось бы чтобы можно было бы двигать и настраивать»)
    {Text::HudEditTitle, "Interface layout", "Розміщення інтерфейсу", "Размещение интерфейса"},
    {Text::HudEditHint, "Drag a part with the mouse \xC2\xB7 wheel: its size \xC2\xB7 right click: as it was",
     "Тягніть частину мишею \xC2\xB7 колесо — розмір \xC2\xB7 правий клік — як було",
     "Тяните часть мышью \xC2\xB7 колесо — размер \xC2\xB7 правый клик — как было"},
    {Text::HudEditReset, "Reset all", "Скинути все", "Сбросить всё"},
    {Text::HudEditDone, "Done", "Готово", "Готово"},
    {Text::HudPartCharacter, "Character block", "Блок персонажа", "Блок персонажа"},
    {Text::HudPartMinimap, "Minimap", "Мінікарта", "Миникарта"},
    {Text::HudPartBuffs, "Buffs", "Бафи", "Бафы"},
    {Text::HudPartTracker, "Tracking", "Відстеження", "Отслеживание"},
    {Text::HudPartHotbar, "Hotbar", "Панель навичок", "Панель навыков"},
    {Text::HudPartTarget, "Target card", "Картка цілі", "Карточка цели"},
    {Text::HudPartParty, "Party", "Група", "Группа"},
    {Text::HudPartChat, "Chat", "Чат", "Чат"},
    {Text::HudPartHunt, "Hunt", "Полювання", "Охота"},  // 3.0.11-beta.278
    {Text::HudPartMvp, "MVP", "MVP", "MVP"},  // 3.0.11-beta.280
    {Text::ConsoleHudEdit, "Arrange the interface (Ctrl+Shift+L)", "Розмістити інтерфейс (Ctrl+Shift+L)",
     "Разместить интерфейс (Ctrl+Shift+L)"},
    {Text::InterfaceLayoutNote,
     "The parts' places and sizes: in the game drag a part with the mouse (a click stays a click), the grip at its "
     "corner sizes it, twice on the grip puts it back; Ctrl+Shift+L shows them all framed.",
     "Місця й розміри частин: у грі потягніть частину мишею (клік лишається кліком), куточок біля неї змінює розмір, "
     "подвійний клік по куточку повертає її на місце; Ctrl+Shift+L показує їх усі в рамках.",
     "Места и размеры частей: в игре потяните часть мышью (клик остаётся кликом), уголок рядом с ней меняет размер, "
     "двойной клик по уголку возвращает её на место; Ctrl+Shift+L показывает их все в рамках."},
    {Text::HudEditUnseen, "not shown now", "зараз не видно", "сейчас не видно"},
    // 3.0.11-beta.78 (the owner: «на панели скилов не те скилы появляются, с других аккаунтов, плюс появляются те
    // которых на чаре нету»)
    {Text::InterfaceHotbarOwn, "Each character's own skill bar", "Своя панель навичок у кожного персонажа",
     "Своя панель навыков у каждого персонажа"},
    {Text::InterfaceHotbarOwnHint,
     "The game keeps one hotkeys bar for every account's character in the same place of the selection screen, so "
     "skills of other characters turn up. The driver keeps each character's own bar by its name and puts it back when "
     "the character comes into the game; met the first time, it keeps what the game had without the skills the "
     "character has not learned.",
     "Гра тримає одну панель клавіш для персонажів усіх акаунтів на тому самому місці екрана вибору, тому з'являються "
     "навички інших персонажів. Драйвер зберігає власну панель кожного персонажа за його ім'ям і повертає її, коли "
     "персонаж входить у гру; уперше — лишає те, що було в грі, без навичок, яких персонаж не має.",
     "Игра держит одну панель клавиш для персонажей всех аккаунтов на том же месте экрана выбора, поэтому появляются "
     "навыки других персонажей. Драйвер хранит собственную панель каждого персонажа по его имени и возвращает её, "
     "когда персонаж входит в игру; впервые — оставляет то, что было в игре, без навыков, которых у персонажа нет."},
    {Text::HotbarUnlearned, "The character does not have this skill", "Персонаж не має цієї навички",
     "У персонажа нет этого навыка"},
    {Text::PartyTitle, "Party", "Група", "Группа"},  // 3.0.11-beta.79: the party panel's header
    {Text::PartyDead, "dead", "мертвий", "мёртв"},  // 3.0.11-beta.81: a party member's
    {Text::InterfacePadHints, "Controller's buttons on the interface", "Кнопки контролера на інтерфейсі",
     "Кнопки контроллера на интерфейсе"},  // 3.0.11-beta.86
    {Text::InterfacePadHintsHint,
     "While you play with a controller, its buttons show on the skill bar and the target's card instead of the keys' "
     "names; a click, a key or the mouse moved brings the keys back.",
     "Поки граєте з контролером, його кнопки видно на панелі навичок і в картці цілі замість назв клавіш; клік, "
     "клавіша чи рух миші повертають клавіші.",
     "Пока играете с контроллером, его кнопки видны на панели навыков и в карточке цели вместо названий клавиш; клик, "
     "клавиша или движение мыши возвращают клавиши."},
    {Text::TargetPadAttack, "Attack", "Атака", "Атака"},  // 3.0.11-beta.86: the target card's A
    {Text::TargetPadNext, "Another target", "Інша ціль", "Другая цель"},  // its right stick
    {Text::InterfaceTargetRing, "Target's ring on the ground", "Кільце цілі на землі",
     "Кольцо цели на земле"},  // 3.0.11-beta.87
    {Text::InterfaceTargetRingHint,
     "A gold ring under the monster the controller aims at, a chevron over its head; a dashed ring under the next one "
     "while the right stick is tilted lightly, a green one under the party member a heal is to go on. «Always» - also "
     "under the monster the character fights with the mouse.",
     "Золоте кільце під монстром, у якого цілиться контролер, і стрілка над ним; пунктирне — під наступним, поки "
     "правий стік трохи нахилений, зелене — під учасником групи, на якого піде лікування. «Завжди» — також під "
     "монстром, якого персонаж б'є мишею.",
     "Золотое кольцо под монстром, в которого целится контроллер, и стрелка над ним; пунктирное — под следующим, пока "
     "правый стик слегка наклонён, зелёное — под участником группы, на которого пойдёт лечение. «Всегда» — также под "
     "монстром, которого персонаж бьёт мышью."},
    {Text::InterfaceTargetRingOff, "Off", "Вимкнено", "Выключено"},
    {Text::InterfaceTargetRingPad, "With the controller", "З контролером", "С контроллером"},
    {Text::InterfaceTargetRingAlways, "Always", "Завжди", "Всегда"},
    {Text::InterfaceMobHp, "Monsters' HP bars", "Смужки HP монстрів", "Полоски HP монстров"},  // 3.0.11-beta.95
    {Text::InterfaceMobHpHint,  // 3.0.11-beta.99: with the mouse too; 3.0.11-beta.100: every monster's by default
     "A thin bar over a monster's head from the per cent the server sends to everyone near on each hit: green, red "
     "below a quarter. With the mouse and with the controller alike: the target (the one the character fights or the "
     "controller chose) and the monster under the mouse cursor have it wider, with the per cent. «All» - over every "
     "monster on the screen (a whole one's per cent asked of the server, as the game asks it when the mouse points at "
     "it); «The target and the wounded» - a wounded monster's bar for 8 seconds after its last hit.",
     "Тонка смужка над головою монстра з відсотка, який сервер надсилає всім поруч на кожен удар: зелена, нижче чверті "
     "— червона. І з мишею, і з контролером: у цілі (кого б'є персонаж або вибрав контролер) і в монстра під курсором "
     "миші вона ширша, з відсотком. «Усі» — над кожним монстром на екрані (відсоток цілого питаємо в сервера, як гра "
     "питає при наведенні миші); «Ціль і поранені» — смужка пораненого ще 8 секунд після останнього удару.",
     "Тонкая полоска над головой монстра из процента, который сервер присылает всем рядом на каждый удар: зелёная, "
     "ниже четверти — красная. И с мышью, и с контроллером: у цели (кого бьёт персонаж или выбрал контроллер) и у "
     "монстра под курсором мыши она шире, с процентом. «Все» — над каждым монстром на экране (процент целого "
     "спрашиваем у сервера, как игра спрашивает при наведении мыши); «Цель и раненые» — полоска раненого ещё 8 "
     "секунд после последнего удара."},
    {Text::InterfaceMobHpOff, "Off", "Вимкнено", "Выключено"},
    {Text::InterfaceMobHpTarget, "The target only", "Лише ціль", "Только цель"},
    {Text::InterfaceMobHpWounded, "The target and the wounded", "Ціль і поранені", "Цель и раненые"},
    {Text::InterfaceMobHpAll, "All", "Усі", "Все"},
    {Text::InterfaceMobHpPercent, "HP per cent by the target's bar", "Відсоток HP біля смужки цілі",
     "Процент HP возле полоски цели"},
    {Text::InterfaceMobHpPercentHint,
     "A small dark plate with the target's HP per cent (95 %) to the right of its bar.",
     "Маленька темна плашка з відсотком HP цілі (95 %) праворуч від її смужки.",
     "Маленькая тёмная плашка с процентом HP цели (95 %) справа от её полоски."},
    {Text::InterfaceBossBars, "Boss HP bars", "Смужки HP босів", "Полоски HP боссов"},  // 3.0.13-beta.3
    {Text::InterfaceBossBarsHint,
     "An MVP's bar wider and gold, a mini-boss's wider and violet - seen at once in a crowd.",
     "Смужка MVP ширша й золота, міні-боса — ширша й фіолетова: видно одразу навіть у натовпі.",
     "Полоска MVP шире и золотая, мини-босса — шире и фиолетовая: видно сразу даже в толпе."},
    {Text::InterfaceQuickTeleport, "Quick teleport on the map", "Швидкий телепорт на карті",
     "Быстрый телепорт на карте"},  // 3.0.13-beta.7
    {Text::InterfaceQuickTeleportHint,
     "Teleport and Fly Wing on the same map without the game's quarter-second darkening: the new place at once.",
     "Teleport і Fly Wing у межах карти без затемнення гри на чверть секунди: нове місце одразу.",
     "Teleport и Fly Wing в пределах карты без затемнения игры на четверть секунды: новое место сразу."},
    {Text::InterfaceScreenShake, "Screen shake", "Тряска екрана", "Тряска экрана"},  // 3.0.13-beta.8
    {Text::InterfaceScreenShakeHint,
     "Earthquake and similar skills shake the camera, as in the game. Off: the picture stays still.",
     "Earthquake і подібні навички трясуть камеру, як у грі. Вимкнено: картинка лишається нерухомою.",
     "Earthquake и подобные навыки трясут камеру, как в игре. Выключено: картинка остаётся неподвижной."},
    {Text::InterfaceSkillJokes, "Frost Joke and Scream lines in the chat", "Жарти Frost Joke і Scream у чат",
     "Шутки Frost Joke и Scream в чат"},  // 3.0.13-beta.8
    {Text::InterfaceSkillJokesHint,
     "Your Frost Joke and Scream say a joke line in the public chat, as in the game. Off: the skill works, the chat "
     "stays quiet.",
     "Ваші Frost Joke і Scream кажуть рядок-жарт у загальний чат, як у грі. Вимкнено: навичка працює, чат мовчить.",
     "Ваши Frost Joke и Scream говорят строку-шутку в общий чат, как в игре. Выключено: навык работает, чат молчит."},
    {Text::InterfaceSwearFilter, "The game's swear filter", "Фільтр лайки гри", "Фильтр брани игры"},  // .9
    {Text::InterfaceSwearFilterHint,
     "The game refuses to send a line with a word of its list (manner.txt) - even inside another word, computer too. "
     "Off: every line goes.",
     "Гра не надсилає рядок зі словом зі свого списку (manner.txt) — навіть усередині іншого слова, як computer. "
     "Вимкнено: йде кожен рядок.",
     "Игра не отправляет строку со словом из своего списка (manner.txt) — даже внутри другого слова, как computer. "
     "Выключено: уходит каждая строка."},
    {Text::InterfaceEscCutin, "Esc closes the NPC's picture", "Esc закриває картинку NPC",
     "Esc закрывает картинку NPC"},  // 3.0.13-beta.10
    {Text::InterfaceEscCutinHint,
     "The big picture of an NPC that sometimes stays on the screen after a talk goes away with Esc.",
     "Велика картинка NPC, що іноді лишається на екрані після розмови, зникає по Esc.",
     "Большая картинка NPC, которая иногда остаётся на экране после разговора, исчезает по Esc."},
    {Text::InterfaceFollow, "Steady following of a player", "Стійке слідування за гравцем",
     "Устойчивое следование за игроком"},  // 3.0.13-beta.11
    {Text::InterfaceFollowHint,
     "Shift+click on a player: the character follows them and keeps following when they stop - even with this game "
     "window behind another (a priest in a second window after you). A plain click on the ground or Esc stops it.",
     "Shift+клік по гравцю: персонаж іде за ним і не зупиняється, коли той став, — навіть коли це вікно гри за іншим "
     "(прист у другому вікні за вами). Звичайний клік по землі або Esc — зупинити.",
     "Shift+клик по игроку: персонаж идёт за ним и не останавливается, когда тот встал, — даже когда это окно игры за "
     "другим (прист во втором окне за вами). Обычный клик по земле или Esc — остановить."},
    {Text::InterfaceLongChat, "Long chat lines", "Довгі рядки чату", "Длинные строки чата"},  // 3.0.13-beta.12
    {Text::InterfaceLongChatHint,
     "The chat's field takes up to 234 letters instead of 70; the server takes 70 at most, so a longer line goes as "
     "several, cut between words.",
     "Поле чату приймає до 234 літер замість 70; сервер бере щонайбільше 70, тож довший рядок іде кількома, "
     "розрізаний між словами.",
     "Поле чата принимает до 234 букв вместо 70; сервер берёт не больше 70, поэтому более длинная строка уходит "
     "несколькими, разрезанная между словами."},
    {Text::InterfaceWhoHits, "Who hits me", "Хто б'є мене", "Кто бьёт меня"},  // 3.0.13-beta.13
    {Text::InterfaceWhoHitsHint,
     "The monsters that hit you in the last seconds get a red frame round their HP bar, and a red ⚔ 3 over your "
     "character counts them - seen at once when the crowd grows.",
     "Монстри, що били вас останніми секундами, отримують червону рамку довкола смужки HP, а червоне ⚔ 3 над "
     "персонажем їх рахує — одразу видно, коли натовп росте.",
     "Монстры, бившие вас последние секунды, получают красную рамку вокруг полоски HP, а червоне ⚔ 3 над персонажем "
     "их считает — сразу видно, когда толпа растёт."},
    {Text::InterfaceChatColourParty, "Chat: the party's colour", "Чат: колір групи", "Чат: цвет группы"},  // .3
    {Text::InterfaceChatColourGuild, "Chat: the guild's colour", "Чат: колір гільдії", "Чат: цвет гильдии"},
    {Text::InterfaceChatColourOwn, "Chat: your own lines", "Чат: ваші рядки", "Чат: ваши строки"},
    {Text::InterfaceChatColourOthers, "Chat: the others' lines", "Чат: рядки інших", "Чат: строки других"},
    {Text::InterfaceChatLog, "Chat log to a file", "Журнал чату у файл", "Журнал чата в файл"},  // 3.0.13-beta.3
    {Text::InterfaceChatLogHint,
     "Every chat line with its time into uaro-graphics\\chat-logs, a file a day - for disputes over trades. Whispers "
     "too; the file stays on this computer and is never sent with a report.",
     "Кожен рядок чату з часом — у uaro-graphics\\chat-logs, файл на день: для суперечок про угоди. Шепіт теж; файл "
     "лишається на цьому комп'ютері й ніколи не йде зі звітом.",
     "Каждая строка чата со временем — в uaro-graphics\\chat-logs, файл на день: для споров о сделках. Шёпот тоже; "
     "файл остаётся на этом компьютере и никогда не уходит с отчётом."},
    {Text::InterfaceChatColourHint, "The colour of these lines in the chat; a GM's lines keep their own.",
     "Колір цих рядків у чаті; рядки GM лишаються своїм кольором.",
     "Цвет этих строк в чате; строки GM остаются своим цветом."},
    {Text::ChatColourDefault, "As it is", "Як є", "Как есть"},
    {Text::ChatColourBlue, "Blue", "Блакитний", "Голубой"},
    {Text::ChatColourGreen, "Green", "Зелений", "Зелёный"},
    {Text::ChatColourYellow, "Yellow", "Жовтий", "Жёлтый"},
    {Text::ChatColourOrange, "Orange", "Помаранчевий", "Оранжевый"},
    {Text::ChatColourPink, "Pink", "Рожевий", "Розовый"},
    {Text::ChatColourViolet, "Violet", "Фіолетовий", "Фиолетовый"},
    {Text::ChatColourWhite, "White", "Білий", "Белый"},
    {Text::InterfaceSkills, "Skills", "Навички", "Навыки"},  // 3.0.11-beta.96
    {Text::InterfaceSkillsHint,
     "The skill tree (Alt+S) in the new look: tabs by the jobs the character passed through, each skill's level out of "
     "its most, a plus where a free point raises it, the arrows of the level it is used at. A double click uses it, a "
     "press drags it onto the hotbar, a right click shows its description. The title moves it.",
     "Дерево навичок (Alt+S) у новому вигляді: вкладки за професіями, які пройшов персонаж, рівень кожної навички з "
     "її найбільшого, плюс там, де вільне очко її підвищує, стрілки рівня, яким вона застосовується. Подвійний клік — "
     "застосувати, затиснути — перетягнути на панель навичок, права кнопка — опис. За заголовок вікно рухається.",
     "Дерево навыков (Alt+S) в новом виде: вкладки по профессиям, которые прошёл персонаж, уровень каждого навыка из "
     "его наибольшего, плюс там, где свободное очко его повышает, стрелки уровня, которым он применяется. Двойной "
     "клик — применить, зажать — перетащить на панель навыков, правая кнопка — описание. За заголовок окно "
     "двигается."},
    {Text::SkillsTitle, "Skills", "Навички", "Навыки"},
    {Text::SkillsPoints, "Free points: {1}", "Вільних очок: {1}", "Свободных очков: {1}"},
    {Text::SkillsDrag, "Drag onto the skill bar · right click — description", "Перетягніть на панель · ПКМ — опис",
     "Перетащите на панель · ПКМ — описание"},
    {Text::SkillsPassive, "Passive", "Пасивна", "Пассивный"},
    {Text::SkillsEnemy, "On an enemy", "На ворога", "На врага"},
    {Text::SkillsGround, "On the ground", "На землю", "На землю"},
    {Text::SkillsSelf, "On yourself", "На себе", "На себя"},
    {Text::SkillsAlly, "On an ally", "На союзника", "На союзника"},
    {Text::SkillsTrap, "A trap", "Пастка", "Ловушка"},
    {Text::SkillsSp, "SP {1}", "SP {1}", "SP {1}"},
    {Text::SkillsRange, "range {1}", "дальність {1}", "дальность {1}"},
    {Text::SkillsNeeds, "Needs: {1}", "Потрібно: {1}", "Нужно: {1}"},
    {Text::SkillsUnlearned, "Not learned", "Не вивчено", "Не изучен"},
    {Text::SkillsPadUse, "Use", "Застосувати", "Применить"},
    {Text::SkillsPadRaise, "+1 level", "+1 рівень", "+1 уровень"},
    {Text::SkillsPadAssign, "Take", "Взяти", "Взять"},  // 3.0.11-beta.121: X carries it to a cell (was «Призначити»)
    {Text::SkillsPadClose, "Close", "Закрити", "Закрыть"},
    {Text::InterfaceEquip, "Equipment", "Спорядження", "Снаряжение"},  // 3.0.11-beta.97
    {Text::InterfaceEquipHint,
     "The equipment (Alt+Q) in the new look: the slots round the character, each worn thing's icon, refine and name, "
     "dots for its card slots; the character's portrait, levels and battle numbers in the middle. A double click "
     "takes a thing off, a press drags it into the bag, a thing dragged from «Items» onto it is put on.",
     "Спорядження (Alt+Q) у новому вигляді: слоти навколо персонажа, у кожної вдягненої речі іконка, заточка й назва, "
     "кружечки слотів карток; посередині портрет персонажа, рівні й бойові числа. Подвійний клік — зняти, затиснути — "
     "перетягнути в сумку, річ, перетягнута з «Речі» на вікно, вдягається.",
     "Снаряжение (Alt+Q) в новом виде: слоты вокруг персонажа, у каждой надетой вещи иконка, заточка и название, "
     "кружки слотов карт; посередине портрет персонажа, уровни и боевые числа. Двойной клик — снять, зажать — "
     "перетащить в сумку, вещь, перетащенная из «Вещи» на окно, надевается."},
    {Text::EquipTitle, "Equipment", "Спорядження", "Снаряжение"},
    {Text::EquipHeadMid, "Middle head", "Середина голови", "Середина головы"},
    {Text::EquipHeadLow, "Lower head", "Низ голови", "Низ головы"},
    {Text::EquipArmor, "Armour", "Броня", "Броня"},
    {Text::EquipGarment, "Garment", "Плащ", "Плащ"},
    {Text::EquipAccessory, "Accessory", "Аксесуар", "Аксессуар"},
    {Text::EquipHeadTop, "Upper head", "Верх голови", "Верх головы"},
    {Text::EquipWeapon, "Weapon", "Зброя", "Оружие"},
    {Text::EquipShield, "Shield", "Щит", "Щит"},
    {Text::EquipShoes, "Shoes", "Взуття", "Обувь"},
    {Text::EquipAmmo, "Ammo", "Набої", "Снаряды"},
    {Text::EquipCards, "Dots - card slots", "Кружечки — слоти карток", "Кружки — слоты карт"},
    {Text::EquipDrag, "Drag a thing from «Items» onto a slot", "Перетягніть річ із «Речі» на слот",
     "Перетащите вещь из «Вещи» на слот"},
    {Text::EquipLevel, "Lv {1} / {2}", "Рів. {1} / {2}", "Ур. {1} / {2}"},
    {Text::EquipPickTitle, "What to put on: {1}", "Що вдягнути: {1}", "Что надеть: {1}"},
    {Text::EquipPickNone, "Nothing in the bag fits", "У сумці нічого не підходить", "В сумке ничего не подходит"},
    {Text::EquipPadOff, "Take off", "Зняти", "Снять"},
    {Text::EquipPadPut, "Put on", "Вдягнути", "Надеть"},
    {Text::EquipPadChange, "Change", "Замінити", "Заменить"},
    {Text::EquipPadCard, "Card", "Картка", "Карточка"},
    {Text::EquipPadBack, "Back", "Назад", "Назад"},
    {Text::InterfaceNpc, "Talk with NPCs", "Розмова з NPC", "Разговор с NPC"},  // 3.0.11-beta.98
    {Text::InterfaceNpcHint,
     "The NPCs' words in the new look at the bottom middle: the name, the text with its colours, the menu's options "
     "numbered in two columns, Next and Close. Keys 1-9 choose, Enter answers, Esc closes (while the chat's line is "
     "empty); the controller answers as before. The text is the server's, untranslated.",
     "Слова NPC у новому вигляді внизу посередині: ім'я, текст із його кольорами, варіанти меню з номерами у дві "
     "колонки, «Далі» й «Закрити». Клавіші 1-9 — вибір, Enter — відповідь, Esc — закрити (поки рядок чату порожній); "
     "контролер відповідає як раніше. Текст — сервера, без перекладу.",
     "Слова NPC в новом виде внизу посередине: имя, текст с его цветами, варианты меню с номерами в две "
     "колонки, «Далее» и «Закрыть». Клавиши 1-9 — выбор, Enter — ответ, Esc — закрыть (пока строка чата "
     "пуста); контроллер отвечает как раньше. Текст — сервера, без перевода."},
    {Text::NpcNext, "Next", "Далі", "Далее"},
    {Text::NpcKeys, "1-9 choose \xC2\xB7 Enter answers \xC2\xB7 Esc closes",
     "1-9 вибір \xC2\xB7 Enter далі \xC2\xB7 Esc закрити", "1-9 выбор \xC2\xB7 Enter далее \xC2\xB7 Esc закрыть"},
    {Text::NpcServer, "The text is the server's", "Текст — як у сервера", "Текст — как у сервера"},
    {Text::NpcChoose, "choose", "вибір", "выбор"},
    {Text::InterfaceNpcRemember, "Remember NPC menu choices", "Пам'ятати вибір у меню NPC",
     "Запоминать выбор в меню NPC"},  // 3.0.11-beta.255
    {Text::InterfaceNpcRememberHint,
     "At the same NPC's same menu the option chosen last time is chosen again when it opens, marked «last time»: a "
     "Kafra's teleport to the same town is A and A. Each character has its own.",
     "У того самого NPC в тому самому меню одразу вибрано те, що обирали минулого разу, з позначкою «минулого разу»: "
     "телепорт Kafra в те саме місто — A і A. У кожного персонажа своє.",
     "У того же NPC в том же меню сразу выбрано то, что выбирали в прошлый раз, с пометкой «в прошлый раз»: "
     "телепорт Kafra в тот же город — A и A. У каждого персонажа своё."},
    {Text::NpcLastTime, "last time", "минулого разу", "в прошлый раз"},
    // 3.0.11-beta.273: the character's picture
    {Text::InterfacePortraits, "Character portraits", "Портрет персонажа", "Портрет персонажа"},
    {Text::InterfacePortraitsHint,
     "Now and then, while the character stands still with no window open, its picture as it looks in the game (hair, "
     "headgear, gear) is kept and shown on its place in the character select instead of the plain head.",
     "Час від часу, коли персонаж стоїть без відкритих вікон, його вигляд у грі (зачіска, шапки, спорядження) "
     "зберігається й показується на його місці у виборі персонажа замість простої голови.",
     "Время от времени, когда персонаж стоит без открытых окон, его вид в игре (причёска, шапки, снаряжение) "
     "сохраняется и показывается на его месте в выборе персонажа вместо простой головы."},
    // 3.0.11-beta.275: the character select as a carousel
    {Text::InterfaceCarousel, "Character select as a carousel", "Вибір персонажа каруселлю",
     "Выбор персонажа каруселью"},
    {Text::InterfaceCarouselHint,
     "The chosen character in the middle, two neighbours each side smaller; ‹ ›, the wheel, the D-pad or a click on a "
     "neighbour go round all the places.",
     "Вибраний персонаж посередині, по двоє сусідів з боків менші; ‹ ›, коліщатко, хрестовина чи клік по сусіду ведуть "
     "по колу всіма місцями.",
     "Выбранный персонаж посередине, по двое соседей по бокам меньше; ‹ ›, колёсико, крестовина или клик по соседу "
     "ведут по кругу через все места."},
    // 3.0.11-beta.276: the delay after a skill on the hotbar
    {Text::InterfaceDelayRing, "Delay after a skill on the hotbar", "Затримка після вміння на панелі",
     "Задержка после умения на панели"},
    {Text::InterfaceDelayRingHint,
     "After a skill the hotbar's skills darken and clear clockwise for as long as the server keeps you from the next "
     "one (its delay, shorter with DEX on uaRO).",
     "Після вміння вміння на панелі темніють і світлішають за годинниковою стрілкою, поки сервер не дає наступне (його "
     "затримка, на uaRO коротша з DEX).",
     "После умения умения на панели темнеют и светлеют по часовой стрелке, пока сервер не даёт следующее (его "
     "задержка, на uaRO короче с DEX)."},
    // 3.0.11-beta.277: what killed the character
    {Text::InterfaceDeathRecap, "What killed me", "Що мене вбило", "Что меня убило"},
    {Text::InterfaceDeathRecapHint,
     "The death screen lists the last hits of the 10 seconds before it: who, by which skill, how much and when.",
     "Екран смерті показує останні удари за 10 секунд до неї: хто, яким умінням, скільки й коли.",
     "Экран смерти показывает последние удары за 10 секунд до неё: кто, каким умением, сколько и когда."},
    {Text::RecapTitle, "Last hits · {1} damage", "Останні удари · {1} шкоди", "Последние удары · {1} урона"},
    {Text::RecapAttack, "attack", "удар", "удар"},
    {Text::RecapAgo, "{1} s before", "{1} с до того", "{1} с до того"},
    {Text::InterfaceHunt, "Hunt panel", "Панель полювання", "Панель охоты"},  // 3.0.11-beta.278
    {Text::InterfaceHuntHint,
     "A panel over the game: damage a second (10 s and a minute) and the skills' shares, experience an hour and the "
     "time to the level, zeny an hour, monsters killed. The round arrow begins it anew; its place is set in «Hunt».",
     "Панель над грою: шкода за секунду (10 с і хвилина) та частки навичок, досвід за годину й час до рівня, зені "
     "за годину, убиті монстри. Кругла стрілка починає наново; місце задається в «Полювання».",
     "Панель над игрой: урон в секунду (10 с и минута) и доли навыков, опыт в час и время до уровня, зени в час, "
     "убитые монстры. Круглая стрелка начинает заново; место задаётся в «Охота»."},
    {Text::HuntTitle, "Hunt · {1}", "Полювання · {1}", "Охота · {1}"},
    {Text::HuntDps, "DPS {1} · a minute {2}", "DPS {1} · за хв {2}", "DPS {1} · за мин {2}"},
    {Text::HuntExp, "Exp {1} %/h", "Досвід {1} %/год", "Опыт {1} %/ч"},
    {Text::HuntToLevel, "level in {1}", "до рівня {1}", "до уровня {1}"},
    {Text::HuntZeny, "Zeny {1}/h", "Зені {1}/год", "Зени {1}/ч"},
    {Text::HuntKills, "Killed {1}: {2}", "Убито {1}: {2}", "Убито {1}: {2}"},
    {Text::HuntParty, "Party: {1}", "Група: {1}", "Группа: {1}"},  // 3.0.13-beta.14
    {Text::HuntMe, "you", "ви", "вы"},
    {Text::InterfaceMobCard, "Monster card by the cursor", "Картка монстра біля курсора",
     "Карточка монстра у курсора"},  // 3.0.11-beta.279
    {Text::InterfaceMobCardHint,
     "Point at a monster: its level, HP, element, race and size, your chance to hit it and its chance to hit you, its "
     "blow after your DEF and your blow by the server's rules (without element, size and cards).",
     "Наведіть на монстра: рівень, HP, стихія, раса й розмір, ваш шанс влучити й його шанс влучити у вас, його удар "
     "після вашої DEF і ваш удар за правилами сервера (без стихії, розміру й карток).",
     "Наведите на монстра: уровень, HP, стихия, раса и размер, ваш шанс попасть и его шанс попасть по вам, его удар "
     "после вашей DEF и ваш удар по правилам сервера (без стихии, размера и карт)."},
    {Text::MobCardHit, "You hit {1} %", "Ви влучаєте {1} %", "Вы попадаете {1} %"},
    {Text::MobCardIts, "It hits you {1} %", "Він влучає {1} %", "Он попадает {1} %"},
    {Text::MobCardBlow, "Its blow {1}–{2} · {3}/s", "Його удар {1}–{2} · {3}/с", "Его удар {1}–{2} · {3}/с"},
    {Text::MobCardMine, "Your blow ≈{1} · {2} blows", "Ваш удар ≈{1} · ударів {2}", "Ваш удар ≈{1} · ударов {2}"},
    {Text::InterfaceMvp, "Boss timers", "Таймери босів", "Таймеры боссов"},  // 3.0.11-beta.280
    {Text::InterfaceMvpHint,
     "When a boss dies in front of you, a panel counts down to its return by the server's rule: name, map, time left; "
     "a plus once it is back. A click on a row forgets it. Kept between starts; its place is set in «MVP».",
     "Коли бос гине у вас на очах, панель відлічує час до його повернення за правилом сервера: ім'я, карта, скільки "
     "лишилось; плюс — коли він уже мав повернутися. Клік по рядку забуває його. Зберігається між запусками; місце "
     "задається в «MVP».",
     "Когда босс погибает у вас на глазах, панель отсчитывает время до его возвращения по правилу сервера: имя, "
     "карта, сколько осталось; плюс — когда он уже должен был вернуться. Клик по строке забывает его. Сохраняется "
     "между запусками; место задаётся в «MVP»."},
    {Text::MvpTitle, "Bosses", "Боси", "Боссы"},
    {Text::InterfacePotionWarn, "Potions running low", "Зілля закінчуються", "Зелья заканчиваются"},  // .281
    {Text::InterfacePotionWarnHint,
     "Healing items on the hotbar: a soft chime when 10 are left and when none are, the count turns red from 10 down.",
     "Лікувальні предмети на панелі: тихий сигнал, коли лишилось 10 і коли не лишилось жодного; число червоніє від 10.",
     "Лечебные предметы на панели: тихий сигнал, когда осталось 10 и когда не осталось ни одного; число краснеет от "
     "10."},
    {Text::InterfacePartyAlarm, "Healer's alarm", "Тривога для лікаря", "Тревога для лекаря"},  // .282
    {Text::InterfacePartyAlarmHint,
     "When someone of your party nearby falls under 30 % HP, a short chime sounds and their row on the party panel "
     "pulses red for 3 seconds.",
     "Коли хтось із групи поруч падає нижче 30 % HP, лунає короткий сигнал, а його рядок на панелі групи 3 секунди "
     "пульсує червоним.",
     "Когда кто-то из группы рядом падает ниже 30 % HP, звучит короткий сигнал, а его строка на панели группы 3 "
     "секунды пульсирует красным."},
    {Text::InterfaceDropNames, "Item names on Alt", "Назви предметів по Alt", "Названия предметов по Alt"},
    {Text::InterfaceDropNamesHint,
     "Hold Alt: every item on the ground gets its name and amount on a small plate over it.",
     "Затисніть Alt: кожен предмет на землі отримує назву й кількість на маленькій плашці над ним.",
     "Зажмите Alt: каждый предмет на земле получает название и количество на маленькой плашке над ним."},
    {Text::InterfaceDropHighlight, "Valuable drops", "Цінна здобич", "Ценная добыча"},
    {Text::InterfaceDropHighlightHint,
     "Valuable items on the ground are always named: cards purple, weapons and armour blue, others worth 10 000 "
     "zeny or more gold.",
     "Цінні предмети на землі підписані завжди: картки фіолетовим, зброя й броня синім, інше від 10 000 зені — "
     "золотим.",
     "Ценные предметы на земле подписаны всегда: карты фиолетовым, оружие и броня синим, остальное от 10 000 зени "
     "— золотым."},
    {Text::ChatSearch, "Search", "Пошук", "Поиск"},  // 3.0.11-beta.284
    {Text::ChatSearchHint, "Type to find in the lines · Esc closes", "Пишіть, щоб знайти в рядках · Esc — закрити",
     "Пишите, чтобы найти в строках · Esc — закрыть"},
    {Text::ItemsCompareInstead, "Instead of {1}: {2}", "Замість {1}: {2}", "Вместо {1}: {2}"},  // .285
    {Text::ItemsCompareNone, "Nothing worn there: {1}", "Там нічого не вдягнуто: {1}", "Там ничего не надето: {1}"},
    {Text::ItemsCompareSlots, "slots", "слоти", "слоты"},
    {Text::StashAlso, "Also: {1}", "Ще: {1}", "Ещё: {1}"},  // 3.0.11-beta.286
    {Text::StashStorage, "Storage", "Склад", "Склад"},
    {Text::StashCart, "cart", "візок", "тележка"},
    {Text::ItemsMarket, "Market: from {1} z · {2} lots", "Ринок: від {1} z · лотів {2}",
     "Рынок: от {1} z · лотов {2}"},  // 3.0.11-beta.287
    {Text::InterfaceLayoutPerChar, "Layout per character", "Розміщення для кожного персонажа",
     "Размещение для каждого персонажа"},  // 3.0.11-beta.288
    {Text::InterfaceLayoutPerCharHint,
     "Each character keeps its own places and sizes of the interface's parts (the common ones until you change them "
     "for it). The hotbar has three sets for each character: Ctrl+Shift+1, 2, 3.",
     "Кожен персонаж має власні місця й розміри частин інтерфейсу (спільні, доки ви не зміните їх для нього). Панель "
     "швидких клавіш має три набори для кожного персонажа: Ctrl+Shift+1, 2, 3.",
     "Каждый персонаж хранит свои места и размеры частей интерфейса (общие, пока вы не измените их для него). Панель "
     "быстрых клавиш имеет три набора для каждого персонажа: Ctrl+Shift+1, 2, 3."},
    {Text::HotbarWheelLevel, "wheel — level (1–{1})", "колесо — рівень (1–{1})", "колесо — уровень (1–{1})"},
    {Text::InterfaceHomunAi, "uaRO AI for the homunculus and mercenary", "ШІ uaRO для гомункула й найманця",
     "ИИ uaRO для гомункула и наёмника"},  // 3.0.11-beta.290
    {Text::InterfaceHomunAiHint,
     "Our own AI, put into the game's AI\\USER_AI folder (never over another AI): follows you, defends you, heals. "
     "Turn on «Own homunculus AI (/hoai)» and «Own mercenary AI (/merai)» for the game to use it.",
     "Власний ШІ, кладеться в теку гри AI\\USER_AI (ніколи поверх іншого ШІ): ходить за вами, захищає, лікує. "
     "Увімкніть «Свій ШІ гомункула (/hoai)» і «Свій ШІ найманця (/merai)», щоб гра його брала.",
     "Собственный ИИ, кладётся в папку игры AI\\USER_AI (никогда поверх другого ИИ): ходит за вами, защищает, "
     "лечит. Включите «Свой ИИ гомункула (/hoai)» и «Свой ИИ наёмника (/merai)», чтобы игра его брала."},
    {Text::InterfaceHomunAggressive, "AI: attacks by itself", "ШІ: нападає сам", "ИИ: нападает сам"},
    {Text::InterfaceHomunAggressiveHint, "It attacks monsters near you without waiting for them to attack.",
     "Нападає на монстрів поруч із вами, не чекаючи, поки вони нападуть.",
     "Нападает на монстров рядом с вами, не дожидаясь, пока они нападут."},
    {Text::InterfaceHomunDefend, "AI: defends you", "ШІ: захищає вас", "ИИ: защищает вас"},
    {Text::InterfaceHomunDefendHint, "It attacks monsters that attack you or it.",
     "Нападає на монстрів, що нападають на вас чи на нього.", "Нападает на монстров, атакующих вас или его."},
    {Text::InterfaceHomunSkills, "AI: uses skills", "ШІ: використовує навички", "ИИ: использует навыки"},
    {Text::InterfaceHomunSkillsHint,
     "Lif heals you under half HP; Filir and Vanilmirth open a fight with their skill (level 1).",
     "Lif лікує вас, коли HP менше половини; Filir і Vanilmirth починають бій своєю навичкою (рівень 1).",
     "Lif лечит вас, когда HP меньше половины; Filir и Vanilmirth начинают бой своим навыком (уровень 1)."},
    {Text::NpcTalkTag, "Talk", "Говорити", "Говорить"},  // 3.0.11-beta.256: the NPC's plate before its name comes
    {Text::InterfaceShop, "NPC shops", "Крамниці NPC", "Магазины NPC"},  // 3.0.11-beta.100
    {Text::InterfaceShopHint,  // 3.0.11-beta.101: the keys, the amount typed, the description
     "The NPCs' shops in the new look: the list on the left with a search, the cart on the right, the total, the zeny "
     "left and the weight after it; «Buy» and «Sell» are tabs. The mouse: a click puts one in the cart (with Shift "
     "ten), the right button shows the item's description, a cart row's cross takes it back, a click on its amount "
     "types it. The keys: the arrows choose, Space +1, digits the amount, Del takes back, F1 the description, Enter "
     "buys, Esc closes. The controller: A +1, X −1, LT takes back, Y the description, Start buys or sells, LB / RB the "
     "tabs, B closes. Off: the game's own windows.",
     "Крамниці NPC у новому вигляді: список ліворуч із пошуком, кошик праворуч, сума, залишок зені й вага після "
     "покупки; «Купити» й «Продати» — вкладки. Миша: клацання кладе одну в кошик (із Shift — десять), права кнопка "
     "показує опис предмета, хрестик у рядку кошика повертає його, клацання по кількості — ввести її. Клавіші: "
     "стрілки — вибір, Пробіл — +1, цифри — кількість, Del — прибрати, F1 — опис, Enter — купити, Esc — закрити. "
     "Контролер: A +1, X −1, LT — прибрати, Y — опис, Start — купити чи продати, LB / RB — вкладки, B — закрити. "
     "Вимкнено — вікна гри.",
     "Магазины NPC в новом виде: список слева с поиском, корзина справа, сумма, остаток зени и вес после покупки; "
     "«Купить» и «Продать» — вкладки. Мышь: щелчок кладёт одну в корзину (с Shift — десять), правая кнопка "
     "показывает описание предмета, крестик в строке корзины возвращает его, щелчок по количеству — ввести его. "
     "Клавиши: стрелки — выбор, Пробел — +1, цифры — количество, Del — убрать, F1 — описание, Enter — купить, Esc — "
     "закрыть. Контроллер: A +1, X −1, LT — убрать, Y — описание, Start — купить или продать, LB / RB — вкладки, "
     "B — закрыть. Выключено — окна игры."},
    {Text::ShopTitle, "Shop", "Крамниця", "Магазин"},
    {Text::ShopBuy, "Buy", "Купити", "Купить"},
    {Text::ShopSell, "Sell", "Продати", "Продать"},
    {Text::ShopSearch, "Search the shop", "Пошук у крамниці", "Поиск в магазине"},
    {Text::ShopWare, "Item", "Товар", "Товар"},
    {Text::ShopPrice, "Price", "Ціна", "Цена"},
    {Text::ShopWeight, "Weight", "Вага", "Вес"},
    {Text::ShopCart, "Cart", "Кошик", "Корзина"},
    {Text::ShopAmount, "Amount", "Кількість", "Количество"},
    {Text::ShopSum, "Sum", "Сума", "Сумма"},
    {Text::ShopTotal, "Total", "Разом", "Итого"},
    {Text::ShopLeft, "Left", "Залишиться", "Останется"},
    {Text::ShopAfter, "You will have", "Буде", "Будет"},
    {Text::ShopWeightBuy, "Weight after buying", "Вага після покупки", "Вес после покупки"},
    {Text::ShopWeightSell, "Weight after selling", "Вага після продажу", "Вес после продажи"},
    {Text::ShopClear, "Clear", "Очистити", "Очистить"},
    {Text::ShopNote, "Prices are the server's, weights its item base's", "Ціни — від сервера, вага — з бази предметів",
     "Цены — от сервера, вес — из базы предметов"},
    {Text::ShopEmpty, "Click an item to put it here", "Клацніть товар, щоб додати", "Щёлкните товар, чтобы добавить"},
    {Text::ShopWaiting, "Loading the list…", "Завантажую список…", "Загружаю список…"},
    {Text::ShopChoose, "choose", "вибір", "выбор"},  // 3.0.11-beta.101: the keys' words
    {Text::ShopToCart, "list / cart", "список / кошик", "список / корзина"},
    {Text::ShopKeyAmount, "amount", "кількість", "количество"},
    {Text::ShopRemove, "take back", "прибрати", "убрать"},
    {Text::ShopInfo, "description", "опис", "описание"},
    {Text::ShopSpace, "Space", "Пробіл", "Пробел"},
    {Text::ShopEditHint, "Enter: done \xC2\xB7 Esc: undo \xC2\xB7 at most {1}",
     "Enter — готово \xC2\xB7 Esc — скасувати \xC2\xB7 найбільше {1}",
     "Enter — готово \xC2\xB7 Esc — отменить \xC2\xB7 не больше {1}"},
    {Text::InterfaceStorage, "Kafra storage", "Склад Кафри", "Склад Кафры"},  // 3.0.11-beta.115
    {Text::InterfaceStorageHint,
     "The Kafra storage in the new look: «Storage» with the same tabs, search and grid as «Items», and «Items» opened "
     "beside it. Drag a thing from one into the other, or double click it: in «Storage» it goes into the bag, in "
     "«Items» into the storage; how many - the game asks for a stack. The right button shows the item's card. The "
     "controller: A moves the thing, Y its card, LB / RB the tabs, B closes. Off: the game's own window.",
     "Склад Кафри в новому вигляді: «Склад» із тими самими вкладками, пошуком і сіткою, що й «Речі», а «Речі» "
     "відкриваються поруч. Перетягніть річ з одного в інше або клацніть двічі: у «Склад» — річ іде в сумку, у «Речі» — "
     "на склад; скільки — гра питає для стосу. Права кнопка показує картку речі. Контролер: A переносить річ, Y — "
     "картка, LB / RB — вкладки, B закриває. Вимкнено — вікно самої гри.",
     "Склад Кафры в новом виде: «Склад» с теми же вкладками, поиском и сеткой, что и «Вещи», а «Вещи» открываются "
     "рядом. Перетащите вещь из одного в другое или дважды щёлкните: в «Склад» — вещь идёт в сумку, в «Вещи» — на "
     "склад; сколько — игра спрашивает для стопки. Правая кнопка показывает карточку вещи. Контроллер: A переносит "
     "вещь, Y — карточка, LB / RB — вкладки, B закрывает. Выключено — окно самой игры."},
    {Text::StorageTitle, "Storage", "Склад", "Склад"},
    {Text::StoragePlaces, "Places", "Місця", "Места"},
    {Text::StorageTake, "double click: into the bag", "двічі клік — у сумку", "двойной клик — в сумку"},
    {Text::StoragePadTake, "into the bag", "у сумку", "в сумку"},
    {Text::StoragePadMore, "{1}: take \xC2\xB7 {2}: its card", "{1}: взяти \xC2\xB7 {2}: картка",
     "{1}: взять \xC2\xB7 {2}: карточка"},  // 3.0.11-beta.121: X carries it
    {Text::ItemsToStorage, "double click: into the storage", "двічі клік — на склад", "двойной клик — на склад"},
    {Text::ItemsPadToStorage, "into the storage", "на склад", "на склад"},
    {Text::PadCarryPut, "put", "покласти", "положить"},  // 3.0.11-beta.121
    {Text::PadCarryCancel, "cancel", "скасувати", "отменить"},
    {Text::PadCarryNowhere, "nowhere to put it", "нікуди покласти", "некуда положить"},
    {Text::InterfaceTrade, "Trade", "Обмін", "Обмен"},  // 3.0.11-beta.116
    {Text::InterfaceTradeHint,
     "The trade between players in the new look: «Trade» with your side and the partner's - ten cells each, the zeny "
     "under each - and «Items» opened beside it. Drag things from «Items» onto your side or double click them; type "
     "the zeny in your field (Enter shows it to the partner). «Ready» locks your offer, «Trade» completes it once both "
     "are ready. What the partner adds or changes after your «Ready» turns red with a warning. The controller: A - "
     "the next step, Y - the card, B - cancel. Off: the game's own window.",
     "Обмін між гравцями в новому вигляді: «Обмін» із вашим боком і боком партнера — по десять клітинок і зені під "
     "ними, — а «Речі» відкриваються поруч. Перетягніть речі з «Речі» на свій бік або клацніть по них двічі; зені "
     "введіть у своє поле (Enter показує їх партнерові). «Готово» фіксує вашу пропозицію, «Обмін» завершує, коли "
     "обидва готові. Що партнер додав чи змінив після вашого «Готово», стає червоним із попередженням. Контролер: A — "
     "наступний крок, Y — картка, B — скасувати. Вимкнено — вікно самої гри.",
     "Обмен между игроками в новом виде: «Обмен» с вашей стороной и стороной партнёра — по десять ячеек и зени под "
     "ними, — а «Вещи» открываются рядом. Перетащите вещи из «Вещи» на свою сторону или дважды щёлкните по ним; зени "
     "введите в своё поле (Enter показывает их партнёру). «Готово» фиксирует ваше предложение, «Обмен» завершает, "
     "когда оба готовы. Что партнёр добавил или изменил после вашего «Готово», становится красным с предупреждением. "
     "Контроллер: A — следующий шаг, Y — карточка, B — отменить. Выключено — окно самой игры."},
    {Text::TradeTitle, "Trade", "Обмін", "Обмен"},
    {Text::TradeYou, "You", "Ви", "Вы"},
    {Text::TradeZeny, "Zeny", "Зені", "Зени"},
    {Text::TradeReady, "Ready", "Готово", "Готово"},
    {Text::TradeDo, "Trade", "Обмін", "Обмен"},
    {Text::TradeCancel, "Cancel", "Скасувати", "Отменить"},
    {Text::TradeLate, "Changed after your «Ready»", "Змінено після вашого «Готово»", "Изменено после вашего «Готово»"},
    {Text::TradeHint, "Drag things from «Items» or double click them", "Перетягніть речі з «Речі» або клацніть двічі",
     "Перетащите вещи из «Вещи» или щёлкните дважды"},
    {Text::TradePadHint, "{1}: ready / trade \xC2\xB7 {2}: cancel", "{1}: готово / обмін \xC2\xB7 {2}: скасувати",
     "{1}: готово / обмен \xC2\xB7 {2}: отменить"},
    {Text::ItemsToTrade, "double click: into the trade", "двічі клік — в обмін", "двойной клик — в обмен"},
    {Text::ItemsPadToTrade, "into the trade", "в обмін", "в обмен"},
    // 3.0.11-beta.140 (the owner «давай»): the menu and the death screen
    {Text::InterfaceMenu, "The menu and the death screen", "Меню й екран загибелі", "Меню и экран гибели"},
    {Text::InterfaceMenuHint,
     "Esc's menu and the window after a death in the new look: a card under the middle of the screen, a row a choice "
     "- the save point, character select, exit, back to the game (reviving with a Token of Siegfried when it is in "
     "the bag). Off: the game's own window.",
     "Меню за Esc і вікно після загибелі в новому вигляді: картка під серединою екрана, рядок на кожен вибір — точка "
     "збереження, вибір персонажа, вихід, повернення до гри (воскресіння жетоном Зігфріда, коли він є в сумці). "
     "Вимкнено: власне вікно гри.",
     "Меню по Esc и окно после гибели в новом виде: карточка под серединой экрана, строка на каждый выбор — точка "
     "сохранения, выбор персонажа, выход, возврат в игру (воскрешение жетоном Зигфрида, если он есть в сумке). "
     "Выключено: собственное окно игры."},
    {Text::MenuTitle, "Menu", "Меню", "Меню"},
    {Text::MenuDead, "You died", "Ви загинули", "Вы погибли"},
    {Text::MenuRevive, "Revive here (Token of Siegfried)", "Воскреснути тут (жетон Зігфріда)",
     "Воскреснуть здесь (жетон Зигфрида)"},
    {Text::MenuSavePoint, "To the save point", "До точки збереження", "К точке сохранения"},
    {Text::MenuCharSelect, "Character select", "Вибір персонажа", "Выбор персонажа"},
    {Text::MenuExit, "Exit the game", "Вийти з гри", "Выйти из игры"},
    {Text::MenuBack, "Back to the game", "Повернутися до гри", "Вернуться в игру"},
    // 3.0.11-beta.149: the item card and the skill card
    {Text::InterfaceCards, "Item and skill cards", "Картки предметів і навичок", "Карточки предметов и навыков"},
    {Text::InterfaceCardsHint,
     "The card of an item (a right click on it) and of a skill in the new look, beside the cursor: the picture, the "
     "name, the game's own description, the cards in an item's slots; the wheel scrolls a long one, Esc closes it. "
     "Off: the game's own windows.",
     "Картка предмета (правий клік по ньому) і навички в новому вигляді, поруч із курсором: картинка, назва, власний "
     "опис гри, карти в слотах предмета; коліщатко гортає довгий опис, Esc закриває. Вимкнено: власні вікна гри.",
     "Карточка предмета (правый клик по нему) и навыка в новом виде, рядом с курсором: картинка, название, "
     "собственное описание игры, карты в слотах предмета; колёсико листает длинное описание, Esc закрывает. "
     "Выключено: собственные окна игры."},
    {Text::CardSlots, "Slots", "Слоти", "Слоты"},
    {Text::CardEmpty, "empty", "порожньо", "пусто"},
    {Text::CardUnknown, "Not identified", "Не опізнано", "Не опознано"},
    {Text::CardNotLearned, "Not learned", "Не вивчено", "Не изучено"},
    {Text::CardLevel, "Level {1}", "Рівень {1}", "Уровень {1}"},
    // 3.0.11-beta.150: the requests and the NPC's inputs
    {Text::InterfaceAsks, "Requests and NPC inputs", "Запити та введення для NPC", "Запросы и ввод для NPC"},
    {Text::InterfaceAsksHint,
     "A trade's, a party's, a guild's or a friend's request and an NPC's number or words to type in the new look: a "
     "card at the screen's middle, accept or decline (Esc, the controller's B declines). Off: the game's own windows.",
     "Запит на обмін, запрошення до групи чи гільдії, запит у друзі та число чи текст для NPC у новому вигляді: "
     "картка посередині екрана, прийняти чи відмовити (Esc або B на контролері відмовляє). Вимкнено: власні вікна "
     "гри.",
     "Запрос обмена, приглашение в группу или гильдию, запрос в друзья и число или текст для NPC в новом виде: "
     "карточка посередине экрана, принять или отказать (Esc или B на контроллере отказывает). Выключено: собственные "
     "окна игры."},
    {Text::AskTradeTitle, "Trade", "Обмін", "Обмен"},
    {Text::AskTrade, "{1} offers you a trade", "{1} пропонує вам обмін", "{1} предлагает вам обмен"},
    {Text::AskTradeAny, "Someone offers you a trade", "Вам пропонують обмін", "Вам предлагают обмен"},
    {Text::AskPartyTitle, "Party", "Група", "Группа"},
    {Text::AskParty, "An invitation to the party {1}", "Запрошення до групи {1}", "Приглашение в группу {1}"},
    {Text::AskPartyAny, "An invitation to a party", "Запрошення до групи", "Приглашение в группу"},
    {Text::AskGuildTitle, "Guild", "Гільдія", "Гильдия"},
    {Text::AskGuild, "An invitation to the guild {1}", "Запрошення до гільдії {1}", "Приглашение в гильдию {1}"},
    {Text::AskGuildAny, "An invitation to a guild", "Запрошення до гільдії", "Приглашение в гильдию"},
    {Text::AskFriendTitle, "Friends", "Друзі", "Друзья"},
    {Text::AskFriend, "{1} wants to add you to their friends", "{1} хоче додати вас до друзів",
     "{1} хочет добавить вас в друзья"},
    {Text::AskFriendAny, "Someone wants to add you to their friends", "Вас хочуть додати до друзів",
     "Вас хотят добавить в друзья"},
    {Text::AskNumberTitle, "Type a number", "Введіть число", "Введите число"},
    {Text::AskTextTitle, "Type the words", "Введіть текст", "Введите текст"},
    {Text::AskTypeHint, "Type it on the keyboard; Enter sends it", "Наберіть на клавіатурі; Enter надсилає",
     "Наберите на клавиатуре; Enter отправляет"},
    {Text::AskAccept, "Accept", "Прийняти", "Принять"},
    {Text::AskDecline, "Decline", "Відмовити", "Отказать"},
    {Text::AskOk, "Done", "Готово", "Готово"},
    // 3.0.11-beta.151: the game's message boxes
    {Text::InterfaceMessages, "Game messages", "Повідомлення гри", "Сообщения игры"},
    {Text::InterfaceMessagesHint,
     "The game's message boxes in the new look - its errors (a wrong login, a lost connection), its questions and "
     "its waits - on the login's screens too: the game's words, its buttons (Enter, Esc; A and B on the "
     "controller). Off: the game's own boxes.",
     "Повідомлення гри в новому вигляді — її помилки (невірний логін, обрив з'єднання), запитання й очікування, також "
     "на екранах входу: слова гри, її кнопки (Enter, Esc; A і B на контролері). Вимкнено: власні вікна гри.",
     "Сообщения игры в новом виде — её ошибки (неверный логин, обрыв соединения), вопросы и ожидания, также на "
     "экранах входа: слова игры, её кнопки (Enter, Esc; A и B на контроллере). Выключено: собственные окна игры."},
    {Text::MsgTitle, "Message", "Повідомлення", "Сообщение"},
    {Text::MsgWaitTitle, "Please wait", "Зачекайте", "Подождите"},
    {Text::MsgYes, "Yes", "Так", "Да"},
    {Text::MsgNo, "No", "Ні", "Нет"},
    {Text::MsgCancel, "Cancel", "Скасувати", "Отмена"},
    {Text::InterfaceLogin, "Login window", "Вікно входу", "Окно входа"},
    {Text::InterfaceLoginHint,
     "The game's login in the new look at the screen's middle: the account and the password (typed as before - the "
     "game's own fields), remember the account, enter, exit, and the registration on uaro.kiev.ua. Off: the game's "
     "own window.",
     "Вхід у гру в новому вигляді посередині екрана: логін і пароль (набираються як раніше — це поля самої гри), "
     "запам'ятати логін, увійти, вийти й реєстрація на uaro.kiev.ua. Вимкнено: власне вікно гри.",
     "Вход в игру в новом виде посередине экрана: логин и пароль (набираются как раньше — это поля самой игры), "
     "запомнить логин, войти, выйти и регистрация на uaro.kiev.ua. Выключено: собственное окно игры."},
    {Text::LoginSubtitle, "Into the game", "Вхід у гру", "Вход в игру"},
    {Text::LoginAccount, "Account", "Логін", "Логин"},
    {Text::LoginPassword, "Password", "Пароль", "Пароль"},
    {Text::LoginRemember, "Remember the account", "Запам'ятати логін", "Запомнить логин"},
    {Text::LoginEnter, "Enter", "Увійти", "Войти"},
    {Text::LoginExit, "Exit", "Вийти", "Выйти"},
    // 3.0.11-beta.172: the login's refusals in words
    {Text::LoginErrUnknownId, "There is no login {1}. Check how it is written or",
     "Логіна {1} немає. Перевірте написання або", "Логина {1} нет. Проверьте написание или"},
    {Text::LoginErrPassword, "The password does not fit {1}.", "Пароль не підходить до {1}.",
     "Пароль не подходит к {1}."},
    {Text::LoginErrExpired, "This account has expired.", "Строк дії акаунта минув.", "Срок действия аккаунта истёк."},
    {Text::LoginErrMaintenance, "The server is under maintenance. Try again later.",
     "На сервері технічні роботи. Спробуйте пізніше.", "На сервере технические работы. Попробуйте позже."},
    {Text::LoginErrOldExe, "The game's exe file is old: update the client.", "Файл гри застарів: оновіть клієнт.",
     "Файл игры устарел: обновите клиент."},
    {Text::LoginErrBannedUntil, "Logging in is forbidden until {1}.", "Вхід заборонено до {1}.",
     "Вход запрещён до {1}."},
    {Text::LoginErrJammed, "The server is full. Try again in a few minutes.",
     "Сервер переповнений. Спробуйте за кілька хвилин.", "Сервер переполнен. Попробуйте через несколько минут."},
    {Text::LoginErrRemoved, "This account has been removed.", "Цей акаунт видалено.", "Этот аккаунт удалён."},
    {Text::LoginErrRejected, "The server refused the login.", "Сервер відмовив у вході.", "Сервер отказал во входе."},
    {Text::LoginCreateAccount, "Create an account", "Створити акаунт", "Создать аккаунт"},
    {Text::LoginResetPassword, "Reset the password on the site", "Відновити пароль на сайті",
     "Восстановить пароль на сайте"},
    {Text::LoginHintCaps, "Caps Lock is on", "Увімкнено Caps Lock", "Включён Caps Lock"},
    {Text::LoginHintLayout, "Layout {1} - logins are usually Latin ({2}?)",
     "Розкладка {1} — логін зазвичай латиницею ({2}?)", "Раскладка {1} — логин обычно латиницей ({2}?)"},
    // 3.0.11-beta.173: the server's status on the login screen
    {Text::InterfaceLoginStatus, "Server status on the login", "Стан сервера на вході", "Состояние сервера на входе"},
    {Text::InterfaceLoginStatusHint,
     "A line under the login window: the server answers, how many play now, the ping. When it does not answer - "
     "one line with the next try's countdown instead of the game's boxes; a login is tried again by itself only "
     "after you pressed Enter.",
     "Рядок під вікном входу: сервер відповідає, скільки гравців онлайн, пінг. Коли не відповідає — один рядок з "
     "відліком до наступної спроби замість вікон гри; вхід повторюється сам лише після того, як ви натиснули "
     "«Увійти».",
     "Строка под окном входа: сервер отвечает, сколько игроков онлайн, пинг. Когда не отвечает — одна строка с "
     "отсчётом до следующей попытки вместо окон игры; вход повторяется сам только после того, как вы нажали "
     "«Войти»."},
    {Text::LoginStatusUp, "Server up · {1} players online · ping {2} ms",
     "Сервер працює · {1} гравців онлайн · пінг {2} мс", "Сервер работает · {1} игроков онлайн · пинг {2} мс"},
    {Text::LoginStatusUpCount, "Server up · {1} players online", "Сервер працює · {1} гравців онлайн",
     "Сервер работает · {1} игроков онлайн"},
    {Text::LoginStatusUpNoCount, "Server up · ping {1} ms", "Сервер працює · пінг {1} мс",
     "Сервер работает · пинг {1} мс"},
    {Text::LoginStatusDown, "The server does not answer - trying again in {1} s",
     "Сервер не відповідає — пробуємо ще раз через {1} с", "Сервер не отвечает — пробуем ещё раз через {1} с"},
    {Text::LoginStatusDownIdle, "The server does not answer", "Сервер не відповідає", "Сервер не отвечает"},
    // 3.0.11-beta.174: the site's event or news now on the login screen
    {Text::InterfaceLoginNews, "News on the login", "Новини на вході", "Новости на входе"},
    {Text::InterfaceLoginNewsHint,
     "Above the server's status: the event on now or next, else the newest news from uaro.kiev.ua. A click opens it "
     "in the browser, «All news» - the site's news.",
     "Над станом сервера: подія, що йде зараз або наступна, інакше найсвіжіша новина з uaro.kiev.ua. Клік відкриває "
     "її в браузері, «Усі новини» — новини сайту.",
     "Над состоянием сервера: событие, которое идёт сейчас или следующее, иначе самая свежая новость с uaro.kiev.ua. "
     "Клик открывает её в браузере, «Все новости» — новости сайта."},
    {Text::LoginAllNews, "All news", "Усі новини", "Все новости"},
    // 3.0.11-beta.175: the shelf of accounts
    {Text::LoginAddShelf, "Add to the shelf after login", "Додати на полицю після входу",
     "Добавить на полку после входа"},
    {Text::ShelfAdded, "{1} added to the shelf", "{1} додано на полицю", "{1} добавлен на полку"},
    {Text::ShelfUndo, "Undo", "Скасувати", "Отменить"},
    // 3.0.11-beta.176: «Полиця»
    {Text::ShelfBack, "‹ Shelf", "‹ Полиця", "‹ Полка"},
    {Text::ShelfFirstNote, "The shelf is empty yet: after a login the account shows on it, then - one click to enter",
     "Полиця ще порожня: після входу акаунт з'явиться на ній, далі — вхід одним кліком",
     "Полка пока пуста: после входа аккаунт появится на ней, дальше — вход одним кликом"},
    {Text::ShelfTitle, "Choose an account", "Оберіть акаунт", "Выберите аккаунт"},
    {Text::ShelfTitleHint, "- one click and you play", "— один клік, і ви в грі", "— один клик, и вы в игре"},
    {Text::ShelfPlay, "Play", "Грати", "Играть"},
    {Text::ShelfLogin, "login {1}", "логін {1}", "логин {1}"},
    {Text::ShelfAdd, "Add an account", "Додати акаунт", "Добавить аккаунт"},
    {Text::ShelfAddHint, "the login and password once, then - one click", "логін і пароль один раз, далі — в один клік",
     "логин и пароль один раз, дальше — в один клик"},
    {Text::ShelfOther, "Another account", "Інший акаунт", "Другой аккаунт"},
    {Text::ShelfCreate, "Create an account", "Створити акаунт", "Создать аккаунт"},
    {Text::ShelfRefused, "The password does not fit", "Пароль не підходить", "Пароль не подходит"},
    {Text::ShelfBusy, "Already in the game (window {1})", "Вже в грі (вікно {1})", "Уже в игре (окно {1})"},
    {Text::ShelfNoChar, "Not played yet", "Ще не грали", "Ещё не играли"},
    {Text::ShelfMenuAutoEnter, "Straight into the game", "Одразу в гру", "Сразу в игру"},
    {Text::ShelfMenuKeepPassword, "Keep the password", "Зберігати пароль", "Хранить пароль"},
    {Text::ShelfMenuLabel, "Label…", "Підпис…", "Подпись…"},
    {Text::ShelfMenuEdit, "Change login and password…", "Змінити логін і пароль…", "Изменить логин и пароль…"},
    {Text::ShelfMenuLeft, "Move left", "Перемістити ліворуч", "Переместить влево"},
    {Text::ShelfMenuRight, "Move right", "Перемістити праворуч", "Переместить вправо"},
    {Text::ShelfMenuRemove, "Take off the shelf", "Прибрати з полиці", "Убрать с полки"},
    {Text::ShelfMenuRemoveSure, "Sure? Press again", "Точно? Натисніть ще раз", "Точно? Нажмите ещё раз"},
    // 3.0.11-beta.177: the views
    {Text::InterfaceLoginView, "The login screen", "Екран входу", "Экран входа"},
    {Text::InterfaceLoginViewHint,
     "«Shelf» - a card an account, one click to enter; «Classic» - the login window with ▾ the saved logins; "
     "«Continue» - the last character big and «Play». With no saved accounts the login window whichever. Also at the "
     "login screen's foot.",
     "«Полиця» — картка на акаунт, вхід одним кліком; «Класичний» — вікно входу з ▾ збереженими логінами; "
     "«Продовжити» — останній персонаж великим планом і «Грати». Без збережених акаунтів — вікно входу. Також унизу "
     "екрана входу.",
     "«Полка» — карточка на аккаунт, вход одним кликом; «Классический» — окно входа с ▾ сохранёнными логинами; "
     "«Продолжить» — последний персонаж крупным планом и «Играть». Без сохранённых аккаунтов — окно входа. Также "
     "внизу экрана входа."},
    {Text::LoginViewLabel, "View", "Вигляд", "Вид"},
    {Text::LoginViewClassic, "Classic", "Класичний", "Классический"},
    {Text::LoginViewShelf, "Shelf", "Полиця", "Полка"},
    {Text::LoginViewContinue, "Continue", "Продовжити", "Продолжить"},
    {Text::LoginForgot, "Forgot the password?", "Забули пароль?", "Забыли пароль?"},
    {Text::LoginOtherLogin, "+ Another login", "+ Інший логін", "+ Другой логин"},
    {Text::ContinueHeading, "Continue the game", "Продовжити гру", "Продолжить игру"},
    {Text::ContinueOtherChar, "Another character", "Інший персонаж", "Другой персонаж"},
    {Text::ContinueChange, "Change account", "Змінити акаунт", "Сменить аккаунт"},
    {Text::ContinueBack, "‹ Continue", "‹ Продовжити", "‹ Продолжить"},
    // 3.0.11-beta.180: the site account in the client
    {Text::SiteOffer, "Bring in all logins from the site account", "Підтягнути всі логіни з акаунта сайту",
     "Подтянуть все логины из аккаунта сайта"},
    {Text::SiteOut, "Sign out", "Вийти", "Выйти"},
    {Text::SiteChip, "{1} · logins: {2}", "{1} · логінів: {2}", "{1} · логинов: {2}"},
    {Text::SiteTitle, "The uaro.kiev.ua account", "Акаунт сайту uaro.kiev.ua", "Аккаунт сайта uaro.kiev.ua"},
    {Text::SiteSubtitle, "All your game logins on the shelf at once", "Усі ваші ігрові логіни на полиці одним входом",
     "Все ваши игровые логины на полке одним входом"},
    {Text::SiteWayPassword, "Site password", "Пароль сайту", "Пароль сайта"},
    {Text::SiteWayBrowser, "Code in the browser", "Код у браузері", "Код в браузере"},
    {Text::SiteWayKey, "Key from the cabinet", "Ключ з кабінету", "Ключ из кабинета"},
    {Text::SiteCode2fa, "Two-factor code", "Код двофакторної перевірки", "Код двухфакторной проверки"},
    {Text::SiteKeyLabel, "The key (the cabinet: Security → Devices)", "Ключ (кабінет: Безпека → Пристрої)",
     "Ключ (кабинет: Безопасность → Устройства)"},
    {Text::SiteStartBrowser, "Confirm in the browser", "Підтвердити в браузері", "Подтвердить в браузере"},
    {Text::SiteWaiting, "Waiting for the confirmation… {1} s", "Чекаємо підтвердження… {1} с",
     "Ждём подтверждения… {1} с"},
    {Text::SiteBack, "‹ Back", "‹ Назад", "‹ Назад"},
    {Text::SiteErrEmpty, "Type the e-mail and the password", "Введіть e-mail і пароль", "Введите e-mail и пароль"},
    {Text::SiteErrNoAnswer, "The site does not answer - try again", "Сайт не відповідає — спробуйте ще раз",
     "Сайт не отвечает — попробуйте ещё раз"},
    {Text::SiteErrFailed, "Could not sign in ({1})", "Не вдалося увійти ({1})", "Не удалось войти ({1})"},
    {Text::SiteErrTwofa, "Type the two-factor code", "Введіть код двофакторної перевірки",
     "Введите код двухфакторной проверки"},
    {Text::SiteErrKey, "The key does not fit", "Ключ не підходить", "Ключ не подходит"},
    {Text::SiteErrExpired, "The code expired - start again", "Код минув — почніть знову", "Код истёк — начните заново"},
    // 3.0.11-beta.181: the site's logins on the shelf, «Персонажі», the forms
    {Text::ShelfNoPassword, "The password - typed once", "Пароль — ввести один раз", "Пароль — ввести один раз"},
    {Text::ShelfNoPasswordShort, "the password typed once", "пароль — ввести один раз", "пароль — ввести один раз"},
    {Text::ShelfChars, "Characters ›", "Персонажі ›", "Персонажи ›"},
    {Text::ShelfNewLogin, "+ A new game login", "+ Новий ігровий логін", "+ Новый игровой логин"},
    {Text::ShelfLinkLogin, "Link an existing login", "Прив'язати наявний логін", "Привязать существующий логин"},
    {Text::ShelfMenuGamePassword, "Change the game password…", "Змінити пароль гри…", "Изменить пароль игры…"},
    {Text::ShelfCharsTitle, "Characters", "Персонажі", "Персонажи"},
    {Text::ShelfCharsBack, "‹ Accounts", "‹ Акаунти", "‹ Аккаунты"},
    {Text::ShelfNewChar, "+ A new character", "+ Новий персонаж", "+ Новый персонаж"},
    {Text::SiteFormNewLogin, "A new game login", "Новий ігровий логін", "Новый игровой логин"},
    {Text::SiteFormLink, "Link an existing login", "Прив'язати наявний логін", "Привязать существующий логин"},
    {Text::SiteFormGamePassword, "Change the game password", "Змінити пароль гри", "Изменить пароль игры"},
    {Text::SiteGamePass, "Game password", "Пароль гри", "Пароль игры"},
    {Text::SiteCurrentPass, "The current game password", "Поточний пароль гри", "Текущий пароль игры"},
    {Text::SiteNewPass, "The new game password", "Новий пароль гри", "Новый пароль игры"},
    {Text::SiteSexM, "Male", "Чоловічий", "Мужской"},
    {Text::SiteSexF, "Female", "Жіночий", "Женский"},
    {Text::SiteCreate, "Create the login", "Створити логін", "Создать логин"},
    {Text::SiteLink, "Link", "Прив'язати", "Привязать"},
    {Text::SiteChange, "Change", "Змінити", "Изменить"},
    {Text::SiteErrForm, "The login and the password: 4 letters at least", "Логін і пароль — щонайменше 4 символи",
     "Логин и пароль — минимум 4 символа"},
    // 3.0.11-beta.181: the registration in the client
    {Text::RegTitle, "A new account", "Новий акаунт", "Новый аккаунт"},
    {Text::RegSteps, "1 Account → 2 Character → 3 Into the game", "1 Акаунт → 2 Персонаж → 3 У гру",
     "1 Аккаунт → 2 Персонаж → 3 В игру"},
    {Text::RegPassword2, "The password again", "Пароль ще раз", "Пароль ещё раз"},
    {Text::RegAgree, "I agree with the server's rules", "Погоджуюся з правилами сервера",
     "Согласен с правилами сервера"},
    {Text::RegRules, "the rules", "правила", "правила"},
    {Text::RegCreate, "Create the account", "Створити акаунт", "Создать аккаунт"},
    {Text::RegHaveAccount, "An account on the site already? Sign in and add a login",
     "Вже є акаунт на сайті? Увійти й додати логін", "Уже есть аккаунт на сайте? Войти и добавить логин"},
    {Text::RegFree, "free", "вільний", "свободен"},
    {Text::RegTaken, "taken", "зайнятий", "занят"},
    {Text::RegErrLogin, "The login: Latin letters, digits, _ and -, 4 to 23",
     "Логін: латиниця, цифри, _ і -, від 4 до 23", "Логин: латиница, цифры, _ и -, от 4 до 23"},
    {Text::RegErrPassword, "The password: 8 to 32 letters", "Пароль: від 8 до 32 символів",
     "Пароль: от 8 до 32 символов"},
    {Text::RegErrPassword2, "The passwords differ", "Паролі не збігаються", "Пароли не совпадают"},
    {Text::RegErrEmail, "The e-mail does not look like one", "E-mail схожий на помилку", "E-mail похож на ошибку"},
    {Text::RegErrSex, "Choose the character's sex", "Оберіть стать персонажа", "Выберите пол персонажа"},
    {Text::RegErrAgree, "Agree with the rules to go on", "Погодьтеся з правилами, щоб продовжити",
     "Согласитесь с правилами, чтобы продолжить"},
    // the site's refusals in our words
    {Text::SiteErrCredentials, "A wrong e-mail or password", "Невірний e-mail або пароль",
     "Неверный e-mail или пароль"},
    {Text::SiteErrLinkWrong, "A wrong game login or password", "Невірний ігровий логін або пароль",
     "Неверный игровой логин или пароль"},
    {Text::RegErrTaken, "This login is taken already", "Цей логін уже зайнятий", "Этот логин уже занят"},
    {Text::RegErrEmailTaken, "This e-mail is registered on the site already", "Цей e-mail уже зареєстровано на сайті",
     "Этот e-mail уже зарегистрирован на сайте"},
    {Text::SiteErrCurrentPass, "The current game password does not fit", "Поточний пароль гри не підходить",
     "Текущий пароль игры не подходит"},
    // 3.0.11-beta.152: the character select
    {Text::InterfaceCharSelect, "Character select", "Вибір персонажа", "Выбор персонажа"},
    {Text::InterfaceCharSelectHint,
     "The game's character select in the new look: three cards a page - the face, the name, the job, the levels, HP "
     "and SP, the stats; a click chooses, two clicks enter (on an empty place: make a character), the controller's "
     "A enters, X makes, LB and RB turn the pages. Off: the game's own window.",
     "Вибір персонажа в новому вигляді: три картки на сторінці — обличчя, ім'я, професія, рівні, HP і SP, "
     "характеристики; клік обирає, подвійний клік — у гру (на вільному місці — створити персонажа), на контролері "
     "A — увійти, X — створити, LB і RB гортають сторінки. Вимкнено: власне вікно гри.",
     "Выбор персонажа в новом виде: три карточки на странице — лицо, имя, профессия, уровни, HP и SP, "
     "характеристики; клик выбирает, двойной клик — в игру (на свободном месте — создать персонажа), на "
     "контроллере A — войти, X — создать, LB и RB листают страницы. Выключено: собственное окно игры."},
    {Text::CharsTitle, "Characters", "Вибір персонажа", "Выбор персонажа"},
    {Text::CharsEnter, "Enter", "Увійти", "Войти"},
    {Text::CharsMake, "Make", "Створити", "Создать"},
    {Text::CharsRemove, "Delete", "Видалити", "Удалить"},
    {Text::CharsBack, "Back", "Назад", "Назад"},
    {Text::CharsEmpty, "Free place", "Вільне місце", "Свободное место"},
    {Text::CharsMakeHint, "Make a new character here", "Створіть тут нового персонажа",
     "Создайте здесь нового персонажа"},
    // 3.0.11-beta.153: the character make
    {Text::InterfaceCharMake, "Character make", "Створення персонажа", "Создание персонажа"},
    {Text::InterfaceCharMakeHint,
     "The game's character make in the new look: the face with the hair's style and colour, the name (typed as "
     "before - the game's own field), the stats in the game's three pairs (a stat's + takes a point from its pair, "
     "each pair 10 together). The controller: up and down a row, left and right change it, A makes, B cancels. Off: "
     "the game's own window.",
     "Створення персонажа в новому вигляді: обличчя із зачіскою та кольором волосся, ім'я (набирається як раніше — "
     "це поле самої гри), характеристики в трьох парах гри (+ біля характеристики бере очко з її пари, разом пара "
     "завжди 10). Контролер: вгору й униз — рядок, вліво й управо — змінити, A — створити, B — скасувати. Вимкнено: "
     "власне вікно гри.",
     "Создание персонажа в новом виде: лицо с причёской и цветом волос, имя (набирается как раньше — это поле самой "
     "игры), характеристики в трёх парах игры (+ возле характеристики берёт очко из её пары, вместе пара всегда 10). "
     "Контроллер: вверх и вниз — строка, влево и вправо — изменить, A — создать, B — отменить. Выключено: собственное "
     "окно игры."},
    {Text::MakeTitle, "New character", "Новий персонаж", "Новый персонаж"},
    {Text::MakeName, "Name", "Ім'я", "Имя"},
    {Text::MakeNameHint, "4 to 23 letters", "Від 4 до 23 символів", "От 4 до 23 символов"},
    {Text::MakeHair, "Hair", "Зачіска", "Причёска"},
    {Text::MakeColour, "Hair colour", "Колір волосся", "Цвет волос"},
    {Text::MakePairsHint, "A point moves inside its pair: 10 together", "Очко переходить у парі: разом завжди 10",
     "Очко переходит в паре: вместе всегда 10"},
    // 3.0.11-beta.154: the merchant's cart
    {Text::InterfaceCart, "Cart", "Візок", "Тележка"},
    {Text::InterfaceCartHint,
     "The merchant's cart in the new look, as the storage: the tabs, the search, the weight, the things of the most in "
     "its title; drag things between the cart, the bag and the storage, a double click takes one into the bag, a right "
     "click shows its card. Off: the game's own window.",
     "Візок торговця в новому вигляді, як склад: вкладки, пошук, вага, кількість речей у заголовку; перетягуйте речі "
     "між візком, сумкою і складом, подвійний клік забирає річ у сумку, правий клік показує її картку. Вимкнено: "
     "власне вікно гри.",
     "Тележка торговца в новом виде, как склад: вкладки, поиск, вес, количество вещей в заголовке; перетаскивайте "
     "вещи между тележкой, сумкой и складом, двойной клик забирает вещь в сумку, правый клик показывает её карточку. "
     "Выключено: собственное окно игры."},
    {Text::CartTitle, "Cart", "Візок", "Тележка"},
    {Text::CartPadCard, "{1}: its card", "{1}: картка", "{1}: карточка"},
    // 3.0.11-beta.155: the game's «how many» window
    {Text::InterfaceCounts, "How many window", "Вікно кількості", "Окно количества"},
    {Text::InterfaceCountsHint,
     "The game's how many window (a stack into the storage, the cart, the bag, a trade, a shop) as a card in the new "
     "look: what for and the most, the number typed into an empty field (Enter alone: all), ten, half or all in a "
     "click, Cancel. The controller: A takes all, B gives up. Off: the game's own window.",
     "Вікно кількості гри (стос на склад, у візок, у сумку, в обмін, до крамниці) карткою в новому вигляді: для чого "
     "і скільки найбільше, число набирається в порожнє поле (просто Enter — усі), десять, половина або всі одним "
     "кліком, скасувати. Контролер: A — усі, B — скасувати. Вимкнено: власне вікно гри.",
     "Окно количества игры (стопка на склад, в тележку, в сумку, в обмен, в магазин) карточкой в новом виде: для чего "
     "и сколько максимум, число набирается в пустое поле (просто Enter — все), десять, половина или все одним "
     "кликом, отменить. Контроллер: A — все, B — отменить. Выключено: собственное окно игры."},
    {Text::CountTitle, "How many?", "Скільки?", "Сколько?"},
    {Text::CountHint, "How many? At most {1}", "Скільки штук? Найбільше — {1}", "Сколько штук? Максимум — {1}"},
    {Text::CountHalf, "Half", "Половина", "Половина"},
    {Text::CountAll, "All", "Усі", "Все"},
    {Text::CountDrop, "Drop on the ground", "Викинути на землю", "Выбросить на землю"},
    {Text::CountBuy, "Buy", "Купити", "Купить"},
    {Text::CountSell, "Sell", "Продати", "Продать"},
    {Text::CountTrade, "Into the trade", "В обмін", "В обмен"},
    {Text::CountBag, "Into the bag", "У сумку", "В сумку"},
    {Text::CountStorage, "Into the storage", "На склад", "На склад"},
    {Text::CountCart, "Into the cart", "У візок", "В тележку"},
    {Text::CountShop, "Into the shop", "До лавки", "В лавку"},
    {Text::CountUnbuy, "Out of the purchase", "Прибрати з покупки", "Убрать из покупки"},
    {Text::CountToBuy, "Into the purchase", "До покупки", "В покупку"},
    {Text::CountMail, "Into the letter", "До листа", "В письмо"},
    // 3.0.11-beta.156: another's shop
    {Text::InterfaceVending, "Merchant's shop", "Лавка торговця", "Лавка торговца"},
    {Text::InterfaceVendingHint,
     "Another player's shop in the new look: a row a thing with its price and how many the shop has, + and - choose "
     "how many to buy (Shift: ten), the total against your zeny, Buy and Cancel. The controller: A one more, X one "
     "less, Y buys, B gives up. Off: the game's own two windows.",
     "Лавка іншого гравця в новому вигляді: рядок на річ із ціною і скільки її є, + і − обирають, скільки купити "
     "(Shift — по десять), сума проти ваших зені, купити і скасувати. Контролер: A — ще одна, X — на одну менше, "
     "Y — купити, B — скасувати. Вимкнено: два власні вікна гри.",
     "Лавка другого игрока в новом виде: строка на вещь с ценой и сколько её есть, + и − выбирают, сколько купить "
     "(Shift — по десять), сумма против ваших зени, купить и отменить. Контроллер: A — ещё одна, X — на одну меньше, "
     "Y — купить, B — отменить. Выключено: два собственных окна игры."},
    {Text::VendTitle, "Shop", "Лавка", "Лавка"},
    {Text::VendStock, "{1} in the shop", "є {1}", "есть {1}"},
    {Text::VendTotal, "Total", "Разом", "Итого"},
    {Text::VendZeny, "Your zeny", "Ваші зені", "Ваши зени"},
    {Text::VendBuy, "Buy", "Купити", "Купить"},
    {Text::VendPadHint, "{1}: +1 \xC2\xB7 {2}: -1 \xC2\xB7 {3}: its card \xC2\xB7 {4}: buy",  // .138 Y the card
     "{1}: +1 \xC2\xB7 {2}: \xE2\x88\x92" "1 \xC2\xB7 {3}: картка \xC2\xB7 {4}: купити",
     "{1}: +1 \xC2\xB7 {2}: \xE2\x88\x92" "1 \xC2\xB7 {3}: карточка \xC2\xB7 {4}: купить"},
    // 3.0.11-beta.157: the choice of things (the magnifier's, a card's)
    {Text::InterfacePicks, "Choice of things", "Вибір речі", "Выбор вещи"},
    {Text::InterfacePicksHint,
     "The magnifier's list of things not known yet and a card's list of the things it goes into, in the new look: a "
     "row a thing with its icon, a click chooses, two clicks choose and confirm; the skills' lists too (Teleport, "
     "repair, making, arrows, cooking, eggs). Off: the game's own windows.",
     "Список речей для лупи й список речей, у які йде карта, у новому вигляді: рядок на річ з іконкою, клік обирає, "
     "подвійний клік — обрати й підтвердити; так само списки вмінь (Teleport, ремонт, "
     "виготовлення, стріли, готування, яйця). Вимкнено: власні вікна гри.",
     "Список вещей для лупы и список вещей, в которые идёт карта, в новом виде: строка на вещь с иконкой, клик "
     "выбирает, двойной клик — выбрать и подтвердить; так же списки умений (Teleport, ремонт, "
     "изготовление, стрелы, готовка, яйца). Выключено: собственные окна игры."},
    {Text::PickIdentifyTitle, "Identify", "Опізнати річ", "Опознать вещь"},
    {Text::PickIdentifyHint, "Choose the thing to identify", "Оберіть річ, яку опізнати",
     "Выберите вещь для опознания"},
    {Text::PickIdentifyOk, "Identify", "Опізнати", "Опознать"},
    {Text::PickCompoundTitle, "Insert the card", "Вставити карту", "Вставить карту"},
    {Text::PickCompoundHint, "Choose the thing for the card", "Оберіть річ для карти", "Выберите вещь для карты"},
    {Text::PickCompoundOk, "Insert", "Вставити", "Вставить"},
    {Text::PickEmpty, "No thing fits", "Немає відповідних речей", "Нет подходящих вещей"},
    // 3.0.11-beta.261 (the tester: «телепорт в старому стилі»): a skill's menu (Teleport, Warp Portal) - its title the
    // skill's name when known
    {Text::PickMenuTitle, "Choose", "Вибір", "Выбор"},
    {Text::PickMenuHint, "Choose one and press «Choose»", "Оберіть варіант і натисніть «Вибрати»",
     "Выберите вариант и нажмите «Выбрать»"},
    {Text::PickMenuOk, "Choose", "Вибрати", "Выбрать"},
    // 3.0.11-beta.263: the skills' other lists
    {Text::PickRepairTitle, "Repair", "Ремонт", "Ремонт"},
    {Text::PickUpgradeTitle, "Upgrade a weapon", "Покращення зброї", "Улучшение оружия"},
    {Text::PickProduceTitle, "Make", "Виготовлення", "Изготовление"},
    {Text::PickArrowsTitle, "Arrow Crafting", "Виготовлення стріл", "Изготовление стрел"},
    {Text::PickCookTitle, "Cooking", "Готування", "Готовка"},
    {Text::PickEggsTitle, "Pet incubator", "Інкубатор улюбленця", "Инкубатор питомца"},
    // 3.0.18-beta.1 (the owner: the interface's deferred windows - «делай»): Change Cart's window (UI window 0x5F)
    {Text::PickCartTitle, "Cart look", "Вигляд візка", "Вид тележки"},
    {Text::PickCartRow, "Cart {1}", "Візок {1}", "Тележка {1}"},
    // 3.0.11-beta.158: the party's settings
    {Text::InterfacePartySetup, "Party settings", "Налаштування групи", "Настройки группы"},
    {Text::InterfacePartySetupHint,
     "The party's settings (Alt+P) as a card in the new look: the experience, the things picked up and their sharing, "
     "two choices each (the game's own leader check). The controller: up and down a row, left and right its choice, A "
     "OK, B cancel. Off: the game's own window.",
     "Налаштування групи (Alt+P) карткою в новому вигляді: досвід, підбір речей і їх розподіл, по два варіанти "
     "(перевірка лідера — самої гри). Контролер: вгору й униз — рядок, вліво й управо — вибір, A — готово, "
     "B — скасувати. Вимкнено: власне вікно гри.",
     "Настройки группы (Alt+P) карточкой в новом виде: опыт, подбор вещей и их распределение, по два варианта "
     "(проверка лидера — самой игры). Контроллер: вверх и вниз — строка, влево и вправо — выбор, A — готово, "
     "B — отменить. Выключено: собственное окно игры."},
    {Text::PartySetupTitle, "Party settings", "Налаштування групи", "Настройки группы"},
    {Text::PartyExp, "Experience", "Досвід", "Опыт"},
    {Text::PartyPickup, "Things picked up", "Підбір речей", "Подбор вещей"},
    {Text::PartyShare, "Sharing the things", "Розподіл речей", "Распределение вещей"},
    {Text::PartyEach, "Each takes", "Кожному своє", "Каждому своё"},
    {Text::PartyEven, "Shared evenly", "Порівну", "Поровну"},
    {Text::PartyShared, "Shared", "Спільно", "Сообща"},
    {Text::PartyLeaderOnly, "Only the party's leader changes them", "Змінює лише лідер групи",
     "Меняет только лидер группы"},
    {Text::PartyThingsFixed, "The things are shared as set when the party was made",  // 3.0.11-beta.200
     "Речі діляться так, як задано при створенні групи", "Вещи делятся так, как задано при создании группы"},
    // 3.0.11-beta.159: the pet
    {Text::InterfacePet, "Pet card", "Картка улюбленця", "Карточка питомца"},
    {Text::InterfacePetHint,
     "The pet's window (Alt+J) as a card in the new look: its name and level, fullness and closeness as bars with the "
     "game's words, feed, perform, the accessory off, back into the egg. Off: the game's own window.",
     "Вікно улюбленця (Alt+J) карткою в новому вигляді: ім'я й рівень, ситість і близькість смугами зі словами гри, "
     "нагодувати, трюк, зняти прикрасу, повернути в яйце. Вимкнено: власне вікно гри.",
     "Окно питомца (Alt+J) карточкой в новом виде: имя и уровень, сытость и близость полосами со словами игры, "
     "покормить, трюк, снять украшение, вернуть в яйцо. Выключено: собственное окно игры."},
    {Text::PetTitle, "Pet", "Улюбленець", "Питомец"},
    {Text::PetLevel, "Level {1}", "Рівень {1}", "Уровень {1}"},
    {Text::PetHunger, "Fullness", "Ситість", "Сытость"},
    {Text::PetIntimacy, "Closeness", "Близькість", "Близость"},
    {Text::PetHungry0, "Very hungry", "Дуже голодний", "Очень голоден"},
    {Text::PetHungry1, "Hungry", "Голодний", "Голоден"},
    {Text::PetHungry2, "Neutral", "Звичайно", "Нормально"},
    {Text::PetHungry3, "Satisfied", "Ситий", "Сыт"},
    {Text::PetHungry4, "Stuffed", "Переїв", "Объелся"},
    {Text::PetClose0, "Awkward", "Неприязний", "Неприязненный"},
    {Text::PetClose1, "Shy", "Сором'язливий", "Стеснительный"},
    {Text::PetClose2, "Neutral", "Звичайний", "Обычный"},
    {Text::PetClose3, "Cordial", "Довіряє", "Доверяет"},
    {Text::PetClose4, "Loyal", "Відданий", "Преданный"},
    {Text::PetFeed, "Feed", "Нагодувати", "Покормить"},
    {Text::PetPerform, "Perform", "Трюк", "Трюк"},
    {Text::PetUnequip, "Accessory off", "Зняти прикрасу", "Снять украшение"},
    {Text::PetEgg, "Into the egg", "У яйце", "В яйцо"},
    {Text::PetStatusCheck, "Pet status", "Стан улюбленця", "Состояние питомца"},
    // 3.0.11-beta.160: the homunculus
    {Text::InterfaceHomun, "Homunculus card", "Картка гомункула", "Карточка гомункула"},
    {Text::InterfaceHomunHint,
     "The homunculus' window (Alt+R) as a card in the new look: its name and level, HP, SP and experience bars, "
     "fullness and closeness, its eight stats, feed and its skills listed (a click uses one, a plus raises it; "
     "deleting it small at the foot, the game asks first). Off: the game's own window.",
     "Вікно гомункула (Alt+R) карткою в новому вигляді: ім'я й рівень, смуги HP, SP і досвіду, ситість і "
     "близькість, вісім характеристик, нагодувати й перелік навичок (клік застосовує, плюс піднімає; видалення — "
     "дрібно внизу, гра перепитує). Вимкнено: власне вікно гри.",
     "Окно гомункула (Alt+R) карточкой в новом виде: имя и уровень, полосы HP, SP и опыта, сытость и близость, "
     "восемь характеристик, покормить и список навыков (щелчок применяет, плюс поднимает; удаление — мелко внизу, "
     "игра переспрашивает). Выключено: собственное окно игры."},
    {Text::HomunTitle, "Homunculus", "Гомункул", "Гомункул"},
    {Text::HomunSkills, "Skills", "Навички", "Навыки"},
    {Text::HomunDelete, "Delete", "Видалити", "Удалить"},
    {Text::HomunClose0, "Hostile", "Ворожий", "Враждебный"},
    {Text::HomunClose1, "Unfriendly", "Неприязний", "Неприязненный"},
    {Text::HomunClose2, "Awkward", "Уникає", "Избегает"},
    {Text::HomunClose7, "Unknown", "Невідомо", "Неизвестно"},
    // 3.0.11-beta.161: the mercenary
    {Text::InterfaceMerc, "Mercenary card", "Картка найманця", "Карточка наёмника"},
    {Text::InterfaceMercHint,
     "The mercenary's window (Ctrl+R) as a card in the new look: its kind, name and level, HP and SP bars, the "
     "contract's end, loyalty, summons and kills, its eight stats, its skills (dismissing it small at the foot, the "
     "game asks first). While one is hired, its block under the party's panel: HP, SP, the contract's time left - a "
     "click opens the card. Off: the game's own window.",
     "Вікно найманця (Ctrl+R) карткою в новому вигляді: вид, ім'я й рівень, смуги HP і SP, кінець контракту, "
     "вірність, виклики й убиті, вісім характеристик, навички (звільнення — дрібно внизу, гра перепитує). "
     "Поки найманця найнято — його блок під панеллю групи: HP, SP, скільки лишилось контракту; клік відкриває "
     "картку. Вимкнено: власне вікно гри.",
     "Окно наёмника (Ctrl+R) карточкой в новом виде: вид, имя и уровень, полосы HP и SP, конец контракта, "
     "верность, вызовы и убитые, восемь характеристик, навыки (увольнение — мелко внизу, игра переспрашивает). "
     "Пока наёмник нанят — его блок под панелью группы: HP, SP, сколько осталось контракта; клик открывает "
     "карточку. Выключено: собственное окно игры."},
    {Text::MercArcher, "Mercenary archer", "Найманець-лучник", "Наёмник-лучник"},
    {Text::MercSpearman, "Mercenary spearman", "Найманець-списник", "Наёмник-копейщик"},
    {Text::MercSwordsman, "Mercenary swordsman", "Найманець-мечник", "Наёмник-мечник"},
    {Text::MercEnds, "Contract until", "Контракт до", "Контракт до"},
    {Text::MercLoyalty, "Loyalty", "Вірність", "Верность"},
    {Text::MercSummons, "Summons", "Викликів", "Вызовов"},
    {Text::MercKills, "Kills", "Убито", "Убито"},
    {Text::MercDismiss, "Dismiss", "Звільнити", "Уволить"},
    // 3.0.11-beta.162: the game's options
    {Text::InterfaceOptions, "Game options card", "Картка налаштувань гри", "Карточка настроек игры"},
    {Text::InterfaceOptionsHint,
     "The game's options window (Alt+O) as a card in the new look: the music and the sounds with their volumes, the "
     "cursor's snap to a target for the attack, the skills and the things, the game's windows opaque. Off: the game's "
     "own window.",
     "Вікно налаштувань гри (Alt+O) карткою в новому вигляді: музика і звуки з гучністю, прилипання курсора до цілі "
     "для атаки, навичок і речей, непрозорі вікна гри. Вимкнено: власне вікно гри.",
     "Окно настроек игры (Alt+O) карточкой в новом виде: музыка и звуки с громкостью, прилипание курсора к цели для "
     "атаки, навыков и вещей, непрозрачные окна игры. Выключено: собственное окно игры."},
    {Text::OptionsTitle, "Game options", "Налаштування гри", "Настройки игры"},
    {Text::OptionsMusic, "Music", "Музика", "Музыка"},
    {Text::OptionsSounds, "Sounds", "Звуки", "Звуки"},
    {Text::OptionsAttackSnap, "Snap: the attack", "Прилипання: атака", "Прилипание: атака"},
    {Text::OptionsSkillSnap, "Snap: the skills", "Прилипання: навички", "Прилипание: навыки"},
    {Text::OptionsItemSnap, "Snap: the things", "Прилипання: речі", "Прилипание: вещи"},
    {Text::OptionsOpaque, "The game's windows opaque", "Непрозорі вікна гри", "Непрозрачные окна игры"},
    {Text::OptionsSnapHint, "Snap: the cursor finds the nearest target by itself",
     "Прилипання: курсор сам наводиться на найближчу ціль", "Прилипание: курсор сам наводится на ближайшую цель"},
    // 3.0.11-beta.163: the friends and the party
    {Text::InterfaceSocial, "Friends and party card", "Картка друзів і групи", "Карточка друзей и группы"},
    {Text::InterfaceSocialHint,
     "The friends and party window (Alt+H, the party's button) as a card in the new look: two tabs, a row a person "
     "(online or not, the leader, where they are), whisper, delete or expel, leave the party, the settings. Off: the "
     "game's own window.",
     "Вікно друзів і групи (Alt+H, кнопка групи) карткою в новому вигляді: дві вкладки, рядок на людину (у грі чи ні, "
     "лідер, де зараз), шепіт, видалити чи вигнати, вийти з групи, налаштування. Вимкнено: власне вікно гри.",
     "Окно друзей и группы (Alt+H, кнопка группы) карточкой в новом виде: две вкладки, строка на человека (в игре или "
     "нет, лидер, где сейчас), шёпот, удалить или выгнать, выйти из группы, настройки. Выключено: собственное окно "
     "игры."},
    {Text::SocialFriends, "Friends", "Друзі", "Друзья"},
    {Text::SocialParty, "Party", "Група", "Группа"},
    {Text::SocialOffline, "offline", "не в грі", "не в игре"},
    {Text::SocialEmpty, "Nobody here", "Нікого немає", "Никого нет"},
    {Text::SocialNoParty, "You are in no party", "Ви не в групі", "Вы не в группе"},
    {Text::SocialPartyName, "Party: {1}", "Група: {1}", "Группа: {1}"},
    {Text::SocialFriendCount, "Friends: {1} of {2}", "Друзі: {1} з {2}", "Друзья: {1} из {2}"},
    {Text::SocialWhisper, "Whisper", "Шепіт", "Шёпот"},
    {Text::SocialExpel, "Expel", "Вигнати", "Выгнать"},
    {Text::SocialLeave, "Leave the party", "Вийти з групи", "Выйти из группы"},
    {Text::SocialSettings, "Settings", "Налаштування", "Настройки"},
    {Text::SocialDelete, "Delete", "Видалити", "Удалить"},
    // 3.0.11-beta.164: the server select
    {Text::InterfaceServers, "Server select", "Вибір сервера", "Выбор сервера"},
    {Text::InterfaceServersHint,
     "The game's server select between the login and the characters in the new look: a row a server (the only one "
     "chosen at once), two clicks go on. Off: the game's own window.",
     "Вибір сервера між входом і персонажами в новому вигляді: рядок на сервер (єдиний обрано одразу), подвійний клік "
     "— далі. Вимкнено: власне вікно гри.",
     "Выбор сервера между входом и персонажами в новом виде: строка на сервер (единственный выбран сразу), двойной "
     "клик — дальше. Выключено: собственное окно игры."},
    {Text::ServersTitle, "Server select", "Вибір сервера", "Выбор сервера"},
    {Text::ServersHint, "Choose the server", "Оберіть сервер", "Выберите сервер"},
    {Text::ServersNext, "Next", "Далі", "Далее"},
    {Text::ServersEmpty, "No server found", "Сервери не знайдено", "Серверы не найдены"},
    // 3.0.11-beta.165: the guild
    {Text::InterfaceGuild, "Guild card", "Картка гільдії", "Карточка гильдии"},
    {Text::InterfaceGuildHint,
     "The guild's window (Alt+G) as a card in the new look, all six tabs: its information, its members, the "
     "positions (rights and tax), the guild's skills (a plus to raise one), the expelled and the notice; changing the "
     "positions or the notice opens the game's own window. Off: the game's own window with all its tabs.",
     "Вікно гільдії (Alt+G) карткою в новому вигляді, усі шість вкладок: інформація, учасники, посади (права й "
     "податок), навички гільдії (плюс, щоб підняти), вигнані й оголошення; зміна посад чи оголошення відкриває власне "
     "вікно гри. Вимкнено: власне вікно гри з усіма вкладками.",
     "Окно гильдии (Alt+G) карточкой в новом виде, все шесть вкладок: информация, участники, должности (права и "
     "налог), навыки гильдии (плюс, чтобы поднять), изгнанные и объявление; изменение должностей или объявления "
     "открывает собственное окно игры. Выключено: собственное окно игры со всеми вкладками."},
    {Text::GuildTitle, "Guild", "Гільдія", "Гильдия"},
    {Text::GuildInfo, "Information", "Інформація", "Информация"},
    {Text::GuildMembers, "Members", "Учасники", "Участники"},
    {Text::GuildMaster, "Master", "Майстер", "Мастер"},
    {Text::GuildLevel, "Level", "Рівень", "Уровень"},
    {Text::GuildAverage, "Average level", "Середній рівень", "Средний уровень"},
    {Text::GuildExp, "Experience", "Досвід", "Опыт"},
    {Text::GuildTax, "Tax points", "Податок", "Налог"},
    {Text::GuildTerritory, "Territory", "Територія", "Территория"},
    {Text::GuildNone, "none", "немає", "нет"},
    {Text::GuildOnline, "{1} of {2} online", "{1} з {2} у грі", "{1} из {2} в игре"},
    // 3.0.11-beta.166: one's own open shop
    {Text::InterfaceMyShop, "Own shop card", "Картка своєї лавки", "Карточка своей лавки"},
    {Text::InterfaceMyShopHint,
     "One's own open shop as a card in the new look: the sign, a row a thing still for sale with its price and how "
     "many are left, what is left is worth, closing the shop (the controller's Y). Off: the game's own window.",
     "Своя відкрита лавка карткою в новому вигляді: вивіска, рядок на річ, що ще продається, з ціною і скільки "
     "лишилось, на скільки ще товару, закрити лавку (Y на контролері). Вимкнено: власне вікно гри.",
     "Своя открытая лавка карточкой в новом виде: вывеска, строка на вещь, что ещё продаётся, с ценой и сколько "
     "осталось, на сколько ещё товара, закрыть лавку (Y на контроллере). Выключено: собственное окно игры."},
    {Text::MyShopTitle, "My shop", "Моя лавка", "Моя лавка"},
    {Text::MyShopSign, "My shop: {1}", "Моя лавка: {1}", "Моя лавка: {1}"},
    {Text::MyShopLeft, "{1} left", "лишилось {1}", "осталось {1}"},
    {Text::MyShopWorth, "Still for sale", "Ще на продаж", "Ещё на продажу"},
    {Text::MyShopClose, "Close the shop", "Закрити лавку", "Закрыть лавку"},
    {Text::MyShopSold, "All sold", "Усе продано", "Всё продано"},
    // 3.0.16-beta.33: the shop's opening
    {Text::InterfaceVendSetup, "Opening a shop card", "Картка відкриття лавки", "Карточка открытия лавки"},
    {Text::InterfaceVendSetupHint,
     "Opening one's shop (Vending) as one card in the new look: the shop's name, the cart's things (a click puts one "
     "up for sale), the things for sale with a price field each and a cross to take one back, «Open the shop» with "
     "the game's own checks of the prices. Off at first: the game's own two windows.",
     "Відкриття своєї лавки (Vending) однією карткою в новому вигляді: назва лавки, речі з візка (клік виставляє на "
     "продаж), речі на продаж з полем ціни й хрестиком, щоб забрати, «Відкрити лавку» з перевірками цін самої гри. "
     "Спочатку вимкнено: два власні вікна гри.",
     "Открытие своей лавки (Vending) одной карточкой в новом виде: название лавки, вещи из тележки (клик выставляет "
     "на продажу), вещи на продажу с полем цены и крестиком, чтобы убрать, «Открыть лавку» с проверками цен самой "
     "игры. Сначала выключено: два собственных окна игры."},
    {Text::VendSetupTitle, "Open a shop", "Відкрити лавку", "Открыть лавку"},
    {Text::VendSetupSign, "Shop name", "Назва лавки", "Название лавки"},
    {Text::VendSetupSignHint, "Click and type the name", "Клацніть і напишіть назву", "Нажмите и напишите название"},
    {Text::VendSetupCart, "Cart · a click puts it up", "Візок · клік — на продаж", "Тележка · клик — на продажу"},
    {Text::VendSetupSale, "For sale {1} / {2}", "На продаж {1} / {2}", "На продажу {1} / {2}"},
    {Text::VendSetupOpen, "Open the shop", "Відкрити лавку", "Открыть лавку"},
    {Text::VendSetupCancel, "Cancel", "Скасувати", "Отмена"},
    {Text::VendSetupEmpty, "Nothing in the cart", "У візку нічого немає", "В тележке ничего нет"},
    {Text::VendSetupHint, "Click a thing in the cart to put it up", "Клацніть річ у візку, щоб виставити",
     "Нажмите вещь в тележке, чтобы выставить"},
    // 3.0.18-beta.104 (R-K5QG6Z): a thing for sale - its whole stack's price
    {Text::VendSetupTotal, "{1} z for all", "{1} z за все", "{1} z за все"},
    {Text::VendSetupPadPut, "put up", "виставити", "выставить"},
    {Text::VendSetupPadTake, "take back", "зняти", "снять"},
    {Text::VendSetupPadPrice, "price", "ціна", "цена"},
    // 3.0.11-beta.168: the whisper's settings
    {Text::InterfaceWhisperSetup, "Whisper settings", "Налаштування шепоту", "Настройки шёпота"},
    {Text::InterfaceWhisperSetupHint,
     "The whisper's settings (the friends' window's Settings) as a card of switches: a whisper's own window for "
     "strangers and for friends, the new whisper's sound. Off: the game's own window.",
     "Налаштування шепоту (кнопка налаштувань у вікні друзів) карткою перемикачів: окреме вікно для шепоту незнайомих "
     "і друзів, звук нового шепоту. Вимкнено: власне вікно гри.",
     "Настройки шёпота (кнопка настроек в окне друзей) карточкой переключателей: отдельное окно для шёпота "
     "незнакомых и друзей, звук нового шёпота. Выключено: собственное окно игры."},
    {Text::WhisperTitle, "Whisper settings", "Налаштування шепоту", "Настройки шёпота"},
    {Text::WhisperHint, "When someone whispers to you", "Коли вам шепочуть", "Когда вам шепчут"},
    {Text::WhisperStrangers, "Own window for strangers' whispers", "Окреме вікно для шепоту незнайомих",
     "Отдельное окно для шёпота незнакомых"},
    {Text::WhisperFriends, "Own window for friends' whispers", "Окреме вікно для шепоту друзів",
     "Отдельное окно для шёпота друзей"},
    {Text::WhisperSound, "A sound for a new whisper", "Звук нового шепоту", "Звук нового шёпота"},
    // 3.0.11-beta.169: the chat's macros
    {Text::InterfaceMacros, "Chat macros card", "Картка макросів чату", "Карточка макросов чата"},
    {Text::InterfaceMacrosHint,
     "The chat's macros (Alt+M) as a card in the new look: ten lines for Alt+1 to Alt+0, typed as before (the game's "
     "own fields), Save. Off: the game's own window.",
     "Макроси чату (Alt+M) карткою в новому вигляді: десять рядків для Alt+1…Alt+0, набираються як раніше (поля "
     "самої гри), зберегти. Вимкнено: власне вікно гри.",
     "Макросы чата (Alt+M) карточкой в новом виде: десять строк для Alt+1…Alt+0, набираются как раньше (поля самой "
     "игры), сохранить. Выключено: собственное окно игры."},
    {Text::MacroTitle, "Chat macros", "Макроси чату", "Макросы чата"},
    {Text::MacroHint, "Alt and a digit write the line into the chat", "Alt+цифра пише рядок у чат",
     "Alt+цифра пишет строку в чат"},
    {Text::MacroSave, "Save", "Зберегти", "Сохранить"},
    // 3.0.11-beta.204: the game's tip of the day
    {Text::InterfaceTips, "Tip of the day card", "Картка поради дня", "Карточка совета дня"},
    {Text::InterfaceTipsHint,
     "The game's tip of the day (on the first map after the start, when the game shows them) as a card in the new "
     "look: the tip, its number, back, next, close and the game's own box to show the tips at the start. Off: the "
     "game's own window.",
     "Порада дня гри (на першій мапі після запуску, якщо гра їх показує) карткою в новому вигляді: порада, її номер, "
     "назад, далі, закрити й власна галочка гри показувати поради під час входу. Вимкнено: власне вікно гри.",
     "Совет дня игры (на первой карте после запуска, если игра их показывает) карточкой в новом виде: совет, его "
     "номер, назад, далее, закрыть и собственная галочка игры показывать советы при входе. Выключено: собственное "
     "окно игры."},
    // 3.0.18-beta.180: the game's map loading bar
    {Text::InterfaceLoadingBar, "Map loading bar", "Смуга завантаження карти", "Полоса загрузки карты"},
    {Text::InterfaceLoadingBarHint,
     "The game's bar under the loading picture at a map change, in the new look at its place: sharp, its per cent in "
     "the interface's font. Off: the game's own bar.",
     "Смуга гри під картинкою завантаження при зміні карти — у новому вигляді на своєму місці: чітка, відсоток "
     "шрифтом інтерфейсу. Вимкнено: власна смуга гри.",
     "Полоса игры под картинкой загрузки при смене карты — в новом виде на своём месте: чёткая, процент шрифтом "
     "интерфейса. Выключено: собственная полоса игры."},
    {Text::TipTitle, "Tip of the day", "Порада дня", "Совет дня"},
    {Text::TipBack, "Back", "Назад", "Назад"},
    {Text::TipNext, "Next", "Далі", "Далее"},
    {Text::TipClose, "Close", "Закрити", "Закрыть"},
    {Text::TipStartup, "Show the tips when the game starts", "Показувати поради під час входу в гру",
     "Показывать советы при входе в игру"},
    // 3.0.11-beta.205: the game's emotion list
    {Text::InterfaceEmotes, "Emotions card", "Картка емоцій", "Карточка эмоций"},
    {Text::InterfaceEmotesHint,
     "The game's emotion list (Alt+L) as a card in the new look: the game's pictures over their commands; a click "
     "puts the emotion into the chat, a double click shows it at once. Off: the game's own window.",
     "Список емоцій гри (Alt+L) карткою в новому вигляді: малюнки гри над їхніми командами; клік ставить емоцію в "
     "чат, подвійний клік показує її одразу. Вимкнено: власне вікно гри.",
     "Список эмоций игры (Alt+L) карточкой в новом виде: картинки игры над их командами; клик ставит эмоцию в чат, "
     "двойной клик показывает её сразу. Выключено: собственное окно игры."},
    {Text::EmoteTitle, "Emotions", "Емоції", "Эмоции"},
    {Text::EmoteHint, "A click puts it into the chat, a double click shows it at once",
     "Клік — у чат, подвійний клік — одразу", "Клик — в чат, двойной клик — сразу"},
    // 3.0.11-beta.206: the game's command list
    {Text::InterfaceCommands, "Command list card", "Картка списку команд", "Карточка списка команд"},
    {Text::InterfaceCommandsHint,
     "The game's command list (Alt+Y) as a card in the new look: its switches with their state, its commands and the "
     "windows by their Alt keys on three tabs; a click does what the game's does. Off: the game's own window.",
     "Список команд гри (Alt+Y) карткою в новому вигляді: її перемикачі з їхнім станом, команди й вікна за клавішами "
     "Alt на трьох вкладках; клік робить те саме, що й у грі. Вимкнено: власне вікно гри.",
     "Список команд игры (Alt+Y) карточкой в новом виде: её переключатели с их состоянием, команды и окна по клавишам "
     "Alt на трёх вкладках; клик делает то же, что и в игре. Выключено: собственное окно игры."},
    {Text::CmdTitle, "Command list", "Список команд", "Список команд"},
    {Text::CmdTabSwitches, "Switches", "Перемикачі", "Переключатели"},
    {Text::CmdTabCommands, "Commands", "Команди", "Команды"},
    {Text::CmdTabWindows, "Windows", "Вікна", "Окна"},
    {Text::CmdOn, "on", "увімк.", "вкл."},
    {Text::CmdOff, "off", "вимк.", "выкл."},
    {Text::CmdWinChat, "Chat room", "Чат-кімната", "Чат-комната"},
    {Text::CmdWinStatus, "Status", "Характеристики", "Характеристики"},
    {Text::CmdWinDetail, "Detail level", "Деталізація", "Детализация"},
    {Text::CmdWinOptions, "Game options", "Налаштування гри", "Настройки игры"},
    {Text::CmdWinItems, "Items", "Речі", "Вещи"},
    {Text::CmdWinCart, "Cart", "Візок", "Тележка"},
    {Text::CmdWinSkills, "Skills", "Навички", "Навыки"},
    {Text::CmdWinEquip, "Equipment", "Спорядження", "Снаряжение"},
    {Text::CmdWinBasic, "Character", "Персонаж", "Персонаж"},
    {Text::CmdWinGuild, "Guild", "Гільдія", "Гильдия"},
    {Text::CmdWinParty, "Party settings", "Налаштування групи", "Настройки группы"},
    {Text::CmdWinFriendSetup, "Friend settings", "Налаштування друзів", "Настройки друзей"},
    {Text::CmdWinFriends, "Friends and party", "Друзі й група", "Друзья и группа"},
    {Text::CmdWinWhispers, "Whispers", "Шепіт", "Шёпот"},
    {Text::CmdWinMacros, "Chat macros", "Макроси чату", "Макросы чата"},
    {Text::CmdWinEmotions, "Emotions", "Емоції", "Эмоции"},
    {Text::CmdWinPet, "Pet", "Улюбленець", "Питомец"},
    // 3.0.11-beta.207: the game's chat rooms
    {Text::InterfaceChatRooms, "Chat room cards", "Картки чат-кімнат", "Карточки чат-комнат"},
    {Text::InterfaceChatRoomsHint,
     "The game's chat room windows (Alt+C) as cards in the new look: a new room's name, places, open or with a "
     "password - the game's own fields typed as before. Off: the game's own windows.",
     "Вікна чат-кімнат гри (Alt+C) картками в новому вигляді: назва нової кімнати, місця, відкрита чи з паролем — "
     "власні поля гри набираються як раніше. Вимкнено: власні вікна гри.",
     "Окна чат-комнат игры (Alt+C) карточками в новом виде: название новой комнаты, места, открытая или с паролем — "
     "собственные поля игры набираются как раньше. Выключено: собственные окна игры."},
    {Text::RoomNewTitle, "New chat room", "Нова чат-кімната", "Новая чат-комната"},
    {Text::RoomName, "Name", "Назва", "Название"},
    {Text::RoomPlaces, "Places", "Місць", "Мест"},
    {Text::RoomOpen, "Open", "Відкрита", "Открытая"},
    {Text::RoomLocked, "With a password", "З паролем", "С паролем"},
    {Text::RoomPassword, "Password (4 to 8 characters)", "Пароль (від 4 до 8 знаків)", "Пароль (от 4 до 8 знаков)"},
    {Text::RoomCreate, "Create", "Створити", "Создать"},
    {Text::RoomCancel, "Cancel", "Скасувати", "Отмена"},
    // 3.0.11-beta.208: the room's own card and its settings
    {Text::RoomSetupTitle, "Room settings", "Налаштування кімнати", "Настройки комнаты"},
    {Text::RoomSave, "Save", "Зберегти", "Сохранить"},
    {Text::RoomMembers, "Members", "Учасники", "Участники"},
    {Text::RoomSetup, "Settings", "Налаштування", "Настройки"},
    {Text::RoomLeave, "Leave", "Вийти", "Выйти"},
    {Text::RoomKick, "Kick out", "Вигнати", "Выгнать"},
    {Text::RoomOwner, "Make the owner", "Зробити власником", "Сделать владельцем"},
    {Text::RoomFriend, "Add as a friend", "У друзі", "В друзья"},
    // 3.0.11-beta.209: Alt+D - the driver's console
    {Text::InterfaceDetailConsole, "Alt+D opens the graphics console", "Alt+D відкриває консоль графіки",
     "Alt+D открывает консоль графики"},
    {Text::InterfaceDetailConsoleHint,
     "The game's own detail window (Alt+D: object detail, shadows, trilinear) changes almost nothing with this "
     "renderer - Alt+D opens this console with the real settings instead (as Ctrl+` does). Off: the game's window.",
     "Власне вікно деталізації гри (Alt+D: деталізація об'єктів, тіні, трилінійна фільтрація) з цим рендерером майже "
     "нічого не змінює — Alt+D натомість відкриває цю консоль зі справжніми налаштуваннями (як Ctrl+`). Вимкнено: "
     "вікно гри.",
     "Собственное окно детализации игры (Alt+D: детализация объектов, тени, трилинейная фильтрация) с этим "
     "рендерером почти ничего не меняет — Alt+D вместо него открывает эту консоль с настоящими настройками (как "
     "Ctrl+`). Выключено: окно игры."},
    // 3.0.11-beta.210: the guild card's other tabs
    {Text::GuildTabPositions, "Positions", "Посади", "Должности"},
    {Text::GuildTabSkills, "Skills", "Навички", "Навыки"},
    {Text::GuildTabExpelled, "Expelled", "Вигнані", "Изгнанные"},
    {Text::GuildTabNotice, "Notice", "Оголошення", "Объявление"},
    // 3.0.11-beta.211: the game's popup menus
    {Text::InterfacePopups, "Popup menus", "Спливні меню", "Всплывающие меню"},
    {Text::InterfacePopupsHint,
     "The game's popup menus (a right click on a player, a chat room member, a guild member) in the new look, at the "
     "click; their items as the game gives them. Off: the game's own menus.",
     "Спливні меню гри (правий клік по гравцю, учаснику чат-кімнати, учаснику гільдії) у новому вигляді, біля "
     "кліку; пункти як їх дає гра. Вимкнено: власні меню гри.",
     "Всплывающие меню игры (правый клик по игроку, участнику чат-комнаты, участнику гильдии) в новом виде, у "
     "клика; пункты как их даёт игра. Выключено: собственные меню игры."},
    // 3.0.11-beta.212: the game's 1:1 whisper windows
    {Text::InterfaceWhispers, "Whisper windows", "Вікна шепоту", "Окна шёпота"},
    {Text::InterfaceWhispersHint,
     "The game's 1:1 whisper windows as cards in the new look: the partner, the lines, the game's own line to type "
     "into (Enter sends as before) and a send button. Off: the game's own windows.",
     "Вікна шепоту гри (1:1) картками в новому вигляді: співрозмовник, рядки, власний рядок гри для набору (Enter "
     "надсилає як і раніше) і кнопка надсилання. Вимкнено: власні вікна гри.",
     "Окна шёпота игры (1:1) карточками в новом виде: собеседник, строки, собственная строка игры для набора (Enter "
     "отправляет как и раньше) и кнопка отправки. Выключено: собственные окна игры."},
    {Text::WhisperCardWith, "Whisper: {1}", "Шепіт: {1}", "Шёпот: {1}"},
    {Text::WhisperCardSend, "Send", "Надіслати", "Отправить"},
    // 3.0.11-beta.214: the chat wheel
    {Text::WheelPhrases, "Phrases", "Фрази", "Фразы"},
    {Text::WheelEmotes, "Emotions", "Емоції", "Эмоции"},
    {Text::WheelNear, "Everyone", "Усім", "Всем"},
    {Text::WheelParty, "Party", "Групі", "Группе"},
    {Text::WheelGuild, "Guild", "Гільдії", "Гильдии"},
    {Text::WheelKeyboard, "{1} keyboard", "{1} клавіатура", "{1} клавиатура"},
    {Text::WheelVoice, "{1} voice", "{1} голос", "{1} голос"},
    {Text::WheelHint, "Stick: choose · let go: send · {1}/{2}: page · {3}/{4}: channel",
     "Стик: вибір · відпустіть: надіслати · {1}/{2}: сторінка · {3}/{4}: канал",
     "Стик: выбор · отпустите: отправить · {1}/{2}: страница · {3}/{4}: канал"},
    {Text::WheelPhrase1, "Hi!", "Привіт!", "Привет!"},
    {Text::WheelPhrase2, "Thanks!", "Дякую!", "Спасибо!"},
    {Text::WheelPhrase3, "Heal please", "Хіл, будь ласка", "Хил, пожалуйста"},
    {Text::WheelPhrase4, "Coming", "Йду", "Иду"},
    {Text::WheelPhrase5, "Wait", "Зачекайте", "Подождите"},
    {Text::WheelPhrase6, "Yes", "Так", "Да"},
    {Text::WheelPhrase7, "No", "Ні", "Нет"},
    {Text::WheelPhrase8, "Buff please", "Бафніть, будь ласка", "Бафните, пожалуйста"},
    {Text::PadChatHeading, "Chat wheel", "Колесо чату", "Колесо чата"},
    {Text::PadChatWheel, "Chat wheel (hold the talk button)", "Колесо чату (утримати кнопку розмови)",
     "Колесо чата (удерживать кнопку разговора)"},
    {Text::PadChatWheelHint,
     "The talk button (the D-pad down) held opens a wheel: the right stick points at a phrase or an emotion, letting "
     "the button go sends it; LB and RB the page, LT and RT everyone, the party or the guild; Y the Windows voice "
     "typing into the chat's line. A tap still talks to the nearest NPC.",
     "Утримана кнопка розмови (хрестовина вниз) відкриває колесо: правий стик вказує на фразу чи емоцію, відпущена "
     "кнопка надсилає її; LB і RB — сторінка, LT і RT — усім, групі чи гільдії; Y — голосове введення Windows у "
     "рядок чату. Коротке натискання, як і раніше, — розмова з найближчим NPC.",
     "Удержанная кнопка разговора (крестовина вниз) открывает колесо: правый стик указывает на фразу или эмоцию, "
     "отпущенная кнопка отправляет её; LB и RB — страница, LT и RT — всем, группе или гильдии; Y — голосовой ввод "
     "Windows в строку чата. Короткое нажатие, как и раньше, — разговор с ближайшим NPC."},
    {Text::PadChatPhrase, "Phrase {1}", "Фраза {1}", "Фраза {1}"},
    {Text::PadChatEmotes, "The wheel's emotions", "Емоції колеса", "Эмоции колеса"},
    {Text::PadChatEmotesHint,
     "Ten emotion commands by spaces, as the chat takes them (/! /? /ho ...); empty - the game's own Alt+1 to Alt+0.",
     "Десять команд емоцій через пробіл, як їх приймає чат (/! /? /ho ...); порожньо — власні Alt+1…Alt+0 гри.",
     "Десять команд эмоций через пробел, как их принимает чат (/! /? /ho ...); пусто — собственные Alt+1…Alt+0 "
     "игры."},
    {Text::OskSpace, "space", "пробіл", "пробел"},
    {Text::OskSend, "Send", "Надіслати", "Отправить"},
    // 3.0.18-beta.134 (the pad analysis: chat): {8} the channel, {9} the reply, the right stick the history
    {Text::OskHint,
     "{1} type · {2} erase · {3} space · {4}/{5} language · {6} Shift · Start send · {7} close · {8} channel · "
     "{9} reply · right stick: history",
     "{1} ввести · {2} стерти · {3} пробіл · {4}/{5} мова · {6} Shift · "
     "Start надіслати · {7} закрити · {8} канал · {9} відповісти · правий стик: історія",
     "{1} ввести · {2} стереть · {3} пробел · {4}/{5} язык · {6} Shift · "
     "Start отправить · {7} закрыть · {8} канал · {9} ответить · правый стик: история"},
    // 3.0.11-beta.141: the character block's experience (forms: one, a few, many)
    {Text::CharSkillPoints, "skill point|skill points", "очко навичок|очки навичок|очок навичок",
     "очко навыков|очка навыков|очков навыков"},
    {Text::CharExpMax, "max", "макс.", "макс."},
    {Text::CharHour, "h", "год", "ч"},
    {Text::CharMinute, "min", "хв", "мин"},
    {Text::CharToLevel, "To the level {1}", "До рівня {1}", "До уровня {1}"},
    {Text::CharLevelIn, "Level in \xE2\x89\x88 {1}", "Рівень через \xE2\x89\x88 {1}", "Уровень через \xE2\x89\x88 {1}"},
    {Text::CharPerHour, "An hour +{1}", "За годину +{1}", "За час +{1}"},
    {Text::CharSession, "This session +{1}", "За сесію +{1}", "За сессию +{1}"},
    {Text::CharLastGain, "Last gain +{1}", "Остання здобич +{1}", "Последняя добыча +{1}"},
    {Text::CharFold, "Alt+V \xE2\x80\x94 fold", "Alt+V \xE2\x80\x94 згорнути", "Alt+V \xE2\x80\x94 свернуть"},
    {Text::CharUnfold, "Alt+V \xE2\x80\x94 unfold", "Alt+V \xE2\x80\x94 розгорнути", "Alt+V \xE2\x80\x94 развернуть"},
    // 3.0.11-beta.142: a ground skill's cells on the ground
    {Text::InterfaceSkillCells, "Skill cells on the ground", "Клітинки навичок на землі", "Клетки навыков на земле"},
    {Text::InterfaceSkillCellsHint,
     "Where a ground skill goes: Fire Wall's 3 or 5 cells, Storm Gust's square, Sanctuary, a trap - the cells the "
     "server makes, under the mouse while the game waits for the place, and flashed after the cast from the "
     "controller and the mouse.",
     "Куди ляже навичка по землі: 3 чи 5 клітинок Fire Wall, квадрат Storm Gust, Sanctuary, пастка — клітинки, які "
     "створить сервер: під мишею, поки гра чекає місце, і коротко після касту з геймпада й мишею.",
     "Куда ляжет умение по земле: 3 или 5 клеток Fire Wall, квадрат Storm Gust, Sanctuary, ловушка — клетки, которые "
     "создаст сервер: под мышью, пока игра ждёт место, и коротко после каста с геймпада и мышью."},
    // 3.0.16-beta.7
    {Text::InterfaceMouseCell, "Cell under the mouse", "Клітинка під мишею", "Клетка под мышью"},
    {Text::InterfaceMouseCellHint,
     "The cell of the ground under the mouse in the new look: a soft light square with a thin edge, as the skills' "
     "cells, in place of the game's own.",
     "Клітинка землі під мишею в новому вигляді: м'який світлий квадрат із тонким краєм, як клітинки навичок, "
     "замість ігрової.",
     "Клетка земли под мышью в новом виде: мягкий светлый квадрат с тонким краем, как клетки навыков, вместо "
     "игровой."},
    {Text::InterfaceSigns, "Signs over the characters", "Таблички над персонажами", "Таблички над персонажами"},
    {Text::InterfaceSignsHint,
     "The shops' and the chat rooms' signs in the new look: a dark plate with the title, gold for a shop, blue for a "
     "room with its places, grey for a locked one. A click opens them as before.",
     "Таблички крамниць і чат-кімнат у новому вигляді: темна плашка з назвою, золота для крамниці, синя для кімнати "
     "з її місцями, сіра для закритої. Клік відкриває їх, як і раніше.",
     "Таблички лавок и чат-комнат в новом виде: тёмная плашка с названием, золотая для лавки, синяя для комнаты с её "
     "местами, серая для закрытой. Клик открывает их, как и раньше."},
    {Text::InterfaceGroundCounts, "Walls and traps on the hotbar", "Стіни й пастки на панелі",
     "Стены и ловушки на панели"},
    {Text::InterfaceGroundCountsHint,
     "On a ground skill's slot: how many of its walls stand - Fire Wall 2/3, Quagmire, Fire Pillar - with the slot's "
     "edge flashing when one burnt out, so you put it again; on a trap's slot the Traps left in the bag.",
     "На клітинці навички по землі: скільки її стін стоїть — Fire Wall 2/3, Quagmire, Fire Pillar — і край клітинки "
     "блимає, коли одна прогоріла, щоб поставити знову; на клітинці пастки — скільки предметів Trap лишилося в сумці.",
     "На ячейке умения по земле: сколько его стен стоит — Fire Wall 2/3, Quagmire, Fire Pillar — и край ячейки мигает, "
     "когда одна прогорела, чтобы поставить снова; на ячейке ловушки — сколько предметов Trap осталось в сумке."},
    {Text::InterfacePadWalk, "The controller walks", "Хід контролером", "Ход контроллером"},  // 3.0.11-beta.88
    {Text::InterfacePadWalkHint,  // 3.0.11-beta.102: by the map's cells, the default
     "How the left stick walks. With an arrow: by the map's cells, the same at any camera's distance - the character "
     "goes where the stick points a few cells ahead of it, asked on before it gets there (no stops on the way), slides "
     "along walls, and stops at the next cell when the stick is let go; an arrow on the ground shows the way, the "
     "game's cursor hidden while you play with the controller. With the cursor: the game's cursor held out, as before.",
     "Як ходить лівий стік. Стрілкою: клітинками карти, однаково за будь-якої відстані камери - персонаж іде, куди "
     "показує стік, на кілька клітинок уперед, і крок просимо далі, поки він не дійшов (без зупинок дорогою), уздовж "
     "стін ковзає, а відпустили стік - стає на наступній клітинці; стрілка на землі показує шлях, курсор гри схований, "
     "поки граєте контролером. Курсором: курсор гри тримається збоку, як раніше.",
     "Как ходит левый стик. Стрелкой: клетками карты, одинаково при любом расстоянии камеры - персонаж идёт, куда "
     "показывает стик, на несколько клеток вперёд, и шаг просим дальше, пока он не дошёл (без остановок по пути), "
     "вдоль стен скользит, а отпустили стик - встаёт на следующей клетке; стрелка на земле показывает путь, курсор "
     "игры спрятан, пока играете контроллером. Курсором: курсор игры держится сбоку, как раньше."},
    {Text::InterfacePadWalkMouse, "With the cursor (as before)", "Курсором (як раніше)", "Курсором (как раньше)"},
    {Text::InterfacePadWalkArrow, "With an arrow, no mouse", "Стрілкою, без миші", "Стрелкой, без мыши"},
    {Text::InterfaceWalkArrow, "Walking arrow", "Стрілка ходу", "Стрелка хода"},  // 3.0.11-beta.103
    {Text::InterfaceWalkArrowHint,
     "How the arrow on the ground looks while the left stick walks («The controller walks»: with an arrow). Chevrons: "
     "three marks running on to the cell, a gold diamond in it. Slim arrow: a thin line widening to a head, the cell's "
     "corners. The concept's arrow: a wide one with a gold edge and the cell. Ring and pointer: a ring round the feet "
     "with a gold pointer on it. Dots of the way: a dotted path and a gold ring. The cell alone: its gold frame and a "
     "small wedge at the feet. A wall in the way shows as a red bar at the end.",
     "Як виглядає стрілка на землі, коли ходить лівий стік («Хід контролером»: стрілкою). Шеврони: три позначки "
     "біжать до клітинки, у ній золотий ромб. Тонка стрілка: тонка лінія розширюється до вістря, кути клітинки. "
     "Стрілка з концепту: широка, із золотою облямівкою, і клітинка. Кільце-компас: кільце навколо ніг і золотий "
     "покажчик на ньому. Точки шляху: пунктир і золоте кільце. Лише клітинка: її золота рамка й невеликий клин біля "
     "ніг. Стіна на шляху - червона риска в кінці.",
     "Как выглядит стрелка на земле, когда ходит левый стик («Ход контроллером»: стрелкой). Шевроны: три отметки "
     "бегут к клетке, в ней золотой ромб. Тонкая стрелка: тонкая линия расширяется к острию, углы клетки. Стрелка из "
     "концепта: широкая, с золотой каймой, и клетка. Кольцо-компас: кольцо вокруг ног и золотой указатель на нём. "
     "Точки пути: пунктир и золотое кольцо. Только клетка: её золотая рамка и небольшой клин у ног. Стена на пути - "
     "красная черта в конце."},
    {Text::WalkArrowChevrons, "Chevrons", "Шеврони", "Шевроны"},
    {Text::WalkArrowSlim, "Slim arrow", "Тонка стрілка", "Тонкая стрелка"},
    {Text::WalkArrowConcept, "The concept's arrow", "Стрілка з концепту", "Стрелка из концепта"},
    {Text::WalkArrowRing, "Ring and pointer", "Кільце-компас", "Кольцо-компас"},
    {Text::WalkArrowDots, "Dots of the way", "Точки шляху", "Точки пути"},
    {Text::WalkArrowCell, "The cell alone", "Лише клітинка", "Только клетка"},
    {Text::WalkArrowOff, "None", "Немає", "Нет"},
    // 3.0.11-beta.198 (the tester: «курсор миші стандартний не дуже підходить в загальний стиль»; the owner: «да,
    // сделай возможность курсор менять»)
    {Text::InterfaceCursor, "Cursor", "Курсор", "Курсор"},
    {Text::InterfaceCursorHint,
     "The mouse cursor: the game's own, or in the new interface's style - dark glass in a gold edge, or white in a "
     "dark one. A shape for each of the game's: the arrow, talk, attack, pick up, a warp, a target, the camera turned, "
     "not allowed.",
     "Курсор миші: власний курсор гри або в стилі нового інтерфейсу — темне скло в золотому краї чи білий у темному. "
     "Своя фігура на кожен випадок гри: стрілка, розмова, атака, підняти, портал, ціль, поворот камери, заборона.",
     "Курсор мыши: собственный курсор игры или в стиле нового интерфейса — тёмное стекло в золотом крае или белый в "
     "тёмном. Своя фигура на каждый случай игры: стрелка, разговор, атака, поднять, портал, цель, поворот камеры, "
     "запрет."},
    {Text::CursorGame, "The game's", "Гри", "Игры"},
    {Text::CursorDark, "Dark", "Темний", "Тёмный"},
    {Text::CursorLight, "Light", "Світлий", "Светлый"},
    {Text::InterfaceWalkMark, "The cell it goes to", "Клітинка, куди йти", "Клетка, куда идти"},  // 3.0.11-beta.105
    {Text::InterfaceWalkMarkHint,
     "A mark at the walking arrow's end where the character goes: a gold diamond (chevrons), the cell's corners (slim "
     "arrow), the cell's frame (the concept's arrow), a small gold ring (ring and pointer, dots of the way). Off: the "
     "arrow alone. «The cell alone» always shows its cell - it is that look.",
     "Позначка в кінці стрілки ходу, куди йде персонаж: золотий ромб (шеврони), кути клітинки (тонка стрілка), рамка "
     "клітинки (стрілка з концепту), маленьке золоте кільце (кільце-компас, точки шляху). Вимкнено: лише стрілка. "
     "«Лише клітинка» завжди показує свою клітинку - це і є її вигляд.",
     "Отметка в конце стрелки хода, куда идёт персонаж: золотой ромб (шевроны), углы клетки (тонкая стрелка), рамка "
     "клетки (стрелка из концепта), маленькое золотое кольцо (кольцо-компас, точки пути). Выключено: только стрелка. "
     "«Только клетка» всегда показывает свою клетку - это и есть её вид."},
    {Text::InterfacePadTiles, "Start opens the windows' tile", "Start відкриває плитку вікон",
     "Start открывает плитку окон"},  // 3.0.11-beta.90
    {Text::InterfacePadTilesHint,
     "Start on the controller opens a tile of the game's windows (items, status, skills, equipment, party, the map, "
     "«Reference», the game's options, the controller's own menu): the D-pad chooses, A opens, B closes. Off: Start "
     "opens the controller's own menu, as before.",
     "Start на контролері відкриває плитку вікон гри (речі, характеристики, уміння, спорядження, група, карта, "
     "«Довідник», налаштування гри, власне меню контролера): хрестовина вибирає, A відкриває, B закриває. Вимкнено: "
     "Start відкриває меню контролера, як раніше.",
     "Start на контроллере открывает плитку окон игры (вещи, характеристики, умения, снаряжение, группа, карта, "
     "«Справочник», настройки игры, собственное меню контроллера): крестовина выбирает, A открывает, B закрывает. "
     "Выключено: Start открывает меню контроллера, как раньше."},
    {Text::TilesTitle, "Menu", "Меню", "Меню"},
    {Text::TileSkills, "Skills", "Уміння", "Умения"},
    {Text::TileEquip, "Equipment", "Спорядження", "Снаряжение"},
    {Text::TileMap, "Map", "Карта", "Карта"},
    {Text::TileOptions, "Game options", "Налаштування гри", "Настройки игры"},
    {Text::TilesOpen, "Open", "Відкрити", "Открыть"},
    {Text::TilesClose, "Close", "Закрити", "Закрыть"},
    {Text::AssignTitle, "Put on a button", "Призначити на кнопку", "Назначить на кнопку"},  // 3.0.11-beta.91
    {Text::AssignPut, "Put", "Поставити", "Поставить"},
    {Text::AssignCancel, "Cancel", "Скасувати", "Отмена"},
    {Text::AssignEmpty, "empty", "порожньо", "пусто"},
    {Text::ItemsPadUse, "use", "використати", "использовать"},
    {Text::ItemsPadWear, "wear", "вдягнути", "надеть"},
    {Text::ItemsPadCard, "into a slot", "у слот", "в слот"},
    {Text::ItemsPadMore, "{1}: take · {2}: its card", "{1}: взяти · {2}: картка",
     "{1}: взять · {2}: карточка"},  // 3.0.11-beta.121: X carries it (was «призначити»)
    // 3.0.11-beta.33
    {Text::InterfaceMinimapWay, "The way on the ground", "Шлях на землі", "Путь на земле"},
    {Text::InterfaceMinimapWayHint,
     "The way «Reference» leads you drawn on the location itself, not only on the minimap.",
     "Шлях, яким вас веде «Довідник», просто на землі локації, не лише на мінікарті.",
     "Путь, которым вас ведёт «Справочник», прямо на земле локации, не только на миникарте."},
    {Text::InterfaceMinimapWaySteps, "Footprints", "Сліди", "Следы"},
    {Text::InterfaceMinimapWayLine, "A line", "Смуга", "Полоса"},
    {Text::InterfaceMinimapWayOff, "Off", "Вимкнено", "Выключено"},
    // 3.0.11-beta.35: the buffs' column (stage 7 of the new interface)
    {Text::InterfaceBuffs, "Buffs on the character", "Бафи на персонажі", "Бафы на персонаже"},
    {Text::InterfaceBuffsHint,  // 3.0.11-beta.231: no glass, the time gone dark over the icon
     "The character's buffs in a column at the screen's right edge under the minimap, or a row under it: the icons "
     "straight on the picture, the time gone darkening each, its numbers over its foot; favourites first and larger "
     "(a click on an icon), debuffs last with a red outline; the round button over them shows the names. As in the "
     "game: the game's own icons.",
     "Бафи персонажа стовпчиком біля правого краю під мінікартою або рядком під нею: значки просто на картинці, "
     "минулий час затемнює кожен, цифри — на його нижньому краї; обрані — першими й більшими (клік по значку), "
     "дебафи — останніми, у червоній обвідці; кругла кнопка над ними показує назви. Як у грі — значки самої гри.",
     "Бафы персонажа столбиком у правого края под миникартой или строкой под ней: значки прямо на картинке, "
     "прошедшее время затемняет каждый, цифры — на его нижнем крае; избранные — первыми и крупнее (клик по значку), "
     "дебафы — последними, в красной обводке; круглая кнопка над ними показывает названия. Как в игре — значки самой "
     "игры."},
    {Text::InterfaceBuffsOff, "As in the game", "Як у грі", "Как в игре"},
    {Text::InterfaceBuffsColumn, "Column", "Стовпчиком", "Столбиком"},
    {Text::InterfaceBuffsRow, "Row", "Рядком", "Строкой"},
    // 3.0.11-beta.186 (the tester: «варто додати заглавні букви ... "Назви або Іконки"?»): the column's two looks
    {Text::BuffsNames, "Names", "Назви", "Названия"},
    {Text::BuffsHideNames, "Icons", "Іконки", "Иконки"},
    {Text::BuffsMore, "{1} more", "ще {1}", "ещё {1}"},
    {Text::BuffsStar, "click: to favourites", "клік — в обрані", "клик — в избранное"},
    {Text::BuffsUnstar, "click: off favourites", "клік — прибрати з обраних", "клик — убрать из избранного"},
    // 3.0.11-beta.63: what the buffs' column does not hold (the concept's question 17)
    {Text::InterfaceBuffsOverflow, "Buffs that do not fit", "Бафи, що не вміщаються", "Бафы, которые не помещаются"},
    {Text::InterfaceBuffsOverflowHint,
     "When the column is full: the buffs that do not fit hide under the «N more» button (a click shows them all "
     "with their names) or stand in a second column on its left (the row: a second row under it). Debuffs are never "
     "hidden: those that do not fit open a column of their own.",
     "Коли стовпчик заповнений: бафи, що не вміщаються, ховаються під кнопку «ще N» (клік показує всі з назвами) "
     "або стають у другий стовпчик ліворуч від нього (рядок — у другий рядок під ним). Дебафи не ховаються ніколи: "
     "ті, що не вмістилися, відкривають свій стовпчик.",
     "Когда столбик заполнен: бафы, которые не помещаются, прячутся под кнопку «ещё N» (клик показывает все с "
     "названиями) или встают во второй столбик слева от него (строка — во вторую строку под ней). Дебафы не "
     "прячутся никогда: те, что не поместились, открывают свой столбик."},
    {Text::InterfaceBuffsOverflowMore, "Under «N more»", "Під «ще N»", "Под «ещё N»"},
    {Text::InterfaceBuffsOverflowColumn, "In a second column", "У другий стовпчик", "Во второй столбик"},
    // 3.0.11-beta.36: the character block (stage 7 of the new interface)
    {Text::InterfaceCharacter, "Character block", "Блок персонажа", "Блок персонажа"},
    {Text::InterfaceCharacterHint,
     "The character at the screen's top left instead of the game's window with HP and SP: the full block (the "
     "portrait, the levels, the weight, the zeny, HP, SP, the experience, the windows' buttons), the compact one - "
     "with bars or rings - or a ring icon; a click on the portrait goes round them, Alt+V the full or the small one, "
     "as the game's window. Low HP: red and a short warning. As in the game: the game's own window.",
     "Персонаж у лівому верхньому куті замість вікна гри з HP і SP: повний блок (портрет, рівні, вага, зені, HP, SP, "
     "досвід, кнопки вікон), компактний — смужками або кільцями — чи значок-кільця; клік по портрету перемикає їх, "
     "Alt+V — повний чи малий, як вікно гри. Мало HP — червоним і коротке попередження. Як у грі — вікно самої гри.",
     "Персонаж в левом верхнем углу вместо окна игры с HP и SP: полный блок (портрет, уровни, вес, зени, HP, SP, "
     "опыт, кнопки окон), компактный — полосками или кольцами — или значок-кольца; клик по портрету переключает их, "
     "Alt+V — полный или малый, как окно игры. Мало HP — красным и короткое предупреждение. Как в игре — окно самой "
     "игры."},
    {Text::InterfaceCharacterOff, "As in the game", "Як у грі", "Как в игре"},
    {Text::InterfaceCharacterBars, "Bars", "Смужками", "Полосками"},
    {Text::InterfaceCharacterRings, "Rings", "Кільцями", "Кольцами"},
    {Text::CharWeight, "Weight", "Вага", "Вес"},
    {Text::CharPoints, "point|points", "очко|очки|очок", "очко|очка|очков"},  // forms: one, a few, many
    {Text::CharLowHp, "Low HP \xC2\xB7 {1} %", "Мало HP \xC2\xB7 {1} %", "Мало HP \xC2\xB7 {1} %"},
    {Text::CharArrows, "Arrows", "Стріли", "Стрелы"},  // 3.0.11-beta.114: the ammo worn running low
    {Text::CharAmmo, "Ammo", "Боєприпаси", "Боеприпасы"},
    {Text::CharStats, "Stats \xC2\xB7 Alt+A", "Характеристики \xC2\xB7 Alt+A", "Характеристики \xC2\xB7 Alt+A"},
    {Text::CharEquip, "Equipment \xC2\xB7 Alt+Q", "Спорядження \xC2\xB7 Alt+Q", "Снаряжение \xC2\xB7 Alt+Q"},
    {Text::CharItems, "Items \xC2\xB7 Alt+E", "Речі \xC2\xB7 Alt+E", "Вещи \xC2\xB7 Alt+E"},
    {Text::CharSkills, "Skills \xC2\xB7 Alt+S", "Навички \xC2\xB7 Alt+S", "Навыки \xC2\xB7 Alt+S"},
    {Text::CharFriends, "Friends \xC2\xB7 Alt+Z", "Друзі \xC2\xB7 Alt+Z", "Друзья \xC2\xB7 Alt+Z"},
    {Text::CharOptions, "Options \xC2\xB7 Alt+O", "Налаштування \xC2\xB7 Alt+O", "Настройки \xC2\xB7 Alt+O"},
    // 3.0.11-beta.38: the target's card (stage 7 of the new interface)
    {Text::InterfaceTarget, "Target card", "Картка цілі", "Карточка цели"},
    {Text::InterfaceTargetHint,
     "The monster the character fights, in a short card at the screen's top middle: its picture, name, level and "
     "element; pointed at - its race, size, HP, experience, the elements that hurt it most and those that heal it. "
     "A click opens its card in the reference.",
     "Монстр, з яким б'ється персонаж, у короткій картці вгорі посередині: картинка, ім'я, рівень і стихія; "
     "наведіть — раса, розмір, HP, досвід, стихії, що б'ють його найсильніше, і ті, що його лікують. Клік відкриває "
     "його картку в довіднику.",
     "Монстр, с которым дерётся персонаж, в короткой карточке вверху посередине: картинка, имя, уровень и стихия; "
     "наведите — раса, размер, HP, опыт, стихии, бьющие его сильнее всего, и те, что его лечат. Клик открывает его "
     "карточку в справочнике."},
    {Text::TargetWeak, "Weak to: {1}", "Слабкий до: {1}", "Слаб к: {1}"},
    {Text::TargetHeals, "Healed by: {1}", "Лікують: {1}", "Лечат: {1}"},
    {Text::TargetBook, "Click: its card in the reference", "Клік — картка в довіднику",
     "Клик — карточка в справочнике"},
    // 3.0.11-beta.39: the party's panel (stage 7 of the new interface)
    {Text::InterfaceParty, "Party", "Група", "Группа"},
    {Text::InterfacePartyHint,
     "The party at the screen's top left under the character: each member's name (a crown on the leader), HP per "
     "cent and its bar; low HP red with an exclamation mark; on another map - its map, offline - the word. The server "
     "sends no SP.",
     "Група ліворуч угорі під персонажем: ім'я кожного (корона в лідера), відсоток HP і його смужка; мало HP — "
     "червоним зі знаком оклику; на іншій карті — назва карти, не в грі — слово офлайн. SP сервер не надсилає.",
     "Группа слева вверху под персонажем: имя каждого (корона у лидера), процент HP и его полоска; мало HP — красным "
     "со знаком восклицания; на другой карте — название карты, не в игре — слово оффлайн. SP сервер не присылает."},
    {Text::PartyOffline, "offline", "офлайн", "оффлайн"},
    // 3.0.11-beta.40: the hotkeys' bar (stage 8 of the new interface)
    {Text::InterfaceHotbar, "Hotbar", "Панель навичок", "Панель навыков"},
    {Text::InterfaceHotbarHint,
     "The game's hotkeys bar at the bottom middle: F1-F9 of the row (F12 goes round them), in the battle mode its "
     "three rows Q, A, Z; each cell's key, a skill's level, an item's count in the bag; a click uses it. Skills and "
     "items are dragged onto its cells; a cell's icon held and dragged to another cell swaps them, dragged off the "
     "bar takes it off; a right click shows its details; the swords button turns the battle mode on and off. A "
     "skill waiting for its target has a gold frame; one with too little SP a blue SP; one the map forbids a crossed "
     "circle, the reason in its tip.",
     "Панель клавіш гри внизу посередині: F1-F9 ряду (F12 перемикає ряди), у бойовому режимі — три ряди Q, A, Z; у "
     "клітинці клавіша, рівень навички, кількість предмета в сумці; клік — використати. Навички й предмети "
     "перетягуються на клітинки; значок, утриманий і перетягнутий на іншу клітинку, міняється з нею місцями, а за "
     "межі панелі — прибирається; ПКМ показує опис; кнопка з мечами вмикає й вимикає бойовий режим. Навичка, що "
     "чекає ціль, — у золотій рамці; якщо SP не вистачає — синя плашка SP; заборонене на карті — перекреслене коло, "
     "причина в підказці.",
     "Панель клавиш игры внизу посередине: F1-F9 ряда (F12 переключает ряды), в боевом режиме — три ряда Q, A, Z; в "
     "ячейке клавиша, уровень навыка, количество предмета в сумке; клик — использовать. Навыки и предметы "
     "перетаскиваются на ячейки; значок, удержанный и перетащенный на другую ячейку, меняется с ней местами, а за "
     "пределы панели — убирается; ПКМ показывает описание; кнопка с мечами включает и выключает боевой режим. Навык, "
     "ждущий цель, — в золотой рамке; если SP не хватает — синяя плашка SP; запрещённое на карте — перечёркнутый "
     "круг, причина в подсказке."},
    {Text::HotbarBattle, "battle mode", "бойовий режим", "боевой режим"},
    {Text::HotbarBattleHint, "letters: skills and items \xC2\xB7 Enter: the chat",
     "літери — навички й предмети \xC2\xB7 Enter — чат", "буквы — навыки и предметы \xC2\xB7 Enter — чат"},
    // 3.0.11-beta.147: the battle mode button's tips (the pencil's place); a cell's ways - its card by a right click,
    // its icon dragged
    {Text::HotbarBattleOn, "Turn the battle mode on", "Увімкнути бойовий режим", "Включить боевой режим"},
    {Text::HotbarBattleOff, "Turn the battle mode off", "Вимкнути бойовий режим", "Выключить боевой режим"},
    {Text::HotbarUse, "click: use \xC2\xB7 right click: details \xC2\xB7 drag: move",
     "клік — використати \xC2\xB7 ПКМ — опис \xC2\xB7 тягніть — перемістити",
     "клик — использовать \xC2\xB7 ПКМ — описание \xC2\xB7 тяните — переместить"},
    // 3.0.11-beta.45: the hotbar's cell states
    {Text::HotbarNoSp, "No SP \xC2\xB7 needs {1}, you have {2}", "Немає SP \xC2\xB7 потрібно {1}, у вас {2}",
     "Нет SP \xC2\xB7 нужно {1}, у вас {2}"},
    {Text::HotbarNotHere, "Not here \xC2\xB7 {1}", "Не тут \xC2\xB7 {1}", "Не здесь \xC2\xB7 {1}"},
    {Text::HotbarNoTeleport, "no teleporting on this map", "на цій карті телепорт заборонено",
     "на этой карте телепорт запрещён"},
    {Text::HotbarNoReturn, "no way back to the save point from this map",
     "з цієї карти не повернутися на точку збереження", "с этой карты не вернуться на точку сохранения"},
    {Text::HotbarNoWarp, "no portals on this map", "на цій карті портали заборонено",
     "на этой карте порталы запрещены"},
    {Text::HotbarNoBranch, "no branches on this map", "на цій карті гілки заборонено", "на этой карте ветки запрещены"},
    {Text::HotbarNoIceWall, "no Ice Wall on this map", "на цій карті Ice Wall заборонено",
     "на этой карте Ice Wall запрещён"},
    {Text::HotbarNoCall, "nobody can be called to this map", "на цю карту не покликати",
     "на эту карту не позвать"},
    {Text::HotbarNoSkills, "no skills on this map", "на цій карті навички заборонено",
     "на этой карте навыки запрещены"},
    {Text::HotbarGuildWar, "not on the guilds' war maps", "не діє на картах війни гільдій",
     "не действует на картах войны гильдий"},
    {Text::HotbarOnlyGuildWar, "only on the guilds' war maps", "лише на картах війни гільдій",
     "только на картах войны гильдий"},
    // 3.0.11-beta.46: the new interface's motion
    {Text::InterfaceAnimations, "Interface motion", "Анімації інтерфейсу", "Анимации интерфейса"},
    {Text::InterfaceAnimationsHint,
     "The new interface moves a little: the target's card shows and goes smoothly, the character block changes its "
     "look smoothly, its edge flares gold at a new level, the low HP warning slides in from above. Off: everything at "
     "once.",
     "Новий інтерфейс трохи рухається: картка цілі з'являється й зникає плавно, блок персонажа плавно змінює вигляд, "
     "на новому рівні його край спалахує золотом, попередження про мало HP виїжджає згори. Вимкнено — усе одразу.",
     "Новый интерфейс немного двигается: карточка цели появляется и исчезает плавно, блок персонажа плавно меняет "
     "вид, на новом уровне его край вспыхивает золотом, предупреждение о малом HP выезжает сверху. Выключено — всё "
     "сразу."},
    // 3.0.11-beta.47: the new interface's grounds
    {Text::InterfacePanelOpacity, "Panels' opacity", "Непрозорість панелей", "Непрозрачность панелей"},
    {Text::InterfacePanelOpacityHint,
     "The ground of the character block, the minimap's bar, the buffs, the party, the target's card, the hotbar and "
     "the chat (the chat never under 80 %). Less: the world shows through them more; the words and icons stay whole.",
     "Підкладка блоку персонажа, смуги мінікарти, бафів, групи, картки цілі, панелі навичок і чату (чат — не менше "
     "80 %). Менше — світ видно крізь них більше; слова й іконки лишаються цілими.",
     "Подложка блока персонажа, полосы миникарты, баффов, группы, карточки цели, панели навыков и чата (чат — не "
     "меньше 80 %). Меньше — мир виден сквозь них больше; слова и иконки остаются целыми."},
    {Text::InterfaceWindowOpacity, "Windows' opacity", "Непрозорість вікон", "Непрозрачность окон"},
    {Text::InterfaceWindowOpacityHint,
     "The ground of the windows «Items», «Status» and «Reference»; at least 80 %, so that their tables and lists read "
     "well.",
     "Підкладка вікон «Речі», «Характеристики» і «Довідник»; не менше 80 %, щоб таблиці й списки читалися.",
     "Подложка окон «Вещи», «Характеристики» и «Справочник»; не меньше 80 %, чтобы таблицы и списки читались."},
    // 3.0.11-beta.48: the cast's label over the hero
    {Text::InterfaceCast, "Cast over the hero", "Каст над героєм", "Каст над героем"},
    {Text::InterfaceCastHint,
     "While the character casts: over the hero the skill's icon and name, the seconds left and a gold bar in the "
     "place of the game's old gauge (that one hidden).",
     "Поки персонаж читає навичку: над героєм її іконка й назва, скільки секунд лишилося, і золота смужка на місці "
     "старої смужки гри (та прихована).",
     "Пока персонаж читает навык: над героем его иконка и название, сколько секунд осталось, и золотая полоска на "
     "месте старой полоски игры (та скрыта)."},
    {Text::CastSeconds, "s", "\xD1\x81", "\xD1\x81"},
    // 3.0.11-beta.50: the screen's edges red at low HP
    {Text::InterfaceLowHpEdge, "Red edges at low HP", "Червоні краї при малому HP", "Красные края при малом HP"},
    {Text::InterfaceLowHpEdgeHint,
     "While HP is under a quarter, the screen's edges glow red; how strongly, from 0 (none, as by default) to 100 %.",
     "Поки HP менше чверті, краї екрана світяться червоним; наскільки сильно — від 0 (немає, як за замовчуванням) до "
     "100 %.",
     "Пока HP меньше четверти, края экрана светятся красным; насколько сильно — от 0 (нет, как по умолчанию) до "
     "100 %."},
    // 3.0.11-beta.54: the new interface's size
    {Text::InterfaceSize, "Interface size", "Розмір інтерфейсу", "Размер интерфейса"},
    {Text::InterfaceSizeHint,
     "How large the new interface's panels and windows are. Auto: full size from a game of 1280 x 720 up, smaller on a "
     "smaller one (896 x 672: 70 %), never so small its words get hard to read.",
     "Наскільки великі панелі й вікна нового інтерфейсу. Авто: повний розмір від гри 1280 x 720 і більше, на меншій — "
     "менший (896 x 672: 70 %), але ніколи не настільки, щоб слова стало важко читати.",
     "Насколько велики панели и окна нового интерфейса. Авто: полный размер от игры 1280 x 720 и больше, на меньшей — "
     "меньше (896 x 672: 70 %), но никогда не настолько, чтобы слова стало трудно читать."},
    {Text::InterfaceSizeAuto, "Auto", "Авто", "Авто"},
    {Text::InterfaceSize70, "70 %", "70 %", "70 %"},
    {Text::InterfaceSize85, "85 %", "85 %", "85 %"},
    {Text::InterfaceSize100, "100 %", "100 %", "100 %"},
    {Text::InterfaceSize115, "115 %", "115 %", "115 %"},
    {Text::InterfaceSize130, "130 %", "130 %", "130 %"},
    // 3.0.11-beta.67 (the concept: «Масштаб текста — отдельная настройка 80–150 %»)
    {Text::InterfaceTextSize, "Text size", "Розмір тексту", "Размер текста"},
    {Text::InterfaceTextSizeHint,
     "The windows you read in - the chat, «Reference», «Items», «Status» - larger or smaller together with their "
     "words, on top of the interface size; the panels and icons stay as the interface size has them.",
     "Вікна, які читають, — чат, «Довідник», «Речі», «Характеристики» — більші чи менші разом зі своїми словами, "
     "понад розмір інтерфейсу; панелі й значки лишаються такими, як за розміром інтерфейсу.",
     "Окна, которые читают, — чат, «Справочник», «Вещи», «Характеристики» — крупнее или мельче вместе со своими "
     "словами, сверх размера интерфейса; панели и значки остаются такими, как по размеру интерфейса."},
    {Text::InterfaceText80, "80 %", "80 %", "80 %"},
    {Text::InterfaceText90, "90 %", "90 %", "90 %"},
    {Text::InterfaceText150, "150 %", "150 %", "150 %"},
    {Text::InterfaceItemWindows, "Item windows size", "Розмір вікон речей", "Размер окон вещей"},  // 3.0.15-beta.9
    {Text::InterfaceItemWindowsHint,
     "«Items», the storage (the guild's too) and the cart smaller, so two of them side by side leave the world in "
     "view; the other windows stay as they are.",
     "«Речі», склад (і гільдії) та візок менші, щоб двоє поруч не закривали світ; решта вікон лишається як є.",
     "«Вещи», склад (и гильдии) и тележка меньше, чтобы два рядом не закрывали мир; остальные окна остаются как есть."},
    // 3.0.11-beta.41: the chat (stage 8 of the new interface)
    {Text::InterfaceChat, "Chat", "Чат", "Чат"},
    {Text::InterfaceChatHint,
     "The chat at the bottom left with its tabs: all, the party, the guild, whispers, trade (those near selling or "
     "buying) and the battle (hits, kills, experience, pick-ups); each channel its colour; idle, it dims to its last "
     "lines. Type as before; Tab or a click on the channel goes round near, the party, the guild, a whisper; a click "
     "on a name whispers to them. F10 its height, Alt+F10 away until you type.",
     "Чат унизу ліворуч із вкладками: усі, група, гільдія, шепіт, торг (хто поруч продає чи купує) і бій (удари, "
     "перемоги, досвід, підібране); кожен канал своїм кольором; без нових рядків тьмяніє до останніх. Пишіть як "
     "раніше; Tab або клік на каналі — по колу: усі, група, гільдія, шепіт; клік на імені — шепіт йому. F10 — "
     "висота, Alt+F10 — сховати, поки не почнете писати.",
     "Чат внизу слева со вкладками: все, группа, гильдия, шёпот, торг (кто рядом продаёт или покупает) и бой "
     "(удары, победы, опыт, подобранное); каждый канал своим цветом; без новых строк тускнеет до последних. Пишите "
     "как раньше; Tab или клик на канале — по кругу: все, группа, гильдия, шёпот; клик на имени — шёпот ему. F10 — "
     "высота, Alt+F10 — спрятать, пока не начнёте писать."},
    {Text::ChatTabAll, "All", "Усі", "Все"},
    {Text::ChatTabParty, "Party", "Група", "Группа"},
    {Text::ChatTabGuild, "Guild", "Гільдія", "Гильдия"},
    {Text::ChatTabWhisper, "Whisper", "Шепіт", "Шёпот"},
    {Text::ChatTabTrade, "Trade", "Торг", "Торг"},
    {Text::ChatTabBattle, "Battle", "Бій", "Бой"},
    {Text::ChatFrom, "from {1}", "від {1}", "от {1}"},
    {Text::ChatTo, "to {1}", "до {1}", "к {1}"},
    {Text::ChatHintSend, "Enter: send \xC2\xB7 Tab: the channel", "Enter — надіслати \xC2\xB7 Tab — канал",
     "Enter — отправить \xC2\xB7 Tab — канал"},
    {Text::ChatHintType, "Enter: type \xC2\xB7 Tab: the channel", "Enter — писати \xC2\xB7 Tab — канал",
     "Enter — писать \xC2\xB7 Tab — канал"},
    {Text::ChatKilled, "{1} defeated", "{1} переможено", "{1} побеждён"},
    {Text::ChatPicked, "Picked up: {1}", "Підібрано: {1}", "Подобрано: {1}"},
    {Text::ChatMiss, "miss", "промах", "промах"},
    // 3.0.11-beta.42: the windows (stage 8 of the new interface)
    {Text::InterfaceItems, "Items", "Речі", "Вещи"},
    {Text::InterfaceItemsHint,
     "The bag (Alt+E) in the new look: the game's three tabs, a search by the name, a grid of eight cells a row with "
     "the counts, the weight and the zeny. A double click uses or wears, a press drags (onto the hotbar, the "
     "storage, the ground), a right click shows the item's card. The title moves it.",
     "Сумка (Alt+E) у новому вигляді: три вкладки гри, пошук за назвою, сітка по вісім клітинок із кількістю, вага "
     "й зені. Подвійний клік — використати чи вдягнути, затиснути — перетягнути (на панель навичок, у склад, на "
     "землю), правий клік — картка предмета. За заголовок вікно рухається.",
     "Сумка (Alt+E) в новом виде: три вкладки игры, поиск по названию, сетка по восемь ячеек с количеством, вес и "
     "зени. Двойной клик — использовать или надеть, зажать — перетащить (на панель навыков, на склад, на землю), "
     "правый клик — карточка предмета. За заголовок окно двигается."},
    {Text::InterfaceStatus, "Status", "Характеристики", "Характеристики"},
    {Text::InterfaceStatusHint,
     "The stats (Alt+A) in the new look: base, bonus, total and the next point's price, a plus where the free points "
     "pay for it; the battle's numbers beside; under them what the stat pointed at gives by the game's formulas.",
     "Характеристики (Alt+A) у новому вигляді: база, бонус, разом і ціна наступного очка, плюс там, де вистачає "
     "вільних очок; поруч бойові числа; під ними — що дає стат під курсором за формулами гри.",
     "Характеристики (Alt+A) в новом виде: база, бонус, итог и цена следующего очка, плюс там, где хватает свободных "
     "очков; рядом боевые числа; под ними — что даёт стат под курсором по формулам игры."},
    {Text::ItemsTitle, "Items", "Речі", "Вещи"},
    {Text::ItemsTabItems, "Consumables", "Витратне", "Расходники"},
    {Text::ItemsTabGear, "Gear", "Спорядження", "Снаряжение"},
    {Text::ItemsTabEtc, "Misc", "Різне", "Разное"},
    {Text::ItemsFresh, "new", "нове", "новое"},  // 3.0.11-beta.112
    {Text::ItemsTabAll, "All", "Усе", "Всё"},           // 3.0.11-beta.113 (the concept's question 9)
    {Text::ItemsTabCards, "Cards", "Карти", "Карты"},
    {Text::ItemsSearch, "Search by the name", "Пошук за назвою", "Поиск по названию"},
    {Text::ItemsUse, "double click: use", "двічі клік — використати", "двойной клик — использовать"},
    {Text::ItemsWear, "double click: wear", "двічі клік — вдягнути", "двойной клик — надеть"},
    {Text::ItemsCard, "double click: into a slot", "двічі клік — у слот", "двойной клик — в слот"},
    {Text::ItemsInfo, "right click: its card", "ПКМ — картка", "ПКМ — карточка"},
    {Text::StatsTitle, "Status", "Характеристики", "Характеристики"},
    {Text::StatsFree, "Free points: {1}", "Вільні очки: {1}", "Свободные очки: {1}"},
    {Text::StatsColumnStat, "Stat", "Стат", "Стат"},
    {Text::StatsColumnBase, "Base", "База", "База"},
    {Text::StatsColumnBonus, "Bonus", "Бонус", "Бонус"},
    {Text::StatsColumnTotal, "Total", "Разом", "Итого"},
    {Text::StatsColumnCost, "Price", "Ціна", "Цена"},
    {Text::StatsBattle, "Battle", "Бій", "Бой"},
    {Text::StatsCast, "Cast time", "Час касту", "Время каста"},
    {Text::StatsMaxed, "max", "макс.", "макс."},
    {Text::StatsCost, "{1} {2} \xE2\x86\x92 {3} costs {4}, you have {5}",
     "{1} {2} \xE2\x86\x92 {3} коштує {4}, у вас {5}", "{1} {2} \xE2\x86\x92 {3} стоит {4}, у вас {5}"},
    {Text::StatsShort, "{1} short", "бракує {1}", "не хватает {1}"},
    {Text::StatsTop, "{1} {2} \xE2\x80\x94 the most", "{1} {2} \xE2\x80\x94 максимум",
     "{1} {2} \xE2\x80\x94 максимум"},
    {Text::StatsEffectStr, "ATK +{1}, carry +30", "ATK +{1}, вантаж +30", "ATK +{1}, груз +30"},
    {Text::StatsEffectAgi, "FLEE {1}, ASPD", "FLEE {1}, ASPD", "FLEE {1}, ASPD"},
    {Text::StatsEffectVit, "Max HP +1 %, DEF", "Max HP +1 %, DEF", "Max HP +1 %, DEF"},
    {Text::StatsEffectInt, "MATK +{1} ~ +{2}, Max SP +1 %", "MATK +{1} ~ +{2}, Max SP +1 %",
     "MATK +{1} ~ +{2}, Max SP +1 %"},
    {Text::StatsEffectDex, "HIT {1} \xC2\xB7 the cast shorter: {2} \xE2\x86\x92 {3} %",  // 3.0.11-beta.202
     "HIT {1} \xC2\xB7 каст коротший: {2} \xE2\x86\x92 {3} %", "HIT {1} \xC2\xB7 каст короче: {2} \xE2\x86\x92 {3} %"},
    {Text::StatsEffectLuk, "CRIT +0.3 %", "CRIT +0,3 %", "CRIT +0,3 %"},
    {Text::StatsHint, "Point at a stat: its price and what the next point gives",
     "Наведіть на стат — ціна й що дає наступне очко", "Наведите на стат — цена и что даёт следующее очко"},
    {Text::ItemsWorn, "worn", "вдягнуто", "надето"},  // 3.0.11-beta.44: the word rising over a worn item's cell
    // 3.0.11-beta.266 (the «Характеристики» concept, stage 2): the new rows and the panel «Як рахується»
    {Text::StatsRowDelay, "Delay after a skill",
     "Затримка після вміння",
     "Задержка после умения"},
    {Text::StatsRowRegen, "Recovery a tick",
     "Відновлення за тік",
     "Восстановление за тик"},
    {Text::StatsRowWeight, "Weight",
     "Вага",
     "Вес"},
    {Text::StatsPerSecond, "{1}/s",
     "{1}/с",
     "{1}/с"},
    {Text::StatsHow, "How it is counted · {1}",
     "Як рахується · {1}",
     "Как считается · {1}"},
    {Text::StatsHowNone, "Point at a row - here is how it is counted",
     "Наведіть на рядок — тут буде, як він рахується",
     "Наведите на строку — здесь будет, как она считается"},
    {Text::StatsHowSame, "the server's {1} - the same",
     "сервер: {1} — збігається",
     "сервер: {1} — совпадает"},
    {Text::StatsHowMore, "the server's {1}: the stats {2}, things, buffs and skills {3}",
     "сервер: {1} — статами {2}, речі, бафи й уміння {3}",
     "сервер: {1} — статами {2}, вещи, бафы и умения {3}"},
    {Text::StatsHowWeapon, "the weapon's ATK and its arrows: {1} · refining: + {2}",
     "ATK зброї й стріл: {1} · заточка: + {2}",
     "ATK оружия и стрел: {1} · заточка: + {2}"},
    {Text::StatsHowLevel, "level {1}",
     "рівень {1}",
     "уровень {1}"},
    {Text::StatsHowAspdWeapon, "{1} with {2}: delay {3}",
     "{1} із {2}: затримка {3}",
     "{1} с {2}: задержка {3}"},
    {Text::StatsHowAspdUnknown, "the weapon not found: the server's ASPD has skills' or things' speed in it",
     "зброю не визначено: в ASPD сервера є прискорення від умінь чи речей",
     "оружие не определено: в ASPD сервера есть ускорение от умений или вещей"},
    {Text::StatsHowHits, "{1} hits a second",
     "{1} удару на секунду",
     "{1} удара в секунду"},
    {Text::StatsHowAspdGoal, "ASPD {1} needs AGI {2}",
     "до ASPD {1} потрібно AGI {2}",
     "до ASPD {1} нужно AGI {2}"},
    {Text::StatsHowAspdNever, "ASPD {1} needs AGI {2}: past 99 - only skills or things",
     "до ASPD {1} потрібно AGI {2}: більше за 99 — лише вміння чи речі",
     "до ASPD {1} нужно AGI {2}: больше 99 — только умения или вещи"},
    {Text::StatsHowDef, "armour: a hit {1} % weaker · VIT takes {2}-{3} off each hit",
     "броня: удар на {1} % слабший · VIT знімає {2}–{3} з кожного удару",
     "броня: удар на {1} % слабее · VIT снимает {2}–{3} с каждого удара"},
    {Text::StatsHowMdef, "armour: magic {1} % weaker · INT + VIT/2 = {2} taken off",
     "броня: магія на {1} % слабша · INT + VIT/2 = {2} знімається",
     "броня: магия на {1} % слабее · INT + VIT/2 = {2} снимается"},
    {Text::StatsHowPerfect, "+ {1}: the perfect dodge 1 + LUK/10 = {2} %",
     "+ {1}: ідеальне ухилення 1 + LUK/10 = {2} %",
     "+ {1}: идеальное уклонение 1 + LUK/10 = {2} %"},
    {Text::StatsHowCast, "DEX {1} more - no cast at all",
     "ще {1} DEX — касту не буде зовсім",
     "ещё {1} DEX — каста не будет совсем"},
    {Text::StatsHowCastNone, "no cast at all",
     "касту немає зовсім",
     "каста нет совсем"},
    {Text::StatsHowDelay, "on uaRO DEX shortens the delay after a skill too",
     "на uaRO DEX скорочує й затримку після вміння",
     "на uaRO DEX сокращает и задержку после умения"},
    {Text::StatsHowRegen, "HP each 6 s (sitting 3 s), SP each 8 s (sitting 4 s); none from 70 % of the weight",
     "HP кожні 6 с (сидячи 3 с), SP кожні 8 с (сидячи 4 с); з 70 % ваги — ні",
     "HP каждые 6 с (сидя 3 с), SP каждые 8 с (сидя 4 с); с 70 % веса — нет"},
    {Text::StatsHowWeight, "70 %: {1} - HP and SP stop · 90 %: {2} - no attacks or skills",
     "70 %: {1} — HP і SP не відновлюються · 90 %: {2} — без атак і вмінь",
     "70 %: {1} — HP и SP не восстанавливаются · 90 %: {2} — без атак и умений"},
    {Text::StatsHowJob, "from the job levels: +{1}",
     "від рівнів професії: +{1}",
     "от уровней профессии: +{1}"},
    {Text::StatsHowNext, "the next point of {1}: {2}",
     "наступне очко {1}: {2}",
     "следующее очко {1}: {2}"},
    {Text::StatsUsesStr, "goes into: melee ATK STR + (STR/10)² · a bow's ATK STR/5 · weight + 30 a point",
     "впливає на: ATK ближнього бою STR + (STR/10)² · ATK лука STR/5 · вагу + 30 за очко",
     "влияет на: ATK ближнего боя STR + (STR/10)² · ATK лука STR/5 · вес + 30 за очко"},
    {Text::StatsUsesAgi, "goes into: FLEE level + AGI · ASPD 4 × AGI of each 1000",
     "впливає на: FLEE рівень + AGI · ASPD 4 × AGI з кожних 1000",
     "влияет на: FLEE уровень + AGI · ASPD 4 × AGI из каждых 1000"},
    {Text::StatsUsesVit, "goes into: Max HP × (1 + VIT/100) · DEF + VIT · MDEF + VIT/2 · HP a tick + VIT/5",
     "впливає на: Max HP × (1 + VIT/100) · DEF + VIT · MDEF + VIT/2 · HP за тік + VIT/5",
     "влияет на: Max HP × (1 + VIT/100) · DEF + VIT · MDEF + VIT/2 · HP за тик + VIT/5"},
    {Text::StatsUsesInt, "goes into: MATK · Max SP × (1 + INT/100) · MDEF + INT · SP a tick + INT/6",
     "впливає на: MATK · Max SP × (1 + INT/100) · MDEF + INT · SP за тік + INT/6",
     "влияет на: MATK · Max SP × (1 + INT/100) · MDEF + INT · SP за тик + INT/6"},
    {Text::StatsUsesDex,
     "goes into: HIT level + DEX · a bow's ATK DEX + (DEX/10)² · the cast and the delay × (150 − DEX)/150",
     "впливає на: HIT рівень + DEX · ATK лука DEX + (DEX/10)² · каст і затримку × (150 − DEX)/150",
     "влияет на: HIT уровень + DEX · ATK лука DEX + (DEX/10)² · каст и задержку × (150 − DEX)/150"},
    {Text::StatsUsesLuk, "goes into: CRIT 1 + LUK × 0.3 · the perfect dodge 1 + LUK/10 · ATK + LUK/5",
     "впливає на: CRIT 1 + LUK × 0,3 · ідеальне ухилення 1 + LUK/10 · ATK + LUK/5",
     "влияет на: CRIT 1 + LUK × 0,3 · идеальное уклонение 1 + LUK/10 · ATK + LUK/5"},
    // 3.0.11-beta.267 (stage 3): the tabs and «Довідка»
    {Text::StatsTabChar, "Character", "Персонаж", "Персонаж"},
    {Text::StatsTabRef, "Reference", "Довідка", "Справка"},
    {Text::StatsRefClass, "Class", "Клас", "Класс"},
    {Text::StatsRefAspd, "ASPD by weapon (DEX {1})", "ASPD за зброєю (DEX {1})", "ASPD по оружию (DEX {1})"},
    {Text::StatsRefWeapon, "Weapon", "Зброя", "Оружие"},
    {Text::StatsRefDelay, "Delay", "Затримка", "Задержка"},
    {Text::StatsRefYourAgi, "your AGI {1}", "ваш AGI {1}", "ваш AGI {1}"},
    {Text::StatsRefCost, "What a stat costs from 1", "Скільки коштує стат з 1", "Сколько стоит стат с 1"},
    {Text::StatsRefCostFrom, "up to", "до", "до"},
    {Text::StatsRefCostPoints, "points", "очок", "очков"},
    {Text::StatsRefBonus, "The job levels that give +1", "Рівні професії, що дають +1", "Уровни профессии, дающие +1"},
    {Text::StatsRefBonusNone, "none", "немає", "нет"},
    {Text::StatsRefBonusNow, "(now {1} of {2})", "(зараз {1} з {2})", "(сейчас {1} из {2})"},
    // 3.0.11-beta.268 (stage 4): «Планування»
    {Text::StatsTabPlan, "Planning",
     "Планування",
     "Планирование"},
    {Text::StatsPlanLevel, "Plan's level",
     "Рівень плану",
     "Уровень плана"},
    {Text::StatsPlanPoints, "Points: {1} now + {2} to level {3} = {4} · the plan takes {5}",
     "Очки: {1} зараз + {2} до рівня {3} = {4} · план бере {5}",
     "Очки: {1} сейчас + {2} до уровня {3} = {4} · план берёт {5}"},
    {Text::StatsPlanShort, "{1} short",
     "бракує {1}",
     "не хватает {1}"},
    {Text::StatsPlanResult, "NOW → PLANNED",
     "БУЛО → СТАНЕ",
     "БЫЛО → СТАНЕТ"},
    {Text::StatsPlanReset, "Reset",
     "Скинути",
     "Сбросить"},
    {Text::StatsPlanNote, "The plan only shows: you put the points yourself with + on «Character»",
     "План лише показує: очки вкладаєте ви самі кнопкою + на вкладці «Персонаж»",
     "План только показывает: очки вкладываете вы сами кнопкой + на вкладке «Персонаж»"},
    // 3.0.11-beta.269 (stage 5): the goals
    {Text::StatsGoals, "Goals",
     "Цілі",
     "Цели"},
    {Text::StatsGoalAspd, "ASPD {1}: AGI {2} - {3} points more",
     "ASPD {1}: AGI {2} — ще {3} очок",
     "ASPD {1}: AGI {2} — ещё {3} очков"},
    {Text::StatsGoalAspdNever, "ASPD {1}: AGI {2}, past 99 - only skills or things",
     "ASPD {1}: AGI {2}, більше за 99 — лише вміння чи речі",
     "ASPD {1}: AGI {2}, больше 99 — только умения или вещи"},
    {Text::StatsGoalAspdNone, "ASPD: the weapon not found (sped up by skills or things)",
     "ASPD: зброю не визначено (прискорено вміннями чи речами)",
     "ASPD: оружие не определено (ускорено умениями или вещами)"},
    {Text::StatsGoalCast, "No cast: DEX {1} - {2} points more",
     "Без касту: DEX {1} — ще {2} очок",
     "Без каста: DEX {1} — ещё {2} очков"},
    {Text::StatsGoalCastDone, "No cast: already",
     "Без касту: уже",
     "Без каста: уже"},
    {Text::StatsGoalCastNever, "No cast: DEX {1}, past 99 - {2} more from things or buffs",
     "Без касту: DEX {1}, більше за 99 — ще {2} від речей чи бафів",
     "Без каста: DEX {1}, больше 99 — ещё {2} от вещей или бафов"},
    // 3.0.11-beta.296: the map's monsters now and with the plan
    {Text::StatsMobs, "Monsters of {1}", "Монстри карти {1}", "Монстры карты {1}"},
    {Text::StatsMobsHit, "You hit", "Влучання", "Попадание"},
    {Text::StatsMobsDodge, "You dodge", "Ухилення", "Уклонение"},
    {Text::StatsMobsHits, "Blows to kill", "Ударів", "Ударов"},
    {Text::StatsMobsNone, "No monsters known on this map", "На цій карті монстрів не знайдено",
     "На этой карте монстров не найдено"},
    // 3.0.11-beta.297: the buffs the plan counts
    {Text::StatsPlanBuffs, "Buffs in the plan", "Бафи в плані", "Бафы в плане"},
    {Text::StatsPlanBuffNow, "now", "зараз", "сейчас"},
    // 3.0.11-beta.273: how long ago a character was played
    {Text::AgoNow, "just now", "щойно", "только что"},
    {Text::AgoMinutes, "{1} min ago", "{1} хв тому", "{1} мин назад"},
    {Text::AgoHours, "{1} h ago", "{1} год тому", "{1} ч назад"},
    {Text::AgoDays, "{1} d ago", "{1} дн. тому", "{1} дн. назад"},
    {Text::AgoMonths, "{1} mo ago", "{1} міс. тому", "{1} мес. назад"},
    // 3.0.11-beta.274: the last session
    {Text::SessionHead, "Last game {1}:", "Остання гра {1}:", "Последняя игра {1}:"},
    {Text::SessionHours, "{1} h {2} min", "{1} год {2} хв", "{1} ч {2} мин"},
    {Text::SessionMinutes, "{1} min", "{1} хв", "{1} мин"},
    {Text::SessionBase, "Base +{1} %", "Base +{1} %", "Base +{1} %"},
    {Text::SessionJob, "Job +{1} %", "Job +{1} %", "Job +{1} %"},
    {Text::SessionZeny, "{1} z", "{1} z", "{1} z"},
    {Text::MinimapRespawn, "{1} min", "{1} хв", "{1} мин"},
    {Text::MinimapMore, "and {1} more", "і ще {1}", "и ещё {1}"},
    {Text::MinimapNoReference, "No game reference yet", "Довідника гри ще немає", "Справочника игры ещё нет"},
    {Text::MinimapNoMonsters, "No monsters here", "Монстрів тут немає", "Монстров здесь нет"},
    // 3.0.11-beta.7: «Довідник» (win/reference_window, core/reference_pages)
    {Text::InterfaceReference,
     "Reference - Ctrl+F",
     "Довідник - Ctrl+F",
     "Справочник - Ctrl+F"},
    {Text::InterfaceReferenceHint,
     "Ctrl+F in the game opens the game's reference: type a part of a name - monsters, items, maps; a name in a "
     "card opens its own card. Esc closes it, Alt+← (the mouse's back button) goes back. From uaro.kiev.ua's "
     "database, fetched when the game starts.",
     "Ctrl+F у грі відкриває довідник гри: введіть частину назви - монстри, предмети, карти; назва в картці "
     "відкриває свою картку. Esc закриває, Alt+← (бокова кнопка миші) - назад. Дані - з бази uaro.kiev.ua, "
     "приходять під час запуску гри.",
     "Ctrl+F в игре открывает справочник игры: введите часть названия - монстры, предметы, карты; название в "
     "карточке открывает свою карточку. Esc закрывает, Alt+← (боковая кнопка мыши) - назад. Данные - из базы "
     "uaro.kiev.ua, приходят при запуске игры."},
    {Text::RefTitle, "Reference", "Довідник", "Справочник"},
    {Text::RefSearchHint,
     "Search: a monster, an item, a map, a quest, an NPC",
     "Пошук: монстр, предмет, карта, квест, NPC",
     "Поиск: монстр, предмет, карта, квест, NPC"},
    {Text::RefBack, "Back to {1}", "Назад до {1}", "Назад к {1}"},
    {Text::RefFooter,
     "Data pack · {1} · server {2}",
     "Пакет даних · {1} · сервер {2}",
     "Пакет данных · {1} · сервер {2}"},
    {Text::RefNoData,
     "No game reference here yet: it comes from uaro.kiev.ua when the game starts with the new minimap or the "
     "reference on.",
     "Довідника гри тут ще немає: він приходить з uaro.kiev.ua під час запуску гри з увімкненою новою мінікартою "
     "чи довідником.",
     "Справочника игры здесь ещё нет: он приходит с uaro.kiev.ua при запуске игры с включённой новой миникартой "
     "или справочником."},
    {Text::RefSecMobs, "Monsters", "Монстри", "Монстры"},
    {Text::RefSecMobsHint,
     "level, element, where they live, what they drop",
     "рівень, стихія, де водяться, що падає",
     "уровень, стихия, где водятся, что падает"},
    {Text::RefSecBosses, "MVP", "MVP", "MVP"},
    {Text::RefSecBossesHint,
     "where and when they appear",
     "де й коли з'являються",
     "где и когда появляются"},
    {Text::RefSecItems, "Items", "Предмети", "Предметы"},
    {Text::RefSecItemsHint,
     "where to buy, who drops it, what it is for",
     "де купити, хто дропає, для чого потрібні",
     "где купить, кто дропает, для чего нужны"},
    {Text::RefSecCards, "Cards", "Картки", "Карточки"},
    {Text::RefSecMaps, "Locations", "Локації", "Локации"},
    {Text::RefSecMapsHint,
     "monsters, exits, how to get there",
     "монстри, виходи, як дійти",
     "монстры, выходы, как дойти"},
    {Text::RefSecShops, "Shops", "Крамниці", "Магазины"},
    {Text::RefSecTeleports, "Kafra teleports", "Телепорти Kafra", "Телепорты Kafra"},
    {Text::RefSecPets, "Pets", "Пети", "Петы"},
    {Text::RefSecPetsHint,
     "whom to tame, what to feed, the bonus",
     "кого приручити, чим годувати, бонус",
     "кого приручить, чем кормить, бонус"},
    {Text::RefSecCraft, "Crafting", "Крафт", "Крафт"},
    {Text::RefSecCraftHint,
     "what to make, of what, who can",
     "що зробити, з чого, хто вміє",
     "что сделать, из чего, кто умеет"},
    {Text::RefFoundMobs, "Monsters · {1}", "Монстри · {1}", "Монстры · {1}"},
    {Text::RefFoundItems, "Items · {1}", "Предмети · {1}", "Предметы · {1}"},
    {Text::RefFoundMaps, "Locations · {1}", "Локації · {1}", "Локации · {1}"},
    {Text::RefShownOf,
     "the first {1} of {2} - type more of the name",
     "перші {1} з {2} - уточніть назву",
     "первые {1} из {2} - уточните название"},
    {Text::RefWhere, "Where to find", "Де знайти", "Где найти"},
    {Text::RefDrops, "What it drops", "Що випадає", "Что выпадает"},
    {Text::RefDroppedBy, "Dropped by", "З кого випадає", "С кого падает"},
    {Text::RefSoldAt, "Where to buy", "Де купити", "Где купить"},
    {Text::RefCraftOf, "What it is made of", "З чого зробити", "Из чего сделать"},
    {Text::RefUsedIn, "Goes into", "Входить у крафт", "Входит в крафт"},
    {Text::RefPet, "Pet", "Улюбленець", "Питомец"},
    {Text::RefWhoHere, "Who is here", "Хто тут", "Кто здесь"},
    {Text::RefExits, "Exits", "Виходи", "Выходы"},
    {Text::RefTeleports, "Teleports", "Телепорти", "Телепорты"},
    {Text::RefShopsHere, "Shops", "Магазини", "Магазины"},
    {Text::RefNpcs, "NPCs", "NPC", "NPC"},
    {Text::RefShopItems, "Goods", "Товари", "Товары"},
    {Text::RefLevel, "Lv {1}", "Lv {1}", "Lv {1}"},
    {Text::RefHp, "HP {1}", "HP {1}", "HP {1}"},
    {Text::RefExp, "exp {1} / {2}", "досвід {1} / {2}", "опыт {1} / {2}"},
    {Text::RefWeight, "weight {1}", "вага {1}", "вес {1}"},
    {Text::RefPrice, "price {1} z", "ціна {1} z", "цена {1} z"},
    {Text::RefSlots, "{1} slots", "слотів: {1}", "слотов: {1}"},
    {Text::RefYouHere, "you are here", "ви тут", "вы здесь"},
    {Text::RefNowhere,
     "Not on the server's maps (a summon, an event, a quest)",
     "Не трапляється на картах сервера (виклик, подія, квест)",
     "Не встречается на картах сервера (вызов, событие, квест)"},
    {Text::RefNoDrops, "Drops nothing", "Нічого не випадає", "Ничего не падает"},
    {Text::RefMvpReward, "MVP reward", "нагорода MVP", "награда MVP"},
    {Text::RefZeny, "{1} z", "{1} z", "{1} z"},
    {Text::RefPetLure, "taming item", "приманка", "приманка"},
    {Text::RefPetEgg, "egg", "яйце", "яйцо"},
    {Text::RefPetFood, "food", "корм", "корм"},
    {Text::RefPetEquip, "accessory", "аксесуар", "аксессуар"},
    {Text::RefBoss, "boss", "бос", "босс"},
    {Text::RefItemsCount, "item|items", "товар|товари|товарів", "товар|товара|товаров"},  // 3.0.11-beta.32: forms
    {Text::RefMobKinds, "kind of monsters|kinds of monsters", "вид монстрів|види монстрів|видів монстрів",
     "вид монстров|вида монстров|видов монстров"},
    {Text::RefType0, "Healing", "Лікування", "Лечение"},
    {Text::RefType2, "Usable", "Вживане", "Расходуемое"},
    {Text::RefType3, "Misc", "Інше", "Прочее"},
    {Text::RefType4, "Weapon", "Зброя", "Оружие"},
    {Text::RefType5, "Armour", "Броня", "Броня"},
    {Text::RefType6, "Card", "Картка", "Карточка"},
    {Text::RefType7, "Pet egg", "Яйце улюбленця", "Яйцо питомца"},
    {Text::RefType8, "Pet accessory", "Аксесуар улюбленця", "Аксессуар питомца"},
    {Text::RefType10, "Ammunition", "Набої", "Боеприпасы"},
    {Text::RefType11, "Usable (delayed)", "Вживане (із затримкою)", "Расходуемое (с задержкой)"},
    {Text::RefType18, "Cash shop", "Кеш-магазин", "Кеш-магазин"},
    {Text::RefTypeOther, "Other", "Інше", "Прочее"},
    // 3.0.11-beta.30 (the owner: «Огонь, Вітер ітд. это игровые параметры, которые можно не переводить»): the
    // elements, races and sizes by the game's own names in every language
    {Text::RefEle0, "Neutral", "Neutral", "Neutral"},
    {Text::RefEle1, "Water", "Water", "Water"},
    {Text::RefEle2, "Earth", "Earth", "Earth"},
    {Text::RefEle3, "Fire", "Fire", "Fire"},
    {Text::RefEle4, "Wind", "Wind", "Wind"},
    {Text::RefEle5, "Poison", "Poison", "Poison"},
    {Text::RefEle6, "Holy", "Holy", "Holy"},
    {Text::RefEle7, "Shadow", "Shadow", "Shadow"},
    {Text::RefEle8, "Ghost", "Ghost", "Ghost"},
    {Text::RefEle9, "Undead", "Undead", "Undead"},
    {Text::RefRace0, "Formless", "Formless", "Formless"},
    {Text::RefRace1, "Undead", "Undead", "Undead"},
    {Text::RefRace2, "Brute", "Brute", "Brute"},
    {Text::RefRace3, "Plant", "Plant", "Plant"},
    {Text::RefRace4, "Insect", "Insect", "Insect"},
    {Text::RefRace5, "Fish", "Fish", "Fish"},
    {Text::RefRace6, "Demon", "Demon", "Demon"},
    {Text::RefRace7, "Demi-Human", "Demi-Human", "Demi-Human"},
    {Text::RefRace8, "Angel", "Angel", "Angel"},
    {Text::RefRace9, "Dragon", "Dragon", "Dragon"},
    {Text::RefSize0, "Small", "Small", "Small"},
    {Text::RefSize1, "Medium", "Medium", "Medium"},
    {Text::RefSize2, "Large", "Large", "Large"},
    {Text::RefFlagTown, "town", "місто", "город"},
    {Text::RefFlagPvp, "PvP", "PvP", "PvP"},
    {Text::RefFlagGvg, "GvG", "GvG", "GvG"},
    {Text::RefFlagNoTeleport, "no teleport", "без телепорту", "без телепорта"},
    {Text::RefFlagNoMemo, "no memo", "без запам'ятовування", "без запоминания"},
    {Text::RefFlagNoSave, "no save point", "без точки збереження", "без точки сохранения"},
    {Text::RefHours, "{1} h", "{1} год", "{1} ч"},
    {Text::RefHoursMinutes, "{1} h {2} min", "{1} год {2} хв", "{1} ч {2} мин"},
    {Text::RefAll, "all {1} ›", "усі {1} ›", "все {1} ›"},
    // 3.0.11-beta.8: «Як дістатися» (core/reference_pages, reference_route)
    {Text::RefRoute, "How to get there", "Як дістатися", "Как добраться"},
    {Text::RefRouteFrom, "from {1}", "з {1}", "из {1}"},
    {Text::RefRouteWalk, "On foot", "Пішки", "Пешком"},
    {Text::RefRouteKafra, "By Kafra", "Через Kafra", "Через Kafra"},
    {Text::RefRouteNpc, "By an NPC", "Через NPC", "Через NPC"},
    {Text::RefRouteMoves, "moves: {1}", "переходів: {1}", "переходов: {1}"},
    {Text::RefRouteExit, "exit {1}", "вихід {1}", "выход {1}"},
    {Text::RefRouteStairs, "passage {1}", "перехід {1}", "переход {1}"},
    {Text::RefRouteVaries, "the price varies", "ціна різна", "цена разная"},
    {Text::RefRouteCondition, "a condition", "умова", "условие"},
    {Text::RefRouteConditionNote,
     "A move on the way has a condition: the NPC may want a quest or an item, or leads elsewhere.",
     "На шляху є перехід з умовою: NPC може вимагати квест чи предмет або вести деінде.",
     "На пути есть переход с условием: NPC может требовать квест или предмет либо вести в другое место."},
    {Text::RefRouteNone,
     "No way from here is known: the moves may be in scripts the reference does not read.",
     "Звідси шляху не знайдено: переходи можуть бути в скриптах, яких довідник не читає.",
     "Отсюда путь не найден: переходы могут быть в скриптах, которых справочник не читает."},
    // 3.0.11-beta.9: the way followed on the minimap (core/route_follow)
    {Text::RefRouteFollow, "on the minimap ›", "вести ›", "вести ›"},
    {Text::RefRouteFollowing, "followed · stop ›", "веду · зупинити ›", "веду · остановить ›"},
    {Text::MinimapRouteLeft, "{1} left", "ще {1}", "ещё {1}"},
    {Text::MinimapRouteThere, "You are there", "Ви на місці", "Вы на месте"},
    {Text::MinimapRouteNone, "No way from here", "Звідси шляху немає", "Отсюда пути нет"},
    {Text::MinimapGuildMate, "A guild mate", "Член гільдії", "Член гильдии"},  // 3.0.11-beta.201
    // 3.0.11-beta.10: the players now (core/reference_live, the site's «Онлайн»)
    {Text::RefPlayersNow, "players now: {1}", "гравців зараз: {1}", "игроков сейчас: {1}"},
    // 3.0.11-beta.11: the world map (core/world_map, the site's sheet and the client's minimaps)
    {Text::RefWorld, "World map", "Карта світу", "Карта мира"},
    {Text::RefWorldHint,
     "a mosaic of the maps, as on the site; the dungeons apart",
     "мозаїка карт, як на сайті; підземелля окремо",
     "мозаика карт, как на сайте; подземелья отдельно"},
    {Text::RefDungeons, "Dungeons", "Підземелля", "Подземелья"},
    {Text::RefWorldNone,
     "The world map comes from uaro.kiev.ua when the game starts - not here yet.",
     "Карта світу приходить з uaro.kiev.ua під час запуску гри - її ще немає.",
     "Карта мира приходит с uaro.kiev.ua при запуске игры - её ещё нет."},
    {Text::RefWorldHelp,
     "Point at a map - its name and players; a click opens its card. Drag to move it, the wheel zooms.",
     "Наведіть на локацію - її назва й гравці; клацання відкриває її картку. Тягніть мишею, колесо - зум.",
     "Наведите на локацию - её название и игроки; щелчок открывает её карточку. Тяните мышью, колесо - зум."},
    // 3.0.11-beta.12: the players' public MVP timers, the site's market (core/reference_live)
    {Text::RefTimers, "Players' timers", "Таймери гравців", "Таймеры игроков"},
    {Text::RefTimerSoon, "in {1}", "через {1}", "через {1}"},
    {Text::RefTimerDue, "should be up", "вже має бути", "уже должен быть"},
    {Text::RefTimerAgo, "killed {1} ago", "вбито {1} тому", "убит {1} назад"},
    {Text::RefTimerNoMap, "the map not told", "карту не вказано", "карта не указана"},
    {Text::RefMarket, "On the site's market", "На ринку сайту", "На рынке сайта"},
    {Text::RefMarketFrom, "from {1}", "від {1}", "от {1}"},
    {Text::RefMarketLots, "lots: {1} · pieces: {2}", "лотів: {1} · штук: {2}", "лотов: {1} · штук: {2}"},
    {Text::MinimapTimerDue, "up", "вже", "уже"},
    // 3.0.11-beta.13: the server's element table (db/attr_fix.txt)
    {Text::RefElements, "Damage by element", "Шкода стихіями", "Урон стихиями"},
    {Text::RefElementHeals, "heals {1} %", "лікує {1} %", "лечит {1} %"},
    // 3.0.11-beta.15: the NPCs found by their names, a map's nearest Kafra
    {Text::RefFoundNpcs, "NPCs · {1}", "NPC · {1}", "NPC · {1}"},
    {Text::RefKafraNear, "The nearest Kafra", "Kafra поруч", "Kafra рядом"},
    {Text::RefHere, "here", "тут", "здесь"},
    // 3.0.11-beta.16: the site's quests (core/reference_quests)
    {Text::RefSecQuests, "Quests", "Квести", "Квесты"},
    {Text::RefSecQuestsHint,
     "who gives it, what to bring, the steps",
     "хто дає, що принести, кроки",
     "кто даёт, что принести, шаги"},
    {Text::RefFoundQuests, "Quests · {1}", "Квести · {1}", "Квесты · {1}"},
    {Text::RefQuestNpc, "Who gives it", "Хто дає", "Кто даёт"},
    {Text::RefQuestItems, "What it takes", "Що потрібно", "Что нужно"},
    {Text::RefQuestSteps, "Steps", "Кроки", "Шаги"},
    {Text::RefQuestRewards, "Reward", "Нагорода", "Награда"},
    {Text::RefQuestFor, "For quests", "Для квестів", "Для квестов"},
    {Text::RefQuestLevel, "from Lv {1}", "від Lv {1}", "от Lv {1}"},
    {Text::RefQuestJobs, "Job changes", "Зміна професії", "Смена профессии"},
    {Text::RefQuestHeadgear, "Headgear", "Головні убори", "Головные уборы"},
    {Text::RefQuestAccess, "Access to towns", "Доступ до міст", "Доступ к городам"},
    {Text::RefQuestEvents, "Events", "Події", "События"},
    {Text::RefQuestOther, "Other", "Інше", "Другое"},
    {Text::RefQuestsNone,
     "The quests come from uaro.kiev.ua when the game starts - not here yet.",
     "Квести приходять з uaro.kiev.ua під час запуску гри - їх ще немає.",
     "Квесты приходят с uaro.kiev.ua при запуске игры - их ещё нет."},
    // 3.0.19-beta.4: the site's new quest kinds (its auto cards of every server quest)
    {Text::RefQuestSkills, "Quest skills", "Квестові навички", "Квестовые навыки"},
    {Text::RefQuestStory, "Story quests", "Сюжетні квести", "Сюжетные квесты"},
    // 3.0.19-beta.4: the character's step as the server keeps it (the site reads its quest variable)
    {Text::RefQuestYou, "Your progress", "Ваш прогрес", "Ваш прогресс"},
    {Text::RefQuestStepOf, "Step {1} of {2}", "Крок {1} з {2}", "Шаг {1} из {2}"},
    {Text::RefQuestDone, "Finished", "Завершено", "Завершён"},
    {Text::RefQuestNotStarted, "Not started yet", "Ще не розпочато", "Ещё не начат"},
    {Text::RefQuestNext, "Next: {1}", "Далі: {1}", "Дальше: {1}"},
    // 3.0.11-beta.18: the first screen's recent and starred cards
    {Text::RefRecent, "Opened lately", "Нещодавні", "Недавние"},
    {Text::RefFavourites, "Starred", "Обране", "Избранное"},
    // 3.0.11-beta.19: a monster's maps on the world map
    {Text::RefOnWorld, "On the world map ›", "Показати на карті світу ›", "Показать на карте мира ›"},
    {Text::RefWorldOf, "{1} on the world map", "{1} на карті світу", "{1} на карте мира"},
    {Text::RefAlsoInside, "Dungeons and rooms", "Підземелля й приміщення", "Подземелья и помещения"},
    // 3.0.11-beta.20: an item's droppers on a map; how fresh the site's live data is
    {Text::RefDroppersHere,
     "monsters dropping it: {1}",
     "монстрів, з яких випадає: {1}",
     "монстров, с которых падает: {1}"},
    // 3.0.11-beta.22: what an NPC pays for it (half its price); the item and the monster of a number
    {Text::RefSells, "sells for {1} z", "продаж {1} z", "продажа {1} z"},
    {Text::RefById, "Number {1}", "Номер {1}", "Номер {1}"},
    // 3.0.11-beta.26: the concept's look (core/reference_window: the header, the first screen, the bottom's chips)
    {Text::RefFind, "Search the reference", "Пошук у довіднику", "Поиск в справочнике"},
    {Text::RefSearchExplain,
     "Finds by a part of the name, the words in any order: «wing fly» finds Fly Wing; capital and small letters the "
     "same. The names are English, as in the game.",
     "Шукає за частиною назви, слова в будь-якому порядку: «wing fly» знайде Fly Wing; "
     "великі й малі літери однакові. Назви англійською, як у грі.",
     "Ищет по части названия, слова в любом порядке: «wing fly» найдёт Fly Wing; "
     "большие и малые буквы одинаковы. "
     "Названия английские, как в игре."},
    {Text::RefSecServices, "NPCs and services", "NPC і послуги", "NPC и услуги"},
    {Text::RefSecServicesHint,
     "shops, Kafra, prices",
     "крамниці, Kafra, ціни",
     "магазины, Kafra, цены"},
    // 3.0.18-beta.239 (the owner 13.11): «Таблиці» held the elements' table alone - named for it
    {Text::RefSecTables, "Elements", "Стихії", "Стихии"},
    {Text::RefSecTablesHint, "damage by element", "шкода за стихіями", "урон по стихиям"},
    {Text::RefWorldHintN,
     "a mosaic of {1} maps, as on the site; {2} dungeons apart",
     "мозаїка з {1} карт, як на сайті; {2} підземель окремо",
     "мозаика из {1} карт, как на сайте; {2} подземелий отдельно"},
    {Text::RefKeysList,
     "↑ ↓ pick · Enter open · Esc close",
     "↑ ↓ вибір · Enter відкрити · Esc закрити",
     "↑ ↓ выбор · Enter открыть · Esc закрыть"},
    {Text::RefKeysCard,
     "Alt+← back · Esc close",
     "Alt+← назад · Esc закрити",
     "Alt+← назад · Esc закрыть"},
    {Text::RefLiveNow,
     "Live · {1} in the game · {2} ago",
     "Живі дані · у грі {1} · {2} тому",
     "Живые данные · в игре {1} · {2} назад"},
    {Text::RefLiveOff,
     "Live data: no link",
     "Живі дані: немає зв'язку",
     "Живые данные: нет связи"},
    {Text::RefKindMob, "monster", "монстр", "монстр"},
    {Text::RefKindItem, "item", "предмет", "предмет"},
    {Text::RefKindMap, "location", "локація", "локация"},
    {Text::RefKindQuest, "quest", "квест", "квест"},
    {Text::RefFavEmpty,
     "☆ on a card - and it is here. Kept on your PC.",
     "☆ на картці — і запис тут. Зберігається на вашому ПК.",
     "☆ на карточке — и запись здесь. Хранится на вашем ПК."},
    {Text::RefTableElements,
     "Attack elements against a defence of level {1}",
     "Стихії атаки проти захисту рівня {1}",
     "Стихии атаки против защиты уровня {1}"},
    {Text::RefElementsHeal, "heal it: {1}", "лікують: {1}", "лечат: {1}"},
    {Text::RefRailHome, "Home", "Головна", "Главная"},
    {Text::RefRouteSteps, "Steps · {1}", "Кроки · {1}", "Шаги · {1}"},
    // 3.0.11-beta.28: the concept's search results (v21_search)
    {Text::RefFoundAegis,
     "By the service name · {1}",
     "За службовою назвою · {1}",
     "По служебному названию · {1}"},
    {Text::RefFoundMore, "{1} more ›", "ще {1} ›", "ещё {1} ›"},
    {Text::RefOftenFirst, "the commonest first", "частіші — вище", "частые — выше"},
    {Text::RefNearFirst, "the nearest first", "ближчі — вище", "ближние — выше"},
    {Text::RefOtherName, "other name: {1}", "друга назва: {1}", "другое название: {1}"},
    {Text::RefServiceName, "service name {1}", "службова назва {1}", "служебное название {1}"},
    {Text::RefShopsN, "shop|shops", "крамниця|крамниці|крамниць", "магазин|магазина|магазинов"},
    {Text::RefDropsFrom, "drops from {1}", "падає з {1}", "падает с {1}"},
    {Text::RefFromMobsN, "monster|monsters", "монстра|монстрів|монстрів", "монстра|монстров|монстров"},
    {Text::RefMapsN, "map|maps", "карта|карти|карт", "карта|карты|карт"},
    {Text::RefKindNpc, "NPC", "NPC", "NPC"},
    {Text::RefNothingFor, "Nothing found for «{1}»", "За «{1}» нічого не знайдено", "По «{1}» ничего не найдено"},
    {Text::RefNothingHint,
     "The names are English, as in the game. Try a part of a name: «wing», «poring».",
     "Назви в довіднику англійською, як у грі. Спробуйте частину назви: «wing», «poring».",
     "Названия в справочнике английские, как в игре. Попробуйте часть названия: «wing», «poring»."},
    // 3.0.11-beta.29: the concept's world map (v19_route)
    {Text::RefOnline, "{1} online", "{1} онлайн", "{1} онлайн"},
    {Text::RefRouteTo, "The way to {1}", "Шлях до {1}", "Путь до {1}"},
    {Text::RefRouteShort, "The way", "Шлях", "Путь"},
    {Text::RefRouteFromAt, "From: {1}", "Від: {1}", "От: {1}"},
    {Text::RefRouteWarn,
     "NPCs' moves with conditions, events and closed passages are not counted. Whether a move is open to you, the "
     "game does not know.",
     "Не враховано переходи через NPC з умовами, події й закриті проходи. "
     "Чи доступний перехід саме вам — гра не знає.",
     "Не учтены переходы через NPC с условиями, события и закрытые проходы. "
     "Доступен ли переход именно вам — игра не знает."},
    {Text::RefFollowOn, "✓ Lead on the minimap", "✓ Вести на міні-карті", "✓ Вести на мини-карте"},
    {Text::RefFollowOff, "Stop leading", "Не вести", "Не вести"},
    {Text::RefReset, "Reset", "Скинути", "Сбросить"},
    {Text::RefLegendRoute, "the way from where you are", "шлях від вашої позиції", "путь от вашей позиции"},
    {Text::RefLegendJump, "a teleport: Kafra or an NPC", "телепорт: Kafra чи NPC", "телепорт: Kafra или NPC"},
    {Text::RefLegendPlayers, "players on the map now", "гравців на карті зараз", "игроков на карте сейчас"},
    {Text::RefTopPlayers, "Most players now", "Найбільше гравців зараз", "Больше всего игроков сейчас"},
    {Text::RefToDungeon, "{1} → dungeon", "{1} → підземелля", "{1} → подземелье"},
    {Text::RefHereLabel, "You are here · {1}", "Ви тут · {1}", "Вы здесь · {1}"},
    {Text::RefKeysWorld,  // 3.0.11-beta.64: Ctrl+M opens and closes it
     "Click — a map's card · drag — move · wheel — zoom · Ctrl+M, Esc close",
     "Клік — картка · тягніть — зсув · колесо — зум · Ctrl+M, Esc закрити",
     "Клик — карточка · тяните — сдвиг · колесо — зум · Ctrl+M, Esc закрыть"},
    {Text::RefMoreN, "and {1} more", "і ще {1}", "и ещё {1}"},
    // 3.0.11-beta.32
    {Text::RefMobsOnMaps, "On the maps · {1}", "На картах · {1}", "На картах · {1}"},
    {Text::RefMobsNowhere,
     "Met on no map (events', summons') · {1}",
     "Не трапляються на картах (події, виклики) · {1}",
     "Не встречаются на картах (события, призывы) · {1}"},
    {Text::RefTableHelp,
     "The attack's element in a line, the target's in a column: per cents of the damage; below 0 it heals.",
     "Стихія атаки — рядок, стихія цілі — стовпчик: відсотки шкоди; менше 0 — лікує.",
     "Стихия атаки — строка, стихия цели — столбец: проценты урона; меньше 0 — лечит."},
    {Text::RefTowns, "Towns · {1}", "Міста · {1}", "Города · {1}"},
    {Text::RefMapsWithMobs, "With monsters · {1}", "З монстрами · {1}", "С монстрами · {1}"},
    {Text::RefCardsDropped, "Dropped by monsters · {1}", "Випадають з монстрів · {1}", "Выпадают с монстров · {1}"},
    {Text::RefCardsOther,
     "Not dropped by monsters (events, quests) · {1}",
     "Не випадають з монстрів (події, квести) · {1}",
     "Не выпадают с монстров (события, квесты) · {1}"},
    {Text::RefCardsEnchants, "Enchantments · {1}", "Зачарування · {1}", "Зачарования · {1}"},
    // 3.0.11-beta.64: the world map's dungeon box (the concept's v19_route), its counts («184 локації · 128 підземель»)
    {Text::RefDungeonOf, "{1} dungeon", "Підземелля {1}", "Подземелье {1}"},
    {Text::RefLocationsN, "location|locations", "локація|локації|локацій", "локация|локации|локаций"},
    {Text::RefDungeonsN, "dungeon|dungeons", "підземелля|підземелля|підземель", "подземелье|подземелья|подземелий"},
    // 3.0.11-beta.68 (the concept's «Ваша панель: Fire Bolt ×2…»): the skills on the hotbar - their elements'
    // multipliers on this monster, not a forecast of the damage
    {Text::RefYourBar, "Your bar (element multiplier): {1}", "Ваша панель (множник стихії): {1}",
     "Ваша панель (множитель стихии): {1}"},
    {Text::RefBarHeals, "heals it", "лікує", "лечит"},
    {Text::TargetYourBar, "Your bar: {1}", "Ваша панель: {1}", "Ваша панель: {1}"},
    // 3.0.11-beta.69 (the concept's question 21: the data's states, «Подробиці» by a click on the bottom's chips)
    {Text::RefFooterStale, "Data pack · {1} · may be out of date", "Пакет даних · {1} · може бути застарілим",
     "Пакет данных · {1} · может быть устаревшим"},
    {Text::RefLiveStale, "Live data · {1} ago · may be out of date", "Живі дані · {1} тому · можуть бути застарілими",
     "Живые данные · {1} назад · могут быть устаревшими"},
    {Text::RefDataTitle, "Reference data", "Дані довідника", "Данные справочника"},
    {Text::RefDataPack, "Data pack", "Пакет даних", "Пакет данных"},
    {Text::RefDataPackWhat,
     "Monsters, items, maps, NPCs, shops and quests - on your PC, they work without a link.",
     "Монстри, предмети, карти, NPC, магазини й квести — на вашому ПК, працюють без зв'язку.",
     "Монстры, предметы, карты, NPC, магазины и квесты — на вашем ПК, работают без связи."},
    {Text::RefDataPackFresh, "Made from the server {1}; the app brings a new one when the server changes.",
     "Зібрано із сервера {1}; утиліта приносить новий, коли сервер змінюється.",
     "Собрано с сервера {1}; утилита приносит новый, когда сервер меняется."},
    {Text::RefDataPackStale,
     "Older than 14 days - may be out of date (server {1}): the app brings a new one while the game runs.",
     "Старіший за 14 днів — може бути застарілим (сервер {1}): утиліта приносить новий, поки гра працює.",
     "Старше 14 дней — может быть устаревшим (сервер {1}): утилита приносит новый, пока игра работает."},
    {Text::RefDataLive, "Live data", "Живі дані", "Живые данные"},
    {Text::RefDataLiveWhat,
     "The site's market, the last MVP kills, the players on the maps - from uaro.kiev.ua while there is a link.",
     "Ринок сайту, останні вбивства MVP, гравці на картах — з uaro.kiev.ua, поки є зв'язок.",
     "Рынок сайта, последние убийства MVP, игроки на картах — с uaro.kiev.ua, пока есть связь."},
    {Text::RefDataLiveFresh, "updated {1} ago", "оновлено {1} тому", "обновлено {1} назад"},
    {Text::RefDataLiveStale, "{1} ago · may be out of date, not shown in the cards",
     "{1} тому · можуть бути застарілими, у картках не показано",
     "{1} назад · могут быть устаревшими, в карточках не показаны"},
    {Text::RefDataLiveNone, "none came yet - the rest of the cards works without them",
     "ще не приходили — решта карток працює без них", "ещё не приходили — остальные карточки работают без них"},
    // 3.0.11-beta.70: the tracker panel (the concept's «держать перед глазами»)
    {Text::RefTrack, "Track", "Відстежувати", "Отслеживать"},
    {Text::RefTracked, "✓ Tracked", "✓ Відстежується", "✓ Отслеживается"},
    {Text::RefTrackItem, "Track collecting", "Відстежувати збір", "Отслеживать сбор"},
    {Text::RefTrackedItem, "✓ Collecting tracked", "✓ Збір відстежується", "✓ Сбор отслеживается"},
    {Text::TrackerTitle, "Tracking", "Відстеження", "Отслеживание"},
    {Text::TrackerQuest, "quest", "квест", "квест"},
    {Text::TrackerRoute, "way", "шлях", "путь"},
    {Text::TrackerItem, "collect", "збір", "сбор"},
    {Text::TrackerGiveTo, "Give to: {1}", "Здати: {1}", "Сдать: {1}"},
    {Text::TrackerMore, "{1} more", "ще {1}", "ещё {1}"},
    {Text::InterfaceTracker, "Tracking", "Відстеження", "Отслеживание"},
    {Text::InterfaceTrackerHint,
     "What you pin stays in a panel left of the buffs: a quest («Track» in its card) - what to bring and whom to give "
     "it, the way the minimap leads, an item to collect - who drops it most. ▾ folds it to the names, ✕ hides it "
     "until you pin something again; a click on a name opens its card.",
     "Те, що ви закріпили, — у панелі ліворуч від бафів: квест («Відстежувати» в його картці) — що принести й кому "
     "здати, шлях, яким веде мінікарта, річ для збору — хто її найчастіше кидає. ▾ згортає до назв, ✕ ховає, доки "
     "не закріпите щось знову; клік по назві відкриває картку.",
     "То, что вы закрепили, — в панели слева от бафов: квест («Отслеживать» в его карточке) — что принести и кому "
     "сдать, путь, которым ведёт миникарта, вещь для сбора — кто её чаще всего роняет. ▾ сворачивает до названий, ✕ "
     "прячет, пока не закрепите что-то снова; клик по названию открывает карточку."},
    // 3.0.11-beta.71 (the owner: «хочется на карте прокладывать путь»): a way set on the minimap by clicks
    {Text::MinimapPlotTo, "Goal · {1}, {2}", "Ціль · {1}, {2}", "Цель · {1}, {2}"},
    {Text::MinimapPlotCells, "{1} cells", "{1} кл.", "{1} кл."},
    {Text::MinimapPlotHint, "Shift+click: one more point", "Shift+клік — ще точка", "Shift+клик — ещё точка"},
    // 3.0.11-beta.148 (R-4FBB8B; built 2.6.17, parked): getting onto a map ([connection])
    {Text::ConnectionHeading, "Getting onto a map", "Вхід на карту", "Вход на карту"},
    {Text::ConnPatientEntry, "Patient entry onto a map", "Терплячий вхід на карту", "Терпеливый вход на карту"},
    {Text::ConnPatientEntryHint,
     "The game waits for the map server up to a minute instead of 15 seconds, and connects again when its connection "
     "did not come up - instead of \"Failed to connect to Server\".",
     "Гра чекає на сервер карти до хвилини замість 15 секунд і підключається знову, якщо з'єднання не встановилося, "
     "— замість помилки гри Failed to connect to Server.",
     "Игра ждёт сервер карты до минуты вместо 15 секунд и подключается снова, если соединение не установилось, — "
     "вместо ошибки игры Failed to connect to Server."},
    {Text::ConnEntryPause, "Pause after a failed entry", "Пауза після невдалого входу", "Пауза после неудачного входа"},
    {Text::ConnEntryPauseHint,
     "If getting onto a map failed, the next character selection waits until the server has forgotten that attempt "
     "(two to five minutes, a countdown over the game; also after the game is closed and opened again) - straight "
     "away it would fail again. Opened again within half a minute of closing, the first selection waits out that "
     "half minute: the server is still saving the closed session.",
     "Якщо вхід на карту зірвався, наступний вибір персонажа чекає, доки сервер забуде ту спробу (від двох до п'яти "
     "хвилин, відлік над грою; і після закриття й нового запуску гри), — одразу він знову не вдався б. Якщо гру "
     "відкрили знову за пів хвилини після закриття, перший вибір чекає ці пів хвилини: сервер ще зберігає закриту "
     "сесію.",
     "Если вход на карту сорвался, следующий выбор персонажа ждёт, пока сервер забудет ту попытку (от двух до пяти "
     "минут, отсчёт над игрой; и после закрытия и нового запуска игры), — сразу он снова бы не удался. Если игру "
     "открыли снова в течение полуминуты после закрытия, первый выбор ждёт эти полминуты: сервер ещё сохраняет "
     "закрытую сессию."},
    {Text::ConnEntryWait, "Onto the map in {1}: the server has not let go of the last entry yet",
     "Вхід на карту через {1}: сервер ще не відпустив минулий вхід",
     "Вход на карту через {1}: сервер ещё не отпустил прошлый вход"},
    // 3.0.18-beta.164 (R-CFNPF6, the tester: «загрузка вечно»): the time left on the veil's card itself
    {Text::ConnEntryWaitCard, "The server still holds the last entry · {1}", "Сервер ще тримає минулий вхід · {1}",
     "Сервер ещё держит прошлый вход · {1}"},
    // 3.0.11-beta.184 (R-ANMTEJ): the camera's zoom and tilt kept between the games
    {Text::CameraTilt, "Camera tilt", "Нахил камери", "Наклон камеры"},  // 3.0.13-beta.5
    {Text::CameraTiltHint,
     "How low Shift with the right button (or the wheel) tilts the camera: as the game (-28°), lower (-13°) or down to "
     "the ground (-3°). In buildings it stays steeper.",
     "Як низько Shift із правою кнопкою (або колесом) нахиляє камеру: як у грі (-28°), нижче (-13°) або до самої "
     "землі (-3°). У будівлях лишається крутішою.",
     "Как низко Shift с правой кнопкой (или колесом) наклоняет камеру: как в игре (-28°), ниже (-13°) или до самой "
     "земли (-3°). В зданиях остаётся круче."},
    {Text::CameraTiltGame, "As the game", "Як у грі", "Как в игре"},
    {Text::CameraTiltLower, "Lower", "Нижче", "Ниже"},
    {Text::CameraTiltGround, "To the ground", "До землі", "До земли"},
    {Text::CameraRemember, "Remember the camera's zoom", "Пам'ятати віддалення камери",
     "Запоминать отдаление камеры"},
    {Text::CameraRememberHint,
     "The game starts every session with the camera close in. With this box the zoom and the tilt stay as you left "
     "them - outdoors and in buildings apart - after the next start of the game too.",
     "Гра щоразу починає з наближеною камерою. З цим прапорцем віддалення й нахил лишаються такими, як ви їх "
     "залишили, - окремо на вулиці й у будівлях, - і після нового запуску гри.",
     "Игра каждый раз начинает с приближенной камерой. С этим флажком отдаление и наклон остаются такими, как вы их "
     "оставили, - отдельно на улице и в зданиях, - и после нового запуска игры."},
    // 3.0.11-beta.186 (the tester: «тут дублюючий інтерфейс»): the line on a thing picked up
    {Text::InterfaceItemToast, "The line on a thing picked up", "Рядок про підібрану річ",
     "Строка о подобранной вещи"},
    {Text::InterfaceItemToastHint,
     "+1 and the thing's name for 2 seconds over the bag (at the screen's lower middle when it is closed), in place of "
     "the game's own box at the top of the screen. Off: the game's box as before. Works with «Items» on.",
     "+1 і назва речі на 2 секунди над сумкою (унизу посередині екрана, коли її закрито) замість власного віконця гри "
     "вгорі екрана. Вимкнено: віконце гри, як раніше. Працює з увімкненими «Речі».",
     "+1 и название вещи на 2 секунды над сумкой (внизу посередине экрана, когда она закрыта) вместо собственного "
     "окошка игры вверху экрана. Выключено: окошко игры, как раньше. Работает с включёнными «Вещи»."},
    // 3.0.11-beta.187 (the tester took the fan for the character's sight): the camera's fan on the minimap
    {Text::InterfaceMinimapView, "The camera's view on the minimap", "Огляд камери на міні-карті",
     "Обзор камеры на миникарте"},
    {Text::InterfaceMinimapViewHint,
     "A light fan round the arrow: what the camera sees. The arrow shows where the character faces. Off: the arrow "
     "alone.",
     "Світле віяло навколо стрілки: те, що бачить камера. Куди дивиться персонаж, показує стрілка. Вимкнено: лише "
     "стрілка.",
     "Светлый веер вокруг стрелки: то, что видит камера. Куда смотрит персонаж, показывает стрелка. Выключено: только "
     "стрелка."},
    // 3.0.11-beta.189 (the owner: «возможно еще и поиск»): the shelf's search
    {Text::ShelfSearch, "Search…", "Пошук…", "Поиск…"},
    {Text::EnterVeilTitle, "Entering the game…", "Вхід у гру…", "Вход в игру…"},  // 3.0.11-beta.203
    {Text::ShelfNoMatch, "Nothing found", "Нічого не знайдено", "Ничего не найдено"},
    // 3.0.11-beta.216 (the owner: «в интерфейсе нету управления наемниками»): the mercenary's block and tile
    {Text::MercTile, "Mercenary", "Найманець", "Наёмник"},
    {Text::MercLevelShort, "Lv. {1}", "Рів. {1}", "Ур. {1}"},
    {Text::MercLeftMinutes, "{1} min left", "ще {1} хв", "ещё {1} мин"},
    {Text::MercLeftHours, "{1} h {2} min left", "ще {1} год {2} хв", "ещё {1} ч {2} мин"},
    {Text::MercDismissAsk, "Dismiss the mercenary? Everything it has gained so far will be lost.",
     "Звільнити найманця? Усе, чого він набув досі, буде втрачено.",
     "Уволить наёмника? Всё, что он накопил до сих пор, будет потеряно."},
    // 3.0.11-beta.220 (the owner «1-3 делай»): the guild's tabs 2-5 on the card
    {Text::GuildEdit, "Change", "Змінити", "Изменить"},
    // 3.0.11-beta.271: the notice edited on the card
    {Text::GuildNoticeSubject, "Subject", "Тема",
     "Тема"},
    {Text::GuildNoticeBody, "Text", "Текст",
     "Текст"},
    {Text::GuildNoticeSave, "Save", "Зберегти",
     "Сохранить"},
    {Text::GuildNoticeHint, "Tab - the other field · Enter - save · Esc - cancel",
     "Tab — інше поле · Enter — зберегти · Esc — скасувати",
     "Tab — другое поле · Enter — сохранить · Esc — отменить"},
    {Text::GuildPositionsHead, "Positions: {1} - rights and tax", "Посад: {1} — права й податок",
     "Должностей: {1} — права и налог"},
    {Text::GuildRightInvite, "Invite", "Запрошення", "Приглашение"},
    {Text::GuildRightExpel, "Expel", "Вигнання", "Изгнание"},
    {Text::GuildSkillPoints, "Skill points left: {1}", "Вільних очок навичок: {1}", "Свободных очков навыков: {1}"},
    {Text::HomunSkillsHead, "Skills - points left: {1}", "Навички · вільних очок: {1}",
     "Навыки · свободных очков: {1}"},  // 3.0.11-beta.221
    // 3.0.11-beta.222 (the tester: «togle в налаштуваннях старий \ новий стиль»)
    {Text::InterfaceMinimapClassic, "Map marks as in the game", "Позначки на карті як у грі",
     "Отметки на карте как в игре"},
    {Text::InterfaceMinimapClassicHint,
     "The minimap's and the big map's marks as round dots in the game's own colours: guild mates pink, party members "
     "blue. Off: the new marks - a guild mate's pink triangle, a party member's face.",
     "Позначки на міні-карті й великій карті — круглими крапками в кольорах самої гри: члени гільдії рожеві, групи — "
     "блакитні. Вимкнено: нові позначки — рожевий трикутник члена гільдії, обличчя учасника групи.",
     "Отметки на мини-карте и большой карте — круглыми точками в цветах самой игры: члены гильдии розовые, группы — "
     "голубые. Выключено: новые отметки — розовый треугольник члена гильдии, лицо участника группы."},
    // 3.0.11-beta.223 (the skills concept, stage 1): the skills the server has not sent, and why
    {Text::SkillsQuest, "Quest - not learned with points", "Квест — очками не вчиться", "Квест — очками не изучается"},
    {Text::SkillsWedding, "Married characters only", "Лише для подружжя", "Только для супругов"},
    {Text::SkillsSoulLink, "Only under a Soul Link", "Лише під Soul Link", "Только под Soul Link"},
    {Text::SkillsEarlier, "Spend the {1} points first", "Спершу витратьте очки {1}", "Сначала потратьте очки {1}"},
    {Text::SkillsEarlierNotice, "{1} skills open once the free points go into {2} skills",
     "Навички {1} відкриються, коли витратите вільні очки на навички {2}",
     "Навыки {1} откроются, когда потратите свободные очки на навыки {2}"},
    {Text::SkillsBasicNotice, "A job change needs Basic Skill 9", "Для зміни професії потрібен Basic Skill 9",
     "Для смены профессии нужен Basic Skill 9"},
    // 3.0.11-beta.224 (the skills concept, stage 2): the tree view
    {Text::SkillsViewList, "List", "Список", "Список"},
    {Text::SkillsViewTree, "Tree", "Дерево", "Дерево"},
    {Text::SkillsPath, "To {1}: {2} more points", "До {1} — ще очок: {2}", "До {1} — ещё очков: {2}"},
    // 3.0.11-beta.225 (the skills concept, stage 3): the plan
    {Text::SkillsPlan, "Plan", "План", "План"},
    {Text::SkillsPlanApply, "Apply", "Застосувати", "Применить"},
    {Text::SkillsPlanApplying, "Applying…", "Застосовується…", "Применяется…"},
    {Text::SkillsPlanReset, "Reset", "Скинути", "Сбросить"},
    {Text::SkillsPlanned, "planned: {1}", "у плані: {1}", "в плане: {1}"},
    {Text::SkillsPlanStopped, "The plan stopped at {1}: the server does not let raise it yet",
     "План зупинився на {1}: сервер поки не дає її підняти",
     "План остановился на {1}: сервер пока не даёт его поднять"},
    // 3.0.11-beta.226 (the skills concept, stage 4): the next job's tab
    {Text::SkillsNextTab, "Next", "Наступна", "Следующая"},
    {Text::SkillsAfterChange, "After the job change", "Після зміни професії", "После смены профессии"},
    // 3.0.11-beta.229: a skill the game's table has no description of - what the driver knows
    {Text::CardNoDescription, "The game has no description of it", "Опису в грі немає", "Описания в игре нет"},
    {Text::CardMostLevel, "Levels: {1}", "Рівнів: {1}", "Уровней: {1}"},
    // 3.0.11-beta.229 (R-EQDQWH): the tree's side panel
    {Text::SkillsLevelOf, "Level {1} of {2}", "Рівень {1} з {2}", "Уровень {1} из {2}"},
    {Text::SkillsNeedsHead, "Needs", "Потрібно", "Нужно"},
    {Text::SkillsUnlocks, "Opens", "Відкриває", "Открывает"},
    {Text::SkillsSideHint, "Point at a skill", "Наведіть на навичку", "Наведите на навык"},
    {Text::SkillsSpByLevel, "SP by level", "SP за рівнями", "SP по уровням"},  // 3.0.11-beta.230
    // 3.0.11-beta.235 (the concept «Блок персонажа»): the full block's new lines and its tips
    {Text::CharGuild, "Guild {1}", "Гільдія {1}", "Гильдия {1}"},
    {Text::CharParty, "Party {1}", "Група {1}", "Группа {1}"},
    {Text::CharRegen, "+{1} every {2} s", "+{1} кожні {2} с", "+{1} каждые {2} с"},
    {Text::CharRegenFull, "full", "повне", "полное"},
    {Text::CharRegenToFull, "full in ≈ {1}", "до повного ≈ {1}", "до полного ≈ {1}"},
    {Text::CharRegenNone, "no regen: overweight", "не відновлюється: вага", "не восстанавливается: вес"},
    {Text::CharSessionHead, "Session · {1}", "Сесія · {1}", "Сессия · {1}"},
    {Text::CharKilled, "killed", "вбито", "убито"},
    {Text::CharDeaths, "deaths", "смертей", "смертей"},
    {Text::CharTipWeight, "70 %: HP and SP stop recovering · 90 %: no attacks or skills",
     "70 %: HP і SP не відновлюються · 90 %: без атак і вмінь",
     "70 %: HP и SP не восстанавливаются · 90 %: без атак и умений"},
    {Text::CharTipPoints, "Free stat points: Alt+A", "Вільні очки характеристик: Alt+A",
     "Свободные очки характеристик: Alt+A"},
    {Text::CharTipSkillPoints, "Free skill points: Alt+S", "Вільні очки навичок: Alt+S",
     "Свободные очки навыков: Alt+S"},
    {Text::CharTipRegen1, "Standing; sitting twice as fast, walking half as fast",
     "Стоячи; сидячи — удвічі швидше, на ходу — удвічі повільніше",
     "Стоя; сидя — вдвое быстрее, на ходу — вдвое медленнее"},
    {Text::CharTipRegen2, "The server's formulas with the skills; items and buffs not counted",
     "За формулами сервера з навичками; предмети й бафи не враховано",
     "По формулам сервера с навыками; предметы и бафы не учтены"},
    {Text::CharTipAtk, "{1} from STR, DEX and LUK; {2} from the weapon and its refine",
     "{1} — від STR, DEX і LUK; {2} — від зброї та заточки", "{1} — от STR, DEX и LUK; {2} — от оружия и заточки"},
    {Text::CharTipDef, "{1} from the armour (a share of the damage); {2} from VIT",
     "{1} — з броні (частка шкоди); {2} — від VIT", "{1} — от брони (доля урона); {2} — от VIT"},
    {Text::CharTipFlee, "{1} dodge; {2} perfect dodge (LUK)", "{1} — ухилення; {2} — ідеальне ухилення (LUK)",
     "{1} — уклонение; {2} — идеальное уклонение (LUK)"},
    {Text::CharTipAspd, "{1} hits a second; the server's limit 190", "{1} удару за секунду; межа сервера 190",
     "{1} удара в секунду; предел сервера 190"},
    {Text::CharTipOpen, "A click opens «Status»", "Клік відкриває «Характеристики»", "Клик открывает «Характеристики»"},
    {Text::CharTipTime, "In the game for {1}", "У грі {1}", "В игре {1}"},
    {Text::CharTipZeny, "Zeny this session: {1}", "Зені за сесію: {1}", "Зени за сессию: {1}"},
    {Text::CharTipKills, "Monsters you hit that died this session", "Монстри, яких ти бив і які загинули за сесію",
     "Монстры, которых ты бил и которые погибли за сессию"},
    {Text::CharTipDeaths, "Your deaths this session", "Твої смерті за сесію", "Твои смерти за сессию"},
    {Text::CharTipOnline, "{1} of {2} in the game", "{1} з {2} у грі", "{1} из {2} в игре"},
    // 3.0.11-beta.236 (the concept «Блок персонажа»): the experience's bars at the screen's bottom
    {Text::InterfaceExpBars, "Experience bars at the bottom", "Смужки досвіду внизу екрана",
     "Полоски опыта внизу экрана"},
    {Text::InterfaceExpBarsHint,
     "Two thin bars along the screen's bottom edge: the base's experience over the job's. Point at them for the per "
     "cent, the numbers, the pace and the time to the level. Gone at the last levels.",
     "Дві тонкі смужки вздовж нижнього краю екрана: досвід Base над досвідом Job. Наведіть — відсоток, числа, темп і "
     "час до рівня. На максимальних рівнях зникають.",
     "Две тонкие полоски вдоль нижнего края экрана: опыт Base над опытом Job. Наведите — процент, числа, темп и время "
     "до уровня. На максимальных уровнях исчезают."},
    // 3.0.11-beta.237 (the owner: «над мобом было типа + столько то опыта»)
    {Text::InterfaceExpGain, "Experience over a killed monster", "Досвід над убитим монстром",
     "Опыт над убитым монстром"},
    {Text::InterfaceExpGainHint,
     "A monster you hit dies: the experience it gave rises over it for a moment, and shows by the experience bars at "
     "the bottom for three seconds with its part of the level. Experience from the party or a quest - by the bars "
     "only.",
     "Монстр, якого ти бив, помирає: досвід за нього на мить спливає над ним і три секунди видно біля смужок досвіду "
     "внизу, з часткою рівня. Досвід від групи чи квесту — лише біля смужок.",
     "Монстр, которого ты бил, умирает: опыт за него на миг всплывает над ним и три секунды виден у полосок опыта "
     "внизу, с долей уровня. Опыт от группы или квеста — только у полосок."},
    // 3.0.11-beta.238 (the owner: «когда сидишь, реген совсем другой»; «желтеньким … убийство последнего моба»)
    {Text::CharRegenPerMin, "+{1}/min · {2}", "+{1}/хв · {2}", "+{1}/мин · {2}"},
    {Text::CharStanding, "standing", "стоячи", "стоя"},
    {Text::CharSitting, "sitting", "сидячи", "сидя"},
    {Text::CharRegenNatural, "natural +{1} every {2} s", "природне +{1} кожні {2} с", "естественное +{1} каждые {2} с"},
    {Text::CharRegenSkill, "From a skill: +{1} every 10 s", "Від навички: +{1} кожні 10 с",
     "От навыка: +{1} каждые 10 с"},
    {Text::CharRegenSitSkill, "Sitting, from a skill: +{1} every 10 s", "Сидячи, від навички: +{1} кожні 10 с",
     "Сидя, от навыка: +{1} каждые 10 с"},
    {Text::CharFightCost, "last fight −{1}", "останній бій −{1}", "последний бой −{1}"},
    // 3.0.11-beta.240 (the chat concept, stage 1): the input's commands, the fade
    {Text::ChatCmdParty, "/p words: to the party", "/p текст — у групу", "/p текст — в группу"},
    {Text::ChatCmdGuild, "/g words: to the guild", "/g текст — у гільдію", "/g текст — в гильдию"},
    {Text::ChatCmdWhisper, "/w Name words: a whisper", "/w Ім'я текст — шепіт", "/w Имя текст — шёпот"},
    {Text::ChatCmdReply, "/r words: answer {1}", "/r текст — відповісти {1}", "/r текст — ответить {1}"},
    {Text::InterfaceChatFade, "The chat fades when idle", "Чат згасає без нових рядків",
     "Чат гаснет без новых строк"},
    {Text::InterfaceChatFadeHint,
     "With no new lines for this long the chat's ground grows faint; its tabs, its lines and the input stay where "
     "they are.",
     "Коли нових рядків немає стільки часу, фон чату блідне; вкладки, рядки й поле введення лишаються на місці.",
     "Когда новых строк нет столько времени, фон чата бледнеет; вкладки, строки и поле ввода остаются на месте."},
    {Text::InterfaceChatFade12, "After 12 s", "Через 12 с", "Через 12 с"},
    {Text::InterfaceChatFade30, "After 30 s", "Через 30 с", "Через 30 с"},
    {Text::InterfaceChatFadeNever, "Never", "Ніколи", "Никогда"},
    // 3.0.11-beta.242 (the chat concept, stage 3): a name's menu
    {Text::ChatMenuWhisper, "Whisper", "Шепнути", "Шепнуть"},
    {Text::ChatMenuBlock, "Block whispers", "Блокувати шепіт", "Блокировать шёпот"},
    {Text::ChatMenuCopy, "Copy the name", "Копіювати ім'я", "Копировать имя"},
    // 3.0.11-beta.243 (the chat concept, stage 4): «Усі»'s filters, the server's own lines made short
    {Text::ChatFilterSystem, "The server's lines", "Системні рядки", "Системные строки"},
    {Text::ChatFilterLoot, "Picked up and experience", "Підібране й досвід", "Подобранное и опыт"},
    {Text::ChatFilterAnnounce, "Announcements", "Оголошення", "Объявления"},
    {Text::ChatFilterTrade, "Trade", "Торгівля", "Торговля"},
    {Text::ChatSysAutoloot, "Autoloot: up to {1}", "Автолут: до {1}", "Автолут: до {1}"},
    {Text::ChatSysAutolootOff, "Autoloot off", "Автолут вимкнено", "Автолут выключен"},
    {Text::ChatSysExpOn, "Experience shown", "Показ досвіду ввімкнено", "Показ опыта включён"},
    {Text::ChatSysExpOff, "Experience not shown", "Показ досвіду вимкнено", "Показ опыта выключен"},
    {Text::ChatSysZenyOn, "Zeny shown", "Показ зені ввімкнено", "Показ зени включён"},
    {Text::ChatSysZenyOff, "Zeny not shown", "Показ зені вимкнено", "Показ зени выключен"},
    {Text::ChatSysExpGained, "Experience: Base +{1} · Job +{2}", "Досвід: Base +{1} · Job +{2}",
     "Опыт: Base +{1} · Job +{2}"},
    {Text::ChatSysNoSp, "Not enough SP", "Недостатньо SP", "Недостаточно SP"},  // 3.0.12-beta.3
    {Text::ChatMenuCopyLine, "Copy the line", "Копіювати рядок", "Копировать строку"},  // 3.0.11-beta.244
    {Text::InterfaceUauiShowcase, "Components showcase (for developers)", "Вітрина компонентів (для розробників)",
     "Витрина компонентов (для разработчиков)"},
    {Text::InterfaceUauiShowcaseHint,
     "A window with the new library's components: tabs, the bag's items, a list, buttons, switches. Works with the "
     "new interface library on.",
     "Вікно з компонентами нової бібліотеки: вкладки, предмети з сумки, список, кнопки, перемикачі. Працює з "
     "увімкненою новою бібліотекою інтерфейсу.",
     "Окно с компонентами новой библиотеки: вкладки, предметы из сумки, список, кнопки, переключатели. Работает с "
     "включённой новой библиотекой интерфейса."},
    {Text::InterfaceUauiMotion, "Smooth motion (new library)", "Плавний рух (нова бібліотека)",
     "Плавное движение (новая библиотека)"},
    {Text::InterfaceUauiMotionHint,
     "Parts drawn by the new library move softly: windows slide in, cells and buttons light up and press, "
     "HP and SP bars glide, the low HP plate pulses, new items and experience float in. Works with the "
     "new interface library on.",
     "Частини, які малює нова бібліотека, рухаються м'яко: вікна виїжджають, клітинки й кнопки плавно "
     "підсвічуються і натискаються, смуги HP і SP ковзають, табличка низького HP пульсує, нові речі й "
     "досвід випливають. Працює з увімкненою новою бібліотекою інтерфейсу.",
     "Части, которые рисует новая библиотека, двигаются мягко: окна выезжают, клетки и кнопки плавно "
     "подсвечиваются и нажимаются, полоски HP и SP скользят, табличка низкого HP пульсирует, новые вещи и "
     "опыт выплывают. Работает с включённой новой библиотекой интерфейса."},
    {Text::InterfaceUauiEffects, "Effects (new library)", "Ефекти (нова бібліотека)",
     "Эффекты (новая библиотека)"},
    {Text::InterfaceUauiEffectsHint,
     "Parts drawn by the new library get soft shadows under windows and panels, a glow on the chosen and "
     "ready cells, a gentle sheen on bars and buttons, round corners cut cleanly. Works with the new "
     "interface library on.",
     "Частини, які малює нова бібліотека, отримують м'які тіні під вікнами й панелями, світіння на "
     "вибраних і готових клітинках, легкий відблиск на смугах і кнопках, чисто обрізані круглі кути. "
     "Працює з увімкненою новою бібліотекою інтерфейсу.",
     "Части, которые рисует новая библиотека, получают мягкие тени под окнами и панелями, свечение на "
     "выбранных и готовых клетках, лёгкий блик на полосках и кнопках, чисто обрезанные круглые углы. "
     "Работает с включённой новой библиотекой интерфейса."},
    {Text::InterfaceUauiGlass, "Frosted glass windows (new library)", "Скляні вікна (нова бібліотека)",
     "Стеклянные окна (новая библиотека)"},
    {Text::InterfaceUauiGlassHint,
     "Windows of the new library show the world behind them softly blurred, like frosted glass. Direct3D "
     "12 only; costs some video card time. Works with the new interface library on.",
     "Вікна нової бібліотеки показують світ за собою м'яко розмитим, як матове скло. Лише Direct3D 12; "
     "трохи навантажує відеокарту. Працює з увімкненою новою бібліотекою інтерфейсу.",
     "Окна новой библиотеки показывают мир за собой мягко размытым, как матовое стекло. Только Direct3D "
     "12; немного нагружает видеокарту. Работает с включённой новой библиотекой интерфейса."},
    // 3.0.18-beta.13: uaUI's themes
    {Text::InterfaceTheme, "Interface theme", "Тема інтерфейсу", "Тема интерфейса"},
    {Text::InterfaceThemeHint,
     "The colours and corners of the new interface drawn by the new library: today's, the old game's warm browns, or "
     "a sharper contrast. Your own colours: uaro-graphics\\ui\\palette.txt.",
     "Кольори й кути нового інтерфейсу, який малює нова бібліотека: як зараз, теплі коричневі старої гри або чіткіший "
     "контраст. Свої кольори: uaro-graphics\\ui\\palette.txt.",
     "Цвета и углы нового интерфейса, который рисует новая библиотека: как сейчас, тёплые коричневые старой игры или "
     "чёткий контраст. Свои цвета: uaro-graphics\\ui\\palette.txt."},
    {Text::InterfaceThemeStandard, "Standard", "Стандартна", "Стандартная"},
    {Text::InterfaceThemeClassic, "Classic RO", "Класична RO", "Классическая RO"},
    {Text::InterfaceThemeContrast, "Contrast", "Контрастна", "Контрастная"},
    // 3.0.18-beta.114 (the owner: «нету кнопки для анманта пеко итд»): the game's own window's one button (its 0xB6)
    {Text::EquipCartOff, "Remove the cart", "Зняти візок", "Снять тележку"},
    {Text::EquipFalconOff, "Release the falcon", "Відпустити сокола", "Отпустить сокола"},
    {Text::EquipPecoOff, "Get off the Peco", "Злізти з Пеко", "Слезть с Пеко"},
    // 3.0.18-beta.120 (the concept «Ігровий інтерфейс»): «Що мене вбило» on its own; the
    // character block's second row
    {Text::RecapNone, "No death yet since the game started", "Від запуску гри смертей ще не було",
     "С запуска игры смертей ещё не было"},
    {Text::CharOwnReference, "Reference", "Довідник", "Справочник"},
    {Text::CharOwnHunt, "Hunt", "Полювання", "Охота"},
    {Text::CharOwnMvp, "Boss timers", "Таймери босів",
     "Таймеры боссов"},
    {Text::CharOwnTracker, "Tracking", "Відстеження",
     "Отслеживание"},
    {Text::CharOwnParty, "Party", "Група", "Группа"},
    {Text::CharOwnRecap, "What killed me", "Що мене вбило",
     "Что меня убило"},
    {Text::CharOwnSettings, "Settings", "Налаштування",
     "Настройки"},
    // 3.0.18-beta.121 (the concept «Ігровий інтерфейс»): the menu's own over the game's
    {Text::MenuSettings, "Settings", "Налаштування", "Настройки"},
    {Text::MenuLayout, "Arrange the interface", "Розмістити інтерфейс", "Разместить интерфейс"},
    {Text::MenuTest, "Graphics test", "Тест графіки", "Тест графики"},
    {Text::MenuReport, "Send a report", "Надіслати звіт", "Отправить отчёт"},
    // 3.0.18-beta.122 (the concept «Ігровий інтерфейс»): «Налаштування» in place of the
    // game's Alt+O
    {Text::SettingsWinHint, "Everything changes at once", "Усе змінюється одразу", "Всё меняется сразу"},
    {Text::SettingsWinNone, "Nothing found", "Нічого не знайдено", "Ничего не найдено"},
    {Text::SettingsWinConsole, "All graphics settings\xE2\x80\xA6 (Alt+D)",
     "Усі налаштування графіки\xE2\x80\xA6 (Alt+D)", "Все настройки графики\xE2\x80\xA6 (Alt+D)"},
    // 3.0.18-beta.125 (the concept «Ігровий інтерфейс»: «Клавіші»): our windows' keys
    {Text::InterfaceSecKeys, "Keys", "Клавіші", "Клавиши"},
    {Text::InterfaceKeyHunt, "Key: Hunt", "Клавіша: Полювання", "Клавиша: Охота"},
    {Text::InterfaceKeyMvp, "Key: Boss timers", "Клавіша: Таймери босів",
     "Клавиша: Таймеры боссов"},
    {Text::InterfaceKeyTracker, "Key: Tracking", "Клавіша: Відстеження",
     "Клавиша: Отслеживание"},
    {Text::InterfaceKeyParty, "Key: Party", "Клавіша: Група", "Клавиша: Группа"},
    {Text::InterfaceKeyRecap, "Key: What killed me", "Клавіша: Що мене вбило",
     "Клавиша: Что меня убило"},
    {Text::InterfaceKeyHint, "Opens and hides the window; only letters the game leaves free with Ctrl",
     "Відкриває й ховає вікно; лише букви, які гра "
     "лишає вільними з Ctrl",
     "Открывает и прячет окно; только буквы, которые "
     "игра оставляет свободными с Ctrl"},
    {Text::KeyOff, "No key", "Без клавіші", "Без клавиши"},
    {Text::KeyCtrlB, "Ctrl+B", "Ctrl+B", "Ctrl+B"},
    {Text::KeyCtrlD, "Ctrl+D", "Ctrl+D", "Ctrl+D"},
    {Text::KeyCtrlG, "Ctrl+G", "Ctrl+G", "Ctrl+G"},
    {Text::KeyCtrlH, "Ctrl+H", "Ctrl+H", "Ctrl+H"},
    {Text::KeyCtrlI, "Ctrl+I", "Ctrl+I", "Ctrl+I"},
    {Text::KeyCtrlJ, "Ctrl+J", "Ctrl+J", "Ctrl+J"},
    {Text::KeyCtrlK, "Ctrl+K", "Ctrl+K", "Ctrl+K"},
    {Text::KeyCtrlL, "Ctrl+L", "Ctrl+L", "Ctrl+L"},
    {Text::KeyCtrlN, "Ctrl+N", "Ctrl+N", "Ctrl+N"},
    {Text::KeyCtrlU, "Ctrl+U", "Ctrl+U", "Ctrl+U"},
    {Text::KeyCtrlY, "Ctrl+Y", "Ctrl+Y", "Ctrl+Y"},
    {Text::CharOwnMore, "Windows of uaRO", "Вікна uaRO", "Окна uaRO"},  // 3.0.18-beta.128
    {Text::PadWheelTitle, "Wheel", "Колесо", "Колесо"},  // 3.0.18-beta.132
    {Text::TileBarProfile, "Bar profile {1}", "Профіль панелі {1}", "Профиль панели {1}"},  // 3.0.18-beta.136
    {Text::VendFree, "free slots: {1}",  // 3.0.18-beta.138
     "вільних слотів: {1}",
     "свободных слотов: {1}"},
    {Text::VendCardTip, "A thing's name: its card",
     "Назва речі - її картка",
     "Название вещи - её карточка"},
    {Text::SocialOnline, "{1} / {2} online",  // 3.0.18-beta.141
     "{1} / {2} у мережі",
     "{1} / {2} в сети"},
    {Text::SocialNearCells, "near \xC2\xB7 {1} cells",
     "поруч \xC2\xB7 {1} кл.",
     "рядом \xC2\xB7 {1} кл."},
    {Text::SocialNear, "near",
     "поруч",
     "рядом"},
    {Text::SocialDeadNow, "dead",
     "загинув",
     "погиб"},
    {Text::SocialRoute, "Route",
     "Маршрут",
     "Маршрут"},
    {Text::SocialAddFriend, "Add to friends",
     "Додати в друзі",
     "Добавить в друзья"},
    {Text::SocialCopy, "Copy the name",
     "Копіювати ім'я",
     "Копировать имя"},
    {Text::SocialExpEven, "Experience: shared",
     "Досвід: порівну",
     "Опыт: поровну"},
    {Text::SocialExpEach, "Experience: each own",
     "Досвід: кожному свій",
     "Опыт: каждому свой"},
    {Text::SocialNoPartyHint, "No party - make one: /organize Name",
     "Групи немає - створіть: /organize Назва",
     "Группы нет - создайте: /organize Название"},
    {Text::SocialNoFriendsHint, "No friends yet - type a name below and add",
     "Друзів поки немає - введіть ім'я нижче й додайте",
     "Друзей пока нет - введите имя ниже и добавьте"},
    {Text::SocialNameField, "A player's name",
     "Ім'я гравця",
     "Имя игрока"},
    {Text::SocialAdd, "Add",
     "Додати",
     "Добавить"},
    {Text::SocialMouseHint, "Right click on a person: what to do \xC2\xB7 twice: whisper",
     "Права кнопка по людині - дії \xC2\xB7 двічі - шепіт",
     "Правая кнопка по человеку - действия \xC2\xB7 дважды - шёпот"},
    {Text::SocialPadHint, "{1}: what to do \xC2\xB7 {2} / {3}: tabs \xC2\xB7 {4}: close",
     "{1}: дії \xC2\xB7 {2} / {3}: вкладки \xC2\xB7 {4}: закрити",
     "{1}: действия \xC2\xB7 {2} / {3}: вкладки \xC2\xB7 {4}: закрыть"},
    {Text::VendSetupAll, "If all sells: {1} z",  // 3.0.18-beta.142
     "Якщо продати все: {1} z",
     "Если продать всё: {1} z"},
    {Text::ThingCardTip, "Right click on a thing: its card",
     "Права кнопка по речі - її картка",
     "Правая кнопка по вещи - её карточка"},
    {Text::ThingForgedBy, "Forged by {1}",
     "Викував {1}",
     "Выковал {1}"},
    {Text::ThingForged, "Forged",
     "Кована",
     "Кованая"},
    {Text::VendPadCard, "its card", "картка", "карточка"},
    {Text::MyShopSoldCount, "sold {1}", "продано {1}", "продано {1}"},  // 3.0.18-beta.143
    {Text::MyShopEarned, "Earned", "Зароблено", "Заработано"},
    {Text::MyShopSales, "Sales", "Продажі", "Продажи"},
    {Text::MyShopNoSales, "Nothing sold yet", "Поки нічого не продано", "Пока ничего не продано"},
    {Text::TrackerEmptyTitle, "Nothing tracked yet",  // 3.0.18-beta.145
     "Поки нічого не відстежується",
     "Пока ничего не отслеживается"},
    {Text::TrackerEmptyHint, "Open «Reference» and pin a quest, things to collect or a way with «Track»",
     "Відкрийте «Довідник» і закріпіть квест, речі для збору чи шлях "
     "кнопкою «Відстежувати»",
     "Откройте «Справочник» и закрепите квест, вещи для сбора или путь "
     "кнопкой «Отслеживать»"},
    {Text::TrackerEmptyTag, "how", "як", "как"},
    {Text::ServersPlayers, "{1} playing", "{1} у грі", "{1} в игре"},  // 3.0.18-beta.153
    // 3.0.18-beta.154; .219 (the owner: «так и не понял как кнопкой 1 2 3 пользоваться»): what the row is for
    {Text::EquipSetsHint, "keep what is worn in a row - and change into it with one click",  // .220: «сделай явно»
     "збережіть надіте в рядок — і перевдягайтеся одним кліком",
     "сохраните надетое в строку — и переодевайтесь одним кликом"},
    {Text::StorageTakePicked, "Take the picked ({1})",
     "Забрати вибране ({1})",
     "Забрать выбранное ({1})"},
    {Text::PickedPut, "Put the picked ({1})",
     "Покласти вибране ({1})",
     "Положить выбранное ({1})"},
    {Text::PickedHint, "Ctrl+click picks, Shift+click a run, Ctrl+A all",
     "Ctrl+клік — вибрати, Shift+клік — підряд, Ctrl+A — усе",
     "Ctrl+клик — выбрать, Shift+клик — подряд, Ctrl+A — всё"},
    {Text::CountFits, "The weight lets in {1} of {2} - empty: {1}",
     "За вагою влізе {1} з {2} — порожнє поле: {1}",
     "По весу влезет {1} из {2} — пустое поле: {1}"},
    {Text::LauncherPlay, "PLAY", "ГРАТИ", "ИГРАТЬ"},
    {Text::LauncherUpdate, "UPDATE", "ОНОВИТИ", "ОБНОВИТЬ"},
    {Text::LauncherUpdating, "Updating {1} % · {2} MB/s", "Оновлення {1} % · {2} МБ/с", "Обновление {1} % · {2} МБ/с"},
    // 3.0.18-beta.206 (the owner: «не явно сказано что можно запустить еще одно окно»): the button's word while it runs
    {Text::LauncherRunning, "+ ONE MORE WINDOW", "+ ЩЕ ОДНЕ ВІКНО", "+ ЕЩЁ ОДНО ОКНО"},
    {Text::LauncherServer, "Server up · {1} online", "Сервер працює · {1} онлайн", "Сервер работает · {1} онлайн"},
    {Text::LauncherServerDown, "Server unreachable", "Сервер недоступний", "Сервер недоступен"},
    {Text::LauncherServerAsking, "Asking the server…", "Питаю сервер…", "Спрашиваю сервер…"},
    {Text::LauncherWhatsNew, "What's new", "Що нового", "Что нового"},
    {Text::LauncherUpdated, "Updated to {1}", "Оновлено до {1}", "Обновлено до {1}"},
    {Text::LauncherNewSince, "Updates since your last game: {1}",
     "Оновлень з вашої останньої гри: {1}",
     "Обновлений с вашей последней игры: {1}"},
    {Text::LauncherLog, "Changelog →", "Журнал змін →", "Журнал изменений →"},
    {Text::LauncherSettings, "Settings", "Налаштування", "Настройки"},
    {Text::LauncherReports, "Reports", "Звернення", "Обращения"},
    {Text::LauncherSite, "Site", "Сайт", "Сайт"},
    {Text::LauncherForum, "Forum", "Форум", "Форум"},
    {Text::LauncherAnother, "One more game window", "Ще вікно гри", "Ещё окно игры"},
    {Text::LauncherSafe, "Safe start", "Безпечний запуск", "Безопасный запуск"},
    {Text::LauncherNoUpdate, "Play without the update", "Грати без оновлення", "Играть без обновления"},
    {Text::LauncherVersionLine, "{1} · {2}", "{1} · {2}", "{1} · {2}"},
    {Text::LauncherAuto, "auto for {1}", "авто для {1}", "авто для {1}"},
    {Text::LauncherChange, "change ›", "змінити ›", "изменить ›"},
    {Text::LauncherBack, "← To the game", "← До гри", "← К игре"},
    {Text::LauncherTagline, "Ragnarok Online", "Ragnarok Online", "Ragnarok Online"},
    {Text::LauncherBeta, "beta", "бета", "бета"},
    {Text::LauncherRelease, "release", "реліз", "релиз"},
    {Text::LauncherPlateSafeAuto, "Safe mode: the last starts failed, so effects, Direct3D 12 and DLSS are off",
     "Безпечний режим: останні запуски не вдалися, тож ефекти, Direct3D 12 і DLSS вимкнено",
     "Безопасный режим: последние запуски не удались, поэтому эффекты, Direct3D 12 и DLSS выключены"},
    {Text::LauncherPlateSafe, "Safe mode is on: effects, Direct3D 12 and DLSS are off",
     "Безпечний режим увімкнено: ефекти, Direct3D 12 і DLSS вимкнено",
     "Безопасный режим включён: эффекты, Direct3D 12 и DLSS выключены"},
    {Text::LauncherPlateOn, "Switch on again", "Увімкнути знову", "Включить снова"},
    {Text::LauncherPickAuto, "Auto for your graphics card", "Авто для вашої відеокарти", "Авто для вашей видеокарты"},
    {Text::LauncherPickTest, "Check the graphics card", "Перевірити відеокарту", "Проверить видеокарту"},
    {Text::LauncherPickAll, "All settings ›", "Усі налаштування ›", "Все настройки ›"},
    {Text::LauncherPickUndo, "Changed · Undo", "Змінено · Скасувати", "Изменено · Отменить"},
    {Text::LauncherPickNoCard, "the graphics card is not known yet",
     "відеокарту ще не визначено",
     "видеокарта ещё не определена"},
    {Text::LauncherLogTitle, "Changelog", "Журнал змін", "Журнал изменений"},
    {Text::LauncherFilterAll, "All", "Усе", "Всё"},
    {Text::LauncherFilterReleases, "Releases only", "Лише релізи", "Только релизы"},
    {Text::LauncherFilterNew, "New for me · {1}", "Нове для мене · {1}", "Новое для меня · {1}"},
    {Text::LauncherTagNew, "new", "нове", "новое"},
    {Text::LauncherTry, "Try it", "Спробувати", "Попробовать"},
    {Text::LauncherToday, "Today", "Сьогодні", "Сегодня"},
    {Text::LauncherYesterday, "Yesterday", "Вчора", "Вчера"},
    {Text::LauncherMonths, "January|February|March|April|May|June|July|August|September|October|November|December",
     "січня|лютого|березня|квітня|травня|червня|липня|серпня|вересня|жовтня|листопада|грудня",
     "января|февраля|марта|апреля|мая|июня|июля|августа|сентября|октября|ноября|декабря"},
    {Text::LauncherLogEmpty, "Nothing here yet", "Тут поки порожньо", "Здесь пока пусто"},
    {Text::LauncherFirstSkip, "Skip - the app chooses itself",
     "Пропустити — вибере сама програма",
     "Пропустить — выберет сама программа"},
    {Text::LauncherFirstFits, "for your graphics card", "для вашої відеокарти", "для вашей видеокарты"},
    {Text::LauncherPickChecked, "Checked: {1} ({2}) - {3}",
     "Перевірено: {1} ({2}) — {3}",
     "Проверено: {1} ({2}) — {3}"},
    {Text::LauncherByMemory, "by its memory", "за пам'яттю відеокарти", "по памяти видеокарты"},
    {Text::LauncherByGame, "by how it drew the game", "за тим, як вона малювала гру", "по тому, как она рисовала игру"},
    {Text::LauncherUpdateWhat, "{1} · {2} MB", "{1} · {2} МБ", "{1} · {2} МБ"},
    {Text::LauncherFirstFound, "Already chosen for {1}: {2} · {3}", "Уже підібрано для {1}: {2} · {3}",
     "Уже подобрано для {1}: {2} · {3}"},
    {Text::LauncherCheckingNow, "Checking…", "Перевіряю…", "Проверяю…"},
    {Text::LauncherChecking, "checking…", "перевіряю…", "проверяю…"},
    {Text::LauncherCheckFailed, "not checked", "не вдалося перевірити", "не удалось проверить"},
    {Text::LauncherCheck, "Check", "Перевірити", "Проверить"},
    {Text::LauncherChannelBeta, "Beta", "Бета", "Бета"},
    {Text::LauncherRunningCount, "The game is running · windows: {1}", "Гра вже запущена · вікон: {1}",
     "Игра уже запущена · окон: {1}"},
    {Text::InterfaceArrowsWalk, "Walking with the arrows", "Ходьба стрілками", "Ходьба стрелками"},
    {Text::InterfaceArrowsWalkHint,
     "The arrow keys walk the character while the chat line is empty: up is away from the camera, two arrows a "
     "diagonal, let go - it stops. Off: the arrows are the game's, as before.",
     "Стрілки ведуть персонажа, поки рядок чату порожній: вгору — від камери, дві стрілки — навскіс, відпустили — "
     "зупиняється. Вимкнено: стрілки, як раніше, у гри.",
     "Стрелки ведут персонажа, пока строка чата пуста: вверх — от камеры, две стрелки — по диагонали, отпустили — "
     "останавливается. Выключено: стрелки, как раньше, у игры."},
    {Text::InterfaceWalkMode, "Walk mode (` key)", "Режим ходьби (клавіша `)", "Режим ходьбы (клавиша `)"},
    {Text::InterfaceWalkModeHint,
     "` (the key left of 1) switches it in the game while the chat line is empty: the letters stop typing, W A S D, "
     "the arrows and the numpad walk, Enter opens the chat line and sends it, Esc clears it; Tab / Shift+Tab the "
     "target, Space attack, F pick up, R talk, C sit, Q / E the camera, 1-9 the bar. The chat line says it: "
     "«Walking · Enter: type».",
     "` (клавіша ліворуч від 1) вмикає його в грі, поки рядок чату порожній: літери не пишуться в чат, W A S D, "
     "стрілки й цифровий блок ведуть персонажа, Enter відкриває рядок чату й надсилає, Esc стирає; Tab / Shift+Tab — "
     "ціль, пробіл — атака, F — підібрати, R — NPC, C — сісти, Q / E — камера, 1–9 — панель. Рядок чату "
     "показує: «Ходьба · Enter — писати».",
     "` (клавиша слева от 1) включает его в игре, пока строка чата пуста: буквы не пишутся в чат, W A S D, "
     "стрелки и цифровой блок ведут персонажа, Enter открывает строку чата и отправляет, Esc стирает; Tab / "
     "Shift+Tab — цель, пробел — атака, F — подобрать, R — NPC, C — сесть, Q / E — камера, 1–9 — панель. Строка "
     "чата показывает: «Ходьба · Enter — писать»."},
    {Text::ChatHintWalk, "Walking \xC2\xB7 Enter: type \xC2\xB7 `: off",
     "Ходьба \xC2\xB7 Enter — писати \xC2\xB7 ` — вимкнути", "Ходьба \xC2\xB7 Enter — писать \xC2\xB7 ` — выключить"},
    {Text::InterfaceSpaceAction, "Space acts", "Пробіл — дія", "Пробел — действие"},
    {Text::InterfaceSpaceActionHint,
     "Space with the chat line empty does what the gamepad's A does: attacks the target, else picks up loot beside, "
     "else attacks the nearest monster, else talks to the NPC near. Ctrl+arrows turn the camera and zoom it.",
     "Пробіл, коли рядок чату порожній, робить те саме, що A геймпада: б'є ціль, інакше підбирає лут поруч, інакше "
     "б'є найближчого монстра, інакше говорить з NPC поруч. Ctrl+стрілки повертають і наближають камеру.",
     "Пробел, когда строка чата пуста, делает то же, что A геймпада: бьёт цель, иначе подбирает лут рядом, иначе "
     "бьёт ближайшего монстра, иначе говорит с NPC рядом. Ctrl+стрелки поворачивают и приближают камеру."},
    {Text::EquipSetsTitle, "Sets", "Комплекти", "Комплекты"},  // 3.0.18-beta.219, .220
    {Text::EquipSetEmpty, "empty", "порожній", "пустой"},
    {Text::EquipSetPutOn, "Put on", "Надягти", "Надеть"},
    {Text::EquipSetWornNow, "\xE2\x9C\x93 Worn", "\xE2\x9C\x93 Надіто", "\xE2\x9C\x93 Надето"},
    {Text::EquipSetMenuKeep, "Keep what is worn", "Зберегти надіте", "Сохранить надетое"},
    {Text::EquipSetMenuClear, "Clear", "Очистити", "Очистить"},
    {Text::EquipSetKept, "Set {1} kept - things: {2}", "Комплект {1} збережено — речей: {2}",
     "Комплект {1} сохранён — вещей: {2}"},
    {Text::EquipSetNothingWorn, "Nothing is worn - nothing to keep", "Нічого не надіто — нема що зберегти",
     "Ничего не надето — нечего сохранять"},
    {Text::EquipSetPutting, "Putting on set {1}…", "Надягаю комплект {1}…", "Надеваю комплект {1}…"},
    {Text::EquipSetDone, "Set {1} worn", "Комплект {1} надіто", "Комплект {1} надет"},
    {Text::EquipSetMissing, "Set {1}: not in the bag - {2}", "Комплект {1}: немає в сумці — {2}",
     "Комплект {1}: нет в сумке — {2}"},
    {Text::EquipSetPartly, "Set {1}: not all of it could be worn", "Комплект {1}: не все вдалося надягти",
     "Комплект {1}: не всё удалось надеть"},
    {Text::EquipSetCleared, "Set {1} cleared", "Комплект {1} очищено", "Комплект {1} очищен"},
    {Text::ItemsNotWorn, "not worn", "не вдягнуто", "не надето"},  // 3.0.18-beta.224 (R-5K5NRW)
    {Text::MenuKeys, "Keys", "Клавіші", "Клавиши"},  // 3.0.18-beta.225
    {Text::MenuKeysBack, "Back", "Назад", "Назад"},
    {Text::MenuAccount, "Change account", "Змінити акаунт", "Сменить аккаунт"},  // 3.0.18-beta.226
    {Text::MakeKeysHint, "↑ ↓ row · ← → change", "↑ ↓ рядок · ← → змінити", "↑ ↓ строка · ← → изменить"},  // .228
    {Text::FxFixNegative, "No negative after skill effects", "Без негативу після ефектів умінь",
     "Без негатива после эффектов умений"},  // 3.0.18-beta.229 (R-H2CQCJ)
    {Text::FxFixNegativeHint,
     "Some skill effects are drawn by the game in inverted colours, and it forgets to set them back: the whole world "
     "turns into a negative or the ground washes white. The inverted colours stay on the effect itself.",
     "Деякі ефекти вмінь гра малює вивернутими кольорами й забуває повернути звичайні — тоді весь світ стає "
     "негативом або земля біліє. Вивернуті кольори лишаються тільки на самому ефекті.",
     "Некоторые эффекты умений игра рисует вывернутыми цветами и забывает вернуть обычные — тогда весь мир "
     "становится негативом или земля белеет. Вывернутые цвета остаются только на самом эффекте."},
    // 3.0.18-beta.240 (the reference's maps, the owner 13.11): the NPCs' groups on a map, the NPC card, the layers
    {Text::RefNpcGroupKafra, "Kafra", "Кафра", "Кафра"},
    {Text::RefNpcGroupMerchant, "Merchants and services", "Торговці й послуги", "Торговцы и услуги"},
    {Text::RefNpcGroupQuest, "Quests", "Квести", "Квесты"},
    {Text::RefNpcGroupCustom, "Our server's NPCs", "Наші NPC сервера", "Наши NPC сервера"},
    {Text::RefNpcGroupGuide, "Guides", "Провідники", "Проводники"},
    {Text::RefNpcGroupCity, "Townsfolk", "Міські жителі", "Горожане"},
    {Text::RefNpcGroupGuild, "Guilds", "Гільдії", "Гильдии"},
    {Text::RefNpcGroupJob, "Jobs", "Професії", "Профессии"},
    {Text::RefNpcGroupEvent, "Events", "Події", "События"},
    {Text::RefNpcGroupFerry, "Ferrymen", "Перевізники", "Перевозчики"},
    {Text::RefNpcGroupOther, "Others", "Інші", "Другие"},
    {Text::RefNpcLead, "Lead me here ›", "Вести сюди ›", "Вести сюда ›"},
    {Text::RefNpcOnMap, "Show on the map ›", "Показати на карті ›", "Показать на карте ›"},
    {Text::RefNpcFerry, "Takes you to", "Куди везе", "Куда везёт"},
    {Text::RefNpcByKind, "By kind", "За видами", "По видам"},
    {Text::RefLayerNpc, "NPC", "NPC", "NPC"},
    {Text::RefLayerWarps, "Warps", "Переходи", "Переходы"},
    {Text::RefLayerMobs, "Monsters", "Монстри", "Монстры"},
    {Text::RefLayerHere, "You are here", "Ви тут", "Вы здесь"},
    {Text::RefPadKeys,
     "{1} open · {2} back · {3} lead me here · {4} search · {5} {6} sections",
     "{1} відкрити · {2} назад · {3} вести сюди · {4} пошук · {5} {6} розділи",
     "{1} открыть · {2} назад · {3} вести сюда · {4} поиск · {5} {6} разделы"},
    {Text::RefPadCrossKeys,
     "Stick — move · {1} open · {3} next mark · {5} {6} zoom · {2} leave the map",
     "Стік — рух · {1} відкрити · {3} наступна позначка · {5} {6} масштаб · {2} вийти з карти",
     "Стик — движение · {1} открыть · {3} следующая отметка · {5} {6} масштаб · {2} выйти из карты"},
    {Text::RefPadWorldKeys,
     "Stick — move · {1} a map's card · {5} {6} zoom · {2} leave the map",
     "Стік — рух · {1} картка карти · {5} {6} масштаб · {2} вийти з карти",
     "Стик — движение · {1} карточка карты · {5} {6} масштаб · {2} выйти из карты"},
    {Text::RefTimersOwn, "Your timers", "Ваші таймери", "Ваши таймеры"},
    {Text::RefHaveBag, "in the bag {1}", "у сумці {1}", "в сумке {1}"},
    {Text::RefHaveCart, "in the cart {1}", "у візку {1}", "в тележке {1}"},
    {Text::RefHaveStorage, "in storage {1}", "на складі {1}", "на складе {1}"},
    {Text::RefCraftHave, "have {1}", "є {1}", "есть {1}"},
    {Text::RefCraftAll, "have all", "є все", "есть всё"},
    {Text::RefCraftMissing, "missing {1}", "бракує {1}", "не хватает {1}"},
    {Text::RefOfN, "{1} of {2}", "{1} з {2}", "{1} из {2}"},
    {Text::RefInReference, "In the reference ›", "У довіднику ›", "В справочнике ›"},
    {Text::RefSecSkills, "Skills", "Навички", "Навыки"},
    {Text::RefSecSkillsHint, "by jobs: levels, SP, words", "за професіями: рівні, SP, опис",
     "по профессиям: уровни, SP, описание"},
    {Text::RefJobsNovice, "Novice", "Новачок", "Новичок"},
    {Text::RefJobsFirst, "First jobs", "Перші професії", "Первые профессии"},
    {Text::RefJobsSecond, "Second jobs", "Другі професії", "Вторые профессии"},
    {Text::RefJobsTrans, "Transcendent", "Перероджені", "Перерождённые"},
    {Text::RefJobsOther, "Other jobs", "Інші професії", "Другие профессии"},
    {Text::RefSkillsN, "skills: {1}", "навичок: {1}", "навыков: {1}"},
    {Text::RefSkillsEarlier, "{1} skills ›", "Навички {1} ›", "Навыки {1} ›"},
    {Text::RefSkillKindWeapon, "Physical", "Фізична", "Физическая"},
    {Text::RefSkillKindMagic, "Magic", "Магічна", "Магическая"},
    {Text::RefSkillKindMisc, "Other", "Інша", "Другая"},
    {Text::RefSkillKindNone, "Passive/other", "Пасивна/інша", "Пассивная/другая"},
    {Text::RefSkillWeaponElement, "The weapon's element", "Стихія зброї", "Стихия оружия"},
    {Text::RefSkillOnEnemy, "on an enemy", "на ворога", "на врага"},
    {Text::RefSkillOnGround, "on the ground", "на землю", "на землю"},
    {Text::RefSkillOnSelf, "on oneself", "на себе", "на себя"},
    {Text::RefSkillOnFriend, "on an ally", "на союзника", "на союзника"},
    {Text::RefSkillTrap, "a trap", "пастка", "ловушка"},
    {Text::RefSkillSpByLevel, "SP by level", "SP за рівнями", "SP по уровням"},
    {Text::RefSkillAllLevels, "All levels", "Усі рівні", "Все уровни"},
    {Text::RefSkillLevelN, "Level {1}", "Рівень {1}", "Уровень {1}"},
    {Text::RefSkillNeeds, "Needs", "Потрібно", "Нужно"},
    {Text::RefSkillJobs, "Who learns it", "Хто вчить", "Кто учит"},
    {Text::RefFoundSkills, "Skills · {1}", "Навички · {1}", "Навыки · {1}"},
    {Text::RefKindSkill, "skill", "навичка", "навык"},
    {Text::RefTypedUk, "Typed in the Ukrainian layout - searching {1}", "Набрано в українській розкладці — шукаю {1}",
     "Набрано в украинской раскладке — ищу {1}"},
    {Text::RefTypedRu, "Typed in the Russian layout - searching {1}", "Набрано в російській розкладці — шукаю {1}",
     "Набрано в русской раскладке — ищу {1}"},
    {Text::RefSearchAsTyped, "search as typed ›", "шукати як набрано ›", "искать как набрано ›"},
    {Text::RefCellMaps, "maps", "карт", "карт"},
    {Text::RefCellWeight, "weight", "вага", "вес"},
    {Text::RefCellShop, "in a shop", "у магазині", "в магазине"},
    {Text::RefCellChance, "chance", "шанс", "шанс"},
    {Text::RefCellNeed, "needed", "треба", "нужно"},
    {Text::RefCellHave, "in the bag", "у сумці", "в сумке"},
    {Text::RefCellHere, "on the map", "на карті", "на карте"},
    {Text::RefCellGoods, "goods", "товарів", "товаров"},
    {Text::RefCellPlayers, "players", "гравців", "игроков"},
    {Text::RefCellKinds, "monsters", "видів", "видов"},
    {Text::RefCellRespawn, "respawn", "поява", "появление"},
    {Text::RefCellWeaponLv, "weapon Lv", "Lv зброї", "Lv оружия"},
    {Text::RefCellBookLv, "cookbook", "книга", "книга"},
    {Text::RefDroppedFrom, "dropped by {1} · {2}", "випадає з {1} · {2}", "выпадает из {1} · {2}"},
    {Text::RefFromCraft, "crafted", "крафт", "крафт"},
    {Text::RefForQuest, "needed for a quest", "потрібен для квесту", "нужен для квеста"},
    {Text::RefForPet, "for a pet", "для пета", "для питомца"},
    {Text::RefCraftCooking, "Cooking", "Кулінарія", "Кулинария"},
    {Text::RefCraftPotions, "Potions", "Зілля", "Зелья"},
    {Text::RefCraftWeapons, "Weapons", "Зброя", "Оружие"},
    {Text::RefCraftMetals, "Metals and stones", "Метали й камені", "Металлы и камни"},
    {Text::RefCraftConverters, "Converters", "Конвертери", "Конвертеры"},
    {Text::RefCraftOther, "Other", "Інше", "Другое"},
    {Text::RefFactWeight, "Weight", "Вага", "Вес"},
    {Text::RefFactPrice, "Price", "Ціна", "Цена"},
    {Text::RefFactSells, "Sells for", "Продаж", "Продажа"},
    {Text::RefFactSlots, "Slots", "Слоти", "Слоты"},
    {Text::RefFactClass, "Class", "Клас", "Класс"},
    {Text::RefFactAttack, "Attack", "Атака", "Атака"},
    {Text::RefFactDefense, "Defense", "Захист", "Защита"},
    {Text::RefFactWeaponLevel, "Weapon level", "Рівень зброї", "Уровень оружия"},
    {Text::RefFactRequiredLevel, "Required level", "Потрібен рівень", "Нужен уровень"},
    {Text::RefFactJobs, "Jobs", "Професії", "Профессии"},
    {Text::RefFactLocation, "Worn on", "Місце", "Место"},
    {Text::RefFactElement, "Element", "Стихія", "Стихия"},
    {Text::RefFactCompoundOn, "Goes into", "Вставляється в", "Вставляется в"},
    {Text::RefFactHave, "You have", "У вас", "У вас"},
    {Text::RefFactRange, "Attack range", "Дальність атаки", "Дальность атаки"},
    {Text::RefFactStep, "A step", "Крок", "Шаг"},
    {Text::RefFactAttackEvery, "Attacks every", "Атака раз на", "Атака раз в"},
    {Text::RefMs, "{1} ms", "{1} мс", "{1} мс"},
    {Text::RefSeconds, "{1} s", "{1} с", "{1} с"},
    {Text::RefModeAggressive, "Aggressive", "Агресивний", "Агрессивный"},
    {Text::RefModePassive, "Passive", "Пасивний", "Пассивный"},
    {Text::RefModeLooter, "Picks up items", "Підбирає речі", "Подбирает вещи"},
    {Text::RefModeAssist, "Helps its kind", "Допомагає своїм", "Помогает своим"},
    {Text::RefModeCastSensor, "Hears casting", "Чує каст", "Слышит каст"},
    {Text::RefModeDetector, "Sees the hidden", "Бачить схованих", "Видит скрытых"},
    {Text::RefModeStill, "Does not move", "Не рухається", "Не двигается"},
    {Text::RefModePlant, "Plant", "Рослина", "Растение"},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "a text for every Text");

// The first entry out of the order of Text, Count when none: a text put in at another place shifts every text
// after it onto its neighbour's row (3.0.2-beta.4 and beta.5 showed the Main page's keys that way).
constexpr size_t firstOutOfOrder() {
    for (size_t i = 0; i < sizeof(kTexts) / sizeof(kTexts[0]); ++i)
        if (kTexts[i].id != Text(i)) return i;
    return size_t(Text::Count);
}
static_assert(firstOutOfOrder() == size_t(Text::Count), "kTexts in the order of Text");

}  // namespace

std::optional<Lang> parseLang(const std::string& token) {
    std::string t;
    for (char ch : token) t += char(std::tolower(static_cast<unsigned char>(ch)));
    if (t == "en") return Lang::En;
    if (t == "uk") return Lang::Uk;
    if (t == "ru") return Lang::Ru;
    return std::nullopt;
}

const char* langToken(Lang l) { return l == Lang::Uk ? "uk" : l == Lang::Ru ? "ru" : "en"; }

const char* langName(Lang l) { return l == Lang::Uk ? "Українська" : l == Lang::Ru ? "Русский" : "English"; }

Lang startLang(std::optional<Lang> chosen) { return chosen ? *chosen : Lang::En; }

Lang windowLang(std::optional<Lang> chosen, unsigned primaryLanguage) {
    if (chosen) return *chosen;
    return primaryLanguage == 0x22 ? Lang::Uk : primaryLanguage == 0x19 ? Lang::Ru : Lang::En;
}

const char* tr(Lang l, Text t) {
    const Entry& e = kTexts[size_t(t)];
    return l == Lang::Uk ? e.uk : l == Lang::Ru ? e.ru : e.en;
}

Text effectsSetText(int level) {
    static const Text kSets[] = {Text::TestSetOff, Text::FxPresetWeak, Text::FxPresetMedium, Text::FxPresetStrong};
    return kSets[level < 0 ? 0 : level > 3 ? 3 : level];
}

std::string fmt(const std::string& pattern, const std::vector<std::string>& args) {
    std::string out;
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '{' && i + 2 < pattern.size() && pattern[i + 2] == '}' && pattern[i + 1] >= '1' &&
            pattern[i + 1] <= '9') {
            const size_t n = size_t(pattern[i + 1] - '1');
            out += n < args.size() ? args[n] : std::string();
            i += 2;
        } else {
            out += pattern[i];
        }
    }
    return out;
}

}  // namespace uagfx::settings

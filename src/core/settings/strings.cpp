#include "core/settings/strings.hpp"

#include <cctype>

namespace uagfx::settings {
namespace {

struct Entry {
    const char* en;
    const char* uk;
    const char* ru;
};

// In the order of Text.
const Entry kTexts[] = {
    {"uaRO Graphics Settings", "uaRO Graphics Settings", "uaRO Graphics Settings"},
    {"Renderer", "Рендер", "Рендер"},
    {"Screen", "Екран", "Экран"},
    {"Quality", "Якість", "Качество"},
    {"Frames", "Кадри", "Кадры"},
    {"Diagnostics", "Діагностика", "Диагностика"},
    {"Service", "Сервіс", "Сервис"},
    {"&Apply", "&Застосувати", "&Применить"},
    {"Apply and &play", "Застосувати й &грати", "Применить и и&грать"},
    {"&Undo", "&Скасувати", "&Отменить"},
    {"&Close", "За&крити", "За&крыть"},
    {"The game is running.", "Гра запущена.", "Игра запущена."},
    {"The game is not running.", "Гра не запущена.", "Игра не запущена."},
    {"Not applied: {1}.", "Не застосовано: {1}.", "Не применено: {1}."},
    {"Applied.", "Застосовано.", "Применено."},
    {"The settings app runs as administrator: a game started from it would not see its settings. Close it and start "
     "it normally.",
     "Утиліту запущено від імені адміністратора: гра, запущена з неї, не побачить свої налаштування. Закрий її та "
     "запусти звичайно.",
     "Утилита запущена от имени администратора: игра, запущенная из неё, не увидит свои настройки. Закрой её и "
     "запусти обычно."},
    {"↻ after the game restarts", "↻ після перезапуску гри", "↻ после перезапуска игры"},
    {"These settings work in a running game too, without a restart.",
     "Ці налаштування діють і в запущеній грі, без перезапуску.",
     "Эти настройки действуют и в запущенной игре, без перезапуска."},
    {"The game is running: a slider applies as soon as you let it go.",
     "Гра запущена: повзунок застосовується, щойно його відпустиш.",
     "Игра запущена: ползунок применяется, как только его отпустишь."},
    {"Some changes are not applied. Close without them?", "Деякі зміни не застосовано. Закрити без них?",
     "Часть изменений не применена. Закрыть без них?"},
    {"What draws the game", "Чим малювати гру", "Чем рисовать игру"},
    {"uaRO Graphics (Direct3D 12)", "uaRO Graphics (Direct3D 12)", "uaRO Graphics (Direct3D 12)"},
    {"dgVoodoo", "dgVoodoo", "dgVoodoo"},
    {"Our renderer: the picture at the screen's real pixels, sharpening, crisp text. Needs Windows 10 or newer.",
     "Наш рендер: картинка в справжніх точках екрана, різкість, чіткий текст. Потрібна Windows 10 або новіша.",
     "Наш рендер: картинка в настоящих точках экрана, резкость, чёткий текст. Нужна Windows 10 или новее."},
    {"As before: dgVoodoo draws the game.", "Як раніше: гру малює dgVoodoo.", "Как раньше: игру рисует dgVoodoo."},
    {"Our renderer is not available: it needs Windows 10 or newer.",
     "Наш рендер недоступний: потрібна Windows 10 або новіша.", "Наш рендер недоступен: нужна Windows 10 или новее."},
    {"Our renderer is not available: Direct3D 12 does not start ({1}).",
     "Наш рендер недоступний: Direct3D 12 не запускається ({1}).",
     "Наш рендер недоступен: Direct3D 12 не запускается ({1})."},
    {"Installed: uaRO Graphics {1}.", "Встановлено: uaRO Graphics {1}.", "Установлено: uaRO Graphics {1}."},
    {"uaRO Graphics' ddraw.dll is not in the game folder: run install.bat.",
     "ddraw.dll uaRO Graphics немає в теці гри: запусти install.bat.",
     "ddraw.dll uaRO Graphics нет в папке игры: запусти install.bat."},
    {"dgVoodoo is in the dgvoodoo folder.", "dgVoodoo — у теці dgvoodoo.", "dgVoodoo — в папке dgvoodoo."},
    {"dgVoodoo is not in the dgvoodoo folder.", "dgVoodoo немає в теці dgvoodoo.", "dgVoodoo нет в папке dgvoodoo."},
    {"In the game, Pause (or Scroll Lock) switches every improvement off and on, to compare.",
     "У грі клавіша Pause (або Scroll Lock) вимикає й вмикає всі покращення — для порівняння.",
     "В игре клавиша Pause (или Scroll Lock) выключает и включает все улучшения — для сравнения."},
    {"Show", "Показ", "Показ"},
    {"Full screen", "На весь екран", "Во весь экран"},
    {"In a window", "У вікні", "В окне"},
    {"Monitor", "Монітор", "Монитор"},
    {"{1}: {2}×{3}, scaling {4} %", "{1}: {2}×{3}, масштаб {4} %", "{1}: {2}×{3}, масштаб {4} %"},
    {", main", ", основний", ", основной"},
    {"Interface size", "Розмір інтерфейсу", "Размер интерфейса"},
    {"Screen pixels per game pixel. More: a larger interface and less of the world.",
     "Скільки точок екрана на одну точку гри. Більше — більший інтерфейс і менше видно світу.",
     "Сколько точек экрана на одну точку игры. Больше — крупнее интерфейс и меньше видно мира."},
    {"exactly, %", "точно, %", "точно, %"},
    {"With dgVoodoo the interface size is Windows scaling ({1} %).",
     "З dgVoodoo розмір інтерфейсу — це масштаб Windows ({1} %).",
     "С dgVoodoo размер интерфейса — это масштаб Windows ({1} %)."},
    {"Monitor mode", "Режим монітора", "Режим монитора"},
    {"dgVoodoo may switch the monitor to this mode.", "dgVoodoo може перемкнути монітор у цей режим.",
     "dgVoodoo может переключить монитор в этот режим."},
    {"Window size", "Розмір вікна", "Размер окна"},
    {"In the screen's real pixels; only sizes that fit the screen above the task bar.",
     "У справжніх точках екрана; лише ті, що вміщуються над панеллю завдань.",
     "В настоящих точках экрана; только те, что помещаются над панелью задач."},
    {"{1}×{2} (game {3}×{4})", "{1}×{2} (гра {3}×{4})", "{1}×{2} (игра {3}×{4})"},
    {"Game {1}×{2} → screen {3}×{4}, interface ×{5}", "Гра {1}×{2} → екран {3}×{4}, інтерфейс ×{5}",
     "Игра {1}×{2} → экран {3}×{4}, интерфейс ×{5}"},
    {" — the crispest interface", " — найчіткіший інтерфейс", " — самый чёткий интерфейс"},
    {"It will not fit: the window is larger than the screen above the task bar.",
     "Не вміститься: вікно більше за екран над панеллю завдань.",
     "Не поместится: окно больше экрана над панелью задач."},
    {"Too small: the game needs at least 640×480.", "Замало: грі потрібно щонайменше 640×480.",
     "Слишком мало: игре нужно не меньше 640×480."},
    {"Not a valid size.", "Неправильний розмір.", "Неверный размер."},
    {"Now: in a window · window {1}×{2} · interface {3} % · game {4}×{5}",
     "Зараз: у вікні · вікно {1}×{2} · інтерфейс {3} % · гра {4}×{5}",
     "Сейчас: в окне · окно {1}×{2} · интерфейс {3} % · игра {4}×{5}"},
    {"Now: full screen · game {1}×{2} · interface {3} %", "Зараз: на весь екран · гра {1}×{2} · інтерфейс {3} %",
     "Сейчас: во весь экран · игра {1}×{2} · интерфейс {3} %"},
    {"setup.exe has {1}×{2}, {3}; the game uses this app's values.",
     "У setup.exe {1}×{2}, {3}; гра бере значення цієї утиліти.",
     "В setup.exe {1}×{2}, {3}; игра берёт значения этой утилиты."},
    {"in a window", "у вікні", "в окне"},
    {"full screen", "на весь екран", "во весь экран"},
    {"As in setup.exe", "Як у setup.exe", "Как в setup.exe"},
    {"ROExt sets the window size ({1}): the app writes the same size there.",
     "Розмір вікна задає ROExt ({1}): утиліта запише туди той самий розмір.",
     "Размер окна задаёт ROExt ({1}): утилита запишет туда тот же размер."},
    {"The game has no settings in the registry yet: Apply creates everything it needs, no setup.exe required.",
     "У реєстрі ще немає налаштувань гри: «Застосувати» створить усе потрібне, setup.exe не знадобиться.",
     "В реестре ещё нет настроек игры: «Применить» создаст всё нужное, setup.exe не понадобится."},
    {"In full screen the game minimises itself on Alt+Tab: that is how it works.",
     "На весь екран гра сама згортається при Alt+Tab — так вона влаштована.",
     "Во весь экран игра сама сворачивается при Alt+Tab — так она устроена."},
    {"High resolution", "Висока роздільність", "Высокое разрешение"},
    {"Draw the game at the window's real pixels: with Windows scaling the world is sharper.",
     "Малювати гру в справжніх точках вікна: при масштабі Windows світ чіткіший.",
     "Рисовать игру в настоящих точках окна: при масштабе Windows мир чётче."},
    {"Render scale", "Масштаб рендера", "Масштаб рендера"},
    {"Over 100 %: the frame is drawn larger and shrunk when shown - smoother edges, more work for the graphics card. "
     "Under 100 % (a large screen, a weaker card): drawn smaller and made larger sharply (a Catmull-Rom filter and "
     "CAS sharpening, like FSR 1) - less work, a little softer.",
     "Понад 100 %: кадр малюється більшим і зменшується при показі — гладші краї, відеокарта працює більше. Менше 100 "
     "% (великий екран, слабша карта): малюється меншим і збільшується різко (фільтр Catmull-Rom і різкість CAS, як "
     "FSR 1) — менше роботи, трохи м'якше.",
     "Больше 100 %: кадр рисуется больше и уменьшается при показе — глаже края, видеокарта работает больше. Меньше "
     "100 % (большой экран, слабая карта): рисуется меньше и увеличивается резко (фильтр Catmull-Rom и резкость CAS, "
     "как FSR 1) — меньше работы, чуть мягче."},
    {"Sharpening", "Різкість", "Резкость"},
    {"Sharpening of the picture on screen; 0 = off. Screenshots are not sharpened.",
     "Різкість картинки на екрані; 0 — вимкнено. Скриншоти не змінюються.",
     "Резкость картинки на экране; 0 — выключено. Скриншоты не меняются."},
    {"Interface pictures", "Картинки інтерфейсу", "Картинки интерфейса"},
    {"How enlarged interface pictures look. With crisp text the windows come from their crisp twins.",
     "Як виглядають збільшені картинки інтерфейсу. З чітким текстом вікна беруться з чітких двійників.",
     "Как выглядят увеличенные картинки интерфейса. При чётком тексте окна берутся из чётких двойников."},
    {"sharp", "чіткі", "чёткие"},
    {"smooth", "м'які", "мягкие"},
    {"Crisp text", "Чіткий текст", "Чёткий текст"},
    {"The game's text, window frames and fills at the screen's real pixels when the game is enlarged.",
     "Текст, рамки й заливки вікон гри в справжніх точках екрана, коли гру збільшено.",
     "Текст, рамки и заливки окон игры в настоящих точках экрана, когда игра увеличена."},
    {"Vertical sync (VSync)", "Вертикальна синхронізація (VSync)", "Вертикальная синхронизация (VSync)"},
    {"Frames are shown in step with the monitor: no tearing.", "Кадри показуються в такт із монітором: без розривів.",
     "Кадры показываются в такт с монитором: без разрывов."},
    {"FPS limit", "Обмеження FPS", "Ограничение FPS"},
    {"Frames per second at most, 0-1000; 0 = no limit.", "Кадрів на секунду не більше, 0–1000; 0 — без обмеження.",
     "Кадров в секунду не больше, 0–1000; 0 — без ограничения."},
    {"Tearing for G-SYNC and FreeSync", "Розриви для G-SYNC і FreeSync", "Разрывы для G-SYNC и FreeSync"},
    {"With VSync off, G-SYNC and FreeSync monitors show each frame as soon as it is ready.",
     "Без VSync монітори G-SYNC і FreeSync показують кожен кадр, щойно він готовий.",
     "Без VSync мониторы G-SYNC и FreeSync показывают каждый кадр, как только он готов."},
    {"Check", "Перевірка", "Проверка"},
    {"Result", "Результат", "Результат"},
    {"&Fix", "&Виправити", "&Исправить"},
    {"Open the log", "Відкрити лог", "Открыть лог"},
    {"Open the folder", "Відкрити теку", "Открыть папку"},
    {"Report for the forum", "Звіт для форуму", "Отчёт для форума"},
    {"Check again", "Перевірити знову", "Проверить снова"},
    {"Detailed log", "Докладний лог", "Подробный лог"},
    {"The log writes more details: useful when something is wrong.",
     "Лог пише більше подробиць: потрібно, коли щось не так.",
     "Лог пишет больше подробностей: нужно, когда что-то не так."},
    {"The report is on the clipboard and in uaro-graphics-report.txt.",
     "Звіт скопійовано в буфер обміну й збережено в uaro-graphics-report.txt.",
     "Отчёт скопирован в буфер обмена и сохранён в uaro-graphics-report.txt."},
    {"Windows", "Windows", "Windows"},
    {"{1}.{2}, build {3}", "{1}.{2}, збірка {3}", "{1}.{2}, сборка {3}"},
    {" — our renderer needs Windows 10", " — нашому рендеру потрібна Windows 10", " — нашему рендеру нужна Windows 10"},
    {"Graphics card", "Відеокарта", "Видеокарта"},
    {"{1}, {2} MB, driver {3}", "{1}, {2} МБ, драйвер {3}", "{1}, {2} МБ, драйвер {3}"},
    {"Direct3D 12", "Direct3D 12", "Direct3D 12"},
    {"works", "працює", "работает"},
    {"does not start ({1})", "не запускається ({1})", "не запускается ({1})"},
    {"Monitor {1}", "Монітор {1}", "Монитор {1}"},
    {"{1}×{2}, {3} Hz, scaling {4} %, HDR: {5}", "{1}×{2}, {3} Гц, масштаб {4} %, HDR: {5}",
     "{1}×{2}, {3} Гц, масштаб {4} %, HDR: {5}"},
    {"Monitor {1}: work area", "Монітор {1}: робоча область", "Монитор {1}: рабочая область"},
    {"{1}×{2} without the task bar", "{1}×{2} без панелі завдань", "{1}×{2} без панели задач"},
    {"yes", "так", "да"},
    {"no", "ні", "нет"},
    {"ddraw.dll", "ddraw.dll", "ddraw.dll"},
    {"not uaRO Graphics: run install.bat", "не uaRO Graphics: запусти install.bat",
     "не uaRO Graphics: запусти install.bat"},
    {"missing: run install.bat", "немає: запусти install.bat", "нет: запусти install.bat"},
    {"in the dgvoodoo folder", "у теці dgvoodoo", "в папке dgvoodoo"},
    {"not in the dgvoodoo folder: dgVoodoo will not start", "немає в теці dgvoodoo: dgVoodoo не запуститься",
     "нет в папке dgvoodoo: dgVoodoo не запустится"},
    {"ROExt (reads {1})", "ROExt (читає {1})", "ROExt (читает {1})"},
    {"an unknown module", "невідомий модуль", "неизвестный модуль"},
    {" — off", " — вимкнено", " — отключён"},
    {"Input module", "Модуль вводу", "Модуль ввода"},
    {"none: neither ROExt nor uaRO Pad", "немає: ні ROExt, ні uaRO Pad", "нет: ни ROExt, ни uaRO Pad"},
    {"Game folder", "Тека гри", "Папка игры"},
    {"writable", "можна писати", "можно писать"},
    {"not writable: the settings cannot be saved", "не можна писати: налаштування не збережуться",
     "нельзя писать: настройки не сохранятся"},
    {"in Program Files: Windows redirects the game's files; better move the client, e.g. to C:\\Games",
     "у Program Files: Windows підміняє файли гри; краще перенести клієнт, наприклад у C:\\Games",
     "в Program Files: Windows подменяет файлы игры; лучше перенести клиент, например в C:\\Games"},
    {"Settings app", "Утиліта", "Утилита"},
    {"runs as administrator: start it normally", "запущена від імені адміністратора: запусти звичайно",
     "запущена от имени администратора: запусти обычно"},
    {"Game", "Гра", "Игра"},
    {"running", "запущена", "запущена"},
    {"not running", "не запущена", "не запущена"},
    {"Flag {1} ({2})", "Прапорець {1} ({2})", "Флажок {1} ({2})"},
    {"this user", "ваш користувач", "ваш пользователь"},
    {"all users", "усі користувачі", "все пользователи"},
    {"the game does not see its personal settings in the registry",
     "гра не бачить свої особисті налаштування в реєстрі", "игра не видит свои личные настройки в реестре"},
    {"an old Windows mode: in the way of Direct3D 12 and scaling", "режим старої Windows: заважає Direct3D 12 і масштабу",
     "режим старой Windows: мешает Direct3D 12 и масштабу"},
    {"scaling overridden: the picture is not at the real pixels", "масштаб перевизначено: картинка не в справжніх точках",
     "масштаб переопределён: картинка не в настоящих точках"},
    {"old colours or 640×480", "старі кольори або 640×480", "старые цвета или 640×480"},
    {"a Windows compatibility flag", "прапорець сумісності Windows", "флажок совместимости Windows"},
    {"; remove it in uaRO.exe's properties as administrator",
     "; знімається у властивостях uaRO.exe з правами адміністратора",
     "; снимается в свойствах uaRO.exe с правами администратора"},
    {"Game settings in the registry", "Налаштування гри в реєстрі", "Настройки игры в реестре"},
    {"personal copy: {1}, setup.exe's copy: {2}", "особиста копія: {1}, копія setup.exe: {2}",
     "личная копия: {1}, копия setup.exe: {2}"},
    {"none: Apply on the Screen page creates them", "немає: «Застосувати» на сторінці «Екран» створить їх",
     "нет: «Применить» на странице «Экран» создаст их"},
    {"the game uses {1}, setup.exe has {2}", "гра бере {1}, у setup.exe {2}", "игра берёт {1}, в setup.exe {2}"},
    {"Renderer last time", "Рендер минулого разу", "Рендер в прошлый раз"},
    {"Crisp text last time", "Чіткий текст минулого разу", "Чёткий текст в прошлый раз"},
    {"Errors in the log", "Помилки в лозі", "Ошибки в логе"},
    {"none", "немає", "нет"},
    {"{1}, the last: {2}", "{1}, остання: {2}", "{1}, последняя: {2}"},
    {"Frames per second", "Кадрів на секунду", "Кадров в секунду"},
    {"Log", "Лог", "Лог"},
    {"none yet: start the game", "ще немає: запусти гру", "ещё нет: запусти игру"},
    {"Fixed.", "Виправлено.", "Исправлено."},
    {"Presets", "Пресети", "Пресеты"},
    {"Maximum", "Максимум", "Максимум"},
    {"Balance", "Баланс", "Баланс"},
    {"Speed", "Швидкість", "Скорость"},
    {"No improvements", "Без покращень", "Без улучшений"},
    {"A preset fills in the Quality and Smoothing pages; Apply writes it. Now: {1}.",
     "Пресет заповнює сторінки «Якість» і «Згладжування»; записує «Застосувати». Зараз: {1}.",
     "Пресет заполняет страницы «Качество» и «Сглаживание»; записывает «Применить». Сейчас: {1}."},
    {"your own values", "свої значення", "свои значения"},
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
    {"Cannot start uaRO.exe (error {1}). The settings app must be in the uaRO client folder, next to uaRO.exe (or in "
     "its uaro-graphics folder).",
     "Не вдалося запустити uaRO.exe (помилка {1}). Утиліта має бути в теці клієнта uaRO, поруч з uaRO.exe (або в її "
     "теці uaro-graphics).",
     "Не удалось запустить uaRO.exe (ошибка {1}). Утилита должна быть в папке клиента uaRO, рядом с uaRO.exe (или в "
     "её папке uaro-graphics)."},
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
    {"Loading and the layout reset wait for the game to close: it rewrites its key when it exits.",
     "Завантаження й скидання розкладки — при закритій грі: вона перезаписує свій ключ при виході.",
     "Загрузка и сброс раскладки — при закрытой игре: она перезаписывает свой ключ при выходе."},
    {"{1} holds only a part of the game's key (a regedit export of one folder, say): loading it would replace all the "
     "rest. Such a file is added with a double-click (regedit).",
     "У {1} лише частина ключа гри (наприклад, вивантаження однієї теки з regedit): завантаження замінило б усе інше. "
     "Такий файл додають подвійним клацанням (regedit).",
     "В {1} только часть ключа игры (например, выгрузка одной папки из regedit): загрузка заменила бы всё остальное. "
     "Такой файл добавляют двойным щелчком (regedit)."},
    {"Smoothing", "Згладжування", "Сглаживание"},
    {"Edge smoothing (MSAA)", "Згладжування країв (MSAA)", "Сглаживание краёв (MSAA)"},
    {"Smooths the jagged edges of models, walls and the ground. 4× is a good balance; 8× needs more video memory.",
     "Згладжує зубчасті краї моделей, стін і землі. 4× — добрий баланс; 8× потребує більше відеопам'яті.",
     "Сглаживает зубчатые края моделей, стен и земли. 4× — хороший баланс; 8× требует больше видеопамяти."},
    {"Smooth edges of leaves and fences", "Рівні краї листя й огорож", "Ровные края листвы и оград"},
    {"With MSAA: the cut-out parts of models get soft edges instead of jagged ones (alpha to coverage).",
     "Разом з MSAA: вирізані частини моделей отримують м'які краї замість зубчастих (alpha to coverage).",
     "Вместе с MSAA: у вырезанных частей моделей края мягкие, а не зубчатые (alpha to coverage)."},
    {"Anisotropic filtering", "Анізотропна фільтрація", "Анизотропная фильтрация"},
    {"Keeps the ground and walls sharp at a slant and into the distance. Best with the game's mipmaps on the Quality "
     "page.",
     "Земля й стіни лишаються чіткими під кутом і вдалині. Найкраще разом з міп-рівнями гри в «Якості».",
     "Земля и стены остаются чёткими под углом и вдали. Лучше всего вместе с мип-уровнями игры в «Качестве»."},
    {"Off", "Вимкнено", "Выключено"},
    {"2×", "2×", "2×"},
    {"4×", "4×", "4×"},
    {"8×", "8×", "8×"},
    {"16×", "16×", "16×"},
    {"64-bit renderer (test, for DLSS)", "64-бітний рендер (перевірка, для DLSS)",
     "64-битный рендер (проверка, для DLSS)"},
    {"Draws in a separate 64-bit process, uaro-render64.exe: DLSS and frame generation need it. The picture should "
     "look the same.",
     "Малює в окремому 64-бітному процесі uaro-render64.exe: він потрібен для DLSS і генерації кадрів. Картинка має "
     "бути та сама.",
     "Рисует в отдельном 64-битном процессе uaro-render64.exe: он нужен для DLSS и генерации кадров. Картинка "
     "должна быть та же."},
    {"NVIDIA DLSS", "NVIDIA DLSS", "NVIDIA DLSS"},
    {"A technology of NVIDIA for GeForce RTX cards, in the 64-bit renderer (it starts by itself). DLAA smooths at "
     "full resolution; Quality ... Ultra draw the world smaller and rebuild it. Instead of MSAA.",
     "Технологія NVIDIA для карт GeForce RTX, у 64-бітному рендері (вмикається сам). DLAA згладжує в повній "
     "роздільності; «Якість» … «Ультра» малюють світ меншим і відновлюють його. Замість MSAA.",
     "Технология NVIDIA для карт GeForce RTX, в 64-битном рендере (включается сам). DLAA сглаживает в полном "
     "разрешении; «Качество» … «Ультра» рисуют мир меньше и восстанавливают его. Вместо MSAA."},
    {"DLAA (smoothing only)", "DLAA (лише згладжування)", "DLAA (только сглаживание)"},
    {"Quality", "Якість", "Качество"},
    {"Balanced", "Баланс", "Баланс"},
    {"Performance", "Продуктивність", "Производительность"},
    {"Ultra performance", "Ультрапродуктивність", "Ультрапроизводительность"},
    {"NVIDIA DLSS frame generation", "Генерація кадрів NVIDIA DLSS", "Генерация кадров NVIDIA DLSS"},
    {"Adds made-up frames between the game's: GeForce RTX 40 and 50 (3x and 4x: RTX 50), only with DLSS on the "
     "Smoothing page. Smoother movement, a little more delay; off in menus by itself.",
     "Додає вигадані кадри між кадрами гри: GeForce RTX 40 і 50 (3x і 4x — RTX 50), лише з DLSS у «Згладжуванні». "
     "Плавніший рух, трохи більша затримка; у меню вимикається сама.",
     "Добавляет придуманные кадры между кадрами игры: GeForce RTX 40 и 50 (3x и 4x — RTX 50), только с DLSS в "
     "«Сглаживании». Плавнее движение, чуть больше задержка; в меню выключается сама."},
    {"3×", "3×", "3×"},
    {"Improve textures with a neural network (experiment)", "Покращувати текстури нейромережею (експеримент)",
     "Улучшать текстуры нейросетью (эксперимент)"},
    {"The game's textures redrawn larger and sharper by a neural network (Real-ESRGAN): the ready pictures of the "
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
    {"GPU time a frame, ms", "Час відеокарти на кадр, мс", "Время видеокарты на кадр, мс"},
    {"How much of each frame the neural network may take, 1-20 ms: more makes textures faster, less keeps the "
     "frame rate.",
     "Скільки часу кожного кадру може брати нейромережа, 1–20 мс: більше — текстури швидше, менше — FPS вищий.",
     "Сколько времени каждого кадра может брать нейросеть, 1–20 мс: больше — текстуры быстрее, меньше — FPS выше."},
    {"Grain smoothing", "Згладжування зерна", "Сглаживание зерна"},
    {"How much of the textures' grain the realistic model smooths away: 0 keeps all of it (sand and grass stay "
     "crisp), 100 makes them smooth as if painted. Walls and floors look alike at any value. Textures made at "
     "another value are made again.",
     "Скільки зерна текстур згладжує реалістична модель: 0 — лишає все (пісок і трава чіткі), 100 — гладкі, наче "
     "намальовані. Стіни й підлога майже однакові за будь-якого значення. Текстури, зроблені з іншим значенням, "
     "робляться знову.",
     "Сколько зерна текстур сглаживает реалистичная модель: 0 — оставляет всё (песок и трава чёткие), 100 — "
     "гладкие, будто нарисованные. Стены и пол почти одинаковы при любом значении. Текстуры, сделанные с другим "
     "значением, делаются заново."},
    {"RTX Remix (experiment)", "RTX Remix (експеримент)", "RTX Remix (эксперимент)"},
    {"Draws through Direct3D 9 for NVIDIA RTX Remix: path tracing, and DLSS 5 once NVIDIA adds it to Remix "
     "(announced for 23 October 2026). Needs RTX Remix's d3d9.dll and .trex folder next to uaRO.exe and rtx.conf "
     "from uaro-graphics\\remix copied there; Alt+X opens Remix's menu. Without them the game draws with "
     "Direct3D 12 as usual.",
     "Малює через Direct3D 9 для NVIDIA RTX Remix: трасування шляхів, а DLSS 5 — коли NVIDIA додасть його в Remix "
     "(обіцяно 23 жовтня 2026). Потрібні d3d9.dll і тека .trex з RTX Remix поруч з uaRO.exe і туди ж rtx.conf з "
     "uaro-graphics\\remix; Alt+X відкриває меню Remix. Без них гра малює через Direct3D 12, як завжди.",
     "Рисует через Direct3D 9 для NVIDIA RTX Remix: трассировка путей, а DLSS 5 — когда NVIDIA добавит его в Remix "
     "(обещано 23 октября 2026). Нужны d3d9.dll и папка .trex из RTX Remix рядом с uaRO.exe и туда же rtx.conf "
     "из uaro-graphics\\remix; Alt+X открывает меню Remix. Без них игра рисует через Direct3D 12, как обычно."},
    {"AI textures", "Нейромережа", "Нейросеть"},
    {"Ready textures", "Готові текстури", "Готовые текстуры"},
    {"Set", "Набір", "Набор"},
    {"Recommended works on the ground and buildings: the safest choice. Everything takes characters and effects "
     "too. Change any setting below and the set becomes Custom.",
     "«Рекомендований» покращує землю й будівлі — найнадійніший вибір. «Усе» бере ще персонажів та ефекти. "
     "Змініть будь-що нижче — і набір стане «Свій».",
     "«Рекомендуемый» улучшает землю и здания — самый надёжный выбор. «Всё» берёт ещё персонажей и эффекты. "
     "Измените что-нибудь ниже — и набор станет «Свой»."},
    {"Recommended: ground and buildings", "Рекомендований: земля й будівлі", "Рекомендуемый: земля и здания"},
    {"Everything: characters and effects too", "Усе: і персонажі, і ефекти", "Всё: и персонажи, и эффекты"},
    {"Custom", "Свій", "Свой"},
    {"What to improve:", "Що покращувати:", "Что улучшать:"},
    {"Map ground", "Земля карт", "Земля карт"},
    {"Sand, grass, paving, roads: everything you walk on.", "Пісок, трава, плити, дороги: усе, чим ходять.",
     "Песок, трава, плиты, дороги: всё, по чему ходят."},
    {"Buildings, trees and water", "Будівлі, дерева й вода", "Здания, деревья и вода"},
    {"Everything else that stands on a map: houses, walls, trees, rocks, water.",
     "Усе інше, що стоїть на карті: будинки, стіни, дерева, каміння, вода.",
     "Всё остальное, что стоит на карте: дома, стены, деревья, камни, вода."},
    {"Monsters", "Монстри", "Монстры"},
    {"Monsters, pets, homunculi and mercenaries: their animation frames from the texture pack, one by one as the game "
     "draws them. Small pixel pictures: the network may change their look - try it and see.",
     "Монстри, вихованці, гомункули й найманці: кадри їхніх анімацій із пакета текстур, по одному, щойно гра їх "
     "малює. Це маленькі піксельні картинки: нейромережа може змінити їхній вигляд — спробуйте й подивіться.",
     "Монстры, питомцы, гомункулы и наёмники: кадры их анимаций из пакета текстур, по одному, как только игра их "
     "рисует. Это маленькие пиксельные картинки: нейросеть может изменить их вид — попробуйте и посмотрите."},
    {"Skill effects", "Ефекти вмінь", "Эффекты умений"},
    {"Glows, sparks and light added onto the picture. Usually small and quick: the difference is little.",
     "Сяйва, іскри й світло, що додаються до картинки. Зазвичай маленькі й швидкі: різниця невелика.",
     "Свечения, искры и свет, которые добавляются к картинке. Обычно маленькие и быстрые: разница небольшая."},
    {"Model for maps", "Модель для карт", "Модель для карт"},
    {"For the ground and buildings. Realistic keeps the texture's own detail; Drawn makes it smooth and clean like a "
     "painting and is twice as fast.",
     "Для землі й будівель. «Реалістична» зберігає власні деталі текстури; «Мальована» робить її гладкою й чистою, "
     "як малюнок, і вдвічі швидша.",
     "Для земли и зданий. «Реалистичная» сохраняет собственные детали текстуры; «Рисованная» делает её гладкой и "
     "чистой, как рисунок, и вдвое быстрее."},
    {"Model for characters", "Модель для персонажів", "Модель для персонажей"},
    {"For characters, monsters, NPCs, items, effects and icons. Drawn (made for anime) keeps clean lines and flat "
     "colours.",
     "Для персонажів, монстрів, NPC, речей, ефектів та іконок. «Мальована» (зроблена для аніме) зберігає чисті лінії "
     "й рівні кольори.",
     "Для персонажей, монстров, NPC, вещей, эффектов и иконок. «Рисованная» (сделана для аниме) сохраняет чистые "
     "линии и ровные цвета."},
    {"Realistic (general-x4v3)", "Реалістична (general-x4v3)", "Реалистичная (general-x4v3)"},
    {"Drawn (animevideov3)", "Мальована (animevideov3)", "Рисованная (animevideov3)"},
    {"Result size", "Розмір результату", "Размер результата"},
    {"×4: a 256×256 texture becomes 1024×1024, about 5.6 MB of video memory with its smaller copies. ×2: 512×512, "
     "about 1.4 MB - for cards with 4 GB or less (the pack's pictures are taken at half their size).",
     "×4: текстура 256×256 стає 1024×1024 — близько 5,6 МБ відеопам'яті з меншими копіями. ×2: 512×512, близько 1,4 "
     "МБ — для відеокарт із 4 ГБ і менше (картинки пакета беруться вдвічі меншими).",
     "×4: текстура 256×256 становится 1024×1024 — около 5,6 МБ видеопамяти с уменьшенными копиями. ×2: 512×512, около "
     "1,4 МБ — для видеокарт с 4 ГБ и меньше (картинки пакета берутся вдвое меньшими)."},
    {"×4 - the sharpest", "×4 — найчіткіші", "×4 — самые чёткие"},
    {"×2 - a quarter of the memory", "×2 — учетверо менше пам'яті", "×2 — вчетверо меньше памяти"},
    {"Texture sides, from - to", "Сторони текстур, від — до", "Стороны текстур, от — до"},
    {"Which textures of the world it takes, by their sides in pixels (16-1024). From 16: shadows, the frame under "
     "the cursor, the small things of a map too. Over 512 needs much video memory: the network takes about 0.5 GB "
     "for 1024×1024.",
     "Які текстури світу брати — за їхніми сторонами в пікселях (16–1024). Від 16 — також тіні, рамка під курсором, "
     "дрібниці карти. Понад 512 потребує багато відеопам'яті: нейромережі треба близько 0,5 ГБ на 1024×1024.",
     "Какие текстуры мира брать — по их сторонам в пикселях (16–1024). От 16 — также тени, рамка под курсором, "
     "мелочи карты. Больше 512 требует много видеопамяти: нейросети нужно около 0,5 ГБ на 1024×1024."},
    {"Prepare a map's known textures in advance", "Заздалегідь готувати знайомі текстури карти",
     "Заранее готовить знакомые текстуры карты"},
    {"A map's textures the texture pack knows are taken from it as soon as the map loads, not when they come into "
     "view: fewer of them sharpen before your eyes.",
     "Текстури карти, які знає пакет текстур, беруться з нього щойно карта завантажилась, а не коли потраплять у "
     "кадр: менше «проявлення» на очах.",
     "Текстуры карты, которые знает пакет текстур, берутся из него сразу после загрузки карты, а не когда попадут в "
     "кадр: меньше «проявления» на глазах."},
    {"Tick the box above and the network starts improving textures while you play. The Ready textures page shows "
     "and deletes what it has made.",
     "Поставте галочку вище — і нейромережа почне покращувати текстури, поки ви граєте. Сторінка «Готові текстури» "
     "показує й видаляє зроблене.",
     "Поставьте галочку выше — и нейросеть начнёт улучшать текстуры, пока вы играете. Страница «Готовые текстуры» "
     "показывает и удаляет сделанное."},
    {"The texture pack uaro-textures.zip: {1} pictures, {2} (the file: {3}).",
     "Пакет текстур uaro-textures.zip: картинок — {1}, {2} (файл: {3}).",
     "Пакет текстур uaro-textures.zip: картинок — {1}, {2} (файл: {3})."},
    {"There is no texture pack (uaro-textures.zip) yet: the Texture pack page makes it.",
     "Пакета текстур (uaro-textures.zip) ще немає: його створює вкладка «Пакет текстур».",
     "Пакета текстур (uaro-textures.zip) пока нет: его создаёт вкладка «Пакет текстур»."},
    {"uaro-textures.zip cannot be read: {1}",
     "uaro-textures.zip не читається: {1}",
     "uaro-textures.zip не читается: {1}"},
    {"Map", "Карта", "Карта"},
    {"Ground", "Земля", "Земля"},
    {"Buildings", "Будівлі", "Здания"},
    {"Characters", "Персонажі", "Персонажи"},
    {"Effects", "Ефекти", "Эффекты"},
    {"Total", "Усього", "Всего"},
    {"Size", "Розмір", "Размер"},
    {"All maps", "Усі карти", "Все карты"},
    {"no map (the builder's; icons)",
     "без карти (від збирача; іконки)",
     "без карты (от сборщика; иконки)"},
    {"Delete from the ticked maps:", "Видаляти з позначених карт:", "Удалять с отмеченных карт:"},
    {"Delete ticked", "Видалити позначене", "Удалить отмеченное"},
    {"Delete the old upscaled.grf ({1})",
     "Видалити старий upscaled.grf ({1})",
     "Удалить старый upscaled.grf ({1})"},
    {"Delete all", "Видалити все", "Удалить всё"},
    {"The pictures the pack's builder made are of no map; the maps are noted when the game makes pictures itself "
     "(making new ones while playing). Deleted pictures are made again by the Texture pack page. upscaled.grf "
     "(0.11-0.47) is not used any more.",
     "Картинки, які створив збирач пакета, без карти; карти записуються, коли картинки створює сама гра (створення "
     "нових під час гри). Видалені картинки знову створює вкладка «Пакет текстур». upscaled.grf (0.11-0.47) більше не "
     "використовується.",
     "Картинки, которые создал сборщик пакета, без карты; карты записываются, когда картинки создаёт сама игра "
     "(создание новых во время игры). Удалённые картинки снова создаёт вкладка «Пакет текстур». upscaled.grf "
     "(0.11-0.47) больше не используется."},
    {"The game or the pack's builder is running: pictures can be deleted once it is closed (it keeps the pack open).",
     "Гра або збирач пакета запущені: видаляти можна, коли вони закриті (вони тримають пакет відкритим).",
     "Игра или сборщик пакета запущены: удалять можно, когда они закрыты (они держат пакет открытым)."},
    {"Delete the ticked pictures from the texture pack ({1})? The Texture pack page makes them again.",
     "Видалити позначені картинки з пакета текстур ({1} шт.)? Вкладка «Пакет текстур» створить їх знову.",
     "Удалить отмеченные картинки из пакета текстур ({1} шт.)? Вкладка «Пакет текстур» создаст их снова."},
    {"Delete the whole texture pack ({1})? The Texture pack page makes it again.",
     "Видалити весь пакет текстур ({1})? Вкладка «Пакет текстур» створить його знову.",
     "Удалить весь пакет текстур ({1})? Вкладка «Пакет текстур» создаст его снова."},
    {"Deleted pictures: {1}. The texture pack is {2} now.",
     "Видалено картинок: {1}. Пакет текстур тепер {2}.",
     "Удалено картинок: {1}. Пакет текстур теперь {2}."},
    {"The texture pack is deleted.",
     "Пакет текстур видалено.",
     "Пакет текстур удалён."},
    {"Nothing to delete: tick maps in the table first.", "Нічого видаляти: спершу позначте карти в таблиці.",
     "Нечего удалять: сначала отметьте карты в таблице."},
    {"The texture pack could not be changed: {1}",
     "Не вдалося змінити пакет текстур: {1}",
     "Не удалось изменить пакет текстур: {1}"},
    {"{1} MB", "{1} МБ", "{1} МБ"},
    {"{1} GB", "{1} ГБ", "{1} ГБ"},
    {"Interface: icons, pictures, loading screen", "Інтерфейс: іконки, картинки, заставка",
     "Интерфейс: иконки, картинки, заставка"},
    {"Items and skills in the windows, an item's picture in its description, cards' and NPCs' illustrations: smooth "
     "at the window's real pixels instead of in squares (needs crisp text, Quality page). The loading screen and the "
     "login's backgrounds too. The model for characters draws them; the mouse cursor comes with the characters' box.",
     "Речі й уміння у вікнах, картинка речі в її описі, ілюстрації карт і NPC: гладко в реальних пікселях вікна, а "
     "не квадратами (потрібен чіткий текст, сторінка «Якість»). А також заставка завантаження й тло входу. Їх малює "
     "модель для персонажів; курсор миші — разом із галочкою персонажів.",
     "Вещи и умения в окнах, картинка вещи в её описании, иллюстрации карт и NPC: гладко в настоящих пикселях окна, "
     "а не квадратами (нужен чёткий текст, страница «Качество»). А также заставка загрузки и фон входа. Их рисует "
     "модель для персонажей; курсор мыши — вместе с галочкой персонажей."},
    {"Interface", "Інтерфейс", "Интерфейс"},
    {"Window frames and buttons", "Рамки й кнопки вікон", "Рамки и кнопки окон"},
    {"With the interface's box: the windows' small pieces too - buttons, tabs, slots, arrows. The words on buttons are "
     "pictures and get smoothed too: should they look blurred, untick this and keep the icons.",
     "Разом із галочкою інтерфейсу: дрібні частини вікон — кнопки, вкладки, комірки, стрілки. Написи на кнопках — це "
     "картинки, вони теж згладжуються: якщо здадуться розмитими, зніміть цю галочку, іконки лишаться.",
     "Вместе с галочкой интерфейса: мелкие части окон — кнопки, вкладки, ячейки, стрелки. Надписи на кнопках — это "
     "картинки, они тоже сглаживаются: если покажутся размытыми, снимите эту галочку, иконки останутся."},
    {"Players", "Гравці", "Игроки"},
    {"Characters of players, your own too: bodies, heads, headgear, weapons, shields, garments - every frame by "
     "itself.",
     "Персонажі гравців, ваш теж: тіла, голови, головні убори, зброя, щити, плащі — кожен кадр окремо.",
     "Персонажи игроков, ваш тоже: тела, головы, головные уборы, оружие, щиты, плащи — каждый кадр отдельно."},
    {"NPCs", "NPC", "NPC"},
    {"The game's characters in towns and dungeons: Kafra, merchants, guards.",
     "Персонажі гри в містах і підземеллях: Кафра, торговці, вартові.",
     "Персонажи игры в городах и подземельях: Кафра, торговцы, стражники."},
    {"Items on the ground", "Речі на землі", "Вещи на земле"},
    {"What lies on the ground: drops, loot.", "Те, що лежить на землі: здобич, луп.",
     "То, что лежит на земле: добыча, лут."},
    {"Effects", "Ефекти", "Эффекты"},
    {"Modern effects over the game's own picture. They work with the uaRO Graphics renderer (not with dgVoodoo or "
     "RTX Remix) and apply at once, in a running game too. Pause or Scroll Lock in the game compares with the "
     "original picture.",
     "Сучасні ефекти поверх картинки самої гри. Працюють із рендером uaRO Graphics (не з dgVoodoo і не з RTX Remix) "
     "і діють одразу, у запущеній грі теж. Pause або Scroll Lock у грі порівнює з оригінальною картинкою.",
     "Современные эффекты поверх картинки самой игры. Работают с рендером uaRO Graphics (не с dgVoodoo и не с RTX "
     "Remix) и действуют сразу, в запущенной игре тоже. Pause или Scroll Lock в игре сравнивает с исходной "
     "картинкой."},
    {"Glow of effects", "Світіння ефектів", "Свечение эффектов"},
    {"Fire, lightning, the light of skills, portals and lamps shine with a soft halo, as in modern games. The rest "
     "of the picture keeps its brightness.",
     "Вогонь, блискавки, світло вмінь, портали й ліхтарі світяться м'яким ореолом, як у сучасних іграх. Решта "
     "картинки не змінює яскравості.",
     "Огонь, молнии, свет умений, порталы и фонари светятся мягким ореолом, как в современных играх. Остальная "
     "картинка не меняет яркости."},
    {"Glow brightness", "Яскравість світіння", "Яркость свечения"},
    {"How bright the halo is: 100 % as intended, 200 % twice as bright, 0 % none.",
     "Наскільки яскравий ореол: 100 % — як задумано, 200 % — удвічі яскравіше, 0 % — без нього.",
     "Насколько ярок ореол: 100 % — как задумано, 200 % — вдвое ярче, 0 % — без него."},
    {"Halo size", "Розмір ореолу", "Размер ореола"},
    {"How far the glow spreads around an effect: less is a tight glow, more a wide haze.",
     "Як далеко світіння розходиться навколо ефекту: менше — щільне сяйво, більше — широкий серпанок.",
     "Как далеко свечение расходится вокруг эффекта: меньше — плотное сияние, больше — широкая дымка."},
    {"Modern water", "Сучасна вода", "Современная вода"},
    {"The maps' water drawn anew: waves, reflections of the shore, trees and characters, the sun's glints, the bed "
     "seen through the water and darker where it is deep, foam along the shore. Lava stays as it is.",
     "Вода на картах малюється заново: хвилі, відображення берега, дерев і персонажів, відблиски сонця, дно крізь "
     "воду, темніше там, де глибоко, піна вздовж берега. Лава лишається як є.",
     "Вода на картах рисуется заново: волны, отражения берега, деревьев и персонажей, блики солнца, дно сквозь воду, "
     "темнее там, где глубоко, пена вдоль берега. Лава остаётся как есть."},
    {"Waves", "Хвилі", "Волны"},
    {"How rough the water is: 0 % a still mirror, 100 % wind-driven ripples.",
     "Наскільки неспокійна вода: 0 % — гладке дзеркало, 100 % — брижі від вітру.",
     "Насколько неспокойна вода: 0 % — гладкое зеркало, 100 % — рябь от ветра."},
    {"Reflections", "Відображення", "Отражения"},
    {"How much the water reflects the sky and what stands at its edge.",
     "Наскільки вода відображає небо і те, що стоїть на її березі.",
     "Насколько вода отражает небо и то, что стоит у её берега."},
    {"Clarity", "Прозорість", "Прозрачность"},
    {"How deep one sees into the water: less is murky, more is clear to the bottom.",
     "Як глибоко видно у воду: менше — каламутна, більше — прозора до дна.",
     "Как глубоко видно в воду: меньше — мутная, больше — прозрачная до дна."},
    {"Light on the surroundings", "Світло на оточення", "Свет на окружение"},
    {"How much the light of effects falls on the ground, walls and characters near them, in its own colour: a "
     "fireball lights the street. 0 % is the halo alone.",
     "Наскільки світло ефектів падає на землю, стіни й персонажів поруч, у своєму кольорі: вогняна куля освітлює "
     "вулицю. 0 % — лише ореол.",
     "Насколько свет эффектов падает на землю, стены и персонажей рядом, в своём цвете: огненный шар освещает "
     "улицу. 0 % — только ореол."},
    {"Soft shadows", "М'які тіні", "Мягкие тени"},
    {"Soft shade where surfaces meet: at the foot of walls, in the corners of buildings, under trees and rocks, as in "
     "modern games (ambient occlusion). The characters and the effects keep their light.",
     "М'яка тінь там, де поверхні сходяться: біля стін, у кутах будівель, під деревами й каменями, як у сучасних "
     "іграх (ambient occlusion). Персонажі й ефекти не затіняються.",
     "Мягкая тень там, где поверхности сходятся: у стен, в углах зданий, под деревьями и камнями, как в современных "
     "играх (ambient occlusion). Персонажи и эффекты не затеняются."},
    {"Shadow strength", "Сила тіней", "Сила теней"},
    {"How dark the shade is in the deepest corners.", "Наскільки темна тінь у найглибших кутах.",
     "Насколько темна тень в самых глубоких углах."},
    {"Shadow size", "Розмір тіней", "Размер теней"},
    {"How far the shade reaches from a corner: less hugs the edges, more spreads under trees and between buildings.",
     "Як далеко тінь сягає від кута: менше — тонко вздовж країв, більше — широко під деревами й між будівлями.",
     "Как далеко тень заходит от угла: меньше — тонко вдоль краёв, больше — широко под деревьями и между "
     "зданиями."},
    {"Colour", "Колір", "Цветокоррекция"},
    {"The picture's colours to taste: richer or calmer, more contrast, brighter, warmer or colder, darker corners. "
     "The interface keeps its colours.",
     "Кольори картинки на смак: соковитіші чи спокійніші, контрастніші, яскравіші, тепліші чи холодніші, темніші "
     "кути. Інтерфейс не змінюється.",
     "Цвета картинки по вкусу: сочнее или спокойнее, контрастнее, ярче, теплее или холоднее, темнее по углам. "
     "Интерфейс не меняется."},
    {"Saturation", "Насиченість", "Насыщенность"},
    {"100 % as the game, more is richer colour, less paler, 0 % black and white.",
     "100 % — як у грі, більше — соковитіші кольори, менше — блідіші, 0 % — чорно-біла.",
     "100 % — как в игре, больше — сочнее цвета, меньше — бледнее, 0 % — чёрно-белая."},
    {"Contrast", "Контраст", "Контраст"},
    {"100 % as the game; more deepens the shadows and lifts the light parts, without losing them.",
     "100 % — як у грі; більше — глибші тіні й світліші світлі місця, без втрати деталей.",
     "100 % — как в игре; больше — глубже тени и светлее светлые места, без потери деталей."},
    {"Brightness", "Яскравість", "Яркость"},
    {"100 % as the game; more lifts the middle tones, the brightest stay as they are.",
     "100 % — як у грі; більше — світліші середні тони, найсвітліші лишаються як є.",
     "100 % — как в игре; больше — светлее средние тона, самые светлые остаются как есть."},
    {"Warmth", "Тон", "Тон"},
    {"50 % as the game; less is colder (bluer), more warmer (yellower).",
     "50 % — як у грі; менше — холодніше (синіше), більше — тепліше (жовтіше).",
     "50 % — как в игре; меньше — холоднее (синее), больше — теплее (желтее)."},
    {"Vignette", "Віньєтка", "Виньетка"},
    {"The corners of the screen darker, as in films; 0 % none.",
     "Кути екрана темніші, як у кіно; 0 % — без неї.", "Углы экрана темнее, как в кино; 0 % — без неё."},
    {"Depth of field", "Глибина різкості", "Глубина резкости"},
    {"Sharp around your character in the middle of the screen; what is farther or nearer gets softer, as if the "
     "map were a model (tilt-shift). The interface stays sharp.",
     "Різко довкола вашого персонажа посеред екрана; дальше й ближче — м'якше, ніби карта — макет (tilt-shift). "
     "Інтерфейс лишається різким.",
     "Резко вокруг вашего персонажа в центре экрана; дальше и ближе — мягче, будто карта — макет (tilt-shift). "
     "Интерфейс остаётся резким."},
    {"Blur", "Розмиття", "Размытие"},
    {"How soft the far and the near get.", "Наскільки м'якими стають дальнє й ближнє.",
     "Насколько мягкими становятся дальнее и ближнее."},
    {"Sharp zone", "Зона різкості", "Зона резкости"},
    {"How deep the sharp zone around the character is: less blurs sooner, more keeps more sharp.",
     "Наскільки глибока різка зона довкола персонажа: менше — розмиття починається раніше, більше — різкого більше.",
     "Насколько глубока резкая зона вокруг персонажа: меньше — размытие начинается раньше, больше — резкого "
     "больше."},
    {"Smooth map light", "М'яке світло карти", "Мягкий свет карты"},
    {"The map's baked light - the pools of torches and lamps, the shadows of houses and trees on the ground - smooth "
     "instead of blotches with steps and dots: the game keeps it in a very coarse grid that shows its cells when "
     "enlarged.",
     "Запечене світло карти — плями від смолоскипів і ліхтарів, тіні будинків і дерев на землі — плавне замість "
     "плям зі сходинками й крапками: гра тримає його в дуже грубій сітці, яка при збільшенні показує свої клітинки.",
     "Запечённый свет карты — пятна от факелов и фонарей, тени домов и деревьев на земле — плавный вместо пятен со "
     "ступеньками и точками: игра хранит его в очень грубой сетке, которая при увеличении показывает свои клетки."},
    {"Live torches", "Живі смолоскипи", "Живые факелы"},
    {"The maps' torches drawn anew: a flame that burns, sways and throws sparks instead of the game's blurred drop, a "
     "soft glow in the air around it instead of the red blotch, and a warm flickering light on the ground and the "
     "walls.",
     "Смолоскипи на картах малюються наново: полум'я, що горить, гойдається й сипле іскрами, замість розмитої краплі "
     "гри; м'яке сяйво в повітрі довкола замість червоної плями; тепле мерехтливе світло на землі й стінах.",
     "Факелы на картах рисуются заново: пламя, которое горит, колышется и сыплет искрами, вместо размытой капли "
     "игры; мягкое свечение в воздухе вокруг вместо красной кляксы; тёплый мерцающий свет на земле и стенах."},
    {"Glow around the fire", "Сяйво довкола вогню", "Свечение вокруг огня"},
    {"How bright the soft glow in the air around the flame is; 0 % none.",
     "Наскільки яскраве м'яке сяйво в повітрі довкола полум'я; 0 % — без нього.",
     "Насколько яркое мягкое свечение в воздухе вокруг пламени; 0 % — без него."},
    {"Light around", "Світло довкола", "Свет вокруг"},
    {"How much warm light the torch throws on the ground, the walls and whoever stands near; 0 % none.",
     "Скільки теплого світла смолоскип кидає на землю, стіни й тих, хто поруч; 0 % — без нього.",
     "Сколько тёплого света факел бросает на землю, стены и тех, кто рядом; 0 % — без него."},
    {"Flicker", "Мерехтіння", "Мерцание"},
    {"How much the fire and its light flicker; 0 % burns evenly.",
     "Наскільки мерехтять вогонь і його світло; 0 % — горить рівно.",
     "Насколько мерцают огонь и его свет; 0 % — горит ровно."},
    {"Map objects", "Об'єкти карти", "Объекты карты"},
    {"Things on the maps the game draws as small pictures blown up many times: the driver draws them anew, each "
     "with its own box. They apply in a running game; Pause shows the game's own.",
     "Речі на картах, які гра малює маленькими картинками, розтягнутими в багато разів: драйвер малює їх наново, "
     "кожну — своєю галочкою. Діють у запущеній грі; Pause показує ігрові.",
     "Вещи на картах, которые игра рисует маленькими картинками, растянутыми во много раз: драйвер рисует их "
     "заново, каждую — своей галочкой. Действуют в запущенной игре; Pause показывает игровые."},
    {"New flame", "Новий вогонь", "Новый огонь"},
    {"A flame that burns, sways and throws sparks instead of the game's blurred picture. Off: the game's own flame, "
     "with the new glow and light around it.",
     "Полум'я, що горить, гойдається й сипле іскрами, замість розмитої картинки гри. Вимкнено: вогонь гри, а сяйво "
     "й світло довкола — нові.",
     "Пламя, которое горит, колышется и сыплет искрами, вместо размытой картинки игры. Выключено: огонь игры, а "
     "свечение и свет вокруг — новые."},
    {"Flame brightness", "Яскравість вогню", "Яркость огня"},
    {"How bright and dense the new flame burns: less is softer and more see-through, more is hotter.",
     "Наскільки яскраво й щільно горить новий вогонь: менше — м'якше й прозоріше, більше — гарячіше.",
     "Насколько ярко и плотно горит новый огонь: меньше — мягче и прозрачнее, больше — жарче."},
    {"Chimney smoke", "Дим із труб", "Дым из труб"},
    {"Each puff the chimneys send up drawn as billowing smoke - wisps and holes that change as it rises - instead of "
     "the same small picture blown up.",
     "Кожен клуб диму з труб малюється клубчастим димом — пасма й просвіти, що змінюються, поки він піднімається, — "
     "замість тієї самої маленької розтягнутої картинки.",
     "Каждый клуб дыма из труб рисуется клубящимся дымом — пряди и просветы, которые меняются, пока он поднимается, "
     "— вместо одной и той же маленькой растянутой картинки."},
    {"Smoke density", "Густота диму", "Густота дыма"},
    {"How thick the smoke is: less is thin and see-through, more is dense.",
     "Наскільки густий дим: менше — тонкий і прозорий, більше — щільний.",
     "Насколько густой дым: меньше — тонкий и прозрачный, больше — плотный."},
    {"Lamps' light", "Світло ламп", "Свет ламп"},
    {"The lights the maps' makers placed - lamps, braziers, crystals - light the characters and the walls near them "
     "too, not only the ground, and the fire-coloured ones flicker. Read from the map files; the first visit to a map "
     "with torches teaches the driver how they lie.",
     "Світло, яке розставили автори карт, — лампи, жаровні, кристали — освітлює й персонажів та стіни поруч, а не "
     "лише землю, а вогняне мерехтить. Береться з файлів карт; перший візит на карту зі смолоскипами вчить драйвер, "
     "як вони лягають.",
     "Свет, который расставили авторы карт, — лампы, жаровни, кристаллы — освещает и персонажей и стены рядом, а не "
     "только землю, а огненный мерцает. Берётся из файлов карт; первый заход на карту с факелами учит драйвер, как "
     "они ложатся."},
    {"Lamps' brightness", "Яскравість ламп", "Яркость ламп"},
    {"How much light the lamps add: less is a hint, more is bright.",
     "Скільки світла додають лампи: менше — ледь помітно, більше — яскраво.",
     "Сколько света добавляют лампы: меньше — едва заметно, больше — ярко."},
    {"Fire of skills lights around", "Вогонь умінь світить довкола", "Огонь умений светит вокруг"},
    {"Fire walls, fire bolts, meteors and other fire-coloured effects light the ground, the walls and the characters "
     "around them a little, as a torch does - with the depth of the scene, never washing it out.",
     "Вогняні стіни, вогняні стріли, метеори та інші вогняні ефекти трохи освітлюють землю, стіни й персонажів "
     "довкола, як смолоскип, — з глибиною сцени, без засвічення.",
     "Огненные стены, огненные стрелы, метеоры и другие огненные эффекты немного освещают землю, стены и персонажей "
     "вокруг, как факел, — с глубиной сцены, без пересвета."},
    {"Fire light", "Світло вогню", "Свет огня"},
    {"How much the fire of skills lights around it.", "Наскільки вогонь умінь освітлює довкола.",
     "Насколько огонь умений освещает всё вокруг."},
    {"Softer level 99 aura", "М'якша аура 99 рівня", "Мягкая аура 99 уровня"},
    {"The level 99 aura shines softer and does not dazzle around the characters. The game shows the simplified "
     "aura (the circle); its /aura command turns on the full one (the ring and bubbles too) - after a teleport or a "
     "new map.",
     "Аура 99 рівня світить м'якше й не сліпить довкола персонажів. Гра показує спрощену ауру (коло); її команда "
     "/aura вмикає повну (ще кільце й бульбашки) — після телепорту чи нової карти.",
     "Аура 99 уровня светит мягче и не слепит вокруг персонажей. Игра показывает упрощённую ауру (круг); её команда "
     "/aura включает полную (ещё кольцо и пузырьки) — после телепорта или новой карты."},
    {"Aura brightness", "Яскравість аури", "Яркость ауры"},
    {"How bright the aura is, of what the game draws: 100 % as in the game, 0 % none.",
     "Наскільки яскрава аура від того, як її малює гра: 100 % — як у грі, 0 % — немає.",
     "Насколько ярка аура от того, как её рисует игра: 100 % — как в игре, 0 % — нет."},
    {"Fireflies", "Світлячки", "Светлячки"},
    {"The maps' fireflies glow, flash now and then and light the ground a little around them. Also those the maps' "
     "makers placed that the game never showed (once the map's file is read).",
     "Світлячки на картах світяться, час від часу спалахують і трохи освітлюють землю довкола. Також ті, що їх "
     "поставили автори карт, але гра ніколи не показувала (коли файл карти прочитано).",
     "Светлячки на картах светятся, время от времени вспыхивают и немного освещают землю вокруг. Также те, что "
     "поставили авторы карт, но игра никогда не показывала (когда файл карты прочитан)."},
    {"Fireflies' brightness", "Яскравість світлячків", "Яркость светлячков"},
    {"How bright the fireflies and their light are.", "Наскільки яскраві світлячки та їхнє світло.",
     "Насколько ярки светлячки и их свет."},
    {"Hidden lights of the maps", "Приховані вогники карт", "Скрытые огоньки карт"},
    {"Glows, pools of light and dust the maps' makers placed but the game never showed - Manuk, Splendide, "
     "Nidhoggur's nest, the towers - drawn from the map's file: floating balls of light, light on the ground.",
     "Сяйва, світлові кола й пил, що їх поставили автори карт, але гра ніколи не показувала, — Манук, Сплендід, "
     "гніздо Нідхьоґґа, вежі, — малюються за файлом карти: кулі світла в повітрі, світло на землі.",
     "Свечения, световые круги и пыль, которые поставили авторы карт, но игра никогда не показывала, — Манук, "
     "Сплендид, гнездо Нидхёгга, башни, — рисуются по файлу карты: шары света в воздухе, свет на земле."},
    {"Hidden lights' brightness", "Яскравість вогників", "Яркость огоньков"},
    {"How bright the maps' hidden lights and their light are.", "Наскільки яскраві приховані вогники та їхнє світло.",
     "Насколько ярки скрытые огоньки и их свет."},
    {"Beams of light in the forest", "Промені світла в лісі", "Лучи света в лесу"},
    {"The beams of light falling through the trees (Umbala, Geffenia, Glast Heim) are soft at their ends, shimmer as "
     "if through moving leaves, have dust drifting in them and light the ground where they fall.",
     "Промені світла крізь дерева (Умбала, Гефенія, Гласт Хейм) м'які на кінцях, мерехтять, наче крізь листя, у них "
     "кружляє пил, а там, де вони падають, світиться земля.",
     "Лучи света сквозь деревья (Умбала, Гефения, Гласт Хейм) мягкие на концах, мерцают, будто сквозь листву, в них "
     "кружится пыль, а там, где они падают, светится земля."},
    {"Beams' brightness", "Яскравість променів", "Яркость лучей"},
    {"How bright the beams and the light they leave on the ground are.",
     "Наскільки яскраві промені та світло, яке вони лишають на землі.",
     "Насколько ярки лучи и свет, который они оставляют на земле."},
    {"Living lava", "Жива лава", "Живая лава"},
    {"Lava (the Magma Dungeon) flows: its dark crust drifts, its cracks glow and pulse, and its heat lights the walls "
     "and the characters around it in red and orange.",
     "Лава (Магма-підземелля) тече: темна кірка пливе, тріщини світяться й пульсують, а жар освітлює стіни й "
     "персонажів довкола червоним і помаранчевим.",
     "Лава (Магма-подземелье) течёт: тёмная корка плывёт, трещины светятся и пульсируют, а жар освещает стены и "
     "персонажей вокруг красным и оранжевым."},
    {"Lava's brightness", "Яскравість лави", "Яркость лавы"},
    {"How brightly the lava glows and lights around it.", "Наскільки яскраво лава сяє та освітлює довкола.",
     "Насколько ярко лава светится и освещает всё вокруг."},
    {"Characters' shadows", "Тіні персонажів", "Тени персонажей"},
    {"Characters and monsters cast soft shadows the shape of their sprites - on the ground, away from the light: from "
     "a torch nearby, else from the sun - instead of the game's round spot under the feet.",
     "Персонажі й монстри відкидають м'які тіні за формою спрайта — на землю, від світла: від смолоскипа поруч, інакше "
     "від сонця — замість круглої плями гри під ногами.",
     "Персонажи и монстры отбрасывают мягкие тени по форме спрайта — на землю, от света: от факела рядом, иначе от "
     "солнца — вместо круглого пятна игры под ногами."},
    {"Shadows' darkness", "Темрява тіней", "Темнота теней"},
    {"How dark the characters' shadows are.", "Наскільки темні тіні персонажів.", "Насколько темны тени персонажей."},
    {"Clouds' shadows", "Тіні хмар", "Тени облаков"},
    {"Shadows of clouds drift slowly over the fields and the towns, the ground and the buildings darker under them - "
     "not in dungeons nor houses.",
     "Тіні хмар повільно пливуть полями й містами, земля та будинки під ними темнішають — не в підземеллях і не в "
     "будинках.",
     "Тени облаков медленно плывут по полям и городам, земля и здания под ними темнеют — не в подземельях и не в "
     "домах."},
    {"Clouds' darkness", "Темрява хмар", "Темнота облаков"},
    {"How dark the clouds' shadows are.", "Наскільки темні тіні хмар.", "Насколько темны тени облаков."},
    {"Haze and fog", "Серпанок і туман", "Дымка и туман"},
    {"Under the open sky a light bluish haze far off; in the dungeons the dark far off and a low fog drifting over the "
     "ground. Houses and the like stay as they are.",
     "Під відкритим небом — легкий блакитнуватий серпанок удалині; у підземеллях — темрява вдалині й низький туман, "
     "що пливе над землею. Будинки тощо — без змін.",
     "Под открытым небом — лёгкая голубоватая дымка вдали; в подземельях — темнота вдали и низкий туман, плывущий "
     "над землёй. Дома и прочее — без изменений."},
    {"Haze's strength", "Сила серпанку", "Сила дымки"},
    {"How thick the haze and the fog are.", "Наскільки густі серпанок і туман.", "Насколько густы дымка и туман."},
    {"Heat haze", "Марево від жару", "Марево от жара"},
    {"Over the torches' flames, burning skills and lava, and far off in the desert of Morroc, the picture shimmers as "
     "the hot air rises.",
     "Над полум'ям смолоскипів, вогняними вміннями й лавою, а вдалині — у пустелі Морока, картинка тремтить, наче "
     "гаряче повітря здіймається.",
     "Над пламенем факелов, огненными умениями и лавой, а вдали — в пустыне Морокка, картинка дрожит, будто "
     "поднимается горячий воздух."},
    {"Heat haze's strength", "Сила марева", "Сила марева"},
    {"How much the hot air shimmers.", "Наскільки сильно тремтить гаряче повітря.",
     "Насколько сильно дрожит горячий воздух."},
    {"Other skills in their colours", "Інші вміння своїм кольором", "Другие умения своим цветом"},
    {"Not only fire: ice and water light in cold blue, lightning in brief white flashes, holy skills in gold, poison "
     "in green - each skill in the colour of its own picture.",
     "Не лише вогонь: лід і вода світять холодним блакитним, блискавки — короткими білими спалахами, святі вміння — "
     "золотим, отрута — зеленим: кожне вміння кольором своєї картинки.",
     "Не только огонь: лёд и вода светят холодным голубым, молнии — короткими белыми вспышками, святые умения — "
     "золотым, яд — зелёным: каждое умение цветом своей картинки."},
    {"Softer skills", "М'якші вміння", "Мягкие умения"},
    {"Skills and other effects of the game (not the maps' torches, smoke and the like) shine softer, so that big ones "
     "- Storm Gust, Magnus Exorcismus, Meteor Storm - do not blind.",
     "Уміння та інші ефекти гри (не смолоскипи, дим тощо на картах) світять м'якше, щоб великі — Storm Gust, Magnus "
     "Exorcismus, Meteor Storm — не сліпили.",
     "Умения и другие эффекты игры (не факелы, дым и т. п. на картах) светят мягче, чтобы большие — Storm Gust, Magnus "
     "Exorcismus, Meteor Storm — не слепили."},
    {"Skills' brightness", "Яскравість умінь", "Яркость умений"},
    {"How bright the skills are, of what the game draws: 100 % as in the game.",
     "Наскільки яскраві вміння від того, як їх малює гра: 100 % — як у грі.",
     "Насколько ярки умения от того, как их рисует игра: 100 % — как в игре."},
    {"Living portals", "Живі портали", "Живые порталы"},
    {"The warps and the portals of the Warp Portal skill glow around in soft blue, and the air turns in them.",
     "Варпи й портали вміння Warp Portal м'яко світять довкола блакитним, а повітря в них закручується.",
     "Варпы и порталы умения Warp Portal мягко светят вокруг голубым, а воздух в них закручивается."},
    {"Portals' strength", "Сила порталів", "Сила порталов"},
    {"How bright their glow and how strong the swirl is.", "Наскільки яскраве світіння та сильний вир.",
     "Насколько ярко свечение и силён вихрь."},
    {"Weather", "Погода", "Погода"},
    {"The driver's own weather on some maps: snow over Lutie, leaves falling in Payon's woods, sand in the wind about "
     "Morroc, a fine rain over Niflheim and Einbroch.",
     "Власна погода драйвера на деяких картах: сніг над Лютією, листопад у лісах Пайона, піщаний вітер довкола "
     "Морока, дрібний дощ над Нифльгеймом та Ейнброхом.",
     "Своя погода драйвера на некоторых картах: снег над Лютией, листопад в лесах Пайона, песчаный ветер вокруг "
     "Морокка, мелкий дождь над Нифльхеймом и Эйнброхом."},
    {"Weather's amount", "Кількість опадів", "Количество осадков"},
    {"How much snow, rain, leaves or sand there is.", "Скільки снігу, дощу, листя чи піску.",
     "Сколько снега, дождя, листьев или песка."},
    {"Window buttons, frames and labels", "Кнопки, рамки й написи вікон", "Кнопки, рамки и надписи окон"},
    {"How the windows' buttons, tabs, frames and the labels drawn on them are enlarged. Sharp (xBR) keeps letters "
     "one pixel thick whole; the AI is softer but melts them (the icons and pictures stay the AI's either way, with AI "
     "textures on). Needs crisp text.",
     "Як збільшуються кнопки, вкладки, рамки вікон і написи, намальовані на них. Чіткі (xBR) зберігають літери "
     "завтовшки в одну точку; нейромережа м'якша, але розмиває їх (іконки й картинки з ШІ-текстурами в будь-якому "
     "разі збільшує вона). Потрібен чіткий текст.",
     "Как увеличиваются кнопки, вкладки, рамки окон и надписи, нарисованные на них. Чёткие (xBR) сохраняют буквы "
     "толщиной в одну точку; нейросеть мягче, но размывает их (иконки и картинки с ИИ-текстурами в любом случае "
     "увеличивает она). Нужен чёткий текст."},
    {"Sharp (xBR)", "Чіткі (xBR)", "Чёткие (xBR)"},
    {"AI (with AI textures)", "Нейромережа (з ШІ-текстурами)", "Нейросеть (с ИИ-текстурами)"},
    {"As in the game (pixels)", "Як у грі (пікселі)", "Как в игре (пиксели)"},
    {"Make new ones while playing (debugging)", "Створювати нові під час гри (налагодження)",
     "Создавать новые во время игры (отладка)"},
    {"Direct3D 12: the network makes in the game what the texture pack does not have yet, a little every frame, and "
     "writes it into the pack. For the pack's maker; players take the ready pack - their graphics card makes nothing.",
     "Direct3D 12: нейромережа створює в грі те, чого ще немає в пакеті текстур, потроху кожен кадр, і дописує це в "
     "пакет. Для того, хто збирає пакет; гравці беруть готовий пакет - їхня відеокарта нічого не створює.",
     "Direct3D 12: нейросеть создаёт в игре то, чего ещё нет в пакете текстур, понемногу каждый кадр, и дописывает "
     "это в пакет. Для того, кто собирает пакет; игроки берут готовый пакет - их видеокарта ничего не создаёт."},
    {"The textures come from the texture pack uaro-textures.zip in the game's folder. What it lacks the game's log "
     "says (\"not in the pack\").",
     "Текстури беруться з пакета текстур uaro-textures.zip у теці гри. Чого в ньому бракує, пише журнал гри (\"not in "
     "the pack\").",
     "Текстуры берутся из пакета текстур uaro-textures.zip в папке игры. Чего в нём не хватает, пишет лог игры (\"not "
     "in the pack\")."},
    {"Texture pack", "Пакет текстур", "Пакет текстур"},
    {"The texture pack - uaro-textures.zip in the game's folder - holds the AI's pictures of the game's textures, made "
     "once on this graphics card. Give it to other players: their game shows the pictures at once and their graphics "
     "card makes nothing. Making it takes from tens of minutes to a few hours; it can stop and go on later.",
     "Пакет текстур - uaro-textures.zip у теці гри - містить картинки ШІ для текстур гри, створені один раз на цій "
     "відеокарті. Передай його іншим гравцям: їхня гра одразу показує картинки, а відеокарта нічого не створює. "
     "Створення триває від десятків хвилин до кількох годин; його можна зупинити й продовжити пізніше.",
     "Пакет текстур - uaro-textures.zip в папке игры - содержит картинки ИИ для текстур игры, созданные один раз на "
     "этой видеокарте. Передай его другим игрокам: их игра сразу показывает картинки, а видеокарта ничего не "
     "создаёт. Создание занимает от десятков минут до нескольких часов; его можно остановить и продолжить позже."},
    {"uaro-textures.zip: {1} pictures, {2}.", "uaro-textures.zip: {1} картинок, {2}.",
     "uaro-textures.zip: {1} картинок, {2}."},
    {"There is no texture pack in the game's folder yet.", "У теці гри ще немає пакета текстур.",
     "В папке игры ещё нет пакета текстур."},
    {"What goes into the pack:", "Що входить у пакет:", "Что входит в пакет:"},
    {"Maps", "Карти", "Карты"},
    {"The ground, buildings, trees and water of every map.", "Земля, будівлі, дерева й вода всіх карт.",
     "Земля, здания, деревья и вода всех карт."},
    {"Effects", "Ефекти", "Эффекты"},
    {"The light and sparks of skills and maps.", "Світло й іскри вмінь і карт.", "Свет и искры умений и карт."},
    {"Monsters", "Монстри", "Монстры"},
    {"Monsters, homunculi and mercenaries: every frame of their sprites.",
     "Монстри, гомункули й найманці: кожен кадр їхніх спрайтів.",
     "Монстры, гомункулы и наёмники: каждый кадр их спрайтов."},
    {"Characters", "Персонажі", "Персонажи"},
    {"Bodies, heads, headgear, weapons and shields - the most frames of all.",
     "Тіла, голови, головні убори, зброя й щити - найбільше кадрів.",
     "Тела, головы, головные уборы, оружие и щиты - больше всего кадров."},
    {"NPCs", "NPC", "NPC"},
    {"The NPCs' sprites.", "Спрайти NPC.", "Спрайты NPC."},
    {"Items", "Предмети", "Предметы"},
    {"Items lying on the ground.", "Предмети, що лежать на землі.", "Предметы, лежащие на земле."},
    {"Interface", "Інтерфейс", "Интерфейс"},
    {"Item and skill icons, the windows' pictures.", "Іконки предметів і вмінь, картинки вікон.",
     "Иконки предметов и умений, картинки окон."},
    {"Picture quality", "Якість картинок", "Качество картинок"},
    {"WebP. Balanced is some ten times smaller than lossless and looks the same in the game; lossless keeps every "
     "pixel and is several times larger.",
     "WebP. Збалансована приблизно вдесятеро менша за якість без втрат і в грі виглядає так само; без втрат зберігає "
     "кожну точку й займає в кілька разів більше.",
     "WebP. Сбалансированное примерно в десять раз меньше качества без потерь и в игре выглядит так же; без потерь "
     "сохраняет каждую точку и занимает в несколько раз больше."},
    {"Smaller file (85)", "Менший файл (85)", "Файл поменьше (85)"},
    {"Balanced (92)", "Збалансована (92)", "Сбалансированное (92)"},
    {"Lossless", "Без втрат", "Без потерь"},
    {"The networks are the AI textures page's: {1} for the maps, {2} for the rest.",
     "Нейромережі - зі сторінки ШІ-текстур: {1} для карт, {2} для решти.",
     "Нейросети - со страницы ИИ-текстур: {1} для карт, {2} для остального."},
    {"Make the pack", "Створити пакет", "Создать пакет"},
    {"Stop", "Зупинити", "Остановить"},
    {"Make anew", "Створити заново", "Создать заново"},
    {"Copy the result", "Скопіювати результат", "Скопировать результат"},
    {"Throw away the {1} pictures of the pack and make it anew?",
     "Викинути {1} картинок пакета й створити його заново?", "Выбросить {1} картинок пакета и создать его заново?"},
    {"uaro-graphics\\uaro-render64.exe is missing: install uaRO Graphics again.",
     "Немає uaro-graphics\\uaro-render64.exe: встанови uaRO Graphics ще раз.",
     "Нет uaro-graphics\\uaro-render64.exe: установи uaRO Graphics ещё раз."},
    {"Tick at least one kind.", "Познач хоча б один вид.", "Отметь хотя бы один вид."},
    {"Starting the networks on the graphics card...", "Запуск нейромереж на відеокарті...",
     "Запуск нейросетей на видеокарте..."},
    {"Reading the game's files: {1} of {2}.", "Читання файлів гри: {1} з {2}.", "Чтение файлов игры: {1} из {2}."},
    {"Pictures: {1} of {2}, {3} a second; about {4} left.",
     "Картинки: {1} з {2}, {3} за секунду; лишилося близько {4}.",
     "Картинки: {1} из {2}, {3} в секунду; осталось около {4}."},
    {"Pictures: {1} of {2}.", "Картинки: {1} з {2}.", "Картинки: {1} из {2}."},
    {"Now: {1}", "Зараз: {1}", "Сейчас: {1}"},
    {"Done: the pack holds {1} pictures, {2}.", "Готово: у пакеті {1} картинок, {2}.",
     "Готово: в пакете {1} картинок, {2}."},
    {"Stopped: {1} pictures made this time. Make the pack goes on from here.",
     "Зупинено: цього разу створено {1} картинок. «Створити пакет» продовжить з цього місця.",
     "Остановлено: в этот раз создано {1} картинок. «Создать пакет» продолжит с этого места."},
    {"Failed: {1}", "Помилка: {1}", "Ошибка: {1}"},
    {"The builder closed before it finished. Make the pack goes on from where it stopped.",
     "Збирач закрився, не закінчивши. «Створити пакет» продовжить з того місця, де він зупинився.",
     "Сборщик закрылся, не закончив. «Создать пакет» продолжит с того места, где он остановился."},
    {"The game is running: it shares the graphics card with the builder and may stutter meanwhile.",
     "Гра запущена: вона ділить відеокарту зі збирачем і тим часом може підгальмовувати.",
     "Игра запущена: она делит видеокарту со сборщиком и в это время может подтормаживать."},
    {"The pack is made in the background: this window can be closed, the work goes on.",
     "Пакет створюється у фоні: це вікно можна закрити, робота триває.",
     "Пакет создаётся в фоне: это окно можно закрыть, работа продолжится."},
    {"{1} h {2} min", "{1} год {2} хв", "{1} ч {2} мин"},
    {"{1} min", "{1} хв", "{1} мин"},
    {"{1} s", "{1} с", "{1} с"},
    {"Graphics API", "Графічний API", "Графический API"},
    {"How uaRO Graphics draws. Direct3D 12 has every feature (DLSS, effects, AI textures). Direct3D 11 is for graphics "
     "cards without Direct3D 12 (Radeon HD 5000-7000, GeForce GTX 400-600 with old drivers, Intel HD 4000) and "
     "Windows 7 and 8: the game, the window's size, crisp text and the AI textures of the texture pack, without DLSS "
     "and effects yet. Auto takes Direct3D 12 and else 11.",
     "Як малює uaRO Graphics. Direct3D 12 має всі можливості (DLSS, ефекти, ШІ-текстури). Direct3D 11 - для "
     "відеокарт без Direct3D 12 (Radeon HD 5000-7000, GeForce GTX 400-600 зі старими драйверами, Intel HD 4000) і "
     "Windows 7 та 8: гра, розмір вікна, чіткий текст і ШІ-текстури з пакета текстур, поки без DLSS і ефектів. "
     "«Авто» бере Direct3D 12, а без нього - 11.",
     "Как рисует uaRO Graphics. Direct3D 12 умеет всё (DLSS, эффекты, ИИ-текстуры). Direct3D 11 - для видеокарт без "
     "Direct3D 12 (Radeon HD 5000-7000, GeForce GTX 400-600 со старыми драйверами, Intel HD 4000) и Windows 7 и 8: "
     "игра, размер окна, чёткий текст и ИИ-текстуры из пакета текстур, пока без DLSS и эффектов. «Авто» берёт "
     "Direct3D 12, а без него - 11."},
    {"Direct3D 12 (all features)", "Direct3D 12 (усі можливості)", "Direct3D 12 (все возможности)"},
    {"Direct3D 11 (older graphics cards, test)", "Direct3D 11 (старші відеокарти, тест)",
     "Direct3D 11 (старые видеокарты, тест)"},
    {"Auto: 12, else 11", "Авто: 12, інакше 11", "Авто: 12, иначе 11"},
    {"This computer has no Direct3D 12, but Direct3D 11 works: choose Direct3D 11 or Auto as the graphics API.",
     "На цьому комп'ютері немає Direct3D 12, але Direct3D 11 працює: виберіть графічний API Direct3D 11 або «Авто».",
     "На этом компьютере нет Direct3D 12, но Direct3D 11 работает: выберите графический API Direct3D 11 или «Авто»."},
    {"Direct3D 11", "Direct3D 11", "Direct3D 11"},
    {"works, feature level {1}", "працює, рівень можливостей {1}", "работает, уровень возможностей {1}"},
    {"does not work ({1})", "не працює ({1})", "не работает ({1})"},
    {"With Direct3D 11 (the Render page) the game shows the ready pictures of the texture pack; making new ones while "
     "playing needs Direct3D 12.",
     "З Direct3D 11 (вкладка «Рендер») гра показує готові картинки пакета текстур; створення нових під час гри "
     "потребує Direct3D 12.",
     "С Direct3D 11 (вкладка «Рендер») игра показывает готовые картинки пакета текстур; создание новых во время игры "
     "требует Direct3D 12."},
    {"Delete upscaled.grf ({1})? It is not used any more: the textures come from the texture pack.",
     "Видалити upscaled.grf ({1})? Він більше не використовується: текстури беруться з пакета текстур.",
     "Удалить upscaled.grf ({1})? Он больше не используется: текстуры берутся из пакета текстур."},
    {"upscaled.grf is deleted.",
     "upscaled.grf видалено.",
     "upscaled.grf удалён."},
    {"Updates",
     "Оновлення",
     "Обновление"},
    {"Check for updates",
     "Перевірити оновлення",
     "Проверить обновления"},
    {"Update now",
     "Оновити зараз",
     "Обновить сейчас"},
    {"Asking GitHub for the newest version...",
     "Запитую в GitHub найновішу версію...",
     "Спрашиваю у GitHub новейшую версию..."},
    {"A newer version is out: {1} (this one is {2}). Update now installs it.",
     "Вийшла новіша версія: {1} (ця — {2}). «Оновити зараз» встановить її.",
     "Вышла новая версия: {1} (эта — {2}). «Обновить сейчас» установит её."},
    {"{1} is the newest version.",
     "{1} — найновіша версія.",
     "{1} — новейшая версия."},
    {"Could not check: {1}",
     "Не вдалося перевірити: {1}",
     "Не удалось проверить: {1}"},
    {"update.bat downloads the newest uaRO Graphics from GitHub and installs it; this window closes now. Go on?",
     "update.bat завантажить найновіший uaRO Graphics із GitHub і встановить його; це вікно зараз закриється. "
     "Продовжити?",
     "update.bat скачает новейший uaRO Graphics с GitHub и установит его; это окно сейчас закроется. Продолжить?"},
    {"Close the game first: the update replaces its files.",
     "Спершу закрийте гру: оновлення замінює її файли.",
     "Сначала закройте игру: обновление заменяет её файлы."},
    {"Update now runs update.bat: it downloads the newest package and installs it, the settings stay.",
     "«Оновити зараз» запускає update.bat: він завантажує найновіший пакет і встановлює його, налаштування "
     "залишаються.",
     "«Обновить сейчас» запускает update.bat: он скачивает новейший пакет и устанавливает его, настройки остаются."},
    {"The update could not start: {1}",
     "Оновлення не запустилося: {1}",
     "Обновление не запустилось: {1}"},
    {"Set up for this computer",
     "Налаштувати під цей комп'ютер",
     "Настроить под этот компьютер"},
    {"Set up automatically",
     "Налаштувати автоматично",
     "Настроить автоматически"},
    {"Looks at the graphics card, its video memory and the screen and picks what they carry; the texture pack is used "
     "when uaro-textures.zip lies next to uaRO.exe. On a new computer: press it, then start the game.",
     "Дивиться на відеокарту, її відеопам'ять та екран і вибирає те, що вони потягнуть; пакет текстур вмикається, "
     "якщо uaro-textures.zip лежить поруч з uaRO.exe. На новому комп'ютері: натисніть, потім запустіть гру.",
     "Смотрит на видеокарту, её видеопамять и экран и выбирает то, что они потянут; пакет текстур включается, если "
     "uaro-textures.zip лежит рядом с uaRO.exe. На новом компьютере: нажмите, потом запустите игру."},
    {"Graphics card: {1}, {2} MB of video memory; screen {3}.",
     "Відеокарта: {1}, {2} МБ відеопам'яті; екран {3}.",
     "Видеокарта: {1}, {2} МБ видеопамяти; экран {3}."},
    {"No graphics driver is installed (Windows' basic display adapter) or the card is too old: the game's own look is "
     "set. Install the graphics card's driver from its maker's site and press again.",
     "Драйвер відеокарти не встановлено (базовий відеоадаптер Windows) або карта надто стара: встановлено власний "
     "вигляд гри. Встановіть драйвер відеокарти з сайту виробника й натисніть ще раз.",
     "Драйвер видеокарты не установлен (базовый видеоадаптер Windows) или карта слишком старая: установлен "
     "собственный вид игры. Установите драйвер видеокарты с сайта производителя и нажмите ещё раз."},
    {"The card has no Direct3D 12: Direct3D 11, the picture at the game's size, no smoothing. The Effects page needs "
     "Direct3D 12.",
     "Карта не має Direct3D 12: Direct3D 11, картинка в розмірі гри, без згладжування. Сторінці «Ефекти» потрібен "
     "Direct3D 12.",
     "У карты нет Direct3D 12: Direct3D 11, картинка в размере игры, без сглаживания. Странице «Эффекты» нужен "
     "Direct3D 12."},
    {"A small graphics card: the light settings, the heavy effects off.",
     "Слабка відеокарта: легкі налаштування, важкі ефекти вимкнено.",
     "Слабая видеокарта: лёгкие настройки, тяжёлые эффекты выключены."},
    {"A middle graphics card: the balanced settings.",
     "Середня відеокарта: збалансовані налаштування.",
     "Средняя видеокарта: сбалансированные настройки."},
    {"A strong graphics card: the picture drawn at {1}% and smoothed.",
     "Потужна відеокарта: картинка малюється на {1}% і згладжується.",
     "Мощная видеокарта: картинка рисуется на {1}% и сглаживается."},
    {"AI textures: on, every kind (from the texture pack).",
     "ШІ-текстури: увімкнено, усі види (з пакета текстур).",
     "ИИ-текстуры: включены, все виды (из пакета текстур)."},
    {"AI textures: characters, monsters, items and icons only - the card has under 1 GB of video memory.",
     "ШІ-текстури: лише персонажі, монстри, предмети та іконки — у карти менше 1 ГБ відеопам'яті.",
     "ИИ-текстуры: только персонажи, монстры, предметы и иконки — у карты меньше 1 ГБ видеопамяти."},
    {"AI textures: off - uaro-textures.zip is not next to uaRO.exe.",
     "ШІ-текстури: вимкнено — поруч з uaRO.exe немає uaro-textures.zip.",
     "ИИ-текстуры: выключены — рядом с uaRO.exe нет uaro-textures.zip."},
    {"AI textures: off.",
     "ШІ-текстури: вимкнено.",
     "ИИ-текстуры: выключены."},
    {"Write these settings and start the game?",
     "Записати ці налаштування й запустити гру?",
     "Записать эти настройки и запустить игру?"},
    {"The settings are on the pages now; Apply writes them.",
     "Налаштування вже на сторінках; «Застосувати» запише їх.",
     "Настройки уже на страницах; «Применить» запишет их."},
    {"Graphics card",
     "Відеокарта",
     "Видеокарта"},
    {"Which graphics card draws the game when the computer has several (a laptop's integrated one and its NVIDIA or "
     "AMD). Automatic takes the fast one - Windows' choice for high performance. The game, the 64-bit renderer and "
     "the texture pack's builder use it.",
     "Яка відеокарта малює гру, якщо їх кілька (вбудована в ноутбуці та NVIDIA чи AMD). «Автоматично» бере швидшу — "
     "вибір Windows для високої продуктивності. Нею користуються гра, 64-бітний рендер і збирач пакета текстур.",
     "Какая видеокарта рисует игру, если их несколько (встроенная в ноутбуке и NVIDIA или AMD). «Автоматически» берёт "
     "быструю — выбор Windows для высокой производительности. Ею пользуются игра, 64-битный рендер и сборщик пакета "
     "текстур."},
    {"Automatic: {1}",
     "Автоматично: {1}",
     "Автоматически: {1}"},
    {"{1}, {2} MB",
     "{1}, {2} МБ",
     "{1}, {2} МБ"},
    {"{1} (not found)",
     "{1} (не знайдено)",
     "{1} (не найдена)"},
    {"Report to the developers",
     "Звіт розробникам",
     "Отчёт разработчикам"},
    {"Sends this computer's graphics card, Windows, monitors, uaro-graphics.ini and the logs of uaRO Graphics to "
     "uaro.kiev.ua - no login, no character; the Windows user name is hidden. The developers keep it 90 days.",
     "Надсилає на uaro.kiev.ua відеокарту, Windows, монітори цього комп'ютера, uaro-graphics.ini та журнали uaRO "
     "Graphics — без логіна й персонажа; ім'я користувача Windows приховано. Розробники зберігають його 90 днів.",
     "Отправляет на uaro.kiev.ua видеокарту, Windows, мониторы этого компьютера, uaro-graphics.ini и логи uaRO "
     "Graphics — без логина и персонажа; имя пользователя Windows скрыто. Разработчики хранят его 90 дней."},
    {"What happened",
     "Що сталося",
     "Что случилось"},
    {"for example: black screen after login, slow in Prontera",
     "наприклад: чорний екран після входу, гальмує в Пронтері",
     "например: чёрный экран после входа, тормозит в Пронтере"},
    {"Send the report",
     "Надіслати звіт",
     "Отправить отчёт"},
    {"Send the report to uaro.kiev.ua now?",
     "Надіслати звіт на uaro.kiev.ua зараз?",
     "Отправить отчёт на uaro.kiev.ua сейчас?"},
    {"The report is sent. Its code: {1} (copied) - name it to the developers in the chat or on the forum.",
     "Звіт надіслано. Його код: {1} (скопійовано) — назвіть його розробникам у чаті чи на форумі.",
     "Отчёт отправлен. Его код: {1} (скопирован) — назовите его разработчикам в чате или на форуме."},
    {"The report was not sent: {1}. Try again later, or send uaro-graphics-report.txt (saved next to the game) by "
     "hand.",
     "Звіт не надіслано: {1}. Спробуйте пізніше або надішліть uaro-graphics-report.txt (збережено поруч з грою) "
     "вручну.",
     "Отчёт не отправлен: {1}. Попробуйте позже или отправьте uaro-graphics-report.txt (сохранён рядом с игрой) "
     "вручную."},
    {"Sent from this computer: {1}",
     "Надіслано з цього комп'ютера: {1}",
     "Отправлено с этого компьютера: {1}"},
    {"Send a report by itself when something goes wrong",
     "Надсилати звіт самостійно, якщо щось пішло не так",
     "Отправлять отчёт самостоятельно, если что-то пошло не так"},
    {"After a game in which the renderer would not start, the graphics card was lost or the game closed without "
     "closing the renderer, a report goes to the developers at uaro.kiev.ua (at most one in 6 hours). Takes effect at "
     "the next start of the game.",
     "Після гри, у якій рендер не запустився, відеокарта відпала або гра закрилася, не закривши рендер, звіт іде "
     "розробникам на uaro.kiev.ua (не частіше одного на 6 годин). Діє з наступного запуску гри.",
     "После игры, в которой рендер не запустился, видеокарта отвалилась или игра закрылась, не закрыв рендер, отчёт "
     "уходит разработчикам на uaro.kiev.ua (не чаще одного в 6 часов). Действует со следующего запуска игры."},
    {"Download from uaro.kiev.ua",
     "Завантажити з uaro.kiev.ua",
     "Скачать с uaro.kiev.ua"},
    {"The texture pack made by the developers: nothing to make on this computer. Only the pictures this computer "
     "lacks come; a download cut short goes on where it stopped, and a newer pack on the site brings only its new "
     "pictures.",
     "Пакет текстур, зроблений розробниками: на цьому комп'ютері нічого робити не треба. Приходять лише картинки, "
     "яких тут немає; перерване завантаження продовжується з того місця, а новіший пакет на сайті приносить лише нові "
     "картинки.",
     "Пакет текстур, сделанный разработчиками: на этом компьютере ничего делать не нужно. Приходят только картинки, "
     "которых здесь нет; прерванная загрузка продолжается с того же места, а более новый пакет на сайте приносит "
     "только новые картинки."},
    {"Download the pack",
     "Завантажити пакет",
     "Скачать пакет"},
    {"Stop",
     "Зупинити",
     "Остановить"},
    {"Reading the pack on the site...",
     "Читаю пакет на сайті...",
     "Читаю пакет на сайте..."},
    {"{1} of {2} pictures, {3} of {4}, {5}/s, about {6} min left",
     "{1} з {2} картинок, {3} з {4}, {5}/с, лишилося близько {6} хв",
     "{1} из {2} картинок, {3} из {4}, {5}/с, осталось около {6} мин"},
    {"Done: {1} pictures came, {2} were here already. Start the game.",
     "Готово: прийшло {1} картинок, {2} вже були. Запускайте гру.",
     "Готово: пришло {1} картинок, {2} уже были. Запускайте игру."},
    {"The pack is up to date: every picture of the site's pack is here ({1}).",
     "Пакет актуальний: усі картинки пакета з сайту вже тут ({1}).",
     "Пакет актуален: все картинки пакета с сайта уже здесь ({1})."},
    {"Stopped: {1}. Press again to go on from there.",
     "Зупинено: {1}. Натисніть ще раз, щоб продовжити з того місця.",
     "Остановлено: {1}. Нажмите ещё раз, чтобы продолжить с того же места."},
    {"Stopped ({1} pictures came). Press again to go on from there.",
     "Зупинено (прийшло {1} картинок). Натисніть ще раз, щоб продовжити з того місця.",
     "Остановлено (пришло {1} картинок). Нажмите ещё раз, чтобы продолжить с того же места."},
    {"Make the pack on this computer",
     "Зробити пакет на цьому комп'ютері",
     "Сделать пакет на этом компьютере"},
    {"Frame counter",
     "Лічильник кадрів",
     "Счётчик кадров"},
    {"Frames a second and the frame time in the game window's top left corner. Ctrl+Shift+F11 in the game shows and "
     "hides it at any time.",
     "Кадри за секунду й час кадру в лівому верхньому куті вікна гри. Ctrl+Shift+F11 у грі показує й ховає його "
     "будь-коли.",
     "Кадры в секунду и время кадра в левом верхнем углу окна игры. Ctrl+Shift+F11 в игре показывает и прячет его в "
     "любой момент."},
    {"Profiles",
     "Профілі",
     "Профили"},
    {"A profile keeps every setting of uaro-graphics.ini under a name - one for playing, one for a weak laptop, one "
     "for recording - and brings them all back at a press.",
     "Профіль зберігає всі налаштування uaro-graphics.ini під назвою — для гри, для слабкого ноутбука, для запису — і "
     "повертає їх одним натисканням.",
     "Профиль хранит все настройки uaro-graphics.ini под именем — для игры, для слабого ноутбука, для записи — и "
     "возвращает их одним нажатием."},
    {"Name",
     "Назва",
     "Имя"},
    {"for example: Maximum for 4K",
     "наприклад: Максимум для 4K",
     "например: Максимум для 4K"},
    {"Save as a profile",
     "Зберегти як профіль",
     "Сохранить как профиль"},
    {"Load",
     "Завантажити",
     "Загрузить"},
    {"Delete",
     "Видалити",
     "Удалить"},
    {"(no profiles yet)",
     "(профілів ще немає)",
     "(профилей пока нет)"},
    {"The profile \"{1}\" is saved.",
     "Профіль «{1}» збережено.",
     "Профиль «{1}» сохранён."},
    {"The profile \"{1}\" is in uaro-graphics.ini now: most settings take effect at once, the renderer's at the next "
     "start of the game.",
     "Профіль «{1}» тепер в uaro-graphics.ini: більшість налаштувань діє одразу, налаштування рендера — з наступного "
     "запуску гри.",
     "Профиль «{1}» теперь в uaro-graphics.ini: большинство настроек действует сразу, настройки рендера — со "
     "следующего запуска игры."},
    {"Replace the settings with the profile \"{1}\"? The present ones go to a backup first.",
     "Замінити налаштування профілем «{1}»? Поточні спершу підуть у резервну копію.",
     "Заменить настройки профилем «{1}»? Текущие сначала уйдут в резервную копию."},
    {"Delete the profile \"{1}\"?",
     "Видалити профіль «{1}»?",
     "Удалить профиль «{1}»?"},
    {"A profile's name: letters, digits, spaces, - and _, up to 40.",
     "Назва профілю: літери, цифри, пробіли, - і _, до 40.",
     "Имя профиля: буквы, цифры, пробелы, - и _, до 40."},
    {"Clothes colours",
     "Кольори одягу",
     "Цвета одежды"},
    {"The characters' bodies in every clothes dye too (players who dyed their clothes): 4 to 9 times the bodies' "
     "pictures - a bigger pack and a longer make.",
     "Тіла персонажів також у кожному кольорі фарби одягу (гравці, що пофарбували одяг): у 4–9 разів більше картинок "
     "тіл — більший пакет і довше створення.",
     "Тела персонажей также в каждом цвете краски одежды (игроки, покрасившие одежду): в 4–9 раз больше картинок тел "
     "— больше пакет и дольше создание."},
    {"Compress in video memory",
     "Стискати у відеопам'яті",
     "Сжимать в видеопамяти"},
    {"Direct3D 11: the AI textures compressed (BC1, BC3) - a quarter to an eighth of the video memory, a little less "
     "exact. Automatic: on for a card under 3 GB.",
     "Direct3D 11: ШІ-текстури стиснуті (BC1, BC3) — від чверті до восьмої частини відеопам'яті, трохи менш точно. "
     "Автоматично: увімкнено для карти до 3 ГБ.",
     "Direct3D 11: ИИ-текстуры сжаты (BC1, BC3) — от четверти до восьмой части видеопамяти, чуть менее точно. "
     "Автоматически: включено для карты до 3 ГБ."},
    {"Automatic",
     "Автоматично",
     "Автоматически"},
    {"On",
     "Увімкнено",
     "Включено"},
    {"Off",
     "Вимкнено",
     "Выключено"},
    {"Interface at the screen's pixels",
     "Інтерфейс у пікселях екрана",
     "Интерфейс в пикселях экрана"},
    {"With a render scale over 100 % on Direct3D 12 without DLSS: only the world is drawn larger and made smaller; "
     "the windows and the text are drawn at the screen's own pixels - sharper. A test: say how it looks.",
     "З масштабом рендера понад 100 % на Direct3D 12 без DLSS: більшим малюється й зменшується лише світ; вікна й "
     "текст малюються в пікселях самого екрана — різкіше. Перевірка: скажіть, як виглядає.",
     "С масштабом рендера больше 100 % на Direct3D 12 без DLSS: больше рисуется и уменьшается только мир; окна и "
     "текст рисуются в пикселях самого экрана — резче. Проверка: скажите, как выглядит."},
    {"Night",
     "Ніч",
     "Ночь"},
    {"The world darker and colder; the torches, lamps, skills and fireflies shine out of it. The interface stays as "
     "it is.",
     "Світ темніший і холодніший; факели, лампи, уміння та світлячки світять із темряви. Інтерфейс лишається як є.",
     "Мир темнее и холоднее; факелы, лампы, умения и светлячки светят из темноты. Интерфейс остаётся как есть."},
    {"How dark",
     "Наскільки темно",
     "Насколько темно"},
    {"0 % the day as it is, 100 % a deep night.",
     "0 % — день як є, 100 % — глибока ніч.",
     "0 % — день как есть, 100 % — глубокая ночь."},
    {"By the clock",
     "За годинником",
     "По часам"},
    {"Night only from 20:00 to 6:00 by this computer's clock, with an hour of dusk and of dawn.",
     "Ніч лише з 20:00 до 6:00 за годинником цього комп'ютера, з годиною сутінків і світанку.",
     "Ночь только с 20:00 до 6:00 по часам этого компьютера, с часом сумерек и рассвета."},
    {"Leaves in the wind",
     "Листя на вітрі",
     "Листва на ветру"},
    {"The leaves of trees and bushes sway a little, their tops more than their trunks.",
     "Листя дерев і кущів трохи гойдається, верхівки більше за стовбури.",
     "Листва деревьев и кустов немного колышется, верхушки больше стволов."},
    {"How much",
     "Наскільки",
     "Насколько"},
    {"100 % a light breeze, 200 % a strong wind.",
     "100 % — легкий вітерець, 200 % — сильний вітер.",
     "100 % — лёгкий ветерок, 200 % — сильный ветер."},
    {"HDR",
     "HDR",
     "HDR"},
    {"Direct3D 12 on a monitor with Windows' HDR on: the picture in HDR10, white at 200 nits, the fires and sparks a "
     "little brighter than white. Not with DLSS frame generation. A test.",
     "Direct3D 12 на моніторі з увімкненим HDR у Windows: картинка в HDR10, білий 200 ніт, вогні й іскри трохи "
     "яскравіші за білий. Не з генерацією кадрів DLSS. Перевірка.",
     "Direct3D 12 на мониторе с включённым HDR в Windows: картинка в HDR10, белый 200 нит, огни и искры чуть ярче "
     "белого. Не с генерацией кадров DLSS. Проверка."},
    {"Soft effects at the ground",
     "М’які ефекти біля землі",
     "Мягкие эффекты у земли"},
    {"Skills, sparks and portals fade softly where they touch the ground and the walls instead of being cut off along "
     "a hard line.",
     "Уміння, іскри й портали м’яко згасають там, де торкаються землі та стін, а не обрізаються різкою лінією.",
     "Умения, искры и порталы мягко гаснут там, где касаются земли и стен, а не обрезаются резкой линией."},
    {"Rim light on characters",
     "Контрове світло на персонажах",
     "Контровой свет на персонажах"},
    {"Torches, lamps and burning skills light the edge of a character that faces them more, so the flat sprites look "
     "rounder near a fire.",
     "Факели, лампи й палаючі вміння сильніше освітлюють край персонажа, повернутий до них, тож пласкі спрайти біля "
     "вогню виглядають об’ємніше.",
     "Факелы, лампы и горящие умения сильнее освещают край персонажа, обращённый к ним, поэтому плоские спрайты у "
     "огня выглядят объёмнее."},
    {"Rim light strength",
     "Сила контрового світла",
     "Сила контрового света"},
    {"How bright the lit edge is.",
     "Наскільки яскравий освітлений край.",
     "Насколько ярок освещённый край."},
    {"Sky past the map's edge",
     "Небо за краєм карти",
     "Небо за краем карты"},
    {"On the fields and in the towns the empty space past the edge of the map shows a sky with drifting clouds; at "
     "night it is dark with stars. Direct3D 12.",
     "На полях і в містах порожнеча за краєм карти показує небо з хмарами, що пливуть; уночі воно темне, із зорями. "
     "Direct3D 12.",
     "На полях и в городах пустота за краем карты показывает небо с плывущими облаками; ночью оно тёмное, со "
     "звёздами. Direct3D 12."},
    {"Clouds",
     "Хмари",
     "Облака"},
    {"How much of the sky the clouds cover.",
     "Яку частину неба вкривають хмари.",
     "Какую часть неба закрывают облака."},
    {"Sky strength",
     "Сила неба",
     "Сила неба"},
    {"How much the sky covers the void: less lets the game's own colour show through.",
     "Наскільки небо закриває порожнечу: менше — проступає власний колір гри.",
     "Насколько небо закрывает пустоту: меньше — проступает собственный цвет игры."},
    {"Motes in the air",
     "Порошинки в повітрі",
     "Частицы в воздухе"},
    {"Where the map has no weather of its own, tiny motes drift in the air: pollen over the fields and the towns, "
     "dust in the dungeons and the houses. Direct3D 12.",
     "Де на карті немає власної погоди, у повітрі пливуть дрібні порошинки: пилок над полями й містами, пил у "
     "підземеллях і будинках. Direct3D 12.",
     "Где на карте нет своей погоды, в воздухе плывут мелкие частицы: пыльца над полями и городами, пыль в "
     "подземельях и домах. Direct3D 12."},
    {"How many motes",
     "Скільки порошинок",
     "Сколько частиц"},
    {"How many motes float in the air.",
     "Скільки порошинок пливе в повітрі.",
     "Сколько частиц плывёт в воздухе."},
    {"Ground relief",
     "Рельєф землі",
     "Рельеф земли"},
    {"The cracks, stones and grass of the ground are lit from one side and look raised - best with the AI textures.",
     "Тріщини, каміння й трава на землі освітлені з одного боку й виглядають рельєфними — найкраще з ШІ-текстурами.",
     "Трещины, камни и трава на земле освещены с одной стороны и выглядят рельефными — лучше всего с ИИ-текстурами."},
    {"Relief strength",
     "Сила рельєфу",
     "Сила рельефа"},
    {"How deep the ground looks.",
     "Наскільки глибокою виглядає земля.",
     "Насколько глубокой выглядит земля."},
    {"Reflections on the floor",
     "Відбиття на підлозі",
     "Отражения на полу"},
    {"Indoors (dungeons, castles, houses) and in the rain the floor reflects the characters and what stands on it; a "
     "dry field does not shine. Direct3D 12.",
     "У приміщеннях (підземелля, замки, будинки) і під дощем підлога відбиває персонажів і те, що на ній стоїть; сухе "
     "поле не блищить. Direct3D 12.",
     "В помещениях (подземелья, замки, дома) и под дождём пол отражает персонажей и то, что на нём стоит; сухое поле "
     "не блестит. Direct3D 12."},
    {"Floor shine",
     "Блиск підлоги",
     "Блеск пола"},
    {"How much the floor reflects; in the rain more.",
     "Наскільки підлога відбиває; під дощем більше.",
     "Насколько пол отражает; под дождём больше."},
    {"Light in the air",
     "Світло в повітрі",
     "Свет в воздухе"},
    {"The air glows around torches, lamps and burning skills, as in a light mist (walls in front hide it); on the "
     "fields rays of light stream from the sky past the edge of the map. Direct3D 12.",
     "Повітря світиться навколо факелів, ламп і палаючих умінь, як у легкому серпанку (стіни попереду його ховають); "
     "на полях промені світла ллються з неба за краєм карти. Direct3D 12.",
     "Воздух светится вокруг факелов, ламп и горящих умений, как в лёгкой дымке (стены впереди его прячут); на полях "
     "лучи света льются с неба за краем карты. Direct3D 12."},
    {"Light in the air strength",
     "Сила світла в повітрі",
     "Сила света в воздухе"},
    {"How bright the glowing air and the rays are.",
     "Наскільки яскраві повітря, що світиться, і промені.",
     "Насколько ярки светящийся воздух и лучи."},
    {"Time of day",
     "Час доби",
     "Время суток"},
    {"By the computer's clock the sun moves: the characters' shadows turn and are long in the morning and the "
     "evening, short at noon; the light is pink in the morning and golden in the evening. With \"Night by the clock\" "
     "the day ends in night.",
     "За годинником комп’ютера сонце рухається: тіні персонажів повертаються, довгі вранці й увечері, короткі "
     "опівдні; світло рожеве вранці й золоте ввечері. Разом із «Ніч за годинником» день переходить у ніч.",
     "По часам компьютера солнце движется: тени персонажей поворачиваются, длинные утром и вечером, короткие в "
     "полдень; свет розовый утром и золотой вечером. Вместе с «Ночь по часам» день переходит в ночь."},
    {"TAA (any graphics card)",
     "TAA (будь-яка відеокарта)",
     "TAA (любая видеокарта)"},
    {"Temporal anti-aliasing of the renderer's own for cards without DLSS (AMD, Intel): each frame is blended with "
     "the last, so edges and thin lines stop shimmering. Direct3D 12; DLSS goes first; MSAA is not used with it.",
     "Власне часове згладжування рендера для карт без DLSS (AMD, Intel): кожен кадр змішується з попереднім, тож краї "
     "й тонкі лінії перестають мерехтіти. Direct3D 12; DLSS має перевагу; MSAA разом із ним не використовується.",
     "Собственное временное сглаживание рендера для карт без DLSS (AMD, Intel): каждый кадр смешивается с предыдущим, "
     "поэтому края и тонкие линии перестают мерцать. Direct3D 12; DLSS в приоритете; MSAA вместе с ним не "
     "используется."},
    {"Keep the frame rate",
     "Тримати частоту кадрів",
     "Держать частоту кадров"},
    {"When the game runs below this many frames a second for ten seconds, the costliest effect is turned off for the "
     "rest of the game (the log says which), then the next if needed. The settings file is not changed. 0 = never.",
     "Якщо гра десять секунд іде повільніше за стільки кадрів на секунду, найдорожчий ефект вимикається до кінця гри "
     "(лог каже, який), за потреби наступний. Файл налаштувань не змінюється. 0 — ніколи.",
     "Если игра десять секунд идёт медленнее стольких кадров в секунду, самый дорогой эффект выключается до конца "
     "игры (лог говорит, какой), при необходимости следующий. Файл настроек не меняется. 0 — никогда."},
    {" · ~{1} ms",
     " · ~{1} мс",
     " · ~{1} мс"},
    {"Shadows of houses and trees",
     "Тіні будинків і дерев",
     "Тени домов и деревьев"},
    {"On the fields and in the towns houses, walls and trees cast shadows on the ground away from the sun; with "
     "\"Time of day\" they turn and lengthen. Direct3D 12.",
     "На полях і в містах будинки, стіни й дерева відкидають тіні на землю від сонця; із «Часом доби» вони "
     "повертаються й видовжуються. Direct3D 12.",
     "На полях и в городах дома, стены и деревья отбрасывают тени на землю от солнца; со «Временем суток» они "
     "поворачиваются и удлиняются. Direct3D 12."},
    {"Shadow darkness",
     "Темрява тіней",
     "Темнота теней"},
    {"How dark the shadows are.",
     "Наскільки темні тіні.",
     "Насколько темны тени."},
    {"Bloom of bright places",
     "Сяйво яскравих місць",
     "Свечение ярких мест"},
    {"The brightest parts of the picture - sunlit tiles, snow, windows, fire - glow softly around. Direct3D 12.",
     "Найяскравіші частини картинки — освітлена сонцем плитка, сніг, вікна, вогонь — м’яко світяться навколо. "
     "Direct3D 12.",
     "Самые яркие части картинки — освещённая солнцем плитка, снег, окна, огонь — мягко светятся вокруг. Direct3D 12."},
    {"Bloom strength",
     "Сила сяйва",
     "Сила свечения"},
    {"How much the bright places glow.",
     "Наскільки сяють яскраві місця.",
     "Насколько светятся яркие места."},
    {"Film-like colours",
     "Кольори як у кіно",
     "Цвета как в кино"},
    {"A film camera's curve over the picture: the bright parts roll off softly instead of turning flat white, the "
     "shadows keep their depth. Direct3D 12.",
     "Крива кінокамери над картинкою: яскраві місця м’яко згасають, а не стають пласко-білими, тіні зберігають "
     "глибину. Direct3D 12.",
     "Кривая кинокамеры поверх картинки: яркие места мягко сходят на нет, а не становятся плоско-белыми, тени "
     "сохраняют глубину. Direct3D 12."},
    {"Colours of each region",
     "Кольори кожного регіону",
     "Цвета каждого региона"},
    {"Each region gets a colour mood of its own: Payon warm and green, Niflheim cold and pale, Morroc sun-bleached, "
     "Lutie blue and clean, Glast Heim grim, Einbroch smoky.",
     "Кожен регіон має власний настрій кольорів: Пайон теплий і зелений, Ніфльгейм холодний і блідий, Моррок "
     "вигорілий на сонці, Лютіє блакитний і чистий, Гласт Гейм похмурий, Ейнброх задимлений.",
     "У каждого региона своё настроение цветов: Пайон тёплый и зелёный, Нифльхейм холодный и бледный, Морок "
     "выгоревший на солнце, Лютие голубой и чистый, Гласт Хейм мрачный, Эйнброх задымлённый."},
    {"Region colours strength",
     "Сила кольорів регіону",
     "Сила цветов региона"},
    {"How strong the region's colour mood is.",
     "Наскільки сильний настрій кольорів регіону.",
     "Насколько сильно настроение цветов региона."},
    {"Footprints and dust",
     "Сліди й пил",
     "Следы и пыль"},
    {"Characters leave footprints in the snow and the sand that slowly fade, and raise a little dust as they walk on "
     "dry fields. Needs the characters' shadows. Direct3D 12.",
     "Персонажі лишають сліди на снігу й піску, що поволі зникають, а на сухих полях здіймають трохи пилу. Потрібні "
     "тіні персонажів. Direct3D 12.",
     "Персонажи оставляют следы на снегу и песке, которые понемногу исчезают, а на сухих полях поднимают немного "
     "пыли. Нужны тени персонажей. Direct3D 12."},
    {"Thunderstorms",
     "Грози",
     "Грозы"},
    {"Where it rains (with the weather on), lightning flashes now and then and lights up everything for a moment. "
     "Direct3D 12.",
     "Де йде дощ (з увімкненою погодою), час від часу спалахує блискавка й на мить освітлює все. Direct3D 12.",
     "Где идёт дождь (с включённой погодой), время от времени сверкает молния и на миг освещает всё. Direct3D 12."},
    {"Grass",
     "Трава",
     "Трава"},
    {"Blades of grass grow on the green ground of the fields and towns, sway in the wind and lie down where "
     "characters walk. Costly for the graphics card. Direct3D 12.",
     "На зеленій землі полів і міст ростуть травинки, гойдаються на вітрі й приминаються там, де йдуть персонажі. "
     "Дорого для відеокарти. Direct3D 12.",
     "На зелёной земле полей и городов растут травинки, качаются на ветру и приминаются там, где идут персонажи. "
     "Дорого для видеокарты. Direct3D 12."},
    {"How thick the grass is",
     "Наскільки густа трава",
     "Насколько густая трава"},
    {"How many blades grow.",
     "Скільки травинок росте.",
     "Сколько травинок растёт."},
    {"Sparks of skills",
     "Іскри вмінь",
     "Искры умений"},
    {"Embers rise from burning skills (Fire Wall, Fire Bolt, Meteor Storm), and glints of their colour spread from "
     "the other bright skills. Needs the skills' light. Direct3D 12.",
     "Від палаючих умінь (Fire Wall, Fire Bolt, Meteor Storm) здіймаються жаринки, а від інших яскравих умінь "
     "розлітаються іскорки їхнього кольору. Потрібне світло вмінь. Direct3D 12.",
     "От горящих умений (Fire Wall, Fire Bolt, Meteor Storm) поднимаются угольки, а от других ярких умений "
     "разлетаются искорки их цвета. Нужен свет умений. Direct3D 12."},
    {"Sparks brightness",
     "Яскравість іскор",
     "Яркость искр"},
    {"How bright the sparks are.",
     "Наскільки яскраві іскри.",
     "Насколько ярки искры."},
    {"Film grain",
     "Зерно плівки",
     "Зерно плёнки"},
    {"A fine noise over the picture, as on film; less in the bright parts. Direct3D 12.",
     "Дрібний шум поверх картинки, як на плівці; менше в яскравих місцях. Direct3D 12.",
     "Мелкий шум поверх картинки, как на плёнке; меньше в ярких местах. Direct3D 12."},
    {"Grain strength",
     "Сила зерна",
     "Сила зерна"},
    {"How strong the grain is.",
     "Наскільки сильне зерно.",
     "Насколько сильно зерно."},
    {"Smooth map change",
     "Плавна зміна карти",
     "Плавная смена карты"},
    {"After a warp or a new map the world comes in smoothly from black instead of at once. Direct3D 12.",
     "Після телепорту чи нової карти світ плавно проявляється з темряви, а не одразу. Direct3D 12.",
     "После телепорта или новой карты мир плавно проявляется из темноты, а не сразу. Direct3D 12."},
    {"Round characters",
     "Об’ємні персонажі",
     "Объёмные персонажи"},
    {"Torches, lamps and fire light a character as if it were round: the side towards the fire brighter, the far one "
     "darker.",
     "Факели, лампи й вогонь освітлюють персонажа як округлого: бік до вогню світліший, дальній темніший.",
     "Факелы, лампы и огонь освещают персонажа как округлого: сторона к огню светлее, дальняя темнее."},
    {"Save power in the background",
     "Економити енергію у фоні",
     "Экономить энергию в фоне"},
    {"While another window is in front or the game is minimised it draws 15 frames a second; on the login and the "
     "character select 30.",
     "Поки попереду інше вікно або гру згорнуто, вона малює 15 кадрів на секунду; на вході й виборі персонажа — 30.",
     "Пока впереди другое окно или игра свёрнута, она рисует 15 кадров в секунду; на входе и выборе персонажа — 30."},
    {"Measure where the game spends its time",
     "Заміряти, на що гра витрачає час",
     "Замерять, на что игра тратит время"},
    {"For the developers: about a thousand times a second the driver notes where uaRO.exe is busy and writes the "
     "busiest places into the log every 10 seconds. Costs a little speed - turn it off afterwards.",
     "Для розробників: приблизно тисячу разів на секунду драйвер помічає, чим зайнятий uaRO.exe, і кожні 10 секунд "
     "пише в лог найзавантаженіші місця. Трохи сповільнює — потім вимкніть.",
     "Для разработчиков: примерно тысячу раз в секунду драйвер отмечает, чем занят uaRO.exe, и каждые 10 секунд пишет "
     "в лог самые загруженные места. Немного замедляет — потом выключите."},
    {"Sound through uaRO Graphics",
     "Звук через uaRO Graphics",
     "Звук через uaRO Graphics"},
    {"Sound quality",
     "Якість звуку",
     "Качество звука"},
    {"The game mixes its sound at 22050 Hz at most - the music, recorded at 44100 Hz, loses its highs. 44100 or 48000 "
     "Hz plays it as it was recorded.",
     "Гра змішує звук щонайбільше на 22050 Гц - музика, записана на 44100 Гц, втрачає високі частоти. 44100 чи 48000 "
     "Гц грають її такою, як записано.",
     "Игра смешивает звук максимум на 22050 Гц - музыка, записанная на 44100 Гц, теряет высокие частоты. 44100 или "
     "48000 Гц играют её такой, как записано."},
    {"The game's (setup.exe)",
     "Як у грі (setup.exe)",
     "Как в игре (setup.exe)"},
    {"44100 Hz",
     "44100 Гц",
     "44100 Гц"},
    {"48000 Hz",
     "48000 Гц",
     "48000 Гц"},
    {"The most sounds at once",
     "Найбільше звуків одночасно",
     "Больше всего звуков одновременно"},
    {"48 sounds at once whatever setup.exe says (16 or 32 there): skills in a crowd are not cut short.",
     "48 звуків одночасно, хоч би що стояло в setup.exe (там 16 чи 32): уміння в натовпі не обриваються.",
     "48 звуков одновременно, что бы ни стояло в setup.exe (там 16 или 32): умения в толпе не обрываются."},
    {"Volume in the background",
     "Гучність у фоні",
     "Громкость в фоне"},
    {"While another window is in front or the game is minimised: 0 is silence, 100 as loud as in front.",
     "Коли попереду інше вікно або гру згорнуто: 0 - тиша, 100 - так само гучно, як попереду.",
     "Когда впереди другое окно или игра свёрнута: 0 - тишина, 100 - так же громко, как впереди."},
    {"Even out the loudness in a crowd",
     "Вирівнювати гучність у натовпі",
     "Выравнивать громкость в толпе"},
    {"Past six sounds at once each is made a little quieter, so that many skills together do not crackle.",
     "Понад шість звуків одночасно кожен стає трохи тихішим, щоб багато вмінь разом не хрипіли.",
     "Больше шести звуков одновременно - каждый становится чуть тише, чтобы много умений вместе не хрипели."},
    {"Left and right",
     "Ліворуч і праворуч",
     "Слева и справа"},
    {"A sound beside your character comes from that side of the screen: how far to the side. 0 is the game's "
     "(everything in the middle).",
     "Звук збоку від персонажа лунає з того боку екрана: наскільки сильно. 0 - як у грі (усе посередині).",
     "Звук сбоку от персонажа звучит с той стороны экрана: насколько сильно. 0 - как в игре (всё посередине)."},
    {"Never lose a new sound",
     "Не губити нові звуки",
     "Не терять новые звуки"},
    {"When every sound channel is busy the quietest (the farthest) stops for the new one - the game dropped the new "
     "one.",
     "Коли всі звукові канали зайняті, найтихіший (найдальший) зупиняється заради нового - гра відкидала новий.",
     "Когда все звуковые каналы заняты, самый тихий (самый дальний) останавливается ради нового - игра отбрасывала "
     "новый."},
    {"Smooth change of music",
     "Плавна зміна музики",
     "Плавная смена музыки"},
    {"Moving to another map the old music fades out and the new one in over 1.5 seconds instead of breaking off.",
     "Під час переходу на іншу мапу стара музика затихає, а нова наростає за 1,5 секунди, а не обривається.",
     "При переходе на другую карту старая музыка затихает, а новая нарастает за 1,5 секунды, а не обрывается."},
    {"Fade in monsters and items",
     "Плавна поява монстрів і предметів",
     "Плавное появление монстров и предметов"},
    {"Monsters, characters, NPCs and items on the ground fade in over a quarter of a second where they come into "
     "view, instead of popping up.",
     "Монстри, персонажі, NPC і предмети на землі з’являються за чверть секунди, а не виникають миттєво.",
     "Монстры, персонажи, NPC и предметы на земле появляются за четверть секунды, а не возникают мгновенно."},
    {"Ripples from steps",
     "Кола від кроків",
     "Круги от шагов"},
    {"Rings spread on the water where characters and monsters walk through it (with the modern water, Direct3D 12).",
     "По воді розходяться кола там, де крізь неї йдуть персонажі й монстри (із сучасною водою, Direct3D 12).",
     "По воде расходятся круги там, где через неё идут персонажи и монстры (с современной водой, Direct3D 12)."},
    {"Cards on the ground sparkle",
     "Картки на землі виблискують",
     "Карты на земле поблёскивают"},
    {"A card that dropped flashes a small star now and then, so that it is not missed among the loot.",
     "Картка, що випала, час від часу спалахує маленькою зіркою, щоб її не пропустити серед здобичі.",
     "Выпавшая карта время от времени вспыхивает маленькой звёздочкой, чтобы её не пропустить среди добычи."},
    {"Flash on a hit",
     "Спалах від удару",
     "Вспышка при ударе"},
    {"A monster that is hit lights up white for a moment, as in today's 2D games. Needs the game's sound on: the hit "
     "is known by its sound.",
     "Монстр, по якому влучили, на мить спалахує білим, як у сучасних 2D-іграх. Потрібен увімкнений звук гри: удар "
     "впізнається за звуком.",
     "Монстр, по которому попали, на мгновение вспыхивает белым, как в современных 2D-играх. Нужен включённый звук "
     "игры: удар узнаётся по звуку."},
    {"Wet ground in the rain",
     "Мокра земля в дощ",
     "Мокрая земля в дождь"},
    {"On maps where it rains the ground is a little darker and puddles here and there mirror the houses, the trees "
     "and the characters (with the weather on, Direct3D 12).",
     "На мапах, де йде дощ, земля трохи темніша, а калюжі подекуди віддзеркалюють будинки, дерева й персонажів (з "
     "увімкненою погодою, Direct3D 12).",
     "На картах, где идёт дождь, земля чуть темнее, а лужи кое-где отражают дома, деревья и персонажей (с включённой "
     "погодой, Direct3D 12)."},
    {"Sun flare",
     "Відблиск сонця",
     "Блик солнца"},
    {"Under the open sky in the day a soft glow and a few faint rings of the sun's light cross the screen where the "
     "sky shows towards the sun (Direct3D 12).",
     "Просто неба вдень м'яке сяйво й кілька ледь помітних кілець сонячного світла перетинають екран там, де до сонця "
     "видно небо (Direct3D 12).",
     "Под открытым небом днём мягкое сияние и несколько едва заметных колец солнечного света пересекают экран там, "
     "где к солнцу видно небо (Direct3D 12)."},
    {"Surround sound (new engine)",
     "Об'ємний звук (новий рушій)",
     "Объёмный звук (новый движок)"},
    {"The sounds of monsters, skills and hits played by uaRO Graphics itself: around you on any speakers (stereo, "
     "5.1, 7.1), quieter and duller far off, with the echo of the place. Windows 10 and later; without it the game's "
     "own sound.",
     "Звуки монстрів, умінь і ударів грає сам uaRO Graphics: навколо вас на будь-яких колонках (стерео, 5.1, 7.1), "
     "далекі тихіші й глухіші, з відлунням місця. Windows 10 і новіші; без цього — власний звук гри.",
     "Звуки монстров, умений и ударов играет сам uaRO Graphics: вокруг вас на любых колонках (стерео, 5.1, 7.1), "
     "дальние тише и глуше, с эхом места. Windows 10 и новее; без этого — собственный звук игры."},
    {"Echo of the place",
     "Відлуння місця",
     "Эхо места"},
    {"With the new engine: caves and dungeons echo long, houses short, towns a little, fields hardly. 0 is none.",
     "З новим рушієм: печери й підземелля відлунюють довго, будинки коротко, міста трохи, поля майже ні. 0 — без "
     "відлуння.",
     "С новым движком: пещеры и подземелья отдаются долгим эхом, дома коротким, города немного, поля почти нет. 0 — "
     "без эха."},
    {"Headphones: Windows' spatial sound",
     "Навушники: просторовий звук Windows",
     "Наушники: пространственный звук Windows"},
    {"With the new engine, when Windows Sonic for Headphones or Dolby Atmos is turned on in Windows (the speaker's "
     "properties, Spatial sound): the sounds placed in three dimensions - behind, in front, beside - in headphones "
     "too.",
     "З новим рушієм, коли у Windows увімкнено Windows Sonic для навушників або Dolby Atmos (властивості динаміка, "
     "«Просторовий звук»): звуки розставлено в трьох вимірах — позаду, попереду, збоку — і в навушниках.",
     "С новым движком, когда в Windows включён Windows Sonic для наушников или Dolby Atmos (свойства динамика, "
     "«Пространственный звук»): звуки расставлены в трёх измерениях — сзади, спереди, сбоку — и в наушниках."},
    {"Music makes way in a fight",
     "Музика поступається в бою",
     "Музыка уступает в бою"},
    {"While many sounds play at once (a fight, a crowd's skills) the music goes quieter, down to 65 %, and comes back "
     "over two seconds.",
     "Коли одночасно звучить багато звуків (бій, уміння натовпу), музика тихішає до 65 % і за дві секунди "
     "повертається.",
     "Когда одновременно звучит много звуков (бой, умения толпы), музыка становится тише, до 65 %, и за две секунды "
     "возвращается."},
    {"The interface at {1} % - a whole number of times the game's pixels, the crispest letters.",
     "Інтерфейс {1} % - ціле збільшення пікселів гри, найчіткіші літери.",
     "Интерфейс {1} % - целое увеличение пикселей игры, самые чёткие буквы."},
    {"Smoothing: NVIDIA DLAA (the picture at its own size).",
     "Згладжування: NVIDIA DLAA (картинка у власному розмірі).",
     "Сглаживание: NVIDIA DLAA (картинка в собственном размере)."},
    {"Smoothing: NVIDIA DLSS Quality.",
     "Згладжування: NVIDIA DLSS Quality.",
     "Сглаживание: NVIDIA DLSS Quality."},
    {"HDR: on (the monitor shows it).",
     "HDR: увімкнено (монітор його показує).",
     "HDR: включено (монитор его показывает)."},
    {"Sound: around you on any speakers; with Windows Sonic in headphones too.",
     "Звук: навколо вас на будь-яких колонках; з Windows Sonic і в навушниках.",
     "Звук: вокруг вас на любых колонках; с Windows Sonic и в наушниках."},
    {"Gamepad",
     "Геймпад",
     "Геймпад"},
    {"uaRO Pad: playing with a gamepad",
     "uaRO Pad: гра з геймпадом",
     "uaRO Pad: игра с геймпадом"},
    {"Xbox controllers and PlayStation ones (DualShock 4, DualSense - over USB or Bluetooth) play the game: the stick "
     "walks, the buttons attack the nearest monster and use the skills of the hotkey bar, D-pad down talks to the "
     "nearest NPC and the buttons answer its windows. Start on the controller opens its menu in the game.",
     "Геймпади Xbox і PlayStation (DualShock 4, DualSense - через USB або Bluetooth) керують грою: стік веде "
     "персонажа, кнопки атакують найближчого монстра і застосовують уміння з панелі швидкого доступу, D-pad вниз "
     "говорить із найближчим NPC, а кнопки відповідають його вікнам. Start на геймпаді відкриває його меню в грі.",
     "Геймпады Xbox и PlayStation (DualShock 4, DualSense - по USB или Bluetooth) управляют игрой: стик ведёт "
     "персонажа, кнопки атакуют ближайшего монстра и применяют умения с панели быстрого доступа, D-pad вниз говорит с "
     "ближайшим NPC, а кнопки отвечают его окнам. Start на геймпаде открывает его меню в игре."},
    {"Off: there is no dinput.dll in the game's folder.",
     "Вимкнено: у теці гри немає dinput.dll.",
     "Выключено: в папке игры нет dinput.dll."},
    {"On: uaRO Pad {1}.",
     "Увімкнено: uaRO Pad {1}.",
     "Включено: uaRO Pad {1}."},
    {"On, an older uaRO Pad: Update puts in {1}.",
     "Увімкнено, старіша версія uaRO Pad: «Оновити» поставить {1}.",
     "Включено, более старая версия uaRO Pad: «Обновить» поставит {1}."},
    {"Off. The game's folder has another dinput.dll (ROExt, as the client comes): with the pad on it becomes "
     "dinput8.dll and keeps working, switching the pad off puts it back.",
     "Вимкнено. У теці гри інший dinput.dll (ROExt, як у клієнті): з геймпадом він стане dinput8.dll і працюватиме "
     "далі, вимкнення геймпада поверне його на місце.",
     "Выключено. В папке игры другой dinput.dll (ROExt, как в клиенте): с геймпадом он станет dinput8.dll и будет "
     "работать дальше, выключение геймпада вернёт его на место."},
    {"Switch on",
     "Увімкнути",
     "Включить"},
    {"Switch off",
     "Вимкнути",
     "Выключить"},
    {"Update",
     "Оновити",
     "Обновить"},
    {"uaro-graphics\\pad\\dinput.dll is missing: update uaRO Graphics on the Service page.",
     "Немає uaro-graphics\\pad\\dinput.dll: онови uaRO Graphics на сторінці «Сервіс».",
     "Нет uaro-graphics\\pad\\dinput.dll: обнови uaRO Graphics на странице «Сервис»."},
    {"uaRO Pad is on. Connect the controller and start the game; Start opens the pad's menu.",
     "uaRO Pad увімкнено. Підключи геймпад і запусти гру; Start відкриває меню геймпада.",
     "uaRO Pad включён. Подключи геймпад и запусти игру; Start открывает меню геймпада."},
    {"uaRO Pad is off.",
     "uaRO Pad вимкнено.",
     "uaRO Pad выключен."},
    {"Could not change {1}: close the game, or start uaro-graphics.exe as administrator.",
     "Не вдалося змінити {1}: закрий гру або запусти uaro-graphics.exe від імені адміністратора.",
     "Не удалось изменить {1}: закрой игру или запусти uaro-graphics.exe от имени администратора."},
    {"The game is running: close it to switch the pad on or off.",
     "Гра запущена: закрий її, щоб увімкнути або вимкнути геймпад.",
     "Игра запущена: закрой её, чтобы включить или выключить геймпад."},
    {"Controller",
     "Контролер",
     "Контроллер"},
    {"Controller",
     "Геймпад",
     "Геймпад"},
    {"Any: an Xbox controller if one is connected, else a PlayStation one.",
     "Будь-який: геймпад Xbox, якщо він підключений, інакше PlayStation.",
     "Любой: геймпад Xbox, если он подключён, иначе PlayStation."},
    {"Any (Xbox first)",
     "Будь-який (спершу Xbox)",
     "Любой (сначала Xbox)"},
    {"PlayStation (DualShock 4, DualSense)",
     "PlayStation (DualShock 4, DualSense)",
     "PlayStation (DualShock 4, DualSense)"},
    {"Light bar",
     "Світлова панель",
     "Световая панель"},
    {"The colour of a PlayStation controller's light, over USB and Bluetooth.",
     "Колір світла геймпада PlayStation, через USB і Bluetooth.",
     "Цвет света геймпада PlayStation, по USB и Bluetooth."},
    {"Blue",
     "Синій",
     "Синий"},
    {"Red",
     "Червоний",
     "Красный"},
    {"Green",
     "Зелений",
     "Зелёный"},
    {"Purple",
     "Фіолетовий",
     "Фиолетовый"},
    {"Orange",
     "Помаранчевий",
     "Оранжевый"},
    {"White",
     "Білий",
     "Белый"},
    {"Off",
     "Вимкнено",
     "Выключено"},
    {"Own: {1}",
     "Свій: {1}",
     "Свой: {1}"},
    {"Touchpad click",
     "Натиск на тачпад",
     "Нажатие на тачпад"},
    {"What pressing a PlayStation controller's touchpad does.",
     "Що робить натиск на тачпад геймпада PlayStation.",
     "Что делает нажатие на тачпад геймпада PlayStation."},
    {"Nothing",
     "Нічого",
     "Ничего"},
    {"Back: the pad off and on",
     "Back: вимкнути й увімкнути геймпад",
     "Back: выключить и включить геймпад"},
    {"Start: the pad's menu",
     "Start: меню геймпада",
     "Start: меню геймпада"},
    {"Vibration",
     "Вібрація",
     "Вибрация"},
    {"Short pulses when the target changes or a skill is refused.",
     "Короткі поштовхи, коли змінюється ціль або вміння не спрацювало.",
     "Короткие толчки, когда меняется цель или умение не сработало."},
    {"On when the game starts",
     "Увімкнений на старті гри",
     "Включён при старте игры"},
    {"Off: the pad waits for Back (Share, Create) to switch on.",
     "Вимкнено: геймпад чекає на Back (Share, Create), щоб увімкнутися.",
     "Выключено: геймпад ждёт Back (Share, Create), чтобы включиться."},
    {"Its menu's language",
     "Мова його меню",
     "Язык его меню"},
    {"The language of the pad's menu in the game (Start).",
     "Мова меню геймпада в грі (Start).",
     "Язык меню геймпада в игре (Start)."},
    {"These are saved to uaro-pad.ini at once; the game reads them when it starts. Button layouts and the rest are in "
     "the pad's menu: Start in the game.",
     "Це зберігається в uaro-pad.ini одразу; гра читає їх при запуску. Розкладки кнопок та інше - у меню геймпада: "
     "Start у грі.",
     "Это сохраняется в uaro-pad.ini сразу; игра читает их при запуске. Раскладки кнопок и остальное - в меню "
     "геймпада: Start в игре."},
    {"Hint in NPC windows",
     "Підказка у вікнах NPC",
     "Подсказка в окнах NPC"},
    {"While an NPC window is open, a line at the bottom of the game says what the buttons do there.",
     "Поки відкрите вікно NPC, рядок унизу гри підказує, що там роблять кнопки.",
     "Пока открыто окно NPC, строка внизу игры подсказывает, что там делают кнопки."},
    {"anime 6B (drawn: finer lines, keeps details; 5-7 times slower)",
     "anime 6B (мальоване: тонші лінії, зберігає деталі; у 5-7 разів повільніша)",
     "anime 6B (рисованное: тоньше линии, сохраняет детали; в 5-7 раз медленнее)"},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "a text for every Text");

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

const char* tr(Lang l, Text t) {
    const Entry& e = kTexts[size_t(t)];
    return l == Lang::Uk ? e.uk : l == Lang::Ru ? e.ru : e.en;
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

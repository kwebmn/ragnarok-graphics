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
    {"The frame is drawn larger and shrunk when shown: smoother edges, more work for the graphics card.",
     "Кадр малюється більшим і зменшується при показі: гладші краї, відеокарта працює більше.",
     "Кадр рисуется больше и уменьшается при показе: глаже края, видеокарта работает больше."},
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
    {"While you play, a neural network (Real-ESRGAN) on your graphics card redraws the game's textures larger and "
     "sharper, a little every frame; done ones go into upscaled.grf and show at once next time. Pause shows the "
     "game's own. Any card with Direct3D 12; it starts the 64-bit renderer.",
     "Поки ви граєте, нейромережа (Real-ESRGAN) на вашій відеокарті перемальовує текстури гри більшими й чіткішими, "
     "потроху щокадру; готові йдуть в upscaled.grf і наступного разу з'являються одразу. Pause показує власні "
     "текстури гри. Будь-яка відеокарта з Direct3D 12; вмикає 64-бітний рендер.",
     "Пока вы играете, нейросеть (Real-ESRGAN) на вашей видеокарте перерисовывает текстуры игры крупнее и чётче, "
     "понемногу каждый кадр; готовые идут в upscaled.grf и в следующий раз появляются сразу. Pause показывает "
     "родные текстуры игры. Любая видеокарта с Direct3D 12; включает 64-битный рендер."},
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
    {"Monsters, pets, homunculi and mercenaries. Their animation frames are enlarged one by one as they come (a new "
     "monster within a second or two, then at once from upscaled.grf). Small pixel pictures: the network may change "
     "their look - try it and see.",
     "Монстри, вихованці, гомункули й найманці. Кадри їхніх анімацій покращуються по одному, щойно з'являться "
     "(новий монстр — за секунду-дві, далі одразу з upscaled.grf). Це маленькі піксельні картинки: нейромережа може "
     "змінити їхній вигляд — спробуйте й подивіться.",
     "Монстры, питомцы, гомункулы и наёмники. Кадры их анимаций улучшаются по одному, как только появятся (новый "
     "монстр — за секунду-две, дальше сразу из upscaled.grf). Это маленькие пиксельные картинки: нейросеть может "
     "изменить их вид — попробуйте и посмотрите."},
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
     "about 1.4 MB - for cards with 4 GB or less; upscaled.grf grows 4 times slower too.",
     "×4: текстура 256×256 стає 1024×1024 — близько 5,6 МБ відеопам'яті з меншими копіями. ×2: 512×512, близько "
     "1,4 МБ — для відеокарт із 4 ГБ і менше; upscaled.grf теж росте вчетверо повільніше.",
     "×4: текстура 256×256 становится 1024×1024 — около 5,6 МБ видеопамяти с уменьшенными копиями. ×2: 512×512, "
     "около 1,4 МБ — для видеокарт с 4 ГБ и меньше; upscaled.grf тоже растёт вчетверо медленнее."},
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
    {"Textures seen on a map before are taken from upscaled.grf (or made again after a change of settings) as soon "
     "as the map loads, not when they come into view: fewer of them sharpen before your eyes.",
     "Текстури, які вже траплялися на карті, беруться з upscaled.grf (або робляться знову після зміни налаштувань) "
     "щойно карта завантажилась, а не коли потраплять у кадр: менше «проявлення» на очах.",
     "Текстуры, которые уже встречались на карте, берутся из upscaled.grf (или делаются заново после смены "
     "настроек) сразу после загрузки карты, а не когда попадут в кадр: меньше «проявления» на глазах."},
    {"Tick the box above and the network starts improving textures while you play. The Ready textures page shows "
     "and deletes what it has made.",
     "Поставте галочку вище — і нейромережа почне покращувати текстури, поки ви граєте. Сторінка «Готові текстури» "
     "показує й видаляє зроблене.",
     "Поставьте галочку выше — и нейросеть начнёт улучшать текстуры, пока вы играете. Страница «Готовые текстуры» "
     "показывает и удаляет сделанное."},
    {"upscaled.grf: {1} textures of these settings, {2}. The file: {3} of the 4 GB the format allows.",
     "upscaled.grf: текстур із цими налаштуваннями — {1}, {2}. Файл: {3} з 4 ГБ, які дозволяє формат.",
     "upscaled.grf: текстур с этими настройками — {1}, {2}. Файл: {3} из 4 ГБ, которые позволяет формат."},
    {"There is no upscaled.grf yet: the network makes it once it starts improving textures.",
     "upscaled.grf ще немає: нейромережа створить його, коли почне покращувати текстури.",
     "upscaled.grf пока нет: нейросеть создаст его, когда начнёт улучшать текстуры."},
    {"upscaled.grf cannot be read: {1}", "upscaled.grf не читається: {1}", "upscaled.grf не читается: {1}"},
    {"Map", "Карта", "Карта"},
    {"Ground", "Земля", "Земля"},
    {"Buildings", "Будівлі", "Здания"},
    {"Characters", "Персонажі", "Персонажи"},
    {"Effects", "Ефекти", "Эффекты"},
    {"Total", "Усього", "Всего"},
    {"Size", "Розмір", "Размер"},
    {"All maps", "Усі карти", "Все карты"},
    {"no map (icons; made before 0.12)", "без карти (іконки; зроблені до 0.12)",
     "без карты (иконки; сделаны до 0.12)"},
    {"Delete from the ticked maps:", "Видаляти з позначених карт:", "Удалять с отмеченных карт:"},
    {"Delete ticked", "Видалити позначене", "Удалить отмеченное"},
    {"Delete outdated: {1}", "Видалити застарілі: {1}", "Удалить устаревшие: {1}"},
    {"Delete all", "Видалити все", "Удалить всё"},
    {"Outdated: made with another model, smoothing or size, or by an older version - the game no longer uses them. "
     "A texture seen on several maps is on each of their rows. Deleted ones are made again when they turn up in the "
     "game.",
     "Застарілі — зроблені з іншою моделлю, згладжуванням чи розміром або старішою версією: гра їх більше не "
     "використовує. Текстура, що траплялася на кількох картах, є в кожному їхньому рядку. Видалені робляться знову, "
     "коли трапляться в грі.",
     "Устаревшие — сделанные с другой моделью, сглаживанием или размером или старой версией: игра их больше не "
     "использует. Текстура, встречавшаяся на нескольких картах, есть в каждой их строке. Удалённые делаются снова, "
     "когда попадутся в игре."},
    {"The game is running: textures can be deleted once it is closed (it keeps the file open).",
     "Гра запущена: видаляти можна, коли вона закрита (вона тримає файл відкритим).",
     "Игра запущена: удалять можно, когда она закрыта (она держит файл открытым)."},
    {"Delete the ticked textures from upscaled.grf ({1})? They are made again when they turn up in the game.",
     "Видалити позначені текстури з upscaled.grf ({1} шт.)? Вони зробляться знову, коли трапляться в грі.",
     "Удалить отмеченные текстуры из upscaled.grf ({1} шт.)? Они сделаются снова, когда попадутся в игре."},
    {"Delete the whole upscaled.grf ({1})? Every texture is made again when it turns up in the game.",
     "Видалити весь upscaled.grf ({1})? Усі текстури зробляться знову, коли трапляться в грі.",
     "Удалить весь upscaled.grf ({1})? Все текстуры сделаются снова, когда попадутся в игре."},
    {"Deleted textures: {1}. upscaled.grf is {2} now.", "Видалено текстур: {1}. upscaled.grf тепер {2}.",
     "Удалено текстур: {1}. upscaled.grf теперь {2}."},
    {"upscaled.grf is deleted.", "upscaled.grf видалено.", "upscaled.grf удалён."},
    {"Nothing to delete: tick maps in the table first.", "Нічого видаляти: спершу позначте карти в таблиці.",
     "Нечего удалять: сначала отметьте карты в таблице."},
    {"upscaled.grf could not be changed: {1}", "Не вдалося змінити upscaled.grf: {1}",
     "Не удалось изменить upscaled.grf: {1}"},
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

Lang langForWindows(uint16_t langId) {
    const uint16_t primary = langId & 0x3FF;
    if (primary == 0x22) return Lang::Uk;
    if (primary == 0x19) return Lang::Ru;
    return Lang::En;
}

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

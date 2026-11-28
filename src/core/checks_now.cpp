#include "core/checks_now.hpp"

#include <algorithm>
#include <sstream>

namespace uagfx::checksnow {

const std::vector<Check>& all() {
    static const std::vector<Check> list = {
        // 3.0.24-beta.41 (the laptop's effects run): the remaster mode's own rows say so in «Класика+»
        {"remaster_rows_41", "3.0.24-beta.41", "",
         {"Effects: the remaster mode's own rows say so", "Ефекти: рядки лише для «Ремастеру» кажуть це",
          "Эффекты: строки только для «Ремастера» говорят это"},
         {"Alt+O → «Picture» → «Mode» «Classic+ (any card)». «Fire and smoke» → «Shadows from torches and lamps» and "
          "«Water» → «World reflections in water»: each row greyed, «Remaster mode only» under it. «Mode» "
          "«Remaster (a strong card)»: the line gone, the shadows by the torches (moc_pryd01) drawn.",
          "Alt+O → «Картинка» → «Режим» «Класика+ (будь-яка відеокарта)». «Вогонь і дим» → «Тіні від факелів і ламп» "
          "і «Вода» → «Відображення світу у воді»: рядки сірі, під ними «Лише в режимі «Ремастер»». «Режим» «Ремастер "
          "(сильна відеокарта)»: рядка немає, тіні від факелів (moc_pryd01) малюються.",
          "Alt+O → «Картинка» → «Режим» «Классика+ (любая видеокарта)». «Огонь и дым» → «Тени от факелов и ламп» и "
          "«Вода» → «Отражения мира в воде»: строки серые, под ними «Только в режиме «Ремастер»». «Режим» «Ремастер "
          "(сильная видеокарта)»: строки нет, тени от факелов (moc_pryd01) рисуются."}},
        // 3.0.24-beta.38 (the owner: «оба»): keep_fps lowers MSAA first
        {"keep_fps_msaa_38", "3.0.24-beta.38", "frames.keep_fps",
         {"Keep the frame rate: smoothing lowered first", "Тримати частоту кадрів: спершу менше згладжування",
          "Держать частоту кадров: сначала меньше сглаживания"},
         {"Direct3D 12, Alt+O → «Speed and screen» → «Smoothing» → «Edge smoothing (MSAA)» 4×, «Frames» → «Keep the "
          "frame rate» on. On a map where the frames fall under it (Yuno, Morroc on a weak card): within 10-20 s "
          "«Smoothing (MSAA) 4× → 2× for the rest of the game», then «→ Off»; an effect goes off only after that.",
          "Direct3D 12, Alt+O → «Швидкість і екран» → «Згладжування» → «Згладжування країв (MSAA)» 4×, «Кадри» → "
          "«Тримати частоту кадрів» увімкнено. На карті, де кадрів менше (Юно, Морок на слабкій відеокарті): за "
          "10-20 с «Згладжування (MSAA) 4× → 2× до кінця гри», потім «→ Вимкнено»; ефект вимикається лише після цього.",
          "Direct3D 12, Alt+O → «Скорость и экран» → «Сглаживание» → «Сглаживание краёв (MSAA)» 4×, «Кадры» → "
          "«Держать частоту кадров» включено. На карте, где кадров меньше (Юно, Морок на слабой видеокарте): за "
          "10-20 с «Сглаживание (MSAA) 4× → 2× до конца игры», потом «→ Выключено»; эффект выключается только после "
          "этого."}},
        // 3.0.24-beta.36 (the laptop's run): uaro-render64.exe's shaders past the game's 90 s - kept now, waited for
        {"start_slow_pc_36", "3.0.24-beta.36", "",
         {"Start on a slow computer: the second one quick", "Старт на слабкому комп'ютері: другий швидкий",
          "Старт на слабом компьютере: второй быстрый"},
         {"Where the game took a minute and a half or more to show its picture (a weak laptop): start it, wait for "
          "«Compiling shaders: N of M» to end (once, it may take two or three minutes), close it and start it again - "
          "the picture in a few seconds.",
          "Там, де гра показувала картинку півтори хвилини й довше (слабкий ноутбук): запустіть, дочекайтеся кінця "
          "«Збираємо шейдери: N з M» (один раз, може тривати дві-три хвилини), закрийте й запустіть знову — "
          "картинка за кілька секунд.",
          "Там, где игра показывала картинку полторы минуты и дольше (слабый ноутбук): запустите, дождитесь конца "
          "«Собираем шейдеры: N из M» (один раз, может длиться две-три минуты), закройте и запустите снова — "
          "картинка за несколько секунд."}},
        // 3.0.24-beta.36 (the laptop's run): «Навички»'s names and their lines up to the level
        {"skill_names_36", "3.0.24-beta.36", "",
         {"Skills: the names whole", "Навички: назви цілі", "Навыки: названия целые"},
         {"Alt+S: the skills' names and the lines under them (Increase Weight Limit, «Quest - not learned with "
          "points») whole up to the level on the right, nothing cut short beside an empty room.",
          "Alt+S: назви навичок і рядки під ними (Increase Weight Limit, «Квест — очками не вчиться») цілі аж до "
          "рівня праворуч, нічого не обрізано поруч із порожнім місцем.",
          "Alt+S: названия навыков и строки под ними (Increase Weight Limit, «Квест — очками не изучается») целые до "
          "уровня справа, ничего не обрезано рядом с пустым местом."}},
        // 3.0.24-beta.34 (R-AVW884): «Речі»'s vertical tabs their column's width, the word in the frame's middle
        {"bag_tabs_34", "3.0.24-beta.34", "interface.items",
         {"Items: the tabs on the left whole", "Речі: вкладки зліва цілі", "Вещи: вкладки слева целые"},
         {"Alt+E, then each tab on the left: its frame as wide as the column, its word in the frame's middle; the "
          "chosen one's gold frame round its word (100 % and a larger interface alike).",
          "Alt+E, потім кожна вкладка зліва: рамка на всю ширину колонки, слово посередині рамки; золота рамка "
          "вибраної — довкола її слова (і при 100 %, і при більшому інтерфейсі).",
          "Alt+E, затем каждая вкладка слева: рамка на всю ширину колонки, слово посередине рамки; золотая рамка "
          "выбранной — вокруг её слова (и при 100 %, и при большем интерфейсе)."}},
        // 3.0.24-beta.33 (R-8BCJAH, R-VMTY6D): the theme changed with the windows open; the light theme read
        {"theme_switch_33", "3.0.24-beta.33", "",
         {"Theme: changed at once, the light one readable", "Тема: міняється одразу, світла читається",
          "Тема: меняется сразу, светлая читается"},
         {"Open the bag, the chat and the character's block; Alt+O → «Interface» → «Interface theme» → «Light»: every "
          "word turns dark at once (no restart), the chat and the block less see-through, the game's coloured lines "
          "readable. «Accent colour»: its words wide, the chips on the right. Back to «Dark» - as it was.",
          "Відкрийте «Речі», чат і блок персонажа; Alt+O → «Інтерфейс» → «Тема інтерфейсу» → «Світла»: усі слова "
          "одразу темні (без перезапуску), чат і блок менш прозорі, кольорові рядки гри читаються. «Колір "
          "акценту»: слова широко, кнопки праворуч. Назад «Темна» — як було.",
          "Откройте «Вещи», чат и блок персонажа; Alt+O → «Интерфейс» → «Тема интерфейса» → «Светлая»: все слова "
          "сразу тёмные (без перезапуска), чат и блок менее прозрачные, цветные строки игры читаются. «Цвет "
          "акцента»: слова широко, кнопки справа. Назад «Тёмная» — как было."}},
        // 3.0.24-beta.32 (the server's 0x02c4 of 27.11): «Група»'s invite by name
        {"party_invite_name", "3.0.24-beta.32", "interface.party_invite",
         {"Party: invite by name", "Група: запросити за ім'ям", "Группа: пригласить по имени"},
         {"From the server's restart at 03:01 on 28.11. Be in a party; in the friends' card, the «Party» tab: under "
          "the list a player's name and «Invite» - the player is asked as by the game's own invite; a name not in "
          "the game - «… is not in the game now» in the chat.",
          "З перезапуску сервера 28.11 о 03:01. Будьте в групі; у картці друзів, вкладка «Група»: під списком ім'я "
          "гравця і «Запросити» — гравцеві прийде запрошення, як від запрошення самої гри; ім'я, якого немає в грі, "
          "— у чаті «… зараз не в грі».",
          "С перезапуска сервера 28.11 в 03:01. Будьте в группе; в карточке друзей, вкладка «Группа»: под списком "
          "имя игрока и «Пригласить» — игроку придёт приглашение, как от приглашения самой игры; имени нет в игре — "
          "в чате «… сейчас не в игре»."}},
        // 3.0.24-beta.32 (the server's vending tax taken since its fix of 27.11): «Зароблено» what the server paid
        {"shop_paid_32", "3.0.24-beta.32", "interface.my_shop",
         {"My shop: «Earned» what the server paid", "Моя лавка: «Зароблено» — скільки заплатив сервер",
          "Моя лавка: «Заработано» — сколько заплатил сервер"},
         {"Open a shop and sell something: «Earned» grows by as much as your zeny did. From the server's restart "
          "at 03:01 on 28.11 it keeps 2 % back - «Earned» shows it so.",
          "Відкрийте лавку й продайте щось: «Зароблено» росте на стільки ж, на скільки ваші зені. З перезапуску "
          "сервера 28.11 о 03:01 сервер утримує 2 % — «Зароблено» так і покаже.",
          "Откройте лавку и продайте что-нибудь: «Заработано» растёт на столько же, на сколько ваши зени. С "
          "перезапуска сервера 28.11 в 03:01 сервер удерживает 2 % — «Заработано» так и покажет."}},
        // 3.0.24-beta.31 (R-F4GZ3T): the game's ground's light map stage set whole - no white ground after wings
        {"ground_not_white_31", "3.0.24-beta.31", "",
         {"Fields: the ground not white after fly wings", "Поля: земля не біліє після крил",
          "Поля: земля не белеет после крыльев"},
         {"On a field (gef_fild07, prt_fild08) teleport by wings several times and walk about: the ground keeps its "
          "colour. If it goes white - «A problem - report» at once, while it is white.",
          "На полі (gef_fild07, prt_fild08) кілька разів телепортуйтеся крилами й пройдіться: земля лишається "
          "свого кольору. Побіліла — одразу «Є проблема — звіт», поки вона біла.",
          "На поле (gef_fild07, prt_fild08) несколько раз телепортируйтесь крыльями и пройдитесь: земля остаётся "
          "своего цвета. Побелела — сразу «Есть проблема — отчёт», пока она белая."}},
        // 3.0.24-beta.31 (R-MWNRVM and four more): the server's daily play-time limit in the player's words
        {"play_limit_31", "3.0.24-beta.31", "",
         {"Daily play limit: in your words, no drop report", "Денний ліміт гри: зрозуміло, без звіту про розрив",
          "Дневной лимит игры: понятно, без отчёта о разрыве"},
         {"When the server says the daily play-time limit is reached: the chat's line in your language (the hours, "
          "when it resets, how to buy time); after the disconnect the same in the game's box, and no drop report "
          "goes.",
          "Коли сервер скаже, що денний ліміт гри вичерпано: рядок у чаті вашою мовою (скільки годин, коли "
          "скинеться, як докупити час); після від'єднання те саме у вікні гри, і звіт про розрив не надсилається.",
          "Когда сервер скажет, что дневной лимит игры исчерпан: строка в чате на вашем языке (сколько часов, когда "
          "сбросится, как докупить время); после отключения то же в окне игры, и отчёт о разрыве не отправляется."}},
        // 3.0.24-beta.30: the deferred small fixes of parts 1-3
        {"small_fixes_30", "3.0.24-beta.30", "interface.theme",
         {"Small fixes: the light theme, the shop's weight, the pad", "Дрібні виправлення: світла тема, вага, геймпад",
          "Мелкие исправления: светлая тема, вес, геймпад"},
         {"In the «Light» theme with the effects on the windows' shadows dark, the words on red and green buttons "
          "light. «Merchant's shop» refuses a purchase 0.5 over the weight as the server does. In «Choice of things» "
          "with a search the pad's ↑ ↓ walk the rows shown. The reference's kept star in the accent.",
          "У темі «Світла» з ефектами тіні вікон темні, написи на червоних і зелених кнопках світлі. «Лавка торговця» "
          "не дає купити на 0,5 понад вагу, як і сервер. У «Вибір речі» з пошуком ↑ ↓ геймпада йдуть по видимих "
          "рядках. Зірочка довідника — кольору акценту.",
          "В теме «Светлая» с эффектами тени окон тёмные, надписи на красных и зелёных кнопках светлые. «Лавка "
          "торговца» не даёт купить на 0,5 сверх веса, как и сервер. В «Выбор вещи» с поиском ↑ ↓ геймпада идут по "
          "видимым строкам. Звёздочка справочника — цвета акцента."}},
        // 3.0.24-beta.29 (part 4): the site market's average deal price; its line in the item card
        {"market_average", "3.0.24-beta.29", "interface.items",
         {"Site market: the average, in the item card", "Ринок сайту: середня ціна, у картці речі",
          "Рынок сайта: средняя цена, в карточке вещи"},
         {"Point at a thing sold on the site's market three times or more: after «Market: from X z · N lots» comes "
          "«average Y z · M deals»; the same line in its card (a right click) under «An NPC pays».",
          "Наведіть на річ, яку на ринку сайту продали тричі й більше: після «Ринок: від X z · лотів N» іде "
          "«середня Y z · угод M»; той самий рядок у її картці (ПКМ) під «NPC заплатить».",
          "Наведите на вещь, которую на рынке сайта продали трижды и больше: после «Рынок: от X z · лотов N» идёт "
          "«средняя Y z · сделок M»; та же строка в её карточке (ПКМ) под «NPC заплатит»."}},
        // 3.0.24-beta.28 (part 3, task 6): the windows in the old layout already - each in both themes
        {"guild_old_layout", "3.0.24-beta.28", "interface.guild",
         {"Guild card: as in the old game, both themes", "Картка гільдії: як у старій грі, обидві теми",
          "Карточка гильдии: как в старой игре, обе темы"},
         {"Alt+G in the dark and the «Light» theme: the tabs over it, the emblem and the data, who is online, the "
          "guild's experience a bar.",
          "Alt+G у темній і «Світла» темі: вкладки згори, герб і дані, хто в грі, досвід гільдії смужкою.",
          "Alt+G в тёмной и «Светлая» теме: вкладки сверху, герб и данные, кто в игре, опыт гильдии полоской."}},
        {"char_select_old_layout", "3.0.24-beta.28", "interface.char_select",
         {"Character select: as in the old game, both themes", "Вибір персонажа: як у старій грі, обидві теми",
          "Выбор персонажа: как в старой игре, обе темы"},
         {"In both themes: the characters' cards in a carousel (the chosen in the middle), HP and SP and the stats on "
          "a card; enter, make, delete, back.",
          "В обох темах: картки персонажів каруселлю (обраний посередині), HP і SP та параметри на картці; увійти, "
          "створити, видалити, назад.",
          "В обеих темах: карточки персонажей каруселью (выбранный посередине), HP и SP и параметры на карточке; "
          "войти, создать, удалить, назад."}},
        {"login_make_themes", "3.0.24-beta.28", "interface.login",
         {"Login and making a character: both themes", "Вхід і створення персонажа: обидві теми",
          "Вход и создание персонажа: обе темы"},
         {"Switch the theme to «Light» and back: the login window and the character's making read well in both, "
          "the stats at the making as in the old game.",
          "Перемкніть тему на «Світла» й назад: вікно входу та створення персонажа читаються в обох, характеристики "
          "при створенні — як у старій грі.",
          "Переключите тему на «Светлая» и обратно: окно входа и создание персонажа читаются в обеих, "
          "характеристики при создании — как в старой игре."}},
        {"hud_old_places", "3.0.24-beta.28", "interface.theme_hud",
         {"Main screen: the old places, both themes", "Основний екран: старі місця, обидві теми",
          "Основной экран: старые места, обе темы"},
         {"The character's block top left, the minimap top right, F1-F9 under, the chat bottom left - in both themes; "
          "the buffs, the target's card, the bosses, the hunt, the key hints and «The line on a thing picked up» "
          "too.",
          "Блок персонажа ліворуч угорі, мінікарта праворуч угорі, F1-F9 знизу, чат ліворуч унизу — в обох темах; "
          "бафи, картка цілі, боси, полювання, підказки клавіш і «Рядок про підібрану річ» теж.",
          "Блок персонажа слева вверху, миникарта справа вверху, F1-F9 снизу, чат слева внизу — в обеих темах; "
          "баффы, карточка цели, боссы, охота, подсказки клавиш и «Строка о подобранной вещи» тоже."}},
        // 3.0.24-beta.27 (part 3, task 5): «Запросити в групу» for a friend - our own packet (the testers' box)
        {"party_invite", "3.0.24-beta.27", "interface.party_invite",
         {"Friends: invite to the party", "Друзі: запросити в групу", "Друзья: пригласить в группу"},
         {"Be in a party; in the friends' card a friend in the game, not of it, has «Invite to the party»: press it - "
          "the friend is asked as by the game's own invite.",
          "Будьте в групі; у картці друзів друг у грі, не з неї, має «Запросити в групу»: натисніть — другові прийде "
          "запрошення, як від запрошення самої гри.",
          "Будьте в группе; в карточке друзей друг в игре, не из неё, имеет «Пригласить в группу»: нажмите — другу "
          "придёт приглашение, как от приглашения самой игры."}},
        // 3.0.24-beta.27 (part 3, task 4): the NPC's choice its own list beside the talk
        {"npc_choice_list", "3.0.24-beta.27", "interface.npc_talk",
         {"NPC: the choice a list of its own", "NPC: вибір — окремим списком", "NPC: выбор — отдельным списком"},
         {"Talk to an NPC with a choice (a Kafra): the answers stand in their own list right of the talk (left or "
          "under it at the screen's edge); the mouse, the wheel, ↑ ↓, 1-9, Enter, Esc and the pad choose.",
          "Поговоріть з NPC, що дає вибір (Кафра): відповіді стоять окремим списком праворуч від розмови (ліворуч чи "
          "під нею біля краю екрана); обирають миша, коліщатко, ↑ ↓, 1-9, Enter, Esc і геймпад.",
          "Поговорите с NPC, который даёт выбор (Кафра): ответы стоят отдельным списком справа от разговора (слева или "
          "под ним у края экрана); выбирают мышь, колесо, ↑ ↓, 1-9, Enter, Esc и геймпад."}},
        // 3.0.24-beta.27 (part 3, task 3): «Характеристики» - «база + бонус» as the old game's
        {"stats_base_bonus", "3.0.24-beta.27", "interface.status",
         {"Stats: «base + bonus» as in the old game", "Характеристики: «база + бонус», як у старій грі",
          "Характеристики: «база + бонус», как в старой игре"},
         {"Alt+A: each stat «9 + 3» in one column, its cost and «+»; the battle's numbers wider on the right; "
          "«Planning» and the formulas as before.",
          "Alt+A: кожна характеристика «9 + 3» в одному стовпці, ціна й «+»; бойові числа праворуч ширше; "
          "«Планування» й формули — як було.",
          "Alt+A: каждая характеристика «9 + 3» в одном столбце, цена и «+»; боевые числа справа шире; "
          "«Планирование» и формулы — как было."}},
        // 3.0.24-beta.26 (part 3, task 2): «Навички» a narrow list as the old game's; the facts in the card
        {"skills_narrow", "3.0.24-beta.26", "interface.skills",
         {"Skills: a narrow list, the facts by a right click", "Навички: вузький список, відомості — ПКМ",
          "Навыки: узкий список, сведения — ПКМ"},
         {"Alt+S: a narrow list - icon, name, its line, the use level's arrows, «N/M», «+»; a long name ends in «…». "
          "A right click on a skill: its card with the level, what it needs (✓ or how far), what it opens, SP by "
          "level. «Tree» as before.",
          "Alt+S: вузький список — значок, назва, рядок під нею, стрілки рівня застосування, «N/M», «+»; довга назва "
          "закінчується «…». ПКМ на навичці: її картка з рівнем, що потрібно (✓ або скільки бракує), що відкриває, SP "
          "за рівнями. «Дерево» — як було.",
          "Alt+S: узкий список — значок, название, строка под ним, стрелки уровня применения, «N/M», «+»; длинное "
          "название заканчивается «…». ПКМ по навыку: его карточка с уровнем, что нужно (✓ или сколько не хватает), "
          "что открывает, SP по уровням. «Дерево» — как было."}},
        // 3.0.24-beta.26 (part 3, task 1): every window's signs letters of the interface's font, or drawn
        {"font_signs", "3.0.24-beta.26", "interface.items",
         {"Windows' signs: all from the font", "Знаки у вікнах — усі зі шрифту", "Знаки в окнах — все из шрифта"},
         {"Look at the closes of the chat rooms, emotes, guild, skills, stats, trade, settings: a drawn cross, no "
          "box. The party leader and a room's owner: a small gold diamond; the reference's star drawn; «Hunt»'s "
          "«again» an arrow; folds + and -.",
          "Подивіться на закриття чат-кімнат, емоцій, гільдії, навичок, характеристик, обміну, налаштувань: "
          "намальований хрестик, без квадратика. Лідер групи й власник кімнати — маленький золотий ромбик; "
          "зірочка довідника намальована; «знову» в «Полюванні» — стрілка; згортання — + і −.",
          "Посмотрите на закрытие чат-комнат, эмоций, гильдии, навыков, характеристик, обмена, настроек: "
          "нарисованный крестик, без квадратика. Лидер группы и владелец комнаты — маленький золотой ромбик; "
          "звёздочка справочника нарисована; «снова» в «Охоте» — стрелка; сворачивание — + и −."}},
        // 3.0.24-beta.25 (the final review): the item card's cards without the table's lines; «Речі» counts the worn
        {"card_effects_fixed", "3.0.24-beta.25", "interface.cards",
         {"Item card: the cards' effects alone; the bag's count", "Картка речі: лише дія карт; лік сумки",
          "Карточка вещи: только действие карт; счёт сумки"},
         {"Right-click a thing with cards: under each card's name only what it gives - no «Item Class», «Compound on», "
          "«Weight». «Items» title: the worn gear counted too (it holds bag places).",
          "ПКМ на річ із картами: під назвою кожної карти лише те, що вона дає, — без «Item Class», «Compound on», "
          "«Weight». Заголовок «Речі»: вдягнене теж пораховано (воно займає місця сумки).",
          "ПКМ по вещи с картами: под названием каждой карты только то, что она даёт, — без «Item Class», «Compound "
          "on», «Weight». Заголовок «Вещи»: надетое тоже посчитано (оно занимает места сумки)."}},
        // 3.0.24-beta.24 (the plan's task 11): «Вибір речі» - a search over a list of more than 6 rows
        {"pick_search", "3.0.24-beta.24", "interface.picks",
         {"Choosing a thing: the search", "Вибір речі: пошук", "Выбор вещи: поиск"},
         {"Open a long list (the magnifier with many things not known, a card over many pieces): «Search by the name» "
          "over it - click, type part of a name, the list keeps those; Esc stops the typing, a right click - its card.",
          "Відкрийте довгий список (лупа з багатьма неопізнаними речами, карта на багато речей): над ним «Пошук за "
          "назвою» — клацніть, наберіть частину назви, у списку лишаться ці; Esc зупиняє набір, ПКМ — картка речі.",
          "Откройте длинный список (лупа со многими неопознанными вещами, карта на много вещей): над ним «Поиск по "
          "названию» — щёлкните, наберите часть названия, в списке останутся эти; Esc останавливает набор, ПКМ — "
          "карточка вещи."}},
        // 3.0.24-beta.24 (the plan's task 10): «Обмін» - the red frame 5 s before your «Готово» too; the buttons by
        // our own packets (the testers' box)
        {"trade_late", "3.0.24-beta.24", "interface.trade",
         {"Trade: the red frame, 5 s before «Ready» too", "Обмін: червона рамка й за 5 с до «Готово»",
          "Обмен: красная рамка и за 5 с до «Готово»"},
         {"Ask a friend to put a thing in and press «Ready» within 5 s: that thing is framed red, «Changed within 5 s "
          "before your «Ready» or after» shows; a thing put in earlier stays plain.",
          "Попросіть друга покласти річ і натисніть «Готово» протягом 5 с: ця річ у червоній рамці, видно «Змінено за "
          "5 с до вашого «Готово» або після»; річ, покладена раніше, — без рамки.",
          "Попросите друга положить вещь и нажмите «Готово» в течение 5 с: эта вещь в красной рамке, видно «Изменено "
          "за 5 с до вашего «Готово» или после»; вещь, положенная раньше, — без рамки."}},
        {"trade_packets", "3.0.24-beta.24", "interface.trade_packets",
         {"Trade: our own packets for the buttons", "Обмін: свої пакети для кнопок",
          "Обмен: свои пакеты для кнопок"},
         {"Trade with a friend: «Ready» (with zeny typed), «Trade» and «Cancel» work as before.",
          "Обміняйтеся з другом: «Готово» (з набраними зені), «Обмін» і «Скасувати» працюють як раніше.",
          "Обменяйтесь с другом: «Готово» (с набранными зени), «Обмен» и «Отменить» работают как раньше."}},
        // 3.0.24-beta.23 (the plan's task 9): «Відкрити лавку» by our own packet - the testers' box
        {"vend_open_packets", "3.0.24-beta.23", "interface.vend_open_packets",
         {"Opening a shop: our own packet", "Відкриття лавки: свій пакет", "Открытие лавки: свой пакет"},
         {"Use Vending, put things in, press «Open the shop»: the shop opens as before (the game's «My shop» card). "
          "Without a name or a price - a line over the prices says why, nothing is sent.",
          "Скористайтеся навичкою «Vending», покладіть речі, натисніть «Відкрити лавку»: лавка відкривається як "
          "раніше (картка гри «Моя лавка»). Без назви чи без ціни — над цінами написано чому, нічого не надіслано.",
          "Используйте навык «Vending», положите вещи, нажмите «Открыть лавку»: лавка открывается как раньше "
          "(карточка игры «Моя лавка»). Без названия или без цены — над ценами написано почему, ничего не "
          "отправлено."}},
        // 3.0.24-beta.23 (the plan's task 9): the prices remembered for the next shop
        {"vend_prices", "3.0.24-beta.23", "interface.vend_setup",
         {"Opening a shop: the prices remembered", "Відкриття лавки: ціни запам'ятовуються",
          "Открытие лавки: цены запоминаются"},
         {"Open a shop once with prices; next time the same things put in take those prices by themselves.",
          "Відкрийте лавку з цінами; наступного разу ті самі речі самі отримають ці ціни.",
          "Откройте лавку с ценами; в следующий раз те же вещи сами получат эти цены."}},
        // 3.0.24-beta.23 (the plan's task 9): «Моя лавка» - sold N of M, the last buyer, a soft chime
        {"my_shop_sales", "3.0.24-beta.23", "interface.my_shop",
         {"My shop: sold N of M, the buyer, a chime", "Моя лавка: продано N з M, покупець, звук",
          "Моя лавка: продано N из M, покупатель, звук"},
         {"Sell something from your shop: its row says «2 of 5 sold», «Last buyer» names who bought, two quiet "
          "notes sound; «Earned» is what the server paid.",
          "Продайте щось зі своєї лавки: у рядку «продано 2 з 5», «Останній покупець» — хто купив, звучать дві "
          "тихі ноти; «Зароблено» — скільки заплатив сервер.",
          "Продайте что-то из своей лавки: в строке «продано 2 из 5», «Последний покупатель» — кто купил, звучат "
          "две тихие ноты; «Заработано» — сколько заплатил сервер."}},
        // 3.0.24-beta.22 (the plan's task 8): the item card tells what its cards give and what an NPC pays
        {"card_effects", "3.0.24-beta.22", "interface.cards",
         {"Item card: what its cards give, the NPC's price", "Картка речі: що дають карти, ціна NPC",
          "Карточка вещи: что дают карты, цена NPC"},
         {"Right-click a thing with cards: under its words each card's name and what it gives; at the end «An NPC "
          "pays: X z».",
          "ПКМ на річ із картами: під описом — назва кожної карти й що вона дає; наприкінці «NPC заплатить: X z».",
          "ПКМ по вещи с картами: под описанием — название каждой карты и что она даёт; в конце «NPC заплатит: "
          "X z»."}},
        // 3.0.24-beta.22 (the plan's task 7): «Скільки штук» - four quick choices and what fits
        {"count_limit", "3.0.24-beta.22", "interface.storage_packets",
         {"How many: 1 / 10 / half / all and what fits", "Скільки штук: 1 / 10 / половина / усе і що влізе",
          "Сколько штук: 1 / 10 / половина / всё и что влезет"},
         {"With «Storage» by packets, take a big stack into a nearly full bag: four buttons 1, 10, half, all; «N fit "
          "in the bag» under the title, and no more than that is offered.",
          "Зі «Складом» пакетами візьміть велику пачку в майже повну сумку: чотири кнопки 1, 10, половина, усе; «у "
          "сумку влізе N» під заголовком, і більше не пропонує.",
          "Со «Складом» пакетами возьмите большую пачку в почти полную сумку: четыре кнопки 1, 10, половина, всё; «в "
          "сумку влезет N» под заголовком, и больше не предлагает."}},
        // 3.0.24-beta.21 (the plan's task 6): the player's shop tells the weight after the purchase
        {"vend_weight", "3.0.24-beta.21", "interface.vending",
         {"Merchant's shop: weight after the purchase", "Лавка торговця: вага після покупки",
          "Лавка торговца: вес после покупки"},
         {"Open a player's shop, choose things: «Weight after buying X / Y» under the sum; too heavy - it turns red "
          "and «Buy» is off.",
          "Відкрийте лавку гравця, виберіть речі: «Вага після покупки X / Y» під сумою; заважко — червона, і «Купити» "
          "вимкнена.",
          "Откройте лавку игрока, выберите вещи: «Вес после покупки X / Y» под суммой; слишком тяжело — красная, и "
          "«Купить» выключена."}},
        // 3.0.24-beta.21 (the plan's task 5): selling to an NPC, guarded
        {"shop_sell_guard", "3.0.24-beta.21", "interface.shop",
         {"NPC shop: selling, guarded", "Крамниця NPC: продаж із захистом", "Магазин NPC: продажа с защитой"},
         {"Sell at an NPC: the starred things are not in the list; a thing with a card or of +5 asks a second click "
          "(the note says why); «All of «Misc»» puts all the misc in.",
          "Продаж NPC: речей з «Обране» немає в списку; річ із картою чи від +5 просить другого кліку (підказка каже "
          "чому); «Усе з «Різне»» кладе все різне.",
          "Продажа NPC: вещей из «Избранное» нет в списке; вещь с картой или от +5 просит второго клика (подсказка "
          "говорит почему); «Всё из «Разное»» кладёт всё разное."}},
        // 3.0.24-beta.21 (the plan's task 4): the cart opens a shop
        {"cart_open_shop", "3.0.24-beta.21", "interface.cart",
         {"Cart: «Open a shop»", "Візок: «Відкрити лавку»", "Тележка: «Открыть лавку»"},
         {"A merchant with Vending: open «Cart» (Alt+W) - «Open a shop» at the bottom; press it: the shop's making "
          "opens. Pick something with Ctrl - the button takes the picked instead.",
          "Торговець із «Vending»: відкрийте «Візок» (Alt+W) — унизу «Відкрити лавку»; натисніть: відкриється "
          "підготовка лавки. Виберіть щось із Ctrl — кнопка забирає вибране.",
          "Торговец с «Vending»: откройте «Тележку» (Alt+W) — внизу «Открыть лавку»; нажмите: откроется подготовка "
          "лавки. Выберите что-то с Ctrl — кнопка забирает выбранное."}},
        // 3.0.24-beta.20 (docs/superpowers/plans/2026-11-27-item-windows-old-layout.md, tasks 1-2)
        {"items_layout", "3.0.24-beta.20", "interface.items",
         {"Items: the old layout", "Речі: стара розкладка", "Вещи: старая раскладка"},
         {"Turn «Items» on, Alt+E: the six tabs one under another at the left (All ... Starred), «Items 16/100» in the "
          "head, the keys' hint under the cells, the weight and zeny at the bottom; «Storage» and «Cart» alike.",
          "Увімкніть «Речі», Alt+E: шість вкладок одна під одною зліва (Усе ... Обране), «Речі 16/100» у заголовку, "
          "підказка клавіш під клітинками, вага й зені внизу; так само «Склад» і «Візок».",
          "Включите «Вещи», Alt+E: шесть вкладок одна под другой слева (Всё ... Избранное), «Вещи 16/100» в "
          "заголовке, подсказка клавиш под ячейками, вес и зени внизу; так же «Склад» и «Тележка»."}},
        {"items_packets", "3.0.24-beta.20", "interface.items_packets",
         {"Items: our own packets - use, wear, drop", "Речі: свої пакети — використати, вдягнути, викинути",
          "Вещи: свои пакеты — использовать, надеть, выбросить"},
         {"Turn «Items» and this on. Double-click a potion, then a weapon; drag a thing onto the ground (a stack asks "
          "how many); take a thing off in «Equipment»: each happens once, the cell waits for the server.",
          "Увімкніть «Речі» і цю галочку. Подвійний клік на зілля, потім на зброю; перетягніть річ на землю (пачка "
          "спитає скільки); зніміть річ у «Спорядженні»: кожне — один раз, клітинка чекає сервера.",
          "Включите «Вещи» и эту галочку. Двойной клик по зелью, потом по оружию; перетащите вещь на землю (пачка "
          "спросит сколько); снимите вещь в «Снаряжении»: каждое — один раз, ячейка ждёт сервера."}},
        {"ui_themes", "3.0.24-beta.19", "",
         {"Interface: dark and light", "Інтерфейс: темний і світлий", "Интерфейс: тёмный и светлый"},
         {"Alt+O -> «Interface» -> «Theme and colours»: pick the light picture - every window turns light at once "
          "(the chat, the bag, the shops, the main screen). Full mode: a theme per group, «Accent colour» with «own "
          "colour»'s palette. The launcher: «Interface: Dark | Light».",
          "Alt+O -> «Інтерфейс» -> «Тема й кольори»: виберіть світлу картинку - усі вікна одразу світлі (чат, речі, "
          "крамниці, основний екран). Повний режим: тема для кожної групи, «Колір акценту» з палітрою «свій "
          "колір». У лаунчері: «Інтерфейс: Темний | Світлий».",
          "Alt+O -> «Интерфейс» -> «Тема и цвета»: выберите светлую картинку - все окна сразу светлые (чат, вещи, "
          "магазины, основной экран). Полный режим: тема для каждой группы, «Цвет акцента» с палитрой «свой "
          "цвет». В лаунчере: «Интерфейс: Тёмный | Светлый»."}},
        {"vend_packets", "3.0.24-beta.18", "interface.vend_packets",
         {"Merchant's shop: the new purchase", "Лавка торговця: нова покупка", "Лавка торговца: новая покупка"},
         {"Turn «Merchant's shop» and this on. Buy a few things at a player's shop: the shop closes when they come; "
          "with too little zeny it says why.",
          "Увімкніть «Лавка торговця» і цю галочку. Купіть кілька речей у лавці гравця: лавка закривається, коли "
          "речі прийшли; без зені каже чому.",
          "Включите «Лавка торговца» и эту галочку. Купите несколько вещей в лавке игрока: лавка закрывается, когда "
          "вещи пришли; без зени говорит почему."}},
        {"comodo_caves", "3.0.24-beta.16", "view3d.cave_grotto",
         {"Comodo's caves: the new vault", "Печери Комодо: нове склепіння", "Пещеры Комодо: новый свод"},
         {"beach_dun, beach_dun2, beach_dun3, Ctrl+Shift+V, look up: folds of rock in the cave's own light, "
          "stalactites in clusters, the water's light, a few crystals; no rift.",
          "beach_dun, beach_dun2, beach_dun3, Ctrl+Shift+V, погляньте вгору: складки скелі у власному світлі "
          "печери, сталактити гронами, відблиски води, кілька кристалів; без розлому.",
          "beach_dun, beach_dun2, beach_dun3, Ctrl+Shift+V, посмотрите вверх: складки скалы в собственном свете "
          "пещеры, сталактиты гроздьями, блики воды, несколько кристаллов; без разлома."}},
        {"comodo_rift", "3.0.24-beta.11", "view3d.cave_rift",
         {"Comodo: the rift, crystals, grotto", "Комодо: розлом, кристали, грот", "Комодо: разлом, кристаллы, грот"},
         {"Comodo, Ctrl+Shift+V, look up over the bay: the night sky with fireworks in the rift, crystals and the "
          "water's light on the vault, stalactites; no spike down to a lamp post.",
          "Комодо, Ctrl+Shift+V, погляньте вгору над затокою: у розломі нічне небо з феєрверками, на склепінні "
          "кристали й відблиски води, сталактити; жодного «шипа» до ліхтаря.",
          "Комодо, Ctrl+Shift+V, посмотрите вверх над заливом: в разломе ночное небо с фейерверками, на своде "
          "кристаллы и блики воды, сталактиты; никакого «шипа» к фонарю."}},
        {"comodo_cave", "3.0.24-beta.10", "",
         {"Comodo: a cave, no sky", "Комодо: печера, без неба", "Комодо: пещера, без неба"},
         {"Comodo, Ctrl+Shift+V: a rock roof over the town, no sun, no horizon; its fields keep the sky.",
          "Комодо, Ctrl+Shift+V: над містом кам'яний свод, без сонця й обрію; на полях навколо небо як було.",
          "Комодо, Ctrl+Shift+V: над городом каменный свод, без солнца и горизонта; на полях вокруг небо как было."}},
        {"shop_packets", "3.0.24-beta.9", "interface.shop_packets",
         {"NPC shops: the new deal", "Крамниці NPC: нова угода", "Магазины NPC: новая сделка"},
         {"Turn «NPC shops» and this on. Buy a few things at an NPC, then sell some: the window closes when done; with "
          "too little zeny it says why.",
          "Увімкніть «Крамниці NPC» і цю галочку. Купіть у NPC кілька речей, потім продайте: вікно "
          "закривається, коли готово; без зені каже чому.",
          "Включите «Магазины NPC» и эту галочку. Купите у NPC несколько вещей, потом продайте: окно "
          "закрывается, когда готово; без зени говорит почему."}},
        {"cart_packets", "3.0.24-beta.8", "interface.cart_packets",
         {"Cart: the new passing of things", "Візок: нова передача речей", "Тележка: новая передача вещей"},
         {"Turn «Cart» and this on. Drag a stack bag -> cart, double-click to take back, drag cart <-> storage: «How "
          "many» asks.",
          "Увімкніть «Візок» і цю галочку. Перетягніть пачку із сумки у візок, двічі клацніть - назад, перетягніть "
          "візок <-> склад: спитає скільки.",
          "Включите «Тележка» и эту галочку. Перетащите пачку из сумки в тележку, двойной клик - назад, перетащите "
          "тележка <-> склад: спросит сколько."}},
        {"vend_card_right", "3.0.24-beta.6", "",
         {"A shop's ware: its card", "Товар у лавці: його картка", "Товар в лавке: его карточка"},
         {"Open a player's shop, press the right button on a ware: its card with its description and its slots' "
          "cards.",
          "Відкрийте лавку гравця, натисніть правою кнопкою по товару: картка з описом і картами в слотах.",
          "Откройте лавку игрока, нажмите правой кнопкой по товару: карточка с описанием и картами в слотах."}},
        {"settings_reset_all", "3.0.24-beta.5", "",
         {"Settings: all to defaults", "Налаштування: усе за замовчуванням", "Настройки: всё по умолчанию"},
         {"Alt+O, at the bottom «All to defaults», then once more within 4 s: every row back as shipped, «Changed» "
          "empty.",
          "Alt+O, унизу «Усе за замовчуванням», потім ще раз за 4 с: усі рядки як після встановлення, «Змінені» "
          "порожні.",
          "Alt+O, внизу «Всё по умолчанию», затем ещё раз за 4 с: все строки как после установки, «Изменённые» "
          "пусты."}},
        {"storage_packets", "3.0.24-beta.7", "interface.storage_packets",
         {"Storage: the new passing of things", "Склад: нова передача речей", "Склад: новая передача вещей"},
         {"Turn «Kafra storage» and this on. Drag a stack in, double-click to take: «How many» asks, Enter for all. "
          "Alt+RMB moves it whole; click one cell fast - one move.",
          "Увімкніть «Склад Кафри» і цю галочку. Перетягніть пачку, двічі клацніть - забрати: спитає скільки, Enter "
          "- усе. Alt+ПКМ - цілою; швидкі клацання - одна передача.",
          "Включите «Склад Кафры» и эту галочку. Перетащите пачку, двойной клик - забрать: спросит сколько, Enter "
          "- всё. Alt+ПКМ - целиком; быстрые клики - одна передача."}},
        {"settings_simple", "3.0.24-beta.2", "",
         {"Simple settings", "Простий режим налаштувань", "Простой режим настроек"},
         {"Alt+O opens on «Simple»: the three sets, then cards. Turn «Water» off - the water is the game's. «In detail "
          "· N ›» opens that group in «Full».",
          "Alt+O відкривається на «Просто»: три набори, далі картки. Вимкніть «Вода» - вода стає як у грі. "
          "«Детально · N ›» відкриває цю групу в «Повністю».",
          "Alt+O открывается на «Просто»: три набора, дальше карточки. Выключите «Вода» - вода становится как в игре. "
          "«Подробно · N ›» открывает эту группу в «Полностью»."}},
        {"item_windows_game", "3.0.24-beta.1", "",
         {"Item windows are the game's own", "Вікна речей - рідні вікна гри", "Окна вещей - родные окна игры"},
         {"Alt+E, the storage, a shop: the game's own windows. Opened over the character block or the chat, a window "
          "moves itself aside.",
          "Alt+E, склад, магазин: рідні вікна гри. Відкрите поверх блока персонажа чи чату, вікно саме відсувається.",
          "Alt+E, склад, магазин: родные окна игры. Открытое поверх блока персонажа или чата, окно само "
          "отодвигается."}},
        {"guild_fit", "3.0.23-beta.15", "",
         {"The guild window fits the screen", "Вікно гільдії вміщається в екран", "Окно гильдии помещается в экран"},
         {"Alt+G: its bottom and corner are seen. «Members»: drag the scroll bar.",
          "Alt+G: видно низ і куток. «Учасники»: потягніть смугу прокрутки.",
          "Alt+G: виден низ и угол. «Участники»: потяните полосу прокрутки."}},
        {"chat_arrows", "3.0.23-beta.14", "",
         {"The chat leaves the arrows", "Чат не забирає стрілки", "Чат не забирает стрелки"},
         {"The arrows walk. Click the chat, press ↑ - its history. Send a line - the arrows walk again.",
          "Стрілки ведуть персонажа. Клацніть по чату, ↑ - історія. Надішліть рядок - стрілки знову ведуть.",
          "Стрелки ведут персонажа. Кликните по чату, ↑ - история. Отправьте строку - стрелки снова ведут."}},
        {"shop_signs_3d", "3.0.23-beta.12", "",
         {"Shop signs in the 3D view", "Вивіски лавок у 3D-режимі", "Вывески лавок в 3D-режиме"},
         {"Prontera, Ctrl+Shift+V: a sign over its merchant's head; a shop passed - its sign gone.",
          "Пронтера, Ctrl+Shift+V: вивіска над головою продавця; пройдена лавка - вивіски немає.",
          "Пронтера, Ctrl+Shift+V: вывеска над головой продавца; пройденная лавка - вывески нет."}},
        {"malangdo", "3.0.23-beta.12", "",
         {"Malangdo without a crash", "Малангдо без вильоту", "Малангдо без вылета"},
         {"Go to Malangdo a few times (the cat in Alberta).", "Зайдіть на Малангдо кілька разів (кіт в Альберті).",
          "Зайдите на Малангдо несколько раз (кот в Альберте)."}},
        {"room_walls_d3d11", "3.0.23-beta.8", "",
         {"Room walls on Direct3D 11", "Стіни кімнат на Direct3D 11", "Стены комнат на Direct3D 11"},
         {"Direct3D 11, new_zone02, Ctrl+Shift+V: the hall's open sides closed by a plain wall up to the ceiling.",
          "Direct3D 11, new_zone02, Ctrl+Shift+V: відкриті боки залу закриті однотонною стіною до стелі.",
          "Direct3D 11, new_zone02, Ctrl+Shift+V: открытые бока зала закрыты однотонной стеной до потолка."}},
    };
    return list;
}

std::vector<const Check*> open(const std::string& done) {
    std::vector<std::string> ids;
    std::stringstream in(done);
    for (std::string id; std::getline(in, id, ',');)
        if (!id.empty()) ids.push_back(id);
    std::vector<const Check*> out;
    for (const Check& c : all())
        if (std::find(ids.begin(), ids.end(), c.id) == ids.end()) out.push_back(&c);
    return out;
}

std::string markDone(const std::string& done, const std::string& id) {
    if (id.empty() || id.find(',') != std::string::npos) return done;
    if (("," + done + ",").find("," + id + ",") != std::string::npos) return done;
    return done.empty() ? id : done + "," + id;
}

bool shown(const std::string& version) { return version.find("-beta") != std::string::npos; }

const char* words(const char* const texts[3], settings::Lang l) {
    return l == settings::Lang::Uk ? texts[1] : l == settings::Lang::Ru ? texts[2] : texts[0];
}

std::string workedNote(const Check& c, const std::string& version) {
    return std::string("Перевірка «") + c.title[1] + "» (" + c.id + ", з " + c.since + "): працює ✓ - " + version;
}

std::string problemNote(const Check& c, settings::Lang l) {
    return std::string(words(c.title, l)) + " (" + c.id + "): ";
}

}  // namespace uagfx::checksnow

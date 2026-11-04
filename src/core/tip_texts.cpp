#include "core/tip_texts.hpp"

namespace uagfx::tiptexts {
namespace {

struct Tip {
    const char* uk;
    const char* ru;
    const char* en;
};

// The client's tips in its file's order (the game numbers them from 0)
const Tip kTips[kCount] = {
    {"/savechat — зберігає чат у текстовий файл.",
     "/savechat — сохраняет чат в текстовый файл.",
     "/savechat saves the chat to a text file."},
    {"[Ctrl] + [*] перемикає міні-карту між прозорою та звичайною.",
     "[Ctrl] + [*] переключает мини-карту между прозрачной и обычной.",
     "[Ctrl] + [*] switches the minimap between see-through and solid."},
    {"Рухайте мишею, тримаючи праву кнопку та [Shift], — змінюється висота камери.\n"
     "Подвійний клік правою кнопкою повертає камеру в звичайне положення.",
     "Двигайте мышью, удерживая правую кнопку и [Shift], — меняется высота камеры.\n"
     "Двойной щелчок правой кнопкой возвращает камеру в обычное положение.",
     "Move the mouse holding the right button and [Shift] to change the camera's height.\n"
     "A double right click puts the camera back."},
    {"Рухайте мишею, тримаючи праву кнопку та [Ctrl], — екран наближається чи віддаляється.\n"
     "Подвійний клік правою кнопкою з [Ctrl] повертає звичайний масштаб.\n"
     "Масштаб змінює й коліщатко миші.",
     "Двигайте мышью, удерживая правую кнопку и [Ctrl], — экран приближается или отдаляется.\n"
     "Двойной щелчок правой кнопкой с [Ctrl] возвращает обычный масштаб.\n"
     "Масштаб меняет и колёсико мыши.",
     "Move the mouse holding the right button and [Ctrl] to zoom in or out.\n"
     "A double right click with [Ctrl] puts the zoom back.\n"
     "The mouse wheel zooms too."},
    {"Над монстром курсор стає мечем.\n"
     "Клік по монстру починає атаку.",
     "Над монстром курсор становится мечом.\n"
     "Щелчок по монстру начинает атаку.",
     "Over a monster the cursor turns into a sword.\n"
     "A click on the monster attacks it."},
    {"Досвід за монстра залежить від завданої йому шкоди.\n"
     "Якщо двоє гравців б'ють одного монстра, більше досвіду отримує той, хто почав першим.",
     "Опыт за монстра зависит от нанесённого ему урона.\n"
     "Если двое игроков бьют одного монстра, больше опыта получает тот, кто начал первым.",
     "The experience a monster gives depends on the damage you dealt it.\n"
     "When two players hit one monster, the one who started first gets more."},
    {"[Ctrl] + клік по монстру — автоатака, доки монстр не загине.",
     "[Ctrl] + щелчок по монстру — автоатака, пока монстр не погибнет.",
     "[Ctrl] + a click on a monster attacks it until it falls."},
    {"Щоб підняти річ із землі, клацніть по ній лівою кнопкою.\n"
     "Підняті речі потрапляють у ваш інвентар.",
     "Чтобы поднять вещь с земли, щёлкните по ней левой кнопкой.\n"
     "Поднятые вещи попадают в ваш инвентарь.",
     "To pick up a thing from the ground, left-click it.\n"
     "Things picked up go into your inventory."},
    {"Правий клік по речі показує її збільшене зображення та опис.",
     "Правый щелчок по вещи показывает её увеличенное изображение и описание.",
     "A right click on a thing shows its picture and description."},
    {"Дві смужки під персонажем — HP і SP.\n"
     "Зелена — здоров'я (HP), синя — очки умінь (SP). Коли HP доходить до 0, персонаж гине; нижче 25 % "
     "смужка червоніє.",
     "Две полоски под персонажем — HP и SP.\n"
     "Зелёная — здоровье (HP), синяя — очки умений (SP). Когда HP доходит до 0, персонаж погибает; ниже "
     "25 % полоска краснеет.",
     "The two bars under the character are HP and SP.\n"
     "Green is health (HP), blue is skill points (SP). At 0 HP the character dies; under 25 % the bar "
     "turns red."},
    {"Щоразу, коли смужка досвіду (EXP) заповнюється, зростає базовий рівень і ви отримуєте очки "
     "характеристик.",
     "Каждый раз, когда полоска опыта (EXP) заполняется, растёт базовый уровень и вы получаете очки "
     "характеристик.",
     "Each time the experience bar (EXP) fills up, the base level rises and you get status points."},
    {"Щоразу, коли смужка досвіду професії (Job EXP) заповнюється, зростає рівень професії й ви отримуєте "
     "одне очко вмінь.",
     "Каждый раз, когда полоска опыта профессии (Job EXP) заполняется, растёт уровень профессии и вы "
     "получаете одно очко умений.",
     "Each time the job experience bar (Job EXP) fills up, the job level rises and you get one skill "
     "point."},
    {"Коли персонаж стоїть, HP і SP потроху відновлюються.\n"
     "Сидячи — удвічі швидше. Але якщо вага понад 70 % найбільшої, HP і SP не відновлюються.",
     "Когда персонаж стоит, HP и SP понемногу восстанавливаются.\n"
     "Сидя — вдвое быстрее. Но если вес больше 70 % наибольшего, HP и SP не восстанавливаются.",
     "Standing still slowly restores HP and SP.\n"
     "Sitting restores them twice as fast. Over 70 % of the weight limit they do not come back."},
    {"Вага: поточна вага персонажа й найбільша, яку він може нести.\n"
     "Понад 70 % найбільшої HP і SP самі не відновлюються, понад 90 % — не можна атакувати й "
     "користуватися вміннями.",
     "Вес: текущий вес персонажа и наибольший, который он может нести.\n"
     "Свыше 70 % наибольшего HP и SP сами не восстанавливаются, свыше 90 % — нельзя атаковать и "
     "пользоваться умениями.",
     "Weight: what the character carries and the most it can carry.\n"
     "Over 70 % HP and SP stop coming back on their own; over 90 % it cannot attack or use skills."},
    {"Zeny — гроші світу Ragnarok.",
     "Zeny — деньги мира Ragnarok.",
     "Zeny is the money of the Ragnarok world."},
    {"З кожним базовим рівнем ви отримуєте очки характеристик і розподіляєте їх як хочете.\n"
     "На рівнях 1–10 — по 3 очки, далі кожні 5 рівнів очок стає на одне більше.",
     "С каждым базовым уровнем вы получаете очки характеристик и распределяете их как хотите.\n"
     "На уровнях 1–10 — по 3 очка, дальше каждые 5 уровней очков становится на одно больше.",
     "Each base level gives status points to spend as you like.\n"
     "Levels 1-10 give 3 points; then every 5 levels give one more."},
    {"STR (сила) — сила фізичної атаки.\n"
     "Також збільшує найбільшу вагу, тож персонаж несе більше речей.",
     "STR (сила) — сила физической атаки.\n"
     "Также увеличивает наибольший вес, так что персонаж несёт больше вещей.",
     "STR (strength): physical attack.\n"
     "It also raises the weight limit, so the character carries more."},
    {"DEX (спритність) — влучність і середня сила атаки зброєю, особливо луком.\n"
     "Зменшує промахи й час вимови заклинань.",
     "DEX (ловкость) — меткость и средняя сила атаки оружием, особенно луком.\n"
     "Уменьшает промахи и время произнесения заклинаний.",
     "DEX (dexterity): accuracy and the average weapon attack, bows above all.\n"
     "It cuts misses and casting time."},
    {"AGI (швидкість) — ухилення та швидкість атаки.",
     "AGI (быстрота) — уклонение и скорость атаки.",
     "AGI (agility): dodging and attack speed."},
    {"VIT (витривалість) — захист і здоров'я.\n"
     "Менше шкоди від ворогів, більше найбільшого HP і швидше його відновлення.",
     "VIT (живучесть) — защита и здоровье.\n"
     "Меньше урона от врагов, больше наибольшего HP и быстрее его восстановление.",
     "VIT (vitality): defence and health.\n"
     "Less damage taken, more HP and faster HP recovery."},
    {"INT (інтелект) — магія: магічна атака й захист, опір магії.",
     "INT (интеллект) — магия: магическая атака и защита, сопротивление магии.",
     "INT (intelligence): magic - magic attack and defence, resistance to magic."},
    {"LUK (удача) — впливає на багато що.\n"
     "Від неї залежать критичні удари та ідеальне ухилення.",
     "LUK (удача) — влияет на многое.\n"
     "От неё зависят критические удары и идеальное уклонение.",
     "LUK (luck) touches many things.\n"
     "Critical hits and perfect dodges depend on it."},
    {"[Print Screen] або [Scroll Lock] — знімок екрана в теку ScreenShot.",
     "[Print Screen] или [Scroll Lock] — снимок экрана в папку ScreenShot.",
     "[Print Screen] or [Scroll Lock] saves a screenshot in the ScreenShot folder."},
    {"[F10] — змінює висоту вікна чату.",
     "[F10] — меняет высоту окна чата.",
     "[F10] changes the chat window's height."},
    {"[F11] — закриває всі вікна, крім основної інформації та чату.",
     "[F11] — закрывает все окна, кроме основной информации и чата.",
     "[F11] closes every window but the basic info and the chat."},
    {"[F12] — відкриває й закриває панель швидких клавіш.\n"
     "Покладіть туди часті речі та вміння: кожна клітинка має свою клавішу F1–F9.",
     "[F12] — открывает и закрывает панель быстрых клавиш.\n"
     "Положите туда частые вещи и умения: у каждой ячейки своя клавиша F1–F9.",
     "[F12] opens and closes the hotkey bar.\n"
     "Put the things and skills you use often there: each slot has its key F1-F9."},
    {"[Insert] — сісти або встати (те саме, що /sit і /stand). Сидячи, HP і SP відновлюються вдвічі "
     "швидше.",
     "[Insert] — сесть или встать (то же, что /sit и /stand). Сидя, HP и SP восстанавливаются вдвое "
     "быстрее.",
     "[Insert] sits down or stands up (as /sit and /stand). Sitting restores HP and SP twice as fast."},
    {"[Alt] + [End] — показати чи сховати смужки HP і SP.",
     "[Alt] + [End] — показать или скрыть полоски HP и SP.",
     "[Alt] + [End] shows or hides the HP and SP bars."},
    {"[Alt] + [Home] — показати чи сховати клітинку під курсором на землі.",
     "[Alt] + [Home] — показать или скрыть клетку под курсором на земле.",
     "[Alt] + [Home] shows or hides the cell under the cursor on the ground."},
    {"[Alt] + [=] — повертає звичайний режим введення тексту з режиму 1×1. У режимі 1×1 більшість команд "
     "не працює.",
     "[Alt] + [=] — возвращает обычный режим ввода текста из режима 1×1. В режиме 1×1 большинство команд "
     "не работает.",
     "[Alt] + [=] brings typing back from the 1x1 mode, where most commands do not work."},
    {"[Alt] + [F10] — показати чи сховати чат.",
     "[Alt] + [F10] — показать или скрыть чат.",
     "[Alt] + [F10] shows or hides the chat."},
    {"[Alt] + [1–9], [Ctrl] + [-, =, \\] — швидкі клавіші емоцій: /! /? /ho /lv /lv2 /swt /ic /an /ag /$ "
     "/... /gawi (ножиці) /bawi (камінь) /bo (папір) /thx /wah /sry /heh /swt2 /hmm /no1 /?? /omg /oh /X "
     "/hp /go /sob /gg /kis /kis2 /pif",
     "[Alt] + [1–9], [Ctrl] + [-, =, \\] — быстрые клавиши эмоций: /! /? /ho /lv /lv2 /swt /ic /an /ag /$ "
     "/... /gawi (ножницы) /bawi (камень) /bo (бумага) /thx /wah /sry /heh /swt2 /hmm /no1 /?? /omg /oh "
     "/X /hp /go /sob /gg /kis /kis2 /pif",
     "[Alt] + [1-9], [Ctrl] + [-, =, \\] are the emotions' keys: /! /? /ho /lv /lv2 /swt /ic /an /ag /$ "
     "/... /gawi (scissors) /bawi (rock) /bo (paper) /thx /wah /sry /heh /swt2 /hmm /no1 /?? /omg /oh /X "
     "/hp /go /sob /gg /kis /kis2 /pif"},
    {"/who — скільки гравців зараз на сервері.",
     "/who — сколько игроков сейчас на сервере.",
     "/who shows how many players are on the server."},
    {"/where — назва місцевості та ваші координати.",
     "/where — название местности и ваши координаты.",
     "/where shows the place's name and your coordinates."},
    {"/h — список команд.",
     "/h — список команд.",
     "/h lists the commands."},
    {"/music — увімкнути чи вимкнути музику.\n"
     "Гучність — у вікні налаштувань ([Alt] + [O]).",
     "/music — включить или выключить музыку.\n"
     "Громкость — в окне настроек ([Alt] + [O]).",
     "/music turns the music on or off.\n"
     "Its volume is in the options window ([Alt] + [O])."},
    {"/sound — увімкнути чи вимкнути звуки.\n"
     "Гучність — у вікні налаштувань ([Alt] + [O]).",
     "/sound — включить или выключить звуки.\n"
     "Громкость — в окне настроек ([Alt] + [O]).",
     "/sound turns the sounds on or off.\n"
     "Their volume is in the options window ([Alt] + [O])."},
    {"/effect — увімкнути чи вимкнути ефекти. Без них гра швидша.",
     "/effect — включить или выключить эффекты. Без них игра быстрее.",
     "/effect turns the effects on or off. The game runs faster without them."},
    {"/skip — пропуск кадрів.\n"
     "Без пропуску гра на слабкому комп'ютері може гальмувати; якщо великих затримок немає, вмикати не "
     "радимо.",
     "/skip — пропуск кадров.\n"
     "Без пропуска игра на слабом компьютере может тормозить; если больших задержек нет, включать не "
     "советуем.",
     "/skip: frame skipping.\n"
     "Without it a weak computer may lag; unless the game lags a lot, leave it off."},
    {"/ex [ім'я] — не приймати шепіт від гравця.\n"
     "/in [ім'я] — знову приймати.",
     "/ex [имя] — не принимать шёпот от игрока.\n"
     "/in [имя] — снова принимать.",
     "/ex [name] blocks whispers from a player.\n"
     "/in [name] lets them through again."},
    {"/exall — блокує всі розмови, зокрема шепіт.\n"
     "/inall — знову дозволяє.",
     "/exall — блокирует все разговоры, включая шёпот.\n"
     "/inall — снова разрешает.",
     "/exall blocks all talk, whispers too.\n"
     "/inall allows it again."},
    {"/ex — список гравців, чий шепіт ви заблокували.",
     "/ex — список игроков, чей шёпот вы заблокировали.",
     "/ex lists the players whose whispers you blocked."},
    {"/organize [назва] — створити групу.\n"
     "Як лідер, ви можете запрошувати кого завгодно.",
     "/organize [название] — создать группу.\n"
     "Как лидер, вы можете приглашать кого угодно.",
     "/organize [name] makes a party.\n"
     "As its leader you can invite anyone."},
    {"Правий клік по гравцеві — у меню є «Запросити в групу». Так можна зібрати друзів.",
     "Правый щелчок по игроку — в меню есть «Пригласить в группу». Так можно собрать друзей.",
     "A right click on a player shows 'Invite to the party' in its menu."},
    {"/leave — вийти з групи.\n"
     "Команда не перепитує — подумайте заздалегідь.",
     "/leave — выйти из группы.\n"
     "Команда не переспрашивает — подумайте заранее.",
     "/leave leaves the party.\n"
     "It does not ask twice - think first."},
    {"/expel [ім'я] — вигнати гравця з групи.\n"
     "Команда не перепитує — подумайте заздалегідь.",
     "/expel [имя] — выгнать игрока из группы.\n"
     "Команда не переспрашивает — подумайте заранее.",
     "/expel [name] removes a player from the party.\n"
     "It does not ask twice - think first."},
    {"Оберіть у чаті канал групи — і пишіть лише її учасникам.",
     "Выберите в чате канал группы — и пишите только её участникам.",
     "Pick the party channel in the chat to talk to the party alone."},
    {"%[повідомлення] — надіслати повідомлення групі.\n"
     "Те саме робить [Ctrl] + [Enter].",
     "%[сообщение] — отправить сообщение группе.\n"
     "То же делает [Ctrl] + [Enter].",
     "%[message] sends a message to the party.\n"
     "[Ctrl] + [Enter] does the same."},
    {"Учасники групи на тій самій карті видні на міні-карті — так легко знайти одне одного.",
     "Участники группы на той же карте видны на мини-карте — так легко найти друг друга.",
     "Party members on the same map show on the minimap, so you find each other easily."},
    {"Шепіт: ім'я персонажа — ліворуч у рядку чату, повідомлення — праворуч. [Tab] перемикає між ними.",
     "Шёпот: имя персонажа — слева в строке чата, сообщение — справа. [Tab] переключает между ними.",
     "Whispering: the character's name on the chat line's left, the message on its right. [Tab] moves "
     "between them."},
    {"Кнопки [+] і [−] на міні-карті наближають і віддаляють карту.",
     "Кнопки [+] и [−] на мини-карте приближают и отдаляют карту.",
     "The [+] and [-] buttons on the minimap zoom it in and out."},
    {"Система MVP: речі, що випали з боса, одразу потрапляють в інвентар гравця, який завдав йому "
     "найбільше шкоди.\n"
     "Якщо рідкісних речей немає, MVP отримує 50 % досвіду за боса.",
     "Система MVP: вещи, выпавшие из босса, сразу попадают в инвентарь игрока, нанёсшего ему больше всего "
     "урона.\n"
     "Если редких вещей нет, MVP получает 50 % опыта за босса.",
     "The MVP system: a boss's drops go straight to the player who dealt it the most damage.\n"
     "With no rare drops the MVP gets 50 % of the boss's experience."},
    {"У віконному режимі вікно гри можна пересувати: [Alt] + [Пробіл], тримаючи [M], — і стрілками чи "
     "мишею.",
     "В оконном режиме окно игры можно двигать: [Alt] + [Пробел], удерживая [M], — и стрелками или мышью.",
     "In a window the game's window can be moved: [Alt] + [Space] holding [M], then the arrows or the "
     "mouse."},
    {"/fog — увімкнути чи вимкнути туман на полях.\n"
     "На слабкому комп'ютері він може гальмувати; якщо гра вилітає без причини, вимкніть його.",
     "/fog — включить или выключить туман на полях.\n"
     "На слабом компьютере он может тормозить; если игра вылетает без причины, выключите его.",
     "/fog turns the fields' fog on or off.\n"
     "On a weak computer it may lag; if the game closes for no reason, turn it off."},
    {"Речі з монстрів треба розпізнати лупою. Лупи продаються в крамницях інструментів.",
     "Вещи из монстров нужно опознать лупой. Лупы продаются в магазинах инструментов.",
     "Things monsters drop need a magnifier to be identified. Tool shops sell them."},
    {"Пасткам мисливця потрібні пастки — вони продаються в крамницях інструментів.",
     "Ловушкам охотника нужны ловушки — они продаются в магазинах инструментов.",
     "A hunter's trap skills need traps, sold in tool shops."},
    {"Гільдію створюють з Emperium: команда /guild \"[назва]\". Вікно гільдії — [Alt] + [G].",
     "Гильдию создают с Emperium: команда /guild \"[название]\". Окно гильдии — [Alt] + [G].",
     "A guild is made with an Emperium: /guild \"[name]\". The guild window is [Alt] + [G]."},
    {"Чат гільдії: /gc повідомлення, $ повідомлення або повідомлення з [Alt] + [Enter].",
     "Чат гильдии: /gc сообщение, $ сообщение или сообщение с [Alt] + [Enter].",
     "Guild chat: /gc message, $ message, or a message sent with [Alt] + [Enter]."},
    {"Щоб вставити карту: річ має бути в інвентарі, двічі клацніть по карті та оберіть річ у списку.",
     "Чтобы вставить карту: вещь должна быть в инвентаре, дважды щёлкните по карте и выберите вещь в "
     "списке.",
     "To insert a card: with the thing in your inventory, double-click the card and pick the thing from "
     "the list."},
    {"Негативні стани:\n"
     "Stun — оглушення: ви чи монстр не можете рухатися.\n"
     "Blind — сліпота: екран темнішає, монстр бачить ближче.\n"
     "Sleep — сон: нічого не можна робити, доки вас не вдарять.\n"
     "Silence — мовчання: не можна вимовляти заклинання й користуватися вміннями.",
     "Отрицательные состояния:\n"
     "Stun — оглушение: вы или монстр не можете двигаться.\n"
     "Blind — слепота: экран темнеет, монстр видит ближе.\n"
     "Sleep — сон: ничего нельзя делать, пока вас не ударят.\n"
     "Silence — немота: нельзя произносить заклинания и пользоваться умениями.",
     "Bad states:\n"
     "Stun: you or the monster cannot move.\n"
     "Blind: the screen goes dark, a monster sees less far.\n"
     "Sleep: nothing can be done until you are hit.\n"
     "Silence: no spells or skills."},
    {"Після смерті обидві смужки досвіду зменшуються, а 5 секунд не можна користуватися вміннями чи "
     "битися.",
     "После смерти обе полоски опыта уменьшаются, а 5 секунд нельзя пользоваться умениями или сражаться.",
     "Dying takes from both experience bars, and for 5 seconds you cannot use skills or fight."},
    {"Візок, склад і збереження в містах — у дівчат із Kafra.",
     "Тележка, склад и сохранение в городах — у девушек из Kafra.",
     "Carts, storage and saving in towns come from the Kafra ladies."},
    {"Друге коліщатко миші (якщо є) швидко обирає рівень поточного вміння.",
     "Второе колёсико мыши (если есть) быстро выбирает уровень текущего умения.",
     "A second mouse wheel, if you have one, picks the current skill's level fast."},
    {"Пошта Мідгарду:\n"
     "@checkmail — перевірити пошту\n"
     "@readmail <n> — прочитати лист n\n"
     "@deletemail <n> — видалити лист n\n"
     "@sendmail <ім'я> <текст> — надіслати гравцеві лист",
     "Почта Мидгарда:\n"
     "@checkmail — проверить почту\n"
     "@readmail <n> — прочитать письмо n\n"
     "@deletemail <n> — удалить письмо n\n"
     "@sendmail <имя> <текст> — отправить игроку письмо",
     "Midgard's mail:\n"
     "@checkmail checks the mail\n"
     "@readmail <n> reads letter n\n"
     "@deletemail <n> deletes letter n\n"
     "@sendmail <name> <text> sends a player a letter"},
    {"Рейтинги:\n"
     "/blacksmith — 10 найкращих ковалів\n"
     "/alchemist — 10 найкращих алхіміків\n"
     "/pvpinfo — ваш стан на PvP-карті",
     "Рейтинги:\n"
     "/blacksmith — 10 лучших кузнецов\n"
     "/alchemist — 10 лучших алхимиков\n"
     "/pvpinfo — ваше состояние на PvP-карте",
     "Rankings:\n"
     "/blacksmith - the 10 best blacksmiths\n"
     "/alchemist - the 10 best alchemists\n"
     "/pvpinfo - your state on a PvP map"},
    {"Якщо комп'ютер гальмує від ефектів магії, зменште їх командою /mineffect або вимкніть зовсім — "
     "/effect.",
     "Если компьютер тормозит от эффектов магии, уменьшите их командой /mineffect или выключите совсем — "
     "/effect.",
     "If spell effects slow your computer, lighten them with /mineffect or turn them off with /effect."},
    {"Корисні команди:\n"
     "@time — час на сервері\n"
     "@mi <монстр> — усе про монстра\n"
     "@ignorelist — кого ви заблокували\n"
     "@email — змінити e-mail з реєстрації",
     "Полезные команды:\n"
     "@time — время на сервере\n"
     "@mi <монстр> — всё о монстре\n"
     "@ignorelist — кого вы заблокировали\n"
     "@email — сменить e-mail из регистрации",
     "Useful commands:\n"
     "@time - the server's time\n"
     "@mi <monster> - all about a monster\n"
     "@ignorelist - whom you blocked\n"
     "@email - change the e-mail you registered with"},
    {"Набридли чати? Спробуйте /nm і /nm2.",
     "Надоели чаты? Попробуйте /nm и /nm2.",
     "Tired of chat rooms? Try /nm and /nm2."},
};

}  // namespace

bool isUaroList(int count, const std::string& firstTip) {
    return count == kCount && firstTip.find("/savechat") != std::string::npos &&
           firstTip.find("text file") == std::string::npos;
}

std::string tip(int number, settings::Lang l) {
    if (number < 0 || number >= kCount) return {};
    const Tip& t = kTips[number];
    return l == settings::Lang::Uk ? t.uk : l == settings::Lang::Ru ? t.ru : t.en;
}

}  // namespace uagfx::tiptexts

#include "core/checks_now.hpp"

#include <algorithm>
#include <sstream>

namespace uagfx::checksnow {

const std::vector<Check>& all() {
    static const std::vector<Check> list = {
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

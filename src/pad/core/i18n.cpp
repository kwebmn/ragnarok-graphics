#include "pad/core/i18n.hpp"

#include <cctype>
#include <cstddef>

namespace uapad {
namespace {

struct Entry {
    const char* en;
    const char* uk;
    const char* ru;
};

// In Text order.
const Entry kTexts[] = {
    {"Buttons", "Кнопки", "Кнопки"},
    {"Settings", "Налаштування", "Настройки"},
    {"default layout", "типова розкладка", "раскладка по умолчанию"},
    {"character", "персонаж", "персонаж"},
    {"Reset all buttons", "Скинути всі кнопки", "Сбросить все кнопки"},
    {"Reset all settings", "Скинути всі налаштування", "Сбросить все настройки"},
    {"Save", "Зберегти", "Сохранить"},
    {"Cancel", "Скасувати", "Отмена"},
    {"↑↓ choose    ←→ change    Y — default\nLB/RB — tab    A, Start — save    B — cancel",
     "↑↓ вибрати    ←→ змінити    Y — типово\nLB/RB — вкладка    A, Start — зберегти    B — скасувати",
     "↑↓ выбрать    ←→ изменить    Y — по умолчанию\nLB/RB — вкладка    A, Start — сохранить    B — отмена"},
    {"Attack", "Атака", "Атака"},
    {"Slot", "Слот", "Слот"},
    {"row", "ряд", "ряд"},
    {"Pick up", "Підібрати", "Подобрать"},
    {"Sit / stand", "Сісти / встати", "Сесть / встать"},
    {"Clear target", "Скинути ціль", "Сбросить цель"},
    {"Camera / target", "Камера / ціль", "Камера / цель"},
    {"Language", "Мова", "Язык"},
    {"Keep the cursor in the window", "Курсор не виходить з вікна", "Курсор не выходит из окна"},
    {"Click that activates the window", "Клік, що активує вікно", "Клик, активирующий окно"},
    {"Middle mouse button", "Середня кнопка миші", "Средняя кнопка мыши"},
    {"Mouse button 4", "Кнопка миші 4", "Кнопка мыши 4"},
    {"Mouse button 5", "Кнопка миші 5", "Кнопка мыши 5"},
    {"Borderless window", "Вікно без рамки", "Окно без рамки"},
    {"Save CPU in the background", "Економити процесор у фоні", "Экономить процессор в фоне"},
    {"Vibration", "Вібрація", "Вибрация"},
    {"Target mark", "Позначка цілі", "Метка цели"},
    {"Walk radius", "Радіус ходьби", "Радиус ходьбы"},
    {"on", "увімк.", "вкл."},
    {"off", "вимк.", "выкл."},
    {"stays out of the game", "не йде в гру", "не идёт в игру"},
    {"goes to the game", "йде в гру", "идёт в игру"},
    {"\"!\" when chosen", "«!» при виборі", "«!» при выборе"},
    {"fireball (Sight)", "вогник (Sight)", "огонёк (Sight)"},
    {"light (Ruwach)", "світло (Ruwach)", "свет (Ruwach)"},
    {"poison", "отрута", "яд"},
    {"deadly poison", "смертельна отрута", "смертельный яд"},
    {"curse (red)", "прокляття (червоний)", "проклятие (красный)"},
    {"stun (stars)", "оглушення (зірочки)", "оглушение (звёздочки)"},
    {"sleep (zzz)", "сон (zzz)", "сон (zzz)"},
    {"silence", "мовчання", "молчание"},
    {"Talk to NPC", "Говорити з NPC", "Говорить с NPC"},
    {"{A}: next", "{A}: далі", "{A}: далее"},
    {"{A} / {B}: close", "{A} / {B}: закрити", "{A} / {B}: закрыть"},
    {"the NPC is talking  ·  hold {B}: close", "NPC говорить  ·  утримати {B}: закрити",
     "NPC говорит  ·  удерживать {B}: закрыть"},
    {"↑ ↓: choose  ·  {A}: OK  ·  {B}: cancel", "↑ ↓: вибір  ·  {A}: OK  ·  {B}: скасувати",
     "↑ ↓: выбор  ·  {A}: OK  ·  {B}: отмена"},
    {"↑ ↓: ±1  ·  {LB} / {RB}: ±10  ·  ← →: ±100  ·  {A}: OK", "↑ ↓: ±1  ·  {LB} / {RB}: ±10  ·  ← →: ±100  ·  {A}: OK",
     "↑ ↓: ±1  ·  {LB} / {RB}: ±10  ·  ← →: ±100  ·  {A}: OK"},
    {"Reference (uaRO Graphics)", "Довідник (uaRO Graphics)", "Справочник (uaRO Graphics)"},
    {"↑ ↓: whom  ·  {A}: cast  ·  {KEY}: on yourself  ·  {B}: cancel",
     "↑ ↓: кому  ·  {A}: застосувати  ·  {KEY}: на себе  ·  {B}: скасувати",
     "↑ ↓: кому  ·  {A}: применить  ·  {KEY}: на себя  ·  {B}: отмена"},
    {"{B}: close the window", "{B}: закрити вікно", "{B}: закрыть окно"},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "every Text needs its translations");

std::string trimLower(const std::string& s) {
    std::string out;
    for (char c : s)
        if (!std::isspace(static_cast<unsigned char>(c)))
            out.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

}  // namespace

std::optional<Lang> parseLang(const std::string& token) {
    const std::string t = trimLower(token);
    if (t == "en") return Lang::En;
    if (t == "uk") return Lang::Uk;
    if (t == "ru") return Lang::Ru;
    return std::nullopt;
}

const char* langToken(Lang l) {
    switch (l) {
        case Lang::Uk: return "uk";
        case Lang::Ru: return "ru";
        case Lang::En: break;
    }
    return "en";
}

const char* langName(Lang l) {
    switch (l) {
        case Lang::Uk: return "Українська";
        case Lang::Ru: return "Русский";
        case Lang::En: break;
    }
    return "English";
}

Lang nextLang(Lang l, int dir) {
    const int i = int(l) + (dir > 0 ? 1 : -1);
    return Lang((i % kLangCount + kLangCount) % kLangCount);
}

const char* tr(Lang l, Text t) {
    const size_t i = size_t(t);
    if (i >= size_t(Text::Count)) return "";
    const Entry& e = kTexts[i];
    switch (l) {
        case Lang::Uk: return e.uk;
        case Lang::Ru: return e.ru;
        case Lang::En: break;
    }
    return e.en;
}

}  // namespace uapad

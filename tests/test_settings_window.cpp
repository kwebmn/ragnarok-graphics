#include "core/settings/schema.hpp"
#include "core/settings_window.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.122 (the concept «Ігровий інтерфейс», stages 5-6: «Налаштування» in place of the game's Alt+O): its
// sections - «Гра» (the game's own options) and «Картинка» (the three choices) first, then the settings app's
// «Інтерфейс» sections, every key of theirs a row of the schema
TEST(settings_window_sections) {
    const std::vector<settingswin::Section>& s = settingswin::sections();
    CHECK(s.size() >= 5);
    CHECK(s[0].kind == settingswin::Kind::Game && s[0].keys.size() == 6);
    CHECK(s[1].kind == settingswin::Kind::Picture && s[1].keys.size() == 3);
    for (size_t i = 2; i < s.size(); ++i) {
        CHECK(s[i].kind == settingswin::Kind::Keys);
        CHECK(!s[i].keys.empty());
        for (const std::string& k : s[i].keys) CHECK(settings::findSetting(k) != nullptr);
    }
}

// The search: the rows of every section whose words match, in the sections' order; the game's own by their names
TEST(settings_window_search) {
    const std::vector<settingswin::Found> music = settingswin::find("музика", settings::Lang::Uk);
    CHECK(!music.empty());
    if (!music.empty()) CHECK(music[0].section == 0 && music[0].key == "game.0");
    const std::vector<settingswin::Found> map = settingswin::find("мінікарт", settings::Lang::Uk);
    CHECK(map.size() >= 3);
    for (size_t i = 1; i < map.size(); ++i) CHECK(map[i].section >= map[i - 1].section);
    CHECK(settingswin::find("", settings::Lang::Uk).empty());
    CHECK(settingswin::find("zzzzqqq", settings::Lang::En).empty());
}

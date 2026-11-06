#include <algorithm>
#include "core/settings/search.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

bool hasRow(const std::vector<Hit>& hits, const char* key) {
    for (const Hit& h : hits)
        if (h.kind == HitKind::Ini && sectionKey(iniSettings()[h.index]) == key) return true;
    return false;
}

}  // namespace

TEST(search_folds_cyrillic_case) {
    CHECK(foldCase("СВЕТ Ёлка ІЇЄҐ") == "свет елка іїєґ");
    CHECK(foldCase("DLSS Torch") == "dlss torch");
    CHECK(foldCase("") == "");
}

TEST(search_needs_every_word) {
    CHECK(textMatches("Свет факелов", "факел свет"));
    CHECK(textMatches("Свет факелов", "  СВЕТ  "));
    CHECK(!textMatches("Свет факелов", "свет дым"));
}

TEST(search_finds_effects_by_russian_label) {
    const std::vector<Hit> hits = findSettings("факел", Lang::Ru, Mode::Detailed);
    CHECK(hasRow(hits, "effects.torch"));
    for (const Hit& h : hits)
        if (h.kind == HitKind::Ini && sectionKey(iniSettings()[h.index]) == "effects.torch")
            CHECK(h.place.view == View::Effects);
}

TEST(search_hides_developer_rows) {
    CHECK(!hasRow(findSettings("замерять", Lang::Ru, Mode::Detailed), "debug.profile"));
    CHECK(hasRow(findSettings("замерять", Lang::Ru, Mode::Developer), "debug.profile"));
    CHECK(hasRow(findSettings("shader_variants", Lang::En, Mode::Developer), "debug.shader_variants"));
}

TEST(search_simple_places_main_rows) {
    const std::vector<Hit> hits = findSettings("DLSS", Lang::Ru, Mode::Simple);
    CHECK(hasRow(hits, "quality.dlss"));
    // 3.0.11-beta.301: the Detailed mode's rows found too, after the mode's own and marked to switch the mode
    CHECK(hasRow(hits, "quality.dlss_model"));
    bool other = false;
    for (const Hit& h : hits) {
        if (h.level == Mode::Simple) CHECK(!other && (h.place.view == View::Main || h.place.view == View::Effects));
        other = other || h.level != Mode::Simple;
    }
    CHECK(other);
}

// 3.0.11-beta.301 (the review: «травы» found 4 of «трава»'s 13, a Russian word nothing in the Ukrainian app, «шрифт»
// and «оконный режим» nothing): stems, every language, synonyms, the hand-built pages
TEST(search_finds_by_stems_languages_and_synonyms) {
    const size_t grass = findSettings("трава", Lang::Ru, Mode::Detailed).size();
    CHECK(findSettings("травы", Lang::Ru, Mode::Detailed).size() >= grass);
    CHECK(hasRow(findSettings("тени", Lang::Uk, Mode::Detailed), "effects.actor_shadows"));
    CHECK(hasRow(findSettings("шрифт", Lang::Uk, Mode::Detailed), "interface.text_size"));
    CHECK(queryMatches("Згладжування", "плавность"));
    CHECK(queryMatches("Курсор", "мышь"));
    bool window = false;
    for (const Hit& h : findSettings("оконный режим", Lang::Ru, Mode::Detailed))
        window = window || (h.kind == HitKind::Action && h.place.view == View::Screen);
    CHECK(window);
    CHECK(!findSettings("яскравість", Lang::Uk, Mode::Simple).empty());
}

TEST(search_short_query_finds_nothing) {
    CHECK(findSettings("с", Lang::Ru, Mode::Developer).empty());
    CHECK(findSettings("   ", Lang::Ru, Mode::Developer).empty());
}

TEST(search_finds_actions_and_game_rows) {
    bool backups = false;
    for (const Hit& h : findSettings("резервн", Lang::Ru, Mode::Detailed))
        backups = backups || (h.kind == HitKind::Action && h.place.view == View::Service);
    CHECK(backups);
    bool fog = false;  // the game's own key: its fog, on the Quality page (now Graphics)
    for (const Hit& h : findSettings("туман", Lang::Ru, Mode::Detailed))
        fog = fog || (h.kind == HitKind::Reg && h.place.view == View::Graphics);
    CHECK(fog);
}

TEST(search_keeps_the_lists_order) {
    // Graphics before Effects in the Detailed list: a word both have shows the Graphics row first
    const std::vector<Hit> hits = findSettings("свет", Lang::Ru, Mode::Detailed);
    // 3.0.11-beta.301: the matches in a name, then those in a hint alone - each in the list's order
    int lastView = -1, runs = 1;
    const std::vector<View> list = viewsOf(Mode::Detailed);
    for (const Hit& h : hits) {
        int at = 0;
        while (at < int(list.size()) && list[size_t(at)] != h.place.view) ++at;
        if (at < lastView) ++runs;
        lastView = at;
    }
    CHECK(runs <= 2);
    CHECK(hits.size() <= 80);
}

TEST(search_finds_effects_by_their_old_names) {
    // 2.6.22: the effects renamed with the new groups are found by the names the player knew too
    CHECK(hasRow(findSettings("живые персонажи", Lang::Ru, Mode::Detailed), "effects.sprite_light"));
    CHECK(hasRow(findSettings("объём от солнца", Lang::Ru, Mode::Detailed), "effects.sprite_light"));
    CHECK(hasRow(findSettings("контровой", Lang::Ru, Mode::Detailed), "effects.rim_light"));
    CHECK(hasRow(findSettings("Rim light", Lang::En, Mode::Detailed), "effects.rim_light"));
    CHECK(hasRow(findSettings("Вологі стіни", Lang::Uk, Mode::Detailed), "effects.wet_walls"));
    CHECK(!hasRow(findSettings("живые персонажи", Lang::Ru, Mode::Detailed), "effects.torch"));
}

TEST(search_one_setting_by_its_label_hint_and_old_name) {
    // 2.6.22: the console's letters search by the same words as the app's search
    const IniSetting& rim = *findSetting("effects.rim_light");
    CHECK(settingMatches(rim, "край", Lang::Ru));        // its label now
    CHECK(settingMatches(rim, "контровой", Lang::Ru));   // its name before
    CHECK(settingMatches(rim, "факел", Lang::Ru));       // its hint
    CHECK(!settingMatches(rim, "снег", Lang::Ru));
    CHECK(!settingMatches(rim, "", Lang::Ru));           // no words: nothing
}

// 3.0.11-beta.302 (the review, C): every row of «Інтерфейс» in one of its sections, or the Controller page's - none
// lost, none twice
TEST(interface_sections_hold_every_row) {
    std::vector<std::string> seen;
    for (const InterfaceSection& s : interfaceSections())
        for (const char* k : s.keys) seen.push_back(k);
    for (const char* k : padInterfaceKeys()) seen.push_back(k);
    for (const IniSetting& s : iniSettings()) {
        if (s.page != Page::Interface) continue;
        const std::string key = sectionKey(s);
        CHECK_EQ(int(std::count(seen.begin(), seen.end(), key)), 1);
    }
    for (const std::string& k : seen) CHECK(findSetting(k) != nullptr);
}

// 3.0.11-beta.303 (the review, E): «нове» - the keys not in the known list; a first start (no list) marks none
TEST(new_rows_are_the_unknown_keys) {
    const std::vector<std::string> now{"a.one", "a.two", "b.three"};
    CHECK(newRows("", now).empty());
    const std::vector<std::string> fresh = newRows("a.one\r\nb.three\n", now);
    CHECK(fresh.size() == 1 && fresh[0] == "a.two");
    CHECK(newRows(knownRowsText(now), now).empty());
}

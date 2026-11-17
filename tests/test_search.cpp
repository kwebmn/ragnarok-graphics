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
            CHECK(h.elsewhere);  // 3.0.21-beta.6: it lives in the game's «Налаштування», not on a page of the app
}

TEST(search_hides_developer_rows) {
    CHECK(!hasRow(findSettings("замерять", Lang::Ru, Mode::Detailed), "debug.profile"));
    CHECK(hasRow(findSettings("замерять", Lang::Ru, Mode::Developer), "debug.profile"));
    CHECK(hasRow(findSettings("shader_variants", Lang::En, Mode::Developer), "debug.shader_variants"));
}

// 3.0.21-beta.6: one mode - a basic row and the details folded under it are found alike, on «Екран і рендер»; the
// developers' rows not
TEST(search_in_the_one_mode_finds_basic_rows_and_details) {
    const std::vector<Hit> hits = findSettings("DLSS", Lang::Ru, Mode::Detailed);
    CHECK(hasRow(hits, "quality.dlss"));
    CHECK(hasRow(hits, "quality.dlss_model"));
    CHECK(!hasRow(hits, "quality.dlss_jitter"));
    for (const Hit& h : hits)
        if (h.kind == HitKind::Ini && sectionKey(iniSettings()[h.index]) == "quality.dlss")
            CHECK(h.place.view == View::Screen && !h.elsewhere && h.level == Mode::Detailed);
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
        fog = fog || (h.kind == HitKind::Reg && h.place.view == View::Screen);  // 3.0.21-beta.6: «Екран і рендер»
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
    // 3.0.21-beta.6: four runs at most - a name's matches (the app's rows, then the game's), a hint's (the same way)
    CHECK(runs <= 4);
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

// 3.0.20-beta.6 (the owner on «Моделі нашим рендером»: «где именно?», «нету тм такого»): the app's search finds it by
// its Ukrainian name, on «Гра й чат» (its «Гра» part, «3D-режим»), in the Detailed mode and from the Simple one
TEST(search_finds_the_models_by_our_renderer_switch) {
    for (const Mode m : {Mode::Detailed, Mode::Simple}) {
        bool found = false;
        for (const Hit& h : findSettings("моделі нашим", Lang::Uk, m))
            if (h.kind == HitKind::Ini && sectionKey(iniSettings()[h.index]) == "view3d.own_models") {
                found = true;
                CHECK(h.elsewhere);  // 3.0.21-beta.6: in the game's «Налаштування»
            }
        CHECK(found);
    }
}

// 3.0.21-beta.6 (variant 4, step 4): the app's search finds the rows that live in the game too - changed right in
// the results, each marked «elsewhere» for the line that says where it lives; the app's own rows come first; the
// effects hidden behind «Вигляд умінь» are not found
TEST(search_finds_what_lives_in_the_game_after_the_apps_own) {
    const auto rowOf = [](const Hit& h) {
        return h.kind == HitKind::Ini ? sectionKey(iniSettings()[h.index]) : std::string();
    };
    bool grass = false, dlss = false;
    for (const Hit& h : findSettings("трава", Lang::Uk, Mode::Detailed))
        if (rowOf(h) == "effects.grass") grass = h.elsewhere;
    CHECK(grass);
    for (const Hit& h : findSettings("dlss", Lang::Uk, Mode::Detailed))
        if (rowOf(h) == "quality.dlss") dlss = !h.elsewhere && h.place.view == View::Screen;
    CHECK(dlss);
    // «звук»: the rows named so - the app's («Об'ємний звук») before the game's («Звук сповіщень») - and only then
    // those whose hints alone say it
    int app = -1, game = -1, n = 0;
    for (const Hit& h : findSettings("звук", Lang::Uk, Mode::Detailed)) {
        if (rowOf(h) == "sound.engine") app = n;
        if (rowOf(h) == "notify.sound") game = n;
        ++n;
    }
    CHECK(app >= 0 && game > app);
    // a row named by the word comes before one that only tells of it in its hint, wherever they live
    int named = -1, told = -1;
    n = 0;
    for (const Hit& h : findSettings("згладжування", Lang::Uk, Mode::Detailed)) {
        if (rowOf(h) == "quality.msaa") named = n;  // «Згладжування країв (MSAA)», in the game
        if (rowOf(h) == "quality.dlss") told = n;   // «NVIDIA DLSS», the app's: its hint says it
        ++n;
    }
    CHECK(named >= 0 && (told < 0 || told > named));
    for (const Hit& h : findSettings("маг", Lang::Uk, Mode::Detailed)) CHECK(rowOf(h) != "effects.mage_effects");
    // a trial that applies at once is the console's: found in the developer's mode alone, elsewhere
    CHECK(!hasRow(findSettings("крони", Lang::Uk, Mode::Detailed), "view3d.crown_volume"));
    bool crown = false;
    for (const Hit& h : findSettings("крони", Lang::Uk, Mode::Developer))
        if (rowOf(h) == "view3d.crown_volume") crown = h.elsewhere;
    CHECK(crown);
}

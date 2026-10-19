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
    CHECK(!hasRow(hits, "quality.dlss_model"));  // Detailed
    for (const Hit& h : hits) CHECK(h.place.view == View::Main || h.place.view == View::Effects);
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
    int lastView = -1;
    bool ordered = true;
    const std::vector<View> list = viewsOf(Mode::Detailed);
    for (const Hit& h : hits) {
        int at = 0;
        while (at < int(list.size()) && list[size_t(at)] != h.place.view) ++at;
        ordered = ordered && at >= lastView;
        lastView = at;
    }
    CHECK(ordered);
    CHECK(hits.size() <= 80);
}

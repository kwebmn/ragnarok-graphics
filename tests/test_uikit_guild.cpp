#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/guild_live.hpp"

using namespace uagfx;
namespace gc = uagfx::guildcard;
namespace gl = uagfx::uikit::guild;

namespace {
panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }
}  // namespace

// 3.0.18-beta.102: the guild's card - its six tabs by RmlUi two rows of three, the body layered where core
// guildcard has it: the skills' «+» and «Змінити» hit
TEST(uikit_guild_tabs_live_and_the_body_in_place) {
    gc::Info g;
    std::vector<gc::Member> members;
    gc::More more;
    more.lines = {gc::Line{"Guild Extension", "1", "", true}, gc::Line{"Battle Orders", "0", "", false}};
    gc::Show sh;
    sh.tab = gc::Tab(3);
    gc::Texts t;
    t.title = "Гільдія";
    const char* tabs[gc::kTabs] = {"Інфо", "Члени", "Посади", "Навички", "Вигнані", "Оголошення"};
    for (int i = 0; i < gc::kTabs; ++i) t.tabs[i] = tabs[i];
    gc::Layout L = gc::layout(1920, 1080, 1, sh.tab, more.lines.size());
    static gl::Model m;
    gl::fill(m, L, g, members, more, sh, t);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 500, 100, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    const panel::Box coreTab = L.tabs[4], corePlus = L.plus.empty() ? panel::Box{} : L.plus[0];
    CHECK(gl::refit(d, L));
    CHECK(std::abs(float(L.w) - 380) < 2.5f);
    CHECK(std::abs(L.tabs[4].x - coreTab.x) < 1.5f);  // two rows of three, where core guildcard has them
    CHECK(std::abs(L.tabs[4].y - coreTab.y) < 1.5f);
    CHECK(std::abs(L.plus[0].y - corePlus.y) < 1.5f);  // the body in place
    gc::Hit h = gc::hitTest(L, mid(L.tabs[5]).x, mid(L.tabs[5]).y);
    CHECK(h.part == gc::Part::Tab);
    CHECK_EQ(h.index, 5);
    h = gc::hitTest(L, mid(L.plus[0]).x, mid(L.plus[0]).y);
    CHECK(h.part == gc::Part::Plus);
    CHECK(gc::hitTest(L, mid(L.close).x, mid(L.close).y).part == gc::Part::Close);
    rmlhost::close(d);
}

// 3.0.19-beta.1 (the testers' doc 14.11: «якщо збільшуешь то все пливе» - the guild's card pulled bigger by its
// corner: tabs one a row, the facts' values past its right edge): at 1.6 its own size the card is 1.6 wider, three
// tabs a row where core guildcard has them, and every word of its body inside its glass
TEST(uikit_guild_pulled_bigger_keeps_three_tabs_a_row_and_its_words_inside) {
    gc::Info g;
    g.name = "Plague.Corp";
    g.master = "Another";
    g.territory = "Eight Castles";
    g.level = 32;
    g.most = 76;
    g.exp = 507406102;
    g.nextExp = 994000000;
    std::vector<gc::Member> members;
    gc::More more;
    gc::Show sh;
    sh.tab = gc::Tab::Info;
    gc::Texts t;
    t.title = "Гільдія";
    const char* tabs[gc::kTabs] = {"Інформація", "Учасники", "Посади", "Навички", "Вигнані", "Оголошення"};
    for (int i = 0; i < gc::kTabs; ++i) t.tabs[i] = tabs[i];
    const float size = 1.6f;
    gc::Layout L = gc::layout(1920, 1080, size, sh.tab, 0);
    static gl::Model m;
    gl::fill(m, L, g, members, more, sh, t);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 500, 100, &error);
    CHECK_EQ(error, std::string());
    const panel::Box coreTab = L.tabs[4];
    CHECK(gl::refit(d, L));
    std::printf("  guild at %.1f: %d wide, tab 1 at %.0f,%.0f, tab 4 at %.0f,%.0f (core %.0f,%.0f)\n", size, L.w,
                L.tabs[1].x, L.tabs[1].y, L.tabs[4].x, L.tabs[4].y, coreTab.x, coreTab.y);
    CHECK(std::abs(float(L.w) - 380 * size) < 3.0f);
    CHECK(std::abs(L.tabs[1].y - L.tabs[0].y) < 1.0f);  // three a row
    CHECK(std::abs(L.tabs[4].x - coreTab.x) < 2.0f);
    CHECK(std::abs(L.tabs[4].y - coreTab.y) < 2.0f);
    for (const gc::Words& x : gc::words(L, g, members, more, sh, t))
        CHECK(x.box.x >= 0 && x.box.x + x.box.w <= float(L.w) + 0.5f);
    rmlhost::close(d);
}

// 3.0.19-beta.1: the game's own order when the corner pulls the card bigger - core guildcard at the new size, refit on
// the document as it still is, the model filled from that layout, then laid out: the card grows with its size (refit
// used to hand the old width back to the model - the width stayed, the letters grew past it)
TEST(uikit_guild_grows_when_its_corner_pulls_it_bigger) {
    gc::Info g;
    g.master = "Another";
    g.territory = "Eight Castles";
    g.exp = 507406102;
    g.nextExp = 994000000;
    std::vector<gc::Member> members;
    gc::More more;
    gc::Show sh;
    sh.tab = gc::Tab::Info;
    gc::Texts t;
    t.title = "Гільдія";
    const char* tabs[gc::kTabs] = {"Інформація", "Учасники", "Посади", "Навички", "Вигнані", "Оголошення"};
    for (int i = 0; i < gc::kTabs; ++i) t.tabs[i] = tabs[i];
    gc::Layout L = gc::layout(1920, 1080, 1.0f, sh.tab, 0);
    static gl::Model m;
    gl::fill(m, L, g, members, more, sh, t);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(gl::rml(), [&](Rml::DataModelConstructor& c) { gl::bind(c, m); }, 500, 100, &error);
    CHECK_EQ(error, std::string());
    CHECK(gl::refit(d, L));
    for (int frame = 0; frame < 3; ++frame) {  // the corner pulled to 1.6: a few frames as win/guild_card makes them
        L = gc::layout(1920, 1080, 1.6f, sh.tab, 0);
        gl::refit(d, L);
        gl::fill(m, L, g, members, more, sh, t);
        rmlhost::update();
    }
    L = gc::layout(1920, 1080, 1.6f, sh.tab, 0);
    CHECK(gl::refit(d, L));
    std::printf("  guild pulled to 1.6: %d wide, tabs 0/1 at y %.0f/%.0f\n", L.w, L.tabs[0].y, L.tabs[1].y);
    CHECK(std::abs(float(L.w) - 380 * 1.6f) < 3.0f);
    CHECK(std::abs(L.tabs[1].y - L.tabs[0].y) < 1.0f);
    for (const gc::Words& x : gc::words(L, g, members, more, sh, t))
        CHECK(x.box.x >= 0 && x.box.x + x.box.w <= float(L.w) + 0.5f);
    rmlhost::close(d);
}

// 3.0.19-beta.1: «Інформація»'s emblem drawn where core guildcard keeps it (the body's own place); none read yet - a
// quiet frame there in its place
TEST(uikit_guild_info_draws_the_emblem_in_its_place) {
    gc::Info g;
    g.name = "Plague.Corp";
    std::vector<gc::Member> members;
    gc::More more;
    gc::Show sh;
    sh.tab = gc::Tab::Info;
    gc::Texts t;
    t.title = "Гільдія";
    const gc::Layout L = gc::layout(1920, 1080, 1, sh.tab, 0);
    gl::Model m;
    gl::fill(m, L, g, members, more, sh, t, "uaui-picture:emblem");
    CHECK_EQ(m.layer.icons.size(), size_t(1));
    if (!m.layer.icons.empty()) {
        CHECK_EQ(m.layer.icons[0].src, Rml::String("uaui-picture:emblem"));
        CHECK(std::abs(m.layer.icons[0].w - L.emblem.w) < 0.01f);
    }
    gl::Model none;
    gl::fill(none, L, g, members, more, sh, t, "");
    CHECK(none.layer.icons.empty());
    bool frame = false;
    for (const auto& b : none.layer.boxes)
        if (std::abs(b.w - L.emblem.w) < 0.01f && std::abs(b.h - L.emblem.h) < 0.01f) frame = true;
    CHECK(frame);
}

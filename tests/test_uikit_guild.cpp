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

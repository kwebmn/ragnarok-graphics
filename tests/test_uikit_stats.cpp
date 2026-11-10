#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/stats_live.hpp"

using namespace uagfx;
namespace sw = uagfx::statwin;
namespace sl = uagfx::uikit::stats;

namespace {
panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }
float measure(const std::string& s, float px, bool bold) { return float(s.size()) * px * (bold ? 0.6f : 0.55f); }
sw::Texts texts() {
    sw::Texts t;
    t.title = "Характеристики";
    t.hotkey = "Alt+A";
    t.free = "Вільні очки: {1}";
    const char* tabs[3] = {"Персонаж", "Планування", "Довідка"};
    for (int i = 0; i < 3; ++i) t.tabs[i] = tabs[i];
    return t;
}
}  // namespace

// 3.0.18-beta.101: «Характеристики» - the head by RmlUi (its tabs, the chip, ×), the body layered: the rows and «+»
// still hit where the body went; «Планування»'s − and + too; the body's words kept, the head's its own
TEST(uikit_stats_head_live_and_the_body_where_it_went) {
    sw::Data d;
    d.free = 5;
    for (int i = 0; i < sw::kStats; ++i) d.base[i] = 10 + i, d.need[i] = 2;
    sw::Show sh;
    sw::Layout L = sw::layout(0, 0, 1, d, texts(), measure, 0);
    static sl::Model m;
    sl::fill(m, L, d, sh, texts(), measure);
    CHECK_EQ(m.chip, std::string("Вільні очки: 5"));
    CHECK(!m.layer.words.empty());
    for (const auto& w : m.layer.words) CHECK(w.text != std::string("Характеристики"));  // the head's own
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(sl::rml(), [&](Rml::DataModelConstructor& c) { sl::bind(c, m); }, 100, 50, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    const panel::Box coreRow = L.rows[2];
    CHECK(sl::refit(doc, L));
    CHECK(std::abs(float(L.w) - 760) < 2.5f);
    CHECK(std::abs(L.rows[2].y - coreRow.y) < 1.5f);  // the head as high as core statwin's: the body in place
    sw::Hit h = sw::hitTest(L, mid(L.pluses[2]).x, mid(L.pluses[2]).y);
    CHECK(h.part == sw::Part::Plus);
    CHECK_EQ(h.index, 2);
    h = sw::hitTest(L, mid(L.tabs[1]).x, mid(L.tabs[1]).y);
    CHECK(h.part == sw::Part::Tab);
    CHECK_EQ(h.index, 1);
    CHECK(sw::hitTest(L, mid(L.close).x, mid(L.close).y).part == sw::Part::Close);
    CHECK(L.tabs[0].x > 100);  // after the title and its key
    CHECK(L.freeChip.x > L.tabs[2].x + L.tabs[2].w);
    rmlhost::close(doc);
}

TEST(uikit_stats_plan_tab_parts_hit) {
    sw::Data d;
    sw::Show sh;
    sw::Layout L = sw::layout(0, 0, 1, d, texts(), measure, sw::kPlanTab);
    static sl::Model m;
    sl::fill(m, L, d, sh, texts(), measure);
    std::string error;
    Rml::ElementDocument* doc =
        rmlhost::load(sl::rml(), [&](Rml::DataModelConstructor& c) { sl::bind(c, m); }, 0, 0, &error);
    CHECK_EQ(error, std::string());
    CHECK(sl::refit(doc, L));
    sw::Hit h = sw::hitTest(L, mid(L.planPlus[3]).x, mid(L.planPlus[3]).y);
    CHECK(h.part == sw::Part::PlanPlus);
    CHECK_EQ(h.index, 3);
    CHECK(sw::hitTest(L, mid(L.planReset).x, mid(L.planReset).y).part == sw::Part::PlanReset);
    rmlhost::close(doc);
}

// 3.0.24-beta.20 (docs/superpowers/plans/2026-11-27-item-windows-old-layout.md, task 1): the item windows' common
// frame laid out by RmlUi with the built-in theme - the head (its title with the count, its key, a drawn ✕), the six
// tabs one under another on the left, the grid beside them, the keys' hint under it, the weight and zeny at the bottom
#include <cmath>
#include <string>

#include "rml_host.hpp"
#include "test.hpp"

namespace {

Rml::Element* one(Rml::ElementDocument* d, const char* selector) { return d ? d->QuerySelector(selector) : nullptr; }
float left(Rml::Element* e) { return e->GetAbsoluteOffset(Rml::BoxArea::Border).x; }
float top(Rml::Element* e) { return e->GetAbsoluteOffset(Rml::BoxArea::Border).y; }
float width(Rml::Element* e) { return e->GetBox().GetSize(Rml::BoxArea::Border).x; }
float height(Rml::Element* e) { return e->GetBox().GetSize(Rml::BoxArea::Border).y; }

}  // namespace

TEST(uikit_item_frame_tabs_stand_left_hint_and_foot_below) {
    std::string error;
    Rml::ElementDocument* d = rmlhost::load(R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw uw" style="width: 330dp; height: 420dp;">
<div class="uhd"><span class="ut">Речі 16/100</span><span class="uk">Alt+E</span><span class="ux"><span
  class="cxa"></span><span class="cxb"></span></span></div>
<div class="ubody"><div class="vt"><div class="vb on">усе</div><div class="vb">витратне</div><div
  class="vb">спорядж.</div><div class="vb">карти</div><div class="vb">різне</div><div class="vb">обране</div></div><div
  class="umain"><div class="ugrid" style="height: 230dp;"></div></div></div>
<div class="uhint">ПКМ — опис · Alt+ПКМ — перенести цілим</div>
<div class="uft"><span class="uwt">Вага 914 / 5 810</span><div class="ubar"><div class="ufill ok"
  style="width: 15%;"></div></div><span class="uz">1 178 780 z</span></div>
</body></rml>)RML",
                                            [](Rml::DataModelConstructor&) {}, 10, 10, &error);
    CHECK_EQ(error, std::string());
    CHECK(d != nullptr);
    if (!d) return;
    Rml::ElementList tabs;
    d->QuerySelectorAll(tabs, ".vt .vb");
    CHECK_EQ(int(tabs.size()), 6);
    for (size_t i = 1; i < tabs.size(); ++i) {  // one under another, at the same left
        CHECK(std::fabs(left(tabs[i]) - left(tabs[0])) < 0.5f);
        CHECK(top(tabs[i]) >= top(tabs[i - 1]) + height(tabs[i - 1]) - 0.5f);
    }
    Rml::Element *grid = one(d, ".ugrid"), *hint = one(d, ".uhint"), *foot = one(d, ".uft"), *head = one(d, ".uhd");
    Rml::Element *x = one(d, ".ux"), *bar = one(d, ".ubar"), *fill = one(d, ".ufill");
    CHECK(grid && hint && foot && head && x && bar && fill);
    if (!grid || !hint || !foot || !head || !x || !bar || !fill) return;
    CHECK(left(tabs[0]) + width(tabs[0]) <= left(grid) + 0.5f);  // the tabs at the grid's left
    CHECK(top(tabs[0]) >= top(head) + height(head) - 0.5f);      // under the head
    CHECK(top(hint) >= top(grid) + height(grid) - 0.5f);         // the hint under the grid
    CHECK(top(foot) >= top(hint) + height(hint) - 0.5f);         // the foot under the hint
    CHECK(width(x) > 8 && height(x) > 8);                         // the drawn ✕ has room
    CHECK(std::fabs(width(fill) - 0.15f * width(bar)) < 1.0f);
    rmlhost::close(d);
}

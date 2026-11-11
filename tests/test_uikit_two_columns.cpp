#include <cmath>
#include <cstdio>
#include <string>

#include "rml_host.hpp"
#include "test.hpp"

namespace {
// The elements of a class in the document, their boxes' corners
Rml::Vector2f at(Rml::ElementDocument* d, const char* cls, int i) {
    Rml::ElementList l;
    d->GetElementsByClassName(l, cls);
    return i < int(l.size()) ? l[size_t(i)]->GetAbsoluteOffset(Rml::BoxArea::Border) : Rml::Vector2f(-1, -1);
}
float widthOf(Rml::ElementDocument* d, const char* cls, int i) {
    Rml::ElementList l;
    d->GetElementsByClassName(l, cls);
    return i < int(l.size()) ? l[size_t(i)]->GetBox().GetSize(Rml::BoxArea::Border).x : -1;
}
}  // namespace

// 3.0.18-beta.140 (R-JA8NME, a dialog clumsy): the NPC's answers and the cards' button grids two a
// row as meant - their padding and border were over 100 % with the widths: one a row, half the window empty
TEST(uikit_npc_answers_and_button_grids_two_a_row) {
    std::string error;
    Rml::ElementDocument* d = rmlhost::load(R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw npw"><div class="opts"><div class="opt"><span class="num">1</span><span class="otext">Yes</span></div>
<div class="opt"><span class="num">2</span><span class="otext">No</span></div>
<div class="opt"><span class="num">3</span><span class="otext">Maybe</span></div></div></body></rml>)RML",
                                            [](Rml::DataModelConstructor&) {}, 100, 100, &error);
    CHECK_EQ(error, std::string());
    CHECK(std::abs(at(d, "opt", 0).y - at(d, "opt", 1).y) < 0.5f);  // 1 and 2 side by side
    CHECK(at(d, "opt", 1).x > at(d, "opt", 0).x + widthOf(d, "opt", 0) - 0.5f);
    CHECK(at(d, "opt", 2).y > at(d, "opt", 0).y + 1);                // 3 on the next row, on the left
    CHECK(std::abs(at(d, "opt", 2).x - at(d, "opt", 0).x) < 0.5f);
    rmlhost::close(d);
    Rml::ElementDocument* g = rmlhost::load(R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw socw"><div class="btns grid4"><div class="btn">Whisper</div><div class="btn">Expel</div>
<div class="btn">Leave the party</div><div class="btn">Settings</div></div></body></rml>)RML",
                                            [](Rml::DataModelConstructor&) {}, 100, 100, &error);
    CHECK_EQ(error, std::string());
    CHECK(std::abs(at(g, "btn", 0).y - at(g, "btn", 1).y) < 0.5f);
    CHECK(std::abs(at(g, "btn", 2).y - at(g, "btn", 3).y) < 0.5f);
    CHECK(at(g, "btn", 2).y > at(g, "btn", 0).y + 1);
    CHECK(at(g, "btn", 0).x < 100 + 40);  // the first at the left (the body at 100), not pushed right
    rmlhost::close(g);
}

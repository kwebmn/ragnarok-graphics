#include <cmath>
#include <cstdio>
#include <string>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/options_live.hpp"

using namespace uagfx;
namespace ol = uagfx::uikit::options;

namespace {

int shownOf(Rml::ElementDocument* doc, const char* selector) {
    Rml::ElementList found;
    if (doc) doc->QuerySelectorAll(found, selector);
    int n = 0;
    for (Rml::Element* e : found) n += e->IsVisible(true) ? 1 : 0;  // data-if's hidden ones stay in the document
    return n;
}

ol::RowView toggleRow(const char* label, bool on, const char* group = "") {
    ol::RowView r;
    r.key = label;
    r.label = label;
    r.group = group;
    r.toggle = true;
    r.on = on;
    return r;
}

}  // namespace

// 3.0.21-beta.3 (the concept «Настройки uaRO Graphics», variant 4, step 3): «Налаштування» (Alt+O) laid out by RmlUi
// from the catalogue's lines - the list's entries with their counts, a group's name over its first row, a part
// indented under its row, the line that opens a group's details in a row's place, the padding rows hidden
TEST(uikit_options_groups_parts_and_the_opening_line) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.searchHint = "Пошук у налаштуваннях";
    m.hint = "Усе змінюється одразу";
    m.none = "Нічого не знайдено";
    m.layoutBtn = "Розмістити інтерфейс";  // 3.0.21-beta.7: the console's action moved here
    m.consoleBtn = "Проби й діагностика…";
    m.nav = {ol::NavView{"Як виглядає гра", 9}, ol::NavView{"Звук і сповіщення", 24}, ol::NavView{"Гра й мережа", 0}};
    m.sec = 1;
    m.rows.push_back(toggleRow("Шепіт", true, "Сповіщення"));
    m.rows.push_back(toggleRow("Кінець бафу", false));
    ol::RowView seconds;
    seconds.key = "notify.buff_seconds";
    seconds.label = "За скільки секунд до кінця бафу";
    seconds.slider = true, seconds.min = 3, seconds.max = 60, seconds.value = 10, seconds.vtext = "10 s";
    seconds.sub = true;
    m.rows.push_back(seconds);
    ol::RowView more;
    more.key = "more.7";
    more.label = "Ще 14 налаштувань";
    more.more = true, more.opens = 7;
    m.rows.push_back(more);
    ol::RowView look;
    look.key = "picture.0";
    look.label = "Картинка";
    look.group = "Як виглядає гра";
    look.choices = {ol::ChoiceView{"Як у грі", false}, ol::ChoiceView{"Ремастер", true}};
    m.rows.push_back(look);
    m.rows[1].warn = "Працює лише на Direct3D 12";  // 3.0.21-beta.4: why the row does nothing now
    ol::pad(m.rows, 0);  // as the window does: never a shorter list, every row its twelve choices
    CHECK_EQ(m.rows.size(), ol::kLeastRows);
    for (const ol::RowView& r : m.rows) CHECK_EQ(r.choices.size(), ol::kMostChoices);
    m.shownRows = 5;
    int opened = -1, toggled = -1, layouts = 0;
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& c) {
            ol::bind(c, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* e : {"sec_go", "set_slider", "set_step", "set_choice", "open_console", "opt_close"})
                c.BindEventCallback(e, none);
            c.BindEventCallback("open_layout", [&](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                ++layouts;
            });
            c.BindEventCallback("set_more", [&](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                opened = a.empty() ? -2 : a[0].Get<int>();
            });
            c.BindEventCallback("set_toggle", [&](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                toggled = a.empty() ? -2 : a[0].Get<int>();
            });
        },
        100, 60, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    CHECK(doc != nullptr);
    if (!doc) return;
    // the list on the left: three entries, the chosen one lit, a count beside those that hold rows
    CHECK_EQ(shownOf(doc, ".snav .sn"), 3);
    CHECK_EQ(shownOf(doc, ".snav .sn.on"), 1);
    CHECK_EQ(shownOf(doc, ".snav .cnt"), 2);
    // the pane: four rows with controls and the opening line in a row's place; the padding rows not shown
    CHECK_EQ(shownOf(doc, ".spane .sro"), 4);
    CHECK_EQ(shownOf(doc, ".spane .smore"), 1);
    CHECK_EQ(shownOf(doc, ".spane .sgrp"), 2);  // the groups' names over their first rows
    CHECK_EQ(shownOf(doc, ".spane .sro.sub"), 1);  // the part under its row
    CHECK_EQ(shownOf(doc, ".spane .tg"), 2);
    CHECK_EQ(shownOf(doc, ".spane .tg.on"), 1);
    CHECK_EQ(shownOf(doc, ".spane .chs .ch"), 2);
    CHECK_EQ(shownOf(doc, ".spane .chs .ch.on"), 1);
    CHECK_EQ(shownOf(doc, ".spane .snone"), 0);
    CHECK_EQ(shownOf(doc, ".spane .swn"), 1);  // one row says why it does nothing
    Rml::Element* pane = doc->GetElementById("spane");
    CHECK(pane != nullptr);
    Rml::Element* line = nullptr;  // every row holds the line's element: the shown one is the group's
    {
        Rml::ElementList all;
        doc->QuerySelectorAll(all, ".spane .smore");
        for (Rml::Element* e : all)
            if (!line && e->IsVisible(true)) line = e;
    }
    CHECK(line && line->IsClassSet("ctl"));
    // the part starts to the right of its row's name
    Rml::Element* part = doc->QuerySelector(".spane .sro.sub");
    Rml::ElementList rows;
    doc->QuerySelectorAll(rows, ".spane .sro");
    Rml::Element* first = nullptr;
    for (Rml::Element* e : rows)
        if (!first && e->IsVisible(true)) first = e;
    CHECK(part && first && part->GetAbsoluteLeft() + part->GetBox().GetEdge(Rml::BoxArea::Padding, Rml::BoxEdge::Left) >
                               first->GetAbsoluteLeft() + 8);
    // a click on the line asks to open its group; a click on a switch names its row
    if (line) line->Click();
    CHECK_EQ(opened, 3);
    Rml::Element* knob = doc->QuerySelector(".spane .tg");
    if (knob) knob->Click();
    CHECK_EQ(toggled, 0);
    // the foot: «Розмістити інтерфейс» and the way to the console, side by side at the right
    CHECK_EQ(shownOf(doc, ".sfoot .btn"), 2);
    Rml::ElementList foot;
    doc->QuerySelectorAll(foot, ".sfoot .btn");
    CHECK_EQ(foot.size(), size_t(2));
    if (foot.size() == 2) {
        CHECK(foot[0]->GetAbsoluteLeft() + foot[0]->GetOffsetWidth() <= foot[1]->GetAbsoluteLeft() + 1);
        CHECK(std::abs(foot[0]->GetAbsoluteTop() - foot[1]->GetAbsoluteTop()) < 2);
        foot[0]->Click();
    }
    CHECK_EQ(layouts, 1);
    // nothing found: the note in the rows' place
    for (ol::RowView& r : m.rows) r = ol::RowView{};
    ol::pad(m.rows, m.rows.size());
    m.shownRows = 0;
    rmlhost::update();
    CHECK_EQ(shownOf(doc, ".spane .sro"), 0);
    CHECK_EQ(shownOf(doc, ".spane .smore"), 0);
    CHECK_EQ(shownOf(doc, ".spane .snone"), 1);
    rmlhost::close(doc);
}

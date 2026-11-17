#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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
    ol::fit(m.rows);  // 3.0.21-beta.8: as the window does - the list as long as it is, no spare rows or choices
    CHECK_EQ(m.rows.size(), size_t(5));
    CHECK_EQ(m.rows[4].choices.size(), size_t(2));
    CHECK_EQ(m.rows[0].choices.size(), size_t(0));
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
    // the pane: four rows with controls and the opening line in a row's place
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
    m.rows.clear();
    m.shownRows = 0;
    rmlhost::update();
    CHECK_EQ(shownOf(doc, ".spane .sro"), 0);
    CHECK_EQ(shownOf(doc, ".spane .smore"), 0);
    CHECK_EQ(shownOf(doc, ".spane .snone"), 1);
    rmlhost::close(doc);
}

// 3.0.21-beta.8 (R-SPPSF3 «когда открываю Alt-O начинает сильно гудеть на видеокарте вентилятор»; its log: with a
// long entry open RmlUi meshed 93 geometries anew a frame and the game fell from 136 to 49 frames a second): a window
// shown and left alone costs nothing anew - no geometry compiled, no words made into letters, nothing drawn at
// another place than the frame before
TEST(uikit_options_left_alone_makes_nothing_anew) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.hint = "Діє одразу";
    m.layoutBtn = "Розмістити інтерфейс";
    m.consoleBtn = "Проби й діагностика…";
    m.nav = {ol::NavView{"Як виглядає гра", 9}, ol::NavView{"Картинка", 183}};
    m.sec = 1;
    for (int i = 0; i < 180; ++i) {  // as «Картинка» opened whole
        ol::RowView r = toggleRow(("Рядок " + std::to_string(i)).c_str(), i % 2 == 0, i % 12 == 0 ? "Група" : "");
        r.hint = "Що це дає: перше речення підказки";
        if (i % 5 == 4) {
            r.toggle = false, r.slider = true, r.min = 0, r.max = 100, r.value = 40, r.vtext = "40 %";
        }
        m.rows.push_back(r);
    }
    ol::fit(m.rows);
    m.shownRows = 180;
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& c) {
            ol::bind(c, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* e : {"sec_go", "set_toggle", "set_slider", "set_step", "set_choice", "set_more",
                                  "open_layout", "open_console", "opt_close"})
                c.BindEventCallback(e, none);
        },
        100, 60, &error);
    CHECK(doc != nullptr);
    if (!doc) {
        std::printf("    %s\n", error.c_str());
        return;
    }
    for (int i = 0; i < 3; ++i) rmlhost::frame();  // settled
    rmlhost::Work all;
    for (int i = 0; i < 5; ++i) {
        const rmlhost::Work w = rmlhost::frame();
        all.compiled += w.compiled, all.words += w.words, all.moved += w.moved, all.drawn += w.drawn;
    }
    if (all.compiled || all.words || all.moved)
        std::printf("    5 frames left alone: %d geometries compiled, %d words made, %d moved of %d drawn\n",
                    all.compiled, all.words, all.moved, all.drawn);
    CHECK(all.drawn > 0);
    CHECK_EQ(all.compiled, 0);
    CHECK_EQ(all.words, 0);
    CHECK_EQ(all.moved, 0);
    rmlhost::close(doc);
}

// 3.0.21-beta.8: the lighter markup (wrappers under each list's elements, a slider's and the choices' parts made only
// for the rows that have them) lays the window's parts out where 3.0.21-beta.7's markup did - the boxes measured with
// that markup (the host's stand-in font), each at (x, y, width, height)
TEST(uikit_options_parts_stand_where_they_stood) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.hint = "Діє одразу";
    m.layoutBtn = "Розмістити інтерфейс";
    m.consoleBtn = "Проби й діагностика…";
    m.nav = {ol::NavView{"Як виглядає гра", 9}, ol::NavView{"Звук і сповіщення", 24}};
    m.sec = 1;
    ol::RowView a = toggleRow("Шепіт", true, "Сповіщення");
    a.hint = "Що це дає";
    m.rows.push_back(a);
    ol::RowView b = toggleRow("Кінець бафу", false);
    b.warn = "Працює лише на Direct3D 12";
    m.rows.push_back(b);
    ol::RowView c;
    c.key = "notify.buff_seconds", c.label = "За скільки секунд до кінця бафу";
    c.slider = true, c.min = 3, c.max = 60, c.value = 10, c.vtext = "10 s", c.sub = true;
    m.rows.push_back(c);
    ol::RowView d;
    d.key = "more.7", d.label = "Ще 14 налаштувань", d.more = true, d.opens = 7;
    m.rows.push_back(d);
    ol::RowView e;
    e.key = "picture.0", e.label = "Картинка", e.group = "Як виглядає гра";
    e.choices = {ol::ChoiceView{"Як у грі", false}, ol::ChoiceView{"Краще", false}, ol::ChoiceView{"Ремастер", true}};
    m.rows.push_back(e);
    ol::RowView f = toggleRow("Частина", true);
    f.sub = true;
    m.rows.push_back(f);
    ol::fit(m.rows);
    m.shownRows = 6;
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& cc) {
            ol::bind(cc, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* ev : {"sec_go", "set_toggle", "set_slider", "set_step", "set_choice", "set_more",
                                   "open_layout", "open_console", "opt_close"})
                cc.BindEventCallback(ev, none);
        },
        100, 60, &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    struct Gold {
        const char* selector;
        std::vector<std::array<float, 4>> boxes;
    };
    const Gold gold[] = {
        {".sgrp", {{334.0f, 168.0f, 529.0f, 13.2f}, {334.0f, 353.8f, 529.0f, 13.2f}}},
        {".smore", {{334.0f, 313.4f, 529.0f, 26.4f}}},
        {".sro", {{334.0f, 183.2f, 529.0f, 44.4f}, {334.0f, 227.6f, 529.0f, 44.4f}, {334.0f, 272.0f, 529.0f, 41.4f},
                  {334.0f, 369.0f, 529.0f, 41.4f}, {334.0f, 410.4f, 529.0f, 37.0f}}},
        {".sl", {{334.0f, 190.2f, 479.0f, 15.6f}, {334.0f, 234.6f, 479.0f, 15.6f}, {350.0f, 284.4f, 231.0f, 15.6f},
                 {334.0f, 381.4f, 307.0f, 15.6f}, {350.0f, 420.6f, 463.0f, 15.6f}}},
        {".shn", {{334.0f, 205.8f, 479.0f, 13.8f}}},
        {".swn", {{334.0f, 250.2f, 479.0f, 13.8f}}},
        {".tg", {{825.0f, 193.9f, 38.0f, 22.0f}, {825.0f, 238.3f, 38.0f, 22.0f}, {825.0f, 417.4f, 38.0f, 22.0f}}},
        {".kn", {{844.0f, 196.9f, 16.0f, 16.0f}, {828.0f, 241.3f, 16.0f, 16.0f}, {844.0f, 420.4f, 16.0f, 16.0f}}},
        {"input.ctl", {{130.0f, 110.0f, 734.0f, 30.0f}, {603.0f, 283.2f, 140.0f, 18.0f}}},
        {".sv", {{780.0f, 285.0f, 52.0f, 14.4f}}},
        {".ch", {{753.0f, 281.0f, 27.0f, 22.4f}, {836.0f, 281.0f, 27.0f, 22.4f}, {657.0f, 378.0f, 72.0f, 22.4f},
                 {733.0f, 378.0f, 53.0f, 22.4f}, {790.0f, 378.0f, 73.0f, 22.4f}}},
        {".snone", {}},
        {".sfoot .btn", {{549.0f, 600.0f, 154.0f, 24.4f}, {711.0f, 600.0f, 154.0f, 24.4f}}},
        {".sn", {{123.0f, 156.0f, 190.0f, 29.6f}, {123.0f, 185.6f, 190.0f, 29.6f}}},
    };
    for (const Gold& g : gold) {
        Rml::ElementList found;
        doc->QuerySelectorAll(found, g.selector);
        std::vector<std::array<float, 4>> now;
        for (Rml::Element* el : found) {
            if (!el->IsVisible(true)) continue;
            const Rml::Vector2f at = el->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f size = el->GetBox().GetSize(Rml::BoxArea::Border);
            now.push_back({at.x, at.y, size.x, size.y});
        }
        bool same = now.size() == g.boxes.size();
        for (size_t i = 0; same && i < now.size(); ++i)
            for (int k = 0; k < 4; ++k) same = same && std::fabs(now[i][size_t(k)] - g.boxes[i][size_t(k)]) < 0.6f;
        if (!same) {
            std::printf("    %s now:", g.selector);
            for (const auto& bx : now) std::printf(" {%.1f,%.1f,%.1f,%.1f}", bx[0], bx[1], bx[2], bx[3]);
            std::printf("\n");
        }
        CHECK(same);
    }
    rmlhost::close(doc);
}

namespace {

Rml::ElementDocument* loadPlain(ol::Model& m, std::string* error = nullptr) {
    return rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& c) {
            ol::bind(c, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* e : {"sec_go", "set_toggle", "set_slider", "set_step", "set_choice", "set_more",
                                  "open_layout", "open_console", "opt_close"})
                c.BindEventCallback(e, none);
        },
        100, 60, error);
}

// Rows of every kind in turn: a switch, a slider, a choice of `chips`, the line that opens a group
std::vector<ol::RowView> mixedRows(int count, int chips) {
    std::vector<ol::RowView> rows;
    for (int i = 0; i < count; ++i) {
        ol::RowView r = toggleRow(("Рядок " + std::to_string(i)).c_str(), i % 2 == 0, i % 6 == 0 ? "Група" : "");
        if (i % 4 == 1) r.toggle = false, r.slider = true, r.max = 100, r.value = 30, r.vtext = "30 %";
        if (i % 4 == 2) {
            r.toggle = false;
            for (int c = 0; c < chips; ++c) r.choices.push_back(ol::ChoiceView{"Вибір " + std::to_string(c), c == 0});
        }
        if (i % 4 == 3) r.toggle = false, r.more = true, r.opens = i;
        if (i % 3 == 0) r.hint = "Що це дає";
        rows.push_back(r);
    }
    ol::fit(rows);
    return rows;
}

}  // namespace

// 3.0.21-beta.8 (R-SPPSF3): the list is as long as the entry shown - no rows kept from a longer one. 3.0.18-beta.132
// padded it («srows[1].choices[10].on»: RmlUi read a row or a choice past the end of a list just made shorter - a
// view on a list's own element is updated in no fixed order with the list's view); now each list's elements are plain
// wrappers, their views a level down, updated after the list has let the spare ones go - no warning, no spare rows
TEST(uikit_options_a_shorter_list_leaves_no_warnings) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.nav = {ol::NavView{"Як виглядає гра", 9}, ol::NavView{"Картинка", 183}};
    m.sec = 1;
    m.rows = mixedRows(40, 6);
    m.shownRows = 40;
    std::string error;
    Rml::ElementDocument* doc = loadPlain(m, &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    CHECK_EQ(error, std::string());
    CHECK_EQ(shownOf(doc, ".spane .sro"), 30);
    CHECK_EQ(shownOf(doc, ".spane .smore"), 10);
    CHECK_EQ(shownOf(doc, ".spane .chs .ch"), 60);
    const std::string before = rmlhost::log();
    // another entry: fewer rows, of other kinds at the same places, fewer choices
    m.rows = mixedRows(5, 2);
    std::swap(m.rows[0], m.rows[2]);  // a choice where a switch was, a switch where a choice was
    m.shownRows = 5;
    rmlhost::update();
    if (rmlhost::log() != before) std::printf("    RmlUi: %s", rmlhost::log().substr(before.size()).c_str());
    CHECK_EQ(rmlhost::log(), before);
    CHECK_EQ(shownOf(doc, ".spane .sro"), 4);
    CHECK_EQ(shownOf(doc, ".spane .smore"), 1);
    CHECK_EQ(shownOf(doc, ".spane .chs .ch"), 2);
    CHECK_EQ(shownOf(doc, ".spane input.ctl"), 1);
    // and a longer one again
    m.rows = mixedRows(24, 3);
    m.shownRows = 24;
    rmlhost::update();
    CHECK_EQ(rmlhost::log(), before);
    CHECK_EQ(shownOf(doc, ".spane .sro"), 18);
    CHECK_EQ(shownOf(doc, ".spane .chs .ch"), 18);
    // nothing found: no row at all
    m.rows.clear();
    m.shownRows = 0;
    rmlhost::update();
    CHECK_EQ(rmlhost::log(), before);
    CHECK_EQ(shownOf(doc, ".spane .sro"), 0);
    CHECK_EQ(shownOf(doc, ".spane .snone"), 1);
    rmlhost::close(doc);
}

// 3.0.21-beta.8 (R-SPPSF3: RmlUi walks every element of a shown document each frame - 11 000 of them after a long
// entry, 9 ms a frame): the document holds what the entry shows and little else - a row of a switch costs 15 elements
// at most (36 and its words before, and every row ever shown kept), a slider's and the choices' parts only where a
// row has them
TEST(uikit_options_holds_only_what_it_shows) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.nav = {ol::NavView{"Як виглядає гра", 9}};
    const auto elements = [](Rml::ElementDocument* doc) {
        Rml::ElementList all;
        doc->QuerySelectorAll(all, "*");
        return int(all.size());
    };
    m.rows.clear();
    m.shownRows = 0;
    Rml::ElementDocument* doc = loadPlain(m);
    CHECK(doc != nullptr);
    if (!doc) return;
    const int empty = elements(doc);
    for (int i = 0; i < 100; ++i) m.rows.push_back(toggleRow(("Рядок " + std::to_string(i)).c_str(), true));
    ol::fit(m.rows);
    m.shownRows = 100;
    rmlhost::update();
    const int switches = elements(doc) - empty;
    if (switches > 100 * 15) std::printf("    100 switches: %d elements\n", switches);
    CHECK(switches <= 100 * 15);
    CHECK_EQ(shownOf(doc, ".spane .tg"), 100);
    // the entry left: its rows gone from the document
    m.rows.resize(3);
    m.shownRows = 3;
    rmlhost::update();
    CHECK(elements(doc) - empty <= 3 * 15);
    rmlhost::close(doc);
}

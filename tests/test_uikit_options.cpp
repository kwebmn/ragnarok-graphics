#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/options_live.hpp"
#include "uikit/ring.hpp"

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
// indented under its row. 3.0.21-beta.11 (the owner: «раскрытие в настройках лучше убрать, пускай сразу будет все
// вместе, можно както выделить внутри что это подгрупа»): a group's details stand at once under a small heading
// («Додатково»), set in from the rows over them with a rule on their left - no line that opens or folds
TEST(uikit_options_groups_parts_and_the_details_heading) {
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
    ol::RowView head;  // the small heading over the group's details
    head.label = "Додатково";
    head.subhead = true;
    m.rows.push_back(head);
    ol::RowView trade = toggleRow("Запит обміну", true);
    trade.det = true;
    m.rows.push_back(trade);
    ol::RowView look;
    look.key = "picture.0";
    look.label = "Картинка";
    look.group = "Як виглядає гра";
    look.choices = {ol::ChoiceView{"Як у грі", false}, ol::ChoiceView{"Ремастер", true}};
    m.rows.push_back(look);
    m.rows[1].warn = "Працює лише на Direct3D 12";  // 3.0.21-beta.4: why the row does nothing now
    ol::fit(m.rows);  // 3.0.21-beta.8: as the window does - the list as long as it is, no spare rows or choices
    CHECK_EQ(m.rows.size(), size_t(6));
    CHECK_EQ(m.rows[5].choices.size(), size_t(2));
    CHECK_EQ(m.rows[0].choices.size(), size_t(0));
    m.shownRows = 6;
    int toggled = -1, layouts = 0;
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
    // the pane: five rows with controls, the small heading in a row's place
    CHECK_EQ(shownOf(doc, ".spane .sro"), 5);
    CHECK_EQ(shownOf(doc, ".spane .ssub"), 1);
    CHECK_EQ(shownOf(doc, ".spane .sro.det"), 1);
    CHECK_EQ(shownOf(doc, ".spane .sgrp"), 2);  // the groups' names over their first rows
    CHECK_EQ(shownOf(doc, ".spane .sro.sub"), 1);  // the part under its row
    CHECK_EQ(shownOf(doc, ".spane .tg"), 3);
    CHECK_EQ(shownOf(doc, ".spane .tg.on"), 2);
    CHECK_EQ(shownOf(doc, ".spane .chs .ch"), 2);
    CHECK_EQ(shownOf(doc, ".spane .chs .ch.on"), 1);
    CHECK_EQ(shownOf(doc, ".spane .snone"), 0);
    CHECK_EQ(shownOf(doc, ".spane .swn"), 1);  // one row says why it does nothing
    Rml::Element* pane = doc->GetElementById("spane");
    CHECK(pane != nullptr);
    // the heading is no control (nothing opens): the pad does not stop on it; the detail under it starts to the
    // right of the basic rows, the heading on its line
    Rml::Element* line = nullptr;
    {
        Rml::ElementList all;
        doc->QuerySelectorAll(all, ".spane .ssub");
        for (Rml::Element* e : all)
            if (!line && e->IsVisible(true)) line = e;
    }
    CHECK(line && !line->IsClassSet("ctl"));
    Rml::Element* inner = doc->QuerySelector(".spane .sro.det");
    // the part starts to the right of its row's name
    Rml::Element* part = doc->QuerySelector(".spane .sro.sub");
    Rml::ElementList rows;
    doc->QuerySelectorAll(rows, ".spane .sro");
    Rml::Element* first = nullptr;
    for (Rml::Element* e : rows)
        if (!first && e->IsVisible(true)) first = e;
    CHECK(part && first && part->GetAbsoluteLeft() + part->GetBox().GetEdge(Rml::BoxArea::Padding, Rml::BoxEdge::Left) >
                               first->GetAbsoluteLeft() + 8);
    CHECK(inner && first && line && inner->GetAbsoluteLeft() > first->GetAbsoluteLeft() + 6 &&
          line->GetAbsoluteTop() < inner->GetAbsoluteTop());
    // a click on a switch names its row
    Rml::Element* knob = doc->QuerySelector(".spane .tg");
    if (knob) knob->Click();
    CHECK_EQ(toggled, 0);
    // the foot: 3.0.24-beta.5's «Усе за замовчуванням», «Розмістити інтерфейс» and the way to the console, side by
    // side at the right
    CHECK_EQ(shownOf(doc, ".sfoot .btn"), 3);
    Rml::ElementList foot;
    doc->QuerySelectorAll(foot, ".sfoot .btn");
    CHECK_EQ(foot.size(), size_t(3));
    if (foot.size() == 3) {
        CHECK(foot[1]->GetAbsoluteLeft() + foot[1]->GetOffsetWidth() <= foot[2]->GetAbsoluteLeft() + 1);
        CHECK(std::abs(foot[1]->GetAbsoluteTop() - foot[2]->GetAbsoluteTop()) < 2);
        foot[1]->Click();
    }
    CHECK_EQ(layouts, 1);
    // nothing found: the note in the rows' place
    m.rows.clear();
    m.shownRows = 0;
    rmlhost::update();
    CHECK_EQ(shownOf(doc, ".spane .sro"), 0);
    CHECK_EQ(shownOf(doc, ".spane .ssub"), 0);
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
            for (const char* e : {"sec_go", "set_toggle", "set_slider", "set_step", "set_choice",
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
    d.label = "Додатково", d.subhead = true;
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
            for (const char* ev : {"sec_go", "set_toggle", "set_slider", "set_step", "set_choice",
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
        {".ssub", {{342.0f, 313.4f, 521.0f, 26.2f}}},  // 3.0.21-beta.11: the details' heading (the fold line's place)
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
        {".sfoot .btn", {{519.0f, 600.0f, 22.0f, 24.4f}, {549.0f, 600.0f, 154.0f, 24.4f},
                         {711.0f, 600.0f, 154.0f, 24.4f}}},  // 3.0.24-beta.5: the reset first (no words in the test)
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
            for (const char* e : {"sec_go", "set_toggle", "set_slider", "set_step", "set_choice",
                                  "open_layout", "open_console", "opt_close"})
                c.BindEventCallback(e, none);
        },
        100, 60, error);
}

// Rows of every kind in turn: a switch, a slider, a choice of `chips`, the small heading of a group's details
std::vector<ol::RowView> mixedRows(int count, int chips) {
    std::vector<ol::RowView> rows;
    for (int i = 0; i < count; ++i) {
        ol::RowView r = toggleRow(("Рядок " + std::to_string(i)).c_str(), i % 2 == 0, i % 6 == 0 ? "Група" : "");
        if (i % 4 == 1) r.toggle = false, r.slider = true, r.max = 100, r.value = 30, r.vtext = "30 %";
        if (i % 4 == 2) {
            r.toggle = false;
            for (int c = 0; c < chips; ++c) r.choices.push_back(ol::ChoiceView{"Вибір " + std::to_string(c), c == 0});
        }
        if (i % 4 == 3) r.toggle = false, r.subhead = true;
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
    CHECK_EQ(shownOf(doc, ".spane .ssub"), 10);
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
    CHECK_EQ(shownOf(doc, ".spane .ssub"), 1);
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
    // 3.0.24-beta.2: 16 - the simple page's cards' block (data-if: in the document, hidden) one more a row
    if (switches > 100 * 16) std::printf("    100 switches: %d elements\n", switches);
    CHECK(switches <= 100 * 16);
    CHECK_EQ(shownOf(doc, ".spane .tg"), 100);
    // the entry left: its rows gone from the document
    m.rows.resize(3);
    m.shownRows = 3;
    rmlhost::update();
    CHECK(elements(doc) - empty <= 3 * 16);
    rmlhost::close(doc);
}

// 3.0.21-beta.9 (R-MSAM96 «картинка сильно поменялась, не могу разобратся что включил в настройках»; its log: 17:35:33
// «settings: effects.grade_brightness = 50», then contrast = 100 and saturation = 100 in the next two frames, no hand
// on them; R-SPPSF3's: «quality.render_scale = 50» the same way). RmlUi's slider sends «change» not only when a person
// moves it: also when a model's values reach its element - a row's slider is the element of the row that stood at
// that place before, and its new least or most cuts the value it still holds; the window took that for the person's
// move and wrote it into uaro-graphics.ini under the NEW row's name. A change is the person's only outside RmlUi's
// update (uikit::Applying), and a slider shows its row's value whatever stood there before
TEST(uikit_options_a_slider_is_moved_by_a_person_alone) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.nav = {ol::NavView{"Як виглядає гра", 9}, ol::NavView{"Картинка", 183}};
    const auto slider = [](const char* key, int least, int most, int value) {
        ol::RowView r;
        r.key = key, r.label = key;
        r.slider = true, r.min = least, r.max = most, r.value = value, r.vtext = std::to_string(value) + " %";
        return r;
    };
    // an entry: sliders with small values, as «Різкість» 50 of 0..100 or a share 30 of 0..50
    m.rows = {slider("quality.sharpen", 0, 100, 50), slider("effects.some_share", 0, 50, 30),
              slider("effects.other", 0, 100, 100), slider("effects.low", 0, 40, 10)};
    ol::fit(m.rows);
    m.shownRows = 4;
    std::vector<std::pair<int, int>> moved;
    Rml::ElementDocument* doc = rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& c) {
            ol::bind(c, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* e : {"sec_go", "set_toggle", "set_step", "set_choice", "open_layout",
                                  "open_console", "opt_close"})
                c.BindEventCallback(e, none);
            c.BindEventCallback("set_slider", [&](Rml::DataModelHandle, Rml::Event& e, const Rml::VariantList& a) {
                int value = 0;
                if (!a.empty() && ol::sliderMoved(e, m, a[0].Get<int>(), value))
                    moved.emplace_back(a[0].Get<int>(), value);
            });
        },
        100, 60);
    CHECK(doc != nullptr);
    if (!doc) return;
    const auto shown = [&]() {
        std::vector<int> values;
        Rml::ElementList inputs;
        doc->QuerySelectorAll(inputs, ".spane input.ctl");
        for (Rml::Element* e : inputs) values.push_back(int(std::lround(e->GetAttribute("value", -1.0f))));
        return values;
    };
    CHECK(moved.empty());  // the values that reach new sliders are not a person's
    CHECK(shown() == (std::vector<int>{50, 30, 100, 10}));
    // another entry: other rows at the same places, each with a range that does not hold the value left there -
    // «Яскравість» 100 of 50..150 where 30 stood, a row of 100..200 where 100 of 0..100 stood
    m.rows = {slider("effects.grade_contrast", 50, 150, 108), slider("effects.grade_brightness", 50, 150, 100),
              slider("effects.far", 100, 200, 150), slider("effects.grade_saturation", 0, 200, 115),
              slider("effects.new_fifth", 60, 90, 75)};
    ol::fit(m.rows);
    m.shownRows = 5;
    rmlhost::update();
    if (!moved.empty())
        for (const auto& [row, value] : moved) std::printf("    taken for a move: row %d = %d\n", row, value);
    CHECK(moved.empty());
    CHECK(shown() == (std::vector<int>{108, 100, 150, 115, 75}));  // each slider shows its row's own value
    // and back: the first entry's rows again
    m.rows = {slider("quality.sharpen", 0, 100, 50), slider("effects.some_share", 0, 50, 30),
              slider("effects.other", 0, 100, 100), slider("effects.low", 0, 40, 10)};
    ol::fit(m.rows);
    m.shownRows = 4;
    rmlhost::update();
    CHECK(moved.empty());
    CHECK(shown() == (std::vector<int>{50, 30, 100, 10}));
    // a person's move (the slider's value set outside RmlUi's update, as its drag does): told, with the row and the
    // value; the same value again is no move
    Rml::ElementList inputs;
    doc->QuerySelectorAll(inputs, ".spane input.ctl");
    CHECK_EQ(inputs.size(), size_t(4));
    if (inputs.size() == 4) {
        inputs[1]->SetAttribute("value", 45);
        CHECK(moved == (std::vector<std::pair<int, int>>{{1, 45}}));
        moved.clear();
        inputs[0]->SetAttribute("value", 50);  // what the row holds already
        CHECK(moved.empty());
    }
    rmlhost::close(doc);
}

// 3.0.21-beta.9 (R-MSAM96's log, 3.0.21-beta.8: «options: open 107.1 s - 88 frames a second …, the driver's own work
// 8.04 ms a frame; in the 5 s before it … 0.86 ms» - the lighter document alone did not help): the pad's ring of a
// live window was hidden anew every frame (SetProperty "display: none" with no pad in hand), and RmlUi lays the whole
// document out again when a property that can change the layout is set - even to the value it has. The ring's
// properties are set only when it shows, hides or moves (uikit/ring)
TEST(uikit_ring_is_placed_only_when_it_changes) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.nav = {ol::NavView{"Картинка", 183}};
    m.rows = mixedRows(60, 3);
    m.shownRows = 60;
    Rml::ElementDocument* doc = loadPlain(m);
    CHECK(doc != nullptr);
    if (!doc) return;
    Rml::Element* ring = doc->GetElementById("ring");
    CHECK(ring != nullptr);
    if (!ring) return;
    for (int i = 0; i < 3; ++i) rmlhost::frame();
    // what 3.0.21-beta.8 did each frame: the document laid out again (its words measured), nothing changed
    ring->SetProperty("display", "none");
    rmlhost::frame();
    ring->SetProperty("display", "none");
    const rmlhost::Work again = rmlhost::frame();
    CHECK(again.measured > 0);
    // the ring's keeper: hidden once, then nothing set - the frames measure nothing
    uikit::Ring keeper;
    keeper.place(ring, nullptr, 3.0f);
    rmlhost::frame();
    int measured = 0, set = 0;
    for (int i = 0; i < 5; ++i) {
        set += keeper.place(ring, nullptr, 3.0f) ? 1 : 0;
        measured += rmlhost::frame().measured;
    }
    CHECK_EQ(set, 0);
    CHECK_EQ(measured, 0);
    // the pad takes a control: shown round it, once
    const panel::Box box{40, 60, 120, 24};
    CHECK(keeper.place(ring, &box, 3.0f));
    rmlhost::frame();
    CHECK(ring->IsVisible());
    CHECK(std::fabs(ring->GetProperty("left")->Get<float>() - 37.0f) < 0.01f);
    CHECK(std::fabs(ring->GetProperty("width")->Get<float>() - 126.0f) < 0.01f);
    set = measured = 0;
    for (int i = 0; i < 5; ++i) {
        set += keeper.place(ring, &box, 3.0f) ? 1 : 0;
        measured += rmlhost::frame().measured;
    }
    CHECK_EQ(set, 0);
    CHECK_EQ(measured, 0);
    // another control, another scale of the same one: moved
    const panel::Box next{40, 96, 120, 24};
    CHECK(keeper.place(ring, &next, 3.0f));
    CHECK(keeper.place(ring, &next, 4.0f));
    CHECK(!keeper.place(ring, &next, 4.0f));
    CHECK(keeper.place(ring, nullptr, 4.0f));
    rmlhost::frame();
    CHECK(!ring->IsVisible());
    rmlhost::close(doc);
}

// 3.0.21-beta.10 (the owner: «надо кнопку по умолчанию сделать. чтобы не искать»; «если надо перегружать для этого
// игру то надо делать подсказку»): a group whose rows are off their defaults shows «Типові» at the right end of its
// name - a click names the row that heads the group; an entry that does nothing now says so over its rows
TEST(uikit_options_a_group_s_way_back_and_the_entry_s_note) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.nav = {ol::NavView{"Як виглядає гра", 9}, ol::NavView{"3D-режим", 22}, ol::NavView{"Змінені", 3}};
    m.sec = 1;
    ol::RowView a = toggleRow("Стеля в приміщеннях", true, "Приміщення");
    ol::RowView b = toggleRow("Склепіння", true);
    ol::RowView c = toggleRow("Кут огляду", false, "Камера й приціл");
    c.toggle = false, c.slider = true, c.min = 50, c.max = 110, c.value = 50, c.vtext = "50";
    c.reset = "Типові";  // its group has rows off their defaults
    m.rows = {a, b, c};
    ol::fit(m.rows);
    m.shownRows = 3;
    std::vector<int> resets;
    Rml::ElementDocument* doc = rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& cc) {
            ol::bind(cc, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* e : {"sec_go", "set_toggle", "set_slider", "set_step", "set_choice",
                                  "open_layout", "open_console", "opt_close"})
                cc.BindEventCallback(e, none);
            cc.BindEventCallback("set_reset", [&](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& v) {
                resets.push_back(v.empty() ? -1 : v[0].Get<int>());
            });
        },
        100, 60);
    CHECK(doc != nullptr);
    if (!doc) return;
    CHECK_EQ(shownOf(doc, ".spane .sgrp"), 2);
    CHECK_EQ(shownOf(doc, ".spane .srst"), 1);  // only the group with something to bring back
    CHECK_EQ(shownOf(doc, ".spane .snote"), 0);
    Rml::Element* link = nullptr;
    Rml::Element* name = nullptr;
    {
        Rml::ElementList all;
        doc->QuerySelectorAll(all, ".spane .srst");
        for (Rml::Element* e : all)
            if (!link && e->IsVisible(true)) link = e;
        if (link) name = link->GetParentNode();
    }
    CHECK(link && link->IsClassSet("ctl"));  // the pad reaches it
    if (link && name) {
        // at the right end of the group's line, on the line of its name
        const float right = link->GetAbsoluteLeft() + link->GetOffsetWidth();
        const float lineRight = name->GetAbsoluteLeft() + name->GetOffsetWidth();
        CHECK(std::fabs(right - lineRight) < 1.5f);
        CHECK(std::fabs((link->GetAbsoluteTop() + link->GetOffsetHeight() / 2) -
                        (name->GetAbsoluteTop() + name->GetOffsetHeight() / 2)) < 3.0f);
        link->Click();
    }
    CHECK(resets == (std::vector<int>{2}));
    // the entry's note: over the rows, pushing them down
    Rml::ElementList rowsBefore;
    doc->QuerySelectorAll(rowsBefore, ".spane .sro");
    const float firstTop = rowsBefore.empty() ? 0 : rowsBefore[0]->GetAbsoluteTop();
    m.note = "Ці налаштування діють у 3D-режимі (Ctrl+Shift+V). Зараз звичайний вигляд: змін не буде видно.";
    rmlhost::update();
    CHECK_EQ(shownOf(doc, ".spane .snote"), 1);
    Rml::ElementList rowsAfter;
    doc->QuerySelectorAll(rowsAfter, ".spane .sro");
    CHECK(!rowsAfter.empty() && rowsAfter[0]->GetAbsoluteTop() > firstTop + 10);
    Rml::Element* note = doc->QuerySelector(".spane .snote");
    CHECK(note && !rowsAfter.empty() && note->GetAbsoluteTop() < rowsAfter[0]->GetAbsoluteTop());
    rmlhost::close(doc);
}

// 3.0.21-beta.11: with no folding every entry is long («Картинка» 183 rows), and RmlUi walks, clips and hands over
// every shown element each frame, in the pane's view or not. The rows far from the view are hidden from its drawing
// (their place kept): a frame draws what is in the view and a view's height round it
TEST(uikit_options_draws_only_the_rows_near_the_view) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.nav = {ol::NavView{"Картинка", 183}};
    for (int i = 0; i < 180; ++i) {
        ol::RowView r = toggleRow(("Рядок " + std::to_string(i)).c_str(), i % 2 == 0, i % 12 == 0 ? "Група" : "");
        r.hint = "Що це дає";
        m.rows.push_back(r);
    }
    ol::fit(m.rows);
    m.shownRows = 180;
    Rml::ElementDocument* doc = loadPlain(m);
    CHECK(doc != nullptr);
    if (!doc) return;
    Rml::Element* pane = doc->GetElementById("spane");
    CHECK(pane != nullptr);
    if (!pane) return;
    for (int i = 0; i < 3; ++i) rmlhost::frame();
    const int whole = rmlhost::frame().drawn;  // every row handed over, clipped or not
    const int hidden = ol::cull(doc);
    CHECK(hidden > 120);  // most of the 180 rows are far under the view
    rmlhost::frame();
    const rmlhost::Work near = rmlhost::frame();
    if (near.drawn * 3 > whole) std::printf("    drawn %d of %d\n", near.drawn, whole);
    CHECK(near.drawn * 3 < whole);
    CHECK_EQ(near.measured, 0);  // hiding lays nothing out again
    CHECK_EQ(ol::cull(doc), hidden);  // nothing moved: the same rows, no class set anew
    const auto rowsShown = [&]() {
        std::vector<int> shown;
        Rml::ElementList rows;
        doc->QuerySelectorAll(rows, ".spane .sro");
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i]->IsVisible(true)) shown.push_back(int(i));
        return shown;
    };
    std::vector<int> top = rowsShown();
    CHECK(!top.empty() && top.front() == 0 && top.back() < 60);
    // every row in the pane's view is drawn
    const float viewTop = pane->GetAbsoluteTop(), viewBottom = viewTop + pane->GetClientHeight();
    Rml::ElementList rows;
    doc->QuerySelectorAll(rows, ".spane .sro");
    for (Rml::Element* r : rows) {
        const float y = r->GetAbsoluteTop();
        if (y + r->GetOffsetHeight() > viewTop && y < viewBottom) CHECK(r->IsVisible(true));
    }
    // scrolled to the end: the last rows drawn, the first hidden; the rows keep their places (the scroll's length)
    const float length = pane->GetScrollHeight();
    pane->SetScrollTop(length);
    rmlhost::frame();
    ol::cull(doc);
    rmlhost::frame();
    CHECK(std::fabs(pane->GetScrollHeight() - length) < 1.0f);
    std::vector<int> bottom = rowsShown();
    CHECK(!bottom.empty() && bottom.back() == 179 && bottom.front() > 120);
    // the rows told anew (another entry, a search, a row's value): the classes are of the layout before - every row
    // shown for that frame, none left hidden in the view; the frame after hides by the new layout
    CHECK_EQ(ol::cull(doc, true), 0);
    rmlhost::frame();
    CHECK_EQ(int(rowsShown().size()), 180);
    CHECK(ol::cull(doc) > 120);
    rmlhost::frame();
    CHECK(int(rowsShown().size()) < 60);
    // a short entry: nothing to hide
    m.rows.resize(6);
    m.shownRows = 6;
    rmlhost::update();
    pane->SetScrollTop(0);
    rmlhost::frame();
    CHECK_EQ(ol::cull(doc), 0);
    rmlhost::frame();
    CHECK_EQ(int(rowsShown().size()), 6);
    rmlhost::close(doc);
}

// 3.0.24-beta.2 (the simple mode, the owner: «там нужно видить возможности», «включать групами»): the simple page -
// no list on the left, the switch's «Просто» lit, the capability cards two in a line with their words, «Детально» and
// a switch; a card's switch and its «Детально» name their row and card
TEST(uikit_options_simple_page_cards_two_in_a_line) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.simple = true;
    m.modeSimple = "Просто";
    m.modeFull = "Повністю";
    m.nav = {ol::NavView{"Як виглядає гра", 9}, ol::NavView{"Картинка", 186}};
    ol::RowView look;
    look.key = "picture.0";
    look.label = "Картинка";
    look.group = "Як виглядає гра";
    look.choices = {ol::ChoiceView{"Як у грі", false}, ol::ChoiceView{"Ремастер", true}};
    m.rows.push_back(look);
    ol::RowView pair;
    pair.key = "cards.effects.group_time";
    pair.group = "Світ";
    pair.cards = {
        ol::CardView{"День, ніч і пори року", "Сонце йде небом", "Детально · 16 ›", true, "effects.group_time"},
        ol::CardView{"Вода", "Сучасна вода", "", false, "effects.group_waters"}};
    pair.ncards = 2;
    m.rows.push_back(pair);
    ol::fit(m.rows);
    m.shownRows = 2;
    int toggled = -1, toggledCard = -1, more = -1, mode = -1;
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& c) {
            ol::bind(c, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* e : {"sec_go", "set_slider", "set_step", "set_choice", "set_toggle", "set_reset",
                                  "open_console", "open_layout", "opt_close"})
                c.BindEventCallback(e, none);
            c.BindEventCallback("card_toggle", [&](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                toggled = a.size() > 1 ? a[0].Get<int>() : -2;
                toggledCard = a.size() > 1 ? a[1].Get<int>() : -2;
            });
            c.BindEventCallback("card_more", [&](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                more = a.size() > 1 ? a[1].Get<int>() : -2;
            });
            c.BindEventCallback("set_mode", [&](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                mode = a.empty() ? -2 : a[0].Get<int>();
            });
        },
        100, 60, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    CHECK(doc != nullptr);
    if (!doc) return;
    CHECK_EQ(shownOf(doc, ".snav .sn"), 0);  // no list on the left
    CHECK_EQ(shownOf(doc, ".sseg span.on"), 1);
    CHECK_EQ(shownOf(doc, ".spane .scard"), 2);
    CHECK_EQ(shownOf(doc, ".spane .scard.off"), 1);
    CHECK_EQ(shownOf(doc, ".spane .scm"), 1);  // «Детально» on the one with a group
    CHECK_EQ(shownOf(doc, ".spane .sro"), 1);  // the look's row; the cards' row has none
    Rml::ElementList cards;
    doc->QuerySelectorAll(cards, ".spane .scard");
    if (cards.size() == 2) {
        CHECK(std::fabs(cards[0]->GetAbsoluteTop() - cards[1]->GetAbsoluteTop()) < 1.0f);  // side by side
        CHECK(cards[1]->GetAbsoluteLeft() > cards[0]->GetAbsoluteLeft() + 100.0f);
    }
    Rml::ElementList toggles;
    doc->QuerySelectorAll(toggles, ".scard .tg");
    if (toggles.size() == 2) toggles[1]->Click();
    rmlhost::update();
    CHECK_EQ(toggled, 1);
    CHECK_EQ(toggledCard, 1);
    Rml::ElementList mores;
    doc->QuerySelectorAll(mores, ".scard .scm");
    if (!mores.empty()) mores[0]->Click();
    rmlhost::update();
    CHECK_EQ(more, 0);
    Rml::ElementList seg;
    doc->QuerySelectorAll(seg, ".sseg span");
    if (seg.size() == 2) seg[1]->Click();
    rmlhost::update();
    CHECK_EQ(mode, 1);
    rmlhost::close(doc);
}

// 3.0.24-beta.3 («Перевіряємо зараз»): a check's card - its steps, no switch when it names none, «Працює ✓» and
// «Є проблема — звіт» naming its row and place
TEST(uikit_options_a_check_s_card_and_its_buttons) {
    static ol::Model m;
    m = ol::Model{};
    m.title = "Налаштування";
    m.simple = true;
    ol::RowView r;
    r.key = "checks.chat_arrows";
    r.group = "Перевіряємо зараз";
    ol::CardView c{"Чат не забирає стрілки", "Стрілки ведуть персонажа", "", false, ""};
    c.check = true, c.toggle = false, c.ok = "Працює ✓", c.bad = "Є проблема — звіт", c.id = "chat_arrows";
    r.cards = {c};
    r.ncards = 1;
    m.rows.push_back(r);
    ol::fit(m.rows);
    m.shownRows = 1;
    int ok = -1, bad = -1;
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& k) {
            ol::bind(k, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* e : {"sec_go", "set_slider", "set_step", "set_choice", "set_toggle", "set_reset",
                                  "open_console", "open_layout", "opt_close", "set_mode", "card_toggle", "card_more"})
                k.BindEventCallback(e, none);
            k.BindEventCallback("check_ok", [&](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                ok = a.size() > 1 ? a[0].Get<int>() * 10 + a[1].Get<int>() : -2;
            });
            k.BindEventCallback("check_bad", [&](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                bad = a.size() > 1 ? a[0].Get<int>() * 10 + a[1].Get<int>() : -2;
            });
        },
        100, 60, &error);
    CHECK_EQ(error, std::string());
    CHECK(doc != nullptr);
    if (!doc) return;
    CHECK_EQ(shownOf(doc, ".scard"), 1);
    CHECK_EQ(shownOf(doc, ".scard .tg"), 0);  // no switch of its own
    CHECK_EQ(shownOf(doc, ".scard .sca .btn"), 2);
    Rml::ElementList buttons;
    doc->QuerySelectorAll(buttons, ".sca .btn");
    if (buttons.size() == 2) {
        buttons[0]->Click();
        buttons[1]->Click();
    }
    rmlhost::update();
    CHECK_EQ(ok, 0);
    CHECK_EQ(bad, 0);
    rmlhost::close(doc);
}

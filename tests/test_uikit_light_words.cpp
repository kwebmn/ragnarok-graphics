// 3.0.24-beta.33 (R-8BCJAH «белый интерфейс не читабелен»: the owner's Alt+O and the HUD in the light theme - titles,
// the list, numbers light on white): every word a window shows readable on what lies under it, in both themes -
// its colour against the grounds under it, as RmlUi works them out from the theme's style sheet
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/ui_theme.hpp"
#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/options_live.hpp"

using namespace uagfx;
namespace ol = uagfx::uikit::options;

namespace {

float channel(float c) { return c <= 0.03928f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
float luminance(float r, float g, float b) {
    return 0.2126f * channel(r) + 0.7152f * channel(g) + 0.0722f * channel(b);
}

struct Rgbf {
    float r = 0, g = 0, b = 0;
};

// The grounds from the document's body down to `e`, laid over `world`
Rgbf groundUnder(Rml::Element* e, Rgbf world) {
    std::vector<Rml::Element*> chain;
    for (Rml::Element* p = e; p; p = p->GetParentNode()) chain.push_back(p);
    Rgbf c = world;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const Rml::Colourb g = (*it)->GetComputedValues().background_color();
        const float a = g.alpha / 255.0f;
        c = {c.r + (g.red / 255.0f - c.r) * a, c.g + (g.green / 255.0f - c.g) * a, c.b + (g.blue / 255.0f - c.b) * a};
    }
    return c;
}

struct Unread {
    std::string text;
    float contrast = 0;
    unsigned colour = 0, ground = 0;
};

void walk(Rml::Element* e, Rgbf world, float least, std::vector<Unread>& out) {
    for (int i = 0; i < e->GetNumChildren(true); ++i) {
        Rml::Element* c = e->GetChild(i);
        if (auto* t = rmlui_dynamic_cast<Rml::ElementText*>(c)) {
            const std::string text = t->GetText();
            if (text.find_first_not_of(" \n\t") == std::string::npos || !e->IsVisible(true)) continue;
            const Rml::Colourb w = e->GetComputedValues().color();
            const Rgbf g = groundUnder(e, world);
            const float a = w.alpha / 255.0f;
            const Rgbf f{g.r + (w.red / 255.0f - g.r) * a, g.g + (w.green / 255.0f - g.g) * a,
                         g.b + (w.blue / 255.0f - g.b) * a};
            const float l1 = luminance(f.r, f.g, f.b), l2 = luminance(g.r, g.g, g.b);
            const float k = (std::max(l1, l2) + 0.05f) / (std::min(l1, l2) + 0.05f);
            if (k < least) {
                const auto hex = [](Rgbf x) {
                    return unsigned(std::lround(x.r * 255)) << 16 | unsigned(std::lround(x.g * 255)) << 8 |
                           unsigned(std::lround(x.b * 255));
                };
                out.push_back(Unread{text.substr(0, 40), k, hex(f), hex(g)});
            }
            continue;
        }
        walk(c, world, least, out);
    }
}

ol::Model optionsModel(const char* ground) {
    ol::Model m;
    m.title = "Налаштування";
    m.searchHint = "Пошук у налаштуваннях";
    m.hint = "Діє одразу";
    m.none = "Нічого не знайдено";
    m.layoutBtn = "Розмістити інтерфейс";
    m.consoleBtn = "Проби й діагностика…";
    m.modeSimple = "Просто", m.modeFull = "Повністю";
    m.nav = {ol::NavView{"Перевіряємо зараз", 46}, ol::NavView{"Інтерфейс", 142}, ol::NavView{"Гра й мережа", 11}};
    m.sec = 1;
    ol::RowView theme;  // the Alt+O rows of R-VMTY6D's picture: the theme's pictures, «Додатково», the accent's chips
    theme.key = "interface.theme";
    theme.label = "Тема інтерфейсу";
    theme.hint = "Темні або світлі вікна для всього інтерфейсу: основний екран, чат і всі вікна.";
    theme.group = "Тема й кольори";
    theme.swatches = {ol::ChoiceView{"Темна", true}, ol::ChoiceView{"Світла", false}};
    m.rows.push_back(theme);
    ol::RowView head;
    head.label = "Додатково";
    head.subhead = true;
    m.rows.push_back(head);
    ol::RowView accent;
    accent.key = "interface.accent";
    accent.label = "Колір акценту";
    accent.hint = "Колір вибраного, головних кнопок і позначок: за темою (золотий на темній, синій на світлій), "
                  "один із чотирьох або свій.";
    accent.det = true;
    for (const char* a : {"за темою", "золотий", "синій", "зелений", "червоний", "свій колір"})
        accent.accents.push_back(ol::ChoiceView{a, a[0] == '\xD0' && a[1] == '\xB7', "#e6cb8a"});
    m.rows.push_back(accent);
    ol::RowView r;
    r.key = "interface.theme_hud";
    r.label = "Основний екран";
    r.hint = "Блок персонажа, панель, мінікарта";
    r.group = "Тема й кольори";
    r.choices = {ol::ChoiceView{"як усе", true}, ol::ChoiceView{"темна", false}, ol::ChoiceView{"світла", false}};
    m.rows.push_back(r);
    ol::fit(m.rows);
    m.shownRows = int(m.rows.size());
    m.ground = ground;
    return m;
}

}  // namespace

TEST(uikit_light_theme_words_readable_options) {
    for (const bool light : {false, true}) {
        uitheme::Look look;
        look.tone = light ? uitheme::Tone::Light : uitheme::Tone::Dark;
        const uitheme::Theme theme = uitheme::themeFor(look, uitheme::Group::Settings);
        rmlhost::theme(&theme);
        static ol::Model m;
        m = optionsModel(light ? "#f6f8fcf0" : "#10141cf0");
        std::string error;
        Rml::ElementDocument* doc = rmlhost::load(
            ol::rml(),
            [&](Rml::DataModelConstructor& c) {
                ol::bind(c, m);
                const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
                for (const char* e : {"sec_go", "set_slider", "set_step", "set_choice", "open_console", "opt_close",
                                      "open_layout", "set_toggle", "set_mode", "set_reset", "set_swatch",
                                      "set_accent", "set_cell"})
                    c.BindEventCallback(e, none);
            },
            100, 60, &error);
        CHECK(doc != nullptr);
        if (!doc) continue;
        rmlhost::update();
        std::vector<Unread> bad;
        walk(doc, light ? Rgbf{0.55f, 0.55f, 0.55f} : Rgbf{0.3f, 0.3f, 0.3f}, 3.0f, bad);
        for (const Unread& u : bad)
            std::fprintf(stderr, "%s: «%s» #%06x on #%06x - %.2f\n", light ? "light" : "dark", u.text.c_str(),
                         u.colour, u.ground, double(u.contrast));
        CHECK(bad.empty());
        rmlhost::close(doc);
    }
    rmlhost::theme(nullptr);
}

// R-VMTY6D («поехала верстка»): «Колір акценту» - its name and words a column one word wide, its chips over them; a
// row's words keep at least 40 % of the row, its control to their right, never over them
TEST(uikit_options_rows_keep_their_words_wide) {
    const uitheme::Theme theme = uitheme::themeFor(uitheme::Look{}, uitheme::Group::Settings);
    rmlhost::theme(&theme);
    static ol::Model m;
    m = optionsModel("#10141cf0");
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& c) {
            ol::bind(c, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* e : {"sec_go", "set_slider", "set_step", "set_choice", "open_console", "opt_close",
                                  "open_layout", "set_toggle", "set_mode", "set_reset", "set_colour", "set_own_hex"})
                c.BindEventCallback(e, none);
        },
        100, 60, &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    rmlhost::update();
    Rml::ElementList rows;
    doc->QuerySelectorAll(rows, ".spane .sro");
    int seen = 0;
    for (Rml::Element* row : rows) {
        if (!row->IsVisible(true)) continue;
        Rml::Element* words = row->QuerySelector(".st");
        Rml::Element* control = row->QuerySelector(".sctl");
        if (!words || !control) continue;
        ++seen;
        const float rw = row->GetBox().GetSize().x, ww = words->GetBox().GetSize().x;
        const float wl = words->GetAbsoluteLeft(), cl = control->GetAbsoluteLeft();
        Rml::Element* name = row->QuerySelector(".sl");
        std::fprintf(stderr, "row «%s»: row %.0f, words %.0f at %.0f, control %.0f at %.0f\n",
                     name ? name->GetInnerRML().c_str() : "?", double(rw), double(ww), double(wl),
                     double(control->GetBox().GetSize().x), double(cl));
        CHECK(ww >= 0.4f * rw);
        CHECK(cl >= wl + ww - 1);
    }
    CHECK(seen >= 3);
    rmlhost::close(doc);
    rmlhost::theme(nullptr);
}

// R-8BCJAH: the theme changed with the windows open - win/uaui clears RmlUi's style sheets and has each document
// reload its own; RmlUi reads the document again from its source («uaui://<model>»), which no file gave («Failed to
// open file to reload style sheet in document: uaui://…»): the words kept the old theme's colours, the grounds (the
// models' own) the new one's. The documents' markup kept by their source (uikit/doc_sources) and given back
TEST(uikit_theme_change_reaches_the_open_windows) {
    const uitheme::Theme dark = uitheme::themeFor(uitheme::Look{}, uitheme::Group::Settings);
    uitheme::Look lightLook;
    lightLook.tone = uitheme::Tone::Light;
    const uitheme::Theme light = uitheme::themeFor(lightLook, uitheme::Group::Settings);
    rmlhost::theme(&dark);
    static ol::Model m;
    m = optionsModel("#10141cf0");
    std::string error;
    Rml::ElementDocument* doc = rmlhost::load(
        ol::rml(),
        [&](Rml::DataModelConstructor& c) {
            ol::bind(c, m);
            const auto none = [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {};
            for (const char* e : {"sec_go", "set_slider", "set_step", "set_choice", "open_console", "opt_close",
                                  "open_layout", "set_toggle", "set_mode", "set_reset", "set_colour", "set_own_hex"})
                c.BindEventCallback(e, none);
        },
        100, 60, &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    rmlhost::update();
    // the light theme chosen while it is open: as win/uaui does it
    rmlhost::theme(&light);
    const size_t before = rmlhost::log().size();
    doc->ReloadStyleSheet();
    m.ground = "#f6f8fcf0";
    rmlhost::update();
    const std::string said = rmlhost::log().substr(before);
    if (!said.empty()) std::fprintf(stderr, "RmlUi after the reload:\n%s", said.c_str());
    CHECK(said.find("rror") == std::string::npos);
    std::vector<Unread> bad;
    walk(doc, Rgbf{0.55f, 0.55f, 0.55f}, 3.0f, bad);
    for (const Unread& u : bad)
        std::fprintf(stderr, "after the change: «%s» #%06x on #%06x - %.2f\n", u.text.c_str(), u.colour, u.ground,
                     double(u.contrast));
    CHECK(bad.empty());
    CHECK(rmlhost::log().find("Failed to open file to reload style sheet") == std::string::npos);
    rmlhost::close(doc);
    rmlhost::theme(nullptr);
}

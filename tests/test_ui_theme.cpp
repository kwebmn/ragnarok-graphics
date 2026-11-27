#include <array>
#include <string>

#include "core/ui_theme.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
float lum(uint32_t c) {
    return (0.2126f * float((c >> 16) & 0xFF) + 0.7152f * float((c >> 8) & 0xFF) + 0.0722f * float(c & 0xFF)) / 255;
}
}  // namespace

// 3.0.18-beta.13 (the concept's themes; docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 6): the parts'
// colours and corners by the theme chosen
TEST(ui_theme_standard_keeps_everything) {
    const uitheme::Theme t = uitheme::named("standard");
    CHECK(uitheme::colour(t, 0x10141C) == 0x10141Cu && uitheme::colour(t, 0x5BD36C) == 0x5BD36Cu);
    CHECK(t.radius == 1.0f && t.edge == 1.0f);
}

// 3.0.24-beta.19 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, the themes): dark and light
// for every window, a tone per group, the accent's colour
TEST(ui_theme_light_turns_grounds_light_and_words_dark) {
    uitheme::Look look;
    look.tone = uitheme::Tone::Light;
    const uitheme::Theme t = uitheme::themeFor(look, uitheme::Group::Items);
    CHECK(lum(uitheme::colour(t, 0x10141C)) > 0.90f);  // the windows' ground
    CHECK(lum(uitheme::colour(t, 0xEEF2F8)) < 0.15f);  // the words
    CHECK(lum(uitheme::colour(t, 0x9AA3B5)) < 0.55f);  // the quiet words stay quieter than the words' ground
    CHECK(lum(uitheme::colour(t, 0x9AA3B5)) > lum(uitheme::colour(t, 0xEEF2F8)));
    // the spec's light: the ground #f6f8fc, the cells white, the words #181c26
    CHECK(uitheme::colour(t, 0x10141C) == 0xF6F8FCu && uitheme::colour(t, 0x1B1D24) == 0xFFFFFFu);
    CHECK(uitheme::colour(t, 0xEEF2F8) == 0x181C26u && uitheme::colour(t, 0xFFFFFF) == 0x181C26u);
    // a colour that means something (HP's green) stays that colour, dark enough to read on the light ground
    const uint32_t g = uitheme::colour(t, 0x5FD08A);
    CHECK(((g >> 8) & 0xFF) > ((g >> 16) & 0xFF) && ((g >> 8) & 0xFF) > (g & 0xFF));
    CHECK(lum(g) < 0.5f);
    CHECK(uitheme::colour(t, 0xC0392B) == 0xC0392Bu);  // dark enough already: as it is
}

TEST(ui_theme_accent_follows_the_tone_or_the_choice) {
    uitheme::Look look;
    CHECK(uitheme::colour(uitheme::themeFor(look, uitheme::Group::Hud), 0xE6CB8A) == 0xE6CB8Au);  // dark: gold
    CHECK(uitheme::colour(uitheme::themeFor(look, uitheme::Group::Hud), 0xC9A55B) == 0xC9A55Bu);
    look.tone = uitheme::Tone::Light;
    const uint32_t blue = uitheme::colour(uitheme::themeFor(look, uitheme::Group::Hud), 0xE6CB8A);
    CHECK(blue == 0x2F5FD0u);  // light: blue
    look.tone = uitheme::Tone::Dark;
    look.accent = 0x2E8B57;
    CHECK(uitheme::colour(uitheme::themeFor(look, uitheme::Group::Hud), 0xE6CB8A) == 0x2E8B57u);  // own: green
    const uint32_t g = uitheme::colour(uitheme::themeFor(look, uitheme::Group::Hud), 0xC9A55B);
    CHECK(((g >> 8) & 0xFF) > ((g >> 16) & 0xFF) && ((g >> 8) & 0xFF) > (g & 0xFF));  // a darker gold: a darker green
    CHECK(lum(g) < lum(0x2E8B57));
    CHECK(uitheme::colour(uitheme::themeFor(look, uitheme::Group::Hud), 0xF0A050) == 0xF0A050u);  // orange: not gold
}

TEST(ui_theme_groups_follow_or_override_the_tone) {
    uitheme::Look look;
    look.tone = uitheme::Tone::Light;
    look.groups[int(uitheme::Group::Hud)] = uitheme::GroupTone::Dark;
    CHECK(uitheme::themeFor(look, uitheme::Group::Hud).greys == uitheme::Greys::Keep);
    CHECK(uitheme::themeFor(look, uitheme::Group::Chat).greys == uitheme::Greys::Light);
    look.tone = uitheme::Tone::Dark;
    look.groups[int(uitheme::Group::Trade)] = uitheme::GroupTone::Light;
    CHECK(uitheme::themeFor(look, uitheme::Group::Trade).greys == uitheme::Greys::Light);
    CHECK(uitheme::themeFor(look, uitheme::Group::Items).greys == uitheme::Greys::Keep);
}

TEST(ui_theme_each_window_has_its_group) {
    using uitheme::Group;
    CHECK(uitheme::groupOf("charblock") == Group::Hud && uitheme::groupOf("hotbar") == Group::Hud);
    CHECK(uitheme::groupOf("minimap") == Group::Hud && uitheme::groupOf("mob hp") == Group::Hud);
    CHECK(uitheme::groupOf("chat") == Group::Chat && uitheme::groupOf("emotes") == Group::Chat);
    CHECK(uitheme::groupOf("items") == Group::Items && uitheme::groupOf("storage") == Group::Items);
    CHECK(uitheme::groupOf("cart") == Group::Items && uitheme::groupOf("equip") == Group::Items);
    CHECK(uitheme::groupOf("shop") == Group::Trade && uitheme::groupOf("vend shop") == Group::Trade);
    CHECK(uitheme::groupOf("trade") == Group::Trade && uitheme::groupOf("my shop") == Group::Trade);
    CHECK(uitheme::groupOf("skills") == Group::Windows && uitheme::groupOf("npc") == Group::Windows);
    CHECK(uitheme::groupOf("options") == Group::Settings);
    CHECK(uitheme::groupOf("party") == Group::Hud && uitheme::groupOf("whisper") == Group::Chat);
    CHECK(uitheme::groupOf("no such window") == Group::Windows);
    CHECK(std::string(uitheme::groupWord(Group::Hud)) == "hud");
    CHECK(std::string(uitheme::groupWord(Group::Trade)) == "trade");
}

TEST(ui_theme_accent_words_and_own_colour) {
    CHECK(uitheme::accentOf("auto", "") == 0u && uitheme::accentOf("gold", "") == 0xE6CB8Au);
    CHECK(uitheme::accentOf("blue", "") == 0x2F5FD0u && uitheme::accentOf("green", "") == 0x2E8B57u);
    CHECK(uitheme::accentOf("red", "") == 0xC0392Bu && uitheme::accentOf("own", "#C86BFF") == 0xC86BFFu);
    CHECK(uitheme::accentOf("own", "#c86bff") == 0xC86BFFu);
    CHECK(uitheme::accentOf("own", "zzz") == 0u && uitheme::accentOf("own", "#12345") == 0u);
    CHECK(uitheme::accentOf("own", "") == 0u && uitheme::accentOf("purple", "") == 0u);
    CHECK(uitheme::accentOf("own", "#12345G") == 0u);
}

TEST(ui_theme_old_names_are_dark) {
    for (const char* n : {"standard", "classic", "contrast", "dark", "unknown"}) {
        const uitheme::Theme t = uitheme::named(n);
        CHECK(t.greys == uitheme::Greys::Keep && t.accent == 0u && t.radius == 1.0f && t.edge == 1.0f);
    }
    CHECK(uitheme::named("light").greys == uitheme::Greys::Light);
}

TEST(ui_theme_palette_and_the_link) {
    CHECK(uitheme::paletteColour(0, 0) != uitheme::paletteColour(2, 0));
    CHECK(lum(uitheme::paletteColour(2, 4)) > lum(uitheme::paletteColour(0, 4)));
    const uint32_t red = uitheme::paletteColour(1, 0);  // the first column: red
    CHECK(((red >> 16) & 0xFF) > ((red >> 8) & 0xFF) && ((red >> 16) & 0xFF) > (red & 0xFF));
    CHECK(uitheme::paletteColour(1, 0) != uitheme::paletteColour(1, 9));
    CHECK(uitheme::themedLink("<link href=\"uaui:theme.rcss\"/>", uitheme::Group::Trade) ==
          "<link href=\"uaui:theme-trade.rcss\"/>");
    CHECK(uitheme::themedLink("<p>no link</p>", uitheme::Group::Hud) == "<p>no link</p>");
}

// the spec: «Текст на кнопке акцента — просто читаемым цветом»: a grey word on the accent's ground turns light or
// dark by that ground
TEST(ui_theme_text_on_the_accent_reads) {
    const std::string css = ".b { background-color: #e6cb8a; color: #10141c; } .c { color: #10141c; }";
    uitheme::Look look;
    CHECK(uitheme::themedCss(uitheme::themeFor(look, uitheme::Group::Items), css) == css);  // dark, gold: as it was
    look.accent = 0x2E8B57;  // a dark green: the word light
    const std::string green = uitheme::themedCss(uitheme::themeFor(look, uitheme::Group::Items), css);
    CHECK(green.find(".b { background-color: #2e8b57; color: #ffffff; }") != std::string::npos);
    CHECK(green.find(".c { color: #10141c; }") != std::string::npos);  // not on the accent: as it was
    look.tone = uitheme::Tone::Light;
    look.accent = 0xF3D27A;  // a light gold on the light theme: the word dark
    const std::string pale = uitheme::themedCss(uitheme::themeFor(look, uitheme::Group::Items), css);
    CHECK(pale.find(".b { background-color: #f3d27a; color: #181c26; }") != std::string::npos);
}

TEST(ui_theme_reads_a_palette_over_the_theme) {
    uitheme::Theme t = uitheme::named("standard");
    std::string bad;
    uitheme::read("# my colours\n#10141C = #202020\nradius = 0.5\nedge = 2\nnonsense\n", t, bad);
    CHECK(uitheme::colour(t, 0x10141C) == 0x202020u);
    CHECK(t.radius == 0.5f && t.edge == 2.0f);
    CHECK_EQ(bad, std::string("nonsense"));
}

// 3.0.18-beta.54 (the owner: «делай все»; the themes in uaUI's markup): a style sheet's colours through the theme,
// their alpha kept; its corners by the theme's radius, its edges by its edge
TEST(ui_theme_rewrites_a_style_sheet) {
    const std::string css = ".a { background-color: #10141ccc; color: #5bd36c; border: 1dp #ffffff17; "
                            "border-radius: 10dp; }";
    CHECK(uitheme::themedCss(uitheme::named("standard"), css) == css);
    const std::string light = uitheme::themedCss(uitheme::named("light"), css);
    CHECK(light.find("#10141ccc") == std::string::npos && light.find("#f6f8fccc") != std::string::npos);  // alpha kept
    CHECK(light.find("#181c2617") != std::string::npos);  // the white line: a dark line
    CHECK(light.find("border-radius: 10dp") != std::string::npos);
    uitheme::Theme t = uitheme::named("light");
    std::string bad;
    uitheme::read("radius = 0.3\nedge = 1.6\n", t, bad);
    const std::string shaped = uitheme::themedCss(t, css);
    CHECK(shaped.find("border-radius: 3dp") != std::string::npos);  // 10 x 0.3
    CHECK(shaped.find("border: 1.6dp") != std::string::npos);
}

// 3.0.24-beta.19 (task 3): each group's style sheet a file of its own - the name the markup's link gets is the one
// uaUI's files answer
TEST(ui_theme_style_file_names_round_trip) {
    for (int i = 0; i < uitheme::kGroups; ++i) {
        const uitheme::Group g = uitheme::Group(i);
        const std::string rml = uitheme::themedLink("<link type=\"text/rcss\" href=\"uaui:theme.rcss\"/>", g);
        const size_t from = rml.find("href=\"") + 6;
        uitheme::Group back = uitheme::Group::Hud;
        CHECK(uitheme::styleGroup(rml.substr(from, rml.find('"', from) - from), back) && back == g);
    }
    uitheme::Group g = uitheme::Group::Hud;
    CHECK(uitheme::styleGroup("uaui:theme.rcss", g) && g == uitheme::Group::Windows);  // a link not rewritten
    CHECK(!uitheme::styleGroup("uaui:window.rml", g) && !uitheme::styleGroup("uaui:theme-nope.rcss", g));
    CHECK(!uitheme::styleGroup("theme.rcss", g));
}

TEST(ui_theme_group_style_differs_by_tone) {
    uitheme::Look look;
    look.tone = uitheme::Tone::Light;
    look.groups[int(uitheme::Group::Hud)] = uitheme::GroupTone::Dark;
    const std::string css = "body { background-color: #10141c; }";
    CHECK(uitheme::themedCss(uitheme::themeFor(look, uitheme::Group::Hud), css) == css);
    CHECK(uitheme::themedCss(uitheme::themeFor(look, uitheme::Group::Chat), css) != css);
    uitheme::Look other = look;
    CHECK(other == look);
    other.accent = 0x2E8B57;
    CHECK(!(other == look));
}

// the colours the windows' code writes (uaUI's own and the live kits') through the group's theme in use
TEST(ui_theme_active_themes_paint_by_group) {
    CHECK(uitheme::paint(0xEEF2F8, uitheme::Group::Items) == 0xEEF2F8u);  // none set: the dark, as drawn
    uitheme::Look look;
    look.groups[int(uitheme::Group::Items)] = uitheme::GroupTone::Light;
    std::array<uitheme::Theme, uitheme::kGroups> themes;
    for (int i = 0; i < uitheme::kGroups; ++i) themes[size_t(i)] = uitheme::themeFor(look, uitheme::Group(i));
    uitheme::setActive(themes);
    CHECK(uitheme::paint(0xEEF2F8, uitheme::Group::Items) == 0x181C26u);
    CHECK(uitheme::paint(0xEEF2F8, uitheme::Group::Hud) == 0xEEF2F8u);
    CHECK(uitheme::active(uitheme::Group::Items).greys == uitheme::Greys::Light);
    uitheme::setActive({});  // as it was for the other tests
    CHECK(uitheme::paint(0xEEF2F8, uitheme::Group::Items) == 0xEEF2F8u);
}

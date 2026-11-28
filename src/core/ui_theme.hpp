#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace uagfx::uitheme {

// 3.0.18-beta.13 (Claude Doc «Концепция: единая библиотека интерфейса uaRO Graphics», the themes): the new
// interface's parts drawn by uaUI (core/ui_scene) in a theme - their colours through it, their corners and edges by
// its factors; uaro-graphics\ui\palette.txt over it («#RRGGBB = #RRGGBB», «radius = 0.5», «edge = 1.5», «#» a
// remark).
// 3.0.24-beta.19 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, the themes): two tones, dark
// (the colours as drawn) and light (the greys turned over: the spec's ground #f6f8fc, cells white, words #181c26);
// a tone per group of windows; the accent (the gold) in a colour chosen. The old classic and contrast: dark.
enum class Greys : uint8_t { Keep, Light };
struct Theme {
    std::string name = "dark";
    Greys greys = Greys::Keep;  // the colours with no hue to speak of (the grounds, the words, the edges)
    uint32_t accent = 0;        // the gold's colours in this one (0: the gold as drawn)
    float radius = 1, edge = 1;
    std::unordered_map<uint32_t, uint32_t> exact;  // the palette's own, before the greys' rule
    // 3.0.24-beta.47 (the owner: «в светлой форме чат должен быть похож на чат с оригинала»): the light tone's chat as
    // the original's - its strip dark (this theme's greys kept), its line white with dark words (win/chat_window)
    bool whiteLine = false;
};

enum class Tone : uint8_t { Dark, Light };
enum class Group : uint8_t { Hud, Chat, Items, Trade, Windows, Settings };
constexpr int kGroups = 6;
enum class GroupTone : uint8_t { Same, Dark, Light };
struct Look {
    Tone tone = Tone::Dark;
    std::array<GroupTone, kGroups> groups{};  // Same: the tone above
    uint32_t accent = 0;                      // 0: the tone's (dark: the gold #e6cb8a, light: the blue #2f5fd0)
};
inline bool operator==(const Look& a, const Look& b) {
    return a.tone == b.tone && a.groups == b.groups && a.accent == b.accent;
}
inline bool operator!=(const Look& a, const Look& b) { return !(a == b); }

Theme named(const std::string& name);  // "light": light; anything else (dark, the old standard, classic...): dark
Theme themeFor(const Look& look, Group g);
uint32_t colour(const Theme& t, uint32_t rgb);  // 0xRRGGBB
// 3.0.24-beta.33 (R-8BCJAH «белый интерфейс не читабелен»: the chat and the character's block see-through, the world
// under them stronger than their words): a ground's alpha in the theme - one that turns light (the windows' dark
// greys) lets a third of what it let through; any other colour (words, edges, fills that mean something), and the
// dark theme, as given
float groundAlpha(const Theme& t, uint32_t rgb, float a);
// The palette's lines over `t`; the first line not understood in `bad` ("" none)
void read(const std::string& text, Theme& t, std::string& bad);
// 3.0.18-beta.54: a style sheet (RCSS) in the theme - each #rrggbb(aa) through colour() (its alpha kept), each
// «border-radius: N» times the radius, each «border: N» / «border-width: N» times the edge; 3.0.24-beta.19: a grey
// word on the accent's ground light or dark by that ground
std::string themedCss(const Theme& t, const std::string& css);

// A window's group by its document's name (core: the names uaUI's documents are made with); unknown: Windows
Group groupOf(const std::string& docName);
const char* groupWord(Group g);  // "hud", "chat", "items", "trade", "windows", "settings"
// [interface] accent's word and accent_colour: auto 0, gold, blue, green, red, own (the colour "#RRGGBB"; bad: 0)
uint32_t accentOf(const std::string& word, const std::string& colour);
uint32_t paletteColour(int row, int col);  // «свій колір»: 3 rows (darker to lighter) x 10 hues
// A document's markup with its group's style sheet: href="uaui:theme.rcss" -> href="uaui:theme-<group>.rcss"
std::string themedLink(const std::string& rml, Group g);
// The group a style sheet's name is for: "uaui:theme-<group>.rcss", or "uaui:theme.rcss" (a link not rewritten: the
// other windows'); false: not a style sheet of ours
bool styleGroup(const std::string& path, Group& g);

// The themes in use, a group each (uaUI sets them as the look changes; none set: dark, the colours as drawn) - the
// colours the windows' code writes into their documents go through these (uaui::themed, the live kits)
void setActive(const std::array<Theme, kGroups>& themes);
const Theme& active(Group g);
uint32_t paint(uint32_t rgb, Group g);  // colour(active(g), rgb)

}  // namespace uagfx::uitheme

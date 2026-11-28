#pragma once
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/ElementDocument.h>

// 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 4; R-8BCJAH): a window's words as RmlUi
// lays and colours them - each one's box, its colour against the grounds under it (over the world's), whether its box
// cuts it; the faint and the cut ones its problems. The MCP's ui tool in the game, the host tests alike
namespace uagfx::uikit::uicheck {

struct Rgbf {
    float r = 0, g = 0, b = 0;
};
struct Word {
    std::string text;  // its first 60 bytes
    float x = 0, y = 0, w = 0, h = 0;  // its element's box on the screen
    unsigned colour = 0, ground = 0;   // as drawn over `world`, 0xRRGGBB
    float contrast = 0;                // WCAG's, 1-21
    bool cut = false;                  // wider than its box, the box clipping it
};
// 3.0.24-beta.50 (the owner's «делай все», item 2; the audit 28.11: the light theme's icons white on white were not
// caught): a picture's (img's) ink - the mean of its solid pixels; false for a picture of many colours (an item's, a
// face) or one not known. Given, the one-colour pictures are checked as words are («icon <src>»)
using InkOf = std::function<bool(const std::string& src, unsigned& rgb)>;
std::vector<Word> words(Rml::ElementDocument* doc, Rgbf world, const InkOf& inkOf = {});
constexpr float kLeastContrast = 3.0f;
// The words under kLeastContrast or cut (`world` the picture under the window - mid grey for the game's)
std::vector<Word> problems(Rml::ElementDocument* doc, Rgbf world = Rgbf{0.45f, 0.45f, 0.45f},
                           const InkOf& inkOf = {});

}  // namespace uagfx::uikit::uicheck

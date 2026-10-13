#pragma once
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings {

// The settings app's pages in the order of its list (stage 2 spec §6.1; Sound, Game, Input and dgVoodoo come with
// 0.6.1 and 0.6.2).
enum class Page : uint8_t { Render, Screen, Quality, Frames, Diagnostics, Service };
constexpr int kPageCount = 6;
Text pageName(Page p);

enum class Control : uint8_t { Check, Slider, Choice, Number };

// One key of uaro-graphics.ini the pages edit one to one. The Screen page writes [display] mode, monitor,
// window_width and window_height from its own plan (spec §5.3).
struct IniSetting {
    const char* section;
    const char* key;
    Page page;
    Control control;
    int min;
    int max;
    int step;
    std::vector<const char*> tokens;  // Check: {off, on}; Choice: the file's words in the order of `choices`
    std::vector<Text> choices;        // Choice
    Text label;
    Text hint;
    const char* unit;                 // after a slider's number
};
const std::vector<IniSetting>& iniSettings();
const IniSetting* findSetting(const std::string& sectionKey);
std::string sectionKey(const IniSetting& s);  // "quality.sharpen"
// The value the driver uses for it, as the word the file holds: "1", "150", "sharp".
std::string tokenOf(const IniSetting& s, const Config& c);
// Whether a word is one this setting takes (a number in its range, one of its tokens).
bool validToken(const IniSetting& s, const std::string& token);

}  // namespace uagfx::settings

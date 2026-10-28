#pragma once
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings {

// The settings app's pages in the order of its list (stage 2 spec §6.1): Chat holds the whisper, login and chat macro
// rows the Game page has no room for; Smoothing comes with stage 4 (spec §13), Input and dgVoodoo with 0.6.3; Ai (the
// [ai] section) and AiCache (what the texture pack holds, 0.48) with 0.12; Effects (the [effects] section) with 0.18;
// Pack (the texture pack's builder) with 0.43; Pad (uaRO Pad, the gamepad) with 1.4; Debug (2.6) the developer's
// switches - the Developer mode's page.
enum class Page : uint8_t {
    Render, Screen, Quality, Smoothing, Effects, Ai, AiCache, Pack, Frames, Sound, Pad, Game, Chat, Diagnostics,
    Service, Debug, Launcher,  // 2.11: the launcher «Гра»'s rows
    Interface                  // 3.0.11-beta.3: the new interface's switches (its own item)
};
constexpr int kPageCount = 18;
Text pageName(Page p);

enum class Control : uint8_t { Check, Slider, Choice, Number, Line };  // Line: a line of text

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
    // 2.2.2: a switch or a number of [effects] bound to its field - parseConfig reads it and tokenOf gives it by this
    // entry (a number taken from min to max, else the default stays), not by branches of their own
    bool EffectSettings::*flag = nullptr;
    int EffectSettings::*number = nullptr;
};
const std::vector<IniSetting>& iniSettings();
const IniSetting* findSetting(const std::string& sectionKey);
std::string sectionKey(const IniSetting& s);  // "quality.sharpen"
// The value the driver uses for it, as the word the file holds: "1", "150", "sharp".
std::string tokenOf(const IniSetting& s, const Config& c);
// Whether a word is one this setting takes (a number in its range, one of its tokens).
bool validToken(const IniSetting& s, const std::string& token);

}  // namespace uagfx::settings

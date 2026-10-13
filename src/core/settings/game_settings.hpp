#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/settings/game_registry.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings {

// How a value of the game's key reads as a page's word (docs/research/registry.md §1, §4, §5, §6).
enum class RegKind : uint8_t {
    Number,   // a REG_DWORD as it is: a choice's stored number, a slider's value
    Flag,     // "1" when not 0
    FlagOne,  // "1" only when exactly 1: the game tests == 1 (ISLIGHTMAP, CHECKSAVE)
    NotFlag,  // "1" when 0: the music on is bgmIsPaused = 0, the intro on is BINKMODE = 0
    Argb,     // the minimap: "1" (translucent, 0xAAFFFFFF) unless 0xFFFFFFFF (opaque)
    Line,     // REG_SZ
};

// One value of the game's key a page edits (stage 2 spec §6.1, release 0.6.2).
struct RegSetting {
    const char* subkey;  // "" = the key itself
    const char* name;
    Page page;
    Control control;
    RegKind kind;
    uint32_t fallback;             // the game's own value when it is missing (registry.md)
    std::vector<uint32_t> values;  // Choice of a Number: the stored numbers in the order of `choices`
    std::vector<Text> choices;
    int min;  // Slider
    int max;  // Slider; Line: the most letters
    int step;
    Text label;
    Text hint;  // the label itself when there is none
    bool gameWrites;           // the game writes it (at exit, or from its windows): only while it is closed (§4.1)
    const char* fallbackText;  // Line: the game's own text when it is missing
};
const std::vector<RegSetting>& regSettings();
const RegSetting* findRegSetting(const std::string& key);  // "SPRITEMODE", "Option\\isEffectOn"
std::string regKey(const RegSetting& s);
// What the game reads now, as a page's word - "1", "2", "100", the text - with the game's own rules for odd values:
// a choice outside its list is its highest, a slider is clamped.
std::string regWordOf(const RegSetting& s, const GameRegistry& r);
bool validRegWord(const RegSetting& s, const std::string& word);
RegValue regValueOf(const RegSetting& s, const std::string& word);  // what a valid word is written as
// The game's own chat macro of Alt+1 ... Alt+0 (index 0 ... 9), used while its value is empty (registry.md §6).
const char* builtInMacro(int index);

}  // namespace uagfx::settings

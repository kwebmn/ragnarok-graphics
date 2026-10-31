#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace uapad {

// The language of everything the player reads: the Start menu (the log stays English).
enum class Lang : uint8_t { En, Uk, Ru };
constexpr int kLangCount = 3;

std::optional<Lang> parseLang(const std::string& token);  // "en", "uk", "ru", any case
const char* langToken(Lang l);                            // in uaro-pad.ini
const char* langName(Lang l);                             // in its own language: "English", "Українська", "Русский"
Lang nextLang(Lang l, int dir);                           // the menu's ring: en, uk, ru

enum class Text : uint16_t {
    TabButtons,
    TabSettings,
    DefaultLayout,  // the menu title before the character is known
    Character,      // "character" + its id when the name is unknown
    ResetButtons,
    ResetSettings,
    Save,
    Cancel,
    Help,  // two lines
    Attack,
    Slot,
    Row,
    Pickup,
    SitStand,
    ClearTarget,
    CameraToggle,
    Language,
    Lock,
    FocusClick,
    MiddleButton,
    MouseButton4,
    MouseButton5,
    Borderless,
    SaveCpu,
    Rumble,
    Mark,
    WalkRadius,
    On,
    Off,
    FocusStaysOut,
    FocusGoesIn,
    MarkGasp,
    MarkSight,
    MarkRuwach,
    MarkPoison,
    MarkDeadlyPoison,
    MarkCurse,
    MarkStun,
    MarkSleep,
    MarkSilence,
    Talk,  // 0.7: the action, and the NPC window's hint: {A}, {B}, {LB}, {RB} are the buttons' names
    HintNext,
    HintClose,
    HintWait,
    HintMenu,
    HintNumber,
    Reference,  // 3.0.11-beta.22: the action opening uaRO Graphics' «Довідник»
    HintChoose,  // 3.0.11-beta.85: a support skill asks whom: {A}, {B} and {KEY}, the skill's own button
    HintWindow,  // 3.0.11-beta.93: a window B closes is open (the big map, the game's equipment, skills ...)
    Count
};

const char* tr(Lang l, Text t);  // UTF-8

}  // namespace uapad

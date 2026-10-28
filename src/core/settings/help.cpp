#include "core/settings/help.hpp"

#include <string>

#include "core/settings/game_settings.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::settings {
namespace {

// A row a page builds itself rather than from the schema: its view, the first mode that shows it, and the schema's row
// it follows on the page - "" its top, nullptr its end
struct OwnRow {
    View view;
    Mode from;
    const char* after;
    HelpRow row;
};

const OwnRow kOwnRows[] = {
    {View::Play, Mode::Simple, "", {Text::UpdateChannel, Text::UpdateChannelHint}},
    {View::Main, Mode::Simple, "", {Text::PageQuality, Text::MainQualityHint}},
    {View::Screen, Mode::Simple, "", {Text::ScreenShow, Text::ScreenShowHint}},
    {View::Screen, Mode::Simple, "", {Text::ScreenMonitor, Text::ScreenMonitorHint}},
    {View::Screen, Mode::Simple, "", {Text::ScreenUi, Text::ScreenUiHint}},
    {View::Screen, Mode::Simple, "", {Text::ScreenWindowSize, Text::ScreenWindowSizeHint}},
    {View::Ai, Mode::Simple, "ai.enabled", {Text::AiPreset, Text::AiPresetHint}},
    {View::Ai, Mode::Simple, "ai.ahead", {Text::AiModelSet, Text::AiModelSetHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::AiPreviewGroup, Text::AiPreviewHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::AiPreviewZoom, Text::AiPreviewZoomHint}},
    // the pack's tab: making the pack is the developer's (2.15: it comes with the full client)
    {View::Ai, Mode::Developer, nullptr, {Text::PackWorld, Text::PackWorldHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackEffects, Text::PackEffectsHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackMonsters, Text::PackMonstersHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackPlayers, Text::PackPlayersHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackNpcs, Text::PackNpcsHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackItems, Text::PackItemsHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackInterface, Text::PackInterfaceHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackBodyColours, Text::PackBodyColoursHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackQuality, Text::PackQualityHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackRedo, Text::PackRedoHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackMapName, Text::PackMapNameHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackMapGround, Text::PackMapGroundHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackMapModels, Text::PackMapModelsHint}},
    {View::Ai, Mode::Developer, nullptr, {Text::PackMapModel, Text::PackMapModelHint}},
    {View::Pad, Mode::Simple, "pad.enabled", {Text::PadType, Text::PadTypeHint}},
    {View::Pad, Mode::Simple, "pad.enabled", {Text::PadLightbar, Text::PadLightbarHint}},
    {View::Pad, Mode::Simple, "pad.enabled", {Text::PadTouchpad, Text::PadTouchpadHint}},
    {View::Pad, Mode::Simple, "pad.enabled", {Text::PadRumble, Text::PadRumbleHint}},
    {View::Pad, Mode::Simple, "pad.enabled", {Text::PadStartsOn, Text::PadStartsOnHint}},
    {View::Pad, Mode::Simple, "pad.enabled", {Text::PadMenuLanguage, Text::PadMenuLanguageHint}},
    {View::Pad, Mode::Simple, "pad.enabled", {Text::PadNpcHint, Text::PadNpcHintHint}},
};

// The Effects page's head in its order (the mode not in Simple: a set decides it there); the groups follow
const char* const kEffectsHead[] = {"effects.preset", "effects.tier",   "effects.clock",
                                    "effects.night_strength", "effects.season", "effects.weather_kind"};

// The schema's rows «Головне» builds by name (page_main.cpp), in every mode: only the machine decides
const char* const kMainPageKeys[] = {"quality.dlss", "frames.frame_generation", "frames.fps_limit",
                                     "frames.vsync", "display.hdr",             "sound.engine",
                                     "pad.enabled",  "frames.battery_saver",    "ai.enabled"};

bool inView(Page page, View v) {
    for (int tab = 0; tab < tabCount(v); ++tab)
        for (Page p : partsOf(v, tab))
            if (p == page) return true;
    return false;
}

bool endsWith(const std::string& s, const char* tail) {
    const std::string t(tail);
    return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

template <size_t N>
std::vector<const IniSetting*> byName(const char* const (&keys)[N], const Machine& machine) {
    std::vector<const IniSetting*> out;
    for (const char* k : keys)
        if (const IniSetting* s = findSetting(k); s && fitsMachine(k, machine)) out.push_back(s);
    return out;
}

// The schema's rows a view shows: «Гра», «Головне» and «Геймпад» theirs by name, the Effects page its head and its
// groups (each group's effects are on the page itself), every other page the rows of its level and machine
std::vector<const IniSetting*> schemaRows(View v, Mode mode, const Machine& machine) {
    static const char* const kPlay[] = {"launcher.direct_start"};
    static const char* const kPad[] = {"pad.enabled"};
    if (v == View::Play) return byName(kPlay, machine);
    if (v == View::Main) return byName(kMainPageKeys, machine);
    if (v == View::Pad) return byName(kPad, machine);
    std::vector<const IniSetting*> out;
    if (v == View::Effects) {
        for (const char* k : kEffectsHead)
            if (const IniSetting* s = findSetting(k); s && (mode != Mode::Simple || std::string(k) != "effects.tier"))
                out.push_back(s);
        const IniSetting* strength = nullptr;  // the same slider under every group: once, after them
        for (const IniSetting& s : iniSettings()) {
            const std::string k = sectionKey(s);
            if (k.rfind("effects.group_", 0) != 0) continue;
            if (!endsWith(k, "_strength"))
                out.push_back(&s);
            else if (!strength)
                strength = &s;
        }
        if (strength) out.push_back(strength);
        return out;
    }
    for (const IniSetting& s : iniSettings()) {
        const std::string k = sectionKey(s);
        if (levelOf(s) <= mode && fitsMachine(k, machine) && inView(s.page, v)) out.push_back(&s);
    }
    return out;
}

}  // namespace

std::vector<HelpRow> helpRows(View v, Mode mode, const Machine& machine) {
    std::vector<HelpRow> out;
    const auto add = [&](const HelpRow& r) {
        for (const HelpRow& x : out)
            if (x.label == r.label) return;
        out.push_back(r);
    };
    const auto own = [&](const std::string& after, bool end) {
        for (const OwnRow& o : kOwnRows)
            if (o.view == v && o.from <= mode && (o.after ? after == o.after : end)) add(o.row);
    };
    own("", false);
    for (const IniSetting* s : schemaRows(v, mode, machine)) {
        add({s->label, s->hint});
        own(sectionKey(*s), false);
    }
    if (v != View::Main)
        for (const RegSetting& r : regSettings())
            if (inView(r.page, v)) add({r.label, r.hint});
    own("", true);
    for (const OwnRow& o : kOwnRows)  // those whose row is not shown: at the end too
        if (o.view == v && o.from <= mode) add(o.row);
    return out;
}

Text helpOf(View v) {
    switch (v) {
        case View::Play: return Text::HelpViewPlay;
        case View::Main: return Text::HelpViewMain;
        case View::Graphics: return Text::HelpViewGraphics;
        case View::Screen: return Text::HelpViewScreen;
        case View::Effects: return Text::HelpViewEffects;
        case View::Ai: return Text::HelpViewAi;
        case View::Sound: return Text::HelpViewSound;
        case View::Pad: return Text::HelpViewPad;
        case View::GameChat: return Text::HelpViewGameChat;
        case View::Interface: return Text::HelpViewInterface;  // 3.0.11-beta.3
        case View::Service: return Text::HelpViewService;
        case View::Debug: return Text::HelpViewDebug;
        case View::Help: break;
    }
    return Text::HelpIntro;
}

}  // namespace uagfx::settings

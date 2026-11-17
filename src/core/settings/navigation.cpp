#include "core/settings/navigation.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "core/settings/catalogue.hpp"

namespace uagfx::settings {
namespace {

// The Main page's rows (2.6): the choices a player makes, everything else in the Detailed mode.
const char* const kMainKeys[] = {"quality.super_light", "quality.dlss",   "frames.frame_generation",
                                 "frames.fps_limit",    "frames.vsync",   "display.hdr",
                                 "sound.engine",        "frames.battery_saver", "ai.enabled",
                                 "sound.output"};  // 3.0.4-beta.1

// The switches for checks and reports - besides every key of [debug].
const char* const kDeveloperKeys[] = {"renderer.render_thread", "renderer.remix",
                                      "roext.window_x", "roext.window_y"};  // 3.0.1-beta.8: ROExt's own place

bool effectsSimple(const std::string& k) {  // the effects' set and its groups (Simple mode's Effects page)
    return k == "effects.preset" || k.compare(0, 14, "effects.group_") == 0;
}

}  // namespace

const char* modeToken(Mode m) {
    switch (m) {
        case Mode::Simple: return "simple";
        case Mode::Detailed: return "detailed";
        case Mode::Developer: break;
    }
    return "developer";
}

std::optional<Mode> parseMode(const std::string& token) {
    std::string t;
    for (char ch : token) t += char(std::tolower(static_cast<unsigned char>(ch)));
    for (int i = 0; i < kModeCount; ++i)
        if (t == modeToken(Mode(i))) return Mode(i);
    return std::nullopt;
}

Text modeName(Mode m) {
    switch (m) {
        case Mode::Simple: return Text::ModeSimple;
        case Mode::Detailed: return Text::ModeDetailed;
        case Mode::Developer: break;
    }
    return Text::ModeDeveloper;
}

Mode startMode(std::optional<Mode> saved, bool usedBefore) {
    if (saved) return *saved;
    return usedBefore ? Mode::Detailed : Mode::Simple;
}

Text viewName(View v) {
    switch (v) {
        case View::Main: return Text::ViewMain;
        case View::Graphics: return Text::ViewGraphics;
        case View::Screen: return Text::PageScreen;
        case View::Effects: return Text::PageEffects;
        case View::Ai: return Text::PageAi;
        case View::Sound: return Text::PageSound;
        case View::Pad: return Text::PagePad;
        case View::GameChat: return Text::ViewGameChat;
        case View::Interface: return Text::ViewInterface;  // 3.0.11-beta.3
        case View::Service: return Text::PageService;
        case View::Play: return Text::ViewPlay;
        case View::Help: return Text::ViewHelp;
        case View::Debug: break;
    }
    return Text::PageDebug;
}

std::vector<View> viewsOf(Mode m) {
    // 3.0.17-beta.5 (the concept «простой старт»): every page in the simple mode too - its detailed rows folded
    // behind «Ще N налаштувань ›» (page_settings), not hidden with their pages. 3.0.18-beta.193: «Гра» out - the
    // launcher is the app's first screen, the settings behind its gear
    std::vector<View> list{View::Main,  View::Graphics, View::Screen,    View::Effects, View::Ai,     View::Sound,
                           View::Pad,   View::GameChat, View::Interface, View::Service, View::Help};
    if (m == Mode::Developer) list.push_back(View::Debug);
    return list;
}

bool developerView(View v) { return v == View::Debug; }

int tabCount(View v) { return v == View::Ai ? 4 : 1; }

Text tabName(View v, int tab) {
    if (v != View::Ai) return viewName(v);
    switch (tab) {
        case 0: return Text::AiTabWhat;
        case 1: return Text::AiTabModels;
        case 2: return Text::PagePack;
        default: break;
    }
    return Text::PageAiCache;
}

std::vector<Page> partsOf(View v, int tab) {
    switch (v) {
        case View::Main: return {};
        case View::Graphics: return {Page::Render, Page::Quality, Page::Smoothing, Page::Frames};
        case View::Screen: return {Page::Screen};
        case View::Effects: return {Page::Effects};
        case View::Ai: return {tab == 2 ? Page::Pack : tab == 3 ? Page::AiCache : Page::Ai};
        case View::Sound: return {Page::Sound};
        case View::Pad: return {Page::Pad};
        case View::GameChat: return {Page::Game, Page::Chat};
        case View::Interface: return {Page::Interface};
        case View::Service: return {Page::Service, Page::Diagnostics, Page::Launcher};  // .193: the launcher's row
        case View::Play: return {};
        case View::Help: return {};
        case View::Debug: break;
    }
    return {Page::Debug};
}

Place placeOf(Page p) {
    switch (p) {
        case Page::Render:
        case Page::Quality:
        case Page::Smoothing:
        case Page::Frames: return {View::Graphics, 0};
        case Page::Screen: return {View::Screen, 0};
        case Page::Effects: return {View::Effects, 0};
        case Page::Ai: return {View::Ai, 0};
        case Page::Pack: return {View::Ai, 2};
        case Page::AiCache: return {View::Ai, 3};
        case Page::Sound: return {View::Sound, 0};
        case Page::Pad: return {View::Pad, 0};
        case Page::Game:
        case Page::Chat: return {View::GameChat, 0};
        case Page::Interface: return {View::Interface, 0};
        case Page::Debug: return {View::Debug, 0};
        case Page::Launcher:  // 3.0.18-beta.193: «Запускати гру одразу» in «Сервіс»
        case Page::Diagnostics:
        case Page::Service: break;
    }
    return {View::Service, 0};
}

Place placeOf(const IniSetting& s) {
    const std::string k = sectionKey(s);
    if (s.page == Page::Ai &&
        (k.compare(0, 8, "ai.model") == 0 || k.compare(0, 8, "ai.small") == 0 || k == "ai.sprite_model"))
        return {View::Ai, 1};
    return placeOf(s.page);
}

bool mainKey(const std::string& sectionKey) {
    for (const char* k : kMainKeys)
        if (sectionKey == k) return true;
    return false;
}

Mode levelOf(const IniSetting& s) {
    const std::string k = sectionKey(s);
    // 3.0.11-beta.3: the new interface's switches in every mode (its own item is in the simple list too)
    if (mainKey(k) || effectsSimple(k) || k.compare(0, 10, "interface.") == 0) return Mode::Simple;
    if (s.page == Page::Debug || std::strcmp(s.section, "debug") == 0) return Mode::Developer;
    for (const char* d : kDeveloperKeys)
        if (k == d) return Mode::Developer;
    return Mode::Detailed;
}

bool fitsMachine(const std::string& k, const Machine& m) {
    static const char* const kNvidia[] = {"quality.dlss",        "quality.dlss_scale",  "quality.dlss_width",
                                          "quality.dlss_height", "quality.dlss_model",  "quality.dlss_jitter",
                                          "quality.dlss_motion", "frames.frame_generation", "frames.reflex"};
    for (const char* n : kNvidia)
        if (k == n) return m.vendor == 0 || m.vendor == 0x10de;
    if (k == "display.hdr") return m.hdr;
    return true;
}

// 3.0.21-beta.1 (the concept «Настройки uaRO Graphics», the owner: «Вариант 4»): read from the catalogue, no list of
// its own - the groups that hold [interface] rows (those rows alone), in the catalogue's order; the controller's are
// the Controller page's (padInterfaceKeys). A group of «Інтерфейс» with no basic row (each window's own switch) shows
// behind a button.
const std::vector<InterfaceSection>& interfaceSections() {
    static const std::vector<InterfaceSection> list = [] {
        std::vector<InterfaceSection> out;
        for (const catalogue::Group& g : catalogue::groups()) {
            if (g.area == catalogue::Area::Controls && g.name == Text::PagePad) continue;
            InterfaceSection sec{g.name, {}, g.area == catalogue::Area::Interface};
            for (const catalogue::Row& r : g.rows) {
                if (r.key.compare(0, 10, "interface.") != 0) continue;
                sec.keys.push_back(r.key.c_str());
                sec.folded = sec.folded && r.level != catalogue::Level::Basic;
            }
            if (!sec.keys.empty()) out.push_back(std::move(sec));
        }
        return out;
    }();
    return list;
}

std::vector<std::string> newRows(const std::string& knownText, const std::vector<std::string>& keysNow) {
    std::vector<std::string> known, out;
    std::string line;
    for (char c : knownText + "\n") {
        if (c == '\n' || c == '\r') {
            if (!line.empty()) known.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    if (known.empty()) return out;
    std::sort(known.begin(), known.end());
    for (const std::string& k : keysNow)
        if (!std::binary_search(known.begin(), known.end(), k)) out.push_back(k);
    return out;
}

std::string knownRowsText(const std::vector<std::string>& keysNow) {
    std::string out;
    for (const std::string& k : keysNow) out += k + "\n";
    return out;
}

const std::vector<const char*>& padInterfaceKeys() {
    // 3.0.21-beta.1: the catalogue's «Контролер», its [interface] rows
    static const std::vector<const char*> list = [] {
        std::vector<const char*> out;
        for (const catalogue::Group& g : catalogue::groups())
            if (g.area == catalogue::Area::Controls && g.name == Text::PagePad)
                for (const catalogue::Row& r : g.rows)
                    if (r.key.compare(0, 10, "interface.") == 0) out.push_back(r.key.c_str());
        return out;
    }();
    return list;
}

bool resettable(View v, int tab) {
    return v == View::Graphics || v == View::Effects || v == View::Sound || (v == View::Ai && tab < 2);
}

}  // namespace uagfx::settings

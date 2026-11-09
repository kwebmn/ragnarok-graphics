#include "win/console.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>

#include "core/console_box.hpp"
#include "core/console_key.hpp"
#include "core/console_pins.hpp"
#include "core/console_tiles.hpp"
#include "core/layout.hpp"
#include "core/effect_costs.hpp"
#include "core/effect_groups.hpp"
#include "core/effect_where.hpp"
#include "core/settings/effects_layout.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/choice_presets.hpp"
#include "core/settings/navigation.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/search.hpp"
#include "win/context.hpp"  // 3.0.4-beta.35: the card's level now (judged by its speed)
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win {

std::vector<Console::Subclassed> Console::subclassed_;
Console* Console::active_ = nullptr;
bool Console::askedOpen_ = false;
ULONGLONG Console::skipCharUntil_ = 0;

namespace {

using namespace settings;

const wchar_t kClass[] = L"uaroGraphicsConsole";

ATOM windowClass() {
    static ATOM atom = 0;
    if (atom) return atom;
    WNDCLASSW c{};
    c.lpfnWndProc = DefWindowProcW;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = kClass;
    atom = RegisterClassW(&c);
    return atom;
}

std::wstring wide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring w(size_t(std::max(n, 0)), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), w.data(), n);
    return w;
}


// 2.6.22: one letter typed, as UTF-8; and the last one taken off
std::string narrow(wchar_t c) {
    char out[8] = {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, &c, 1, out, int(sizeof out), nullptr, nullptr);
    return std::string(out, size_t(std::max(n, 0)));
}

void eraseLast(std::string& utf8) {
    while (!utf8.empty() && (static_cast<unsigned char>(utf8.back()) & 0xC0) == 0x80) utf8.pop_back();
    if (!utf8.empty()) utf8.pop_back();
}

const IniSetting* settingOf(const std::string& key) { return findSetting(key); }

std::string groupKey(int g) { return std::string("effects.group_") + effect_groups::groupWord(g); }

// 2.2: the settings as the console shows them - each effect's switch as the set has it (the groups aside: they
// stay over the switches, as on the Effects page); a set other than custom decides them, not the file's lines.
Config shownConfig(const Config& c) {
    Config shown = c;
    shown.effects = effect_groups::shown(c.effects, Context::get().cardLevel());
    shown.ai = c.aiNow;  // 2.2.1: the AI pictures as the file says now (shown so at once)
    return shown;
}

}  // namespace

Console::~Console() {
    hide();
    if (window_) DestroyWindow(window_);
}

// The game's window gets no keys while the console is open; ~ itself never (it opens and closes the console).
LRESULT CALLBACK Console::wndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    const bool key = msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
                     msg == WM_CHAR || msg == WM_SYSCHAR;
    // 2.13: Ctrl+Shift+F10 is «Звернення» (win/context): Windows' F10 (the window's menu, the game stopped until Esc)
    // never sees it
    if (key && msg != WM_CHAR && msg != WM_SYSCHAR && wp == VK_F10 && GetKeyState(VK_CONTROL) < 0 &&
        GetKeyState(VK_SHIFT) < 0)
        return 0;
    // 2.6.22: the ~ key's own letter that follows its press (` or, in a Cyrillic layout, ё) opens or closes the
    // console - typed neither in its search nor (2.7.1) in the game's chat, where every opening put an «ё».
    // 2.15: with Ctrl only - the key alone types «ё» and the apostrophe in the chat (core/console_key)
    const bool ctrl = GetKeyState(VK_CONTROL) < 0;
    if (msg == WM_KEYDOWN && console::toggles(ctrl, unsigned(wp))) skipCharUntil_ = GetTickCount64() + 250;
    if (msg == WM_CHAR && GetTickCount64() < skipCharUntil_) {
        skipCharUntil_ = 0;
        return 0;
    }
    if (msg == WM_SYSKEYDOWN && wp == 'D' && active_ && active_->open_) active_->closeAsked_ = true;  // .260: Alt+D
    if (msg == WM_CHAR && active_ && active_->open_ && (wp == 8 || (wp > L' ' && wp != 127)))
        active_->typed_.push_back(wchar_t(wp));  // 2.6.22: letters and Backspace for the search
    const bool toggleKey = msg != WM_CHAR && msg != WM_SYSCHAR && console::toggles(ctrl, unsigned(wp));
    if (key && ((active_ && active_->open_) || toggleKey)) return 0;
    for (const Subclassed& s : subclassed_)
        if (s.window == w) return CallWindowProcW(s.proc, w, msg, wp, lp);
    return DefWindowProcW(w, msg, wp, lp);
}

// 2.2.1: as the Effects page - the set and the mode, then the groups, each with its strength and a row that opens it
// to its effects (each with its own settings under it); some thirty rows, not a hundred and fifty. 2.6.22: letters
// typed - only the effects they find (by the name, the hint, the name before; a group's own name: all of it), under
// their groups' names; «Только на этой карте» - only what can show on the map, a group with none of it left out.
// 3.0.11-beta.302: an interface row as its control asks - a box, a list or a slider; none for a number or a row the
// schema has not
void Console::interfaceRow(const char* key, int depth) {
    const IniSetting* s = settingOf(key);
    if (!s) return;
    if (s->control == Control::Check) rows_.push_back(Row{Kind::Box, key, depth});
    if (s->control == Control::Choice) rows_.push_back(Row{Kind::Choice, key, depth});
    if (s->control == Control::Slider) rows_.push_back(Row{Kind::Slider, key, depth});
}

void Console::build(Lang lang, const Config& config) {
    rows_.clear();
    const EffectLayout* fx = effectLayout();
    const auto layoutOf = [&](const effect_groups::Member& m) -> const EffectLayout* {
        const std::string box = std::string("effects.") + m.key;
        for (int e = 0; e < effectLayoutCount(); ++e)
            if (box == fx[e].box && settingOf(fx[e].box)) return &fx[e];
        return nullptr;
    };
    const auto effect = [&](const EffectLayout& e, int depth) {  // its box, its own checks and sliders under it
        rows_.push_back(Row{Kind::Box, e.box, depth});
        for (int k = 0; k < kLayoutChecks && e.checks[k]; ++k)
            if (settingOf(e.checks[k])) rows_.push_back(Row{Kind::Check, e.checks[k], depth + 1});
        for (int k = 0; k < 5 && e.sliders[k]; ++k)
            if (settingOf(e.sliders[k])) rows_.push_back(Row{Kind::Slider, e.sliders[k], depth + 1});
    };
    if (!filter_.empty()) {
        // 3.0.11-beta.302: the interface's rows found too, under their sections
        for (const InterfaceSection& sec : interfaceSections()) {
            bool named = false;
            for (const char* k : sec.keys) {
                const IniSetting* st = settingOf(k);
                if (!st || !settingMatches(*st, filter_, lang)) continue;
                if (!named) rows_.push_back(Row{Kind::Heading, tr(lang, sec.heading), 0});
                named = true;
                interfaceRow(k, 1);
            }
        }
        for (int g = 0; g < kEffectGroups; ++g) {
            const IniSetting* group = settingOf(groupKey(g));
            if (!group) continue;
            const bool all = settingMatches(*group, filter_, lang);
            bool named = false;
            for (int i = 0; i < effect_groups::memberCount(); ++i) {
                const effect_groups::Member& m = effect_groups::members()[i];
                const EffectLayout* e = m.group == g ? layoutOf(m) : nullptr;
                if (!e || !here(e->box, config)) continue;
                bool found = all || settingMatches(*settingOf(e->box), filter_, lang);
                for (int k = 0; k < kLayoutChecks && e->checks[k] && !found; ++k)
                    if (const IniSetting* c = settingOf(e->checks[k])) found = settingMatches(*c, filter_, lang);
                for (int k = 0; k < 5 && e->sliders[k] && !found; ++k)
                    if (const IniSetting* v = settingOf(e->sliders[k])) found = settingMatches(*v, filter_, lang);
                if (!found) continue;
                if (!named) rows_.push_back(Row{Kind::Heading, tr(lang, group->label), 0});
                named = true;
                effect(*e, 1);
            }
        }
        if (rows_.empty()) rows_.push_back(Row{Kind::Heading, tr(lang, Text::ConsoleNothing), 0});
    } else {
        rows_.push_back(Row{Kind::Tabs, "", 0});  // 3.0.17-beta.4
        const bool all = tab_ == kTabAll;
        if (tab_ == kTabQuick) {  // the three choices, the rows switched most, the player's pinned, the two actions
            rows_.push_back(Row{Kind::Heading, tr(lang, Text::LookHead), 0});
            for (int k = 0; k < presets::kChoices; ++k) rows_.push_back(Row{Kind::Look, std::to_string(k), 0});
            rows_.push_back(Row{Kind::Heading, tr(lang, Text::ConsoleOften), 0});
            // 3.0.17-beta.27: the switches as tiles, four a row; a list or a slider as its row
            std::vector<std::string> tiles;
            for (const char* k : {"effects.group_time", "effects.group_weather", "effects.grass",
                                  "effects.torch_shadows", "effects.mage_effects", "frames.keep_fps",
                                  "frames.fps_counter", "interface.net_badge", "interface.mob_hp"}) {
                if (consolepins::pinned(pins_, k)) continue;
                const IniSetting* st = settingOf(k);
                if (st && st->control == Control::Check) tiles.push_back(k);
                else interfaceRow(k, 0);
            }
            for (const auto& row : consoletiles::rows(tiles))
                rows_.push_back(Row{Kind::Tiles, consoletiles::join(row), 0});
            if (!fresh_.empty())  // 3.0.17-beta.12: the way to «Нове»
                rows_.push_back(
                    Row{Kind::Heading, fmt(tr(lang, Text::ConsoleNewCount), {std::to_string(fresh_.size())}), 0});
            if (!pins_.empty()) rows_.push_back(Row{Kind::Heading, tr(lang, Text::ConsolePinned), 0});
            for (const std::string& k : pins_) interfaceRow(k.c_str(), 0);
            rows_.push_back(Row{Kind::Heading, tr(lang, Text::ConsoleActions), 0});
            rows_.push_back(Row{Kind::Layout, "", 0});
            rows_.push_back(Row{Kind::Test, "", 0});
        }
        if (tab_ == kTabNew) {  // 3.0.17-beta.12: the rows new since the console showed them last
            rows_.push_back(Row{Kind::Heading, fresh_.empty() ? tr(lang, Text::ConsoleNewNone)
                                                              : tr(lang, Text::ConsoleNewNote), 0});
            for (const std::string& k : fresh_) interfaceRow(k.c_str(), 0);
        }
        if (all) rows_.push_back(Row{Kind::Box, "frames.fps_counter", 0});  // the frame counter (Ctrl+Shift+F11)
        // 3.0.5-beta.6: «Тест графіки» - not the first row, where the console opens: a Space there starts nothing
        if (all) rows_.push_back(Row{Kind::Test, "", 0});
        // 3.0.11-beta.2: the new interface on and off in the game; 3.0.11-beta.302 (the review «Ревизия
        // конфигуратора», D): its rows in the app's sections (core/settings/navigation interfaceSections) - every row
        // of them, the windows behind a row that opens them, the controller's on the Controller page
        if ((all || tab_ == kTabInterface) && settingOf("interface.minimap")) {
            rows_.push_back(Row{Kind::Heading, tr(lang, Text::InterfaceHeading), 0});
            rows_.push_back(Row{Kind::Layout, "", 0});  // 3.0.11-beta.75: «Розміщення інтерфейсу»
            for (const InterfaceSection& sec : interfaceSections()) {
                rows_.push_back(Row{Kind::Heading, tr(lang, sec.heading), 0});
                if (sec.folded) {
                    rows_.push_back(Row{Kind::Open, kWindowsKey, 0});
                    if (!windowsOpen_) continue;
                }
                for (const char* k : sec.keys) interfaceRow(k, 0);
            }
            for (const char* k : padInterfaceKeys()) interfaceRow(k, 0);  // the controller's here too: in the game
        }
        // 3.0.3-beta.8: the game windows' panel (the user: on or off in the console too)
        if ((all || tab_ == kTabInterface) && settingOf("panel.show")) {
            rows_.push_back(Row{Kind::Heading, tr(lang, Text::PanelHeading), 0});
            rows_.push_back(Row{Kind::Box, "panel.show", 0});
            rows_.push_back(Row{Kind::Choice, "panel.layout", 1});
            rows_.push_back(Row{Kind::Slider, "panel.size", 1});
            rows_.push_back(Row{Kind::Slider, "panel.opacity", 1});
        }
        // 2.2.1: the AI pictures of the texture pack - a kind off hidden at once, on at once when the game started
        // with it
        if (all || tab_ == kTabTextures) rows_.push_back(Row{Kind::Heading, tr(lang, Text::PageAi), 0});
        for (const char* k : {"ai.enabled", "ai.ground", "ai.objects", "ai.monsters", "ai.players", "ai.npcs",
                              "ai.items", "ai.effects", "ai.icons"})
            if ((all || tab_ == kTabTextures) && settingOf(k))
                rows_.push_back(Row{Kind::Box, k, std::string(k) == "ai.enabled" ? 0 : 1});
        if (all && settingOf("debug.xray")) rows_.push_back(Row{Kind::Box, "debug.xray", 1});  // 2.3: Ctrl+Shift+F9
        if (all && settingOf("debug.seams_view")) rows_.push_back(Row{Kind::Choice, "debug.seams_view", 1});  // 2.7.8
        const bool picture = all || tab_ == kTabPicture;
        if (picture) rows_.push_back(Row{Kind::Heading, tr(lang, Text::FxPreset), 0});
        if (picture) rows_.push_back(Row{Kind::Choice, "effects.preset", 0});
        if (picture) rows_.push_back(Row{Kind::Choice, "effects.tier", 0});
        // 2.6.19: the time of day for all the effects, by the set and the mode
        if (picture && settingOf("effects.clock")) {
            rows_.push_back(Row{Kind::Choice, "effects.clock", 0});
            rows_.push_back(Row{Kind::Slider, "effects.night_strength", 1});
            if (settingOf("effects.season")) rows_.push_back(Row{Kind::Choice, "effects.season", 0});  // 2.6.22
            if (settingOf("effects.weather_kind")) rows_.push_back(Row{Kind::Choice, "effects.weather_kind", 0});
        }
        if (picture) rows_.push_back(Row{Kind::Here, "", 0});  // 2.6.22
        for (int g = 0; g < kEffectGroups && picture; ++g) {
            if (!settingOf(groupKey(g))) continue;
            int listed = 0;
            for (int i = 0; i < effect_groups::memberCount(); ++i) {
                const effect_groups::Member& m = effect_groups::members()[i];
                const EffectLayout* e = m.group == g ? layoutOf(m) : nullptr;
                listed += e && here(e->box, config) ? 1 : 0;
            }
            if (!listed) continue;
            rows_.push_back(Row{Kind::Box, groupKey(g), 0});
            if (effect_groups::hasStrength(g)) rows_.push_back(Row{Kind::Slider, groupKey(g) + "_strength", 1});
            rows_.push_back(Row{Kind::Open, groupKey(g), 1});
            if (!groupOpen_[g]) continue;
            for (int i = 0; i < effect_groups::memberCount(); ++i) {
                const effect_groups::Member& m = effect_groups::members()[i];
                const EffectLayout* e = m.group == g ? layoutOf(m) : nullptr;
                if (e && here(e->box, config)) effect(*e, 2);
            }
        }
    }
    at_ = std::clamp(at_, 0, std::max(0, int(rows_.size()) - 1));
    while (at_ + 1 < int(rows_.size()) && rows_[size_t(at_)].kind == Kind::Heading) ++at_;
}

// 3.0.17-beta.4: the pins next to uaro-graphics.ini, read once
void Console::readPins(const std::string& iniPath) {
    if (pinsRead_) return;
    pinsRead_ = true;
    const size_t slash = iniPath.find_last_of("\\/");
    pinsPath_ = (slash == std::string::npos ? std::string() : iniPath.substr(0, slash + 1)) + layout::kConsolePins;
    std::string text;
    if (readTextFile(pinsPath_, text)) pins_ = consolepins::parse(text);
    // 3.0.17-beta.12: the rows the console has not shown yet (none at its first start: nothing marked)
    knownPath_ = (slash == std::string::npos ? std::string() : iniPath.substr(0, slash + 1)) + layout::kConsoleKnown;
    std::vector<std::string> keys;
    for (const IniSetting& s : iniSettings())
        if (levelOf(s) != Mode::Developer) keys.push_back(sectionKey(s));
    std::string known;
    const bool had = readTextFile(knownPath_, known);
    fresh_ = newRows(known, keys);
    if (!had) writeFileAtomic(knownPath_, knownRowsText(keys));
}

// 3.0.17-beta.12: «Нове» seen - every row known from now on
void Console::markKnown() {
    if (fresh_.empty() || knownPath_.empty()) return;
    std::vector<std::string> keys;
    for (const IniSetting& s : iniSettings())
        if (levelOf(s) != Mode::Developer) keys.push_back(sectionKey(s));
    writeFileAtomic(knownPath_, knownRowsText(keys));
}

bool Console::rowPinnable(const Row& r) const {
    if (r.kind == Kind::Tiles) return true;  // 3.0.17-beta.27: its chosen tile
    return (r.kind == Kind::Box || r.kind == Kind::Check || r.kind == Kind::Slider || r.kind == Kind::Choice) &&
           settingOf(r.key);
}

std::string Console::chosenKey() const {
    if (rows_.empty() || at_ < 0 || at_ >= int(rows_.size())) return "";
    const Row& r = rows_[size_t(at_)];
    if (r.kind != Kind::Tiles) return r.kind == Kind::Heading ? "" : r.key;
    const std::vector<std::string> keys = consoletiles::split(r.key);
    return keys.empty() ? "" : keys[size_t(consoletiles::move(tileCol_, 0, int(keys.size())))];
}

bool Console::here(const std::string& key, const Config& config) const {
    return !thisMap_ || effect_where::worksOn(key, map_, config.effects);
}

bool Console::pressed(int vk, Key& k, bool repeats, ULONGLONG now) {
    const bool down = GetAsyncKeyState(vk) < 0;
    bool fire = false;
    if (down && !k.down) {
        fire = true;
        k.since = k.last = now;
    } else if (down && repeats && now - k.since > 350 && now - k.last > 55) {
        fire = true;
        k.last = now;
    }
    k.down = down;
    return fire;
}

bool Console::change(const std::string& iniPath, const Config& config, int step) {
    if (rows_.empty()) return false;
    const Row& r = rows_[size_t(at_)];
    if (r.kind == Kind::Open) {  // 2.2.1: a group's effects shown or hidden - Space, Enter, right opens, left closes
        for (int g = 0; g < kEffectGroups; ++g)
            if (r.key == groupKey(g)) groupOpen_[g] = step == 0 ? !groupOpen_[g] : step > 0;
        if (r.key == kWindowsKey) windowsOpen_ = step == 0 ? !windowsOpen_ : step > 0;  // 3.0.11-beta.302
        build(lang_, config);
        dirty_ = true;
        return false;
    }
    if (r.kind == Kind::Test) {  // 3.0.5-beta.6: Space or Enter starts it; the console closes (present)
        testAsked_ = testAsked_ || step == 0;
        return false;
    }
    if (r.kind == Kind::Layout) {  // 3.0.11-beta.75: Space or Enter - «Розміщення інтерфейсу»; the console closes
        layoutAsked_ = layoutAsked_ || step == 0;
        return false;
    }
    if (r.kind == Kind::Tabs) {  // 3.0.17-beta.4: Space or right the next tab, left the one before
        if (tab_ == kTabNew) markKnown();  // 3.0.17-beta.12: «Нове» left - seen
        tab_ = ((tab_ + (step < 0 ? -1 : 1)) % kTabs + kTabs) % kTabs;
        build(lang_, config);
        at_ = top_ = 0;
        dirty_ = true;
        return false;
    }
    if (r.kind == Kind::Look) {  // 3.0.17-beta.4: «Як виглядає гра» - the next variant's keys written
        const presets::Choice c = presets::Choice(std::atoi(r.key.c_str()));
        const int n = presets::variants(c);
        const int to = ((presets::match(c, shownConfig(config)).variant + (step < 0 ? -1 : 1)) % n + n) % n;
        std::vector<IniEdit> edits;
        for (const auto& [key, token] : presets::keysOf(c, to)) {
            const size_t dot = key.find('.');
            edits.push_back({key.substr(0, dot), key.substr(dot + 1), token});
        }
        if (!editIniFile(iniPath, edits)) {
            LOG_ERROR("console: uaro-graphics.ini could not be written");
            return false;
        }
        LOG_INFO("console: «%s» - variant %d", r.key.c_str(), to);
        return true;
    }
    if (r.kind == Kind::Tiles && step != 0) {  // 3.0.17-beta.27: left and right choose the tile
        const int count = int(consoletiles::split(r.key).size());
        tileCol_ = consoletiles::move(consoletiles::move(tileCol_, 0, count), step, count);
        dirty_ = true;
        return false;
    }
    if (r.kind == Kind::Here) {  // 2.6.22: «Только на этой карте» - the console's own, not the file's
        thisMap_ = step == 0 ? !thisMap_ : step > 0;
        build(lang_, config);
        dirty_ = true;
        return false;
    }
    const IniSetting* s = r.kind == Kind::Heading ? nullptr : settingOf(chosenKey());  // .27: a tile's
    if (!s) return false;
    const Config shown = shownConfig(config);
    const std::string now = tokenOf(*s, shown);
    std::string next = now;
    if (s->control == Control::Check) {
        const bool on = now == s->tokens[1];
        const bool want = step == 0 ? !on : step > 0;
        next = want ? s->tokens[1] : s->tokens[0];
    } else if (s->control == Control::Slider) {
        const int v = std::atoi(now.c_str()) + (step == 0 ? 1 : step) * std::max(s->step, 1);
        next = std::to_string(std::clamp(v, s->min, s->max));
    } else if (s->control == Control::Choice && !s->tokens.empty()) {
        int i = 0;
        for (size_t k = 0; k < s->tokens.size(); ++k)
            if (now == s->tokens[k]) i = int(k);
        const int n = int(s->tokens.size());
        i = ((i + (step < 0 ? -1 : 1)) % n + n) % n;
        next = s->tokens[size_t(i)];
    }
    if (next == now) return false;
    std::vector<IniEdit> edits;
    const std::string key = sectionKey(*s);
    // 2.6.22: an old file's ten groups written into the effects with this first change (as the settings app does)
    if (effect_groups::hasLegacy(config.effects))
        for (const IniEdit& e : effect_groups::settleLegacy(config.effects, Context::get().cardLevel()))
            edits.push_back(e);
    // 2.2: an effect's own switch or the mode while a set decides them - the set made the player's own first (as it
    // has them, in the same write); 2.2.2: custom chosen here as well, as on the Effects page (the switches the set
    // had, not whatever the file held from before)
    const effect_groups::Member* member = effect_groups::memberOf(key);
    if (member || key == "effects.tier" || (key == "effects.preset" && next == "custom")) {
        const std::string except = member ? member->key : key.substr(8);
        for (const auto& [k, v] : effect_groups::materialize(config.effects, Context::get().cardLevel(), except))
            edits.push_back({"effects", k, v});
    }
    edits.push_back({s->section, s->key, next});
    if (!editIniFile(iniPath, edits)) {  // 2.2.2: read again when the settings app wrote it meanwhile
        LOG_ERROR("console: uaro-graphics.ini could not be written");
        return false;
    }
    LOG_INFO("console: %s = %s", sectionKey(*s).c_str(), next.c_str());
    return true;
}

bool Console::present(HWND game, bool foreground, const std::string& iniPath, const Config& config,
                      const std::string& map, const GpuTiming* timing) {
    if (!game) return false;
    readPins(iniPath);  // 3.0.17-beta.4
    if (game != game_) {  // once a window: its keys pass through wndProc (2.2.2: a window made anew too)
        bool known = false;
        for (const Subclassed& s : subclassed_) known = known || s.window == game;
        if (!known) {
            const auto was = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(game, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Console::wndProc)));
            if (was && was != &Console::wndProc) subclassed_.push_back({game, was});
        }
        active_ = this;
    }
    game_ = game;
    const ULONGLONG now = GetTickCount64();
    if (!foreground) {
        hide();
        comparing_.clear();
        return false;
    }
    const bool toggled = pressed(console::kKey, toggle_, false, now);  // its edge, with Ctrl or not
    const bool asked = askedOpen_ && !open_;  // 3.0.11-beta.209: Alt+D
    askedOpen_ = false;
    if (asked || (toggled && console::toggles(GetAsyncKeyState(VK_CONTROL) < 0, console::kKey))) {
        open_ = !open_;
        if (open_) {
            std::string text;
            readTextFile(iniPath, text);
            const auto ini = readIni(text);
            const auto lang = ini.count("utility.language") ? parseLang(ini.at("utility.language")) : std::nullopt;
            lang_ = startLang(lang);
            filter_.clear();
            typed_.clear();
            clicks_.clear();
            closeAsked_ = false;
            map_ = map;
            build(lang_, config);
            LOG_INFO("console: open");
        }
        dirty_ = true;
    }
    if (!open_) {
        hide();
        comparing_.clear();
        return false;
    }
    if (map != map_) {  // 2.6.22: another map - what can show on it
        map_ = map;
        if (thisMap_) build(lang_, config);
        dirty_ = true;
    }
    if (!typed_.empty()) {  // 2.6.22: the letters typed - the effects found by them
        for (const wchar_t c : typed_) {
            if (c == 8)
                eraseLast(filter_);
            else
                filter_ += narrow(c);
        }
        typed_.clear();
        at_ = top_ = 0;
        build(lang_, config);
        dirty_ = true;
    }
    bool changed = false;
    const int n = int(rows_.size());
    const auto move = [&](int by) {
        int to = at_;
        for (int k = 0; k < std::abs(by); ++k) {
            int step = to + (by > 0 ? 1 : -1);
            while (step >= 0 && step < n && rows_[size_t(step)].kind == Kind::Heading) step += by > 0 ? 1 : -1;
            if (step < 0 || step >= n) break;
            to = step;
        }
        if (to != at_) {
            at_ = to;
            dirty_ = true;
        }
    };
    if (pressed(VK_UP, up_, true, now)) move(-1);
    if (pressed(VK_DOWN, down_, true, now)) move(1);
    if (pressed(VK_PRIOR, pageUp_, true, now)) move(-10);
    if (pressed(VK_NEXT, pageDown_, true, now)) move(10);
    if (pressed(VK_SPACE, space_, false, now) | pressed(VK_RETURN, enter_, false, now))
        changed = change(iniPath, config, 0);
    if (pressed(VK_LEFT, left_, true, now)) changed = change(iniPath, config, -1) || changed;
    if (pressed(VK_INSERT, insert_, false, now) && at_ < n && rowPinnable(rows_[size_t(at_)])) {  // 3.0.17-beta.4
        const std::string key = chosenKey();  // 3.0.17-beta.27: a tile's
        pins_ = consolepins::toggle(pins_, key);
        if (!pinsPath_.empty()) writeFileAtomic(pinsPath_, consolepins::format(pins_));
        LOG_INFO("console: %s %s «Швидко»", key.c_str(), consolepins::pinned(pins_, key) ? "pinned to" : "taken off");
        if (tab_ == kTabQuick) build(lang_, config);
        dirty_ = true;
    }
    if (pressed(VK_RIGHT, right_, true, now)) changed = change(iniPath, config, 1) || changed;
    if (testAsked_ || layoutAsked_) {  // 3.0.5-beta.6: the test runs over the game alone (3.0.11-beta.75: the layout)
        LOG_INFO("console: %s chosen - closed", testAsked_ ? "«Тест графіки»" : "«Розмістити інтерфейс»");
        open_ = false;
        hide();
        comparing_.clear();
        return changed;
    }
    for (const Click& c : clicks_) {  // 3.0.11-beta.260: the mouse
        if (c.wheel) {
            move(c.wheel > 0 ? -3 : 3);
            continue;
        }
        if (drawnLine_ <= 0 || c.y < drawnFirstRow_) continue;
        const int row = top_ + (c.y - drawnFirstRow_) / drawnLine_;
        if ((c.y - drawnFirstRow_) / drawnLine_ >= drawnVisible_ || row < 0 || row >= n) continue;
        if (rows_[size_t(row)].kind == Kind::Heading) continue;
        at_ = row;
        dirty_ = true;
        if (rows_[size_t(row)].kind == Kind::Tiles)  // 3.0.17-beta.27: the tile under the mouse
            tileCol_ = consoletiles::at(c.x, drawnTilesLeft_, drawnTilesWidth_,
                                        int(consoletiles::split(rows_[size_t(row)].key).size()));
        const IniSetting* s = settingOf(chosenKey());
        const bool stepped = s && (s->control == Control::Slider || s->control == Control::Choice);
        const int half = int(drawnBox_.right - drawnBox_.left) / 2;
        changed = change(iniPath, config, stepped ? (c.x >= half ? 1 : -1) : 0) || changed;
    }
    clicks_.clear();
    if (closeAsked_) {  // 3.0.11-beta.260: Alt+D while it is open
        closeAsked_ = false;
        LOG_INFO("console: Alt+D - closed");
        open_ = false;
        hide();
        comparing_.clear();
        return changed;
    }
    if (pressed(VK_ESCAPE, escape_, false, now)) {
        if (!filter_.empty()) {  // 2.6.22: the letters cleared first, then the console closed
            filter_.clear();
            at_ = top_ = 0;
            build(lang_, config);
            dirty_ = true;
        } else {
            open_ = false;
            hide();
            comparing_.clear();
            return changed;
        }
    }
    // Tab held: the chosen effect's own box off while it is held
    std::string comparing;
    if (GetAsyncKeyState(VK_TAB) < 0 && at_ < n) {
        const Row& r = rows_[size_t(at_)];
        if (r.kind == Kind::Box) comparing = r.key;
        if (r.kind == Kind::Check || r.kind == Kind::Slider || r.kind == Kind::Open)
            for (int k = at_; k >= 0; --k)
                if (rows_[size_t(k)].kind == Kind::Box) {
                    comparing = rows_[size_t(k)].key;
                    break;
                }
    }
    if (comparing != comparing_) {
        comparing_ = comparing;
        dirty_ = true;
    }
    if (changed) dirty_ = true;
    draw(game, config, timing);
    return changed;
}

bool Console::mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    Console* c = active_;
    if (!c || !c->open_ || c->game_ != game || c->drawnLine_ <= 0) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (m == WM_MOUSEWHEEL && !ScreenToClient(game, &p)) return false;  // the wheel's point is the screen's
    if (!PtInRect(&c->drawnBox_, p)) return false;
    result = 0;
    if (m == WM_MOUSEWHEEL)
        c->clicks_.push_back(Click{0, 0, GET_WHEEL_DELTA_WPARAM(wp)});
    else if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK)
        c->clicks_.push_back(Click{int(p.x - c->drawnBox_.left), int(p.y - c->drawnBox_.top), 0});
    return true;
}

void Console::hide() {
    if (tab_ == kTabNew) markKnown();  // 3.0.17-beta.12: closed on «Нове» - seen
    if (window_ && IsWindowVisible(window_)) ShowWindow(window_, SW_HIDE);
    shownText_.clear();
}

void Console::draw(HWND game, const Config& file, const GpuTiming* timing) {
    // Redrawn when anything shown changed: the values come from the config the driver read (2.2: as the set has them).
    const Config config = shownConfig(file);
    std::string state = std::to_string(at_) + "|" + comparing_ + "|" + std::to_string(timing ? timing->serial : 0) +
                        "|" + filter_ + "|" + (thisMap_ ? "1" : "0") + map_ + "|" + std::to_string(file.hudOpacity);
    // 3.0.11-beta.270 (the tester: «перекриває і курсор миші, і прицілитись важко»): the console opaque over the
    // game's cursor - an arrow of its own drawn where the mouse is over it
    POINT mouseAt{-1, -1};
    if (GetCursorPos(&mouseAt) && game) ScreenToClient(game, &mouseAt);
    const bool mouseIn = drawnLine_ > 0 && PtInRect(&drawnBox_, mouseAt);
    if (mouseIn) state += "|m" + std::to_string(mouseAt.x) + "," + std::to_string(mouseAt.y);
    for (const Row& r : rows_)
        if (r.kind == Kind::Tiles) {  // 3.0.17-beta.27
            for (const std::string& k : consoletiles::split(r.key))
                if (const IniSetting* s = settingOf(k)) state += tokenOf(*s, config) + ",";
        } else if (r.kind != Kind::Heading) {
            if (const IniSetting* s = settingOf(r.key)) state += tokenOf(*s, config) + ",";
        }
    state += "|c" + std::to_string(tileCol_);
    state += "|t" + std::to_string(tab_) + "|" + consolepins::format(pins_);  // 3.0.17-beta.4
    for (int k = 0; k < presets::kChoices; ++k) {
        const presets::Match m = presets::match(presets::Choice(k), config);
        state += "|" + std::to_string(m.variant) + "/" + std::to_string(m.changed);
    }
    if (!dirty_ && state == shownText_ && window_ && IsWindowVisible(window_)) return;
    dirty_ = false;
    shownText_ = state;
    if (!window_ && windowClass())
        window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                                      WS_EX_TOPMOST,
                                  kClass, L"", WS_POPUP, 0, 0, 1, 1, game, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window_) return;
    RECT client{};
    GetClientRect(game, &client);
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    // 3.0.11-beta.209 (the tester: «не влазить на маленьких екранах»): its size by the game's window (core/console_box)
    const consolebox::Box box = consolebox::box(int(client.right - client.left), int(client.bottom - client.top),
                                                std::max(14, GetDeviceCaps(screen, LOGPIXELSY) * 15 / 96));
    const int fontH = box.fontH, line = box.line, w = box.w, h = box.h;
    HFONT font = CreateFontW(-fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT bold = CreateFontW(-fontH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    const std::wstring keys = wide(tr(lang_, Text::ConsoleKeys));
    RECT measured{0, 0, w - 2 * fontH, 0};  // the foot's lines as GDI breaks them at this width
    HGDIOBJ measuring = SelectObject(dc, font);
    DrawTextW(dc, keys.c_str(), int(keys.size()), &measured, DT_CALCRECT | DT_WORDBREAK | DT_LEFT | DT_NOPREFIX);
    SelectObject(dc, measuring);
    const int footH = std::max(line, int(measured.bottom - measured.top));
    // 3.0.17-beta.27 (Task 8): a line over the foot - the chosen row's «що це дає» (its hint's first sentence)
    const int visible = consolebox::visibleRows(box, footH + line);  // the title, «що це дає», the foot's lines
    if (at_ < top_) top_ = at_;
    if (at_ >= top_ + visible) top_ = at_ - visible + 1;
    top_ = std::clamp(top_, 0, std::max(0, int(rows_.size()) - visible));
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        HGDIOBJ oldFont = SelectObject(dc, font);
        auto* px = static_cast<uint32_t*>(bits);
        // A dark panel; the chosen row a lighter band; GDI's text then made opaque (premultiplied below).
        std::fill(px, px + size_t(w) * size_t(h), 0x00101418u);
        SetBkMode(dc, TRANSPARENT);
        const auto text = [&](int x, int y, const std::wstring& s, COLORREF c, bool strong = false) {
            SelectObject(dc, strong ? bold : font);
            SetTextColor(dc, c);
            TextOutW(dc, x, y, s.c_str(), int(s.size()));
        };
        const auto textRight = [&](int x, int y, const std::wstring& s, COLORREF c) {
            SelectObject(dc, font);
            SIZE sz{};
            GetTextExtentPoint32W(dc, s.c_str(), int(s.size()), &sz);
            SetTextColor(dc, c);
            TextOutW(dc, x - sz.cx, y, s.c_str(), int(s.size()));
        };
        // 2.6.22: the letters typed in the title's place while there are some
        if (filter_.empty())
            text(fontH, line / 4, wide(tr(lang_, Text::ConsoleTitle)), RGB(229, 169, 78), true);
        else
            text(fontH, line / 4, wide(fmt(tr(lang_, Text::ConsoleSearch), {filter_ + "_"})), RGB(240, 236, 226),
                 true);
        // 2.2.1: the card's work a frame now - what an effect costs shows as it is switched (or Tab held)
        std::map<std::string, double> costs;
        if (timing && timing->gpuMs >= 0.0) {
            char gpu[96];
            if (timing->upscaleMs > 0.05)
                std::snprintf(gpu, sizeof gpu, "GPU %.2f ms (%s %.2f)", timing->gpuMs, timing->upscaler.c_str(),
                              timing->upscaleMs);
            else
                std::snprintf(gpu, sizeof gpu, "GPU %.2f ms", timing->gpuMs);
            textRight(w - fontH, line / 4, wide(gpu), RGB(229, 169, 78));
            costs = timing->effects;
        }
        for (int i = 0; i < visible && top_ + i < int(rows_.size()); ++i) {
            const Row& r = rows_[size_t(top_ + i)];
            const int y = line + line / 2 + i * line;
            if (top_ + i == at_)
                for (int yy = std::max(0, y - line / 8); yy < std::min(h, y + line - line / 8); ++yy)
                    std::fill(px + size_t(yy) * size_t(w), px + size_t(yy + 1) * size_t(w), 0x00384048u);
            if (r.kind == Kind::Heading) {
                text(fontH, y, wide(r.key), RGB(229, 169, 78), true);
                continue;
            }
            if (r.kind == Kind::Open && r.key == kWindowsKey) {  // 3.0.11-beta.302: the new look's windows
                size_t n = 0;
                for (const InterfaceSection& sec : interfaceSections())
                    if (sec.folded) n += sec.keys.size();
                const std::string label =
                    windowsOpen_ ? std::string(tr(lang_, Text::InterfaceWindowsHide))
                                 : fmt(tr(lang_, Text::InterfaceWindowsShow), {std::to_string(n)});
                text(fontH + r.depth * fontH * 2, y, wide(label), RGB(140, 180, 220));
                continue;
            }
            if (r.kind == Kind::Open) {  // 2.2.1: "the group's effects >" (2.6.22: "3 of 7 on") or "hide the effects"
                int g = 0, count = 0, lit = 0;
                while (g < kEffectGroups && r.key != groupKey(g)) ++g;
                for (int k = 0; k < effect_groups::memberCount(); ++k) {
                    const effect_groups::Member& m = effect_groups::members()[k];
                    if (m.group != g || !here(std::string("effects.") + m.key, config)) continue;
                    ++count;
                    lit += config.effects.*m.on ? 1 : 0;
                }
                const bool shown = g < kEffectGroups && groupOpen_[g];
                const std::string label = shown ? std::string(tr(lang_, Text::FxDetailsHide))
                                                : fmt(tr(lang_, Text::FxDetails),
                                                      {std::to_string(lit), std::to_string(count)});
                text(fontH + r.depth * fontH * 2, y, wide(label), RGB(140, 180, 220));
                continue;
            }
            if (r.kind == Kind::Test || r.kind == Kind::Layout) {  // 3.0.5-beta.6 (3.0.11-beta.75: the layout)
                const int x = fontH + r.depth * fontH * 2;
                text(x, y, L"▶", RGB(229, 169, 78));
                text(x + fontH * 3 / 2, y,
                     wide(tr(lang_, r.kind == Kind::Test ? Text::TestRow : Text::ConsoleHudEdit)),
                     RGB(240, 236, 226));
                continue;
            }
            if (r.kind == Kind::Tabs) {  // 3.0.17-beta.4: the tabs, the shown one bright between ‹ ›
                const Text names[kTabs] = {Text::ConsoleTabQuick, Text::ConsoleTabNew, Text::LookPicture,
                                           Text::LookUi,           Text::LookTextures,    Text::ConsoleTabAll};
                int x = fontH;
                for (int k = 0; k < kTabs; ++k) {
                    const std::wstring word = k == tab_ ? L"\u2039 " + wide(tr(lang_, names[k])) + L" \u203a"
                                                        : wide(tr(lang_, names[k]));
                    text(x, y, word, k == tab_ ? RGB(229, 169, 78) : RGB(170, 170, 170), k == tab_);
                    SIZE sz{};
                    SelectObject(dc, k == tab_ ? bold : font);
                    GetTextExtentPoint32W(dc, word.c_str(), int(word.size()), &sz);
                    x += int(sz.cx) + fontH * 3 / 2;
                }
                continue;
            }
            if (r.kind == Kind::Look) {  // 3.0.17-beta.4: «Картинка: Ремастер · змінено 2»
                const presets::Choice ch = presets::Choice(std::atoi(r.key.c_str()));
                const presets::Match m = presets::match(ch, config);
                std::string value = tr(lang_, presets::nameOf(ch, m.variant));
                if (m.changed > 0)
                    value = fmt(tr(lang_, Text::LookChanged), {value, std::to_string(m.changed)});
                text(fontH, y, wide(tr(lang_, presets::headOf(ch))), RGB(200, 200, 190));
                textRight(w - fontH, y, L"\u2039 " + wide(value) + L" \u203a", RGB(240, 236, 226));
                continue;
            }
            if (r.kind == Kind::Tiles) {  // 3.0.17-beta.27: four places a row, each its switch - lit when on
                const std::vector<std::string> keys = consoletiles::split(r.key);
                const int left = fontH, width = w - 2 * fontH, gap = std::max(2, fontH / 3);
                drawnTilesLeft_ = left;
                drawnTilesWidth_ = width;
                const int chosen = top_ + i == at_ ? consoletiles::move(tileCol_, 0, int(keys.size())) : -1;
                for (int k = 0; k < int(keys.size()); ++k) {
                    const IniSetting* ts = settingOf(keys[size_t(k)]);
                    if (!ts) continue;
                    const bool on = tokenOf(*ts, config) == ts->tokens[1];
                    const int x0 = left + k * width / consoletiles::kPerRow, x1 = left + (k + 1) * width /
                                                                                    consoletiles::kPerRow - gap;
                    const uint32_t fill = k == chosen ? 0x004A5A6Au : on ? 0x00243C2Cu : 0x0024282Eu;
                    for (int yy = std::max(0, y - line / 8); yy < std::min(h, y + line - line / 8); ++yy)
                        std::fill(px + size_t(yy) * size_t(w) + size_t(x0), px + size_t(yy) * size_t(w) + size_t(x1),
                                  fill);
                    text(x0 + gap, y, on ? L"■" : L"□", on ? RGB(143, 194, 122) : RGB(150, 150, 150));
                    std::wstring label = wide(tr(lang_, ts->label));
                    RECT place{x0 + gap + fontH * 3 / 2, y, x1 - gap, y + line};
                    SelectObject(dc, font);
                    SetTextColor(dc, on ? RGB(240, 236, 226) : RGB(170, 170, 170));
                    DrawTextW(dc, label.c_str(), int(label.size()), &place,
                              DT_SINGLELINE | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
                }
                continue;
            }
            if (r.kind == Kind::Here) {  // 2.6.22: «Только на этой карте» and the map
                const int x = fontH + r.depth * fontH * 2;
                text(x, y, thisMap_ ? L"■" : L"□", thisMap_ ? RGB(143, 194, 122) : RGB(150, 150, 150));
                text(x + fontH * 3 / 2, y, wide(tr(lang_, Text::ConsoleThisMap)), RGB(240, 236, 226));
                if (!map_.empty()) textRight(w - fontH, y, wide(map_), RGB(150, 160, 160));
                continue;
            }
            const IniSetting* s = settingOf(r.key);
            if (!s) continue;
            const std::string tok = tokenOf(*s, config);
            const int x = fontH + r.depth * fontH * 2;
            const bool held = !comparing_.empty() && comparing_ == r.key;
            if (s->control == Control::Check) {
                const bool on = tok == s->tokens[1] && !held;
                // 2.6.8: an effect on under a group that is off draws nothing (effect_groups::effective) - grey, with
                // its group named off, not green as if it worked
                const effect_groups::Member* member = effect_groups::memberOf(r.key);
                const bool muted = on && member && !config.effects.groups[member->group];
                const bool lit = on && !muted;
                text(x, y, on ? L"■" : L"□", lit ? RGB(143, 194, 122) : RGB(150, 150, 150));
                std::wstring label = wide(tr(lang_, s->label));
                if (consolepins::pinned(pins_, r.key)) label = L"\u2605 " + label;  // 3.0.17-beta.4
                if (r.key.compare(0, 3, "ai.") == 0 && aiWaits(r.key, file.ai, file.aiNow))
                    label += L"  \u21bb";  // 2.2.1: at the next start
                if (muted) label += L"  (" + wide(tr(lang_, Text::ConsoleGroupOff)) + L")";
                text(x + fontH * 3 / 2, y, label, lit ? RGB(240, 236, 226) : RGB(170, 170, 170));
                // 2.2.1: its timed pass's ms (a group's: its effects' passes, each once)
                double ms = -1.0;
                int g = 0;
                while (g < kEffectGroups && r.key != groupKey(g)) ++g;
                if (g < kEffectGroups) {
                    std::set<std::string> passes;
                    for (int k = 0; k < effect_groups::memberCount(); ++k) {
                        const effect_groups::Member& m = effect_groups::members()[k];
                        const std::string pass = m.group == g ? effect_costs::passOf(m.key) : "";
                        const auto c = pass.empty() || !passes.insert(pass).second ? costs.end() : costs.find(pass);
                        if (c != costs.end()) ms = std::max(ms, 0.0) + c->second;
                    }
                } else {
                    ms = effect_costs::costOf(costs, r.key);
                }
                if (ms >= 0.005) {
                    char v[32];
                    std::snprintf(v, sizeof v, "%.2f ms", ms);
                    textRight(w - fontH, y, wide(v), RGB(150, 160, 160));
                }
            } else if (s->control == Control::Slider) {
                text(x, y, wide(tr(lang_, s->label)), RGB(200, 200, 190));
                textRight(w - fontH, y, wide(tok + s->unit), RGB(240, 236, 226));
            } else if (s->control == Control::Choice) {
                std::string choice = tok;
                for (size_t k = 0; k < s->tokens.size() && k < s->choices.size(); ++k)
                    if (tok == s->tokens[k]) choice = tr(lang_, s->choices[k]);
                if (r.key == "effects.preset" && tok == "auto") {  // 2.2.2: "Auto: Medium" - the card's (was "{1}")
                    static const Text kLevels[] = {Text::FxPresetWeak, Text::FxPresetWeak, Text::FxPresetMedium,
                                                   Text::FxPresetStrong};
                    const int level = std::clamp(Context::get().cardLevel(), 0, 3);
                    choice = fmt(tr(lang_, Text::FxPresetAuto), {tr(lang_, kLevels[level])});
                }
                text(x, y, wide(tr(lang_, s->label)), RGB(200, 200, 190));
                textRight(w - fontH, y, L"◂ " + wide(choice) + L" ▸", RGB(240, 236, 226));
            }
        }
        if (const IniSetting* cs = settingOf(chosenKey())) {  // 3.0.17-beta.27: «що це дає»
            const std::wstring gives = wide(consoletiles::gist(tr(lang_, cs->hint), 110));
            RECT place{fontH, h - footH - line / 4 - line, w - fontH, h - footH - line / 4};
            SelectObject(dc, font);
            SetTextColor(dc, RGB(150, 190, 205));
            DrawTextW(dc, gives.c_str(), int(gives.size()), &place,
                      DT_SINGLELINE | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
        }
        RECT foot{fontH, h - footH - line / 4, w - fontH, h};  // 2.2.1: wrapped (3.0.11-beta.209: as many lines)
        SelectObject(dc, font);
        SetTextColor(dc, RGB(150, 160, 160));
        DrawTextW(dc, keys.c_str(), int(keys.size()), &foot, DT_WORDBREAK | DT_LEFT | DT_NOPREFIX);
        if (mouseIn) {  // .270: the arrow, white with a dark edge, its tip at the mouse
            const int mx = int(mouseAt.x - box.x), my = int(mouseAt.y - box.y), a = std::max(12, fontH);
            const POINT arrow[7] = {{mx, my},
                                    {mx, my + a * 17 / 12},
                                    {mx + a * 4 / 12, my + a * 13 / 12},
                                    {mx + a * 7 / 12, my + a * 19 / 12},
                                    {mx + a * 9 / 12, my + a * 18 / 12},
                                    {mx + a * 6 / 12, my + a * 12 / 12},
                                    {mx + a * 10 / 12, my + a * 12 / 12}};
            HPEN pen = CreatePen(PS_SOLID, std::max(1, a / 12), RGB(16, 16, 16));
            HBRUSH brush = CreateSolidBrush(RGB(250, 250, 250));
            HGDIOBJ oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, brush);
            Polygon(dc, arrow, 7);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(pen);
            DeleteObject(brush);
        }
        // 3.0.11-beta.260 (the tester: «написано непрозорим, а воно прозоре»): its ground opaque, not the panels'
        const uint32_t ground = 255;
        for (size_t i = 0; i < size_t(w) * size_t(h); ++i) {
            const uint32_t c = px[i] & 0xFFFFFF;
            const uint32_t v = std::max({c & 0xFF, (c >> 8) & 0xFF, c >> 16});
            const uint32_t a = v > 0x50 ? 255u : ground;  // the panel see-through, the text solid
            const uint32_t r = (c >> 16) * a / 255, g = ((c >> 8) & 0xFF) * a / 255, b = (c & 0xFF) * a / 255;
            px[i] = a << 24 | r << 16 | g << 8 | b;
        }
        drawnBox_ = RECT{box.x, box.y, box.x + w, box.y + h};  // 3.0.11-beta.260: where the mouse finds the rows
        drawnFirstRow_ = line + line / 2 - line / 8;
        drawnLine_ = line;
        drawnVisible_ = visible;
        POINT at{0, 0};
        ClientToScreen(game, &at);
        POINT pos{at.x + box.x, at.y + box.y}, origin{0, 0};
        SIZE extent{w, h};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(window_, screen, &pos, &extent, dc, &origin, 0, &blend, ULW_ALPHA);
        if (!IsWindowVisible(window_)) ShowWindow(window_, SW_SHOWNOACTIVATE);
        SelectObject(dc, oldFont);
        SelectObject(dc, oldBmp);
    }
    if (bmp) DeleteObject(bmp);
    DeleteObject(font);
    DeleteObject(bold);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

}  // namespace uagfx::win

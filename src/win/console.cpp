#include "win/console.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>

#include "core/console_key.hpp"
#include "core/effect_costs.hpp"
#include "core/effect_groups.hpp"
#include "core/effect_where.hpp"
#include "core/settings/effects_layout.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/search.hpp"
#include "win/context.hpp"  // 3.0.4-beta.35: the card's level now (judged by its speed)
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win {

std::vector<Console::Subclassed> Console::subclassed_;
Console* Console::active_ = nullptr;
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
        rows_.push_back(Row{Kind::Box, "frames.fps_counter", 0});  // the frame counter (Ctrl+Shift+F11 as well)
        // 3.0.5-beta.6: «Тест графіки» - not the first row, where the console opens: a Space there starts nothing
        rows_.push_back(Row{Kind::Test, "", 0});
        // 3.0.11-beta.2: the new interface's minimap prototype - on and off in the game (the owner could not find it)
        if (settingOf("interface.minimap")) {
            rows_.push_back(Row{Kind::Heading, tr(lang, Text::InterfaceHeading), 0});
            rows_.push_back(Row{Kind::Layout, "", 0});  // 3.0.11-beta.75: «Розміщення інтерфейсу»
            rows_.push_back(Row{Kind::Box, "interface.minimap", 0});
            for (const char* k : {"interface.minimap_npcs", "interface.minimap_warps", "interface.minimap_mobs",
                                  "interface.reference"})
                if (settingOf(k)) rows_.push_back(Row{Kind::Box, k, 0});  // 3.0.11-beta.6: its hints
            if (settingOf("interface.minimap_way"))  // 3.0.11-beta.33: the way on the ground
                rows_.push_back(Row{Kind::Choice, "interface.minimap_way", 1});
            if (settingOf("interface.minimap_reach"))  // 3.0.11-beta.65: where one can walk, an experiment
                rows_.push_back(Row{Kind::Box, "interface.minimap_reach", 0});
            if (settingOf("interface.minimap_hd"))  // 3.0.11-beta.72: the maps' pictures four times bigger
                rows_.push_back(Row{Kind::Box, "interface.minimap_hd", 0});
            if (settingOf("interface.pack_icons"))  // 3.0.11-beta.74: the panels' icons from the AI pack
                rows_.push_back(Row{Kind::Box, "interface.pack_icons", 0});
            if (settingOf("interface.lock_layout"))  // 3.0.11-beta.77: the parts kept where they are
                rows_.push_back(Row{Kind::Box, "interface.lock_layout", 0});
            if (settingOf("interface.hotbar_per_character"))  // 3.0.11-beta.78: each character's own bar
                rows_.push_back(Row{Kind::Box, "interface.hotbar_per_character", 0});
            if (settingOf("interface.pad_hints"))  // 3.0.11-beta.86: the controller's buttons on the interface
                rows_.push_back(Row{Kind::Box, "interface.pad_hints", 0});
            if (settingOf("interface.target_ring"))  // 3.0.11-beta.87: the target's ring on the ground
                rows_.push_back(Row{Kind::Choice, "interface.target_ring", 0});
            if (settingOf("interface.pad_walk"))  // 3.0.11-beta.88: the left stick without the mouse
                rows_.push_back(Row{Kind::Choice, "interface.pad_walk", 0});
            if (settingOf("interface.buffs"))  // 3.0.11-beta.35: the buffs' column
                rows_.push_back(Row{Kind::Choice, "interface.buffs", 0});
            if (settingOf("interface.buffs_overflow"))  // 3.0.11-beta.63: what it does not hold
                rows_.push_back(Row{Kind::Choice, "interface.buffs_overflow", 1});
            if (settingOf("interface.character"))  // 3.0.11-beta.36: the character block
                rows_.push_back(Row{Kind::Choice, "interface.character", 0});
            if (settingOf("interface.target"))  // 3.0.11-beta.38: the target's card
                rows_.push_back(Row{Kind::Box, "interface.target", 0});
            if (settingOf("interface.party"))  // 3.0.11-beta.39: the party's panel
                rows_.push_back(Row{Kind::Box, "interface.party", 0});
            if (settingOf("interface.tracker"))  // 3.0.11-beta.70: the tracker
                rows_.push_back(Row{Kind::Box, "interface.tracker", 0});
            if (settingOf("interface.hotbar"))  // 3.0.11-beta.40: the hotkeys' bar
                rows_.push_back(Row{Kind::Box, "interface.hotbar", 0});
            if (settingOf("interface.chat"))  // 3.0.11-beta.41: the chat
                rows_.push_back(Row{Kind::Box, "interface.chat", 0});
            if (settingOf("interface.cast"))  // 3.0.11-beta.48: the cast's label over the hero
                rows_.push_back(Row{Kind::Box, "interface.cast", 0});
            if (settingOf("interface.dim_world"))  // 3.0.11-beta.49: the world dimmed under the windows
                rows_.push_back(Row{Kind::Box, "interface.dim_world", 0});
            if (settingOf("interface.low_hp_edge"))  // 3.0.11-beta.50: the screen's edges red at low HP
                rows_.push_back(Row{Kind::Slider, "interface.low_hp_edge", 0});
            if (settingOf("interface.size"))  // 3.0.11-beta.54: the new interface's size
                rows_.push_back(Row{Kind::Choice, "interface.size", 0});
            if (settingOf("interface.text_size"))  // 3.0.11-beta.67: the reading windows' text
                rows_.push_back(Row{Kind::Choice, "interface.text_size", 0});
            for (const char* key : {"interface.items", "interface.status"})  // 3.0.11-beta.42: the windows
                if (settingOf(key)) rows_.push_back(Row{Kind::Box, key, 0});
            if (settingOf("interface.animations"))  // 3.0.11-beta.46: the new interface's motion
                rows_.push_back(Row{Kind::Box, "interface.animations", 0});
            for (const char* key : {"interface.panel_opacity", "interface.window_opacity"})  // 3.0.11-beta.47
                if (settingOf(key)) rows_.push_back(Row{Kind::Slider, key, 0});
        }
        // 3.0.3-beta.8: the game windows' panel (the user: on or off in the console too)
        if (settingOf("panel.show")) {
            rows_.push_back(Row{Kind::Heading, tr(lang, Text::PanelHeading), 0});
            rows_.push_back(Row{Kind::Box, "panel.show", 0});
            rows_.push_back(Row{Kind::Choice, "panel.layout", 1});
            rows_.push_back(Row{Kind::Slider, "panel.size", 1});
            rows_.push_back(Row{Kind::Slider, "panel.opacity", 1});
        }
        // 2.2.1: the AI pictures of the texture pack - a kind off hidden at once, on at once when the game started
        // with it
        rows_.push_back(Row{Kind::Heading, tr(lang, Text::PageAi), 0});
        for (const char* k : {"ai.enabled", "ai.ground", "ai.objects", "ai.monsters", "ai.players", "ai.npcs",
                              "ai.items", "ai.effects", "ai.icons"})
            if (settingOf(k)) rows_.push_back(Row{Kind::Box, k, std::string(k) == "ai.enabled" ? 0 : 1});
        if (settingOf("debug.xray")) rows_.push_back(Row{Kind::Box, "debug.xray", 1});  // 2.3: Ctrl+Shift+F9 too
        if (settingOf("debug.seams_view")) rows_.push_back(Row{Kind::Choice, "debug.seams_view", 1});  // 2.7.8
        rows_.push_back(Row{Kind::Heading, tr(lang, Text::FxPreset), 0});
        rows_.push_back(Row{Kind::Choice, "effects.preset", 0});
        rows_.push_back(Row{Kind::Choice, "effects.tier", 0});
        if (settingOf("effects.clock")) {  // 2.6.19: the time of day for all the effects, by the set and the mode
            rows_.push_back(Row{Kind::Choice, "effects.clock", 0});
            rows_.push_back(Row{Kind::Slider, "effects.night_strength", 1});
            if (settingOf("effects.season")) rows_.push_back(Row{Kind::Choice, "effects.season", 0});  // 2.6.22
            if (settingOf("effects.weather_kind")) rows_.push_back(Row{Kind::Choice, "effects.weather_kind", 0});
        }
        rows_.push_back(Row{Kind::Here, "", 0});  // 2.6.22
        for (int g = 0; g < kEffectGroups; ++g) {
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
    if (r.kind == Kind::Here) {  // 2.6.22: «Только на этой карте» - the console's own, not the file's
        thisMap_ = step == 0 ? !thisMap_ : step > 0;
        build(lang_, config);
        dirty_ = true;
        return false;
    }
    const IniSetting* s = r.kind == Kind::Heading ? nullptr : settingOf(r.key);
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
    if (toggled && console::toggles(GetAsyncKeyState(VK_CONTROL) < 0, console::kKey)) {
        open_ = !open_;
        if (open_) {
            std::string text;
            readTextFile(iniPath, text);
            const auto ini = readIni(text);
            const auto lang = ini.count("utility.language") ? parseLang(ini.at("utility.language")) : std::nullopt;
            lang_ = startLang(lang);
            filter_.clear();
            typed_.clear();
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
    if (pressed(VK_RIGHT, right_, true, now)) changed = change(iniPath, config, 1) || changed;
    if (testAsked_ || layoutAsked_) {  // 3.0.5-beta.6: the test runs over the game alone (3.0.11-beta.75: the layout)
        LOG_INFO("console: %s chosen - closed", testAsked_ ? "«Тест графіки»" : "«Розмістити інтерфейс»");
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

void Console::hide() {
    if (window_ && IsWindowVisible(window_)) ShowWindow(window_, SW_HIDE);
    shownText_.clear();
}

void Console::draw(HWND game, const Config& file, const GpuTiming* timing) {
    // Redrawn when anything shown changed: the values come from the config the driver read (2.2: as the set has them).
    const Config config = shownConfig(file);
    std::string state = std::to_string(at_) + "|" + comparing_ + "|" + std::to_string(timing ? timing->serial : 0) +
                        "|" + filter_ + "|" + (thisMap_ ? "1" : "0") + map_;
    for (const Row& r : rows_)
        if (r.kind != Kind::Heading)
            if (const IniSetting* s = settingOf(r.key)) state += tokenOf(*s, config) + ",";
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
    const int fontH = std::max(14, GetDeviceCaps(screen, LOGPIXELSY) * 15 / 96);
    const int line = fontH * 3 / 2;
    const int w = std::max(fontH * 26, int(client.right - client.left) * 2 / 5);
    const int h = std::max(line * 8, int(client.bottom - client.top) * 3 / 4);
    const int visible = std::max(3, (h - line * 4) / line);  // the title, two lines of keys at the foot
    if (at_ < top_) top_ = at_;
    if (at_ >= top_ + visible) top_ = at_ - visible + 1;
    top_ = std::clamp(top_, 0, std::max(0, int(rows_.size()) - visible));
    HFONT font = CreateFontW(-fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT bold = CreateFontW(-fontH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
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
        RECT foot{fontH, h - 2 * line - line / 4, w - fontH, h};  // 2.2.1: two lines, wrapped
        SelectObject(dc, font);
        SetTextColor(dc, RGB(150, 160, 160));
        const std::wstring keys = wide(tr(lang_, Text::ConsoleKeys));
        DrawTextW(dc, keys.c_str(), int(keys.size()), &foot, DT_WORDBREAK | DT_LEFT | DT_NOPREFIX);
        for (size_t i = 0; i < size_t(w) * size_t(h); ++i) {
            const uint32_t c = px[i] & 0xFFFFFF;
            const uint32_t v = std::max({c & 0xFF, (c >> 8) & 0xFF, c >> 16});
            const uint32_t a = v > 0x50 ? 255u : 215u;  // the panel a little see-through, the text solid
            const uint32_t r = (c >> 16) * a / 255, g = ((c >> 8) & 0xFF) * a / 255, b = (c & 0xFF) * a / 255;
            px[i] = a << 24 | r << 16 | g << 8 | b;
        }
        POINT at{0, 0};
        ClientToScreen(game, &at);
        POINT pos{at.x + fontH, at.y + fontH * 3}, origin{0, 0};
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

#include "win/options_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/options_card.hpp"
#include "core/settings/strings.hpp"
#include "core/settings/schema.hpp"
#include "core/settings_window.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"
#include "win/console.hpp"
#include "win/setting_edit.hpp"
#include "win/words.hpp"

namespace uagfx::win::optionscardwin {
namespace {

namespace oc = uagfx::optionscard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the options window (where the manager keeps it, its class's vtable, its SendMsg), a window's shown
// flag; the switches the game keeps (in our toggles' order: the music off - inverted -, the sounds, the snaps, opaque)
// and the window's volumes (the music's +0x74, the sounds' +0x70); its refresh message (0x3c: the controls from them)
constexpr uint32_t kWindowMgr = 0x727ED8, kOptionsAt = kWindowMgr + 0x98, kOptionsClass = 0x698830;
constexpr uint32_t kOptionsSend = 0x473DD0, kShown = 0x38, kSendMsgSlot = 0x94;
constexpr uint32_t kFlags[6] = {0x772D28, 0x6B8320, 0x77196C, 0x771968, 0x771960, 0x77195C};
constexpr uint32_t kVolumes[2] = {0x74, 0x70};
constexpr int kButtonMsg = 6, kRefresh = 0x3C, kPadStep = 8;
constexpr uint32_t kMakeWindow = 0x4D42D0;  // 3.0.18-beta.122: the windows' manager's MakeWindow (UI window 0xD)
constexpr int kOptionsWindow = 0xD;

// 3.0.18-beta.82 (item 4): the game's options a live window - its volumes the library's sliders, its switches
// ticks. 3.0.18-beta.122 (the concept «Ігровий інтерфейс», stages 5-6: «Налаштування» in place of Alt+O): sections on
// the left (core settingswin: «Гра» - the game's own as before -, «Як виглядає гра», the settings app's «Інтерфейс»
// sections), a search over them all, each row a switch, a slider or choices; ours written into uaro-graphics.ini as
// the console writes them (win/setting_edit)
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw ow sw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span><span class="hk">Alt+O</span></div>
<span class="x ctl" data-event-click="opt_close()">&#x2715;</span>
<div class="srch"><input class="ctl" type="text" data-value="query"/><span class="ph"
  data-if="query == ''">{{search_hint}}</span></div>
<div class="sbody">
 <div class="snav"><div class="sn ctl" data-for="n, i : nav" data-class-on="i == sec && query == ''"
   data-event-click="sec_go(i)"><span class="nn">{{n.name}}</span><span class="cnt"
   data-if="n.count > 0">{{n.count}}</span></div></div>
 <div class="spane"><div data-for="r, i : srows" data-if="r.label != ''"><div class="sgrp"
   data-if="r.group != ''">{{r.group}}</div><div class="sro"><div class="st"><span
   class="sl">{{r.label}}</span><span class="shn" data-if="r.hint != ''">{{r.hint}}</span></div><div
   class="sctl"><span class="tg ctl" data-if="r.toggle" data-class-on="r.on"
   data-event-click="set_toggle(i)"><span class="kn"></span></span><input class="ctl" data-if="r.slider"
   type="range" data-attr-min="r.min" data-attr-max="r.max" data-attr-step="r.step" data-attr-value="r.value"
   data-event-change="set_slider(i)"/><span class="ch ctl" data-if="r.slider"
   data-event-click="set_step(i, -1)">&#x2212;</span><span class="sv" data-if="r.slider">{{r.vtext}}</span><span
   class="ch ctl" data-if="r.slider" data-event-click="set_step(i, 1)">+</span><div class="chs"
   data-if="r.choices.size > 0 && r.choices[0].label != ''"><span class="ch ctl"
   data-for="c, j : r.choices" data-if="c.label != ''" data-class-on="c.on"
   data-event-click="set_choice(i, j)">{{c.label}}</span></div></div></div></div>
   <div class="snone" data-if="shown_rows == 0">{{none}}</div></div>
</div>
<div class="sfoot"><span class="sfh">{{hint}}</span><span class="btn ctl"
  data-event-click="open_console()">{{console_btn}}</span></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
struct OptionRowView {
    Rml::String label;
    bool on = false;
    int vol = 0;
};
struct ChoiceView {
    Rml::String label;
    bool on = false;
};
struct SetRowView {  // 3.0.18-beta.122
    Rml::String key, group, label, hint, vtext;
    bool toggle = false, on = false, slider = false;
    int value = 0, min = 0, max = 0, step = 1;
    std::vector<ChoiceView> choices;
};
constexpr size_t kMostChoices = 12, kMostRows = 24;  // 3.0.18-beta.132

struct NavView {
    Rml::String name;
    int count = 0;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<OptionRowView> rowViews;
    // 3.0.18-beta.122: «Налаштування» - the section, the search, the rows shown, the config and the file they write
    int sec = 0;
    Rml::String query, shownQuery, searchHint, none, consoleBtn;
    std::vector<NavView> navViews;
    std::vector<SetRowView> setRows;
    int shownRows = 0;  // 3.0.18-beta.132: the rows with words (the list padded)
    std::string rowsKey;
    const Config* config = nullptr;
    std::string iniPath;
    bool changed = false;
    Rml::String title, hint, ground = "#10141cf0";
    uisheet::Sheet sheet;  // 3.0.18-beta.34: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false;
    uint32_t hidden = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    oc::Values values;
    oc::Show show;
    oc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int dragging = -1;  // the slider held
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t gameOptions() {
    const uint32_t w = read32(kOptionsAt);
    return w && read32(w) == kOptionsClass && read32(read32(w) + kSendMsgSlot) == kOptionsSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("options: the game's options window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

oc::Values valuesOf(uint32_t window) {
    oc::Values v;
    for (int i = 0; i < 6; ++i) v.on[i] = read32(kFlags[i]) != 0;
    v.on[0] = !v.on[0];  // the game keeps the music's «off»
    for (int i = 0; i < 2; ++i)
        v.volume[i] = std::clamp(int(int32_t(read32(window + kVolumes[i]))), 0, oc::kVolumeMost);
    return v;
}

oc::Texts texts() {
    oc::Texts t;
    t.title = say(settings::Text::OptionsTitle);
    const settings::Text names[6] = {settings::Text::OptionsMusic,      settings::Text::OptionsSounds,
                                     settings::Text::OptionsAttackSnap, settings::Text::OptionsSkillSnap,
                                     settings::Text::OptionsItemSnap,   settings::Text::OptionsOpaque};
    for (int i = 0; i < 6; ++i) t.toggles[i] = say(names[i]);
    t.snapHint = say(settings::Text::OptionsSnapHint);
    return t;
}

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.glass;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    int lx = 0, ly = 0, lw = 0, lh = 0;  // 3.0.18-beta.81: the live window's own box
    if (s.live.box(lx, ly, lw, lh)) x0 = float(lx), y0 = float(ly), b.w = float(lw), b.h = float(lh);
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(Blocker::Options, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Options, nullptr, nullptr, false);
    s.shown = false;
    s.dragging = -1;
}

void toggle(State& s, int i) {
    const uint32_t window = gameOptions();
    const uint32_t box = window && i >= 0 && i < 6 ? read32(window + oc::kToggleAt[i]) : 0;
    if (!box) return;
    const bool now = !s.values.on[i];
    const auto send = reinterpret_cast<SendMsgFn>(uintptr_t(kOptionsSend));
    send(window, box, kButtonMsg, oc::kCheckMsg, now ? 1 : 0, 0);
    send(window, 0, kRefresh, 0, 0, 0);  // its own checkboxes from the game's switches again
    if (s.told++ < 40) LOG_INFO("options: switch %d %s (its checkbox's 0x93)", i, now ? "on" : "off");
    s.values = valuesOf(window);
    s.key.clear();
}

void setVolume(State& s, int i, int volume) {
    const uint32_t window = gameOptions();
    const uint32_t slider = window && i >= 0 && i < 2 ? read32(window + oc::kSliderAt[i]) : 0;
    if (!slider) return;
    const int target = std::clamp(volume, 0, oc::kVolumeMost), by = target - s.values.volume[i];
    if (!by) return;
    reinterpret_cast<SendMsgFn>(uintptr_t(kOptionsSend))(window, slider, oc::kStepMsg, by, 0, 0);
    if (s.told++ < 40) LOG_INFO("options: volume %d to %d (its slider's message 8 by %d)", i, target, by);
    s.values = valuesOf(window);
    s.key.clear();
}

void closeIt(State& s) {
    const uint32_t window = gameOptions();
    if (!window) return;
    if (s.told++ < 40) LOG_INFO("options: closed (the window's 0x87)");
    reinterpret_cast<SendMsgFn>(uintptr_t(kOptionsSend))(window, 0, kButtonMsg, oc::kCloseButton, 0, 0);
    s.key.clear();
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    (void)codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("options: [interface] options = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameOptions() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = oc::Show{};
        LOG_INFO("options: the game's options window - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.values = valuesOf(window);
    s.layout = oc::layout(targetWidth, targetHeight, s.scale);
    s.show.hoverToggle = s.show.hoverSlider = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const oc::Hit h = oc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == oc::Part::Toggle) s.show.hoverToggle = h.index;
        if (h.part == oc::Part::Slider) s.show.hoverSlider = h.index;
        s.show.hoverClose = h.part == oc::Part::Close;
    }
    std::string key = std::to_string(s.values.volume[0]) + ',' + std::to_string(s.values.volume[1]) + ';' +
                      std::to_string(s.show.hoverToggle) + ',' + std::to_string(s.show.hoverSlider) +
                      (s.show.hoverClose ? "x" : "") + ',' + std::to_string(s.show.focus) + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (bool b : s.values.on) key += b ? '1' : '0';
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.34
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.122: «Налаштування»'s rows - the section's (or every section's the search finds), as they are now
static std::vector<SetRowView> buildRows(const State& s) {
    namespace sw = uagfx::settingswin;
    const settings::Lang lang = language();
    const std::vector<sw::Section>& secs = sw::sections();
    std::vector<sw::Found> list;
    const bool searching = !s.query.empty();
    if (searching) {
        list = sw::find(s.query, lang);
    } else if (s.sec >= 0 && size_t(s.sec) < secs.size()) {
        for (const std::string& k : secs[size_t(s.sec)].keys) list.push_back(sw::Found{s.sec, k});
    }
    const oc::Texts t = texts();
    const Config shown = s.config ? settingedit::shown(*s.config) : Config{};
    std::vector<SetRowView> out;
    int lastSec = -1;
    for (const sw::Found& f : list) {
        SetRowView r;
        r.key = f.key;
        if (searching && f.section != lastSec) r.group = say(secs[size_t(f.section)].name);
        lastSec = f.section;
        const sw::Kind kind = secs[size_t(f.section)].kind;
        if (kind == sw::Kind::Game) {
            const int i = f.key.back() - '0';
            r.label = t.toggles[i];
            r.toggle = true, r.on = s.values.on[i];
            if (i < 2) r.slider = true, r.min = 0, r.max = oc::kVolumeMost, r.value = s.values.volume[i];
            if (i < 2) r.vtext = std::to_string(s.values.volume[i]);
            if (i == 2) r.hint = t.snapHint;
        } else if (kind == sw::Kind::Picture) {
            const auto c = settings::presets::Choice(f.key.back() - '0');
            r.label = say(settings::presets::headOf(c));
            const int now = s.config ? settings::presets::match(c, shown).variant : -1;
            for (int v = 0; v < settings::presets::variants(c); ++v)
                r.choices.push_back(ChoiceView{say(settings::presets::nameOf(c, v)), v == now});
        } else if (const settings::IniSetting* st = settings::findSetting(f.key)) {
            r.label = say(st->label);
            r.hint = say(st->hint);
            if (r.hint.size() > 160) r.hint.clear();  // the long ones stay the app's (a row, not a page)
            const std::string now = s.config ? settings::tokenOf(*st, shown) : "";
            if (st->control == settings::Control::Check) {
                r.toggle = true, r.on = st->tokens.size() > 1 && now == st->tokens[1];
            } else if (st->control == settings::Control::Slider) {
                r.slider = true, r.min = st->min, r.max = st->max, r.step = std::max(1, st->step);
                r.value = std::atoi(now.c_str());
                r.vtext = now + (st->unit && *st->unit ? std::string(" ") + st->unit : "");
            } else {
                for (size_t j = 0; j < st->tokens.size(); ++j)
                    r.choices.push_back(ChoiceView{j < st->choices.size() ? say(st->choices[j]) : st->tokens[j],
                                                   now == st->tokens[j]});
            }
        }
        out.push_back(std::move(r));
    }
    return out;
}

static std::string rowsKeyOf(const std::vector<SetRowView>& rows) {
    std::string k;
    for (const SetRowView& r : rows) {
        k += r.key + (r.on ? "+" : "-") + std::to_string(r.value) + r.group;
        for (const ChoiceView& c : r.choices) k += c.on ? '1' : '0';
        k += '|';
    }
    return k;
}

// A row's control used: the game's own through its window, ours into uaro-graphics.ini (read again at the next
// present: `changed`)
static void useRow(State& s, int row, int what, int value) {
    if (row < 0 || size_t(row) >= s.setRows.size()) return;
    const std::string key = s.setRows[size_t(row)].key;
    if (key.rfind("game.", 0) == 0) {
        const int i = key.back() - '0';
        if (what == 0) toggle(s, i);
        else if (what == 1) setVolume(s, i, value);
        return;
    }
    if (!s.config || s.iniPath.empty()) return;
    bool wrote = false;
    if (key.rfind("picture.", 0) == 0) {
        // 3.0.18-beta.132 (R-4BXZPM «пропал новый интерфейс на настройках»):
        // «Новий екран, вікна гри» or «Класичний» chosen here turned [interface] options
        // off - this very window gone; it stays (the game's other windows as chosen)
        if (what == 2)
            wrote = settingedit::setLook(s.iniPath, settings::presets::Choice(key.back() - '0'), value,
                                         {"interface.options"});
    } else if (const settings::IniSetting* st = settings::findSetting(key)) {
        if (what == 0 && st->tokens.size() > 1)
            wrote = settingedit::set(s.iniPath, *s.config, key, st->tokens[s.setRows[size_t(row)].on ? 0 : 1]);
        else if (what == 1)
            wrote = settingedit::set(s.iniPath, *s.config, key, std::to_string(std::clamp(value, st->min, st->max)));
        else if (what == 2 && value >= 0 && size_t(value) < st->tokens.size())
            wrote = settingedit::set(s.iniPath, *s.config, key, st->tokens[size_t(value)]);
    }
    s.changed = s.changed || wrote;
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("options", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<ChoiceView>()) {
            v.RegisterMember("label", &ChoiceView::label);
            v.RegisterMember("on", &ChoiceView::on);
        }
        m.RegisterArray<std::vector<ChoiceView>>();
        if (auto v = m.RegisterStruct<SetRowView>()) {
            v.RegisterMember("group", &SetRowView::group);
            v.RegisterMember("label", &SetRowView::label);
            v.RegisterMember("hint", &SetRowView::hint);
            v.RegisterMember("vtext", &SetRowView::vtext);
            v.RegisterMember("toggle", &SetRowView::toggle);
            v.RegisterMember("on", &SetRowView::on);
            v.RegisterMember("slider", &SetRowView::slider);
            v.RegisterMember("value", &SetRowView::value);
            v.RegisterMember("min", &SetRowView::min);
            v.RegisterMember("max", &SetRowView::max);
            v.RegisterMember("step", &SetRowView::step);
            v.RegisterMember("choices", &SetRowView::choices);
            v.RegisterMember("key", &SetRowView::key);
        }
        m.RegisterArray<std::vector<SetRowView>>();
        if (auto v = m.RegisterStruct<NavView>()) {
            v.RegisterMember("name", &NavView::name);
            v.RegisterMember("count", &NavView::count);
        }
        m.RegisterArray<std::vector<NavView>>();
        m.Bind("srows", &s.setRows);
        m.Bind("shown_rows", &s.shownRows);
        m.Bind("nav", &s.navViews);
        m.Bind("sec", &s.sec);
        m.Bind("query", &s.query);
        m.Bind("title", &s.title);
        m.Bind("hint", &s.hint);
        m.Bind("search_hint", &s.searchHint);
        m.Bind("none", &s.none);
        m.Bind("console_btn", &s.consoleBtn);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("sec_go", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (a.empty()) return;
            st.sec = a[0].Get<int>();
            st.query.clear();
            st.live.doc.dirty("query");
            st.live.doc.dirty("sec");
            st.rowsKey.clear();
        });
        m.BindEventCallback("set_toggle", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) useRow(state(), a[0].Get<int>(), 0, 0);
        });
        m.BindEventCallback("set_slider", [](Rml::DataModelHandle, Rml::Event& e, const Rml::VariantList& a) {
            if (a.empty()) return;
            useRow(state(), a[0].Get<int>(), 1, std::atoi(e.GetParameter<Rml::String>("value", "0").c_str()));
        });
        // 3.0.18-beta.124: − and + by a slider (the pad's way to it): a step of its own (the volumes 8)
        m.BindEventCallback("set_step", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (a.size() < 2) return;
            const int i = a[0].Get<int>(), by = a[1].Get<int>();
            if (i < 0 || size_t(i) >= st.setRows.size()) return;
            const SetRowView& r = st.setRows[size_t(i)];
            const int step = r.key.rfind("game.", 0) == 0 ? kPadStep : std::max(1, r.step);
            useRow(st, i, 1, std::clamp(r.value + by * step, r.min, r.max));
        });
        m.BindEventCallback("set_choice", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (a.size() >= 2) useRow(state(), a[0].Get<int>(), 2, a[1].Get<int>());
        });
        m.BindEventCallback("open_console", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            Console::askOpen();
            closeIt(state());
        });
        m.BindEventCallback("opt_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            closeIt(state());
        });
    });
    if (!s.live.made()) return;
    if (!s.shown) {  // 3.0.18-beta.170 (R-CD8KCP: 0.14 ms a frame while closed): nothing built for a hidden card
        s.live.frame(false, s.layout.x, s.layout.y, int(winstack::Win::Options), ".ctl");
        return;
    }
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const Rml::String title = say(settings::Text::MenuSettings);
    if (title != s.title || ground != s.ground) {
        s.title = title, s.hint = say(settings::Text::SettingsWinHint), s.ground = ground;
        s.searchHint = say(settings::Text::SearchCue);
        s.none = say(settings::Text::SettingsWinNone);
        s.consoleBtn = say(settings::Text::SettingsWinConsole);
        std::vector<NavView> nav;
        for (const auto& sec : settingswin::sections()) nav.push_back(NavView{say(sec.name), 0});
        s.navViews = std::move(nav);
        s.live.doc.dirtyAll();
    }
    if (s.query != s.shownQuery) {  // the search's counts on the sections
        s.shownQuery = s.query;
        for (NavView& n : s.navViews) n.count = 0;
        for (const settingswin::Found& f : settingswin::find(s.query, language()))
            if (size_t(f.section) < s.navViews.size()) ++s.navViews[size_t(f.section)].count;
        s.live.doc.dirty("nav");
        s.rowsKey.clear();
    }
    std::vector<SetRowView> rows = buildRows(s);
    // 3.0.18-beta.132 (R-4BXZPM «srows[1].choices[10].on»): never shorter lists than before - a shorter one left
    // RmlUi rows past its end for a frame; the spare rows and choices empty and hidden
    for (SetRowView& r : rows)
        while (r.choices.size() < kMostChoices) r.choices.push_back(ChoiceView{});
    while (rows.size() < std::max(s.setRows.size(), size_t(kMostRows))) rows.push_back(SetRowView{});
    const std::string key = rowsKeyOf(rows) + std::to_string(s.sec);
    if (key != s.rowsKey) {
        s.rowsKey = key;
        s.shownRows =
            int(std::count_if(rows.begin(), rows.end(), [](const SetRowView& r) { return !r.label.empty(); }));
        s.setRows = std::move(rows);
        s.live.doc.dirty("srows");
        s.live.doc.dirty("shown_rows");
    }
    int x = s.layout.x, y = s.layout.y;  // the middle of the screen, as wide as its markup makes it
    int bx = 0, by = 0, bw = 0, bh = 0;
    if (s.live.doc.box(bx, by, bw, bh) && bw > 0) {
        x = std::max(0, (s.targetW - bw) / 2);
        y = std::max(0, (s.targetH - bh) / 3);
    }
    s.live.frame(s.shown, x, y, int(winstack::Win::Options), ".ctl");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    liveFrame(s);  // the old edition (the sheet) gone: the live window alone
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.live.shown) return;  // 3.0.18-beta.81
    s.live.draw(s.shadow, record, renderer, s.scale, s.k);
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (s.live.shown) {  // 3.0.18-beta.81: RmlUi's
        if (!s.live.mouse(game, m, wp, lp)) return false;
        result = 0;
        return true;
    }
    return false;  // the old edition gone: the live window alone
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    closeIt(s);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameOptions(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    int& f = s.show.focus;
    if (f < 0) f = 0;
    switch (c) {
        case PadCommand::Up: f = f <= 0 ? 5 : f - 1; break;
        case PadCommand::Down: f = f >= 5 ? 0 : f + 1; break;
        case PadCommand::Left:
        case PadCommand::Right:
            if (f < 2) setVolume(s, f, s.values.volume[f] + (c == PadCommand::Right ? kPadStep : -kPadStep));
            else toggle(s, f);
            break;
        case PadCommand::Press: toggle(s, f); break;
        case PadCommand::Close: closeIt(s); break;
    }
    s.key.clear();
}

bool box(int& x, int& y, int& w, int& h) {
    const State& s = state();
    if (!s.shown) return false;
    if (s.live.box(x, y, w, h)) return true;  // 3.0.18-beta.81
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

void setSettings(const Config* config, const std::string& iniPath) {
    State& s = state();
    s.config = config;
    s.iniPath = iniPath;
}

bool takeChanged() {
    State& s = state();
    const bool c = s.changed;
    s.changed = false;
    return c;
}

bool toggleOpen(const char* why) {
    State& s = state();
    if (!s.on || !inGameWorld()) return false;
    if (gameOptions()) {
        closeIt(s);
    } else {  // the game's own window made (its Alt+O): the new look shows it
        using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
        reinterpret_cast<MakeFn>(uintptr_t(kMakeWindow))(kWindowMgr, kOptionsWindow);
    }
    LOG_INFO("options: «Налаштування» %s (%s)", gameOptions() ? "opened" : "closed", why);
    return true;
}

}  // namespace uagfx::win::optionscardwin

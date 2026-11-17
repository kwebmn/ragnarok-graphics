#include "win/options_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "core/console_tiles.hpp"
#include "core/settings/catalogue.hpp"
#include "core/window_stack.hpp"
#include "core/options_card.hpp"
#include "core/settings/strings.hpp"
#include "core/settings/schema.hpp"
#include "core/settings_window.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "uikit/options_live.hpp"
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
#include "win/view3d.hpp"
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

// 3.0.21-beta.3: the window's markup and model are uikit's (uikit/options_live: laid out by RmlUi on the host's
// tests too)
using uikit::options::ChoiceView;
using uikit::options::NavView;
using SetRowView = uikit::options::RowView;
struct OptionRowView {
    Rml::String label;
    bool on = false;
    int vol = 0;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<OptionRowView> rowViews;
    // 3.0.18-beta.122: «Налаштування» - the section, the search, the rows shown, the config and the file they write
    uikit::options::Model m;  // 3.0.21-beta.3: what the markup shows (the entry, the search, the list, the rows)
    int shownSec = -1;
    std::set<int> open;  // the catalogue's groups opened to their details
    Rml::String shownQuery;
    ULONGLONG builtAt = 0, hotUntil = 0;  // when the rows were built last; built each frame until then
    std::string rowsKey;
    const Config* config = nullptr;
    std::string iniPath;
    bool changed = false;
    bool layoutAsked = false;  // 3.0.21-beta.7: «Розмістити інтерфейс» pressed
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
        LOG_INFO("options: [interface] settings_window = %d", on ? 1 : 0);  // 3.0.21-beta.7: its key now
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

// 3.0.18-beta.122: «Налаштування»'s rows, as they are now. 3.0.21-beta.3 (the concept «Настройки uaRO Graphics»,
// variant 4, step 3): from the catalogue's lines (core settingswin) - the entry's groups one under another: a group's
// name over its first line, its rows, the line that opens its details - or every entry's rows the search finds, each
// group named «entry · group»
static std::vector<SetRowView> buildRows(const State& s) {
    namespace sw = uagfx::settingswin;
    const settings::Lang lang = language();
    const bool searching = !s.m.query.empty();
    const std::vector<sw::Line> list = searching ? sw::find(s.m.query, lang) : sw::lines(s.m.sec, s.open);
    const std::vector<sw::Nav>& nav = sw::nav();
    const oc::Texts t = texts();
    const Config shown = s.config ? settingedit::shown(*s.config) : Config{};
    std::vector<SetRowView> out;
    for (const sw::Line& f : list) {
        SetRowView r;
        r.key = f.key;
        r.sub = f.part;
        if (f.heads) {
            r.group = say(f.groupName);
            if (searching && f.entry > 0 && size_t(f.entry) < nav.size())
                r.group = say(nav[size_t(f.entry)].name) + " \xC2\xB7 " + r.group;
        }
        if (f.what == sw::Line::What::More) {
            r.more = true;
            r.opens = f.group;
            r.key = "more." + std::to_string(f.group);
            r.label = f.more > 0 ? settings::fmt(settings::tr(lang, settings::Text::MoreRowsShow),
                                                 {std::to_string(f.more)})
                                 : std::string(settings::tr(lang, settings::Text::MoreRowsHide));
        } else if (f.kind == sw::Kind::Game) {
            const int i = f.key.back() - '0';
            r.label = t.toggles[i];
            r.toggle = true, r.on = s.values.on[i];
            if (i < 2) r.slider = true, r.min = 0, r.max = oc::kVolumeMost, r.value = s.values.volume[i];
            if (i < 2) r.vtext = std::to_string(s.values.volume[i]);
            if (i == 2) r.hint = t.snapHint;
        } else if (f.kind == sw::Kind::Picture) {
            const auto c = settings::presets::Choice(f.key.back() - '0');
            r.label = say(settings::presets::headOf(c));
            const int now = s.config ? settings::presets::match(c, shown).variant : -1;
            for (int v = 0; v < settings::presets::variants(c); ++v)
                r.choices.push_back(ChoiceView{say(settings::presets::nameOf(c, v)), v == now});
        } else if (const settings::IniSetting* st = settings::findSetting(f.key)) {
            r.label = say(st->label);
            // a row, not a page: its hint's first sentence (the whole of it stays the settings app's)
            r.hint = consoletiles::gist(settings::tr(lang, st->hint), 130);
            // 3.0.21-beta.4: what keeps it from doing anything now (Direct3D 12, the 3D view, a row it needs)
            if (const settings::catalogue::Row* cat = settings::catalogue::find(f.key))
                r.warn = settings::catalogue::whyNot(*cat, shown, settingedit::direct3d12(), view3dwin::active(),
                                                     lang);
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
        k += r.key + (r.on ? "+" : "-") + std::to_string(r.value) + r.group + (r.sub ? "s" : "") + r.warn;
        if (r.more) k += r.label;  // «Ще N» or the folding line
        for (const ChoiceView& c : r.choices) k += c.on ? '1' : '0';
        k += '|';
    }
    return k;
}

// A row's control used: the game's own through its window, ours into uaro-graphics.ini (read again at the next
// present: `changed`)
static void useRow(State& s, int row, int what, int value) {
    if (row < 0 || size_t(row) >= s.m.rows.size()) return;
    const std::string key = s.m.rows[size_t(row)].key;
    if (s.m.rows[size_t(row)].more) {  // 3.0.21-beta.3: a group's details opened or folded
        const int group = s.m.rows[size_t(row)].opens;
        if (what == 3 && !s.open.erase(group)) s.open.insert(group);
        s.rowsKey.clear();
        return;
    }
    if (key.rfind("game.", 0) == 0) {
        const int i = key.back() - '0';
        if (what == 0) toggle(s, i);
        else if (what == 1) setVolume(s, i, value);
        s.hotUntil = GetTickCount64() + 700;
        return;
    }
    if (!s.config || s.iniPath.empty()) return;
    bool wrote = false;
    if (key.rfind("picture.", 0) == 0) {
        // 3.0.18-beta.132 (R-4BXZPM «пропал новый интерфейс на настройках»): «Новий екран, вікна гри» or
        // «Класичний» chosen here turned this very window off. 3.0.21-beta.7: no look holds its switch any more
        if (what == 2) wrote = settingedit::setLook(s.iniPath, settings::presets::Choice(key.back() - '0'), value);
    } else if (const settings::IniSetting* st = settings::findSetting(key)) {
        if (what == 0 && st->tokens.size() > 1)
            wrote = settingedit::set(s.iniPath, *s.config, key, st->tokens[s.m.rows[size_t(row)].on ? 0 : 1]);
        else if (what == 1)
            wrote = settingedit::set(s.iniPath, *s.config, key, std::to_string(std::clamp(value, st->min, st->max)));
        else if (what == 2 && value >= 0 && size_t(value) < st->tokens.size())
            wrote = settingedit::set(s.iniPath, *s.config, key, st->tokens[size_t(value)]);
    }
    s.changed = s.changed || wrote;
    s.hotUntil = GetTickCount64() + 700;  // the row's new value shown as soon as the driver has read the file
}

// The live window at the middle of the screen, as wide as its markup makes it
static void placeLive(State& s) {
    int x = s.layout.x, y = s.layout.y;
    int bx = 0, by = 0, bw = 0, bh = 0;
    if (s.live.doc.box(bx, by, bw, bh) && bw > 0) {
        x = std::max(0, (s.targetW - bw) / 2);
        y = std::max(0, (s.targetH - bh) / 3);
    }
    s.live.frame(s.shown, x, y, int(winstack::Win::Options), ".ctl");
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("options", uikit::options::rml(), [&](Rml::DataModelConstructor& m) {
        uikit::options::bind(m, s.m);
        m.BindEventCallback("sec_go", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (a.empty()) return;
            st.m.sec = a[0].Get<int>();
            st.m.query.clear();
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
            if (i < 0 || size_t(i) >= st.m.rows.size()) return;
            const SetRowView& r = st.m.rows[size_t(i)];
            const int step = r.key.rfind("game.", 0) == 0 ? kPadStep : std::max(1, r.step);
            useRow(st, i, 1, std::clamp(r.value + by * step, r.min, r.max));
        });
        m.BindEventCallback("set_choice", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (a.size() >= 2) useRow(state(), a[0].Get<int>(), 2, a[1].Get<int>());
        });
        m.BindEventCallback("set_more", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) useRow(state(), a[0].Get<int>(), 3, 0);
        });
        // 3.0.21-beta.7: «Розмістити інтерфейс» - the console's action before (the console is the laboratory now)
        m.BindEventCallback("open_layout", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            state().layoutAsked = true;
            closeIt(state());
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
    if (title != s.m.title || ground != s.m.ground) {
        s.m.title = title, s.m.hint = say(settings::Text::SettingsWinHint), s.m.ground = ground;
        s.m.searchHint = say(settings::Text::SearchCue);
        s.m.none = say(settings::Text::SettingsWinNone);
        s.m.consoleBtn = say(settings::Text::SettingsWinConsole);
        s.m.layoutBtn = say(settings::Text::MenuLayout);  // 3.0.21-beta.7: «Розмістити інтерфейс»
        std::vector<NavView> nav;  // 3.0.21-beta.3: the catalogue's areas, each with its rows' count
        for (const settingswin::Nav& n : settingswin::nav()) nav.push_back(NavView{say(n.name), n.rows});
        s.m.nav = std::move(nav);
        s.shownQuery = "\x01";  // the counts and the rows again
        s.live.doc.dirtyAll();
    }
    if (s.m.query != s.shownQuery || s.m.sec != s.shownSec) {
        if (s.m.query != s.shownQuery) {  // beside an entry: its rows, or what the search found in it
            const std::vector<settingswin::Nav>& list = settingswin::nav();
            for (size_t i = 0; i < s.m.nav.size() && i < list.size(); ++i)
                s.m.nav[i].count = s.m.query.empty() ? list[i].rows : 0;
            if (!s.m.query.empty())
                for (const settingswin::Line& f : settingswin::find(s.m.query, language()))
                    if (f.entry >= 0 && size_t(f.entry) < s.m.nav.size()) ++s.m.nav[size_t(f.entry)].count;
            s.live.doc.dirty("nav");
        }
        s.shownQuery = s.m.query;
        s.shownSec = s.m.sec;
        s.rowsKey.clear();
        if (Rml::Element* pane = s.live.doc.byId("spane")) pane->SetScrollTop(0);  // another entry: from its top
    }
    // 3.0.21-beta.3: an entry holds up to two hundred rows now - they are built anew each frame only for a moment
    // after something is done in the window, else seven times a second (a change from outside - the console, the
    // game's own keys - shows that much later)
    const ULONGLONG now = GetTickCount64();
    if (s.rowsKey.empty()) s.hotUntil = now + 700;
    if (!s.rowsKey.empty() && now >= s.hotUntil && now - s.builtAt < 150) {
        placeLive(s);
        return;
    }
    s.builtAt = now;
    std::vector<SetRowView> rows = buildRows(s);
    // 3.0.21-beta.8 (R-SPPSF3): the list as long as the entry - no spare rows kept from a longer one (uikit
    // options_live fit(): how 3.0.18-beta.132's padding became needless)
    uikit::options::fit(rows);
    const std::string key = rowsKeyOf(rows) + std::to_string(s.m.sec);
    if (key != s.rowsKey) {
        s.rowsKey = key;
        s.m.shownRows = int(rows.size());
        s.m.rows = std::move(rows);
        s.live.doc.dirty("srows");
        s.live.doc.dirty("shown_rows");
    }
    placeLive(s);
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

bool takeLayoutAsk() {
    State& s = state();
    const bool asked = s.layoutAsked;
    s.layoutAsked = false;
    return asked;
}

bool available() {
    const State& s = state();
    return s.on && inGameWorld();
}

bool toggleOpen(const char* why) {
    State& s = state();
    if (!available()) return false;
    if (gameOptions()) {
        closeIt(s);
    } else {  // the game's own window made (its Alt+O): the new look shows it
        using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
        reinterpret_cast<MakeFn>(uintptr_t(kMakeWindow))(kWindowMgr, kOptionsWindow);
    }
    LOG_INFO("options: «Налаштування» %s (%s)", gameOptions() ? "opened" : "closed", why);
    return true;
}

bool open(const char* why) {
    if (!available()) return false;
    if (gameOptions()) return true;  // open already: left so
    return toggleOpen(why);
}

}  // namespace uagfx::win::optionscardwin

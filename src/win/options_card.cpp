#include "win/options_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include "core/checks_now.hpp"
#include "core/console_tiles.hpp"
#include "core/settings/catalogue.hpp"
#include "core/window_stack.hpp"
#include "core/options_card.hpp"
#include "core/settings/strings.hpp"
#include "core/settings/schema.hpp"
#include "core/settings_window.hpp"
#include "core/ui_style.hpp"
#include "core/version.hpp"
#include "pad/win/game_window.hpp"
#include "uikit/applying.hpp"
#include "uikit/options_live.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
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
#include "win/context.hpp"
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
    Rml::String shownQuery;
    ULONGLONG builtAt = 0, hotUntil = 0;  // when the rows were built last; built each frame until then
    std::string rowsKey;
    const Config* config = nullptr;
    std::string iniPath;
    bool changed = false;
    bool layoutAsked = false;  // 3.0.21-beta.7: «Розмістити інтерфейс» pressed
    int quietTold = 0;         // 3.0.21-beta.9: sliders' changes not taken for moves, told in the log
    // 3.0.21-beta.10 (the owner: «надо кнопку по умолчанию сделать. чтобы не искать»): the settings as shipped for
    // this card's level (core settingswin defaults), the rows of a group on their way back - written one a config
    // read, each by the settings as they stand then (an effect's switch may turn a set into the player's own)
    Config defaults;
    int defaultsLevel = -1;
    std::vector<settingswin::Reset> backQueue;
    std::string backKey, backToken;
    int backWait = 0;
    std::string shownNote;
    ULONGLONG countAt = 0;  // when «Змінені» is counted next
    int warnTold = 0;
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
    int scrollGroup = -1;  // 3.0.24-beta.2: «Детально» - the group the full mode scrolls to once laid out
    int scrollTries = 0;
    // 3.0.24-beta.3: «Перевіряємо зараз» - the checks marked done ([utility] checks_done), read once a second; the
    // open ones' count (0: no such entry - not a beta build, or nothing to check): the list's first entry then
    std::string checksDone;
    ULONGLONG checksAt = 0;
    int checks = 0;
    ULONGLONG resetArmedUntil = 0;  // 3.0.24-beta.5: «Усе за замовчуванням» pressed once - the second press until then
};

// 3.0.24-beta.3: the list's entries are the catalogue's (core settingswin nav), «Перевіряємо зараз» before them while
// there are checks - the entry shown as settingswin knows it (-1: the checks)
int navOff(const State& s) { return s.checks > 0 ? 1 : 0; }
int navEntry(const State& s) { return s.m.sec - navOff(s); }

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
// name over its first line, its rows, its details under a small heading - or every entry's rows the search finds, each
// group named «entry · group»
static std::vector<SetRowView> buildRows(const State& s) {
    namespace sw = uagfx::settingswin;
    const settings::Lang lang = language();
    const bool searching = !s.m.query.empty();
    const std::vector<sw::Nav>& nav = sw::nav();
    const oc::Texts t = texts();
    const Config shown = s.config ? settingedit::shown(*s.config) : Config{};
    // 3.0.21-beta.10: «Змінені» - the rows off their defaults, under their groups' names as the search's
    const int entry = navEntry(s);
    const bool offEntry = !searching && entry >= 0 && size_t(entry) < nav.size() &&
                          nav[size_t(entry)].area == sw::kChangedArea;
    // 3.0.24-beta.2: the simple page (the concept https://uaro.kiev.ua/docs/settings-simple/) - no list, its lines
    const bool simple = s.m.simple && !searching;
    const bool checksEntry = !searching && !simple && entry < 0 && s.checks > 0;
    const std::vector<sw::Line> list = searching     ? sw::find(s.m.query, lang)
                                       : simple      ? sw::simpleLines()
                                       : checksEntry ? std::vector<sw::Line>{}
                                       : offEntry    ? sw::changedLines(shown, s.defaults)
                                                     : sw::lines(entry);
    // where the game is now: what a row's reasons for doing nothing are judged by (the renderer, the view, the map)
    settings::catalogue::Now place;
    place.direct3d12 = settingedit::direct3d12();
    place.view3d = view3dwin::active();
    place.map = readGameMap();
    place.ownEntry = !searching && !offEntry;
    std::vector<SetRowView> out;
    // 3.0.24-beta.3: «Перевіряємо зараз» - the simple page's first section, the full mode's first entry
    if ((simple || checksEntry) && s.checks > 0)
        for (const checksnow::Check* c : checksnow::open(s.checksDone)) {
            uikit::options::CardView v;
            v.id = c->id;
            v.check = true;
            v.label = checksnow::words(c->title, lang);
            v.desc = std::string(checksnow::words(c->steps, lang)) + "  \xC2\xB7  " +
                     settings::fmt(say(settings::Text::ChecksSince), {c->since});
            v.ok = say(settings::Text::ChecksOk);
            v.bad = say(settings::Text::ChecksBad);
            v.key = c->key;
            const settings::IniSetting* st = *c->key ? settings::findSetting(c->key) : nullptr;
            v.toggle = st && st->tokens.size() > 1;
            v.on = v.toggle && s.config && settings::tokenOf(*st, shown) == st->tokens[1];
            if (out.empty() || out.back().ncards != 1) {
                SetRowView r;
                r.key = std::string("checks.") + c->id;
                if (out.empty()) r.group = say(settings::Text::ChecksNow);
                out.push_back(std::move(r));
            }
            out.back().cards.push_back(std::move(v));
            out.back().ncards = int(out.back().cards.size());
        }
    const size_t checkRows = out.size();
    for (const sw::Line& f : list) {
        if (f.card) {  // 3.0.24-beta.2: a capability card - two side by side, a section's first starts a line
            uikit::options::CardView c;
            c.key = f.key;
            const settings::IniSetting* st = settings::findSetting(f.key);
            c.label = st ? say(st->label) : f.key;
            c.desc = say(f.desc);
            const std::string now = s.config && st ? settings::tokenOf(*st, shown) : "";
            c.on = st && st->tokens.size() > 1 && now == st->tokens[1];
            if (f.inside > 1) {
                c.more = settings::fmt(say(settings::Text::SimpleMore), {std::to_string(f.inside)});
                c.entry = f.entry, c.group = f.opens;
            }
            if (f.heads || out.size() == checkRows || out.back().ncards != 1) {
                SetRowView r;
                r.key = "cards." + f.key;
                if (f.heads) r.group = say(f.groupName);
                out.push_back(std::move(r));
            }
            out.back().cards.push_back(std::move(c));
            out.back().ncards = int(out.back().cards.size());
            continue;
        }
        SetRowView r;
        r.key = f.key;
        r.sub = f.part;
        if (f.heads) {
            r.group = say(f.groupName);
            if ((searching || offEntry) && !simple && f.entry > 0 && size_t(f.entry) < nav.size())
                r.group = say(nav[size_t(f.entry)].name) + " \xC2\xB7 " + r.group;
            // the group's way back to the shipped values, while it has rows off them
            if (s.config && !simple && f.group >= 0 && !sw::resetsOf(f.group, shown, s.defaults).empty())
                r.reset = say(settings::Text::SettingsDefaults);
            r.groupIndex = f.group;
        }
        r.det = f.detail;
        if (f.what == sw::Line::What::Sub) {
            // 3.0.21-beta.11 (the owner: «раскрытие в настройках лучше убрать … можно както выделить внутри что это
            // подгрупа»): the small heading over a group's details - they stand at once, nothing opens or folds
            r.subhead = true;
            r.key = "sub." + std::to_string(f.group);
            r.label = say(settings::Text::SettingsMoreHead);
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
            // 3.0.21-beta.4: what keeps it from doing anything now (Direct3D 12, the 3D view, a row it needs);
            // 3.0.21-beta.10: the map that has nothing of it, Direct3D 11's own
            if (const settings::catalogue::Row* cat = settings::catalogue::find(f.key))
                r.warn = settings::catalogue::whyNot(*cat, shown, place, lang);
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
        k += r.key + (r.on ? "+" : "-") + std::to_string(r.value) + r.group + (r.sub ? "s" : "") + r.warn + r.reset;
        if (r.subhead) k += r.label;
        if (r.det) k += 'd';
        for (const ChoiceView& c : r.choices) k += c.on ? '1' : '0';
        for (const uikit::options::CardView& c : r.cards) k += c.key + (c.on ? "+" : "-");  // 3.0.24-beta.2
        k += '|';
    }
    return k;
}

// A row's control used: the game's own through its window, ours into uaro-graphics.ini (read again at the next
// present: `changed`)
static void useRow(State& s, int row, int what, int value) {
    if (row < 0 || size_t(row) >= s.m.rows.size()) return;
    const std::string key = s.m.rows[size_t(row)].key;
    if (s.m.rows[size_t(row)].subhead) return;  // a heading: no control
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
    // 3.0.21-beta.10 (the owner: «иногда когда чтото в настройках выбираю то ничего не меняется»): the facts for a
    // report - a row changed while it says itself that a change does not show now
    if (wrote && !s.m.rows[size_t(row)].warn.empty() && s.warnTold++ < 40)
        LOG_INFO("options: «%s» changed while its row says: %s", key.c_str(), s.m.rows[size_t(row)].warn.c_str());
}

// 3.0.24-beta.3: the card an event names (its row, its place in the row), none
static const uikit::options::CardView* cardAt(const State& s, const Rml::VariantList& a) {
    if (a.size() < 2) return nullptr;
    const int i = a[0].Get<int>(), j = a[1].Get<int>();
    if (i < 0 || size_t(i) >= s.m.rows.size() || j < 0 || size_t(j) >= s.m.rows[size_t(i)].cards.size()) return nullptr;
    return &s.m.rows[size_t(i)].cards[size_t(j)];
}

// 3.0.24-beta.3: the list on the left - «Перевіряємо зараз» first while there are checks, then the catalogue's areas
static void buildNav(State& s) {
    std::vector<NavView> nav;
    if (s.checks > 0) nav.push_back(NavView{say(settings::Text::ChecksNow), s.checks});
    for (const settingswin::Nav& n : settingswin::nav()) nav.push_back(NavView{say(n.name), n.rows});
    s.m.nav = std::move(nav);
}

// 3.0.24-beta.3: the checks marked done read again (once a second, at once after «Працює ✓»); the list on the left
// made again when their count changes - the entry shown stays the same entry
static void readChecks(State& s, ULONGLONG now) {
    if (now < s.checksAt) return;
    s.checksAt = now + 1000;
    if (!checksnow::shown(kVersion) || s.iniPath.empty()) return;
    std::ifstream in(s.iniPath, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    const auto ini = settings::readIni(text.str());
    const auto it = ini.find("utility.checks_done");
    s.checksDone = it == ini.end() ? "" : it->second;
    const int open = int(checksnow::open(s.checksDone).size());
    if (open == s.checks) return;
    const int was = navOff(s);
    s.checks = open;
    s.m.sec = std::max(0, s.m.sec - was + navOff(s));
    buildNav(s);
    s.shownQuery = "\x01";  // the counts and the rows again
    s.live.doc.dirty("nav");
    s.live.doc.dirty("sec");
}

// 3.0.24-beta.2: the simple page or the full window, kept in [interface] settings_full
static void setMode(State& s, bool simple) {
    if (s.config && !s.iniPath.empty())
        s.changed = settingedit::set(s.iniPath, *s.config, "interface.settings_full", simple ? "0" : "1") || s.changed;
    if (s.m.simple != simple) LOG_INFO("options: «%s»", simple ? "Просто" : "Повністю");
    s.m.simple = simple;
    s.m.query.clear();
    s.live.doc.dirty("simple");
    s.live.doc.dirty("query");
    s.rowsKey.clear();
    s.hotUntil = GetTickCount64() + 700;
}

// The live window at the middle of the screen, as wide as its markup makes it
static void placeLive(State& s, bool anew = false) {
    int x = s.layout.x, y = s.layout.y;
    int bx = 0, by = 0, bw = 0, bh = 0;
    if (s.live.doc.box(bx, by, bw, bh) && bw > 0) {
        x = std::max(0, (s.targetW - bw) / 2);
        y = std::max(0, (s.targetH - bh) / 3);
    }
    s.live.frame(s.shown, x, y, int(winstack::Win::Options), ".ctl");
    // 3.0.21-beta.11: every entry is long now (no folding) - the rows far from the pane's view are not drawn;
    // `anew`: the rows were told again this frame - all shown until they are laid out
    if (s.shown) uikit::options::cull(s.live.doc.doc(), anew);
}

// 3.0.21-beta.10: the rows a group's «Типові» sends back - one written, then the next once the driver has read
// the file with it (or a second on): each is written by the settings as they stand then
static void backStep(State& s) {
    if (s.backQueue.empty() || !s.config || s.iniPath.empty()) return;
    if (s.backWait > 0) {
        const settings::IniSetting* st = settings::findSetting(s.backKey);
        const bool taken = !st || settings::tokenOf(*st, settingedit::shown(*s.config)) == s.backToken;
        if (!taken && --s.backWait > 0) return;
        s.backWait = 0;
    }
    while (!s.backQueue.empty()) {
        const settingswin::Reset back = s.backQueue.front();
        s.backQueue.erase(s.backQueue.begin());
        if (!settingedit::set(s.iniPath, *s.config, back.key, back.token)) continue;  // so already
        s.changed = true;
        s.backKey = back.key, s.backToken = back.token;
        s.backWait = 120;  // frames at most for the driver to read the file again
        s.hotUntil = GetTickCount64() + 700;
        return;
    }
}

// 3.0.24-beta.2: «Детально · N ›» - once the full mode's rows are laid out, the pane scrolled to the card's group's
// name (a few frames at most; the group not shown - nothing)
static void scrollToGroup(State& s) {
    if (s.scrollGroup < 0) return;
    if (s.m.simple || ++s.scrollTries > 30) {
        s.scrollGroup = -1;
        return;
    }
    int row = -1;
    for (size_t i = 0; i < s.m.rows.size(); ++i)
        if (!s.m.rows[i].group.empty() && s.m.rows[i].groupIndex == s.scrollGroup) row = int(i);
    Rml::Element* pane = s.live.doc.byId("spane");
    if (row < 0 || !pane) return;
    int seen = 0;
    for (int c = 0; c < pane->GetNumChildren(); ++c) {
        Rml::Element* e = pane->GetChild(c);
        if (!e->IsClassSet("srow") || seen++ != row) continue;
        for (int k = 0; k < e->GetNumChildren(); ++k) {
            Rml::Element* line = e->GetChild(k);
            if (line->GetOffsetHeight() <= 0) continue;
            pane->SetScrollTop(line->GetAbsoluteTop() - pane->GetAbsoluteTop() + pane->GetScrollTop() - 4.0f);
            LOG_INFO("options: «Детально» - the full mode at the group %d", s.scrollGroup);
            s.scrollGroup = -1;
            return;
        }
        return;  // not laid out yet: the next frame
    }
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
        // 3.0.21-beta.9 (R-MSAM96, R-SPPSF3: sliders written that no hand had moved - «Яскравість» 50, «Масштаб
        // рендера» 50): only a person's move counts (uikit options_live sliderMoved) - not the «change» RmlUi tells
        // while a row's own values reach a slider left by the row that stood there before
        m.BindEventCallback("set_slider", [](Rml::DataModelHandle, Rml::Event& e, const Rml::VariantList& a) {
            State& st = state();
            if (a.empty()) return;
            const int row = a[0].Get<int>();
            int value = 0;
            if (uikit::options::sliderMoved(e, st.m, row, value)) {
                useRow(st, row, 1, value);
            } else if (uikit::applying() && row >= 0 && size_t(row) < st.m.rows.size()) {
                // the facts for a report: what would have been written before
                const SetRowView& r = st.m.rows[size_t(row)];
                const int told = int(std::lround(e.GetParameter<float>("value", 0.0f)));
                if (told != r.value && st.quietTold++ < 20)
                    LOG_INFO("options: «%s»'s slider told %d as the rows changed (its row holds %d) - not a move, "
                             "not written", r.key.c_str(), told, r.value);
            }
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
        // 3.0.21-beta.10: «Типові» of a group - its rows off their defaults go back, one a config read (backStep)
        m.BindEventCallback("set_reset", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (a.empty() || !st.config) return;
            const int row = a[0].Get<int>();
            if (row < 0 || size_t(row) >= st.m.rows.size() || st.m.rows[size_t(row)].reset.empty()) return;
            const int group = st.m.rows[size_t(row)].groupIndex;
            const std::vector<settingswin::Reset> back =
                settingswin::resetsOf(group, settingedit::shown(*st.config), st.defaults);
            LOG_INFO("options: «Типові» of the group %d - %d row(s) go back to their defaults", group,
                     int(back.size()));
            st.backQueue.insert(st.backQueue.end(), back.begin(), back.end());
        });
        m.BindEventCallback("set_choice", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (a.size() >= 2) useRow(state(), a[0].Get<int>(), 2, a[1].Get<int>());
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
        // 3.0.24-beta.2: «Просто / Повністю» - kept in [interface] settings_full
        m.BindEventCallback("set_mode", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) setMode(state(), a[0].Get<int>() == 0);
        });
        m.BindEventCallback("card_toggle", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (a.size() < 2 || !st.config || st.iniPath.empty()) return;
            const int i = a[0].Get<int>(), j = a[1].Get<int>();
            if (i < 0 || size_t(i) >= st.m.rows.size() || j < 0 || size_t(j) >= st.m.rows[size_t(i)].cards.size())
                return;
            const uikit::options::CardView& c = st.m.rows[size_t(i)].cards[size_t(j)];
            const settings::IniSetting* set = settings::findSetting(c.key);
            if (!set || set->tokens.size() < 2) return;
            const bool wrote = settingedit::set(st.iniPath, *st.config, c.key, set->tokens[c.on ? 0 : 1]);
            st.changed = st.changed || wrote;
            st.hotUntil = GetTickCount64() + 700;
            if (st.told++ < 40) LOG_INFO("options: the simple page's card «%s» %s", c.key.c_str(), c.on ? "off" : "on");
        });
        // 3.0.24-beta.3: «Працює ✓» - the check marked done here ([utility] checks_done) and the mark sent to the owner
        // (the settings app, a report of the reason «check» with the logs); «Є проблема — звіт» - a report as the
        // menu's, its words beginning with the check's name
        m.BindEventCallback("check_ok", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            const uikit::options::CardView* c = cardAt(st, a);
            if (!c || st.iniPath.empty()) return;
            for (const checksnow::Check& k : checksnow::all()) {
                if (c->id != k.id) continue;
                st.checksDone = checksnow::markDone(st.checksDone, k.id);
                editIniFile(st.iniPath, {{"utility", "checks_done", st.checksDone}});
                const std::string dir = st.iniPath.substr(0, st.iniPath.find_last_of("\\/") + 1);
                const std::string file = dir + "check-" + k.id + ".txt";
                std::ofstream(file, std::ios::binary) << checksnow::workedNote(k, kVersion);
                startCheckReport(file);
                LOG_INFO("checks: «%s» works - marked, sent", k.id);
                st.checksAt = 0;  // the list again at once
                st.rowsKey.clear();
                break;
            }
        });
        m.BindEventCallback("check_bad", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            const uikit::options::CardView* c = cardAt(st, a);
            if (!c) return;
            for (const checksnow::Check& k : checksnow::all()) {
                if (c->id != k.id) continue;
                LOG_INFO("checks: «%s» has a problem - a report", k.id);
                Context::get().askReport(checksnow::problemNote(k, language()));
                closeIt(st);
                break;
            }
        });
        // 3.0.24-beta.5 (the owner: «нету кнопки с альт-о сбросить настройки в по умолчанию»): every row back to its
        // default - a second press within 4 s (the first asks «Ще раз — скинути все»), one row a config read as
        // «Типові» does (backStep)
        m.BindEventCallback("reset_all", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            State& st = state();
            if (!st.config) return;
            const ULONGLONG now = GetTickCount64();
            if (now >= st.resetArmedUntil) {
                st.resetArmedUntil = now + 4000;
                st.m.resetBtn = say(settings::Text::SettingsResetAllSure);
                st.live.doc.dirty("reset_btn");
                return;
            }
            st.resetArmedUntil = 0;
            const std::vector<settingswin::Reset> back =
                settingswin::resetsAll(settingedit::shown(*st.config), st.defaults);
            LOG_INFO("options: «Усе за замовчуванням» - %d row(s) go back to their defaults", int(back.size()));
            st.backQueue.insert(st.backQueue.end(), back.begin(), back.end());
            st.m.resetBtn = say(settings::Text::SettingsResetAll);
            st.live.doc.dirty("reset_btn");
        });
        // «Детально · N ›»: the full mode at the card's group
        m.BindEventCallback("card_more", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (a.size() < 2) return;
            const int i = a[0].Get<int>(), j = a[1].Get<int>();
            if (i < 0 || size_t(i) >= st.m.rows.size() || j < 0 || size_t(j) >= st.m.rows[size_t(i)].cards.size())
                return;
            const uikit::options::CardView c = st.m.rows[size_t(i)].cards[size_t(j)];
            if (c.entry < 0) return;
            setMode(st, false);
            st.m.sec = c.entry + navOff(st);
            st.scrollGroup = c.group, st.scrollTries = 0;
            st.live.doc.dirty("sec");
        });
        m.BindEventCallback("opt_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            closeIt(state());
        });
    });
    if (!s.live.made()) return;
    backStep(s);  // 3.0.21-beta.10: a group on its way back goes on though the window is closed meanwhile
    if (!s.shown) {  // 3.0.18-beta.170 (R-CD8KCP: 0.14 ms a frame while closed): nothing built for a hidden card
        s.live.frame(false, s.layout.x, s.layout.y, int(winstack::Win::Options), ".ctl");
        return;
    }
    if (const int level = Context::get().cardLevel(); level != s.defaultsLevel) {
        s.defaultsLevel = level;
        s.defaults = settingswin::defaults(level);
        s.rowsKey.clear();
    }
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const Rml::String title = say(settings::Text::MenuSettings);
    if (title != s.m.title || ground != s.m.ground) {
        s.m.title = title, s.m.hint = say(settings::Text::SettingsWinHint), s.m.ground = ground;
        s.m.searchHint = say(settings::Text::SearchCue);
        s.m.none = say(settings::Text::SettingsWinNone);
        s.m.consoleBtn = say(settings::Text::SettingsWinConsole);
        s.m.layoutBtn = say(settings::Text::MenuLayout);  // 3.0.21-beta.7: «Розмістити інтерфейс»
        s.m.modeSimple = say(settings::Text::SimpleModeSimple);  // 3.0.24-beta.2
        s.m.resetBtn = say(settings::Text::SettingsResetAll);    // 3.0.24-beta.5
        s.m.modeFull = say(settings::Text::SimpleModeFull);
        buildNav(s);  // 3.0.21-beta.3: the catalogue's areas, each with its rows' count; .24-beta.3 the checks first
        s.shownQuery = "\x01";  // the counts and the rows again
        s.live.doc.dirtyAll();
    }
    readChecks(s, GetTickCount64());  // 3.0.24-beta.3
    if (s.resetArmedUntil && GetTickCount64() >= s.resetArmedUntil) {  // 3.0.24-beta.5: the second press not come
        s.resetArmedUntil = 0;
        s.m.resetBtn = say(settings::Text::SettingsResetAll);
        s.live.doc.dirty("reset_btn");
    }
    if (s.m.query != s.shownQuery || s.m.sec != s.shownSec) {
        if (s.m.query != s.shownQuery) {  // beside an entry: its rows, or what the search found in it
            const std::vector<settingswin::Nav>& list = settingswin::nav();
            const size_t off = size_t(navOff(s));  // 3.0.24-beta.3: «Перевіряємо зараз» first
            if (off && !s.m.nav.empty()) s.m.nav[0].count = s.m.query.empty() ? s.checks : 0;
            for (size_t i = 0; i + off < s.m.nav.size() && i < list.size(); ++i)
                s.m.nav[i + off].count = s.m.query.empty() ? list[i].rows : 0;
            if (!s.m.query.empty())
                for (const settingswin::Line& f : settingswin::find(s.m.query, language()))
                    if (f.entry >= 0 && size_t(f.entry) + off < s.m.nav.size()) ++s.m.nav[size_t(f.entry) + off].count;
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
    {  // 3.0.21-beta.10: what the entry says over its rows; «Змінені»'s count and its words when nothing is off
        const std::vector<settingswin::Nav>& list = settingswin::nav();
        const bool searching = !s.m.query.empty();
        const int entry = navEntry(s);  // 3.0.24-beta.3: as settingswin knows it (-1 the checks)
        const settings::Text note = searching || entry < 0 ? settings::Text::Count
                                                           : settingswin::noteOf(entry, view3dwin::active());
        const std::string words = note == settings::Text::Count ? std::string() : say(note);
        if (words != s.shownNote) {
            s.shownNote = words;
            s.m.note = words;
            s.live.doc.dirty("note");
        }
        const bool off = !searching && entry >= 0 && size_t(entry) < list.size() &&
                         list[size_t(entry)].area == settingswin::kChangedArea;
        const Rml::String none = say(off ? settings::Text::SettingsChangedNone : settings::Text::SettingsWinNone);
        if (none != s.m.none) {
            s.m.none = none;
            s.live.doc.dirty("none");
        }
        // counted at once after something is done in the window, else once a second (every row's value is read)
        if (s.config && !searching && (now < s.hotUntil || now >= s.countAt)) {
            s.countAt = now + 1000;
            const size_t navOffset = size_t(navOff(s));
            for (size_t i = 0; i + navOffset < s.m.nav.size() && i < list.size(); ++i) {
                if (list[i].area != settingswin::kChangedArea) continue;
                const int count = int(settingswin::changedLines(settingedit::shown(*s.config), s.defaults).size());
                if (count != s.m.nav[i + navOffset].count) {
                    s.m.nav[i + navOffset].count = count;
                    s.live.doc.dirty("nav");
                }
            }
        }
    }
    // 3.0.24-beta.2: the mode as the file says it (another place may change it) - not while a click of ours waits
    if (s.config && now >= s.hotUntil) {
        const bool simple = !settingedit::shown(*s.config).settingsFull;
        if (simple != s.m.simple) {
            s.m.simple = simple;
            s.live.doc.dirty("simple");
        }
    }
    {
        const Rml::String hint = say(s.m.simple && s.m.query.empty() ? settings::Text::SimpleFoot
                                                                      : settings::Text::SettingsWinHint);
        if (hint != s.m.hint) {
            s.m.hint = hint;
            s.live.doc.dirty("hint");
        }
    }
    std::vector<SetRowView> rows = buildRows(s);
    // 3.0.21-beta.8 (R-SPPSF3): the list as long as the entry - no spare rows kept from a longer one (uikit
    // options_live fit(): how 3.0.18-beta.132's padding became needless)
    uikit::options::fit(rows);
    const std::string key = rowsKeyOf(rows) + std::to_string(s.m.sec) + (s.m.simple ? "S" : "F");
    const bool anew = key != s.rowsKey;
    if (anew) {
        s.rowsKey = key;
        s.m.shownRows = int(rows.size());
        s.m.rows = std::move(rows);
        s.live.doc.dirty("srows");
        s.live.doc.dirty("shown_rows");
    }
    placeLive(s, anew);
    scrollToGroup(s);
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

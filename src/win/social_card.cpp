#include "win/social_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/settings/strings.hpp"
#include "core/social_card.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/card_drag.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::socialcardwin {
namespace {

namespace sc = uagfx::socialcard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the friends and party window (where the manager keeps it, its class's vtable, its SendMsg), a window's
// shown flag, its tab, its list (a node: next +0, valid +8, the name's words +0x18 / +0x1C, the map's +0x28 / +0x2C,
// the role +0x34 - 0 the leader -, offline +0x38) and the chosen row; the party's name the game keeps (a char*)
constexpr uint32_t kWindowMgr = 0x727ED8, kSocialAt = kWindowMgr + 0x11C, kSocialClass = 0x69BBB4;
constexpr uint32_t kSocialSend = 0x4C2250, kShown = 0x38, kSendMsgSlot = 0x94, kTab = 0xC0, kList = 0xA4;
constexpr uint32_t kChosen = 0xAC, kPartyName = 0x772D88;
constexpr int kButtonMsg = 6, kMostFriends = 40;
constexpr size_t kMostNodes = 100;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

// 3.0.18-beta.87 (live windows, group 2): friends and the party a live window - tabs, rows and buttons by flexbox, RmlUi's clicks
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw socw" data-style-background-color="ground">
<div class="tabs"><span class="tab ctl" data-for="t, i : tabs" data-class-on="i == tab" data-event-click="so_tab(i)">{{t}}</span></div>
<span class="x ctl" data-event-click="so_close()">&#x2715;</span>
<div class="lbl">{{header}}</div>
<div class="list"><div class="row ctl" data-for="e, i : entries" data-class-on="i == selected" data-class-off="!e.online"
  data-event-click="so_choose(i)" data-event-dblclick="so_act(0)"><span class="name">{{e.name}}</span><span class="crown"
  data-if="e.leader"> ★</span><span class="where">{{e.online ? e.map : offline}}</span></div>
<div class="hint" data-if="entries.size == 0">{{empty}}</div></div>
<div class="btns grid4"><div class="btn ctl" data-for="a, i : actions" data-event-click="so_act(i)">{{a}}</div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
struct SocialEntryView {
    Rml::String name, map;
    bool online = false, leader = false;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<SocialEntryView> entryViews;
    std::vector<Rml::String> tabViews, actionViews;
    Rml::String header, offline, empty, ground = "#10141cf0";
    int tabNo = 0, selected = -1;
    uisheet::Sheet sheet;  // 3.0.18-beta.34: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    CardDrag drag;  // 3.0.11-beta.270
    bool on = false, shown = false;
    uint32_t hidden = 0;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<sc::Entry> entries;
    std::string partyName;
    sc::Show show;
    sc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t gameSocial() {
    const uint32_t w = read32(kSocialAt);
    return w && read32(w) == kSocialClass && read32(read32(w) + kSendMsgSlot) == kSocialSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("social: the game's friends and party window hidden for the new card (its shown flag 0)");
            s.hidden = window;
        }
    } else if (window && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

std::string words(uint32_t ptr, uint32_t len) {
    if (!ptr || !len || len > 64) return "";
    char buf[65] = {};
    return readGame(ptr, buf, len) ? std::string(buf, len) : "";
}

std::vector<sc::Entry> readList(const State& s, uint32_t window, bool party) {
    std::vector<sc::Entry> out;
    const std::string own = readOwnName();
    const uint32_t head = read32(window + kList);
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[0x40] = {};
        if (!readGame(node, raw, sizeof raw)) break;
        uint32_t f[16];
        std::memcpy(f, raw, sizeof f);
        sc::Entry e;
        const std::string name = words(f[6], f[7]);
        std::string map = words(f[10], f[11]);
        for (const char* ext : {".gat", ".rsw"})  // 3.0.11-beta.270 (the tester: «pay_dun01.rsw»): the .rsw too
            if (map.size() > 4 && map.compare(map.size() - 4, 4, ext) == 0) map.resize(map.size() - 4);
        e.name = utf8Of(widened(name, s.codePage));
        e.map = utf8Of(widened(map, s.codePage));
        e.leader = party && f[13] == 0;
        e.online = f[14] == 0;
        e.self = !own.empty() && name == own;
        out.push_back(std::move(e));
    }
    return out;
}

sc::Texts texts(const State& s) {
    sc::Texts t;
    t.tabs[0] = say(settings::Text::SocialFriends);
    t.tabs[1] = say(settings::Text::SocialParty);
    t.offline = say(settings::Text::SocialOffline);
    t.empty = say(settings::Text::SocialEmpty);
    if (s.show.tab == sc::Tab::Party) {
        t.header = s.partyName.empty() ? say(settings::Text::SocialNoParty)
                                       : settings::fmt(say(settings::Text::SocialPartyName), {s.partyName});
        t.actions[0] = say(settings::Text::SocialWhisper);
        t.actions[1] = say(settings::Text::SocialExpel);
        t.actions[2] = say(settings::Text::SocialLeave);
        t.actions[3] = say(settings::Text::SocialSettings);
    } else {
        t.header = settings::fmt(say(settings::Text::SocialFriendCount),
                                 {std::to_string(s.entries.size()), std::to_string(kMostFriends)});
        t.actions[0] = say(settings::Text::SocialWhisper);
        t.actions[1] = say(settings::Text::SocialDelete);
        t.actions[2] = say(settings::Text::SocialSettings);
    }
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
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(Blocker::Social, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Social, nullptr, nullptr, false);
    s.shown = false;
}

void send(State& s, int id, int b, const char* what) {
    const uint32_t window = gameSocial();
    if (!window) return;
    if (s.told++ < 40) LOG_INFO("social: %s - the window's 0x%X", what, unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(kSocialSend))(window, 0, kButtonMsg, id, b, 0);
    s.key.clear();
}

void choose(State& s, int row) {
    const uint32_t window = gameSocial();
    if (!window || row < 0 || row >= int(s.entries.size())) return;
    write32(window + kChosen, uint32_t(row));
    s.show.selected = row;
    s.key.clear();
}

void act(State& s, int button) {
    const std::vector<sc::Action> acts = sc::actions(s.show.tab);
    if (button < 0 || button >= int(acts.size())) return;
    const sc::Action a = acts[size_t(button)];
    const sc::Entry* chosen =
        s.show.selected >= 0 && s.show.selected < int(s.entries.size()) ? &s.entries[size_t(s.show.selected)] : nullptr;
    if (!sc::allowed(s.show.tab, a, chosen)) return;
    if (a == sc::Action::Leave) {  // the game leaves only with one's own row chosen
        for (size_t i = 0; i < s.entries.size(); ++i)
            if (s.entries[i].self) choose(s, int(i));
    }
    const char* const names[5] = {"whisper", "delete or expel (the game asks)", "leave (the game asks)", "settings",
                                  "close"};
    send(s, sc::gameButton(a), 0, names[int(a)]);
}

void showTab(State& s, sc::Tab tab) {
    if (tab == s.show.tab) return;
    send(s, sc::kTabMsg, sc::tabParam(tab), tab == sc::Tab::Party ? "the party's tab" : "the friends' tab");
    s.show.selected = -1;
    s.show.scroll = 0;
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("social: [interface] social = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameSocial() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) {
        s.show = sc::Show{};
        LOG_INFO("social: the game's friends and party window - the new card shows it");
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.show.tab = read32(window + kTab) ? sc::Tab::Party : sc::Tab::Friends;
    s.entries = readList(s, window, s.show.tab == sc::Tab::Party);
    const uint32_t partyName = read32(kPartyName);
    char name[25] = {};
    s.partyName = partyName && readGame(partyName, name, 24) ? utf8Of(widened(name, s.codePage)) : "";
    s.show.selected = int32_t(read32(window + kChosen));
    if (s.show.selected >= int(s.entries.size())) s.show.selected = -1;
    s.layout = sc::layout(targetWidth, targetHeight, s.scale, s.show.tab, s.entries.size());
    s.drag.apply(s.layout.x, s.layout.y, s.layout.w, s.layout.h, targetWidth, targetHeight, k,
                 48 * s.scale);  // .270: moved by its empty part
    s.show.scroll = std::clamp(s.show.scroll, 0, sc::mostScroll(s.layout, s.entries.size()));
    s.show.hoverRow = s.show.hoverButton = s.show.hoverTab = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        const sc::Hit h = sc::hitTest(s.layout, s.show.scroll, s.entries.size(), lx, ly);
        if (h.part == sc::Part::Row) s.show.hoverRow = h.index;
        if (h.part == sc::Part::Button) s.show.hoverButton = h.index;
        if (h.part == sc::Part::Tab) s.show.hoverTab = h.index;
        s.show.hoverClose = h.part == sc::Part::Close;
    }
    std::string key = std::to_string(int(s.show.tab)) + ';' + std::to_string(s.show.selected) + ',' +
                      std::to_string(s.show.scroll) + ',' + std::to_string(s.show.hoverRow) + ',' +
                      std::to_string(s.show.hoverButton) + ',' + std::to_string(s.show.hoverTab) +
                      (s.show.hoverClose ? "x" : "") + ',' + std::to_string(s.show.focus) + ';' + s.partyName + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (const sc::Entry& e : s.entries)
        key += ';' + e.name + ',' + e.map + (e.online ? "o" : "") + (e.leader ? "l" : "") + (e.self ? "s" : "");
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.34
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.34 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): this window in RmlUi's
// markup, line by line as core socialcard paints it (its boxes, icons, circles); the words where it places them
static void fillSheet(State& s, uisheet::Content& ui) {
    using namespace uagfx::socialcard;
    const auto& L = s.layout;
    const auto& entries = s.entries;
    const auto& sh = s.show;
    [[maybe_unused]] constexpr uint32_t kCard = 0x10141C;
    [[maybe_unused]] constexpr uint32_t kWhite = 0xFFFFFF;
    [[maybe_unused]] constexpr uint32_t kGold = 0xC9A55B;
    [[maybe_unused]] constexpr uint32_t kGold2 = 0xE6CB8A;
    [[maybe_unused]] constexpr uint32_t kText = 0xEEF2F8;
    [[maybe_unused]] constexpr uint32_t kMuted = 0x9AA3B5;
    [[maybe_unused]] constexpr uint32_t kOnline = 0x4CC777;
    [[maybe_unused]] constexpr uint32_t kOffline = 0x5C6370;
    [[maybe_unused]] constexpr float kW = 360;
    [[maybe_unused]] constexpr float kPad = 16;
    [[maybe_unused]] constexpr float kTabH = 30;
    [[maybe_unused]] constexpr float kHeaderH = 20;
    [[maybe_unused]] constexpr float kRowH = 26;
    [[maybe_unused]] constexpr float kButtonH = 32;
    [[maybe_unused]] constexpr float kGap = 6;
    ui.w = float(L.w), ui.h = float(L.h);
    const auto put = [&](const panel::Picture& p, const panel::Box& box, float k = 1.0f) {
        if (p.width > 0)
            ui.icons.push_back(uisheet::Icon{std::round(box.x + (box.w - float(p.width)) / 2),
                                            std::round(box.y + (box.h - float(p.height)) / 2), float(p.width),
                                            float(p.height), k, s.sheet.icon(&p)});
    };
    [[maybe_unused]] const auto circle = [&](float cx, float cy, float r, uint32_t rgb, float a) {
        ui.rect(ui.under, panel::Box{cx - r, cy - r, 2 * r, 2 * r}, r, rgb, a);
    };
    [[maybe_unused]] const auto icon = [&](uiicons::Icon which, float x, float y, float side, uint32_t rgb, float a,
                                           float stroke) {
        const int px = std::max(4, int(std::ceil(side)));
        const std::string key = "u" + std::to_string(int(which)) + ':' + std::to_string(px) + ':' +
                                std::to_string(rgb) + ':' + std::to_string(int(a * 100));
        const std::string src = s.sheet.made(key, px, px, [&](panel::Picture& p) {
            uiicons::draw(p, which, 0, 0, float(px), rgb, a, stroke);
        });
        ui.icons.push_back(uisheet::Icon{x, y, float(px), float(px), 1, src});
    };
    (void)put;

    const float sc = L.s;
    ui.rect(ui.under, L.glass, 12 * sc, kCard, uistyle::window(), kWhite, 0.12f, 1.0f * sc);
    ui.rect(ui.under, L.close, 6 * sc, kWhite, sh.hoverClose ? 0.14f : 0.06f);
    for (int i = 0; i < 2; ++i) {
        const bool on = int(sh.tab) == i, hot = sh.hoverTab == i;
        ui.rect(ui.under, L.tabs[i], 8 * sc, on ? kGold : kWhite, on ? 0.24f : hot ? 0.10f : 0.04f,
                on ? kGold : kWhite, on ? 0.8f : 0.12f, 1.0f * sc);
    }
    if (L.rows.empty()) ui.rect(ui.under, L.list, 8 * sc, 0x000000, 0.18f);
    for (size_t i = 0; i < L.rows.size(); ++i) {
        const size_t at = i + size_t(std::max(0, sh.scroll));
        if (at >= entries.size()) break;
        const Entry& e = entries[at];
        const bool chosen = int(at) == sh.selected, hot = int(at) == sh.hoverRow;
        ui.rect(ui.under, L.rows[i], 7 * sc, chosen ? kGold : kWhite, chosen ? 0.16f : hot ? 0.06f : 0.025f,
                chosen ? kGold : kWhite, chosen ? 0.8f : 0.0f, 1.0f * sc);
        const float d = 8 * sc;
        ui.rect(ui.under, panel::Box{L.rows[i].x + 8 * sc, L.rows[i].y + (L.rows[i].h - d) / 2, d, d}, d / 2,
                e.online ? kOnline : kOffline, 0.95f);
    }
    const std::vector<Action> acts = actions(sh.tab);
    const Entry* chosen = chosenEntry(entries, sh);
    for (size_t i = 0; i < L.buttons.size() && i < acts.size(); ++i) {
        const bool can = allowed(sh.tab, acts[i], chosen), hot = sh.hoverButton == int(i) || sh.focus == int(i);
        ui.rect(ui.under, L.buttons[i], 7 * sc, kWhite, !can ? 0.02f : hot ? 0.13f : 0.06f,
                sh.focus == int(i) ? kGold : kWhite, sh.focus == int(i) ? 0.95f : 0.14f,
                (sh.focus == int(i) ? 1.5f : 1.0f) * sc);
    }
    ui.say(uagfx::socialcard::words(s.layout, s.entries, s.show, texts(s)));
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("social", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<SocialEntryView>()) {
            v.RegisterMember("name", &SocialEntryView::name);
            v.RegisterMember("map", &SocialEntryView::map);
            v.RegisterMember("online", &SocialEntryView::online);
            v.RegisterMember("leader", &SocialEntryView::leader);
        }
        m.RegisterArray<std::vector<SocialEntryView>>();
        m.RegisterArray<std::vector<Rml::String>>();
        m.Bind("entries", &s.entryViews);
        m.Bind("tabs", &s.tabViews);
        m.Bind("actions", &s.actionViews);
        m.Bind("header", &s.header);
        m.Bind("offline", &s.offline);
        m.Bind("empty", &s.empty);
        m.Bind("tab", &s.tabNo);
        m.Bind("selected", &s.selected);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("so_tab", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) showTab(state(), sc::Tab(std::clamp(a[0].Get<int>(), 0, 1)));
        });
        m.BindEventCallback("so_choose", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) choose(state(), a[0].Get<int>());
        });
        m.BindEventCallback("so_act", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) act(state(), a[0].Get<int>());
        });
        m.BindEventCallback("so_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            send(state(), sc::gameButton(sc::Action::Close), 0, "close");
        });
    });
    if (!s.live.made()) return;
    const sc::Texts t = texts(s);
    std::vector<SocialEntryView> entries;
    for (const sc::Entry& e : s.entries) entries.push_back(SocialEntryView{e.name, e.map, e.online, e.leader});
    std::vector<Rml::String> actions;
    for (size_t i = 0; i < sc::actions(s.show.tab).size() && i < 4; ++i) actions.push_back(t.actions[i]);
    const std::vector<Rml::String> tabs{t.tabs[0], t.tabs[1]};
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const bool same = entries.size() == s.entryViews.size() &&
                      std::equal(entries.begin(), entries.end(), s.entryViews.begin(),
                                 [](const SocialEntryView& a, const SocialEntryView& b) {
                                     return a.name == b.name && a.map == b.map && a.online == b.online &&
                                            a.leader == b.leader;
                                 });
    if (!same) s.entryViews = std::move(entries), s.live.doc.dirty("entries");
    if (tabs != s.tabViews || actions != s.actionViews || t.header != s.header || t.offline != s.offline ||
        t.empty != s.empty || int(s.show.tab) != s.tabNo || s.show.selected != s.selected || ground != s.ground) {
        s.tabViews = tabs, s.actionViews = actions, s.header = t.header, s.offline = t.offline, s.empty = t.empty;
        s.tabNo = int(s.show.tab), s.selected = s.show.selected, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    s.live.frame(s.shown, s.layout.x, s.layout.y, int(winstack::Win::Social), ".tab, .row, .btn, .x");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    if (uaui::readyAnywhere()) {  // 3.0.18-beta.81: the live window in place of the sheet
        s.sheet.frame("social", false, true, 0, 0, 0, [](uisheet::Content&) {});
        s.sheetShown = false;
        liveFrame(s);
        return;
    }
    s.live.frame(false, 0, 0, 0, nullptr);
    s.sheetShown = s.sheet.frame("social", s.shown, true, s.layout.x, s.layout.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.sheet.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (s.live.shown) {  // 3.0.18-beta.81
        s.live.draw(s.shadow, record, renderer, s.scale, s.k);
        return;
    }
    if (!s.shown || s.k <= 0 || ((s.picture.width <= 0 || s.picture.height <= 0) && !s.sheetShown)) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h),
                  12 * s.scale, s.scale, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (s.live.shown) {  // 3.0.18-beta.87: RmlUi's - a press on its ground drags it, as before
        if (m == WM_LBUTTONUP) s.drag.end();
        const int r = s.live.mouse(game, m, wp, lp);
        if (!r) return false;
        float tx = 0, ty = 0;
        if (r == 2 && pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) s.drag.begin(tx, ty);
        result = 0;
        return true;
    }
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_MOUSEWHEEL) {
        const int by = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1;
        s.show.scroll = std::clamp(s.show.scroll + by, 0, sc::mostScroll(s.layout, s.entries.size()));
        s.key.clear();
        return true;
    }
    if (m == WM_LBUTTONUP) s.drag.end();
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const sc::Hit h = sc::hitTest(s.layout, s.show.scroll, s.entries.size(), lx, ly);
    switch (h.part) {
        case sc::Part::Tab: showTab(s, sc::Tab(h.index)); break;
        case sc::Part::Row:
            choose(s, h.index);
            if (m == WM_LBUTTONDBLCLK) act(s, 0);  // twice on a person: the whisper to them
            break;
        case sc::Part::Button: act(s, h.index); break;
        case sc::Part::Close: send(s, sc::gameButton(sc::Action::Close), 0, "close"); break;
        case sc::Part::None: s.drag.begin(tx, ty); break;  // .270: dragged by its empty part
    }
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    if (!s.shown) return false;
    send(s, sc::gameButton(sc::Action::Close), 0, "Esc - close");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameSocial(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const int rows = int(s.entries.size()), buttons = int(sc::actions(s.show.tab).size());
    int& f = s.show.focus;
    if (f < 0) f = 0;
    switch (c) {
        case PadCommand::Up: choose(s, std::max(0, s.show.selected - 1)); break;
        case PadCommand::Down: choose(s, std::min(rows - 1, s.show.selected + 1)); break;
        case PadCommand::Left: f = std::max(0, f - 1); break;
        case PadCommand::Right: f = std::min(buttons - 1, f + 1); break;
        case PadCommand::Press: act(s, f); break;
        case PadCommand::TabBack:
        case PadCommand::TabOn:
            showTab(s, s.show.tab == sc::Tab::Party ? sc::Tab::Friends : sc::Tab::Party);
            f = 0;
            break;
        case PadCommand::Close: send(s, sc::gameButton(sc::Action::Close), 0, "the controller's close"); break;
    }
    if (s.show.selected >= 0) {
        if (s.show.selected < s.show.scroll) s.show.scroll = s.show.selected;
        if (s.show.selected >= s.show.scroll + int(s.layout.rows.size()))
            s.show.scroll = s.show.selected - int(s.layout.rows.size()) + 1;
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

}  // namespace uagfx::win::socialcardwin

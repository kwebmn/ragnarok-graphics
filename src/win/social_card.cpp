#include "win/social_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <vector>

#include "core/window_stack.hpp"
#include "core/settings/strings.hpp"
#include "core/social_card.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/card_drag.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "uikit/social_live.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"
#include "win/party_panel.hpp"
#include "win/ui_field.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/runtime.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

namespace uagfx::win::socialcardwin {
namespace {

namespace sc = uagfx::socialcard;
namespace ul = uagfx::uikit::social;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the friends and party window (where the manager keeps it, its class's vtable, its SendMsg), a window's
// shown flag, its tab, its list (a node: next +0, valid +8, the name's words +0x18 / +0x1C, the map's +0x28 / +0x2C,
// the role +0x34 - 0 the leader -, offline +0x38) and the chosen row; the party's name the game keeps (a char*)
constexpr uint32_t kWindowMgr = 0x727ED8, kSocialAt = kWindowMgr + 0x11C, kSocialClass = 0x69BBB4;
constexpr uint32_t kSocialSend = 0x4C2250, kShown = 0x38, kSendMsgSlot = 0x94, kTab = 0xC0, kList = 0xA4;
constexpr uint32_t kChosen = 0xAC, kPartyName = 0x772D88;
constexpr int kButtonMsg = 6, kMostFriends = 40;
constexpr size_t kMostNodes = 100;

struct State {
    int saidMenuRow = -2, saidMenuFocus = -2;  // 3.0.18-beta.173: the menu's row and focus RmlUi was told last
    uilive::Window live;  // 3.0.18-beta.81
    ul::Model mv;  // 3.0.18-beta.141: the live window's model (uikit/social_live)
    std::vector<int> rowEntry;              // a shown row's entry (the friends online first)
    std::vector<sc::Item> menuItems;        // the open menu's
    std::map<const panel::Picture*, std::string> facePrev;
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
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
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
    if (readHoverCursor(cx, cy)) {
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

// 3.0.18-beta.141: the game's friend request by a name (FUN_004cec50, stdcall: the name's 24 bytes - the packet
// 0x0202, as its own menu over a player sends it)
constexpr uint32_t kAddFriend = 0x4CEC50;
using AddFriendFn = void(__attribute__((stdcall)) *)(const char* name24);

static bool leading(const State& s) {
    for (const sc::Entry& e : s.entries)
        if (e.self) return e.leader;
    return false;
}

static const partywin::Person* personOf(const std::vector<partywin::Person>& people, const std::string& name) {
    for (const partywin::Person& p : people)
        if (p.name == name) return &p;
    return nullptr;
}

static void askFriend(State& s, const std::string& utf8) {
    std::string name = narrowed(widened(utf8, CP_UTF8), s.codePage);
    while (!name.empty() && name.back() == ' ') name.pop_back();
    while (!name.empty() && name.front() == ' ') name.erase(0, 1);
    if (name.empty() || name.size() > 23) return;
    char buf[24] = {};
    std::memcpy(buf, name.data(), name.size());
    reinterpret_cast<AddFriendFn>(uintptr_t(kAddFriend))(buf);
    LOG_INFO("social: a friend request to %s (the game's 0x0202)", utf8.c_str());
}

static void closeMenu(State& s) {
    s.mv.menuRow = -1;
    s.menuItems.clear();
    s.mv.menuFocus = 0;
}

static void openMenu(State& s, int row) {
    if (row < 0 || row >= int(s.rowEntry.size())) return;
    const int at = s.rowEntry[size_t(row)];
    choose(s, at);
    const sc::Entry& e = s.entries[size_t(at)];
    const std::vector<partywin::Person> people = partywin::people();
    const partywin::Person* p = personOf(people, e.name);
    const bool elsewhere = p ? !p->where.empty() : (!e.map.empty() && e.map != readGameMap());
    s.menuItems = sc::menu(s.show.tab, e, leading(s), elsewhere);
    s.mv.menuRow = s.menuItems.empty() ? -1 : row;
    s.mv.menuFocus = 0;
}

static void doItem(State& s, sc::Item item) {
    if (s.mv.menuRow < 0 || s.mv.menuRow >= int(s.rowEntry.size())) return;
    const int at = s.rowEntry[size_t(s.mv.menuRow)];
    const std::string name = s.entries[size_t(at)].name;
    choose(s, at);
    switch (item) {
        case sc::Item::Whisper: send(s, sc::gameButton(sc::Action::Whisper), 0, "whisper (its menu)"); break;
        case sc::Item::Expel:
        case sc::Item::RemoveFriend: send(s, sc::gameButton(sc::Action::Remove), 0, "remove (the game asks)"); break;
        case sc::Item::Leave: act(s, 2); break;
        case sc::Item::AddFriend: askFriend(s, name); break;
        case sc::Item::Route:
            if (!partywin::routeTo(name)) LOG_INFO("social: no way to %s known", name.c_str());
            break;
        case sc::Item::CopyName: uifield::setClipboard(name); break;
    }
    closeMenu(s);
}

static Rml::String itemWord(sc::Item i) {
    using settings::Text;
    switch (i) {
        case sc::Item::Whisper: return say(Text::SocialWhisper);
        case sc::Item::Expel: return say(Text::SocialExpel);
        case sc::Item::Leave: return say(Text::SocialLeave);
        case sc::Item::RemoveFriend: return say(Text::SocialDelete);
        case sc::Item::AddFriend: return say(Text::SocialAddFriend);
        case sc::Item::Route: return say(Text::SocialRoute);
        case sc::Item::CopyName: return say(Text::SocialCopy);
    }
    return "";
}

// A row's quick buttons (shown on hover): the whisper and the leader's expel - one's own row: leave
static void quickOf(const State& s, const sc::Entry& e, sc::Item q[2]) {
    q[0] = q[1] = sc::Item::CopyName;  // CopyName here: none
    if (e.self) {
        if (s.show.tab == sc::Tab::Party) q[0] = sc::Item::Leave;
        return;
    }
    if (e.online) q[0] = sc::Item::Whisper;
    if (s.show.tab == sc::Tab::Party && leading(s)) (q[0] == sc::Item::CopyName ? q[0] : q[1]) = sc::Item::Expel;
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.ownDrag = true;  // 3.0.18-beta.92: its own drag
    s.live.make("social", ul::rml(), [&](Rml::DataModelConstructor& m) {
        ul::bind(m, s.mv);
        m.BindEventCallback("so_tab", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (a.empty()) return;
            closeMenu(state());
            showTab(state(), sc::Tab(std::clamp(a[0].Get<int>(), 0, 1)));
        });
        m.BindEventCallback("so_choose", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (a.empty()) return;
            const int row = a[0].Get<int>();
            if (row >= 0 && row < int(st.rowEntry.size())) choose(st, st.rowEntry[size_t(row)]);
            if (st.mv.menuRow != row) closeMenu(st);
        });
        m.BindEventCallback("so_down", [](Rml::DataModelHandle, Rml::Event& ev, const Rml::VariantList& a) {
            if (!a.empty() && ev.GetParameter<int>("button", 0) == 1) openMenu(state(), a[0].Get<int>());
        });
        m.BindEventCallback("so_whisper", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (a.empty()) return;
            const int row = a[0].Get<int>();
            if (row < 0 || row >= int(st.rowEntry.size())) return;
            const sc::Entry& e = st.entries[size_t(st.rowEntry[size_t(row)])];
            if (e.self || !e.online) return;
            choose(st, st.rowEntry[size_t(row)]);
            send(st, sc::gameButton(sc::Action::Whisper), 0, "whisper (twice on a person)");
        });
        m.BindEventCallback("so_quick", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (a.size() < 2) return;
            const int row = a[0].Get<int>(), which = a[1].Get<int>();
            if (row < 0 || row >= int(st.rowEntry.size())) return;
            sc::Item q[2];
            quickOf(st, st.entries[size_t(st.rowEntry[size_t(row)])], q);
            if (which < 0 || which > 1 || q[which] == sc::Item::CopyName) return;
            st.mv.menuRow = row;  // as the menu's: the same doing
            doItem(st, q[which]);
        });
        m.BindEventCallback("so_item", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            State& st = state();
            if (!a.empty() && a[0].Get<int>() >= 0 && a[0].Get<int>() < int(st.menuItems.size()))
                doItem(st, st.menuItems[size_t(a[0].Get<int>())]);
        });
        m.BindEventCallback("so_settings", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            send(state(), sc::gameButton(sc::Action::Settings), 0, "settings (the header)");
        });
        m.BindEventCallback("so_add", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            State& st = state();
            askFriend(st, st.mv.addName);
            st.mv.addName.clear();
            st.live.doc.dirty("add_name");
        });
        m.BindEventCallback("so_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            send(state(), sc::gameButton(sc::Action::Close), 0, "close");
        });
    });
    if (!s.live.made()) return;
    if (!s.shown) {  // 3.0.18-beta.173 (R-AKUGNX: its data marked changed each frame, closed too): hidden alone
        s.live.frame(false, s.layout.x, s.layout.y, int(winstack::Win::Social),
                     ".tab, .row, .btn, .x, .qb, .mi, .sset");
        return;
    }
    using settings::Text;
    const sc::Texts t = texts(s);
    const bool party = s.show.tab == sc::Tab::Party;
    const std::vector<partywin::Person> people = party ? partywin::people() : std::vector<partywin::Person>{};
    s.rowEntry = sc::order(s.entries, s.show.tab);
    std::vector<ul::RowV> rows;
    for (const int at : s.rowEntry) {
        const sc::Entry& e = s.entries[size_t(at)];
        ul::RowV v;
        v.idx = at;
        v.name = e.name;
        v.online = e.online;
        v.leader = e.leader;
        const std::wstring wide = widened(e.name, CP_UTF8);
        v.letter = wide.empty() ? "?" : utf8Of(wide.substr(0, 1));
        v.where = e.online ? e.map : t.offline;
        if (const partywin::Person* p = personOf(people, e.name)) {
            v.job = p->job;
            v.dead = p->dead && p->online;
            if (p->online && p->where.empty()) {
                v.hp = p->dead ? 0 : p->percent;
                v.where = p->cells >= 0 ? settings::fmt(say(Text::SocialNearCells), {std::to_string(p->cells)})
                                        : say(Text::SocialNear);
            } else if (p->online) {
                v.where = p->where;
            }
            if (v.dead) v.where = say(Text::SocialDeadNow);
            if (p->face && p->face->width > 0)
                v.face = uaui::picture(p->face, uaui::stamp(p->face), s.facePrev[p->face]);
        }
        sc::Item q[2];
        quickOf(s, e, q);
        v.q0 = q[0] == sc::Item::CopyName ? "" : itemWord(q[0]);
        v.q1 = q[1] == sc::Item::CopyName ? "" : itemWord(q[1]);
        rows.push_back(std::move(v));
    }
    if (s.mv.menuRow >= int(rows.size())) closeMenu(s);
    std::vector<Rml::String> menu;
    for (const sc::Item i : s.menuItems) menu.push_back(itemWord(i));
    const std::vector<Rml::String> tabs{t.tabs[0], t.tabs[1]};
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const int online = sc::onlineCount(s.entries);
    const Rml::String count = s.entries.empty() || (party && s.partyName.empty())
                                  ? ""
                                  : settings::fmt(say(Text::SocialOnline),
                                                  {std::to_string(online), std::to_string(s.entries.size())});
    const int share = party && !s.partyName.empty() ? partywin::expShare() : -1;
    const Rml::String exp = share < 0 ? "" : say(share ? Text::SocialExpEven : Text::SocialExpEach);
    const Rml::String header = party ? (s.partyName.empty() ? say(Text::SocialNoParty) : s.partyName)
                                     : t.tabs[0];
    const Rml::String settingsWord = party && !s.partyName.empty() ? say(Text::SocialSettings) : "";
    const Rml::String empty = party ? say(Text::SocialNoPartyHint) : say(Text::SocialNoFriendsHint);
    Rml::String hint = say(Text::SocialMouseHint);
    if (uapad::runtime::navLook().shown) {
        hint = say(Text::SocialPadHint);
        const char* const labels[4] = {uapad::buttonLabel(0), uapad::buttonLabel(4), uapad::buttonLabel(5),
                                       uapad::buttonLabel(1)};
        const char* const marks[4] = {"{1}", "{2}", "{3}", "{4}"};
        for (int i = 0; i < 4; ++i)
            if (const size_t at = hint.find(marks[i]); at != Rml::String::npos) hint.replace(at, 3, labels[i]);
    }
    const int padRow = uapad::runtime::navLook().shown ? s.show.focusRow : -1;
    ul::Model& v = s.mv;
    if (rows != v.rows) v.rows = std::move(rows), s.live.doc.dirty("rows");
    if (menu != v.menu) v.menu = std::move(menu), s.live.doc.dirty("menu");
    if (tabs != v.tabs || header != v.header || count != v.count || exp != v.exp || settingsWord != v.settings ||
        empty != v.empty || hint != v.hint || int(s.show.tab) != v.tab || s.show.selected != v.selected ||
        ground != v.ground || padRow != v.padRow) {
        v.tabs = tabs, v.header = header, v.count = count, v.exp = exp, v.settings = settingsWord;
        v.empty = empty, v.hint = hint, v.tab = int(s.show.tab), v.selected = s.show.selected;
        v.ground = ground, v.padRow = padRow;
        v.nameHint = say(Text::SocialNameField), v.add = say(Text::SocialAdd);
        s.live.doc.dirtyAll();
    }
    if (v.menuRow != s.saidMenuRow) s.saidMenuRow = v.menuRow, s.live.doc.dirty("menu_row");  // .173: on a change
    if (v.menuFocus != s.saidMenuFocus) s.saidMenuFocus = v.menuFocus, s.live.doc.dirty("menu_focus");
    s.live.frame(s.shown, s.layout.x, s.layout.y, int(winstack::Win::Social), ".tab, .row, .btn, .x, .qb, .mi, .sset");
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
    if (s.live.shown) {  // 3.0.18-beta.87: RmlUi's - a press on its ground drags it, as before
        if (m == WM_LBUTTONUP) s.drag.end();
        const int r = s.live.mouse(game, m, wp, lp);
        if (!r) return false;
        float tx = 0, ty = 0;
        if (r == 2 && pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) s.drag.begin(tx, ty);
        result = 0;
        return true;
    }
    return false;  // the old edition gone: the live window alone
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

// 3.0.18-beta.141: the controller walks the people (up / down), A opens one's menu - left / right its items, A
// does one, B shuts it (B again closes the card); LB / RB the tabs
void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const int rows = int(s.rowEntry.size());
    int& r = s.show.focusRow;
    if (r >= rows) r = rows - 1;
    const bool menu = s.mv.menuRow >= 0 && s.mv.menuRow == r;
    switch (c) {
        case PadCommand::Up:
        case PadCommand::Down:
            if (rows == 0) break;
            r = std::clamp(r < 0 ? 0 : r + (c == PadCommand::Up ? -1 : 1), 0, rows - 1);
            choose(s, s.rowEntry[size_t(r)]);
            closeMenu(s);
            break;
        case PadCommand::Left:
            if (menu) s.mv.menuFocus = std::max(0, s.mv.menuFocus - 1);
            break;
        case PadCommand::Right:
            if (menu) s.mv.menuFocus = std::min(int(s.menuItems.size()) - 1, s.mv.menuFocus + 1);
            break;
        case PadCommand::Press:
            if (menu && s.mv.menuFocus >= 0 && s.mv.menuFocus < int(s.menuItems.size()))
                doItem(s, s.menuItems[size_t(s.mv.menuFocus)]);
            else if (r >= 0)
                openMenu(s, r);
            break;
        case PadCommand::TabBack:
        case PadCommand::TabOn:
            closeMenu(s);
            showTab(s, s.show.tab == sc::Tab::Party ? sc::Tab::Friends : sc::Tab::Party);
            r = -1;
            break;
        case PadCommand::Close:
            if (s.mv.menuRow >= 0) closeMenu(s);
            else send(s, sc::gameButton(sc::Action::Close), 0, "the controller's close");
            break;
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

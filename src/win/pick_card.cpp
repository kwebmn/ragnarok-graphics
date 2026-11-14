#include "win/pick_card.hpp"

#include <map>
#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/hotbar_panel.hpp"
#include "core/item_window.hpp"
#include "core/pick_card.hpp"
#include "core/reference_index.hpp"
#include "core/skill_info.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/client_tables.hpp"
#include "win/game_memory.hpp"
#include "win/info_card.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/updates_hook.hpp"
#include "win/paths.hpp"
#include "win/game_files.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

namespace uagfx::win::pickcardwin {
namespace {

namespace pc = uagfx::pickcard;
namespace iw = uagfx::itemwin;
using uapad::gamewindow::Blocker;

// 3.0.11-beta.261 (the tester: «телепорт в старому стилі»): Menu - a skill's menu (UI window 0x27: Teleport's and Warp
// Portal's places from 0x011c); its list a vector of 16-byte strings (+0x6C its first, +0x70 past the last; a
// string's words at +4, its length +8), the chosen row in its list box (+0x60, the box's +100), the skill at +0x64;
// the game puts «cancel» last (left out here: the card's own cancel sends it)
// 3.0.11-beta.263 (the tester: «подивитися по коду які вміння генерують меню (крафт стріл, крафт поушенів і т.д) і всі
// поміняти проактивно»): the skills' other lists - Weapon Repair's (0x01FC, UI window 0x6B), the upgrade's (0x0221,
// 0x6F), Forging's and Pharmacy's (0x018D, 0x4F) with their rows in a list box as Menu's (its words +0x58 / +0x5C,
// 16-byte strings); Arrow Crafting's and cooking's (0x01AD / 0x025A, 0x5E: cooking when its +0x98 is 2) and the
// incubator's eggs (0x01A6, 0x5A) with things' nodes as the magnifier's.
// 3.0.11-beta.295 (the owner: «Меню Auto Spell — надо сделать»): Auto Spell's (0x01CD, UI window 0x63 at the
// manager's +0x1C8, class 0x699FF8, SendMsg 0x48A7B0): its skills' numbers a vector (+0x9C first, +0xA0 past the last,
// message 0x1F adds one), the chosen row +0x68; OK 0x76 sends mode message 0xB1 with the chosen skill, 0x77 closes
// 3.0.18-beta.1 (the owner: «отложенные окна ... делай»): Change Cart's (UI window 0x5F at the manager's +0x1C4,
// class 0x699F60, SendMsg 0x48A0B0) - a button a cart, each sending the game mode's message 0xA7 with its number
// (packet 0x01AF); the carts by the base level as the game's window offers them (FUN_00489470: 1 to 40 one, 41-65
// two, 66-80 three, 81-90 four, 91 on five). No chosen place of its own: the card keeps the choice
enum class Kind : uint8_t { Identify, Compound, Menu, Repair, Upgrade, Produce, Arrows, Eggs, AutoSpell, Cart };
bool boxed(Kind k) { return k == Kind::Menu || k == Kind::Repair || k == Kind::Upgrade || k == Kind::Produce; }
const char* nameOf(Kind k) {
    switch (k) {
        case Kind::Identify: return "magnifier's";
        case Kind::Compound: return "card's";
        case Kind::Menu: return "skill menu's";
        case Kind::Repair: return "repair's";
        case Kind::Upgrade: return "upgrade's";
        case Kind::Produce: return "making's";
        case Kind::Arrows: return "arrows' or cooking's";
        case Kind::Eggs: return "incubator's";
        case Kind::AutoSpell: return "Auto Spell's";
        case Kind::Cart: return "cart change's";
    }
    return "";
}
struct GamePick {
    Kind kind;
    uint32_t at, cls, send;
};
// uaRO.exe's: the two windows (where the manager keeps each, its class's vtable, its SendMsg); their list (a node:
// next +0, the ITEM_INFO at +8 - the item's number as words +0x30 / +0x34, known +0x3C, refined +0x40), its count,
// the chosen place (-1 none); a window's shown flag
constexpr uint32_t kWindowMgr = 0x727ED8;
constexpr GamePick kPicks[] = {{Kind::Identify, kWindowMgr + 0xAC, 0x699118, 0x47C580},
                               {Kind::Compound, kWindowMgr + 0xB8, 0x6991B0, 0x47CF20},
                               {Kind::Menu, kWindowMgr + 0xBC, 0x698FE8, 0x47AB10},
                               {Kind::Repair, kWindowMgr + 0xB0, 0x69BCE8, 0x4C4B70},
                               {Kind::Upgrade, kWindowMgr + 0xB4, 0x69BD80, 0x4C5550},
                               {Kind::Produce, kWindowMgr + 0x19C, 0x69BE18, 0x4C5EB0},
                               {Kind::Arrows, kWindowMgr + 0x1C0, 0x699E30, 0x488E00},
                               {Kind::Eggs, kWindowMgr + 0x1B8, 0x699D98, 0x4888E0},
                               {Kind::AutoSpell, kWindowMgr + 0x1C8, 0x699FF8, 0x48A7B0},
                               {Kind::Cart, kWindowMgr + 0x1C4, 0x699F60, 0x48A0B0}};
constexpr uint32_t kBaseLevel = 0x771F94, kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kCartMsg = 0xA7, kCartWindow = 0x5F;
constexpr uint32_t kDeleteWindow = 0x4D1E60;
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using DeleteFn = bool(__attribute__((thiscall)) *)(uint32_t manager, int id);
int g_cartChosen = -1;  // the card's choice in the cart's window (it has none of its own)
constexpr uint32_t kBoxFirst = 0x58, kBoxPast = 0x5C, kArrowsWhat = 0x98;
constexpr uint32_t kSpellsFirst = 0x9C, kSpellsPast = 0xA0;  // .295: Auto Spell's skills
constexpr uint32_t kSkillNames = 0x74B9AC;  // the game's AEGIS names by skill number (win/skill_window)
constexpr int kMostSkill = 3000;
constexpr uint32_t kMenuFirst = 0x6C, kMenuPast = 0x70, kMenuBox = 0x60, kMenuBoxChosen = 100, kMenuSkill = 0x64;
constexpr uint32_t kList = 0x7C, kChosen = 0x68, kShown = 0x38, kSendMsgSlot = 0x94, kInfo = 8;
constexpr int kButtonMsg = 6;
constexpr size_t kInfoSize = 0x48, kMostNodes = 200;

// 3.0.18-beta.87 (live windows, group 2): a list to pick from a live window - rows with their icons, a click
// picks, twice OKs
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw pkw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<div class="msg" data-if="message != ''">{{message}}</div>
<div class="list"><div class="row ctl" data-for="e, i : entries" data-class-on="i == selected"
  data-event-click="pk_choose(i)"
  data-event-dblclick="pk_ok()" data-event-mousedown="pk_card(i)"><img data-if="e.icon != ''"
  data-attr-src="e.icon"/><div class="pwho"><span class="name" data-style-color="e.colour">{{e.name}}</span><span
  class="inside"
  data-if="e.inside != ''">{{e.inside}}</span></div></div>
<div class="hint" data-if="entries.size == 0">{{empty}}</div></div>
<div class="hint" data-if="hint != ''">{{hint}}</div>
<div class="btns"><div class="btn ctl" data-event-click="pk_cancel()">{{cancel}}</div>
<div class="btn main ctl" data-event-click="pk_ok()">{{ok}}</div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
struct PickEntryView {
    Rml::String name, icon, inside, colour = "#eef2f8";
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<PickEntryView> entryViews;
    std::map<const panel::Picture*, std::string> iconPrev;
    Rml::String title, message, ok, cancel, empty, hint, ground = "#10141cf0";
    int selected = -1;
    uisheet::Sheet sheet;  // 3.0.18-beta.31: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    Kind kind = Kind::Identify;
    uint32_t window = 0, hidden = 0;  // the game's window shown now; the one whose shown flag is held at 0
    std::vector<pc::Entry> entries;
    pc::Show show;
    pc::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool padNow = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t rd32(const uint8_t* p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}

// Where the game keeps the chosen row (0: nowhere)
uint32_t chosenAt(uint32_t window, Kind kind) {
    if (kind == Kind::Cart) return 0;
    if (!boxed(kind)) return window + kChosen;
    const uint32_t box = read32(window + kMenuBox);
    return box ? box + kMenuBoxChosen : 0;
}

int32_t chosenOf(uint32_t window, Kind kind) {
    if (kind == Kind::Cart) return g_cartChosen;
    const uint32_t at = chosenAt(window, kind);
    return at ? int32_t(read32(at)) : -1;
}

uint32_t gamePick(Kind& kind) {
    for (const GamePick& g : kPicks) {
        const uint32_t w = read32(g.at);
        if (w && read32(w) == g.cls && read32(read32(w) + kSendMsgSlot) == g.send) {
            kind = g.kind;
            return w;
        }
    }
    return 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (on && window) {
        if (read32(window + kShown) && write32(window + kShown, 0) && s.hidden != window) {
            LOG_INFO("picks: the game's %s window hidden for the new card (its shown flag 0)",
                     nameOf(s.kind));
            s.hidden = window;
        }
    } else if (s.hidden && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

// 3.0.19-beta.1 (the testers: «тут код карти прописує, а встарому назви»): the client's names of maps
// (data\mapnametable.txt) as the game finds the file, else our reader of its archives; looked for again in 10 s while
// none read (the game's files busy while a map loads)
const std::map<std::string, std::string>& clientMapNames() {
    static std::map<std::string, std::string> names;
    static ULONGLONG at = 0;
    if (!names.empty() || (at && GetTickCount64() - at < 10000)) return names;
    at = GetTickCount64();
    std::string text;
    if (!readGameFile("data\\mapnametable.txt", text)) {
        gamefiles::Reader files(gameDir());
        std::vector<uint8_t> bytes;
        if (files.read("data\\mapnametable.txt", bytes, nullptr)) text.assign(bytes.begin(), bytes.end());
    }
    names = refindex::mapNames(text);
    LOG_INFO("picks: %zu names of maps from the client's mapnametable.txt (Warp Portal's and Teleport's rows)",
             names.size());
    return names;
}

std::vector<pc::Entry> readMenu(uint32_t window, unsigned codePage, bool box) {
    std::vector<pc::Entry> out;
    const uint32_t list = box ? read32(window + kMenuBox) : window;  // .263: the list box's own words
    if (!list) return out;
    const uint32_t first = read32(list + (box ? kBoxFirst : kMenuFirst)),
                   past = read32(list + (box ? kBoxPast : kMenuPast));
    if (!first || past <= first || (past - first) / 16 > 64) return out;
    for (uint32_t at = first; at < past; at += 16) {
        char buf[65] = {};
        const uint32_t words = read32(at + 4), len = read32(at + 8);
        if (words && len) readGame(words, buf, std::min<uint32_t>(len, 64));
        std::string name = buf;
        if (at + 16 >= past && name == "cancel") break;  // the game's own last row
        for (const char* end : {".gat", ".rsw"})  // Teleport's save point: its map's name
            if (name.size() > 4 && name.compare(name.size() - 4, 4, end) == 0) name.resize(name.size() - 4);
        pc::Entry e;
        e.name = name.empty() ? "-" : utf8Of(widened(name, codePage));
        if (!box) {  // 3.0.19-beta.1: Teleport's and Warp Portal's maps by the client's names, the code under each
            const pc::Place p = pc::placeOf(clientMapNames(), name);
            if (!p.code.empty()) e.name = p.name, e.inside = p.code;
        }
        out.push_back(std::move(e));
    }
    return out;
}

std::string readText(uint32_t at) {
    char buf[64] = {};
    if (!at || !readGame(at, buf, sizeof buf - 1)) return "";
    return buf;
}

// .295: Auto Spell's skills - a skill's name (the server's, English) and its icon (its AEGIS name in small letters)
std::vector<pc::Entry> readSpells(const State& s, uint32_t window) {
    std::vector<pc::Entry> out;
    const uint32_t first = read32(window + kSpellsFirst), past = read32(window + kSpellsPast);
    if (!first || past <= first || (past - first) / 4 > 16) return out;
    const uint32_t names = read32(kSkillNames);
    const bool known = names && readText(read32(names + 4)) == "NV_BASIC";
    for (uint32_t at = first; at < past; at += 4) {
        const int id = int(read32(at));
        std::string aegis = known && id > 0 && id < kMostSkill ? readText(read32(names + 4 * uint32_t(id))) : "";
        if (aegis.rfind("Skill not defined", 0) == 0) aegis.clear();
        const char* named = skillinfo::name(id);
        pc::Entry e;
        e.name = *named ? std::string(named) : !aegis.empty() ? aegis : "#" + std::to_string(id);
        if (!aegis.empty())
            e.icon = clienttables::icon(hotbar::lowered(aegis), iw::iconSide(s.scale) * 7 / 8, 4 * s.scale);
        out.push_back(std::move(e));
    }
    return out;
}

// 3.0.18-beta.1: the carts the base level has, as the game's window counts them
std::vector<pc::Entry> readCarts() {
    const uint32_t level = read32(kBaseLevel);
    const int count = level >= 91 ? 5 : level >= 81 ? 4 : level >= 66 ? 3 : level >= 41 ? 2 : 1;
    std::vector<pc::Entry> out;
    for (int k = 1; k <= count; ++k) {
        pc::Entry e;
        e.name = settings::fmt(say(settings::Text::PickCartRow), {std::to_string(k)});
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<pc::Entry> readList(const State& s, uint32_t window) {
    if (s.kind == Kind::AutoSpell) return readSpells(s, window);
    if (s.kind == Kind::Cart) return readCarts();
    if (boxed(s.kind)) return readMenu(window, s.codePage, s.kind != Kind::Menu);
    std::vector<pc::Entry> out;
    const auto& names = clienttables::itemNames();
    const auto& unknown = clienttables::unknownNames();
    const auto& files = clienttables::itemFiles();
    const uint32_t head = read32(window + kList);
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kInfoSize] = {};
        if (!readGame(node + kInfo, raw, sizeof raw)) break;
        pc::Entry e;
        const uint32_t ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
        char digits[17] = {};
        if (ptr && len && len <= 16) readGame(ptr, digits, len);
        const std::string id = std::to_string(std::atoi(digits));
        const bool identified = raw[0x3C] != 0;
        std::string words;
        if (!identified) {
            const auto u = unknown.find(id);
            if (u != unknown.end()) words = u->second;
        }
        if (words.empty()) {
            const auto w = names.find(id);
            if (w != names.end()) words = w->second;
        }
        e.name = words.empty() ? "#" + id : utf8Of(widened(hotbar::spaced(words), s.codePage));
        e.name = cardnames::namedFromInfo(e.name, raw, clienttables::cardWords());  // 3.0.18-beta.137: its cards
        infocardwin::describe(raw, std::atoi(id.c_str()), s.codePage, e.name, e.inside);  // .142: [slots], inside
        e.info = node + kInfo;
        if (const std::string c = infocardwin::elementHex(std::atoi(id.c_str()), raw); !c.empty()) e.colour = c;
        const int refine = int(rd32(raw, 0x40));
        e.refine = refine > 0 && refine <= 20 ? refine : 0;
        const auto f = files.find(id);
        if (f != files.end()) e.icon = clienttables::icon(f->second, iw::iconSide(s.scale) * 7 / 8, 4 * s.scale);
        out.push_back(std::move(e));
    }
    return out;
}

pc::Texts texts(const State& s) {
    pc::Texts t;
    const bool identify = s.kind == Kind::Identify;
    t.title = say(identify ? settings::Text::PickIdentifyTitle : settings::Text::PickCompoundTitle);
    t.message = say(identify ? settings::Text::PickIdentifyHint : settings::Text::PickCompoundHint);
    t.ok = say(identify ? settings::Text::PickIdentifyOk : settings::Text::PickCompoundOk);
    if (s.kind == Kind::Menu) {  // 3.0.11-beta.261: the skill's name over its menu
        const char* skill = s.window ? skillinfo::name(int(read32(s.window + kMenuSkill))) : "";
        t.title = *skill ? std::string(skill) : say(settings::Text::PickMenuTitle);
    }
    if (s.kind != Kind::Identify && s.kind != Kind::Compound) {  // .261, .263: a choice of the skill's
        t.message = say(settings::Text::PickMenuHint);
        t.ok = say(settings::Text::PickMenuOk);
    }
    uint16_t what = 0;  // .263: Arrow Crafting's window cooks too
    if (s.kind == Kind::Arrows && s.window) readGame(s.window + kArrowsWhat, &what, 2);
    switch (s.kind) {
        case Kind::Repair: t.title = say(settings::Text::PickRepairTitle); break;
        case Kind::Upgrade: t.title = say(settings::Text::PickUpgradeTitle); break;
        case Kind::Produce: t.title = say(settings::Text::PickProduceTitle); break;
        case Kind::Arrows:
            t.title = say(what == 2 ? settings::Text::PickCookTitle : settings::Text::PickArrowsTitle);
            break;
        case Kind::Eggs: t.title = say(settings::Text::PickEggsTitle); break;
        case Kind::AutoSpell: t.title = "Auto Spell"; break;  // .295: as players call it (the server: «Hindsight»)
        case Kind::Cart: t.title = say(settings::Text::PickCartTitle); break;
        default: break;
    }
    t.cancel = say(settings::Text::MsgCancel);
    t.empty = say(settings::Text::PickEmpty);
    if (s.padNow)  // 3.0.18-beta.142: Y the thing's card
        t.hint = std::string(uapad::buttonLabel(0)) + ": " + t.ok + " \xC2\xB7 " + uapad::buttonLabel(3) + ": " +
                 say(settings::Text::VendPadCard) + " \xC2\xB7 " + uapad::buttonLabel(1) + ": " + t.cancel;
    else
        t.hint = say(settings::Text::ThingCardTip);
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
    uapad::gamewindow::setBlocked(Blocker::Pick, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Pick, nullptr, nullptr, false);
    s.shown = false;
}

void choose(State& s, int row) {
    if (!s.window || row < 0 || row >= int(s.entries.size())) return;
    if (s.kind == Kind::Cart) {  // 3.0.18-beta.1: the card's own choice
        g_cartChosen = row;
        s.show.selected = row;
        s.key.clear();
        return;
    }
    const uint32_t at = chosenAt(s.window, s.kind);
    if (!at) return;
    write32(at, uint32_t(row));
    s.show.selected = row;
    s.key.clear();
}

void press(State& s, int id) {
    Kind kind = Kind::Identify;
    const uint32_t window = gamePick(kind);
    if (!window) return;
    if (id == pc::kOkButton && chosenOf(window, kind) < 0) return;  // nothing chosen: the game's ignores it
    if (kind == Kind::Cart) {  // 3.0.18-beta.1: the chosen cart as its button sends it, then the window closed
        const uint32_t mode = read32(kModeNow);
        const bool modeOk = mode && read32(read32(mode) + kModeMsgSlot) == kModeMsg;
        if (id == pc::kOkButton && modeOk) {
            reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kCartMsg, g_cartChosen + 1, 0, 0);
            LOG_INFO("picks: cart %d chosen - the game mode's 0x%X", g_cartChosen + 1, unsigned(kCartMsg));
        }
        reinterpret_cast<DeleteFn>(uintptr_t(kDeleteWindow))(kWindowMgr, kCartWindow);
        g_cartChosen = -1;
        s.key.clear();
        return;
    }
    if (s.told++ < 30)
        LOG_INFO("picks: %s - the game's %s window's 0x%X", id == pc::kOkButton ? "OK" : "cancel",
                 nameOf(kind), unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)))(window, 0, kButtonMsg, id, 0, 0);
    s.key.clear();
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("picks: [interface] picks = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    Kind kind = Kind::Identify;
    const uint32_t window = world ? gamePick(kind) : 0;
    if (world) {
        s.kind = kind;
        hold(s, window, on);
    }
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    if (!s.shown || window != s.window) {
        s.show = pc::Show{};
        if (window != s.window) g_cartChosen = -1;  // 3.0.18-beta.1
        LOG_INFO("picks: the game's %s window - the new card shows it",
                 nameOf(kind));
    }
    s.window = window;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.padNow = uapad::runtime::navLook().shown;
    s.entries = readList(s, window);
    s.show.selected = chosenOf(window, kind);
    if (s.show.selected >= int(s.entries.size())) s.show.selected = -1;
    s.layout = pc::layout(targetWidth, targetHeight, s.scale, s.entries.size());
    s.show.scroll = std::clamp(s.show.scroll, 0, pc::mostScroll(s.layout, s.entries.size()));
    s.show.hover = -1;
    s.show.hoverPart = 0;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        const pc::Hit h = pc::hitTest(s.layout, s.show.scroll, s.entries.size(), lx, ly);
        s.show.hover = h.row;
        s.show.hoverPart = int(h.part);
    }
    std::string key = std::to_string(int(kind)) + ';' + std::to_string(s.show.selected) + ',' +
                      std::to_string(s.show.hover) + ',' + std::to_string(s.show.hoverPart) + ',' +
                      std::to_string(s.show.scroll) + ';' + std::to_string(s.layout.x) + ',' +
                      std::to_string(s.layout.y) + (s.padNow ? "P" : "") + '#' + std::to_string(uistyle::generation());
    for (const pc::Entry& e : s.entries) key += ';' + e.name + (e.icon ? "i" : "");
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.31
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("pick", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<PickEntryView>()) {
            v.RegisterMember("inside", &PickEntryView::inside);
            v.RegisterMember("colour", &PickEntryView::colour);
            v.RegisterMember("name", &PickEntryView::name);
            v.RegisterMember("icon", &PickEntryView::icon);
        }
        m.RegisterArray<std::vector<PickEntryView>>();
        m.Bind("entries", &s.entryViews);
        m.Bind("title", &s.title);
        m.Bind("message", &s.message);
        m.Bind("ok", &s.ok);
        m.Bind("cancel", &s.cancel);
        m.Bind("empty", &s.empty);
        m.Bind("hint", &s.hint);
        m.Bind("selected", &s.selected);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("pk_choose", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) choose(state(), a[0].Get<int>());
        });
        m.BindEventCallback("pk_card", [](Rml::DataModelHandle, Rml::Event& ev, const Rml::VariantList& a) {
            State& st = state();  // 3.0.18-beta.142: a right click - the thing's card
            if (a.empty() || ev.GetParameter<int>("button", 0) != 1) return;
            const int at = a[0].Get<int>();
            if (at >= 0 && size_t(at) < st.entries.size()) infocardwin::showItem(st.entries[size_t(at)].info);
        });
        m.BindEventCallback("pk_ok", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            press(state(), pc::kOkButton);
        });
        m.BindEventCallback("pk_cancel", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            press(state(), pc::kCancelButton);
        });
    });
    if (!s.live.made()) return;
    const pc::Texts t = texts(s);
    std::vector<PickEntryView> entries;
    for (const pc::Entry& e : s.entries) {
        PickEntryView v;
        v.name = e.refine > 0 ? "+" + std::to_string(e.refine) + " " + e.name : e.name;
        v.inside = e.inside;
        v.colour = e.colour;
        if (e.icon && e.icon->width > 0) v.icon = uaui::picture(e.icon, uaui::stamp(e.icon), s.iconPrev[e.icon]);
        entries.push_back(v);
    }
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const bool same = entries.size() == s.entryViews.size() &&
                      std::equal(entries.begin(), entries.end(), s.entryViews.begin(),
                                 [](const PickEntryView& a, const PickEntryView& b) {
                                     return a.name == b.name && a.icon == b.icon && a.inside == b.inside &&
                                            a.colour == b.colour;
                                 });
    if (!same) s.entryViews = std::move(entries), s.live.doc.dirty("entries");
    if (t.title != s.title || t.message != s.message || t.ok != s.ok || t.cancel != s.cancel || t.empty != s.empty ||
        t.hint != s.hint || s.show.selected != s.selected || ground != s.ground) {
        s.title = t.title, s.message = t.message, s.ok = t.ok, s.cancel = t.cancel, s.empty = t.empty;
        s.hint = t.hint, s.selected = s.show.selected, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    int x = s.layout.x, y = s.layout.y, bx = 0, by = 0, bw = 0, bh = 0;
    if (s.live.box(bx, by, bw, bh)) x = (s.targetW - bw) / 2, y = std::max(0, s.targetH / 3 - bh / 2);
    s.live.frame(s.shown, x, y, int(winstack::Win::Pick), ".row, .btn");
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
    press(s, pc::kCancelButton);
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    Kind kind = Kind::Identify;
    if (const uint32_t window = gamePick(kind)) hold(s, window, true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const int n = int(s.entries.size());
    switch (c) {
        case PadCommand::Up: choose(s, std::max(0, s.show.selected - 1)); break;
        case PadCommand::Down: choose(s, std::min(n - 1, s.show.selected + 1)); break;
        case PadCommand::Ok: press(s, pc::kOkButton); break;
        case PadCommand::Cancel: press(s, pc::kCancelButton); break;
        case PadCommand::Card:  // 3.0.18-beta.142
            if (s.show.selected >= 0 && s.show.selected < n)
                infocardwin::showItem(s.entries[size_t(s.show.selected)].info);
            break;
    }
    if (s.show.selected >= 0) {  // the chosen row in sight
        if (s.show.selected < s.show.scroll) s.show.scroll = s.show.selected;
        if (s.show.selected >= s.show.scroll + int(s.layout.rows.size()))
            s.show.scroll = s.show.selected - int(s.layout.rows.size()) + 1;
    }
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

}  // namespace uagfx::win::pickcardwin

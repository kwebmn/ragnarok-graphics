#include "win/pick_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "core/item_window.hpp"
#include "core/pick_card.hpp"
#include "core/skill_info.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::pickcardwin {
namespace {

namespace pc = uagfx::pickcard;
namespace iw = uagfx::itemwin;
using uapad::gamewindow::Blocker;

// 3.0.11-beta.261 (the tester: «телепорт в старому стилі»): Menu - a skill's menu (UI window 0x27: Teleport's and Warp
// Portal's places from 0x011c); its list a vector of 16-byte strings (+0x6C its first, +0x70 past the last; a
// string's words at +4, its length +8), the chosen row in its list box (+0x60, the box's +100), the skill at +0x64;
// the game puts «cancel» last (left out here: the card's own cancel sends it)
enum class Kind : uint8_t { Identify, Compound, Menu };
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
                               {Kind::Menu, kWindowMgr + 0xBC, 0x698FE8, 0x47AB10}};
constexpr uint32_t kMenuFirst = 0x6C, kMenuPast = 0x70, kMenuBox = 0x60, kMenuBoxChosen = 100, kMenuSkill = 0x64;
constexpr uint32_t kList = 0x7C, kChosen = 0x68, kShown = 0x38, kSendMsgSlot = 0x94, kInfo = 8;
constexpr int kButtonMsg = 6;
constexpr size_t kInfoSize = 0x48, kMostNodes = 200;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);

struct State {
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

std::string say(settings::Text t) { return settings::tr(language(), t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t rd32(const uint8_t* p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p + at, 4);
    return v;
}

// Where the game keeps the chosen row (0: nowhere)
uint32_t chosenAt(uint32_t window, Kind kind) {
    if (kind != Kind::Menu) return window + kChosen;
    const uint32_t box = read32(window + kMenuBox);
    return box ? box + kMenuBoxChosen : 0;
}

int32_t chosenOf(uint32_t window, Kind kind) {
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
                     s.kind == Kind::Identify ? "magnifier's" : s.kind == Kind::Menu ? "skill menu's" : "card's");
            s.hidden = window;
        }
    } else if (s.hidden && s.hidden == window) {
        write32(window + kShown, 1);
        s.hidden = 0;
    } else if (!window) {
        s.hidden = 0;
    }
}

std::vector<pc::Entry> readMenu(uint32_t window, unsigned codePage) {
    std::vector<pc::Entry> out;
    const uint32_t first = read32(window + kMenuFirst), past = read32(window + kMenuPast);
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
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<pc::Entry> readList(const State& s, uint32_t window) {
    if (s.kind == Kind::Menu) return readMenu(window, s.codePage);
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
        t.message = say(settings::Text::PickMenuHint);
        t.ok = say(settings::Text::PickMenuOk);
    }
    t.cancel = say(settings::Text::MsgCancel);
    t.empty = say(settings::Text::PickEmpty);
    if (s.padNow)
        t.hint = std::string(uapad::buttonLabel(0)) + ": " + t.ok + " \xC2\xB7 " + uapad::buttonLabel(1) + ": " +
                 t.cancel;
    return t;
}

std::vector<Words> gdiWords(const std::vector<pc::Words>& in) {
    std::vector<Words> out;
    for (const pc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == pc::Align::Right    ? DT_RIGHT
                                 : w.align == pc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("a choice of things", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    pc::paint(s.picture, s.layout, s.entries, s.show);
    drawWords(s.picture, gdiWords(pc::words(s.layout, s.entries, s.show, texts(s))));
    s.upload = true;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(Blocker::Pick, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Pick, nullptr, nullptr, false);
    s.shown = false;
}

void choose(State& s, int row) {
    if (!s.window || row < 0 || row >= int(s.entries.size())) return;
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
    if (s.told++ < 30)
        LOG_INFO("picks: %s - the game's %s window's 0x%X", id == pc::kOkButton ? "OK" : "cancel",
                 kind == Kind::Identify ? "magnifier's" : kind == Kind::Menu ? "skill menu's" : "card's", unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)))(window, 0, kButtonMsg, id, 0, 0);
    s.key.clear();
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
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
        LOG_INFO("picks: the game's %s window - the new card shows it",
                 kind == Kind::Identify ? "magnifier's" : kind == Kind::Menu ? "skill menu's" : "card's");
    }
    s.window = window;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.padNow = uapad::runtime::look().shown;
    s.entries = readList(s, window);
    s.show.selected = chosenOf(window, kind);
    if (s.show.selected >= int(s.entries.size())) s.show.selected = -1;
    s.layout = pc::layout(targetWidth, targetHeight, s.scale, s.entries.size());
    s.show.scroll = std::clamp(s.show.scroll, 0, pc::mostScroll(s.layout, s.entries.size()));
    s.show.hover = -1;
    s.show.hoverPart = 0;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
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
    if (key != s.key) {
        s.key = key;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
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
        s.show.scroll = std::clamp(s.show.scroll + by, 0, pc::mostScroll(s.layout, s.entries.size()));
        s.key.clear();
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    const pc::Hit h = pc::hitTest(s.layout, s.show.scroll, s.entries.size(), lx, ly);
    switch (h.part) {
        case pc::Part::Row:
            choose(s, h.row);
            if (m == WM_LBUTTONDBLCLK) press(s, pc::kOkButton);  // twice on a row: chosen and OK
            break;
        case pc::Part::Ok: press(s, pc::kOkButton); break;
        case pc::Part::Cancel: press(s, pc::kCancelButton); break;
        case pc::Part::None: break;
    }
    return true;
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
    x = s.layout.x;
    y = s.layout.y;
    w = s.layout.w;
    h = s.layout.h;
    return true;
}

}  // namespace uagfx::win::pickcardwin

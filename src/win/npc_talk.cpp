#include "win/npc_talk.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/npc_talk.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/client_npc.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/chat_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::npcwin {
namespace {

namespace nt = uagfx::npctalk;

// uaRO.exe's NPC windows (the pad's client_npc has their buttons): the text window (0x10) - its shown flag +0x38,
// the button Enter presses +0x58 (0x98 Next, 0x87 Close, 0x75 none), +0x88 set when a Next was pressed (the next
// words begin a new page, its SendMsg 0x1A); the menu (0x11) - its rows' option numbers (an int vector +0x68 .. +0x6C),
// its list (+0x74) and the list's chosen row (+0x64); the number field (0x38)
constexpr uint32_t kSayWnd = 0x727F7C, kMenuWnd = 0x727F80, kNumberWnd = 0x728038;
constexpr uint32_t kSayClass = 0x698CF0, kMenuClass = 0x699080, kNumberClass = 0x6989F8;
constexpr uint32_t kShown = 0x38, kDefaultButton = 0x58, kNewPage = 0x88;
constexpr uint32_t kRowsBegin = 0x68, kRowsEnd = 0x6C, kList = 0x74, kListChosen = 0x64;
constexpr int kBtnNext = 0x98, kBtnClose = 0x87;

struct State {
    WindowShadow shadow;
    bool on = false, shown = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    unsigned codePage = 0;
    uint32_t npc = 0;
    std::vector<std::string> lines, options;  // as the server sent them (the game's code page)
    nt::Talk talk;
    nt::Show show;
    nt::Layout layout;
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

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

uint32_t window(uint32_t slot, uint32_t cls) {
    const uint32_t w = read32(slot);
    return w && read32(w) == cls ? w : 0;
}

void holdGame(bool hidden) {  // the game's text and menu windows out of sight while ours shows
    for (const auto& [slot, cls] : {std::pair{kSayWnd, kSayClass}, std::pair{kMenuWnd, kMenuClass}})
        if (const uint32_t w = window(slot, cls)) {
            const uint32_t want = hidden ? 0 : 1;
            if (read32(w + kShown) != want) write32(w + kShown, want);
        }
}

std::vector<std::string> tokens(const std::string& text) {  // as the game's strtok splits the menu: no empty ones
    std::vector<std::string> out;
    size_t from = 0;
    while (from <= text.size()) {
        size_t end = text.find(':', from);
        if (end == std::string::npos) end = text.size();
        if (end > from) out.push_back(text.substr(from, end - from));
        from = end + 1;
    }
    return out;
}

nt::Texts texts() {
    nt::Texts t;
    t.next = say(settings::Text::NpcNext);
    t.close = say(settings::Text::SkillsPadClose);
    t.keys = say(settings::Text::NpcKeys);
    t.server = say(settings::Text::NpcServer);
    t.choose = say(settings::Text::NpcChoose);
    t.ok = "OK";
    return t;
}

float measure(const std::string& utf8Text, float px, bool bold) {
    return float(wordsWidth(utf8Wide(utf8Text), px, bold));
}

std::vector<Words> gdiWords(const std::vector<nt::Words>& in) {
    std::vector<Words> out;
    for (const nt::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == nt::Align::Right    ? DT_RIGHT
                                 : w.align == nt::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("NPC", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    nt::paint(s.picture, s.layout, s.talk, s.show);
    drawWords(s.picture, gdiWords(nt::words(s.layout, s.talk, s.show, texts(), measure)));
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
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Npc, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Npc, nullptr, nullptr, false);
    s.shown = false;
}

bool choose(State& s, int row, const char* by) {
    const uint32_t menu = window(kMenuWnd, kMenuClass);
    const uint32_t list = menu ? read32(menu + kList) : 0;
    if (!list || row < 0 || row >= int(s.talk.options.size())) return false;
    write32(list + kListChosen, uint32_t(row));
    const bool done = uapad::clientnpc::answer(uapad::DialogCommand::MenuOk);
    if (s.told++ < 20) LOG_INFO("npc: %s chose the option %d %s", by, row + 1, done ? "(the menu's own OK)" : "- not");
    return done;
}

bool press(State& s, uapad::DialogCommand c, const char* by) {
    const bool done = uapad::clientnpc::answer(c);
    if (s.told++ < 20)
        LOG_INFO("npc: %s - %s %s", by,
                 c == uapad::DialogCommand::Next        ? "next"
                 : c == uapad::DialogCommand::Close     ? "close"
                 : c == uapad::DialogCommand::MenuOk    ? "the menu's OK"
                 : c == uapad::DialogCommand::MenuCancel ? "the menu's cancel"
                                                         : "up or down",
                 done ? "(the game's own button)" : "- not done");
    return done;
}

void closeOrCancel(State& s, const char* by) {
    press(s, window(kMenuWnd, kMenuClass) ? uapad::DialogCommand::MenuCancel : uapad::DialogCommand::Close, by);
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    if (const auto* m = std::get_if<packets::NpcSay>(&e)) {
        const uint32_t sayWin = window(kSayWnd, kSayClass);
        if (!sayWin || m->npc != s.npc || read32(sayWin + kNewPage)) s.lines.clear();  // a new talk or page
        s.npc = m->npc;
        s.lines.push_back(m->text);
        if (s.lines.size() > 40) s.lines.erase(s.lines.begin());
    } else if (const auto* m = std::get_if<packets::NpcMenu>(&e)) {
        s.npc = m->npc;
        s.options = tokens(m->text);
    }
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        if (!on) holdGame(false);
        LOG_INFO("npc: [interface] npc_talk = %d", on ? 1 : 0);
    }
    const uint32_t sayWin = inGameWorld() ? window(kSayWnd, kSayClass) : 0;
    if (!sayWin) {
        s.lines.clear();
        s.options.clear();
    }
    if (!on || !sayWin || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    holdGame(true);
    const float scale = uistyle::readingScale(k);
    s.k = float(k);
    s.scale = scale;
    s.codePage = codePage;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    nt::Talk t;
    for (const std::string& line : s.lines) t.lines.push_back(utf8Of(widened(line, codePage)));
    if (const uint32_t menu = window(kMenuWnd, kMenuClass)) {
        const uint32_t begin = read32(menu + kRowsBegin), end = read32(menu + kRowsEnd);
        for (uint32_t at = begin; begin && at < end && at < begin + 4 * 64; at += 4) {
            const int idx = int(read32(at));
            t.options.push_back(idx >= 0 && idx < int(s.options.size()) ? utf8Of(widened(s.options[size_t(idx)],
                                                                                         codePage))
                                                                         : "#" + std::to_string(idx + 1));
        }
        const uint32_t list = read32(menu + kList);
        t.selected = list ? int(read32(list + kListChosen)) : -1;
    } else {
        const int def = int(read32(sayWin + kDefaultButton));
        t.button = def == kBtnNext ? nt::Button::Next : def == kBtnClose ? nt::Button::Close : nt::Button::None;
    }
    const std::string map = readGameMap();
    t.place = map;
    s.talk = nt::named(t);
    const uapad::runtime::Look pad = uapad::runtime::look();
    s.show.pad = pad.shown;
    s.show.sony = pad.sony;
    const nt::Texts tx = texts();
    s.layout = nt::layout(targetWidth / 2, targetHeight - int(std::lround(200 * scale)), scale, s.talk, s.show, tx,
                          measure);
    s.show.hoverOption = -1;
    s.show.hoverButton = s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const nt::Hit h = nt::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == nt::Part::Option) s.show.hoverOption = h.index;
        s.show.hoverButton = h.part == nt::Part::Button;
        s.show.hoverClose = h.part == nt::Part::Close;
    }
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' + std::to_string(scale) +
                      ';' + std::to_string(s.show.hoverOption) + (s.show.hoverButton ? "b" : "") +
                      (s.show.hoverClose ? "x" : "") + (s.show.pad ? (s.show.sony ? "S" : "X") : "") + ';' +
                      std::to_string(s.talk.selected) + ';' + std::to_string(int(s.talk.button)) + ';' + s.talk.name +
                      ';' + s.talk.place + ';';
    for (const std::string& l : s.talk.lines) key += l + '\n';
    for (const std::string& o : s.talk.options) key += o + '|';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {
        s.key = drawn;
        render(s);
    }
    if (!s.shown && s.told < 20) {
        ++s.told;
        LOG_INFO("npc: the talk in the interface's window - %zu line(s), %zu option(s), the NPC %u",
                 s.talk.lines.size(), s.talk.options.size(), s.npc);
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

bool shown() { return state().shown; }

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    (void)wp;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const nt::Hit h = nt::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if (h.part == nt::Part::None || m == WM_MOUSEMOVE) return false;
    result = 0;
    if (m != WM_LBUTTONDOWN) return true;
    switch (h.part) {
        case nt::Part::Option: choose(s, h.index, "a click"); break;
        case nt::Part::Button:
            press(s, s.talk.button == nt::Button::Next ? uapad::DialogCommand::Next : uapad::DialogCommand::Close,
                  "a click");
            break;
        case nt::Part::Close: closeOrCancel(s, "its close"); break;
        default: break;
    }
    s.key.clear();
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (m != WM_KEYDOWN || !s.shown || window(kNumberWnd, kNumberClass) || !chatwin::inputEmpty()) return false;
    const bool menu = window(kMenuWnd, kMenuClass) != 0;
    int digit = -1;
    if (wp >= '1' && wp <= '9') digit = int(wp - '1');
    if (wp >= VK_NUMPAD1 && wp <= VK_NUMPAD9) digit = int(wp - VK_NUMPAD1);
    if (digit >= 0) {
        if (!menu || digit >= int(s.talk.options.size())) return false;
        choose(s, digit, "a key");
        return true;
    }
    switch (wp) {
        case VK_RETURN:
            if (menu)
                press(s, uapad::DialogCommand::MenuOk, "Enter");
            else if (s.talk.button != nt::Button::None)
                press(s, s.talk.button == nt::Button::Next ? uapad::DialogCommand::Next : uapad::DialogCommand::Close,
                      "Enter");
            else
                return false;
            return true;
        case VK_ESCAPE: closeOrCancel(s, "Esc"); return true;
        case VK_UP:
        case VK_DOWN:
            if (!menu) return false;
            press(s, wp == VK_UP ? uapad::DialogCommand::MenuUp : uapad::DialogCommand::MenuDown, "an arrow");
            return true;
        default: return false;
    }
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && s.shown && inGameWorld()) holdGame(true);
}

}  // namespace uagfx::win::npcwin

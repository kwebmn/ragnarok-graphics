#include "win/char_select.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "core/char_block.hpp"
#include "core/char_select.hpp"
#include "core/game_camera.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/head_picture.hpp"
#include "win/login_card.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::charselectwin {
namespace {

namespace cs = uagfx::charselect;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the character select - where the manager keeps it, its class's vtable, its SendMsg; the place chosen,
// the page, the pages (1, or 3 - its constructor FUN_00458960), the places' controls; the game mode (its SendMsg at its
// vtable + 0x14: message 8 with a place's number looks for the character on it - CHARACTER_INFO + 0x68 - 0 none,
// FUN_005a23e0); the account's sex (as the character block reads it)
constexpr uint32_t kWindowMgr = 0x727ED8, kSelectAt = kWindowMgr + 0x20C, kSelectClass = 0x697BA0;
constexpr uint32_t kSelectSend = 0x45AE20, kChosen = 0x8F0, kPage = 0x8F4, kPages = 0x8F8, kPlaces = 0x918;
constexpr uint32_t kRect = 0x14, kSendMsgSlot = 0x94, kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kSex = 0x772DEC;
constexpr int kButtonMsg = 6, kPlaceClick = 0x8A, kInfoMsg = 8, kAway = -4000;
// CHARACTER_INFO (108 bytes): the job level + 0x10, HP + 0x2A, its most + 0x2C, SP + 0x2E, its most + 0x30, the job
// + 0x34, the hair + 0x36, the level + 0x3A, the hair's colour + 0x46, the name + 0x4A (24), the six stats + 0x62
constexpr size_t kInfoSize = 108;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using ModeMsgFn = uint32_t(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false, moved = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, oldX = 0;
    uint32_t window = 0;
    std::vector<cs::Slot> slots;
    std::map<std::tuple<int, int, int, int>, panel::Picture> faces;  // by sex, hair, colour, side
    cs::Show show;
    cs::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    ULONGLONG clickMs = 0;  // the last click on a place (two on it: into the game - the window may get no double click)
    int clickPlace = -1;
    accounts::LastChar chosen;  // 3.0.11-beta.175: the place chosen as last seen (the shelf's last character)
    std::string wanted;         // 3.0.11-beta.176: «Одразу в гру»'s character (UTF-8)
    ULONGLONG wantedAt = 0, stepAt = 0;
    bool makeWanted = false;  // 3.0.11-beta.181: a character made on the first empty place
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

uint32_t gameSelect() {
    const uint32_t w = read32(kSelectAt);
    return w && read32(w) == kSelectClass && read32(read32(w) + kSendMsgSlot) == kSelectSend ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.moved = false;
        return;
    }
    const int32_t x = int32_t(read32(window + kRect));
    if (on && x != kAway) {
        s.oldX = x;
        write32(window + kRect, uint32_t(kAway));
        if (!s.moved) LOG_INFO("characters: the game's character select off the screen for the new one");
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        s.moved = false;
    }
}

uint16_t u16(const uint8_t* p, size_t at) { return uint16_t(p[at] | p[at + 1] << 8); }

// The page's three characters as the game mode gives them
std::vector<cs::Slot> slotsOf(State& s, int page) {
    std::vector<cs::Slot> out(3);
    const uint32_t mode = read32(kModeNow);
    const uint32_t send = mode ? read32(read32(mode) + kModeMsgSlot) : 0;
    if (!send) return out;
    const int sex = read32(kSex) != 0 ? 1 : 0;
    const int side = cs::faceSide(s.scale);
    for (int i = 0; i < 3; ++i) {
        const uint32_t info = reinterpret_cast<ModeMsgFn>(uintptr_t(send))(mode, kInfoMsg, page * 3 + i, 0, 0);
        uint8_t raw[kInfoSize] = {};
        if (!info || !readGame(info, raw, sizeof raw)) continue;
        char name[25] = {};
        std::memcpy(name, raw + 0x4A, 24);
        if (!name[0]) continue;
        cs::Slot& sl = out[size_t(i)];
        sl.used = true;
        sl.name = utf8Of(widened(name, s.codePage));
        const int job = u16(raw, 0x34);
        sl.job = charblock::jobName(job);
        if (sl.job.empty()) sl.job = "#" + std::to_string(job);
        uint32_t jobLevel = 0;
        std::memcpy(&jobLevel, raw + 0x10, 4);
        sl.jobLevel = int(jobLevel);
        sl.level = u16(raw, 0x3A);
        sl.hp = u16(raw, 0x2A);
        sl.maxHp = u16(raw, 0x2C);
        sl.sp = u16(raw, 0x2E);
        sl.maxSp = u16(raw, 0x30);
        for (int k = 0; k < 6; ++k) sl.stats[k] = raw[0x62 + k];
        const auto key = std::make_tuple(sex, int(u16(raw, 0x36)), int(u16(raw, 0x46)), side);
        auto f = s.faces.find(key);
        if (f == s.faces.end()) {
            panel::Picture p;
            headpicture::make(sex, u16(raw, 0x36), u16(raw, 0x46), side, p);
            f = s.faces.emplace(key, std::move(p)).first;
        }
        sl.face = f->second.width > 0 ? &f->second : nullptr;
    }
    return out;
}

cs::Texts texts() {
    cs::Texts t;
    t.title = say(settings::Text::CharsTitle);
    t.enter = say(settings::Text::CharsEnter);
    t.make = say(settings::Text::CharsMake);
    t.remove = say(settings::Text::CharsRemove);
    t.back = say(settings::Text::CharsBack);
    t.empty = say(settings::Text::CharsEmpty);
    t.makeHint = say(settings::Text::CharsMakeHint);
    const char* const stats[6] = {"STR", "AGI", "VIT", "INT", "DEX", "LUK"};
    for (int i = 0; i < 6; ++i) t.stats[i] = stats[i];
    return t;
}

std::vector<Words> gdiWords(const std::vector<cs::Words>& in) {
    std::vector<Words> out;
    for (const cs::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == cs::Align::Right    ? DT_RIGHT
                                 : w.align == cs::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the characters", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    cs::paint(s.picture, s.layout, s.slots, s.show);
    drawWords(s.picture, gdiWords(cs::words(s.layout, s.slots, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::CharSelect, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::CharSelect, nullptr, nullptr, false);
    s.shown = false;
}

// 3.0.11-beta.175: the chosen place's character as the game mode gives it (its CHARACTER_INFO), slot -1 none
accounts::LastChar chosenOf(const State& s, uint32_t window) {
    accounts::LastChar c;
    if (read32(camera::kModeMgr + camera::kModeType) != 0) return c;  // the login mode's message 8 alone
    const int page = std::clamp(int(read32(window + kPage)), 0, 2);
    const int place = std::clamp(int(read32(window + kChosen)), 0, 2);
    const uint32_t mode = read32(kModeNow);
    const uint32_t send = mode ? read32(read32(mode) + kModeMsgSlot) : 0;
    const uint32_t info =
        send ? reinterpret_cast<ModeMsgFn>(uintptr_t(send))(mode, kInfoMsg, page * 3 + place, 0, 0) : 0;
    uint8_t raw[kInfoSize] = {};
    if (!info || !readGame(info, raw, sizeof raw)) return c;
    char name[25] = {};
    std::memcpy(name, raw + 0x4A, 24);
    if (!name[0]) return c;
    c.name = utf8Of(widened(name, s.codePage));
    c.jobId = u16(raw, 0x34);
    c.job = charblock::jobName(c.jobId);
    c.level = u16(raw, 0x3A);
    c.slot = page * 3 + place;
    c.sex = read32(kSex) != 0 ? 1 : 0;
    c.hair = u16(raw, 0x36);
    c.hairColour = u16(raw, 0x46);
    return c;
}

// 3.0.11-beta.176: the name in a place of the account (any page), UTF-8; "" an empty place
std::string nameAt(const State& s, int index) {
    const uint32_t mode = read32(kModeNow);
    const uint32_t send = mode ? read32(read32(mode) + kModeMsgSlot) : 0;
    const uint32_t info = send ? reinterpret_cast<ModeMsgFn>(uintptr_t(send))(mode, kInfoMsg, index, 0, 0) : 0;
    char name[25] = {};
    if (!info || !readGame(info + 0x4A, name, 24)) return {};
    return name[0] ? utf8Of(widened(name, s.codePage)) : std::string();
}

void send(State& s, uint32_t sender, int id, const char* what) {
    const uint32_t window = gameSelect();
    if (!window) return;
    if (s.told++ < 40) LOG_INFO("characters: %s - the game's 0x%X", what, unsigned(id));
    reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)))(window, sender, kButtonMsg, id, 0, 0);
    s.key.clear();
}

void choose(State& s, int place) {
    const uint32_t window = gameSelect();
    if (!window || place < 0 || place > 2) return;
    const uint32_t control = read32(window + kPlaces + 4 * uint32_t(place));
    if (control) send(s, control, kPlaceClick, "a place chosen");
}

// 3.0.11-beta.176 («Одразу в гру»): a step a frame toward the wanted character - its page, its place, «Увійти»
void wantFrame(State& s, uint32_t window) {
    if (read32(camera::kModeMgr + camera::kModeType) != 0) return;  // the login mode's message 8 alone
    const ULONGLONG now = GetTickCount64();
    if (now - s.stepAt < 150) return;  // the game's window takes each press in its own time
    const int pages = std::clamp(int(read32(window + kPages)), 1, 3);
    std::vector<std::string> names(size_t(pages) * 3);
    for (int i = 0; i < pages * 3; ++i) names[size_t(i)] = nameAt(s, i);
    int at = -1;
    if (s.makeWanted) {  // 3.0.11-beta.181: the first empty place
        for (int i = 0; i < pages * 3 && at < 0; ++i)
            if (names[size_t(i)].empty()) at = i;
    } else {
        at = accounts::findSlot(names, s.wanted);
    }
    if (at < 0) {
        if (now - s.wantedAt > 1500) {
            LOG_INFO("characters: %s", s.makeWanted ? "no empty place to make a character in"
                                                    : "the saved character is not on this account - waiting");
            s.wanted.clear();
            s.makeWanted = false;
        }
        return;
    }
    s.stepAt = now;
    const int page = at / 3, place = at % 3, pageNow = int(read32(window + kPage));
    if (page != pageNow) {
        send(s, 0, cs::pageButton(page > pageNow), "a page turned («Одразу в гру»)");
    } else if (place != int(read32(window + kChosen))) {
        choose(s, place);
    } else if (s.makeWanted) {
        send(s, 0, cs::gameButton(cs::Button::Make), "«Створити» (the registration's character)");
        s.makeWanted = false;
    } else {
        send(s, 0, cs::gameButton(cs::Button::Enter), "«Увійти» («Одразу в гру»)");
        LOG_INFO("characters: «Одразу в гру» - the saved character entered (place %d)", at);
        s.wanted.clear();
    }
}

void press(State& s, cs::Button b) {
    const int chosen = s.show.selected;
    const cs::Slot none;
    const cs::Slot& sl = chosen >= 0 && chosen < int(s.slots.size()) ? s.slots[size_t(chosen)] : none;
    if (!cs::allowed(sl, b)) return;
    const char* const what[4] = {"«Увійти»", "«Створити»", "«Видалити»", "«Назад»"};
    send(s, 0, cs::gameButton(b), what[int(b)]);
    if (b == cs::Button::Enter)  // 3.0.11-beta.261: the veil till the map shows, not the game's loading
        loginwin::veilOver(sl.name, sl.job + (sl.level > 0 ? " \xC2\xB7 " + std::to_string(sl.level) : ""));
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("characters: [interface] char_select = %d", on ? 1 : 0);
    }
    const uint32_t window = gameSelect();
    hold(s, window, on);
    if (window) s.chosen = chosenOf(s, window);  // 3.0.11-beta.175: the shelf's last character
    if (window && (!s.wanted.empty() || s.makeWanted)) wantFrame(s, window);  // 3.0.11-beta.176: «Одразу в гру»
    if (!window) {
        s.wanted.clear();
        s.makeWanted = false;
    }
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    if (!s.shown || window != s.window)  // the account's sex logged: the faces' (is it known here already?)
        LOG_INFO("characters: the game's character select - the new one shows it (the account's sex %u, %u pages)",
                 unsigned(read32(kSex)), unsigned(read32(window + kPages)));
    s.window = window;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.show.pages = std::clamp(int(read32(window + kPages)), 1, 3);
    s.show.page = std::clamp(int(read32(window + kPage)), 0, s.show.pages - 1);
    s.show.selected = std::clamp(int(read32(window + kChosen)), 0, 2);
    s.slots = slotsOf(s, s.show.page);
    s.layout = cs::layout(targetWidth, targetHeight, s.scale);
    s.show.hover = s.show.hoverButton = -1;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const cs::Hit h = cs::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == cs::Part::Slot) s.show.hover = h.index;
        if (h.part == cs::Part::Button) s.show.hoverButton = h.index;
    }
    std::string key = std::to_string(s.show.page) + ',' + std::to_string(s.show.selected) + ',' +
                      std::to_string(s.show.hover) + ',' + std::to_string(s.show.hoverButton) + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (const cs::Slot& sl : s.slots)
        key += ';' + sl.name + ',' + std::to_string(sl.level) + ',' + std::to_string(sl.hp) + (sl.face ? "f" : "");
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
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 14 * s.scale,
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
    if (m == WM_MOUSEWHEEL && s.show.pages > 1) {  // the wheel over it turns the pages
        float wx = 0, wy = 0;
        if (pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, wx, wy, nullptr) &&
            s.layout.glass.contains(wx - float(s.layout.x), wy - float(s.layout.y))) {
            send(s, 0, cs::pageButton(GET_WHEEL_DELTA_WPARAM(wp) < 0), "a page (the wheel)");
            result = 0;
            return true;
        }
        return false;
    }
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        const cs::Hit h = cs::hitTest(s.layout, lx, ly);
        if (h.part == cs::Part::Slot) {
            const ULONGLONG now = GetTickCount64();
            const bool twice = h.index == s.show.selected && h.index == s.clickPlace &&
                               (m == WM_LBUTTONDBLCLK || now - s.clickMs <= GetDoubleClickTime());
            s.clickMs = now;
            s.clickPlace = h.index;
            if (h.index != s.show.selected) choose(s, h.index);
            else if (twice && h.index < int(s.slots.size())) {  // twice: into the game (an empty place: make one)
                s.clickPlace = -1;
                press(s, s.slots[size_t(h.index)].used ? cs::Button::Enter : cs::Button::Make);
            }
        } else if (h.part == cs::Part::Button) {
            press(s, cs::Button(h.index));
        } else if (h.part == cs::Part::PageLeft || h.part == cs::Part::PageRight) {
            send(s, 0, cs::pageButton(h.part == cs::Part::PageRight), "a page");
        }
    }
    return true;
}

bool shown() { return state().shown; }

bool gameUp() { return gameSelect() != 0; }

bool makeFirst() {
    State& s = state();
    if (!gameSelect()) return false;
    s.makeWanted = true;
    s.wanted.clear();
    s.wantedAt = GetTickCount64();
    s.stepAt = 0;
    return true;
}

bool enterByName(const std::string& name) {
    State& s = state();
    if (!gameSelect() || name.empty()) return false;
    s.wanted = name;
    s.wantedAt = GetTickCount64();
    s.stepAt = 0;
    return true;
}

accounts::LastChar entered() { return state().chosen; }

void beforeGameDraw() {
    State& s = state();
    if (s.on) hold(s, gameSelect(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    switch (c) {
        case PadCommand::Left: choose(s, std::max(0, s.show.selected - 1)); break;
        case PadCommand::Right: choose(s, std::min(2, s.show.selected + 1)); break;
        case PadCommand::Enter: press(s, cs::Button::Enter); break;
        case PadCommand::Back: press(s, cs::Button::Back); break;
        case PadCommand::Make: press(s, cs::Button::Make); break;
        case PadCommand::PageBefore: send(s, 0, cs::pageButton(false), "a page"); break;
        case PadCommand::PageAfter: send(s, 0, cs::pageButton(true), "a page"); break;
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

}  // namespace uagfx::win::charselectwin

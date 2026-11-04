#include "win/emote_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/emote_card.hpp"
#include "core/settings/strings.hpp"
#include "core/sprite_file.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/updates_hook.hpp"
#include "win/window_shadow.hpp"

namespace uagfx::win::emotewin {
namespace {

namespace ec = uagfx::emotecard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the emotion list (where the manager keeps it, its class's vtable, its SendMsg; its shown flag, the
// emotion it uses, its list of commands - the first, past the last) and its use (thiscall: x, y - a y past its title
// uses + 0x60's); the chat window (where, its class, its SendMsg: 6 / 0x76 its Enter) and the macros' window
constexpr uint32_t kWindowMgr = 0x727ED8, kEmoteAt = kWindowMgr + 0x1AC, kEmoteClass = 0x69C320;
constexpr uint32_t kEmoteSend = 0x4521D0, kEmoteUse = 0x452540, kSendMsgSlot = 0x94, kUseSlot = 0x68;
constexpr uint32_t kShown = 0x38, kChosen = 0x60, kFirst = 0x6C, kPast = 0x70;
constexpr uint32_t kChatAt = kWindowMgr + 0x84, kChatClass = 0x697D78, kChatSend = 0x45EF90;
constexpr uint32_t kMacrosAt = kWindowMgr + 0x1A8;
constexpr int kButtonMsg = 6, kCloseButton = 0x87, kChatEnter = 0x76, kPastTitle = 0xD9;
// The pictures: 이팩트\emotion.spr by 이팩트\emotion_index3.act (CP949), an action each in the list's order
const std::string kEffects = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\";
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using UseFn = void(__attribute__((thiscall)) *)(uint32_t self, int x, int y);

struct State {
    WindowShadow shadow;
    bool on = false, shown = false, hid = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    std::vector<std::string> commands;
    std::vector<panel::Picture> icons;
    int iconSide = 0;
    ec::Show show;
    ec::Layout layout;
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

uint32_t gameEmotes() {
    const uint32_t w = read32(kEmoteAt);
    return w && read32(w) == kEmoteClass && read32(kEmoteClass + kSendMsgSlot) == kEmoteSend &&
                   read32(kEmoteClass + kUseSlot) == kEmoteUse
               ? w
               : 0;
}

bool readFile(const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;  // as the game finds it (its zips too)
    static std::unique_ptr<gamefiles::Reader> files;
    if (!files) files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

// The list's pictures at `side`, read once a size (the card's and the chat wheel's)
const std::vector<panel::Picture>& picturesAt(int side) {
    static std::map<int, std::vector<panel::Picture>> made;
    const auto it = made.find(side);
    if (it != made.end()) return it->second;
    std::vector<panel::Picture>& out = made[side];
    out.assign(size_t(ec::kEmotions), panel::Picture{});
    std::string spr, act;
    spritefile::Sprite sp;
    spritefile::Act ac;
    if (!readFile(kEffects + "emotion.spr", spr) || !readFile(kEffects + "emotion_index3.act", act) ||
        !spritefile::readSpr(spr, sp) || !spritefile::readAct(act, ac)) {
        LOG_INFO("emotions: the list's pictures not read (emotion.spr, emotion_index3.act) - the commands alone");
        return out;
    }
    int count = 0;
    for (int i = 0; i < ec::kEmotions; ++i) {
        spritefile::Image im;
        if (!spritefile::frameImage(sp, ac, size_t(i), 0, im)) continue;
        const spritefile::Image fit = spritefile::fitted(im, side, side);
        panel::Picture& p = out[size_t(i)];
        p.width = fit.w;
        p.height = fit.h;
        p.px = fit.argb;
        ++count;
    }
    LOG_INFO("emotions: %d of the list's %d pictures at %d px (emotion.spr by emotion_index3.act)", count,
             ec::kEmotions, side);
    return out;
}

void loadIcons(State& s, int side) {
    if (side == s.iconSide && !s.icons.empty()) return;
    s.iconSide = side;
    s.icons = picturesAt(side);
}

void readCommands(State& s, uint32_t window) {
    s.commands.clear();
    const uint32_t first = read32(window + kFirst), past = read32(window + kPast);
    if (!first || past <= first || (past - first) / 4 > 64) return;
    for (uint32_t at = first; at < past; at += 4) {
        char buf[33] = {};
        const uint32_t words = read32(at);
        if (!words || !readGame(words, buf, 32)) buf[0] = 0;
        s.commands.push_back(utf8Of(widened(buf, s.codePage)));
    }
}

ec::Texts texts() {
    ec::Texts t;
    t.title = say(settings::Text::EmoteTitle);
    t.hint = say(settings::Text::EmoteHint);
    return t;
}

std::vector<Words> gdiWords(const std::vector<ec::Words>& in) {
    std::vector<Words> out;
    for (const ec::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == ec::Align::Right    ? DT_RIGHT
                                 : w.align == ec::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("the emotions", s.layout.w, s.layout.h);
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;
    ec::paint(s.picture, s.layout, s.show, s.icons);
    drawWords(s.picture, gdiWords(ec::words(s.layout, s.commands, s.show, texts())));
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
    uapad::gamewindow::setBlocked(Blocker::Emotes, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Emotes, nullptr, nullptr, false);
    s.shown = false;
}

// The emotion `i` as the game's list uses it; `send`: the chat's line sent after it, as Enter does
void use(State& s, int i, bool send) {
    const uint32_t w = gameEmotes();
    if (!w || i < 0 || i >= int(s.commands.size())) return;
    write32(w + kChosen, uint32_t(i));
    reinterpret_cast<UseFn>(uintptr_t(kEmoteUse))(w, 0, kPastTitle);
    const char* what = s.commands[size_t(i)].c_str();
    if (!send) {
        if (s.told++ < 30) LOG_INFO("emotions: %s into the chat's line (a click, as the game's list)", what);
        return;
    }
    if (read32(kMacrosAt)) {  // the game's list gave it to the macros' field - no chat line to send
        if (s.told++ < 30) LOG_INFO("emotions: %s into the macros' field (that window open) - not sent", what);
        return;
    }
    const uint32_t chat = read32(kChatAt);
    if (!chat || read32(chat) != kChatClass || read32(kChatClass + kSendMsgSlot) != kChatSend) {
        if (s.told++ < 30) LOG_INFO("emotions: %s - no chat window to send it from", what);
        return;
    }
    reinterpret_cast<SendMsgFn>(uintptr_t(kChatSend))(chat, 0, kButtonMsg, kChatEnter, 0, 0);
    if (s.told++ < 30) LOG_INFO("emotions: %s sent at once (a double click: the chat's Enter after it)", what);
}

}  // namespace

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("emotions: [interface] emotes = %d", on ? 1 : 0);
    }
    const uint32_t window = gameEmotes();
    if (window) {  // the game's window hidden while ours shows; back when ours is off
        if (on && read32(window + kShown) && write32(window + kShown, 0)) s.hid = true;
        else if (!on && s.hid && write32(window + kShown, 1)) s.hid = false;
    } else {
        s.hid = false;
    }
    if (!on || !window || !inGameWorld() || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) LOG_INFO("emotions: the game's emotion list (Alt+L) - the new card shows it");
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readCommands(s, window);
    loadIcons(s, int(std::lround(ec::kIconSide * s.scale)));
    s.layout = ec::layout(targetWidth, targetHeight, s.scale);
    s.show = ec::Show{};
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        s.show.hover = ec::cellAt(s.layout, lx, ly);
        s.show.hoverClose = ec::closeAt(s.layout, lx, ly);
    }
    std::string key = std::to_string(s.show.hover) + (s.show.hoverClose ? "x" : "") + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ',' + std::to_string(s.layout.w) +
                      ';' + std::to_string(s.iconSide) + '#' + std::to_string(uistyle::generation());
    for (const std::string& c : s.commands) key += ';' + c;
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
    (void)wp;
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, nullptr)) return false;
    const float lx = tx - float(s.layout.x), ly = ty - float(s.layout.y);
    if (!s.layout.glass.contains(lx, ly)) return false;
    result = 0;
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    if (ec::closeAt(s.layout, lx, ly)) {
        close();
        return true;
    }
    const int i = ec::cellAt(s.layout, lx, ly);
    if (i >= 0) use(s, i, m == WM_LBUTTONDBLCLK);
    return true;
}

bool shown() { return state().shown; }

bool close() {
    State& s = state();
    const uint32_t w = gameEmotes();
    if (!s.shown || !w) return false;
    if (s.told++ < 30) LOG_INFO("emotions: closed (the window's 0x87)");
    reinterpret_cast<SendMsgFn>(uintptr_t(kEmoteSend))(w, 0, kButtonMsg, kCloseButton, 0, 0);
    s.key.clear();
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (!s.on) return;
    const uint32_t w = gameEmotes();
    if (w && read32(w + kShown) && write32(w + kShown, 0)) s.hid = true;
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

const std::vector<panel::Picture>& emotionPictures(int side) { return picturesAt(side); }

int emotionNumber(const std::string& command) {
    using MsgStringFn = const char*(__cdecl*)(int line);
    static std::vector<std::string> list;
    if (list.empty()) {
        const auto add = [](int from, int to) {
            for (int i = from; i <= to; ++i) {
                const char* t = reinterpret_cast<MsgStringFn>(uintptr_t(0x504FB0))(i);
                list.push_back(t ? t : "");
            }
        };
        add(0x220, 0x23D);
        add(0x304, 0x30F);
        add(0x3B1, 0x3B7);
    }
    const auto it = std::find(list.begin(), list.end(), command);
    return it == list.end() ? -1 : int(it - list.begin());
}

}  // namespace uagfx::win::emotewin

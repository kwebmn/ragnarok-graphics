#include "win/buff_column.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/buff_panel.hpp"
#include "core/buff_timers.hpp"
#include "core/layout.hpp"
#include "core/pack_source.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/buff_overlay.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/minimap_proto.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/updates_hook.hpp"

namespace uagfx::win::buffcolumn {
namespace {

namespace bp = uagfx::buffpanel;

// uaRO.exe's CSession::IconImage (thiscall, the session in ECX; the icon and the layout's pass 0-4; ret 8): the icon's
// .tga in that pass, or none - and its one call in the icons' layout (FUN_005d56a0), the bytes round it
using ImageFn = const char*(__attribute__((thiscall)) *)(void* session, uint32_t icon, uint32_t pass);
constexpr uint32_t kIconImage = 0x648BC0, kSession = 0x771148, kLayoutCall = 0x5D5785;
constexpr uint32_t kAroundAt = 0x5D577E;  // push eax (pass); push ecx (icon); mov ecx, session; call; mov edi, eax ...
const uint8_t kAround[] = {0x50, 0x51, 0xB9, 0x48, 0x11, 0x77, 0x00, 0xE8, 0x36,
                           0x34, 0x07, 0x00, 0x8B, 0xF8, 0x85, 0xFF, 0x74, 0xA7};
const uint8_t kImageCode[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x83, 0xF8, 0x03,
                              0x0F, 0x87, 0x95, 0x05, 0x00, 0x00};
constexpr uint32_t kIconsBegin = 0x728484, kIconsEnd = 0x728488;  // the player's icons (ints), FUN_00593d00's
constexpr size_t kMostIcons = 64;
// The player's icon effect (FUN_005b71a0: player + 0x428 = the effect 0xa4 laying the icons out) and the message that
// lays them out again - FUN_00593d00 sends it on each status icon packet: mode + 0xC4 the world, + 0x3C the player
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
constexpr uint32_t kWorld = 0xC4, kPlayer = 0x3C, kIconEffect = 0x428;
constexpr int kRelayout = 0x2C;
constexpr ULONGLONG kRepaintMs = 100;  // the times and the rings painted again at most this often

bool g_hooked = false, g_hide = false;

// The layout's look at an icon's picture: none while the driver's column shows them - the game makes no icons
const char* __attribute__((thiscall)) layoutImage(void* session, uint32_t icon, uint32_t pass) {
    if (g_hide) return nullptr;
    return reinterpret_cast<ImageFn>(uintptr_t(kIconImage))(session, icon, pass);
}

// A call's 4 bytes after its E8 to `to`
bool redirect(uint32_t call, const void* to) {
    auto* rel = reinterpret_cast<uint8_t*>(uintptr_t(call + 1));
    DWORD old = 0;
    if (!VirtualProtect(rel, 4, PAGE_EXECUTE_READWRITE, &old)) return false;
    const int32_t by = int32_t(reinterpret_cast<uintptr_t>(to) - (uintptr_t(call) + 5));
    std::memcpy(rel, &by, sizeof by);
    DWORD unused = 0;
    VirtualProtect(rel, 4, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), rel, 4);
    return true;
}

bool sameBytes(uint32_t at, const uint8_t* bytes, size_t size) {
    uint8_t got[32] = {};
    SIZE_T n = 0;
    return size <= sizeof got &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(at)), got, size, &n) &&
           n == size && std::memcmp(got, bytes, size) == 0;
}

// The game's icons laid out again at once (hidden, or back): the icon effect's own message, as the game sends it
bool relayout() {
    const uint32_t mode = readGameMode();
    uint32_t world = 0, player = 0, effect = 0, vtable = 0, fn = 0;
    if (!mode || !readGame(mode + kWorld, &world, 4) || !world || !readGame(world + kPlayer, &player, 4) || !player ||
        !readGame(player + kIconEffect, &effect, 4) || !effect || !readGame(effect, &vtable, 4) ||
        vtable < 0x401000 || vtable >= 0x800000 || !readGame(vtable + 8, &fn, 4) || fn < 0x401000 || fn >= 0x6A0000)
        return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(fn))(effect, 0, kRelayout, 1, 0, 0);
    return true;
}

struct Icon {  // a status icon's .tga as the game's files have it, and its pictures by side
    bool looked = false;
    std::vector<uint32_t> argb;
    int w = 0, h = 0;
    bool alpha = false;
    std::map<int, panel::Picture> sized;
};

struct GameIcon {
    uint16_t icon = 0;
    std::string file;  // effect\...tga (the game's code page)
};

struct State {
    BuffsLook look = BuffsLook::Off;
    bool shown = false, wantedTold = false, hookTold = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    bool stateRead = false, names = false;
    std::set<uint16_t> favourites;
    std::map<std::string, Icon> icons;  // by file
    std::unique_ptr<gamefiles::Reader> files;
    std::vector<GameIcon> game;
    std::vector<bp::Entry> entries;
    bp::Layout layout;
    int hover = -1;
    std::string shape, fill;  // what the picture shows: its parts, its times
    ULONGLONG paintedAt = 0;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int iconsLogged = 0, clicksLogged = 0;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kBuffsState), text)) return;
    if (const size_t at = text.find("names "); at != std::string::npos) s.names = std::atoi(text.c_str() + at + 6) != 0;
    if (const size_t at = text.find("favourites"); at != std::string::npos) {
        const char* p = text.c_str() + at + 10;
        while (*p && *p != '\n') {
            char* end = nullptr;
            const long v = std::strtol(p, &end, 10);
            if (end == p) {
                ++p;
                continue;
            }
            if (v >= 0 && v < 0x8000) s.favourites.insert(uint16_t(v));
            p = end;
        }
    }
}

void keepState(const State& s) {
    std::string text = "names " + std::string(s.names ? "1" : "0") + "\nfavourites";
    for (const uint16_t f : s.favourites) text += " " + std::to_string(f);
    text += "\n";
    const std::string path = layout::join(gameDir(), layout::kBuffsState);
    if (!writeFileAtomic(path, text)) LOG_INFO("buffs: %s cannot be written", path.c_str());
}

// The character's icons as the game lays them out: pass by pass, each pass in the list's order, one a key (0x2A is
// 0x0C's), those with a picture in no pass not shown
std::vector<GameIcon> gameIcons() {
    std::vector<GameIcon> out;
    uint32_t begin = 0, end = 0;
    if (!readGame(kIconsBegin, &begin, 4) || !readGame(kIconsEnd, &end, 4) || !begin || end < begin ||
        (end - begin) / 4 > kMostIcons)
        return out;
    std::vector<int32_t> list((end - begin) / 4);
    if (!list.empty() && !readGame(begin, list.data(), list.size() * 4)) return out;
    const auto image = reinterpret_cast<ImageFn>(uintptr_t(kIconImage));
    void* session = reinterpret_cast<void*>(uintptr_t(kSession));
    std::vector<int32_t> keys;
    for (uint32_t pass = 0; pass < 5; ++pass)
        for (const int32_t icon : list) {
            if (icon < 0 || icon > 0x7FFF) continue;
            const int32_t key = icon == 0x2A ? 0xC : icon;
            if (std::find(keys.begin(), keys.end(), key) != keys.end()) continue;
            const char* file = image(session, uint32_t(icon), pass);
            if (!file || !*file) continue;
            keys.push_back(key);
            out.push_back(GameIcon{uint16_t(icon), file});
        }
    return out;
}

// An icon's words: its skill's or item's in the server's data, a state's, its file's Latin name, its number
std::string nameOf(const GameIcon& g) {
    if (const char* n = buffs::name(g.icon); n && *n) return n;
    if (const char* n = bp::stateName(g.icon); *n) return n;
    std::string stem = g.file.substr(g.file.find_last_of("\\/") + 1);
    stem = stem.substr(0, stem.find('.'));
    if (stem.rfind("i_", 0) == 0 || stem.rfind("I_", 0) == 0) stem = stem.substr(2);
    const bool latin = !stem.empty() && std::all_of(stem.begin(), stem.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
    if (latin) {
        for (char& c : stem) c = c == '_' ? ' ' : c;
        return stem;
    }
    return "#" + std::to_string(g.icon);
}

bool readFile(State& s, const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;  // as the game finds it (its zips too)
    if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!s.files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

const panel::Picture* pictureOf(State& s, const GameIcon& g, int side) {
    Icon& ic = s.icons[g.file];
    if (!ic.looked) {
        ic.looked = true;
        std::string bytes;
        const std::string entry = "data\\texture\\" + g.file;
        const bool read = readFile(s, entry, bytes);
        if (!read || !pack::decodeTga(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), ic.argb, ic.w,
                                      ic.h, ic.alpha)) {
            ic.argb.clear();
            if (s.iconsLogged < 40) {
                ++s.iconsLogged;
                LOG_INFO("buffs: icon %u's picture %s %s - a dark tile in its place", g.icon, entry.c_str(),
                         read ? "not a .tga the driver reads" : "not found");
            }
        } else if (s.iconsLogged < 40) {
            ++s.iconsLogged;
            LOG_INFO("buffs: icon %u (%s) - %s %dx%d%s", g.icon, nameOf(g).c_str(), entry.c_str(), ic.w, ic.h,
                     ic.alpha ? "" : ", its magenta keyed out");
        }
    }
    if (ic.argb.empty() || side <= 0) return nullptr;
    auto it = ic.sized.find(side);
    if (it == ic.sized.end())
        it = ic.sized.emplace(side, bp::iconPicture(ic.argb, ic.w, ic.h, ic.alpha, side, 5 * s.scale)).first;
    return &it->second;
}

bp::Texts texts() {
    return bp::Texts{say(settings::Text::BuffsNames), say(settings::Text::BuffsHideNames),
                     say(settings::Text::BuffsMore), say(settings::Text::BuffsStar),
                     say(settings::Text::BuffsUnstar)};
}

std::vector<Words> gdiWords(const std::vector<bp::Words>& in) {
    std::vector<Words> out;
    for (const bp::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == bp::Align::Right    ? DT_RIGHT
                                 : w.align == bp::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    s.picture = panel::Picture{};
    s.picture.width = s.layout.w;
    s.picture.height = s.layout.h;
    if (s.layout.w <= 0 || s.layout.h <= 0) return;
    s.picture.px.assign(size_t(s.layout.w) * size_t(s.layout.h), 0u);
    const bp::Texts t = texts();
    bp::paint(s.picture, s.layout, s.entries, s.hover);
    bp::paintTip(s.picture, s.layout, s.entries, s.hover, t);
    drawWords(s.picture, gdiWords(bp::words(s.layout, s.entries, s.hover, t)));
    s.upload = true;
    s.paintedAt = GetTickCount64();
}

bool toTarget(const State& s, HWND game, int x, int y, float& tx, float& ty) {
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    tx = float(x) * float(s.targetW) / float(client.right);  // Windows' size of the window (uaRO.exe is not DPI-aware)
    ty = float(y) * float(s.targetH) / float(client.bottom);
    return true;
}

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the game's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& h = s.layout.hit;
    const float x0 = float(s.layout.x) + h.x, y0 = float(s.layout.y) + h.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + h.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + h.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Buffs, game, &r, false);
}

void unblock() { uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Buffs, nullptr, nullptr, false); }

void hide(State& s) {
    if (s.shown) unblock();
    s.shown = false;
    s.hover = -1;
}

// Hidden or back: the game's icons laid out again where they are shown
void setHide(bool hideThem) {
    if (hideThem == g_hide) return;
    g_hide = hideThem;
    const bool again = inGameWorld() && relayout();
    LOG_INFO("buffs: the game's own status icons %s (%s)", hideThem ? "hidden for the column" : "back",
             again ? "laid out again at once" : "at the game's next look at them: no character's icons now");
}

}  // namespace

bool installBuffIcons() {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    if (!knownGameBuild() || !sameBytes(kAroundAt, kAround, sizeof kAround) ||
        !sameBytes(kIconImage, kImageCode, sizeof kImageCode)) {
        LOG_INFO("buffs: the game's status icons are not where this build of uaRO.exe has them (0x%X) - [interface] "
                 "buffs shows nothing, the game's own icons stay",
                 kLayoutCall);
        return false;
    }
    if (!redirect(kLayoutCall, reinterpret_cast<const void*>(&layoutImage))) {
        LOG_INFO("buffs: the game's code cannot be changed (error %lu) - the game's own icons stay", GetLastError());
        return false;
    }
    g_hooked = true;
    done = 1;
    LOG_INFO("buffs: the game's status icons laid out through the driver (uaRO.exe 0x%X) - hidden while [interface] "
             "buffs shows its column",
             kLayoutCall);
    return true;
}

bool hidingGameIcons() { return g_hide; }

void frame(HWND game, BuffsLook look, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (look != s.look) {
        LOG_INFO("buffs: [interface] buffs = %s", buffsLookToken(look));
        s.look = look;
        s.shape.clear();
    }
    const bool wanted = look != BuffsLook::Off;
    if (wanted && !g_hooked && !s.hookTold) {
        s.hookTold = true;
        LOG_INFO("buffs: the column not shown - the game's own icons could not be hidden in this build");
    }
    setHide(wanted && g_hooked);
    if (!g_hide || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) {
        hide(s);
        return;
    }
    readState(s);
    const float scale = std::clamp(float(k), 0.75f, 6.0f);
    if (scale != s.scale) {
        for (auto& [file, ic] : s.icons) ic.sized.clear();  // the pictures at the new size
        s.shape.clear();
    }
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    // the entries: the game's icons, their kinds, words, times and pictures
    s.game = gameIcons();
    const uint64_t now = GetTickCount64();
    const buffs::Timers& timers = buffoverlay::timers();
    const std::vector<buffs::Timers::Running> running = timers.running();
    s.entries.clear();
    for (const GameIcon& g : s.game) {
        bp::Entry e;
        e.icon = g.icon;
        e.name = nameOf(g);
        e.kind = bp::debuff(g.icon)              ? bp::Kind::Debuff
                 : s.favourites.count(g.icon) > 0 ? bp::Kind::Favourite
                                                  : bp::Kind::Buff;
        const uint32_t left = timers.left(g.icon, now);
        e.time = buffs::label(left);
        e.warn = left > 0 && left < 10000;
        for (const buffs::Timers::Running& r : running)
            if (r.icon == g.icon && r.end > r.from && r.end > now)
                e.left = std::clamp(float(double(r.end - now) / double(r.end - r.from)), 0.0f, 1.0f);
        e.picture = pictureOf(s, g, bp::iconSide(e.kind, scale));
        s.entries.push_back(e);
    }
    if (s.entries.empty()) {  // nothing on the character: no glass either
        hide(s);
        return;
    }
    // its place: under the new minimap when it shows, else where the game's own icons begin (150 of its pixels)
    int right = targetWidth - int(std::lround(10 * scale)), top = int(std::lround(150 * k)), widest = 0;
    int mx = 0, my = 0, mw = 0, mh = 0;
    if (minimapproto::panelBox(mx, my, mw, mh)) {
        right = mx + mw;
        top = my + mh + int(std::lround(8 * scale));
        widest = mw;
    }
    widest = std::max(widest, int(std::lround(300 * scale)));
    const int bottom = targetHeight - int(std::lround(50 * k));
    const bp::Look panelLook = look == BuffsLook::Row ? bp::Look::Row : s.names ? bp::Look::Names : bp::Look::Column;
    // the hovered icon: its name in a tip
    int hover = -1;
    int cx = 0, cy = 0;
    s.layout = bp::layout(s.entries, panelLook, right, top, bottom, widest, scale);
    if (readGameCursor(cx, cy)) {
        const bp::Hit h = bp::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == bp::Part::Icon) hover = h.slot;
    }
    s.hover = hover;
    std::string shape = std::to_string(int(panelLook)) + ';' + std::to_string(s.layout.x) + ',' +
                        std::to_string(s.layout.y) + ',' + std::to_string(s.layout.w) + 'x' +
                        std::to_string(s.layout.h) + ';' + std::to_string(hover) + ';' + std::to_string(scale) + ';' +
                        std::to_string(uistyle::generation()) + ';';  // 3.0.11-beta.47: its ground's opacity
    std::string fill;
    for (const bp::Entry& e : s.entries) {
        shape += std::to_string(e.icon) + ':' + std::to_string(int(e.kind)) + (e.picture ? "p" : "") + ',';
        fill += e.time + ':' + std::to_string(int(std::lround(e.left * 90))) + (e.warn ? "w" : "") + ',';
    }
    if (shape != s.shape || (fill != s.fill && now - s.paintedAt >= kRepaintMs)) {
        s.shape = shape;
        s.fill = fill;
        render(s);
    }
    if (!s.wantedTold) {
        s.wantedTold = true;
        LOG_INFO("buffs: the column shows %zu of the character's icons at %d,%d (%dx%d, %s)", s.layout.slots.size(),
                 s.layout.x, s.layout.y, s.layout.w, s.layout.h, buffsLookToken(look));
    }
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPicture(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    (void)wp;
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (m == WM_MOUSEWHEEL && !ScreenToClient(game, &p)) return false;  // the wheel's point is the screen's
    float tx = 0, ty = 0;
    if (!toTarget(s, game, p.x, p.y, tx, ty)) return false;
    const float px = tx - float(s.layout.x), py = ty - float(s.layout.y);
    if (!s.layout.hit.contains(px, py)) return false;
    if (m == WM_LBUTTONDOWN) {
        const bp::Hit h = bp::hitTest(s.layout, px, py);
        if (h.part == bp::Part::Toggle || (h.part == bp::Part::More && s.look == BuffsLook::Column)) {
            s.names = h.part == bp::Part::More ? true : !s.names;  // «ще N»: the names list holds more
            keepState(s);
            LOG_INFO("buffs: the names %s", s.names ? "shown" : "hidden");
        } else if (h.part == bp::Part::Icon && h.slot >= 0 && size_t(h.slot) < s.layout.slots.size()) {
            const bp::Entry& e = s.entries[size_t(s.layout.slots[size_t(h.slot)].entry)];
            if (e.kind != bp::Kind::Debuff) {  // a favourite: first and larger (debuffs stand last, always)
                const bool star = s.favourites.insert(e.icon).second;
                if (!star) s.favourites.erase(e.icon);
                keepState(s);
                if (s.clicksLogged++ < 10)
                    LOG_INFO("buffs: %s (icon %u) %s the favourites", e.name.c_str(), e.icon, star ? "among" : "off");
            }
        }
        s.shape.clear();  // painted again
    }
    result = 0;
    return true;
}

}  // namespace uagfx::win::buffcolumn

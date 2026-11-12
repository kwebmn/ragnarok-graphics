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
#include "win/game_memory.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/pack_pictures.hpp"
#include "win/minimap_proto.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/uaui.hpp"
#include "win/updates_hook.hpp"
#include "win/window_input.hpp"
#include "win/words.hpp"

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
constexpr uint32_t kWorld = 0xC4, kPlayer = 0x3C, kIconEffect = 0x428;
constexpr int kRelayout = 0x2C;
constexpr ULONGLONG kRepaintMs = 100;  // the times and the rings painted again at most this often
constexpr uint64_t kTipMs = 400;       // 3.0.11-beta.51: the concept's - a tip after 0.4 s pointed at

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
    int madeFor = -2;  // 3.0.11-beta.74: packpictures::iconsStamp() it was read for (-2 never)
    std::vector<uint32_t> argb;
    int w = 0, h = 0;
    bool alpha = false;
    std::map<int, panel::Picture> sized;  // made again in their places
    // 3.0.18-beta.187 (the tester: Wind Walk's «зараз сірим»): its file not read (the game's files busy, a map
    // loading) - read again a while later, a few times, not a dark tile for the rest of the game
    ULONGLONG retryAt = 0;
    int retries = 0;
};
constexpr ULONGLONG kIconRetryMs = 3000;
constexpr int kIconRetries = 5;

struct GameIcon {
    uint16_t icon = 0;
    std::string file;  // effect\...tga (the game's code page)
};

struct State {
    BuffsLook look = BuffsLook::Off;
    BuffsOverflow overflow = BuffsOverflow::More;  // 3.0.11-beta.63
    bool shown = false, wantedTold = false, hookTold = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    bool stateRead = false, names = false;
    std::set<uint16_t> favourites;
    bool uiShown = false;  // 3.0.18-beta.20: drawn by uaUI
    std::map<std::string, Icon> icons;  // by file
    std::unique_ptr<gamefiles::Reader> files;
    std::vector<GameIcon> game;
    std::vector<bp::Entry> entries;
    bp::Layout layout;
    int pressesLogged = 0;  // 3.0.11-beta.233
    int hover = -1, tip = -1;  // 3.0.11-beta.51: its tip's - the hovered one's after 0.4 s (the concept's)
    uint64_t hoverSince = 0;
    std::string shape, fill;  // what the picture shows: its parts, its times
    ULONGLONG paintedAt = 0;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int iconsLogged = 0, clicksLogged = 0;
    size_t linesTold = 0;  // 3.0.11-beta.63: the further columns logged
    int linesLogged = 0;
};

State& state() {
    static State s;
    return s;
}

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

// 3.0.11-beta.202 (the tester: «тут теж на 70% показує іконку 50»): the game's «50%» weight icon (SI_WEIGHT50, 35) -
// uAthena on uaRO sends it at 70 % of the most weight: the interface's own in its place, its «70%» in words
// 3.0.11-beta.261 (the tester: «іконки статусів перегрузу різні, 70% нова, 90% стара»): SI_WEIGHT90 (36) the same way,
// «90%» on a deeper red
constexpr uint16_t kWeight50 = 35, kWeight90 = 36;
const panel::Picture* weightIcon(int side, bool ninety = false) {
    static std::map<int, panel::Picture> made;
    if (side <= 0) return nullptr;
    side = ninety ? -side : side;  // its own entry
    auto it = made.find(side);
    if (it != made.end()) return &it->second;
    panel::Picture& p = made[side];
    side = std::abs(side);
    p.width = p.height = side;
    p.px.assign(size_t(side) * size_t(side), 0u);
    const float r = float(side) * 0.18f;
    panel::roundRect(p, panel::Box{0, 0, float(side), float(side)}, r, ninety ? 0x8E1414 : 0xC92A2A, 0.95f,
                     ninety ? 0xFF6B6B : 0xFFB3B3, ninety ? 0.8f : 0.55f, std::max(1.0f, float(side) / 24.0f));
    drawWords(p, {Words{panel::Box{0, 0, float(side), float(side)}, ninety ? L"90%" : L"70%", 0xFFFFFF,
                        float(side) * 0.42f, true,
                        DT_CENTER}});
    return &p;
}

const panel::Picture* pictureOf(State& s, const GameIcon& g, int side) {
    Icon& ic = s.icons[g.file];
    // 3.0.11-beta.74: the AI texture pack's picture when it has one ([interface] pack_icons), read again when that
    // changes (the pack read, the switch)
    const ULONGLONG now = GetTickCount64();
    if (const int want = packpictures::iconsStamp(); want != ic.madeFor || (ic.retryAt && now >= ic.retryAt)) {
        const bool first = ic.madeFor == -2;
        const bool retry = ic.retryAt != 0;
        ic.retryAt = 0;
        ic.madeFor = want;
        ic.argb.clear();
        ic.w = ic.h = 0;
        ic.alpha = false;
        std::string bytes;
        const std::string entry = "data\\texture\\" + g.file;
        const bool read = readFile(s, entry, bytes);
        const auto* data = reinterpret_cast<const uint8_t*>(bytes.data());
        std::string name;
        if (read && want > 0 && packpictures::ofTexture(data, bytes.size(), true, ic.argb, ic.w, ic.h, &name)) {
            ic.alpha = true;
            if (s.iconsLogged < 40) {
                ++s.iconsLogged;
                LOG_INFO("buffs: icon %u (%s) - the AI texture pack's %s %dx%d", g.icon, nameOf(g).c_str(),
                         name.c_str(), ic.w, ic.h);
            }
        } else if (!read || !pack::decodeTga(data, bytes.size(), ic.argb, ic.w, ic.h, ic.alpha)) {
            ic.argb.clear();
            if (!read && ic.retries < kIconRetries) {  // 3.0.18-beta.187: again a while later
                ++ic.retries;
                ic.retryAt = now + kIconRetryMs;
            }
            if (first && s.iconsLogged < 40) {
                ++s.iconsLogged;
                LOG_INFO("buffs: icon %u's picture %s %s - a dark tile in its place%s", g.icon, entry.c_str(),
                         read ? "not a .tga the driver reads" : "not found", ic.retryAt ? " (read again in 3 s)" : "");
            }
        } else if (retry && s.iconsLogged < 40) {  // 3.0.18-beta.187
            ++s.iconsLogged;
            LOG_INFO("buffs: icon %u (%s) - %s %dx%d read on another try", g.icon, nameOf(g).c_str(), entry.c_str(),
                     ic.w, ic.h);
        } else if (first && s.iconsLogged < 40) {
            ++s.iconsLogged;
            LOG_INFO("buffs: icon %u (%s) - %s %dx%d%s", g.icon, nameOf(g).c_str(), entry.c_str(), ic.w, ic.h,
                     ic.alpha ? "" : ", its magenta keyed out");
        }
        for (auto& [at, picture] : ic.sized)
            picture = ic.argb.empty() ? panel::Picture{}
                                      : bp::iconPicture(ic.argb, ic.w, ic.h, ic.alpha, at, 5 * s.scale);
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

void block(const State& s, HWND game) {  // the mouse's clicks over it are its own, not the game's
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const auto rect = [&](const panel::Box& h) {
        const float x0 = float(s.layout.x) + h.x, y0 = float(s.layout.y) + h.y;
        return RECT{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                    LONG(int64_t(x0 + h.w) * client.right / s.targetW + 1),
                    LONG(int64_t(y0 + h.h) * client.bottom / s.targetH + 1)};
    };
    const RECT r = rect(s.layout.hit);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Buffs, game, &r, false);
    // 3.0.11-beta.63: the further columns' (rows') own box - the world under the empty corner by them stays the game's
    const RECT lines = rect(s.layout.linesHit);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::BuffsLines, game,
                                  s.layout.linesHit.w > 0 ? &lines : nullptr, false);
}

void unblock() {
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Buffs, nullptr, nullptr, false);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::BuffsLines, nullptr, nullptr, false);
}

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


// The names' button, «ще N» and an icon pressed (3.0.11-beta.186, .231: a favourite, first and larger) - the old
// picture's clicks and uaUI's events alike
void pressPart(State& s, const bp::Hit& h) {
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
    s.shape.clear();  // laid out and painted again
}

// ---- 3.0.18-beta.20 (the owner: «Переписывать 73 части в разметку RmlUi вручную»): the column as an RmlUi document
// while [interface] uaui is on - its icons, times, names, «ще N», the names' button and the tip at core buffpanel's
// places (the target's pixels: what fits and where the others go stays its layout's), its time's sweep a <ua-ring>
// over the whole icon; the clicks into pressPart(); its picture not painted then ----

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="bc" data-model="buffs" data-style-font-size="dp + 'px'"
 data-style-width="w + 'px'" data-style-height="h + 'px'">
<div class="toggle" data-if="toggle.w > 0" data-class-on="names" data-event-click="toggle()"
     data-style-left="toggle.x + 'px'" data-style-top="toggle.y + 'px'" data-style-width="toggle.w + 'px'"
     data-style-height="toggle.h + 'px'" data-style-border-radius="toggle.h / 2 + 'px'">
 <span class="ln" data-style-top="toggle.h / 2 - 4 * dp - 0.75 * dp + 'px'"></span>
 <span class="ln" data-style-top="toggle.h / 2 - 0.75 * dp + 'px'"></span>
 <span class="ln" data-style-top="toggle.h / 2 + 4 * dp - 0.75 * dp + 'px'"></span></div>
<div class="slot" data-for="b, i : slots" data-event-click="press(i)">
 <div class="shadow" data-style-left="b.fx - 2.5 * dp + 'px'" data-style-top="b.fy - 2.5 * dp + 'px'"
      data-style-width="b.fw + 5 * dp + 'px'" data-style-height="b.fh + 5 * dp + 'px'" data-style-border-radius="7.5 * dp + 'px'"></div>
 <div class="outline" data-style-left="b.fx - dp + 'px'" data-style-top="b.fy - dp + 'px'"
      data-style-width="b.fw + 2 * dp + 'px'" data-style-height="b.fh + 2 * dp + 'px'" data-style-border-radius="6 * dp + 'px'"></div>
 <div class="icon" data-style-left="b.x + 'px'" data-style-top="b.y + 'px'" data-style-width="b.w + 'px'"
      data-style-height="b.h + 'px'" data-style-border-radius="5 * dp + 'px'"><img data-if="b.pic != ''" data-attr-src="b.pic"/></div>
 <ua-ring class="abs" sector="1" square="1" fill="#00000000" track="#00000099" data-if="b.left >= 0"
      data-style-left="b.x + 'px'" data-style-top="b.y + 'px'" data-style-width="b.w + 'px'" data-style-height="b.h + 'px'"
      data-attr-value="b.left" style="position: absolute;"></ua-ring>
 <div class="bad" data-if="b.debuff" data-style-left="b.fx - dp + 'px'" data-style-top="b.fy - dp + 'px'"
      data-style-width="b.fw + 2 * dp + 'px'" data-style-height="b.fh + 2 * dp + 'px'" data-style-border-radius="6 * dp + 'px'"></div>
 <div class="hover" data-if="i == hover" data-style-left="b.fx - 1.5 * dp + 'px'" data-style-top="b.fy - 1.5 * dp + 'px'"
      data-style-width="b.fw + 3 * dp + 'px'" data-style-height="b.fh + 3 * dp + 'px'" data-style-border-radius="6.5 * dp + 'px'"></div>
 <span class="time" data-if="b.time != ''" data-class-warn="b.warn" data-class-bad="b.debuff"
       data-style-left="b.tx + 'px'" data-style-top="b.ty + 'px'" data-style-width="b.tw + 'px'"
       data-style-height="b.th + 'px'" data-style-line-height="b.th + 'px'">{{b.time}}</span>
 <span class="name" data-if="names" data-class-bad="b.debuff" data-style-left="b.nx + 'px'" data-style-top="b.ny + 'px'"
       data-style-width="b.nw + 'px'" data-style-height="b.nh + 'px'" data-style-line-height="b.nh + 'px'">{{b.name}}</span>
</div>
<span class="more" data-if="more.w > 0" data-event-click="more()" data-style-left="more.x + 'px'"
      data-style-top="more.y + 'px'" data-style-width="more.w + 'px'" data-style-height="more.h + 'px'"
      data-style-line-height="more.h + 'px'">{{more_text}}</span>
<div class="tip" data-if="tip.w > 0" data-style-left="tip.x + 'px'" data-style-top="tip.y + 'px'"
     data-style-width="tip.w - 20 * dp + 'px'" data-style-height="tip.h - 4 * dp + 'px'">
 <span class="a">{{tip_a}}</span><span class="b" data-if="tip_b != ''">{{tip_b}}</span></div>
</body></rml>)RML";

struct BoxView {
    float x = 0, y = 0, w = 0, h = 0;
};
struct SlotView {
    float x = 0, y = 0, w = 0, h = 0, fx = 0, fy = 0, fw = 0, fh = 0, tx = 0, ty = 0, tw = 0, th = 0, nx = 0, ny = 0,
          nw = 0, nh = 0, left = -1;
    Rml::String pic, time, name;
    bool warn = false, debuff = false;
};

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0, dp = 1;
    bool names = false;
    int hover = -1;
    BoxView toggle, more, tip;
    Rml::String moreText, tipA, tipB;
    std::vector<SlotView> slots;
    std::map<const panel::Picture*, std::string> sources;
    std::string key;
};

Ui& ui() {
    static Ui u;
    return u;
}

BoxView boxOf(const panel::Box& b) { return BoxView{b.x, b.y, b.w, b.h}; }

bool uiFrame(State& s) {
    Ui& u = ui();
    if (!uaui::ready()) {
        u.doc.show(false);
        return false;
    }
    if (!u.doc.made() && !u.doc.make("buffs", kDoc, [&](Rml::DataModelConstructor& m) {
            if (auto b = m.RegisterStruct<BoxView>()) {
                b.RegisterMember("x", &BoxView::x);
                b.RegisterMember("y", &BoxView::y);
                b.RegisterMember("w", &BoxView::w);
                b.RegisterMember("h", &BoxView::h);
            }
            if (auto v = m.RegisterStruct<SlotView>()) {
                for (const auto& [name, field] : std::initializer_list<std::pair<const char*, float SlotView::*>>{
                         {"x", &SlotView::x},   {"y", &SlotView::y},   {"w", &SlotView::w},   {"h", &SlotView::h},
                         {"fx", &SlotView::fx}, {"fy", &SlotView::fy}, {"fw", &SlotView::fw}, {"fh", &SlotView::fh},
                         {"tx", &SlotView::tx}, {"ty", &SlotView::ty}, {"tw", &SlotView::tw}, {"th", &SlotView::th},
                         {"nx", &SlotView::nx}, {"ny", &SlotView::ny}, {"nw", &SlotView::nw}, {"nh", &SlotView::nh},
                         {"left", &SlotView::left}})
                    v.RegisterMember(name, field);
                v.RegisterMember("pic", &SlotView::pic);
                v.RegisterMember("time", &SlotView::time);
                v.RegisterMember("name", &SlotView::name);
                v.RegisterMember("warn", &SlotView::warn);
                v.RegisterMember("debuff", &SlotView::debuff);
            }
            m.RegisterArray<std::vector<SlotView>>();
            m.Bind("w", &u.w);
            m.Bind("h", &u.h);
            m.Bind("dp", &u.dp);
            m.Bind("names", &u.names);
            m.Bind("hover", &u.hover);
            m.Bind("toggle", &u.toggle);
            m.Bind("more", &u.more);
            m.Bind("tip", &u.tip);
            m.Bind("more_text", &u.moreText);
            m.Bind("tip_a", &u.tipA);
            m.Bind("tip_b", &u.tipB);
            m.Bind("slots", &u.slots);
            m.BindEventCallback("toggle", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                pressPart(state(), bp::Hit{bp::Part::Toggle, -1});
            });
            m.BindEventCallback("more", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                pressPart(state(), bp::Hit{bp::Part::More, -1});
            });
            m.BindEventCallback("press", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
                if (!a.empty()) pressPart(state(), bp::Hit{bp::Part::Icon, a[0].Get<int>()});
            });
        }))
        return false;
    const std::string key = s.shape + '|' + s.fill + '|' + std::to_string(s.tip);
    if (key != u.key) {
        u.key = key;
        const bp::Layout& L = s.layout;
        const bp::Texts t = texts();
        u.w = float(L.w), u.h = float(L.h), u.dp = L.s;
        u.names = L.look == bp::Look::Names;
        u.hover = s.hover;
        u.toggle = boxOf(L.toggle);
        u.more = boxOf(L.more);
        u.moreText.clear();
        if (L.more.w > 0) {
            u.moreText = t.more;
            const size_t at = u.moreText.find("{1}");
            if (at != std::string::npos) u.moreText.replace(at, 3, std::to_string(L.hidden));
        }
        std::string first, second;
        u.tip = boxOf(bp::tipText(L, s.entries, s.tip, t, first, second));
        u.tipA = first, u.tipB = second;
        u.slots.clear();
        for (const bp::Slot& slot : L.slots) {
            const bp::Entry& e = s.entries[size_t(slot.entry)];
            SlotView v;
            v.x = slot.icon.x, v.y = slot.icon.y, v.w = slot.icon.w, v.h = slot.icon.h;
            v.fx = slot.frame.x, v.fy = slot.frame.y, v.fw = slot.frame.w, v.fh = slot.frame.h;
            v.tx = slot.time.x, v.ty = slot.time.y, v.tw = slot.time.w, v.th = slot.time.h;
            v.nx = slot.name.x, v.ny = slot.name.y, v.nw = slot.name.w, v.nh = slot.name.h;
            v.left = e.left >= 0 ? std::clamp(e.left, 0.0f, 1.0f) : -1;
            v.pic = e.picture ? uaui::picture(e.picture, uaui::stamp(e.picture), u.sources[e.picture]) : "";
            v.time = e.time;
            v.name = e.name;
            v.warn = e.warn;
            v.debuff = e.kind == bp::Kind::Debuff;
            u.slots.push_back(v);
        }
        u.doc.dirtyAll();
    }
    u.doc.place(s.layout.x, s.layout.y);
    u.doc.show(true);
    return true;
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

bool glassBox(int& left, int& top) {
    const State& s = state();
    if (!s.shown || s.layout.w <= 0) return false;
    const panel::Box& furthest = s.layout.lines.empty() || s.layout.look == bp::Look::Row ? s.layout.body
                                                                                          : s.layout.lines.back();
    left = s.layout.x + int(std::floor(std::min(furthest.x, s.layout.body.x)));
    top = s.layout.y + int(std::floor(s.layout.look == bp::Look::Row ? 0.0f : s.layout.toggle.y));
    return true;
}

void frame(HWND game, BuffsLook look, BuffsOverflow overflow, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (look != s.look) {
        LOG_INFO("buffs: [interface] buffs = %s", buffsLookToken(look));
        s.look = look;
        s.shape.clear();
    }
    if (overflow != s.overflow) {  // 3.0.11-beta.63
        LOG_INFO("buffs: [interface] buffs_overflow = %s", buffsOverflowToken(overflow));
        s.overflow = overflow;
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
    // 3.0.11-beta.75: its size in «Розміщення інтерфейсу»
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Buffs);
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
        e.picture = g.icon == kWeight50 || g.icon == kWeight90  // 3.0.11-beta.202: «70%» (.261: «90%»)
                        ? weightIcon(bp::iconSide(e.kind, scale), g.icon == kWeight90)
                        : pictureOf(s, g, bp::iconSide(e.kind, scale));
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
    s.layout = bp::layout(s.entries, panelLook, right, top, bottom, widest, scale,
                          s.overflow == BuffsOverflow::Column ? bp::Overflow::Lines : bp::Overflow::More);
    hudedit::place(hudlayout::Part::Buffs, float(k), targetWidth, targetHeight, s.layout.w, s.layout.h, s.layout.x,
                   s.layout.y);  // 3.0.11-beta.75: where the player put it
    if (hudedit::movedOf(hudlayout::Part::Buffs) && s.layout.y + s.layout.h > bottom) {
        // 3.0.11-beta.261 (the tester: «треба щоб тільки вниз було, а потім в 2 колонки»): put lower than its own
        // place - the icons counted again from there down to the bottom (the further columns, «ще N»), placed again
        s.layout = bp::layout(s.entries, panelLook, s.layout.x + s.layout.w, s.layout.y, bottom, widest, scale,
                              s.overflow == BuffsOverflow::Column ? bp::Overflow::Lines : bp::Overflow::More);
        hudedit::place(hudlayout::Part::Buffs, float(k), targetWidth, targetHeight, s.layout.w, s.layout.h,
                       s.layout.x, s.layout.y);
    }
    if (s.layout.lines.size() != s.linesTold) {  // 3.0.11-beta.63: a further column opened or closed
        s.linesTold = s.layout.lines.size();
        if (s.linesLogged++ < 20)
            LOG_INFO("buffs: %zu further %s (%zu of %zu icons shown, %d under «ще N», %s)", s.linesTold,
                     panelLook == bp::Look::Row ? "rows" : "columns", s.layout.slots.size(), s.entries.size(),
                     s.layout.hidden, buffsOverflowToken(s.overflow));
    }
    if (readHoverCursor(cx, cy)) {
        const bp::Hit h = bp::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == bp::Part::Icon) hover = h.slot;
        if (h.part == bp::Part::Toggle) hover = bp::kToggleTip;  // 3.0.11-beta.231: its words in a tip
    }
    if (hover != s.hover) s.hoverSince = now;
    s.hover = hover;
    s.tip = hover != -1 && now - s.hoverSince >= kTipMs ? hover : -1;
    std::string shape = std::to_string(int(panelLook)) + ';' + std::to_string(s.layout.w) + 'x' +
                        std::to_string(s.layout.h) + ';' + std::to_string(hover) + ',' + std::to_string(s.tip) + ';' +
                        std::to_string(scale) + ';' +
                        std::to_string(uistyle::generation()) + ';';  // 3.0.11-beta.47: its ground's opacity
    std::string fill;
    for (const bp::Entry& e : s.entries) {
        shape += std::to_string(e.icon) + ':' + std::to_string(int(e.kind)) + (e.picture ? "p" : "") + ',';
        fill += e.time + ':' + std::to_string(int(std::lround(e.left * 90))) + (e.warn ? "w" : "") + ',';
    }
    if (shape != s.shape || (fill != s.fill && now - s.paintedAt >= kRepaintMs)) {
        s.shape = shape;
        s.fill = fill;
        s.paintedAt = now;
    }
    s.uiShown = uiFrame(s);
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
    if (!s.shown) ui().doc.show(false);  // 3.0.18-beta.20
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown || s.uiShown) return false;  // 3.0.18-beta.20: uaUI's events take its clicks
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    (void)wp;
    // 3.0.11-beta.233 (the owner: «на иконку бургера нажимаю и оно не срабатывает на второй третий раз»): the press
    // where the game's own cursor stands - as the hover follows it and every other part takes its clicks
    // (game_camera pointerInTarget, 3.0.11-beta.43); the message's own point strayed from it on a scaled 4K screen
    float tx = 0, ty = 0;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty)) return false;
    const float px = tx - float(s.layout.x), py = ty - float(s.layout.y);
    if (!s.layout.hit.contains(px, py) && !s.layout.linesHit.contains(px, py)) return false;
    // 3.0.11-beta.232 («кнопочка переключалки иногда работает а иногда нет»): a quick second click comes as a double
    // click - a press too
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        const bp::Hit h = bp::hitTest(s.layout, px, py);
        if (s.pressesLogged++ < 12)
            LOG_INFO("buffs: a press at the column's (%.0f, %.0f) - part %d", px, py, int(h.part));
        pressPart(s, h);
        s.shape.clear();  // painted again
    }
    result = 0;
    return true;
}

const panel::Picture* iconPicture(uint16_t icon, int side) {
    if (icon == kWeight50 || icon == kWeight90) return weightIcon(side, icon == kWeight90);  // .202, .261
    State& s = state();
    const auto image = reinterpret_cast<ImageFn>(uintptr_t(kIconImage));
    void* session = reinterpret_cast<void*>(uintptr_t(kSession));
    for (uint32_t pass = 0; pass < 5; ++pass)
        if (const char* file = image(session, icon, pass); file && *file)
            return pictureOf(s, GameIcon{icon, file}, side);
    return nullptr;
}

}  // namespace uagfx::win::buffcolumn

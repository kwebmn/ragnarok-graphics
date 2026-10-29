#include "win/char_block.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/char_status.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/sprite_file.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/updates_hook.hpp"

namespace uagfx::win::charpanel {
namespace {

namespace cb = uagfx::charblock;

// uaRO.exe's: the windows' manager and its UI window 0 (Basic Info: its class's vtable, its shown flag the manager
// draws it and gives it the mouse by - FUN_004cd720, FUN_004cea60 -, its SendMsg FUN_0046c980: message 6 a button's)
constexpr uint32_t kWindowMgr = 0x727ED8, kBasicInfo = kWindowMgr + 0x90, kBasicInfoClass = 0x6984A0;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kSendMsg = 0x46C980;
constexpr int kButtonMsg = 6;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
// The session's character: its class, its sex (+0x1CA4, which GetSex FUN_006425b0 keeps in +0x770), its head and
// hair colour; the session's tables FUN_0062e7b0 names a head's sprite by - the sexes' folders (+0x840), the heads'
// names (+0x850 a man's, +0x860 a woman's), the sexes' endings (+0x8A0)
constexpr uint32_t kSession = 0x771148, kJob = 0x771F8C, kSex = 0x772DEC, kHead = 0x7718F4, kHairColour = 0x771910;
constexpr uint32_t kFolders = kSession + 0x840, kManHeads = kSession + 0x850, kWomanHeads = kSession + 0x860;
constexpr uint32_t kEndings = kSession + 0x8A0;
constexpr ULONGLONG kWarnMs = 2000;  // the low HP warning's
const std::string kHuman = "\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xD3\xB8\xAE\xC5\xEB\\";  // 인간족\머리통\ (CP949)
const std::string kHairPalettes = "\xB8\xD3\xB8\xAE\\\xB8\xD3\xB8\xAE";            // 머리\머리
const std::string kMan = "\xB3\xB2", kWoman = "\xBF\xA9";                            // 남, 여

struct State {
    CharacterLook look = CharacterLook::Off;
    bool shown = false, stateRead = false, hidOld = false, oldTold = false;
    cb::View view = cb::View::Full;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    cb::Data data;
    cb::Layout layout;
    cb::Show show;
    bool wasLow = false;
    ULONGLONG warnUntil = 0;
    std::string key;  // what the picture shows
    std::string portraitKey;
    panel::Picture portrait;
    std::unique_ptr<gamefiles::Reader> files;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int clicksLogged = 0;
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

std::string readString(uint32_t at) {
    char buf[64] = {};
    if (!at || !readGame(at, buf, sizeof buf - 1)) return "";
    buf[sizeof buf - 1] = 0;
    return buf;
}

// UI window 0 when it is the Basic Info window of this build
uint32_t basicInfo() {
    const uint32_t window = read32(kBasicInfo);
    return window && read32(window) == kBasicInfoClass ? window : 0;
}

bool write32(uint32_t at, uint32_t v) {
    SIZE_T n = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(at)), &v, 4, &n) && n == 4;
}

// Its shown flag held: 0 while the block shows (each frame - the game shows it again on a new map), 1 back
void holdGameWindow(State& s, bool hide) {
    const uint32_t window = basicInfo();
    if (!window) {
        if (hide && !s.oldTold) {
            s.oldTold = true;
            LOG_INFO("character: the game's Basic Info window is not where this build has it - it stays as it is");
        }
        return;
    }
    const uint32_t shown = read32(window + kShown);
    if (hide && shown != 0 && write32(window + kShown, 0)) {
        if (!s.hidOld) LOG_INFO("character: the game's Basic Info window hidden for the block (its shown flag 0)");
        s.hidOld = true;
    } else if (!hide && s.hidOld && shown == 0) {
        write32(window + kShown, 1);
        s.hidOld = false;
        LOG_INFO("character: the game's Basic Info window back");
    }
}

// One of the Basic Info window's buttons, as its own: its SendMsg (message 6) with the button's command
bool press(int command) {
    const uint32_t window = basicInfo();
    if (!window || read32(read32(window) + kSendMsgSlot) != kSendMsg) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(kSendMsg))(window, 0, kButtonMsg, command, 0, 0);
    return true;
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kCharacterState), text)) return;
    if (text.find("view compact") != std::string::npos) s.view = cb::View::Compact;
    if (text.find("view icon") != std::string::npos) s.view = cb::View::Icon;
}

void keepState(const State& s) {
    const char* view = s.view == cb::View::Compact ? "compact" : s.view == cb::View::Icon ? "icon" : "full";
    const std::string path = layout::join(gameDir(), layout::kCharacterState);
    if (!writeFileAtomic(path, std::string("view ") + view + "\n"))
        LOG_INFO("character: %s cannot be written", path.c_str());
}

bool readFile(State& s, const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;  // as the game finds it (its zips too)
    if (!s.files) s.files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!s.files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

// The portrait: the head's first frame (standing, facing down) in its hair colour, fitted into `side`
void makePortrait(State& s, int side) {
    const int sex = int(read32(kSex)) != 0 ? 1 : 0;  // as FUN_0062e7b0: not 0 - a man
    int head = int(read32(kHead));
    const int colour = int(read32(kHairColour));
    if (head <= 0) head = 1;  // FUN_00639900's for none
    const std::string key = std::to_string(sex) + ':' + std::to_string(head) + ':' + std::to_string(colour) + ':' +
                            std::to_string(side);
    if (key == s.portraitKey) return;
    s.portraitKey = key;
    s.portrait = panel::Picture{};
    std::string folder = readString(read32(read32(kFolders) + 4 * uint32_t(sex)));
    std::string name = readString(read32(read32(sex ? kManHeads : kWomanHeads) + 4 * uint32_t(head)));
    std::string ending = readString(read32(read32(kEndings) + 4 * uint32_t(sex)));
    if (folder.empty() || name.empty()) {  // the plain heads' names, as the game's own "%d_남.spr"
        folder = sex ? kMan : kWoman;
        name = std::to_string(head);
        ending = "_" + folder;
    }
    const std::string base = "data\\sprite\\" + kHuman + folder + "\\" + name + ending;
    std::string spr, act;
    if (!readFile(s, base + ".spr", spr) || !readFile(s, base + ".act", act)) {
        LOG_INFO("character: no head sprite %s.spr - the portrait the name's letter", base.c_str());
        return;
    }
    std::string pal;
    const std::string palName =
        "data\\palette\\" + kHairPalettes + name + ending + "_" + std::to_string(colour) + ".pal";
    if (colour > 0 && readFile(s, palName, pal) && pal.size() >= 1024 && spr.size() > 1024)
        spr.replace(spr.size() - 1024, 1024, pal, 0, 1024);  // the hair colour in the sprite's palette, as the game
    spritefile::Sprite sp;
    spritefile::Act ac;
    spritefile::Image im;
    if (!spritefile::readSpr(spr, sp) || !spritefile::readAct(act, ac) || !spritefile::frameImage(sp, ac, 0, 0, im)) {
        LOG_INFO("character: the head sprite %s.spr not read - the portrait the name's letter", base.c_str());
        return;
    }
    const spritefile::Image fit = spritefile::fitted(im, side, side);
    s.portrait.width = fit.w;
    s.portrait.height = fit.h;
    s.portrait.px = fit.argb;
    LOG_INFO("character: the portrait - %s.spr (%dx%d)%s", base.c_str(), im.w, im.h,
             pal.empty() ? "" : (", the hair colour " + std::to_string(colour)).c_str());
}

cb::Texts texts() {
    cb::Texts t;
    t.weight = say(settings::Text::CharWeight);
    t.points = say(settings::Text::CharPoints);
    t.lowHp = say(settings::Text::CharLowHp);
    const settings::Text names[cb::kButtons] = {settings::Text::CharStats,  settings::Text::CharEquip,
                                                settings::Text::CharItems,  settings::Text::CharSkills,
                                                settings::Text::CharFriends, settings::Text::CharOptions};
    for (int i = 0; i < cb::kButtons; ++i) t.buttons[i] = say(names[i]);
    t.decimal = startup().lang == settings::Lang::En ? '.' : ',';
    return t;
}

std::vector<Words> gdiWords(const std::vector<cb::Words>& in) {
    std::vector<Words> out;
    for (const cb::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == cb::Align::Right    ? DT_RIGHT
                                 : w.align == cb::Align::Center ? DT_CENTER
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
    const cb::Texts t = texts();
    cb::paint(s.picture, s.layout, s.data, t, s.show);
    drawWords(s.picture, gdiWords(cb::words(s.layout, s.data, t, s.show)));
    s.upload = true;
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
    const panel::Box& b = s.layout.block;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Character, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Character, nullptr, nullptr, false);
    s.shown = false;
}

float share(int value, int next, int level) {
    if (next > 0) return std::clamp(float(value) / float(next), 0.0f, 1.0f);
    return level > 0 ? 1.0f : -1.0f;  // next 0: the last level
}

}  // namespace

void frame(HWND game, CharacterLook look, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (look != s.look) {
        LOG_INFO("character: [interface] character = %s", characterLookToken(look));
        s.look = look;
        s.key.clear();
    }
    const bool on = look != CharacterLook::Off;
    const bool world = inGameWorld();
    if (world) holdGameWindow(s, on);
    if (!on || !world || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    readState(s);
    charstatus::Status st;
    if (!readOwnStatus(st)) {
        hide(s);
        return;
    }
    const float scale = std::clamp(float(k), 0.75f, 6.0f);
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const cb::Look panelLook = look == CharacterLook::Rings ? cb::Look::Rings : cb::Look::Bars;
    s.layout = cb::layout(s.view, panelLook, int(std::lround(10 * scale)), int(std::lround(10 * scale)), scale);
    makePortrait(s, int(std::lround(s.layout.portrait.w * 0.86f)));
    cb::Data& d = s.data;
    d.name = readOwnName();
    d.job = cb::jobName(int(read32(kJob)));
    d.baseLevel = st.baseLevel;
    d.jobLevel = st.jobLevel;
    d.hp = st.hp;
    d.hpMost = st.hpMost;
    d.sp = st.sp;
    d.spMost = st.spMost;
    d.baseExp = share(st.baseExp, st.baseNext, st.baseLevel);
    d.jobExp = share(st.jobExp, st.jobNext, st.jobLevel);
    d.weight = st.weightMost > 0 ? int(int64_t(st.weight) * 100 / st.weightMost) : -1;
    d.zeny = st.zeny;
    d.statusPoints = st.statusPoints;
    d.portrait = s.portrait.width > 0 ? &s.portrait : nullptr;
    // low HP: a short warning as it falls under a quarter
    const ULONGLONG now = GetTickCount64();
    const bool low = cb::lowHp(d);
    if (low && !s.wasLow) {
        s.warnUntil = now + kWarnMs;
        LOG_INFO("character: low HP - %d of %d", d.hp, d.hpMost);
    }
    s.wasLow = low;
    cb::Show show;
    show.warning = now < s.warnUntil;
    int cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) {
        const cb::Hit h = cb::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == cb::Part::Button) show.hoverButton = h.index;
        show.hoverBlock = h.part != cb::Part::None;
    }
    s.show = show;
    const std::string key =
        std::to_string(int(s.view)) + ';' + std::to_string(int(panelLook)) + ';' + std::to_string(scale) + ';' +
        d.name + ';' + d.job + ';' + std::to_string(d.baseLevel) + ',' + std::to_string(d.jobLevel) + ';' +
        std::to_string(d.hp) + '/' + std::to_string(d.hpMost) + ',' + std::to_string(d.sp) + '/' +
        std::to_string(d.spMost) + ';' + std::to_string(int(d.baseExp * 1000)) + ',' +
        std::to_string(int(d.jobExp * 1000)) + ';' + std::to_string(d.weight) + ';' + std::to_string(d.zeny) + ';' +
        std::to_string(d.statusPoints) + ';' + s.portraitKey + (d.portrait ? "p" : "") + ';' +
        std::to_string(show.hoverButton) + (show.hoverBlock ? "h" : "") + (show.warning ? "w" : "");
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
    const cb::Hit h = cb::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if (h.part == cb::Part::None) return false;
    if (m == WM_LBUTTONDOWN) {
        if (h.part == cb::Part::Portrait) {  // full, compact, the icon, full again
            s.view = cb::nextView(s.view);
            keepState(s);
            s.key.clear();
            LOG_INFO("character: the block %s", s.view == cb::View::Full      ? "full"
                                                : s.view == cb::View::Compact ? "compact"
                                                                              : "folded to its icon");
        } else if (h.part == cb::Part::Button && h.index >= 0 && h.index < cb::kButtons) {
            const bool sent = press(cb::kButton[h.index].command);
            if (s.clicksLogged++ < 10)
                LOG_INFO("character: the Basic Info window's button 0x%X %s", cb::kButton[h.index].command,
                         sent ? "pressed through the window"
                              : "not pressed - the window is not where this build has it");
        }
    }
    result = 0;
    return true;
}

}  // namespace uagfx::win::charpanel

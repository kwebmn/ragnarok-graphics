#include "win/char_make.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "core/window_stack.hpp"
#include "core/char_make.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/head_picture.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"
#include "win/window_stack.hpp"
#include "win/words.hpp"

namespace uagfx::win::charmakewin {
namespace {

namespace cm = uagfx::charmake;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the character make - the manager's windows' list (a node: next +0, previous +4, the window +8), the
// window's class's vtable (its constructor FUN_0045b7b0) and SendMsg; its name field (an edit control: its words
// FUN_00443300, thiscall), the stats, the hair's style and colour; the manager's control with the keys (+ 0x6C) and
// its SetFocus; the account's sex (as the character block reads it)
constexpr uint32_t kWindowMgr = 0x727ED8, kList = 0x38, kMakeClass = 0x697C3C, kMakeSend = 0x45C5F0;
constexpr uint32_t kNameEdit = 0x60, kStats = 0xC6, kHair = 0x9A, kColour = 0xAA, kRect = 0x14, kSendMsgSlot = 0x94;
constexpr uint32_t kGetText = 0x443300, kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510, kSex = 0x772DEC;
constexpr int kButtonMsg = 6, kAway = -4000;
constexpr size_t kMostWindows = 128;
constexpr ULONGLONG kBlinkMs = 530;
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);

// 3.0.18-beta.106 (the owner: «делай 1-3», the login screens): the character make live - the face, the hair's and
// the colour's ‹ ›, the name's field (the game's own, focused by a click), the three pairs' + with their bars,
// «Створити», «Скасувати» RmlUi's; each press the window's own act() (the game's buttons)
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw ckw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<div class="cols"><div class="lc"><div class="face"><img data-if="face != ''" data-attr-src="face"
  data-style-width="faceW + 'px'" data-style-height="faceH + 'px'"/></div>
<div class="lbl">{{hairWord}}</div><div class="chooser" data-class-focus="focusRow == 0"><span class="ar ctl"
  data-event-click="cm_act(2, -1)">&#x2039;</span><span class="val">{{hair}}</span><span class="ar ctl"
  data-event-click="cm_act(3, -1)">&#x203A;</span></div>
<div class="lbl">{{colourWord}}</div><div class="chooser" data-class-focus="focusRow == 1"><span class="ar ctl"
  data-event-click="cm_act(4, -1)">&#x2039;</span><span class="val">{{colour}}</span><span class="ar ctl"
  data-event-click="cm_act(5, -1)">&#x203A;</span></div></div>
<div class="rc"><div class="lbl">{{nameWord}}</div><div class="field ctl" data-class-typing="nameFocus"
  data-event-click="cm_act(1, -1)"><span data-if="name != ''">{{name}}</span><span class="caret"
  data-if="caret"></span><span class="ph" data-if="name == '' && !nameFocus">{{nameHint}}</span></div>
<div class="pair" data-for="p, i : pairs" data-class-focus="focusRow == i + 2"><span class="pt ctl"
  data-class-off="!p.canLeft" data-event-click="cm_act(6, i)">+</span><span class="sl">{{p.left}}</span><div
  class="bar"><div class="fill" data-style-width="p.share + '%'"></div></div><span class="sr">{{p.right}}</span><span
  class="pt ctl" data-class-off="!p.canRight" data-event-click="cm_act(7, i)">+</span></div>
<div class="hint">{{pairsHint}}</div><div class="hint">{{keysHint}}</div></div></div>
<div class="btns"><div class="btn ctl" data-event-click="cm_act(9, -1)">{{cancel}}</div>
<div class="btn main ctl" data-event-click="cm_act(8, -1)">{{make}}</div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
struct MakePairView {
    Rml::String left, right;
    float share = 50;
    bool canLeft = false, canRight = false;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<MakePairView> pairViews;  // 3.0.18-beta.106: the live window's
    Rml::String title, hairWord, colourWord, nameWord, nameHint, pairsHint, make, cancel, name, face, hair, colour;
    Rml::String keysHint;  // 3.0.18-beta.228
    Rml::String ground = "#00000000", facePrev;
    float faceW = 0, faceH = 0;
    bool nameFocus = false, caretOn = false;
    int focusNo = -1;
    std::string liveKey;
    uisheet::Sheet sheet;  // 3.0.18-beta.33: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, moved = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, oldX = 0;
    uint32_t window = 0;
    cm::Fields fields;
    std::map<std::tuple<int, int, int, int>, panel::Picture> faces;  // by sex, hair, colour, side
    cm::Show show;
    cm::Layout layout;
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

// The character make in the manager's list (it is kept nowhere else); none in the world
uint32_t gameMake() {
    if (inGameWorld()) return 0;
    const uint32_t head = read32(kWindowMgr + kList);
    if (!head) return 0;
    uint32_t node = read32(head);
    for (size_t n = 0; n < kMostWindows && node && node != head; ++n) {
        const uint32_t w = read32(node + 8);
        if (w && read32(w) == kMakeClass && read32(read32(w) + kSendMsgSlot) == kMakeSend) return w;
        node = read32(node);
    }
    return 0;
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
        if (!s.moved)
            LOG_INFO("character make: the game's window off the screen for the new one (its field keeps the keys)");
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        s.moved = false;
    }
}

uint16_t read16(uint32_t at) {
    uint16_t v = 0;
    return readGame(at, &v, 2) ? v : 0;
}

cm::Fields fieldsOf(State& s, uint32_t window) {
    cm::Fields f;
    const uint32_t edit = read32(window + kNameEdit);
    if (edit) {
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
        char buf[64] = {};
        if (t && readGame(uint32_t(reinterpret_cast<uintptr_t>(t)), buf, sizeof buf - 1)) {
            buf[sizeof buf - 1] = 0;
            f.name = utf8Of(widened(buf, s.codePage));
        }
    }
    f.focus = edit && read32(kFocus) == edit;
    f.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
    uint8_t stats[6] = {};
    if (readGame(window + kStats, stats, sizeof stats))
        for (int i = 0; i < 6; ++i) f.stats[i] = stats[i];
    f.hair = int16_t(read16(window + kHair));
    f.colour = int16_t(read16(window + kColour));
    // 3.0.18-beta.222: at the window's own size too (its corner pulled) - the window laid out at it, no transform
    const int sex = read32(kSex) != 0 ? 1 : 0,
              side = cm::faceSide(s.scale * windowstack::sizeOf(winstack::Win::CharMake));
    const auto key = std::make_tuple(sex, f.hair, f.colour, side);
    auto it = s.faces.find(key);
    if (it == s.faces.end()) {
        panel::Picture p;
        headpicture::make(sex, f.hair, f.colour, side, p);
        it = s.faces.emplace(key, std::move(p)).first;
    }
    f.face = it->second.width > 0 ? &it->second : nullptr;
    return f;
}

cm::Texts texts() {
    cm::Texts t;
    t.title = say(settings::Text::MakeTitle);
    t.name = say(settings::Text::MakeName);
    t.nameHint = say(settings::Text::MakeNameHint);
    t.hair = say(settings::Text::MakeHair);
    t.colour = say(settings::Text::MakeColour);
    t.pairsHint = say(settings::Text::MakePairsHint);
    t.keysHint = say(settings::Text::MakeKeysHint);  // 3.0.18-beta.228
    t.make = say(settings::Text::CharsMake);
    t.cancel = say(settings::Text::MsgCancel);
    const char* const stats[6] = {"STR", "AGI", "VIT", "INT", "DEX", "LUK"};
    for (int i = 0; i < 6; ++i) t.stats[i] = stats[i];
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
    uapad::gamewindow::setBlocked(Blocker::CharMake, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::CharMake, nullptr, nullptr, false);
    s.shown = false;
}

void send(State& s, int id, int times, const char* what) {
    const uint32_t window = gameMake();
    if (!window) return;
    if (s.told++ < 40) LOG_INFO("character make: %s - the game's 0x%X%s", what, unsigned(id), times > 1 ? " x8" : "");
    const auto fn = reinterpret_cast<SendMsgFn>(uintptr_t(read32(read32(window) + kSendMsgSlot)));
    for (int i = 0; i < times; ++i) fn(window, 0, kButtonMsg, id, 0, 0);
    s.key.clear();
}

void act(State& s, cm::Part p, int index) {
    switch (p) {
        case cm::Part::Name: {
            const uint32_t window = gameMake();
            const uint32_t edit = window ? read32(window + kNameEdit) : 0;
            if (edit) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
            s.key.clear();
            break;
        }
        case cm::Part::HairBefore: send(s, cm::hairButton(false), 1, "the hair before"); break;
        case cm::Part::HairAfter: send(s, cm::hairButton(true), 1, "the hair after"); break;
        case cm::Part::ColourBefore:
            send(s, cm::kColourButton, cm::colourPresses(false), "the hair's colour before");
            break;
        case cm::Part::ColourAfter:
            send(s, cm::kColourButton, cm::colourPresses(true), "the hair's colour after");
            break;
        case cm::Part::PairLeft:
        case cm::Part::PairRight: {
            const bool toLeft = p == cm::Part::PairLeft;
            if (cm::canMove(s.fields, index, toLeft)) send(s, cm::pointButton(index, toLeft), 1, "a point moved");
            break;
        }
        case cm::Part::Make: send(s, cm::kMakeButton, 1, "«Створити»"); break;
        case cm::Part::Cancel: send(s, cm::kCancelButton, 1, "«Скасувати»"); break;
        case cm::Part::None: break;
    }
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("character make: [interface] char_make = %d", on ? 1 : 0);
    }
    const uint32_t window = gameMake();
    hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        s.window = 0;
        return;
    }
    if (!s.shown || window != s.window) {
        LOG_INFO("character make: the game's window - the new one shows it (the account's sex %u)",
                 unsigned(read32(kSex)));
        s.show.focusRow = -1;
    }
    s.window = window;
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.fields = fieldsOf(s, window);
    s.layout = cm::layout(targetWidth, targetHeight, s.scale);
    s.show.hover = cm::Part::None;
    s.show.hoverIndex = -1;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const cm::Hit h = cm::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        s.show.hover = h.part;
        s.show.hoverIndex = h.index;
    }
    const cm::Fields& f = s.fields;
    std::string key = f.name + ';' + (f.focus ? "f" : "") + (f.caret ? "|" : "") + ';' + std::to_string(f.hair) + ',' +
                      std::to_string(f.colour) + (f.face ? "p" : "") + ';' + std::to_string(int(s.show.hover)) + ',' +
                      std::to_string(s.show.hoverIndex) + ',' + std::to_string(s.show.focusRow) + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (int st : f.stats) key += ',' + std::to_string(st);
    if (const std::string drawn = key + (uaui::readyAnywhere() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.33
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.anywhere = true;  // before the world
    s.live.make("charmake", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<MakePairView>()) {
            v.RegisterMember("left", &MakePairView::left);
            v.RegisterMember("right", &MakePairView::right);
            v.RegisterMember("share", &MakePairView::share);
            v.RegisterMember("canLeft", &MakePairView::canLeft);
            v.RegisterMember("canRight", &MakePairView::canRight);
        }
        m.RegisterArray<std::vector<MakePairView>>();
        m.Bind("pairs", &s.pairViews);
        for (auto [n, v] : std::initializer_list<std::pair<const char*, Rml::String*>>{
                 {"title", &s.title}, {"hairWord", &s.hairWord}, {"colourWord", &s.colourWord},
                 {"nameWord", &s.nameWord}, {"nameHint", &s.nameHint}, {"pairsHint", &s.pairsHint},
                 {"make", &s.make}, {"cancel", &s.cancel}, {"name", &s.name}, {"face", &s.face},
                 {"hair", &s.hair}, {"colour", &s.colour}, {"ground", &s.ground}, {"keysHint", &s.keysHint}})
            m.Bind(n, v);
        m.Bind("faceW", &s.faceW);
        m.Bind("faceH", &s.faceH);
        m.Bind("nameFocus", &s.nameFocus);
        m.Bind("caret", &s.caretOn);
        m.Bind("focusRow", &s.focusNo);
        // a part of core charmake's (its Part's number) and its index - the window's own act()
        m.BindEventCallback("cm_act", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (a.size() < 2) return;
            State& st = state();
            st.show.focusRow = -1;  // the mouse leads now
            act(st, cm::Part(a[0].Get<int>()), a[1].Get<int>());
        });
    });
    if (!s.live.made()) return;
    const cm::Texts t = texts();
    const cm::Fields& f = s.fields;
    std::string key = f.name + (f.focus ? "f" : "") + (f.focus && f.caret ? "|" : "") + ';' + std::to_string(f.hair) +
                      ',' + std::to_string(f.colour) + ',' + std::to_string(uintptr_t(f.face)) + ';' +
                      std::to_string(s.show.focusRow) + ';' + t.title + t.make;
    for (int st : f.stats) key += ',' + std::to_string(st);
    if (key != s.liveKey) {
        s.liveKey = key;
        s.title = t.title, s.hairWord = t.hair, s.colourWord = t.colour, s.nameWord = t.name;
        s.nameHint = t.nameHint, s.pairsHint = t.pairsHint, s.make = t.make, s.cancel = t.cancel;
        s.keysHint = t.keysHint;
        s.name = f.name, s.nameFocus = f.focus, s.caretOn = f.focus && f.caret;
        s.hair = std::to_string(f.hair), s.colour = std::to_string(f.colour), s.focusNo = s.show.focusRow;
        s.face = f.face && f.face->width > 0 ? uaui::picture(f.face, uaui::stamp(f.face), s.facePrev) : "";
        s.faceW = f.face ? float(f.face->width) : 0, s.faceH = f.face ? float(f.face->height) : 0;
        s.pairViews.resize(3);
        for (int i = 0; i < 3; ++i) {
            const int a = cm::kPairs[i].left, b = cm::kPairs[i].right;
            MakePairView& v = s.pairViews[size_t(i)];
            v.left = t.stats[a] + " " + std::to_string(f.stats[a]);
            v.right = std::to_string(f.stats[b]) + " " + t.stats[b];
            v.share = f.stats[a] + f.stats[b] > 0 ? 100.0f * float(f.stats[a]) / float(f.stats[a] + f.stats[b]) : 50;
            v.canLeft = cm::canMove(f, i, true);
            v.canRight = cm::canMove(f, i, false);
        }
        s.ground = uisheet::Sheet::ground(0x10141C, std::max(0.94f, uistyle::window()));
        s.live.doc.dirtyAll();
    }
    int x = s.layout.x, y = s.layout.y, bx = 0, by = 0, bw = 0, bh = 0;
    if (s.live.box(bx, by, bw, bh)) x = (s.targetW - bw) / 2, y = (s.targetH - bh) / 2;
    s.live.frame(s.shown, x, y, int(winstack::Win::CharMake), ".ar, .field, .pt, .btn");
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

void beforeGameDraw() {
    State& s = state();
    if (s.on) hold(s, gameMake(), true);
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    int& row = s.show.focusRow;
    const bool side = c == PadCommand::Left || c == PadCommand::Right;
    switch (c) {
        case PadCommand::Up: row = row <= 0 ? cm::kRows - 1 : row - 1; break;
        case PadCommand::Down: row = row < 0 || row >= cm::kRows - 1 ? 0 : row + 1; break;
        case PadCommand::Make: act(s, cm::Part::Make, -1); break;
        case PadCommand::Cancel: act(s, cm::Part::Cancel, -1); break;
        default: break;
    }
    if (side) {
        if (row < 0) row = 0;
        const bool after = c == PadCommand::Right;
        if (row == 0) act(s, after ? cm::Part::HairAfter : cm::Part::HairBefore, -1);
        else if (row == 1) act(s, after ? cm::Part::ColourAfter : cm::Part::ColourBefore, -1);
        else act(s, after ? cm::Part::PairRight : cm::Part::PairLeft, row - 2);
    }
    s.key.clear();
}

bool key(UINT m, WPARAM wp, LPARAM) {  // 3.0.18-beta.228
    State& s = state();
    if (!s.shown || (m != WM_KEYDOWN && m != WM_KEYUP)) return false;
    if (wp != VK_UP && wp != VK_DOWN && wp != VK_LEFT && wp != VK_RIGHT) return false;
    if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0) return false;
    const bool sides = wp == VK_LEFT || wp == VK_RIGHT;
    if (sides && !cm::keySides(s.show.focusRow)) return false;  // the name's: its caret, the game's
    if (m == WM_KEYUP) return true;
    if (sides) {
        padCommand(wp == VK_RIGHT ? PadCommand::Right : PadCommand::Left);
    } else {
        s.show.focusRow = cm::keyRow(s.show.focusRow, wp == VK_DOWN);
        s.key.clear();
    }
    if (s.told++ < 40) LOG_INFO("character make: a key - the row %d (-1 the name)", s.show.focusRow);
    return true;
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

}  // namespace uagfx::win::charmakewin

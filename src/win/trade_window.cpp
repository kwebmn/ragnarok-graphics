#include "win/trade_window.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/card_names.hpp"
#include "core/hotbar_panel.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/trade_window.hpp"
#include "core/ui_style.hpp"
#include "core/window_drag.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/client_tables.hpp"
#include "win/game_memory.hpp"
#include "win/info_card.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/item_window.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_input.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "uikit/trade_live.hpp"
#include "win/live_part.hpp"
#include "win/ui_sheet.hpp"
#include "win/window_stack.hpp"
#include "win/words.hpp"

namespace uagfx::win::tradewin {
namespace {

namespace tw = uagfx::tradewin;
namespace iw = uagfx::itemwin;

// uaRO.exe's: the windows' manager's trade window (UI window 0x1F, made at the trade's start - 0x00e7 result 3 - and
// deleted at its end; its class's vtable, its SendMsg FUN_004840c0 - message 6 with its buttons: 0x76 «OK» (its zeny
// field's words added to the zeny offered, then the lock: the game mode's 0x38 and 0x39), 0xA4 «Trade» (0x3B), 0x77
// «Cancel» (0x3A), 0xA0 the zeny field's arrow (0x38) - and its drop FUN_004848a0: a bag thing's drag, the count window
// for a stack, refused once locked); its zeny field (+0x6C, an edit: SetText at its vtable + 0x98), the partner's name
// (an std::string: c_str +0xA0, length +0xA4), the locks (+0x70 yours by the server, +0x74 the partner's, +0x78 your
// «OK» pressed, +0x7C «Trade» pressed); the item card's window (0xC, its message 0x18 an ITEM_INFO), MakeWindow
constexpr uint32_t kWindowMgr = 0x727ED8, kTradeAt = kWindowMgr + 0x10C, kTradeClass = 0x699A98;
constexpr uint32_t kShown = 0x38, kSendMsgSlot = 0x94, kTradeSendMsg = 0x4840C0, kDropSlot = 0x2C;
constexpr uint32_t kTradeDrop = 0x4848A0, kMakeWindow = 0x4D42D0;
constexpr uint32_t kZenyEdit = 0x6C, kMineLocked = 0x70, kTheirsLocked = 0x74, kOkPressed = 0x78, kTraded = 0x7C;
constexpr uint32_t kNameAt = 0xA0, kNameLength = 0xA4, kSetTextSlot = 0x98, kSetText = 0x446210;
constexpr int kButtonMsg = 6, kOkButton = 0x76, kTradeButton = 0xA4, kCancelButton = 0x77, kZenyButton = 0xA0;
constexpr int kItemCard = 0xC, kShowItem = 0x18;
// The game mode (its SendMsg FUN_005707d0 at vtable + 0x14): 0x11 a bag thing's drag begun (as «Речі»'s press), its
// drag at +0x32C (the kind first)
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0, kDragInfo = 0x32C;
constexpr int kBagDragMsg = 0x11;
// The session's trade: your ten ITEM_INFOs (72 bytes each, a slot empty with its count 0) and the partner's, the zeny
// offered by each; your zeny
constexpr uint32_t kSession = 0x771148, kMine = kSession + 0x1290, kTheirs = kSession + 0x1560, kInfoStride = 0x48;
constexpr uint32_t kMineZeny = kSession + 0x1830, kTheirZeny = kSession + 0x1834, kZenyNow = 0x772038;
constexpr size_t kInfoSize = 0x48;
constexpr ULONGLONG kReadMs = 200, kBlinkMs = 530, kTipMs = 400;
constexpr size_t kMostDigits = 10;

using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);
using MakeFn = uint32_t(__attribute__((thiscall)) *)(uint32_t manager, int id);
using DropFn = void(__attribute__((thiscall)) *)(uint32_t self, int x, int y, uint32_t drag);
using SetTextFn = void(__attribute__((thiscall)) *)(uint32_t edit, const char* text);

struct State {
    // 3.0.18-beta.95 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): live - laid out by RmlUi
    LivePart<uikit::trade::Model> live;
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    WindowEdge edge;
    int tipOf = -1;
    ULONGLONG tipSince = 0;
    bool on = false, shown = false, hid = false, stateRead = false, bagAsked = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    float fx = 0.30f, fy = 0.25f;
    tw::Side me, them, themBefore;
    tw::Late late;
    std::string partner;
    long long zenyNow = 0;
    ULONGLONG readAt = 0;
    tw::Show show;
    tw::Layout layout;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    bool moving = false;
    float grabX = 0, grabY = 0;
    int told = 0, pressesTold = 0;
    int padFocus = 0;
    bool padNow = false;
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

uint32_t gameWindow() {
    const uint32_t w = read32(kTradeAt);
    return w && read32(w) == kTradeClass ? w : 0;
}

void hold(State& s, uint32_t window, bool on) {
    if (!window) {
        s.hid = false;
        return;
    }
    if (on) {
        if (read32(window + kShown) && write32(window + kShown, 0) && !s.hid) {
            LOG_INFO("trade: the game's trade window hidden for «Обмін» (its shown flag 0)");
            s.hid = true;
        }
    } else if (s.hid) {
        write32(window + kShown, 1);
        s.hid = false;
        LOG_INFO("trade: the game's trade window back");
    }
}

uint32_t modeNow() {
    const uint32_t mode = read32(kModeNow);
    return mode && inGameWorld() && read32(read32(mode) + kModeMsgSlot) == kModeMsg ? mode : 0;
}

void readState(State& s) {
    if (s.stateRead) return;
    s.stateRead = true;
    std::string text;
    if (!readTextFile(layout::join(gameDir(), layout::kTradeState), text)) return;
    float x = 0, y = 0;
    if (std::sscanf(text.c_str(), "at %f %f", &x, &y) == 2 && x >= 0 && x <= 1 && y >= 0 && y <= 1) {
        s.fx = x;
        s.fy = y;
    }
}

void keepState(const State& s) {
    char text[48];
    std::snprintf(text, sizeof text, "at %.4f %.4f\n", s.fx, s.fy);
    const std::string path = layout::join(gameDir(), layout::kTradeState);
    if (!writeFileAtomic(path, std::string(text))) LOG_INFO("trade: %s cannot be written", path.c_str());
}

// One side's ten slots as the session has them
void readSide(State& s, uint32_t base, tw::Side& side) {
    const auto& names = clienttables::itemNames();
    const auto& unknown = clienttables::unknownNames();
    const auto& files = clienttables::itemFiles();
    for (int i = 0; i < tw::kSlots; ++i) {
        tw::Thing& x = side.slots[i];
        x = tw::Thing{};
        uint8_t raw[kInfoSize] = {};
        if (!readGame(base + uint32_t(i) * kInfoStride, raw, sizeof raw)) continue;
        const int count = int(rd32(raw, 0x10));
        const uint32_t ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
        if (count <= 0 || !ptr || !len || len > 16) continue;
        char digits[17] = {};
        if (!readGame(ptr, digits, len)) continue;
        x.id = std::atoi(digits);
        x.count = count;
        x.identified = raw[0x3C] != 0;
        const int refine = int(rd32(raw, 0x40));
        x.refine = refine > 0 && refine <= 20 ? refine : 0;
        const std::string id = std::to_string(x.id);
        std::string words;
        if (!x.identified) {
            const auto u = unknown.find(id);
            if (u != unknown.end()) words = u->second;
        }
        if (words.empty()) {
            const auto w = names.find(id);
            if (w != names.end()) words = w->second;
        }
        x.name = words.empty() ? "#" + id : utf8Of(widened(hotbar::spaced(words), s.codePage));
        // 3.0.18-beta.118 (the owner: «в вендинге не видно карт»): its cards in its name, as the game names it
        x.name = cardnames::namedFromInfo(x.name, raw, clienttables::cardWords());
        x.name = infocardwin::withSlots(x.name, x.id, raw);  // 3.0.18-beta.145: «Jur [3]»
        const auto f = files.find(id);
        if (f != files.end()) x.icon = clienttables::icon(f->second, iw::iconSide(s.scale * 0.85f), 4 * s.scale);
    }
}

void readTrade(State& s, uint32_t window) {
    readSide(s, kMine, s.me);
    readSide(s, kTheirs, s.them);
    s.me.zeny = int32_t(read32(kMineZeny));
    s.them.zeny = int32_t(read32(kTheirZeny));
    s.zenyNow = int32_t(read32(kZenyNow));
    s.me.ready = read32(window + kMineLocked) != 0 || read32(window + kOkPressed) != 0;
    s.them.ready = read32(window + kTheirsLocked) != 0;
    s.show.traded = read32(window + kTraded) != 0;
    const uint32_t ptr = read32(window + kNameAt), len = read32(window + kNameLength);
    char name[32] = {};
    if (ptr && len > 0 && len < sizeof name && readGame(ptr, name, len))
        s.partner = utf8Of(widened(std::string(name, len), s.codePage));
    const bool wasLate = s.late.any();
    s.late = tw::watchLate(s.themBefore, s.them, s.me.ready, s.late);
    if (s.late.any() && !wasLate && s.told++ < 30)
        LOG_INFO("trade: the partner changed the offer after your «Готово» - shown in red with the warning");
    s.themBefore = s.them;
}

tw::Texts texts(bool pad = false);

tw::Texts texts(bool pad) {
    tw::Texts t;
    t.title = say(settings::Text::TradeTitle);
    t.partner = state().partner;
    t.you = say(settings::Text::TradeYou);
    t.zeny = say(settings::Text::TradeZeny);
    t.ready = say(settings::Text::TradeReady);
    t.trade = say(settings::Text::TradeDo);
    t.cancel = say(settings::Text::TradeCancel);
    t.late = say(settings::Text::TradeLate);
    t.hint = say(settings::Text::TradeHint);
    t.info = say(settings::Text::ItemsInfo);
    if (pad) {
        std::string h = say(settings::Text::TradePadHint);
        if (const size_t at = h.find("{1}"); at != std::string::npos) h.replace(at, 3, uapad::buttonLabel(0));
        if (const size_t at = h.find("{2}"); at != std::string::npos) h.replace(at, 3, uapad::buttonLabel(1));
        t.hint = h;
        t.info = std::string(uapad::buttonLabel(3)) + ": " + say(settings::Text::ItemsPadCard);
    }
    return t;
}

void render(State& s) {
    ++s.sheetGen;
}

void block(const State& s, HWND game) {
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.glass;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Trade, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Trade, nullptr, nullptr, false);
    s.shown = false;
    s.moving = false;
}

void place(State& s) {
    s.layout = tw::layout(0, 0, s.scale);
    if (s.live.shown()) uikit::trade::refit(s.live.doc.doc(), s.layout);  // 3.0.18-beta.95: its drawn boxes
    const int roomX = std::max(0, s.targetW - s.layout.w), roomY = std::max(0, s.targetH - s.layout.h);
    s.layout.x = int(std::lround(s.fx * float(roomX)));
    s.layout.y = int(std::lround(s.fy * float(roomY)));
}

// One of the game's window's own buttons pressed; «OK» and the zeny field's arrow add its field's words to the zeny
// offered - the zeny typed less what is offered put there first (the field empty again after the arrow)
const char* press(State& s, int button) {
    const uint32_t window = gameWindow();
    if (!window || read32(read32(window) + kSendMsgSlot) != kTradeSendMsg) return "the game's trade window not found";
    const uint32_t edit = read32(window + kZenyEdit);
    const bool zeny = button == kOkButton || button == kZenyButton;
    const bool editable = edit && read32(read32(edit) + kSetTextSlot) == kSetText;
    long long add = 0;
    if (zeny && editable) {
        const long long want = s.show.typed.empty() ? s.me.zeny : std::atoll(s.show.typed.c_str());
        add = std::clamp(want - s.me.zeny, 0LL, std::max(0LL, s.zenyNow));
        reinterpret_cast<SetTextFn>(uintptr_t(kSetText))(edit, std::to_string(add).c_str());
    }
    reinterpret_cast<SendMsgFn>(uintptr_t(kTradeSendMsg))(window, 0, kButtonMsg, button, 0, 0);
    if (button == kZenyButton && editable && gameWindow() == window && read32(window + kZenyEdit) == edit)
        reinterpret_cast<SetTextFn>(uintptr_t(kSetText))(edit, "");
    s.show.typing = false;
    s.show.typed.clear();
    s.readAt = 0;
    if (s.told++ < 30)
        LOG_INFO("trade: its button 0x%X pressed%s", unsigned(button),
                 zeny && add > 0 ? (" (+" + std::to_string(add) + " zeny offered)").c_str() : "");
    return "pressed";
}

void next(State& s, const char* how) {  // «Готово», then «Обмін»
    if (tw::canReady(s.me)) {
        press(s, kOkButton);
    } else if (tw::canTrade(s.me, s.them, s.show.traded)) {
        press(s, kTradeButton);
    } else if (s.told++ < 30) {
        LOG_INFO("trade: %s - nothing to press yet (the partner not ready)", how);
    }
}

bool card(int cell) {
    if (cell < 0 || cell >= 2 * tw::kSlots) return false;
    const uint32_t info = (cell < tw::kSlots ? kMine : kTheirs) + uint32_t(cell % tw::kSlots) * kInfoStride;
    const uint32_t window = reinterpret_cast<MakeFn>(uintptr_t(kMakeWindow))(kWindowMgr, kItemCard);
    const uint32_t send = window ? read32(read32(window) + kSendMsgSlot) : 0;
    if (!send) return false;
    reinterpret_cast<SendMsgFn>(uintptr_t(send))(window, 0, kShowItem, int(info), 0, 0);
    return true;
}

}  // namespace

static void frameInner(HWND game, bool on, unsigned codePage, double k, int targetWidth,
                       int targetHeight) {
    State& s = state();
    s.codePage = codePage;
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("trade: [interface] trade = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameWindow() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        s.show.typing = false;
        s.bagAsked = false;
        hide(s);
        return;
    }
    readState(s);
    s.k = float(k);
    // 3.0.18-beta.151: times its own size (its corner pulled)
    s.scale = uistyle::readingScale(k) * windowstack::sizeOf(winstack::Win::Trade);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const ULONGLONG now = GetTickCount64();
    if (!s.shown) {  // a trade begun: «Речі» beside it, the partner's changes watched anew
        s.bagAsked = true;
        s.readAt = 0;
        s.late = tw::Late{};
        s.themBefore = tw::Side{};
        s.show.typed.clear();
        s.padFocus = 0;
    }
    if (now - s.readAt >= kReadMs) {
        s.readAt = now;
        const bool first = !s.shown;
        readTrade(s, window);
        if (first) LOG_INFO("trade: a trade with %s begun - «Обмін» shows it", s.partner.c_str());
    }
    place(s);
    s.show.caretOn = (now / kBlinkMs) % 2 == 0;
    s.show.late = s.late;
    s.show.hover = -1;
    s.show.hoverButton = -1;
    s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy) && !s.moving) {
        const float lx = float(cx) * float(k) - float(s.layout.x), ly = float(cy) * float(k) - float(s.layout.y);
        const tw::Hit h = tw::hitTest(s.layout, lx, ly);
        if (h.part == tw::Part::Cell) s.show.hover = h.index;
        if (h.part == tw::Part::Button) s.show.hoverButton = h.index;
        s.show.hoverClose = h.part == tw::Part::Close;
    }
    if (s.show.hover != s.tipOf) {
        s.tipOf = s.show.hover;
        s.tipSince = now;
    }
    s.show.tip = s.show.hover >= 0 && now - s.tipSince >= kTipMs;
    s.padNow = uapad::runtime::navLook().shown;
    s.show.dropping = windrag::now().kind == windrag::Kind::Item;
    if (s.padNow) {
        s.padFocus = std::clamp(s.padFocus, 0, 2 * tw::kSlots - 1);
        s.show.hover = s.padFocus;
        s.show.tip = true;
    }
    std::string key = std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + ';' +
                      std::to_string(s.scale) + ';' + std::to_string(s.show.hover) + (s.show.tip ? "t" : "") + ',' +
                      std::to_string(s.show.hoverButton) + (s.show.hoverClose ? "x" : "") + ';' + s.show.typed +
                      (s.show.typing ? "|t" : "") + (s.show.typing && s.show.caretOn ? "c" : "") +
                      (s.show.dropping ? "D" : "") + (s.show.traded ? "T" : "") + (s.padNow ? "P" : "") + ';' +
                      s.partner + ';' + std::to_string(s.me.zeny) + '/' + std::to_string(s.them.zeny) +
                      (s.me.ready ? "r" : "") + (s.them.ready ? "R" : "") + (s.late.any() ? "L" : "") + ';';
    for (const tw::Side* side : {&s.me, &s.them})
        for (const tw::Thing& x : side->slots)
            key += std::to_string(x.id) + 'x' + std::to_string(x.count) + '+' + std::to_string(x.refine) +
                   (x.icon ? "i" : "") + ',';
    for (const bool late : s.late.slots) key += late ? '1' : '0';
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()) + (uaui::ready() ? "u" : "");
        drawn != s.key) {  // 3.0.18-beta.28: uaUI's or the picture
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    namespace tl = uikit::trade;
    const tw::Texts t = texts(s.padNow);
    s.sheetShown = s.live.frame("trade", s.shown, s.layout.x, s.layout.y, s.sheetGen, tl::rml(), tl::bind,
                                [&](tl::Model& m) {
                                    m.ground = s.live.ground();
                                    tl::fill(m, s.me, s.them, s.show, t, s.layout.s,
                                             [&](const panel::Picture* p) { return s.live.icon(p); });
                                });
    if (s.sheetShown && s.live.model.tip) {  // beside the pointed cell, by the tip's own size as laid out last
        const float tx = s.live.model.tipX, ty = s.live.model.tipY;
        tl::placeTip(s.live.doc.doc(), s.live.model, s.layout, s.show.hover);
        if (tx != s.live.model.tipX || ty != s.live.model.tipY) s.live.doc.dirty("tipX"), s.live.doc.dirty("tipY");
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    s.live.raise();  // 3.0.18-beta.43: at its place in the windows' stack
    if (!s.shown || s.k <= 0 || !s.sheetShown) return;  // the old edition (its picture) gone: uaUI draws it
    int cursor = -1, cx = 0, cy = 0;
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    const panel::Box& g = s.layout.glass;
    // 3.0.18-beta.43: the shadow the driver's, under uaUI's window too
    s.shadow.draw(record, renderer, s.layout.x + int(g.x), s.layout.y + int(g.y), int(g.w), int(g.h), 12 * s.scale,
                  s.scale, s.k);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

bool drop(int x, int y, uint32_t drag) {
    State& s = state();
    if (!s.shown || s.k <= 0) return false;
    const float tx = float(x) * s.k - float(s.layout.x), ty = float(y) * s.k - float(s.layout.y);
    if (!s.layout.glass.contains(tx, ty)) return false;
    const uint32_t window = gameWindow();
    if (!window || read32(read32(window) + kDropSlot) != kTradeDrop) return false;
    reinterpret_cast<DropFn>(uintptr_t(kTradeDrop))(window, 0, 0, drag);
    if (s.told++ < 30) LOG_INFO("trade: a drag let go over «Обмін» - given to the game's trade window's drop");
    s.readAt = 0;
    return true;
}

bool put(int index, int count) {
    State& s = state();
    const uint32_t window = s.shown ? gameWindow() : 0;
    const uint32_t mode = modeNow();
    if (!window || !mode || s.me.ready || read32(read32(window) + kDropSlot) != kTradeDrop) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, kBagDragMsg, index, count, 0);  // the drag begun
    if (read32(mode + kDragInfo) != 1) return false;
    reinterpret_cast<DropFn>(uintptr_t(kTradeDrop))(window, 0, 0, mode + kDragInfo);
    s.readAt = 0;
    return true;
}

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    (void)wp;  // nothing scrolls in it
    if (!s.shown) return false;
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL && m != WM_MOUSEMOVE) return false;
    float tx = 0, ty = 0;
    bool fromGame = false;
    if (!pointerInTarget(game, m, lp, s.k, s.targetW, s.targetH, tx, ty, &fromGame)) return false;
    if (s.moving && (m == WM_MOUSEMOVE || m == WM_LBUTTONUP)) {  // its title dragged: it follows the cursor
        const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
        s.fx = std::clamp((tx - s.grabX) / float(roomX), 0.0f, 1.0f);
        s.fy = std::clamp((ty - s.grabY) / float(roomY), 0.0f, 1.0f);
        if (m == WM_LBUTTONUP) {
            s.moving = false;
            keepState(s);
        }
        result = 0;
        return true;
    }
    const tw::Hit h = tw::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    const bool pressed = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN;
    if (pressed && s.pressesTold < 6) {
        ++s.pressesTold;
        int cx = -1, cy = -1;
        readGameCursor(cx, cy);
        LOG_INFO("trade: a press (0x%X), the game's cursor (%d, %d) - the target's (%.0f, %.0f) by the %s; «Обмін» at "
                 "%d, %d %dx%d: part %d", m, cx, cy, tx, ty, fromGame ? "game's cursor" : "message's point",
                 s.layout.x, s.layout.y, s.layout.w, s.layout.h, int(h.part));
    }
    if (h.part == tw::Part::None) {
        if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) s.show.typing = false;
        return false;
    }
    if (m == WM_MOUSEMOVE) return false;
    result = 0;
    if (m == WM_RBUTTONDOWN && h.part == tw::Part::Cell) {
        const bool shownCard = card(h.index);
        if (s.told++ < 30) LOG_INFO("trade: cell %d's card %s", h.index, shownCard ? "asked" : "not shown");
        return true;
    }
    if (m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) return true;
    if (h.part != tw::Part::Zeny) s.show.typing = false;
    switch (h.part) {
        case tw::Part::Close:
        case tw::Part::Button:
            if (h.part == tw::Part::Close || h.index == 2) {
                press(s, kCancelButton);
            } else if (h.index == 0 && tw::canReady(s.me)) {
                press(s, kOkButton);
            } else if (h.index == 1 && tw::canTrade(s.me, s.them, s.show.traded)) {
                press(s, kTradeButton);
            }
            break;
        case tw::Part::Zeny:
            if (!s.me.ready) {
                s.show.typing = true;
                s.show.typed.clear();
            }
            break;
        case tw::Part::Title:
            s.moving = true;
            s.grabX = tx - float(s.layout.x);
            s.grabY = ty - float(s.layout.y);
            break;
        default:
            break;
    }
    s.key.clear();
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    (void)lp;
    if (!s.shown || !s.show.typing) return false;
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
        if (GetKeyState(VK_MENU) < 0) return false;  // Alt+E and the rest: the game's
        if (wp == VK_ESCAPE) {
            s.show.typing = false;
            s.show.typed.clear();
        } else if (wp == VK_RETURN) {
            press(s, kZenyButton);  // shown to the partner at once, not locked
        } else if (wp == VK_BACK) {
            if (!s.show.typed.empty()) s.show.typed.pop_back();
        } else {
            const char c = wp >= '0' && wp <= '9'                 ? char(wp)
                           : wp >= VK_NUMPAD0 && wp <= VK_NUMPAD9 ? char('0' + (wp - VK_NUMPAD0))
                                                                  : 0;
            if (c && s.show.typed.size() < kMostDigits && !(c == '0' && s.show.typed.empty())) s.show.typed += c;
        }
        return true;  // every key the field's while it types
    }
    return m == WM_KEYUP || m == WM_CHAR || m == WM_DEADCHAR;
}

bool shown() { return state().shown; }

void beforeGameDraw() {
    State& s = state();
    if (!s.on || !inGameWorld()) return;
    if (const uint32_t window = gameWindow()) hold(s, window, true);
    if (s.bagAsked) {
        s.bagAsked = false;
        itemswin::open();
    }
}

void padCommand(PadCommand c) {
    State& s = state();
    if (!s.shown) return;
    const int cell = s.padFocus, side = cell / tw::kSlots, at = cell % tw::kSlots;
    switch (c) {
        case PadCommand::Up:
            if (at >= tw::kColumns) s.padFocus -= tw::kColumns;
            break;
        case PadCommand::Down:
            if (at < tw::kColumns) s.padFocus += tw::kColumns;
            break;
        case PadCommand::Left:
            if (at % tw::kColumns > 0) {
                --s.padFocus;
            } else if (side == 1) {  // across to your side's last column
                s.padFocus = at + tw::kColumns - 1;
            } else {
                windowstack::padCross(winstack::Win::Trade, false);
            }
            break;
        case PadCommand::Right:
            if (at % tw::kColumns < tw::kColumns - 1) {
                ++s.padFocus;
            } else if (side == 0) {  // across to the partner's first column
                s.padFocus = tw::kSlots + at - (tw::kColumns - 1);
            } else {
                windowstack::padCross(winstack::Win::Trade, true);
            }
            break;
        case PadCommand::Next: next(s, "the controller's A"); break;
        case PadCommand::Card: card(s.padFocus); break;
        case PadCommand::Cancel: press(s, kCancelButton); break;
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

void moveTo(int x, int y, bool keep) {
    State& s = state();
    if (!s.shown) return;
    const int roomX = std::max(1, s.targetW - s.layout.w), roomY = std::max(1, s.targetH - s.layout.h);
    s.fx = std::clamp(float(x) / float(roomX), 0.0f, 1.0f);
    s.fy = std::clamp(float(y) / float(roomY), 0.0f, 1.0f);
    s.layout.x = x;
    s.layout.y = y;
    if (keep) keepState(s);
}

bool moving() { return state().shown && state().moving; }

}  // namespace uagfx::win::tradewin

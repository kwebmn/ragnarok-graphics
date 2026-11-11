#include "win/macro_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_stack.hpp"
#include "core/macro_card.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/ui_cost.hpp"
#include "win/window_shadow.hpp"
#include "win/uaui.hpp"
#include "win/ui_live.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::macrocardwin {
namespace {

namespace mc = uagfx::macrocard;
using uapad::gamewindow::Blocker;

// uaRO.exe's: the macro window (where the manager keeps it, its class's vtable, its SendMsg), a window's place; an
// edit control's words (thiscall); the manager's control with the keys (+0x6C) and its SetFocus
constexpr uint32_t kWindowMgr = 0x727ED8, kMacroAt = kWindowMgr + 0x1A8, kMacroClass = 0x69C288;
constexpr uint32_t kMacroSend = 0x4534C0, kRect = 0x14, kSendMsgSlot = 0x94, kGetText = 0x443300;
constexpr uint32_t kFocus = kWindowMgr + 0x6C, kSetFocus = 0x4CE510;
constexpr int kButtonMsg = 6, kAway = -4000;
constexpr ULONGLONG kBlinkMs = 530;
using SendMsgFn = int(__attribute__((thiscall)) *)(uint32_t self, uint32_t sender, int msg, int a, int b, int c);
using GetTextFn = const char*(__attribute__((thiscall)) *)(uint32_t edit);
using SetFocusFn = void(__attribute__((thiscall)) *)(uint32_t manager, uint32_t control);

// 3.0.18-beta.87 (live windows, group 2): the macros a live window - ten fields by flexbox, the game's own fields typed
const char* kLive = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw macw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span></div>
<span class="x ctl" data-event-click="mac_close()">&#x2715;</span>
<div class="hint" data-if="hint != ''">{{hint}}</div>
<div class="mrow" data-for="r, i : rows"><span class="key">{{r.key}}</span><div class="field ctl"
  data-class-typing="r.focus"
  data-event-click="mac_field(i)">{{r.text}}<span class="caret" data-if="r.focus && caret"></span></div></div>
<div class="btns"><div class="btn main ctl" data-event-click="mac_save()">{{save}}</div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
struct MacroRowView {
    Rml::String key, text;
    bool focus = false;
};

struct State {
    uilive::Window live;  // 3.0.18-beta.81
    std::vector<MacroRowView> rowViews;
    Rml::String title, hint, saveWord, ground = "#10141cf0";
    bool caretOn = false;
    uisheet::Sheet sheet;  // 3.0.18-beta.34: the window drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    WindowShadow shadow;
    bool on = false, shown = false, moved = false;
    unsigned codePage = 0;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0, oldX = 0;
    std::vector<std::string> texts = std::vector<std::string>(mc::kFields);
    mc::Show show;
    mc::Layout layout;
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

uint32_t gameMacros() {
    const uint32_t w = read32(kMacroAt);
    return w && read32(w) == kMacroClass && read32(read32(w) + kSendMsgSlot) == kMacroSend ? w : 0;
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
            LOG_INFO("macros: the game's macro window off the screen for the new card (its fields keep the keys)");
        s.moved = true;
    } else if (!on && s.moved) {
        write32(window + kRect, uint32_t(s.oldX));
        s.moved = false;
    }
}

void readFields(State& s, uint32_t window) {
    const uint32_t focus = read32(kFocus);
    s.show.focus = -1;
    for (int i = 0; i < mc::kFields; ++i) {
        const uint32_t edit = read32(window + mc::kFirstField + 4 * uint32_t(i));
        std::string& out = s.texts[size_t(i)];
        out.clear();
        if (!edit) continue;
        if (edit == focus) s.show.focus = i;
        const char* t = reinterpret_cast<GetTextFn>(uintptr_t(kGetText))(edit);
        char buf[128] = {};
        if (t && readGame(uint32_t(reinterpret_cast<uintptr_t>(t)), buf, sizeof buf - 1)) {
            buf[sizeof buf - 1] = 0;
            out = utf8Of(widened(buf, s.codePage));
        }
    }
    s.show.caret = (GetTickCount64() / kBlinkMs) % 2 == 0;
}

mc::Texts texts() {
    mc::Texts t;
    t.title = say(settings::Text::MacroTitle);
    t.hint = say(settings::Text::MacroHint);
    t.save = say(settings::Text::MacroSave);
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
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(Blocker::Macros, game, &r, false);
}

void hide(State& s) {
    if (s.shown) uapad::gamewindow::setBlocked(Blocker::Macros, nullptr, nullptr, false);
    s.shown = false;
}

void send(State& s, int msg, int id, const char* what) {
    const uint32_t window = gameMacros();
    if (!window) return;
    if (s.told++ < 30) LOG_INFO("macros: %s", what);
    reinterpret_cast<SendMsgFn>(uintptr_t(kMacroSend))(window, 0, msg, id, 0, 0);
    s.key.clear();
}

void focusField(State& s, int i) {
    const uint32_t window = gameMacros();
    const uint32_t edit = window && i >= 0 && i < mc::kFields ? read32(window + mc::kFirstField + 4 * uint32_t(i)) : 0;
    if (edit) reinterpret_cast<SetFocusFn>(uintptr_t(kSetFocus))(kWindowMgr, edit);
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
        LOG_INFO("macros: [interface] macros = %d", on ? 1 : 0);
    }
    const bool world = inGameWorld();
    const uint32_t window = world ? gameMacros() : 0;
    if (world) hold(s, window, on);
    if (!on || !window || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        hide(s);
        return;
    }
    if (!s.shown) LOG_INFO("macros: the game's macro window - the new card shows it");
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    readFields(s, window);
    s.layout = mc::layout(targetWidth, targetHeight, s.scale);
    s.show.hover = -1;
    s.show.hoverSave = s.show.hoverClose = false;
    int cx = 0, cy = 0;
    if (readHoverCursor(cx, cy)) {
        const mc::Hit h = mc::hitTest(s.layout, float(cx) * float(k) - float(s.layout.x),
                                      float(cy) * float(k) - float(s.layout.y));
        if (h.part == mc::Part::Field) s.show.hover = h.index;
        s.show.hoverSave = h.part == mc::Part::Save;
        s.show.hoverClose = h.part == mc::Part::Close;
    }
    std::string key = std::to_string(s.show.focus) + (s.show.caret ? "|" : "") + ',' + std::to_string(s.show.hover) +
                      (s.show.hoverSave ? "s" : "") + (s.show.hoverClose ? "x" : "") + ';' +
                      std::to_string(s.layout.x) + ',' + std::to_string(s.layout.y) + '#' +
                      std::to_string(uistyle::generation());
    for (const std::string& t : s.texts) key += ';' + t;
    if (const std::string drawn = key + (uaui::ready() ? "u" : ""); drawn != s.key) {  // 3.0.18-beta.34
        s.key = drawn;
        render(s);
    }
    s.shown = true;
    block(s, game);
}

// 3.0.18-beta.81: the live window's model and place
static void liveFrame(State& s) {
    s.live.make("macro", kLive, [&](Rml::DataModelConstructor& m) {
        if (auto v = m.RegisterStruct<MacroRowView>()) {
            v.RegisterMember("key", &MacroRowView::key);
            v.RegisterMember("text", &MacroRowView::text);
            v.RegisterMember("focus", &MacroRowView::focus);
        }
        m.RegisterArray<std::vector<MacroRowView>>();
        m.Bind("rows", &s.rowViews);
        m.Bind("title", &s.title);
        m.Bind("hint", &s.hint);
        m.Bind("save", &s.saveWord);
        m.Bind("caret", &s.caretOn);
        m.Bind("ground", &s.ground);
        m.BindEventCallback("mac_field", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
            if (!a.empty()) focusField(state(), a[0].Get<int>());
        });
        m.BindEventCallback("mac_save", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            send(state(), mc::kSaveMsg, 0, "«Зберегти» - the window's Enter (it writes the macros)");
        });
        m.BindEventCallback("mac_close", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            send(state(), kButtonMsg, mc::kCloseButton, "closed (the window's 0x87)");
        });
    });
    if (!s.live.made()) return;
    const mc::Texts t = texts();
    std::vector<MacroRowView> rows;
    for (int i = 0; i < mc::kFields && i < int(s.texts.size()); ++i)
        rows.push_back(MacroRowView{mc::keyName(i), s.texts[size_t(i)], s.show.focus == i});
    const Rml::String ground = uisheet::Sheet::ground(0x10141C, uistyle::window());
    const bool same = rows.size() == s.rowViews.size() &&
                      std::equal(rows.begin(), rows.end(), s.rowViews.begin(),
                                 [](const MacroRowView& a, const MacroRowView& b) {
                                     return a.key == b.key && a.text == b.text && a.focus == b.focus;
                                 });
    if (!same) s.rowViews = std::move(rows), s.live.doc.dirty("rows");
    if (s.show.caret != s.caretOn) s.caretOn = s.show.caret, s.live.doc.dirty("caret");
    if (t.title != s.title || t.hint != s.hint || t.save != s.saveWord || ground != s.ground) {
        s.title = t.title, s.hint = t.hint, s.saveWord = t.save, s.ground = ground;
        s.live.doc.dirtyAll();
    }
    s.live.frame(s.shown, s.layout.x, s.layout.y, int(winstack::Win::Macros), ".field, .btn, .x");
}

void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, codePage, k, targetWidth, targetHeight);
    State& s = state();
    liveFrame(s);  // the old edition (the sheet) gone: the live window alone
    if (s.sheetShown) {  // 3.0.18-beta.60: the pad walks its boxes (A clicks one)
        std::vector<panel::Box> targets;
        for (const auto& part : std::initializer_list<std::vector<panel::Box>>{s.layout.keys, s.layout.fields, {s.layout.save, s.layout.close}})
            targets.insert(targets.end(), part.begin(), part.end());
        s.sheet.nav(int(winstack::Win::Macros), std::move(targets));
    }
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
    send(s, kButtonMsg, mc::kCloseButton, "Esc - closed (the window's 0x87)");
    return true;
}

void beforeGameDraw() {
    State& s = state();
    if (s.on && inGameWorld()) hold(s, gameMacros(), true);
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

}  // namespace uagfx::win::macrocardwin

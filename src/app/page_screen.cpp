#include <algorithm>
#include <cstdlib>

#include "app/page.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kFullscreen = 1201;
constexpr int kWindow = 1202;
constexpr int kMonitor = 1203;
constexpr int kMode = 1204;
constexpr int kUi = 1205;
constexpr int kUiExact = 1206;
constexpr int kSize = 1207;
constexpr int kWidth = 1208;  // kWidth + 1: the height
constexpr int kAsSetup = 1210;

std::string sizeText(Size s) { return num(s.width) + "×" + num(s.height); }

// The Screen page (stage 2 spec §5): how the game is shown, on which monitor, how large its interface is.
class ScreenPage : public Page {
public:
    explicit ScreenPage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        const std::vector<MonitorInfo>& monitors = m.system().monitors;
        const ScreenChoice c = m.screenChoice();
        const bool uaro = m.backend() == Backend::Uaro;
        p.heading(t(Text::PageScreen));
        if (!gameKeyExists(m.now().registry)) p.note(t(Text::ScreenFirstRun), Tone::Warn);
        if (monitors.empty()) return;
        const int index = uaro && c.monitor >= 0 && size_t(c.monitor) < monitors.size() ? c.monitor : 0;
        const MonitorInfo& mon = monitors[size_t(index)];
        // ROExt sizes the window, and its ini is written only while the game is closed (spec §4.1): until then the
        // choice is grey (0.6.0 review M4).
        const Snapshot& snap = m.now();
        const bool roext = snap.roext.active && (snap.roext.lock || snap.roext.size.width > 0) && snap.roextModule >= 0;
        const bool open = !(roext && snap.gameRunning);

        p.radio(kFullscreen, t(Text::ScreenShow), t(Text::ScreenFullscreen), c.show == Show::Fullscreen, true, L"",
                open);
        p.radio(kWindow, L"", t(Text::ScreenWindow), c.show == Show::Window, false, L"", open);
        p.gap(6);
        std::vector<std::wstring> items;
        for (size_t i = 0; i < monitors.size(); ++i)
            items.push_back(tf(Text::MonitorItem, {num(long(i) + 1), num(monitors[i].pixels.width),
                                                   num(monitors[i].pixels.height), num(monitors[i].scale)}) +
                            (monitors[i].primary ? t(Text::MonitorMain) : L""));
        p.choice(kMonitor, t(Text::ScreenMonitor), items, index, L"", uaro && monitors.size() > 1 && open);

        if (c.show == Show::Fullscreen && !uaro) {
            modes_ = mon.modes;
            const Size current = c.mode.width > 0 ? c.mode : mon.pixels;
            if (std::find(modes_.begin(), modes_.end(), current) == modes_.end()) modes_.insert(modes_.begin(), current);
            items.clear();
            for (Size s : modes_) items.push_back(wide(sizeText(s)));
            p.choice(kMode, t(Text::ScreenMode), items,
                     int(std::find(modes_.begin(), modes_.end(), current) - modes_.begin()), t(Text::ScreenModeHint),
                     open);
        } else {
            uis_ = uiSteps();
            const bool fixed = !uaro;  // dgVoodoo in a window: Windows scaling
            const int ui = fixed ? mon.scale : c.ui;
            if (std::find(uis_.begin(), uis_.end(), ui) == uis_.end()) {
                uis_.push_back(ui);
                std::sort(uis_.begin(), uis_.end());
            }
            items.clear();
            for (int u : uis_) items.push_back(wide(num(u) + " %"));
            p.choice(kUi, t(Text::ScreenUi), items, int(std::find(uis_.begin(), uis_.end(), ui) - uis_.begin()),
                     t(Text::ScreenUiHint), !fixed && open);
            if (fixed)
                p.note(tf(Text::ScreenUiDgVoodoo, {num(mon.scale)}));
            else
                p.number(kUiExact, t(Text::ScreenUiExact), wide(num(c.ui)), t(Text::ScreenUiHint), open);
        }
        if (c.show == Show::Window) {
            sizes_ = windowSizes(mon, m.system().frame);
            if (c.window.width > 0 && std::find(sizes_.begin(), sizes_.end(), c.window) == sizes_.end())
                sizes_.push_back(c.window);
            const int ui = uaro ? c.ui : mon.scale;
            items.clear();
            int selected = -1;
            for (size_t i = 0; i < sizes_.size(); ++i) {
                const Size s = sizes_[i];
                const Size game = gameSize(s, ui);  // as the plan rounds it (0.6.0 review M15)
                items.push_back(
                    tf(Text::WindowItem, {num(s.width), num(s.height), num(game.width), num(game.height)}));
                if (s == c.window) selected = int(i);
            }
            p.choice(kSize, t(Text::ScreenWindowSize), items, selected, t(Text::ScreenWindowSizeHint), open);
            p.pair(kWidth, L"", c.window.width, c.window.height, t(Text::ScreenWindowSizeHint), open);
        }

        const ScreenPlan plan = m.screenPlan();
        if (plan.problem != ScreenProblem::None) {
            p.note(t(problemText(plan.problem)), Tone::Bad);
        } else {
            const Size out = c.show == Show::Fullscreen ? mon.pixels : c.window;
            std::wstring line = tf(Text::ScreenResult, {num(plan.game.width), num(plan.game.height), num(out.width),
                                                        num(out.height), scaleText(plan.s, host_.lang())});
            if (integerScale(plan.s)) line += t(Text::ScreenCrispest);
            p.note(line, Tone::Normal);
        }
        if (c.show == Show::Fullscreen) p.note(t(Text::ScreenAltTab));
        p.gap(6);
        const ScreenNow now = m.screenNow();
        if (now.game.width > 0) {
            if (now.choice.show == Show::Window)
                p.note(tf(Text::ScreenNowWindow, {num(now.choice.window.width), num(now.choice.window.height),
                                                  num(now.choice.ui), num(now.game.width), num(now.game.height)}));
            else
                p.note(tf(Text::ScreenNowFullscreen, {num(now.game.width), num(now.game.height), num(now.choice.ui)}));
        }
        bool setupFull = false;
        if (const auto setup = m.setupScreen(setupFull)) {
            p.note(tf(Text::ScreenSetupDiffers, {num(setup->width), num(setup->height),
                                                 utf8(t(setupFull ? Text::ShowFullscreenWord : Text::ShowWindowWord))}),
                   Tone::Warn);
            p.buttons({{kAsSetup, t(Text::ScreenAsSetup)}}, !m.screenAsSetupPending());
        }
        if (roext) {
            p.note(tf(Text::ScreenRoext, {snap.modules[size_t(snap.roextModule)].info.ini}));
            if (!open) p.note(t(Text::ErrGameRunning), Tone::Warn);  // spec §4.1
        }
        p.note(t(Text::AfterRestart));
    }

    void command(int id, int code) override {
        Model& m = host_.model();
        ScreenChoice c = m.screenChoice();
        HWND ctl = GetDlgItem(host_.window(), id);
        const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
        const std::vector<MonitorInfo>& monitors = m.system().monitors;
        if ((id == kFullscreen || id == kWindow) && code == BN_CLICKED) {
            c.show = id == kFullscreen ? Show::Fullscreen : Show::Window;
            if (c.show == Show::Window && c.window.width <= 0 && !monitors.empty())
                c.window = defaultWindow(monitors[0], m.system().frame);
        } else if (id == kMonitor && code == CBN_SELCHANGE && sel >= 0 && size_t(sel) < monitors.size()) {
            c.monitor = int(sel);
            const std::vector<Size> fit = windowSizes(monitors[size_t(sel)], m.system().frame);
            if (c.show == Show::Window && std::find(fit.begin(), fit.end(), c.window) == fit.end())
                c.window = defaultWindow(monitors[size_t(sel)], m.system().frame);
        } else if (id == kMode && code == CBN_SELCHANGE && sel >= 0 && size_t(sel) < modes_.size()) {
            c.mode = modes_[size_t(sel)];
        } else if (id == kUi && code == CBN_SELCHANGE && sel >= 0 && size_t(sel) < uis_.size()) {
            c.ui = uis_[size_t(sel)];
        } else if (id == kUiExact && code == EN_KILLFOCUS) {
            const int v = std::atoi(controlText(ctl).c_str());
            if (v >= kUiMin && v <= kUiMax) c.ui = v;
        } else if (id == kSize && code == CBN_SELCHANGE && sel >= 0 && size_t(sel) < sizes_.size()) {
            c.window = sizes_[size_t(sel)];
        } else if ((id == kWidth || id == kWidth + 1) && code == EN_KILLFOCUS) {
            const int w = std::atoi(controlText(GetDlgItem(host_.window(), kWidth)).c_str());
            const int h = std::atoi(controlText(GetDlgItem(host_.window(), kWidth + 1)).c_str());
            if (w > 0 && h > 0) c.window = Size{w, h};
        } else if (id == kAsSetup && code == BN_CLICKED) {
            m.screenAsSetup();
            host_.changed();
            host_.rebuild();
            return;
        } else {
            return;
        }
        if (c == m.screenChoice()) {
            // Nothing changed: no rebuild, it would take down the control the focus goes to - only a field that shows
            // something else than its value (a number out of range, "0175") gets the value back.
            if (code == EN_KILLFOCUS && !fieldsShow(c)) host_.rebuild();
            return;
        }
        m.setScreen(c);
        host_.changed();
        host_.rebuild();  // the result, the problem and the lists follow the choice
    }

private:
    // Whether the number fields there are show the choice as build() writes it.
    bool fieldsShow(const ScreenChoice& c) const {
        const HWND w = host_.window();
        const HWND exact = GetDlgItem(w, kUiExact);
        const HWND width = GetDlgItem(w, kWidth);
        const HWND height = GetDlgItem(w, kWidth + 1);
        return (!exact || controlText(exact) == num(c.ui)) && (!width || controlText(width) == num(c.window.width)) &&
               (!height || controlText(height) == num(c.window.height));
    }

    std::vector<int> uis_;
    std::vector<Size> sizes_;
    std::vector<Size> modes_;
};

}  // namespace

std::unique_ptr<Page> makeScreenPage(PageHost& host) { return std::make_unique<ScreenPage>(host); }

}  // namespace uagfx::app

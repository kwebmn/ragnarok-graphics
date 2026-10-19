#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "app/page.hpp"
#include "app/setting_ids.hpp"
#include "core/dlss_size.hpp"
#include "core/live_config.hpp"
#include "core/scale.hpp"
#include "core/settings/auto_setup.hpp"
#include "core/settings/report.hpp"

namespace uagfx::app {
namespace {

using namespace settings;


std::string hresult(long hr) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

bool sameName(const std::string& a, const std::string& b) {  // a folder's name: any case
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

// Render, Quality and Frames (stage 2 spec §6.1): uaro-graphics.ini keys one to one. Quality, Sound, Game and Chat
// (0.6.2): the game's key. 2.6: several of them on one page (Graphics: Render, Quality | Smoothing, Frames), the right
// ones in a second column when the page is wide; the rows above the mode are left out.
class SettingsPage : public Page {
public:
    SettingsPage(PageHost& host, std::vector<settings::Page> left, std::vector<settings::Page> right)
        : Page(host), left_(std::move(left)), right_(std::move(right)) {}

    void build(Panel& p) override {
        const bool two = !right_.empty() && p.split();
        for (size_t i = 0; i < left_.size(); ++i) buildPart(p, left_[i], i > 0);
        if (two) p.nextColumn();
        for (size_t i = 0; i < right_.size(); ++i) buildPart(p, right_[i], i > 0 || !two);
        if (two) p.join();
    }

    void buildPart(Panel& p, settings::Page which, bool after) {
        if (after) p.gap(16);
        which_ = which;
        held_ = false;
        p.heading(t(pageName(which_)));
        switch (which_) {
            case settings::Page::Render:
                buildRender(p);
                return;
            case settings::Page::Quality:
                buildRows(p);
                p.gap();
                p.heading(t(Text::GameQualityHeading));
                regRows(p, [](const RegSetting&) { return true; });
                break;
            case settings::Page::Sound:
                regRows(p, [](const RegSetting& s) { return !*s.subkey; });  // the device
                p.gap();
                regRows(p, [](const RegSetting& s) { return s.control == Control::Check; });  // music, effects
                regRows(p, [](const RegSetting& s) { return s.control == Control::Slider; });
                p.gap();
                p.heading(t(Text::GfxSoundOurs));  // 0.91: uaro-graphics.ini's [sound]
                buildRows(p);
                break;
            case settings::Page::Game:
                regRows(p, [](const RegSetting&) { return true; });
                break;
            case settings::Page::Chat:
                regRows(p, [](const RegSetting& s) { return *s.subkey && s.control == Control::Check; });  // whispers
                p.note(t(Text::ChatFriendsNote));
                regRows(p, [](const RegSetting& s) { return !*s.subkey; });  // remember the login
                buildForgetLogin(p);
                p.heading(t(Text::ChatMacrosHeading));
                regRows(p, [](const RegSetting& s) { return s.control == Control::Line; });
                p.note(t(Text::ChatMacrosHint));
                break;
            default:
                buildRows(p);
                return;
        }
        p.note(t(Text::AfterRestart));
        if (held_) p.note(t(Text::GameClosedOnly), Tone::Warn);
    }

    void command(int id, int code) override {
        Model& m = host_.model();
        if (id == kGpu && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(GetDlgItem(host_.window(), id), CB_GETCURSEL, 0, 0);
            if (sel >= 0 && size_t(sel) < gpuWords_.size()) m.setToken(*findSetting("renderer.gpu"), gpuWords_[sel]);
            host_.changed();
            return;
        }
        if (id == kAuto && code == BN_CLICKED) {
            autoSetupNow();
            return;
        }
        if (id == kForgetLogin && code == BN_CLICKED) {
            forgetLogin();
            return;
        }
        if (const RegSetting* r = regOf(id)) {
            regCommand(*r, GetDlgItem(host_.window(), id), code);
            return;
        }
        const IniSetting* s = settingOf(id);
        if (!s) return;
        HWND ctl = GetDlgItem(host_.window(), id);
        if (s->control == Control::Check && code == BN_CLICKED) {
            const std::string token = s->tokens[SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0];
            m.setToken(*s, token);
            if (sectionKey(*s) == "frames.battery_saver") host_.rebuild();  // 2.5.20: its frames' row with it
            // 0.87: the profiler (2.6: every switch of the Debug page the driver reads live) at once in a running game
            if (s->page == settings::Page::Debug && m.now().gameRunning && appliesNow(sectionKey(*s))) {
                const std::wstring err = m.writeNow(*s, token, host_.lang());
                if (!err.empty()) host_.message(err, true);
            }
        } else if (s->control == Control::Choice && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && size_t(sel) < s->tokens.size()) m.setToken(*s, s->tokens[size_t(sel)]);
            if (sectionKey(*s) == "quality.dlss" && !SendMessageW(ctl, CB_GETDROPPEDSTATE, 0, 0)) host_.rebuild();
        } else if (s->control == Control::Choice && code == CBN_CLOSEUP && sectionKey(*s) == "quality.dlss") {
            host_.rebuild();  // 2.4.2: the rows that come with the mode, once its list is closed
            return;
        } else if (s->control == Control::Number && code == EN_KILLFOCUS) {
            const std::string text = controlText(ctl);
            if (validToken(*s, text)) {
                m.setToken(*s, text);
                showOwnSize();
            } else {
                host_.rebuild();  // back to the value it had
            }
        } else {
            return;
        }
        host_.changed();
    }

    void scroll(int id, int code, int pos) override {
        if (const RegSetting* r = regOf(id)) {
            if (r->control != Control::Slider) return;
            const std::string word = std::to_string(std::clamp(pos, r->min, r->max));
            SetDlgItemTextW(host_.window(), id + 1, wide(word).c_str());
            host_.model().setRegWord(*r, word);
            host_.changed();
            return;
        }
        const IniSetting* s = settingOf(id);
        if (!s || s->control != Control::Slider) return;
        Model& m = host_.model();
        const int step = s->step > 0 ? s->step : 1;
        int v = (pos + step / 2) / step * step;
        v = v < s->min ? s->min : v > s->max ? s->max : v;
        if (v != pos) SendMessageW(GetDlgItem(host_.window(), id), TBM_SETPOS, TRUE, v);
        const std::string token = std::to_string(v);
        SetDlgItemTextW(host_.window(), id + 1, wide(token + s->unit).c_str());
        m.setToken(*s, token);
        showOwnSize();
        // While the game runs a slider applies when it is let go (spec §6): the driver reads the ini within a second.
        if (code == TB_ENDTRACK && m.now().gameRunning && appliesNow(sectionKey(*s))) {
            const std::wstring err = m.writeNow(*s, token, host_.lang());
            if (!err.empty()) host_.message(err, true);
        }
        host_.changed();
    }

private:
    static const IniSetting* settingOf(int id) { return settingOfId(id); }  // 2.4.2: app/setting_ids.hpp

    static const RegSetting* regOf(int id) {
        if (id < kReg) return nullptr;
        const size_t i = size_t(id - kReg) / 4;
        return (id - kReg) % 4 == 0 && i < regSettings().size() ? &regSettings()[i] : nullptr;
    }

    void regCommand(const RegSetting& s, HWND ctl, int code) {
        Model& m = host_.model();
        if (s.control == Control::Check && code == BN_CLICKED) {
            m.setRegWord(s, SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED ? "1" : "0");
        } else if (s.control == Control::Choice && (code == CBN_SELCHANGE || code == CBN_CLOSEUP)) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            const size_t count = s.kind == RegKind::Line ? skinWords_.size() : s.values.size();
            if (sel < 0 || size_t(sel) >= count) return;
            m.setRegWord(s, s.kind == RegKind::Line ? skinWords_[size_t(sel)] : std::to_string(s.values[size_t(sel)]));
            // The speakers are for 3D only: the page again, once the list is closed.
            if (regKey(s) == "SOUNDMODE" && !SendMessageW(ctl, CB_GETDROPPEDSTATE, 0, 0)) host_.rebuild();
        } else if (s.control == Control::Line && code == EN_KILLFOCUS) {
            const std::string text = controlText(ctl);
            if (validRegWord(s, text))
                m.setRegWord(s, text);
            else
                host_.rebuild();  // back to the text it had
        } else {
            return;
        }
        host_.changed();
    }

    // The game's key on this page that `pick` takes, in the order of regSettings(): checks two to a row, the chat
    // macros as one block. What the game writes itself is grey while it runs (spec §4.1).
    void regRows(Panel& p, bool (*pick)(const RegSetting&)) {
        Model& m = host_.model();
        const std::vector<RegSetting>& all = regSettings();
        const bool threeD = m.regWord(*findRegSetting("SOUNDMODE")) == "2";
        std::vector<CheckItem> checks;
        std::vector<LineItem> macros;
        int letters = 0;
        bool macrosOpen = true;
        for (size_t i = 0; i < all.size(); ++i) {
            const RegSetting& s = all[i];
            const std::string key = regKey(s);
            if (s.page != which_ || !pick(s) || (key == "BINKMODE" && !m.hasIntro())) continue;  // no intro to skip
            const int id = kReg + int(i) * 4;
            const std::string word = m.regWord(s);
            const bool open = m.regOpen(s) && (key != "SPEAKERTYPE" || threeD);
            held_ = held_ || !m.regOpen(s);
            const std::wstring hint = s.hint == s.label ? L"" : t(s.hint);
            if (s.control == Control::Check) {
                checks.push_back({id, t(s.label), word == "1", hint, open});
                continue;
            }
            if (s.control == Control::Line) {  // a chat macro: "0" is Alt+1, ..., "9" is Alt+0
                const int n = std::atoi(s.name);
                macros.push_back({id, L"Alt+" + std::to_wstring((n + 1) % 10), wide(word), wide(builtInMacro(n))});
                letters = s.max;
                macrosOpen = open;
                continue;
            }
            if (!checks.empty()) p.checks(checks);
            checks.clear();
            if (s.control == Control::Slider) {
                p.slider(id, t(s.label), s.min, s.max, std::max(s.step, 1), std::atoi(word.c_str()), wide(word), hint,
                         open);
                continue;
            }
            std::vector<std::wstring> items;
            int selected = 0;
            if (s.kind == RegKind::Line) {
                items = skinItems(s, word, selected);
            } else {
                for (size_t c = 0; c < s.choices.size(); ++c) {
                    items.push_back(t(s.choices[c]));
                    if (word == std::to_string(s.values[c])) selected = int(c);
                }
            }
            p.choice(id, t(s.label), items, selected, hint, open);
        }
        if (!checks.empty()) p.checks(checks);
        if (!macros.empty()) p.lines(macros, letters, L"", macrosOpen);
    }

    // "No skin", the folders of skin\, and a name the game has that no folder holds: it shows no skin then.
    std::vector<std::wstring> skinItems(const RegSetting& s, const std::string& word, int& selected) {
        std::vector<std::wstring> items{t(Text::SkinNone)};
        skinWords_ = {s.fallbackText};
        selected = 0;
        for (const std::wstring& folder : host_.model().skins()) {
            items.push_back(folder);
            skinWords_.push_back(utf8(folder));
            if (sameName(skinWords_.back(), word)) selected = int(items.size()) - 1;
        }
        if (selected == 0 && !word.empty() && word != s.fallbackText) {
            items.push_back(tf(Text::SkinMissing, {word}));
            skinWords_.push_back(word);
            selected = int(items.size()) - 1;
        }
        return items;
    }

    void buildForgetLogin(Panel& p) {
        const Snapshot& now = host_.model().now();
        const auto login = effectiveValue(now.registry, "", "ID");
        p.buttons({{kForgetLogin, t(Text::ChatForgetLogin), login && login->isSz() && !login->text.empty() &&
                                                                !now.gameRunning}});
    }

    void forgetLogin() {
        if (!host_.ask(t(Text::ChatForgetAsk))) return;
        const std::wstring err = host_.model().forgetLogin(host_.lang());
        host_.message(err.empty() ? t(Text::ChatLoginForgotten) : err, !err.empty());
        host_.reload();
    }


    // 0.50: what this computer carries, at a press (core/settings/auto_setup) - for a client handed to other players.
    void autoSetupNow() {
        Model& m = host_.model();
        const Machine x = m.machine();  // 2.6: the model's, the recommended values' too
        const AutoSetup a = autoSetup(x);
        m.applyPreset(a.preset);  // the game's own texture quality too
        std::string scale = "100";
        for (const IniEdit& e : a.edits) {
            if (const IniSetting* s = findSetting(e.section + "." + e.key)) m.setToken(*s, e.value);
            if (e.key == "render_scale") scale = e.value;
        }
        // 1.3.3: the interface a whole number of times the game's pixels, where it is a fraction now (167 %)
        bool uiChanged = false;
        if (!integerScale(m.screenPlan().s)) {
            const ScreenChoice before = m.screenChoice();
            ScreenChoice c = before;
            if (c.ui != a.ui) {
                c.ui = a.ui;
                m.setScreen(c);
                uiChanged = m.screenPlan().problem == ScreenProblem::None;
                if (!uiChanged) m.setScreen(before);  // too small a game at that size: as it was
            }
        }
        host_.changed();
        std::wstring text = tf(Text::AutoFound, {x.adapter.empty() ? utf8(t(Text::Unknown)) : x.adapter,
                                                 std::to_string(x.vramMb),
                                                 std::to_string(x.screenWidth) + "x" + std::to_string(x.screenHeight)});
        switch (a.tier) {
            case Tier::NoDriver: text += L"\n\n" + t(Text::AutoTierNoDriver); break;
            case Tier::Old: text += L"\n\n" + t(Text::AutoTierOld); break;
            case Tier::Low: text += L"\n\n" + t(Text::AutoTierLow); break;
            case Tier::Middle: text += L"\n\n" + t(Text::AutoTierMiddle); break;
            case Tier::High: text += L"\n\n" + tf(Text::AutoTierHigh, {scale}); break;
        }
        const Text ai = a.aiWorld ? Text::AutoAiAll
                        : a.ai    ? Text::AutoAiSprites
                        : !x.pack ? Text::AutoAiNoPack
                                  : Text::AutoAiOff;
        text += L"\n" + t(ai);
        if (uiChanged) text += L"\n" + tf(Text::AutoUiWhole, {std::to_string(a.ui)});  // 1.3.3
        if (a.dlss == "dlaa") text += L"\n" + t(Text::AutoDlaa);
        if (a.dlss == "quality") text += L"\n" + t(Text::AutoDlssQuality);
        if (a.hdrOn) text += L"\n" + t(Text::AutoHdr);
        if (a.soundEngine) text += L"\n" + t(Text::AutoSoundEngine);
        text += L"\n\n" + t(Text::AutoAsk);
        if (host_.ask(text)) {
            std::wstring err = m.apply(host_.lang());
            if (err.empty()) err = m.launchGame(host_.lang());
            if (!err.empty()) host_.message(err, true);
            host_.reload();
            return;
        }
        host_.message(t(Text::AutoPending), false);
        host_.rebuild();  // the page shows the new values
    }

    void buildRender(Panel& p) {
        Model& m = host_.model();
        p.heading(t(Text::AutoHeading));  // 0.50: first, for a player who only wants it to work
        p.buttons({{kAuto, t(Text::AutoButton)}});
        p.note(t(Text::AutoNote));
        p.gap();
        const SystemInfo& sys = m.system();
        const bool win10 = sys.windows.major >= 10;
        const bool d3d12 = sys.d3d12Probed && sys.d3d12.ok;
        const bool d3d11 = sys.d3d11.ok;  // 0.46: graphics cards without Direct3D 12
        p.heading(t(Text::RenderHeading));  // 2.2: the uaRO renderer only - dgVoodoo is gone
        const auto& all = iniSettings();
        for (size_t i = 0; i < all.size(); ++i)  // 0.46: Direct3D 12, 11, or 12 and else 11
            if (sectionKey(all[i]) == "renderer.api") {
                const IniSetting& s = all[i];
                std::vector<std::wstring> items;
                int selected = 0;
                const std::string token = m.token(s);
                for (size_t c = 0; c < s.choices.size(); ++c) {
                    items.push_back(t(s.choices[c]));
                    if (token == s.tokens[c]) selected = int(c);
                }
                p.choice(rowId(i), t(s.label), items, selected, t(s.hint), true);
            }
        // 0.50.2: the graphics card, when there are several (or one is chosen that is gone)
        const auto& cards = sys.adapters;
        const std::string card = m.token(*findSetting("renderer.gpu"));
        gpuWords_.clear();
        if (cards.size() > 1 || card != "auto") {
            std::vector<std::wstring> items;
            int selected = 0;
            items.push_back(tf(Text::RenderGpuAuto, {cards.empty() ? utf8(t(Text::Unknown)) : cards.front().name}));
            gpuWords_.push_back("auto");
            for (const win::AdapterProbe& c : cards) {
                if (sameName(c.name, card)) selected = int(items.size());
                items.push_back(tf(Text::RenderGpuItem, {c.name, std::to_string(c.vramMb)}));
                gpuWords_.push_back(c.name);
            }
            if (card != "auto" && !selected) {  // a card no longer there: the renderer takes the fast one
                selected = int(items.size());
                items.push_back(tf(Text::RenderGpuMissing, {card}));
                gpuWords_.push_back(card);
            }
            p.choice(kGpu, t(Text::RenderGpu), items, selected, t(Text::RenderGpuHint), true);
        }
        p.gap();
        // part 3: the renderer in a 64-bit process (2.6: its thread and Remix on the Debug page)
        for (size_t i = 0; i < all.size(); ++i)
            if (sectionKey(all[i]) == "renderer.bridge")
                p.check(rowId(i), t(all[i].label), m.token(all[i]) == "1", t(all[i].hint));
        if ((!win10 || !d3d12) && d3d11 && m.token(*findSetting("renderer.api")) == "d3d12")
            p.note(t(Text::RenderUseD3d11), Tone::Warn);  // 0.46: Direct3D 11 works here
        else if (!win10 && !d3d11)
            p.note(t(Text::RenderNeedsWindows10), Tone::Bad);
        else if (!d3d12 && !d3d11)
            p.note(tf(Text::RenderNoD3D12, {hresult(sys.d3d12.hr)}), Tone::Bad);
        p.note(t(Text::AfterRestart));
        p.gap();
        const Snapshot& now = m.now();
        if (now.ddraw == Ddraw::Ours) {
            const std::string v = !now.installedVersion.empty() ? now.installedVersion
                                  : !now.log.version.empty()     ? now.log.version
                                                                 : utf8(t(Text::Unknown));
            p.note(tf(Text::InstalledOurs, {v}), Tone::Good);
        } else {
            p.note(t(Text::InstalledNoDriver), Tone::Bad);
        }
        p.gap();
        p.note(t(Text::PauseNote));
    }

    // A row of the part being built that the mode shows (2.6) and its values call for.
    bool listed(const IniSetting& s) const { return s.page == which_ && levelOf(s) <= host_.mode() && shown(s); }

    // 2.4.2: DLSS's own size shows under Own size, its model with DLSS on
    bool shown(const IniSetting& s) const {
        const std::string k = sectionKey(s);
        if (k == "quality.world_scale") {  // 2.5.16: Direct3D 11's alone - chosen, or "auto" without Direct3D 12
            const SystemInfo& sys = host_.model().system();
            const std::string api = host_.model().token(*findSetting("renderer.api"));
            return api == "d3d11" || (api == "auto" && !(sys.d3d12Probed && sys.d3d12.ok));
        }
        if (k == "frames.battery_fps")  // 2.5.20: with the battery saver on
            return host_.model().token(*findSetting("frames.battery_saver")) == "1";
        if (k.compare(0, 13, "quality.dlss_") != 0) return true;
        const std::string dlss = host_.model().token(*findSetting("quality.dlss"));
        return k == "quality.dlss_model" ? dlss != "off" : dlss == "custom";
    }

    // 2.4.2: the picture DLSS enlarges the world to - the game's size times the screen's scale (render_scale is not
    // used with DLSS), as the Screen page plans it; 0x0 without a plan.
    Size dlssScreen() const {
        const Model& m = host_.model();
        const ScreenPlan plan = m.screenPlan();
        const std::vector<MonitorInfo>& monitors = m.system().monitors;
        if (plan.problem != ScreenProblem::None || plan.game.width <= 0 || plan.game.height <= 0 || monitors.empty())
            return {};
        const ScreenChoice c = m.screenChoice();
        const size_t index = c.monitor >= 0 && size_t(c.monitor) < monitors.size() ? size_t(c.monitor) : 0;
        const Size out = c.show == Show::Fullscreen ? monitors[index].pixels
                         : c.window.width > 0       ? c.window
                                                    : plan.game;
        const bool high = m.token(*findSetting("display.high_resolution")) == "1";
        const ScalePlan s = planScale(plan.game.width, plan.game.height, out.width, out.height, 100, high);
        return {s.targetWidth, s.targetHeight};
    }

    // 2.4.2: the own size on this screen, under its rows
    std::wstring ownSizeText() const {
        const Model& m = host_.model();
        const auto number = [&m](const char* key) { return std::atoi(m.token(*findSetting(key)).c_str()); };
        const Size out = dlssScreen();
        const dlss::Size want = dlss::asked(out.width, out.height, number("quality.dlss_scale"),
                                            number("quality.dlss_width"), number("quality.dlss_height"));
        const int share = out.width > 0 ? (want.width * 100 + out.width / 2) / out.width : 0;
        return tf(Text::DlssCustomNow, {std::to_string(want.width), std::to_string(want.height),
                                        std::to_string(out.width), std::to_string(out.height), std::to_string(share)});
    }

    void showOwnSize() {  // again as its rows change
        if (ownSize_) SetWindowTextW(ownSize_, ownSizeText().c_str());
    }

    void buildRows(Panel& p) {
        Model& m = host_.model();
        const std::vector<IniSetting>& all = iniSettings();
        ownSize_ = nullptr;
        for (size_t i = 0; i < all.size(); ++i) {
            const IniSetting& s = all[i];
            if (!listed(s)) continue;
            const int id = rowId(i);
            const std::string token = m.token(s);
            switch (s.control) {
                case Control::Check:
                    p.check(id, t(s.label), token == s.tokens[1], t(s.hint));
                    break;
                case Control::Slider:
                    p.slider(id, t(s.label), s.min, s.max, s.step, std::atoi(token.c_str()), wide(token + s.unit),
                             t(s.hint));
                    break;
                case Control::Choice: {
                    std::vector<std::wstring> items;
                    int selected = 0;
                    for (size_t c = 0; c < s.choices.size(); ++c) {
                        items.push_back(t(s.choices[c]));
                        if (token == s.tokens[c]) selected = int(c);
                    }
                    p.choice(id, t(s.label), items, selected, t(s.hint));
                    break;
                }
                case Control::Number:
                    p.number(id, t(s.label), wide(token), t(s.hint));
                    break;
                case Control::Line:  // the game's key only: a key of uaro-graphics.ini is never a line of text
                    break;
            }
            if (sectionKey(s) == "quality.dlss_height" && dlssScreen().width > 0)
                ownSize_ = p.note(ownSizeText(), Tone::Normal);
            // 2.4.2: one note under rows in a row that apply at the next start
            size_t next = i + 1;
            while (next < all.size() && !listed(all[next])) ++next;
            if (!appliesNow(sectionKey(s)) && (next == all.size() || appliesNow(sectionKey(all[next]))))
                p.note(t(Text::AfterRestart));
        }
        p.gap();
        p.note(t(m.now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
    }

    std::vector<settings::Page> left_, right_;
    settings::Page which_ = settings::Page::Render;  // the part being built
    HWND ownSize_ = nullptr;              // 2.4.2: the note with DLSS's own size on this screen
    bool held_ = false;                   // a row is grey: the game runs and writes it itself
    std::vector<std::string> skinWords_;  // the skin list's words, in its order
    std::vector<std::string> gpuWords_;   // 0.50.2: the graphics card list's [renderer] gpu words, in its order
};

}  // namespace

std::unique_ptr<Page> makeSettingsPage(PageHost& host, std::vector<settings::Page> left,
                                       std::vector<settings::Page> right) {
    return std::make_unique<SettingsPage>(host, std::move(left), std::move(right));
}

}  // namespace uagfx::app

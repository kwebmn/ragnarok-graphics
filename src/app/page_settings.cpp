#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "app/page.hpp"
#include "app/setting_ids.hpp"
#include "core/dlss_size.hpp"
#include "core/gpu_order.hpp"
#include "core/live_config.hpp"
#include "core/scale.hpp"
#include "core/audio_output.hpp"
#include "core/settings/auto_setup.hpp"
#include "core/settings/report.hpp"
#include "win/audio_devices.hpp"

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

    // 2.6: one row of the ini's schema ("section.key") or of the game's key ("reg:" and its name), alone (the Main
    // page, the search)
    bool buildRow(Panel& p, const std::string& key) override {
        if (key.compare(0, 4, "reg:") == 0) {
            const std::vector<RegSetting>& reg = regSettings();
            for (size_t i = 0; i < reg.size(); ++i)
                if (key.compare(4, std::string::npos, regKey(reg[i])) == 0) return buildReg(p, i) != nullptr;
            return false;
        }
        if (!fitsMachine(key, host_.model().machine())) return false;  // 2.15
        if (key == "sound.output") return buildOutput(p) != nullptr;  // 3.0.4-beta.1: its own list
        const std::vector<IniSetting>& all = iniSettings();
        for (size_t i = 0; i < all.size(); ++i)
            if (sectionKey(all[i]) == key) return buildOne(p, i) != nullptr;
        return false;
    }

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
                buildOutput(p);                    // 3.0.4-beta.1: first, the device it plays on
                buildRows(p);
                break;
            case settings::Page::Game:
                regRows(p, [](const RegSetting&) { return true; });
                // 3.0.1-beta.8: ROExt's settings, its work in the driver (uaro-graphics.ini [roext])
                if (host_.model().now().roextInDriver && host_.mode() != settings::Mode::Simple) {
                    p.gap();
                    p.heading(t(Text::RoextHeading));
                    p.note(t(Text::RoextNote));
                    buildRows(p, "roext", false);
                }
                // 3.0.3-beta.4: several game windows ([windows]) - their own heading, whatever ROExt is
                if (anyListed("windows")) {
                    p.gap();
                    p.heading(t(Text::WindowsHeading));
                    buildRows(p, "windows");
                }
                // 3.0.3-beta.8: the game windows' panel ([panel]) - settings of its own (the user)
                if (anyListed("panel")) {
                    p.gap();
                    p.heading(t(Text::PanelHeading));
                    buildRows(p, "panel");
                }
                // 3.0.4-beta.5: the camera ([camera]) and the photo mode's keys
                if (anyListed("camera")) {
                    p.gap();
                    p.heading(t(Text::CameraHeading));
                    buildRows(p, "camera", false);
                    p.note(t(Text::CameraPhotoNote));
                }
                if (anyListed("buffs")) {  // 3.0.4-beta.6: the buff icons' times
                    p.gap();
                    p.heading(t(Text::BuffsHeading));
                    buildRows(p, "buffs", false);
                }
                // 3.0.4-beta.3: notifications while the game is behind ([notify])
                if (anyListed("notify")) {
                    p.gap();
                    p.heading(t(Text::NotifyHeading));
                    buildRows(p, "notify");
                }
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
        if (id == kOutput && code == CBN_SELCHANGE) {  // 3.0.4-beta.1
            const LRESULT sel = SendMessageW(GetDlgItem(host_.window(), id), CB_GETCURSEL, 0, 0);
            if (sel >= 0 && size_t(sel) < outputWords_.size())
                m.setToken(*findSetting("sound.output"), outputWords_[sel]);
            host_.changed();
            return;
        }
        if (id == kAuto && code == BN_CLICKED) {
            runAutoSetup(host_);
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

    // 2.6: one row of the game's key alone (the search's): a box, a slider or a list of numbers; the skins' list
    // and the chat macros only on their page.
    HWND buildReg(Panel& p, size_t i) {
        Model& m = host_.model();
        const RegSetting& s = regSettings()[i];
        const int id = kReg + int(i) * 4;
        const std::string word = m.regWord(s);
        const bool open = m.regOpen(s);
        const std::wstring hint = s.hint == s.label ? L"" : t(s.hint);
        if (s.control == Control::Check) return p.check(id, t(s.label), word == "1", hint, open);
        if (s.control == Control::Slider)
            return p.slider(id, t(s.label), s.min, s.max, std::max(s.step, 1), std::atoi(word.c_str()), wide(word),
                            hint, open);
        if (s.control != Control::Choice || s.kind == RegKind::Line) return nullptr;
        std::vector<std::wstring> items;
        int selected = 0;
        for (size_t c = 0; c < s.choices.size(); ++c) {
            items.push_back(t(s.choices[c]));
            if (word == std::to_string(s.values[c])) selected = int(c);
        }
        return p.choice(id, t(s.label), items, selected, hint, open);
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
        // 0.50.2: the graphics card, when there are several (or one is chosen that is gone); 3.0.4-beta.2 (the user):
        // a single one too, grey - which card draws the game
        const auto& cards = sys.adapters;
        const std::string card = m.token(*findSetting("renderer.gpu"));
        gpuWords_.clear();
        const GpuRow gpuShown = gpuRow(cards.size(), card != "auto");
        if (gpuShown.shown) {
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
            p.choice(kGpu, t(Text::RenderGpu), items, selected, t(Text::RenderGpuHint), gpuShown.choosable);
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

    // 3.0.4-beta.1: the output device - «As in Windows» (now: its default), the devices connected, the chosen one when
    // it is not (by the name Windows still knows)
    HWND buildOutput(Panel& p) {
        Model& m = host_.model();
        const std::string chosen = m.token(*findSetting("sound.output"));
        std::vector<win::audio_devices::Device> active;
        std::string defaultId, defaultName;
        win::audio_devices::list(active, defaultId);
        for (const win::audio_devices::Device& d : active)
            if (audio_output::sameId(d.id, defaultId)) defaultName = d.name;
        std::vector<std::wstring> items;
        int selected = 0;
        outputWords_.clear();
        items.push_back(defaultName.empty() ? t(Text::SoundOutputWindows)
                                            : tf(Text::SoundOutputWindowsNow, {defaultName}));
        outputWords_.push_back("auto");
        for (const win::audio_devices::Device& d : active) {
            if (audio_output::sameId(d.id, chosen)) selected = int(items.size());
            items.push_back(wide(d.name.empty() ? d.id : d.name));
            outputWords_.push_back(d.id);
        }
        const bool away = chosen != "auto" && !selected;
        if (away) {
            const std::string name = win::audio_devices::nameOf(chosen);
            selected = int(items.size());
            items.push_back(tf(Text::SoundOutputAway, {name.empty() ? utf8(t(Text::SoundOutputGone)) : name}));
            outputWords_.push_back(chosen);
        }
        HWND row = p.choice(kOutput, t(Text::SoundOutput), items, selected, t(Text::SoundOutputHint), true);
        if (away) p.note(t(Text::SoundOutputAwayNote), Tone::Warn);
        return row;
    }

    // A row of the part being built that the mode shows (2.6) and its values call for.
    // 2.15: and this computer can use (fitsMachine: DLSS on NVIDIA, HDR with an HDR screen)
    bool listed(const IniSetting& s) const {
        return s.page == which_ && levelOf(s) <= host_.mode() && shown(s) &&
               fitsMachine(sectionKey(s), host_.model().machine());
    }

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
        if (k == "sound.output") return false;  // 3.0.4-beta.1: its own list (buildOutput)
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

    // The control of the schema's row i (its id rowId(i)), as the page shows it; 2.6: alone on the Main page and in
    // the search's results too.
    HWND buildOne(Panel& p, size_t i) {
        const IniSetting& s = iniSettings()[i];
        const int id = rowId(i);
        const std::string token = host_.model().token(s);
        HWND row = nullptr;
        switch (s.control) {
            case Control::Check: row = p.check(id, t(s.label), token == s.tokens[1], t(s.hint)); break;
            case Control::Slider:
                row = p.slider(id, t(s.label), s.min, s.max, s.step, std::atoi(token.c_str()), wide(token + s.unit),
                               t(s.hint));
                break;
            case Control::Choice: {
                std::vector<std::wstring> items;
                int selected = 0;
                for (size_t c = 0; c < s.choices.size(); ++c) {
                    items.push_back(t(s.choices[c]));
                    if (token == s.tokens[c]) selected = int(c);
                }
                row = p.choice(id, t(s.label), items, selected, t(s.hint));
                break;
            }
            case Control::Number: row = p.number(id, t(s.label), wide(token), t(s.hint)); break;
            case Control::Line: break;  // the game's key only: a key of uaro-graphics.ini is never a line of text
        }
        // 2.6: a dot before a row whose value is not the one recommended for this computer
        const std::string recommended = host_.model().recommended(s);
        if (row && !recommended.empty() && recommended != token)
            p.dot(row, tf(Text::DotHint, {utf8(tokenText(s, recommended))}));
        return row;
    }

    // A row's word as the page shows it: on or off, the list's item, the number with its unit.
    std::wstring tokenText(const IniSetting& s, const std::string& token) const {
        if (s.control == Control::Check) return t(token == s.tokens[1] ? Text::DotOn : Text::DotOff);
        if (s.control == Control::Choice)
            for (size_t c = 0; c < s.choices.size() && c < s.tokens.size(); ++c)
                if (token == s.tokens[c]) return t(s.choices[c]);
        return wide(token + s.unit);
    }

    // 3.0.3-beta.4: a page's rows of one section alone (nullptr: all); `liveNote` - the note on applying at the end
    void buildRows(Panel& p, const char* section = nullptr, bool liveNote = true) {
        Model& m = host_.model();
        const std::vector<IniSetting>& all = iniSettings();
        const auto mine = [&](const IniSetting& s) {
            return listed(s) && (!section || std::strcmp(s.section, section) == 0);
        };
        ownSize_ = nullptr;
        for (size_t i = 0; i < all.size(); ++i) {
            const IniSetting& s = all[i];
            if (!mine(s)) continue;
            buildOne(p, i);
            if (sectionKey(s) == "quality.dlss_height" && dlssScreen().width > 0)
                ownSize_ = p.note(ownSizeText(), Tone::Normal);
            // 2.4.2: one note under rows in a row that apply at the next start
            size_t next = i + 1;
            while (next < all.size() && !mine(all[next])) ++next;
            if (!appliesNow(sectionKey(s)) && (next == all.size() || appliesNow(sectionKey(all[next]))))
                p.note(t(Text::AfterRestart));
        }
        if (!liveNote) return;
        p.gap();
        p.note(t(m.now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
    }

    bool anyListed(const char* section) const {
        for (const IniSetting& s : iniSettings())
            if (listed(s) && std::strcmp(s.section, section) == 0) return true;
        return false;
    }

    std::vector<settings::Page> left_, right_;
    settings::Page which_ = settings::Page::Render;  // the part being built
    HWND ownSize_ = nullptr;              // 2.4.2: the note with DLSS's own size on this screen
    bool held_ = false;                   // a row is grey: the game runs and writes it itself
    std::vector<std::string> skinWords_;  // the skin list's words, in its order
    std::vector<std::string> gpuWords_;   // 0.50.2: the graphics card list's [renderer] gpu words, in its order
    std::vector<std::string> outputWords_;  // 3.0.4-beta.1: the output device list's [sound] output words
};

}  // namespace

// 2.12: what the auto setup picked, in words - the Main page's question and «Гра»'s «Як підібрано».
std::wstring autoSetupText(const Machine& x, const AutoSetup& a, bool uiChanged, Lang l) {
    std::string scale = "100";
    for (const IniEdit& e : a.edits)
        if (e.key == "render_scale") scale = e.value;
    std::wstring text = wtrf(l, Text::AutoFound, {x.adapter.empty() ? utf8(wtr(l, Text::Unknown)) : x.adapter,
                                             std::to_string(x.vramMb),
                                             std::to_string(x.screenWidth) + "x" + std::to_string(x.screenHeight)});
    switch (a.tier) {
        case Tier::NoDriver: text += L"\n\n" + wtr(l, Text::AutoTierNoDriver); break;
        case Tier::Old: text += L"\n\n" + wtr(l, Text::AutoTierOld); break;
        case Tier::Low: text += L"\n\n" + wtr(l, Text::AutoTierLow); break;
        case Tier::Middle: text += L"\n\n" + wtr(l, Text::AutoTierMiddle); break;
        case Tier::High: text += L"\n\n" + wtrf(l, Text::AutoTierHigh, {scale}); break;
        case Tier::IntegratedWeak: text += L"\n\n" + wtr(l, Text::AutoTierIntegratedWeak); break;  // 2.12
        case Tier::IntegratedStrong: text += L"\n\n" + wtr(l, Text::AutoTierIntegratedStrong); break;
    }
    const Text ai = a.aiWorld ? Text::AutoAiAll
                    : a.ai    ? Text::AutoAiSprites
                    : !x.pack ? Text::AutoAiNoPack
                              : Text::AutoAiOff;
    text += L"\n" + wtr(l, ai);
    if (uiChanged) text += L"\n" + wtrf(l, Text::AutoUiWhole, {std::to_string(a.ui)});  // 1.3.3
    if (a.dlss == "dlaa") text += L"\n" + wtr(l, Text::AutoDlaa);
    if (a.dlss == "quality") text += L"\n" + wtr(l, Text::AutoDlssQuality);
    if (a.soundEngine) text += L"\n" + wtr(l, Text::AutoSoundEngine);
    return text;
}

// 0.50: what this computer carries, at a press (core/settings/auto_setup) - for a client handed to other players.
// 2.6: the Main page's "Recommended settings" too.
void runAutoSetup(PageHost& host) {
    const Lang l = host.lang();
    Model& m = host.model();
    const Machine x = m.machine();  // 2.6: the model's, the recommended values' too
    bool uiChanged = false;
    const AutoSetup a = m.pendAutoSetup(uiChanged);  // 2.12: the model's, the first start's too
    m.recordMachine(l);  // the card and the screen these were picked for: no question about them at the next start
    host.changed();
    std::wstring text = autoSetupText(x, a, uiChanged, l);
    text += L"\n\n" + wtr(l, Text::AutoAsk);
    if (host.ask(text)) {
        std::wstring err = m.apply(l);
        if (err.empty()) err = m.launchGame(l);
        if (!err.empty()) host.message(err, true);
        host.reload();
        return;
    }
    host.message(wtr(l, Text::AutoPending), false);
    host.rebuild();  // the page shows the new values
}

std::unique_ptr<Page> makeSettingsPage(PageHost& host, std::vector<settings::Page> left,
                                       std::vector<settings::Page> right) {
    return std::make_unique<SettingsPage>(host, std::move(left), std::move(right));
}

}  // namespace uagfx::app

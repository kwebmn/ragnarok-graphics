#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "app/page.hpp"
#include "core/live_config.hpp"
#include "core/settings/auto_setup.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kUaro = 1001;
constexpr int kDgVoodoo = 1002;
constexpr int kRows = 1100;  // + 4 x the setting's index in iniSettings()
constexpr int kForgetLogin = 1490;
constexpr int kAsSetup = 1491;
constexpr int kAuto = 1492;  // 0.50
constexpr int kReg = 1500;  // + 4 x the setting's index in regSettings()

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
// (0.6.2): the game's key.
class SettingsPage : public Page {
public:
    SettingsPage(PageHost& host, settings::Page which) : Page(host), which_(which) {}

    void build(Panel& p) override {
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
                regRows(p, [](const RegSetting& s) { return !*s.subkey; });  // the device, as setup.exe sets it
                buildAsSetup(p);
                p.gap();
                regRows(p, [](const RegSetting& s) { return s.control == Control::Check; });  // music, effects
                regRows(p, [](const RegSetting& s) { return s.control == Control::Slider; });
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
        if ((id == kUaro || id == kDgVoodoo) && code == BN_CLICKED) {
            m.setToken(*findSetting("renderer.backend"), id == kUaro ? "uaro" : "dgvoodoo");
            host_.changed();
            host_.rebuild();  // 0.46: the graphics API goes with the uaRO renderer
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
        if (id == kAsSetup && code == BN_CLICKED) {
            m.regAsSetup();
            host_.changed();
            host_.rebuild();  // the lists show setup.exe's now
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
            m.setToken(*s, s->tokens[SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0]);
        } else if (s->control == Control::Choice && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && size_t(sel) < s->tokens.size()) m.setToken(*s, s->tokens[size_t(sel)]);
        } else if (s->control == Control::Number && code == EN_KILLFOCUS) {
            const std::string text = controlText(ctl);
            if (validToken(*s, text))
                m.setToken(*s, text);
            else
                host_.rebuild();  // back to the value it had
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
        // While the game runs a slider applies when it is let go (spec §6): the driver reads the ini within a second.
        if (code == TB_ENDTRACK && m.now().gameRunning && appliesNow(sectionKey(*s))) {
            const std::wstring err = m.writeNow(*s, token, host_.lang());
            if (!err.empty()) host_.message(err, true);
        }
        host_.changed();
    }

private:
    static const IniSetting* settingOf(int id) {
        if (id < kRows || id >= kForgetLogin) return nullptr;
        const size_t i = size_t(id - kRows) / 4;
        return (id - kRows) % 4 == 0 && i < iniSettings().size() ? &iniSettings()[i] : nullptr;
    }

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

    // Spec §4.2: where this app's sound differs from setup.exe's, the page says so and offers setup.exe's.
    void buildAsSetup(Panel& p) {
        const auto differs = host_.model().regSetupDiffers();
        if (differs.empty()) return;
        std::string list;
        for (const auto& d : differs) {
            const RegSetting& s = *d.first;
            std::string value = d.second;
            for (size_t c = 0; c < s.choices.size(); ++c)
                if (d.second == std::to_string(s.values[c])) value = utf8(t(s.choices[c]));
            list += (list.empty() ? "" : ", ") + utf8(t(s.label)) + " — " + value;
        }
        p.note(tf(Text::SoundSetupDiffers, {list}), Tone::Warn);
        p.buttons({{kAsSetup, t(Text::ScreenAsSetup)}});
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
        const SystemInfo& sys = m.system();
        Machine x;
        x.windows10 = sys.windows.major >= 10;
        x.d3d12 = sys.d3d12Probed && sys.d3d12.ok;
        x.d3d11 = sys.d3d11.ok;
        x.d3d11Level = sys.d3d11.level;
        // The Direct3D 12 probe reads the main adapter even where it makes no device (an old Radeon).
        x.vendor = sys.d3d12.vendor ? sys.d3d12.vendor : sys.d3d11.vendor;
        x.vramMb = sys.d3d12.vramMb ? sys.d3d12.vramMb : sys.d3d11.vramMb;
        x.adapter = !sys.d3d12.adapter.empty() ? sys.d3d12.adapter : sys.d3d11.adapter;
        if (!sys.monitors.empty()) {
            x.screenWidth = sys.monitors[0].pixels.width;
            x.screenHeight = sys.monitors[0].pixels.height;
        }
        x.pack = GetFileAttributesW((m.paths().dir + L"\\uaro-textures.zip").c_str()) != INVALID_FILE_ATTRIBUTES;
        const AutoSetup a = autoSetup(x);
        m.applyPreset(a.preset);  // the game's own texture quality too
        std::string scale = "100";
        for (const IniEdit& e : a.edits) {
            if (const IniSetting* s = findSetting(e.section + "." + e.key)) m.setToken(*s, e.value);
            if (e.key == "render_scale") scale = e.value;
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
        text += L"\n" + t(ai) + L"\n\n" + t(Text::AutoAsk);
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
        const bool uaro = m.token(*findSetting("renderer.backend")) == "uaro";
        p.radio(kUaro, t(Text::RenderHeading), t(Text::RenderUaro), uaro, true, t(Text::RenderUaroHint),
                (win10 && d3d12) || d3d11 || uaro);
        p.radio(kDgVoodoo, L"", t(Text::RenderDgVoodoo), !uaro, false, t(Text::RenderDgVoodooHint));
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
                p.choice(kRows + int(i) * 4, t(s.label), items, selected, t(s.hint), uaro);
            }
        p.gap();
        for (size_t i = 0; i < all.size(); ++i)  // part 3 (test): the renderer in a 64-bit process; part 4: Remix
            if (sectionKey(all[i]) == "renderer.bridge" || sectionKey(all[i]) == "renderer.remix")
                p.check(kRows + int(i) * 4, t(all[i].label), m.token(all[i]) == "1", t(all[i].hint));
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
        p.note(t(now.dgvoodoo ? Text::DgVoodooFound : Text::DgVoodooMissing), now.dgvoodoo ? Tone::Good : Tone::Warn);
        p.gap();
        p.note(t(Text::PauseNote));
    }

    void buildRows(Panel& p) {
        Model& m = host_.model();
        const std::vector<IniSetting>& all = iniSettings();
        for (size_t i = 0; i < all.size(); ++i) {
            const IniSetting& s = all[i];
            if (s.page != which_) continue;
            const int id = kRows + int(i) * 4;
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
            if (!appliesNow(sectionKey(s))) p.note(t(Text::AfterRestart));
        }
        p.gap();
        p.note(t(m.now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
    }

    settings::Page which_;
    bool held_ = false;                   // a row is grey: the game runs and writes it itself
    std::vector<std::string> skinWords_;  // the skin list's words, in its order
};

}  // namespace

std::unique_ptr<Page> makeSettingsPage(PageHost& host, settings::Page which) {
    return std::make_unique<SettingsPage>(host, which);
}

}  // namespace uagfx::app

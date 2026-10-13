#include <cstdio>
#include <cstdlib>

#include "app/page.hpp"
#include "core/live_config.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kUaro = 1001;
constexpr int kDgVoodoo = 1002;
constexpr int kRows = 1100;  // + 4 x the setting's index in iniSettings()

std::string hresult(long hr) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

// Render, Quality and Frames (stage 2 spec §6.1): uaro-graphics.ini keys one to one.
class SettingsPage : public Page {
public:
    SettingsPage(PageHost& host, settings::Page which) : Page(host), which_(which) {}

    void build(Panel& p) override {
        p.heading(t(pageName(which_)));
        if (which_ == settings::Page::Render)
            buildRender(p);
        else
            buildRows(p);
    }

    void command(int id, int code) override {
        Model& m = host_.model();
        if ((id == kUaro || id == kDgVoodoo) && code == BN_CLICKED) {
            m.setToken(*findSetting("renderer.backend"), id == kUaro ? "uaro" : "dgvoodoo");
            host_.changed();
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
        if (id < kRows) return nullptr;
        const size_t i = size_t(id - kRows) / 4;
        return (id - kRows) % 4 == 0 && i < iniSettings().size() ? &iniSettings()[i] : nullptr;
    }

    void buildRender(Panel& p) {
        Model& m = host_.model();
        const SystemInfo& sys = m.system();
        const bool win10 = sys.windows.major >= 10;
        const bool d3d12 = sys.d3d12Probed && sys.d3d12.ok;
        const bool uaro = m.token(*findSetting("renderer.backend")) == "uaro";
        p.radio(kUaro, t(Text::RenderHeading), t(Text::RenderUaro), uaro, true, t(Text::RenderUaroHint),
                (win10 && d3d12) || uaro);
        p.radio(kDgVoodoo, L"", t(Text::RenderDgVoodoo), !uaro, false, t(Text::RenderDgVoodooHint));
        p.gap();
        if (!win10)
            p.note(t(Text::RenderNeedsWindows10), Tone::Bad);
        else if (!d3d12)
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
            }
            if (!appliesNow(sectionKey(s))) p.note(t(Text::AfterRestart));
        }
        p.gap();
        p.note(t(m.now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
    }

    settings::Page which_;
};

}  // namespace

std::unique_ptr<Page> makeSettingsPage(PageHost& host, settings::Page which) {
    return std::make_unique<SettingsPage>(host, which);
}

}  // namespace uagfx::app

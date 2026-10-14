#include <algorithm>
#include <cstdlib>
#include <string>

#include "app/page.hpp"
#include "core/live_config.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kGlow = 1001;
constexpr int kStrength = 1010;  // its number is 1011
constexpr int kSize = 1020;      // its number is 1021

const IniSetting& setting(const char* key) { return *findSetting(key); }

// The [effects] section (0.18): each effect a box, its settings under it, grey while it is off. They apply in a
// running game: a box at once, a slider when it is let go.
class EffectsPage : public Page {
public:
    explicit EffectsPage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        p.heading(t(Text::PageEffects));
        p.note(t(Text::FxIntro), Tone::Normal);
        p.gap();
        const bool glow = m.token(setting("effects.glow")) == "1";
        p.check(kGlow, t(Text::FxGlow), glow, t(Text::FxGlowHint));
        slider(p, kStrength, "effects.glow_strength", glow);
        slider(p, kSize, "effects.glow_size", glow);
        p.gap();
        p.note(t(m.now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
    }

    void command(int id, int code) override {
        if (id != kGlow || code != BN_CLICKED) return;
        const bool checked = SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED;
        set(setting("effects.glow"), checked ? "1" : "0", true);
        host_.rebuild();  // the sliders follow the box
    }

    void scroll(int id, int code, int pos) override {
        if (id != kStrength && id != kSize) return;
        const IniSetting& s = setting(id == kStrength ? "effects.glow_strength" : "effects.glow_size");
        const int step = std::max(s.step, 1);
        const int v = std::clamp((pos + step / 2) / step * step, s.min, s.max);
        if (v != pos) SendMessageW(GetDlgItem(host_.window(), id), TBM_SETPOS, TRUE, v);
        const std::string token = std::to_string(v);
        SetDlgItemTextW(host_.window(), id + 1, wide(token + s.unit).c_str());
        set(s, token, code == TB_ENDTRACK);
    }

private:
    void slider(Panel& p, int id, const char* key, bool enabled) {
        const IniSetting& s = setting(key);
        const std::string token = host_.model().token(s);
        p.slider(id, t(s.label), s.min, s.max, s.step, std::atoi(token.c_str()), wide(token + s.unit), t(s.hint),
                 enabled);
    }

    // The player's change; with `now` and the game running it goes into the file at once (the driver reads it within
    // a second) and Undo brings it back.
    void set(const IniSetting& s, const std::string& token, bool now) {
        Model& m = host_.model();
        m.setToken(s, token);
        if (now && m.now().gameRunning && appliesNow(sectionKey(s))) {
            const std::wstring err = m.writeNow(s, token, host_.lang());
            if (!err.empty()) host_.message(err, true);
        }
        host_.changed();
    }
};

}  // namespace

std::unique_ptr<Page> makeEffectsPage(PageHost& host) { return std::make_unique<EffectsPage>(host); }

}  // namespace uagfx::app

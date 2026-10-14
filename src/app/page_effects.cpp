#include <algorithm>
#include <cstdlib>
#include <string>

#include "app/page.hpp"
#include "core/live_config.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

// Each effect: its box and the sliders under it. Ids: the box 1001 + 100 * effect, a slider 1010 + 100 * effect +
// 10 * its place (its number one more).
struct Effect {
    const char* box;
    const char* sliders[3];
};
const Effect kEffects[] = {
    {"effects.glow", {"effects.glow_strength", "effects.glow_size", nullptr}},
    {"effects.water", {"effects.water_waves", "effects.water_reflections", "effects.water_clarity"}},
};
constexpr int kEffectCount = int(sizeof kEffects / sizeof kEffects[0]);

int boxId(int e) { return 1001 + 100 * e; }
int sliderId(int e, int k) { return 1010 + 100 * e + 10 * k; }

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
        for (int e = 0; e < kEffectCount; ++e) {
            p.gap();
            const IniSetting& box = setting(kEffects[e].box);
            const bool on = m.token(box) == "1";
            p.check(boxId(e), t(box.label), on, t(box.hint));
            for (int k = 0; k < 3 && kEffects[e].sliders[k]; ++k) slider(p, sliderId(e, k), kEffects[e].sliders[k], on);
        }
        p.gap();
        p.note(t(m.now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
    }

    void command(int id, int code) override {
        if (code != BN_CLICKED) return;
        for (int e = 0; e < kEffectCount; ++e) {
            if (id != boxId(e)) continue;
            const bool checked = SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            set(setting(kEffects[e].box), checked ? "1" : "0", true);
            host_.rebuild();  // the sliders follow the box
            return;
        }
    }

    void scroll(int id, int code, int pos) override {
        const char* key = sliderKey(id);
        if (!key) return;
        const IniSetting& s = setting(key);
        const int step = std::max(s.step, 1);
        const int v = std::clamp((pos + step / 2) / step * step, s.min, s.max);
        if (v != pos) SendMessageW(GetDlgItem(host_.window(), id), TBM_SETPOS, TRUE, v);
        const std::string token = std::to_string(v);
        SetDlgItemTextW(host_.window(), id + 1, wide(token + s.unit).c_str());
        set(s, token, code == TB_ENDTRACK);
    }

private:
    static const char* sliderKey(int id) {
        for (int e = 0; e < kEffectCount; ++e)
            for (int k = 0; k < 3 && kEffects[e].sliders[k]; ++k)
                if (id == sliderId(e, k)) return kEffects[e].sliders[k];
        return nullptr;
    }

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

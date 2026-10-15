#include <algorithm>
#include <cstdlib>
#include <string>

#include "app/page.hpp"
#include "core/live_config.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

// Each effect: its box, the boxes under it (0.23.2) and its sliders - a slider may need one of those boxes on too.
// Ids: the box 1001 + 100 * effect, a slider 1010 + 100 * effect + 10 * its place (its number one more), a box under
// it 1070 + 100 * effect + 2 * its place. The map's objects drawn anew (0.23.2: object) are a block of their own.
struct Effect {
    const char* box;
    const char* checks[2];
    const char* sliders[5];
    const char* needs[5];  // the box under it each slider needs on, or none
    bool object;
};
const Effect kEffects[] = {
    {"effects.smooth_light", {}, {}, {}, false},
    {"effects.ao", {}, {"effects.ao_strength", "effects.ao_size"}, {}, false},
    {"effects.glow", {}, {"effects.glow_strength", "effects.glow_size", "effects.glow_light"}, {}, false},
    {"effects.fire_light", {"effects.skill_colours"}, {"effects.fire_light_strength"}, {}, false},
    {"effects.actor_shadows", {}, {"effects.actor_shadows_strength"}, {}, false},
    {"effects.clouds", {}, {"effects.clouds_strength"}, {}, false},
    {"effects.haze", {}, {"effects.haze_strength"}, {}, false},
    {"effects.heat", {}, {"effects.heat_strength"}, {}, false},
    {"effects.weather", {}, {"effects.weather_amount"}, {}, false},
    {"effects.aura", {}, {"effects.aura_brightness"}, {}, false},
    {"effects.skills_soft", {}, {"effects.skill_brightness"}, {}, false},
    {"effects.water", {}, {"effects.water_waves", "effects.water_reflections", "effects.water_clarity"}, {}, false},
    {"effects.grade",
     {},
     {"effects.grade_saturation", "effects.grade_contrast", "effects.grade_brightness", "effects.grade_warmth",
      "effects.grade_vignette"},
     {},
     false},
    {"effects.night", {"effects.night_clock"}, {"effects.night_strength"}, {}, false},  // 0.63
    {"effects.foliage", {}, {"effects.foliage_strength"}, {}, false},                    // 0.64
    {"effects.soft_particles", {}, {}, {}, false},  // 0.67
    {"effects.dof", {}, {"effects.dof_strength", "effects.dof_range"}, {}, false},
    {"effects.torch",
     {"effects.torch_flame"},
     {"effects.torch_brightness", "effects.torch_glow", "effects.torch_light", "effects.torch_flicker"},
     {"effects.torch_flame"},
     true},
    {"effects.smoke", {}, {"effects.smoke_density"}, {}, true},
    {"effects.lamps", {}, {"effects.lamps_light"}, {}, true},
    {"effects.fireflies", {}, {"effects.fireflies_brightness"}, {}, true},
    {"effects.map_glows", {}, {"effects.map_glows_brightness"}, {}, true},
    {"effects.rays", {}, {"effects.rays_brightness"}, {}, true},
    {"effects.lava", {}, {"effects.lava_brightness"}, {}, true},
    {"effects.portals", {}, {"effects.portals_strength"}, {}, true},
};
constexpr int kEffectCount = int(sizeof kEffects / sizeof kEffects[0]);

int boxId(int e) { return 1001 + 100 * e; }
int sliderId(int e, int k) { return 1010 + 100 * e + 10 * k; }
int checkId(int e, int k) { return 1070 + 100 * e + 2 * k; }

const IniSetting& setting(const char* key) { return *findSetting(key); }

// The [effects] section (0.18): each effect a box, its settings under it, grey while it is off. They apply in a
// running game: a box at once, a slider when it is let go.
class EffectsPage : public Page {
public:
    explicit EffectsPage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        p.heading(t(Text::PageEffects));
        p.note(t(Text::FxIntro), Tone::Normal);
        bool objects = false;
        for (int e = 0; e < kEffectCount; ++e) {
            const Effect& fx = kEffects[e];
            if (fx.object && !objects) {  // 0.23.2: the map's objects, a block of their own
                objects = true;
                p.gap(16);
                p.heading(t(Text::FxObjects));
                p.note(t(Text::FxObjectsIntro), Tone::Normal);
            }
            p.gap();
            const IniSetting& box = setting(fx.box);
            const bool on = isOn(fx.box);
            p.check(boxId(e), t(box.label), on, t(box.hint));
            for (int k = 0; k < 2 && fx.checks[k]; ++k) {
                const IniSetting& c = setting(fx.checks[k]);
                p.check(checkId(e, k), t(c.label), isOn(fx.checks[k]), t(c.hint), on, true);
            }
            for (int k = 0; k < 5 && fx.sliders[k]; ++k)
                slider(p, sliderId(e, k), fx.sliders[k], on && (!fx.needs[k] || isOn(fx.needs[k])));
        }
        p.gap();
        p.note(t(host_.model().now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
    }

    void command(int id, int code) override {
        if (code != BN_CLICKED) return;
        for (int e = 0; e < kEffectCount; ++e) {
            const char* key = id == boxId(e) ? kEffects[e].box : nullptr;
            for (int k = 0; k < 2 && !key && kEffects[e].checks[k]; ++k)
                if (id == checkId(e, k)) key = kEffects[e].checks[k];
            if (!key) continue;
            const bool checked = SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            set(setting(key), checked ? "1" : "0", true);
            host_.rebuild();  // what is under it follows the box
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
    bool isOn(const char* key) { return host_.model().token(setting(key)) == "1"; }

    static const char* sliderKey(int id) {
        for (int e = 0; e < kEffectCount; ++e)
            for (int k = 0; k < 5 && kEffects[e].sliders[k]; ++k)
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

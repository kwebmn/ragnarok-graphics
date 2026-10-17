#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/files.hpp"
#include "app/page.hpp"
#include "core/effect_costs.hpp"
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
    int block;             // 0 the picture's effects, 1 the map's objects (0.23.2), 2 the living world (1.7)
};
const Effect kEffects[] = {
    {"effects.smooth_light", {}, {}, {}, 0},
    {"effects.ao", {}, {"effects.ao_strength", "effects.ao_size"}, {}, 0},
    {"effects.sun_shadows", {}, {"effects.sun_shadows_strength"}, {}, 0},  // 0.77
    {"effects.bloom", {}, {"effects.bloom_strength"}, {}, 0},  // 0.78
    {"effects.filmic", {}, {}, {}, 0},                           // 0.78
    {"effects.region_looks", {}, {"effects.region_looks_strength"}, {}, 0},  // 0.79
    {"effects.glow", {}, {"effects.glow_strength", "effects.glow_size", "effects.glow_light"}, {}, 0},
    {"effects.fire_light", {"effects.skill_colours"}, {"effects.fire_light_strength"}, {}, 0},
    {"effects.actor_shadows", {}, {"effects.actor_shadows_strength"}, {}, 0},
    {"effects.clouds", {}, {"effects.clouds_strength"}, {}, 0},
    {"effects.haze", {}, {"effects.haze_strength"}, {}, 0},
    {"effects.heat", {}, {"effects.heat_strength"}, {}, 0},
    {"effects.weather", {}, {"effects.weather_amount"}, {}, 0},
    {"effects.footprints", {}, {}, {}, 0},  // 0.80
    {"effects.storms", {}, {}, {}, 0},      // 0.80
    {"effects.grass", {}, {"effects.grass_amount"}, {}, 0},  // 0.81
    {"effects.skill_sparks", {}, {"effects.skill_sparks_amount"}, {}, 0},  // 0.82
    {"effects.film_grain", {}, {"effects.film_grain_amount"}, {}, 0},  // 0.83
    {"effects.map_fade", {}, {}, {}, 0},                                // 0.83
    {"effects.round_light", {}, {}, {}, 0},  // 0.84
    {"effects.appear", {}, {}, {}, 0},  // 0.93
    {"effects.ripples", {}, {}, {}, 0},  // 0.94
    {"effects.glint", {}, {}, {}, 0},  // 0.95
    {"effects.hit_flash", {}, {}, {}, 0},  // 0.96
    {"effects.wet_ground", {}, {}, {}, 0},  // 0.97
    {"effects.flare", {}, {}, {}, 0},  // 0.98
    {"effects.aura", {}, {"effects.aura_brightness"}, {}, 0},
    {"effects.skills_soft", {}, {"effects.skill_brightness"}, {}, 0},
    {"effects.water", {}, {"effects.water_waves", "effects.water_reflections", "effects.water_clarity"}, {}, 0},
    {"effects.grade",
     {},
     {"effects.grade_saturation", "effects.grade_contrast", "effects.grade_brightness", "effects.grade_warmth",
      "effects.grade_vignette"},
     {},
     0},
    {"effects.night", {"effects.night_clock"}, {"effects.night_strength"}, {}, 0},  // 0.63
    {"effects.foliage", {}, {"effects.foliage_strength"}, {}, 0},                    // 0.64
    {"effects.soft_particles", {}, {}, {}, 0},  // 0.67
    {"effects.rim_light", {}, {"effects.rim_strength"}, {}, 0},  // 0.68
    {"effects.sky", {}, {"effects.sky_clouds", "effects.sky_strength"}, {}, 0},  // 0.69
    {"effects.motes", {}, {"effects.motes_amount"}, {}, 0},  // 0.70
    {"effects.relief", {}, {"effects.relief_strength"}, {}, 0},  // 0.71
    {"effects.floor_reflections", {}, {"effects.floor_reflect_strength"}, {}, 0},  // 0.72
    {"effects.shafts", {}, {"effects.shafts_strength"}, {}, 0},  // 0.73
    {"effects.day_cycle", {}, {}, {}, 0},  // 0.74
    {"effects.dof", {}, {"effects.dof_strength", "effects.dof_range"}, {}, 0},
    {"effects.torch",
     {"effects.torch_flame"},
     {"effects.torch_brightness", "effects.torch_glow", "effects.torch_light", "effects.torch_flicker"},
     {"effects.torch_flame"},
     1},
    {"effects.smoke", {}, {"effects.smoke_density"}, {}, 1},
    {"effects.lamps", {}, {"effects.lamps_light"}, {}, 1},
    {"effects.fireflies", {}, {"effects.fireflies_brightness"}, {}, 1},
    {"effects.map_glows", {}, {"effects.map_glows_brightness"}, {}, 1},
    {"effects.rays", {}, {"effects.rays_brightness"}, {}, 1},
    {"effects.lava", {}, {"effects.lava_brightness"}, {}, 1},
    {"effects.portals", {}, {"effects.portals_strength"}, {}, 1},
    {"effects.tree_volume", {}, {}, {}, 2},  // 1.7
    {"effects.leaf_light", {}, {}, {}, 2},  // 1.7
    {"effects.tree_tints", {}, {}, {}, 2},  // 1.7
    {"effects.gusts", {}, {}, {}, 2},  // 1.7
    {"effects.dapple", {}, {}, {}, 2},  // 1.7
    {"effects.flowers", {}, {}, {}, 2},  // 1.7.1
    {"effects.dew", {}, {}, {}, 2},  // 1.7.1
    {"effects.morning_fog", {}, {}, {}, 2},  // 1.7.1
    {"effects.sand_drift", {}, {}, {}, 2},  // 1.7.1
    {"effects.snow_sparkle", {}, {}, {}, 2},  // 1.7.1
    {"effects.wet_walls", {}, {}, {}, 2},  // 1.7.1
    {"effects.shore_foam", {}, {}, {}, 2},  // 1.7.2
    {"effects.fish", {}, {}, {}, 2},  // 1.7.2
    {"effects.swamp", {}, {}, {}, 2},  // 1.7.2
    {"effects.butterflies", {}, {}, {}, 2},  // 1.7.3
    {"effects.birds", {}, {}, {}, 2},  // 1.7.3
    {"effects.leaf_fall", {}, {}, {}, 2},  // 1.7.3
    {"effects.embers", {}, {}, {}, 2},  // 1.7.3
    {"effects.breath", {}, {}, {}, 2},  // 1.7.3
    {"effects.splashes", {}, {}, {}, 2},  // 1.7.3
    {"effects.cave_drips", {}, {}, {}, 2},  // 1.7.3
    {"effects.underwater", {}, {}, {}, 2},  // 1.7.3
    {"effects.materials", {}, {"effects.materials_strength"}, {}, 3},  // 1.8
    {"effects.ground_seams", {}, {}, {}, 3},  // 1.9
};
constexpr int kEffectCount = int(sizeof kEffects / sizeof kEffects[0]);

constexpr int kTier = 990;  // 1.8: the remaster's mode
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
        // 0.76: what each cost the card in the last game (the driver's uaro-graphics\effect-costs.txt)
        std::string text;
        const effect_costs::Costs costs =
            readFile(host_.model().paths().dir + L"\\uaro-graphics\\effect-costs.txt", text)
                ? effect_costs::parse(text)
                : effect_costs::Costs{};
        int block = 0;
        for (int e = 0; e < kEffectCount; ++e) {
            const Effect& fx = kEffects[e];
            if (fx.block != block) {  // 0.23.2: the map's objects, 1.7: the living world - blocks of their own
                block = fx.block;
                p.gap(16);
                p.heading(t(block == 1 ? Text::FxObjects : block == 2 ? Text::FxLiving : Text::FxRemaster));
                p.note(t(block == 1   ? Text::FxObjectsIntro
                         : block == 2 ? Text::FxLivingIntro
                                      : Text::FxRemasterIntro),
                       Tone::Normal);
                if (block == 3) {  // 1.8: the mode, for all of the block
                    const IniSetting& tier = setting("effects.tier");
                    const std::string token = host_.model().token(tier);
                    p.choice(kTier, t(tier.label),
                             {t(Text::FxTierAuto), t(Text::FxTierClassic), t(Text::FxTierRemaster)},
                             token == "classic" ? 1 : token == "remaster" ? 2 : 0, t(tier.hint));
                }
            }
            p.gap();
            const IniSetting& box = setting(fx.box);
            const bool on = isOn(fx.box);
            std::wstring label = t(box.label);
            const double ms = effect_costs::costOf(costs, fx.box);
            if (ms >= 0.0) {
                char v[16];
                std::snprintf(v, sizeof v, "%.1f", std::max(ms, 0.1));
                label += tf(Text::FxCostMs, {v});
            }
            p.check(boxId(e), label, on, t(box.hint));
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
        if (id == kTier && code == CBN_SELCHANGE) {  // 1.8
            const LRESULT sel = SendMessageW(GetDlgItem(host_.window(), id), CB_GETCURSEL, 0, 0);
            static const char* const kTiers[] = {"auto", "classic", "remaster"};
            if (sel >= 0 && sel < 3) set(setting("effects.tier"), kTiers[sel], true);
            return;
        }
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

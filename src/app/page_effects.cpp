#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/files.hpp"
#include "app/page.hpp"
#include "core/effect_costs.hpp"
#include "core/live_config.hpp"
#include "core/settings/effects_layout.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

using Effect = EffectLayout;  // 1.9.3: the layout in core/settings/effects_layout.cpp, shared with the console
const Effect* const kEffects = effectLayout();
const int kEffectCount = effectLayoutCount();

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

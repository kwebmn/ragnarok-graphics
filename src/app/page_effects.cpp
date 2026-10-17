#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/files.hpp"
#include "app/page.hpp"
#include "core/effect_costs.hpp"
#include "core/effect_groups.hpp"
#include "core/live_config.hpp"
#include "core/settings/effects_layout.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

using Effect = EffectLayout;  // 1.9.3: the layout in core/settings/effects_layout.cpp, shared with the console
const Effect* const kEffects = effectLayout();
const int kEffectCount = effectLayoutCount();

constexpr int kTier = 990;  // 1.8: the remaster's mode
constexpr int kBlockOn = 970, kBlockOff = 975;  // 1.13: a block's boxes all on, all off (+ the block)
// 2.2: the set for the card, the details' button, each group's box and slider (its number + 1)
constexpr int kPreset = 960, kDetails = 961;
int groupBox(int g) { return 900 + 4 * g; }
int groupSlider(int g) { return 902 + 4 * g; }

const char* const kPresets[] = {"auto", "off", "weak", "medium", "strong", "custom"};
const Text kGroupNames[kEffectGroups] = {Text::FxGroupLight,  Text::FxGroupShadows, Text::FxGroupSurfaces,
                                         Text::FxGroupNature, Text::FxGroupWater,   Text::FxGroupAir,
                                         Text::FxGroupLife,   Text::FxGroupSkills,  Text::FxGroupPicture};

std::string groupKey(int g) { return std::string("effects.group_") + effect_groups::groupWord(g); }

int boxId(int e) { return 1001 + 100 * e; }
int sliderId(int e, int k) { return 1010 + 100 * e + 10 * k; }
int checkId(int e, int k) { return 1070 + 100 * e + 2 * k; }

const IniSetting& setting(const char* key) { return *findSetting(key); }
const IniSetting& setting(const std::string& key) { return *findSetting(key); }

// The [effects] section (0.18): each effect a box, its settings under it, grey while it is off. They apply in a
// running game: a box at once, a slider when it is let go.
class EffectsPage : public Page {
public:
    explicit EffectsPage(PageHost& host) : Page(host) {}

    // 2.2: the set for the card and nine groups, each on or off with its strength; every effect on its own under
    // Details (the list as it was), where a change makes the set "custom".
    void build(Panel& p) override {
        Model& m = host_.model();
        p.heading(t(Text::PageEffects));
        p.note(t(Text::FxSetIntro), Tone::Normal);
        // 0.76: what each cost the card in the last game (the driver's uaro-graphics\effect-costs.txt)
        std::string text;
        const effect_costs::Costs costs =
            readFile(m.paths().dir + L"\\uaro-graphics\\effect-costs.txt", text) ? effect_costs::parse(text)
                                                                                   : effect_costs::Costs{};
        const IniSetting& preset = setting("effects.preset");
        const std::string word = m.token(preset);
        static const Text kLevels[] = {Text::FxPresetWeak, Text::FxPresetWeak, Text::FxPresetMedium,
                                       Text::FxPresetStrong};
        std::vector<std::wstring> items;
        int selected = 0;
        for (int i = 0; i < 6; ++i) {
            items.push_back(i == 0 ? tf(Text::FxPresetAuto, {utf8(t(kLevels[cardLevel()]))}) : t(preset.choices[i]));
            if (word == kPresets[i]) selected = i;
        }
        p.choice(kPreset, t(preset.label), items, selected, t(preset.hint));
        const bool any = word != "off";
        for (int g = 0; g < kEffectGroups; ++g) {
            p.gap();
            const bool on = m.token(setting(groupKey(g))) == "1";
            std::wstring label = t(kGroupNames[g]);
            double ms = 0.0;
            for (int i = 0; i < effect_groups::memberCount(); ++i) {
                const effect_groups::Member& x = effect_groups::members()[i];
                const double c = x.group == g ? effect_costs::costOf(costs, x.key) : -1.0;
                if (c > 0.0) ms += c;
            }
            if (ms > 0.0) {
                char v[16];
                std::snprintf(v, sizeof v, "%.1f", std::max(ms, 0.1));
                label += tf(Text::FxCostMs, {v});
            }
            p.check(groupBox(g), label, on, t(setting(groupKey(g)).hint), any);
            if (effect_groups::hasStrength(g))
                slider(p, groupSlider(g), (groupKey(g) + "_strength").c_str(), any && on);
        }
        p.gap(16);
        p.buttons({{kDetails, t(details_ ? Text::FxDetailsHide : Text::FxDetails)}});
        if (!details_) {
            p.gap();
            p.note(t(m.now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
            return;
        }
        p.note(t(Text::FxDetailsNote), Tone::Normal);
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
                if (block >= 2)  // 1.13: the whole block on or off at once
                    p.buttons({{kBlockOn + block, t(Text::FxBlockOn)}, {kBlockOff + block, t(Text::FxBlockOff)}});
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
        if (id == kPreset && code == CBN_SELCHANGE) {  // 2.2
            const LRESULT sel = SendMessageW(GetDlgItem(host_.window(), id), CB_GETCURSEL, 0, 0);
            if (sel < 0 || sel >= 6) return;
            if (std::string(kPresets[sel]) == "custom")
                makeCustom();  // from what the set had on
            else
                set(setting("effects.preset"), kPresets[sel], true);
            host_.rebuild();
            return;
        }
        if (id == kTier && code == CBN_SELCHANGE) {  // 1.8
            const LRESULT sel = SendMessageW(GetDlgItem(host_.window(), id), CB_GETCURSEL, 0, 0);
            static const char* const kTiers[] = {"auto", "classic", "remaster"};
            if (sel >= 0 && sel < 3) set(setting("effects.tier"), kTiers[sel], true);
            return;
        }
        if (code != BN_CLICKED) return;
        if (id == kDetails) {
            details_ = !details_;
            host_.rebuild();
            return;
        }
        for (int g = 0; g < kEffectGroups; ++g)
            if (id == groupBox(g)) {
                const bool checked = SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED;
                set(setting(groupKey(g)), checked ? "1" : "0", true);
                host_.rebuild();
                return;
            }
        if ((id >= kBlockOn && id < kBlockOn + 4) || (id >= kBlockOff && id < kBlockOff + 4)) {  // 1.13
            const bool on = id < kBlockOff;
            const int block = id - (on ? kBlockOn : kBlockOff);
            makeCustom();
            for (int e = 0; e < kEffectCount; ++e)
                if (kEffects[e].block == block) set(setting(kEffects[e].box), on ? "1" : "0", true);
            host_.rebuild();
            return;
        }
        for (int e = 0; e < kEffectCount; ++e) {
            const char* key = id == boxId(e) ? kEffects[e].box : nullptr;
            for (int k = 0; k < 2 && !key && kEffects[e].checks[k]; ++k)
                if (id == checkId(e, k)) key = kEffects[e].checks[k];
            if (!key) continue;
            const bool checked = SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (effect_groups::memberOf(key)) makeCustom();  // 2.2: a switch of its own - the set is the player's
            set(setting(key), checked ? "1" : "0", true);
            host_.rebuild();  // what is under it follows the box
            return;
        }
    }

    void scroll(int id, int code, int pos) override {
        const std::string key = sliderKey(id);
        if (key.empty()) return;
        const IniSetting& s = setting(key);
        const int step = std::max(s.step, 1);
        const int v = std::clamp((pos + step / 2) / step * step, s.min, s.max);
        if (v != pos) SendMessageW(GetDlgItem(host_.window(), id), TBM_SETPOS, TRUE, v);
        const std::string token = std::to_string(v);
        SetDlgItemTextW(host_.window(), id + 1, wide(token + s.unit).c_str());
        set(s, token, code == TB_ENDTRACK);
    }

private:
    bool details_ = false;  // 2.2: every effect on its own shown

    bool isOn(const char* key) { return host_.model().token(setting(key)) == "1"; }

    static std::string sliderKey(int id) {
        for (int g = 0; g < kEffectGroups; ++g)
            if (id == groupSlider(g)) return groupKey(g) + "_strength";
        for (int e = 0; e < kEffectCount; ++e)
            for (int k = 0; k < 5 && kEffects[e].sliders[k]; ++k)
                if (id == sliderId(e, k)) return kEffects[e].sliders[k];
        return {};
    }

    // 2.2: the card's level for the automatic set, as the driver finds it (effect_groups::cardLevel)
    int cardLevel() const {
        Model& m = host_.model();
        const SystemInfo& sys = m.system();
        const bool d3d12 = sys.d3d12Probed && sys.d3d12.ok && m.token(setting("renderer.api")) != "d3d11";
        return effect_groups::cardLevel(d3d12, sys.d3d12.vramMb);
    }

    // 2.2: a set made the player's own - each switch written as the set has it now, then "custom"
    void makeCustom() {
        Model& m = host_.model();
        if (m.token(setting("effects.preset")) == "custom") return;
        EffectSettings e = m.pendingConfig().effects;
        for (bool& g : e.groups) g = true;  // the groups stay as they are, over the switches
        const EffectSettings now = effect_groups::effective(e, cardLevel());
        for (int i = 0; i < effect_groups::memberCount(); ++i) {
            const effect_groups::Member& x = effect_groups::members()[i];
            set(setting(std::string("effects.") + x.key), now.*x.on ? "1" : "0", true);
        }
        set(setting("effects.tier"), now.tier == 2 ? "remaster" : now.tier == 1 ? "classic" : "auto", true);
        set(setting("effects.preset"), "custom", true);
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

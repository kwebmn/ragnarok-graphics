#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
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

// The controls' numbers: the main window hands a page only those from 1000 on (2.2.1: the set's, the groups' and the
// mode's were under it - they did nothing).
constexpr int kPreset = 20000, kTier = 20001;
int groupBox(int g) { return 20100 + 20 * g; }
int groupSlider(int g) { return 20102 + 20 * g; }  // its number: + 1
int groupOpen(int g) { return 20105 + 20 * g; }
int boxId(int e) { return 1001 + 100 * e; }
int sliderId(int e, int k) { return 1010 + 100 * e + 10 * k; }
int checkId(int e, int k) { return 1070 + 100 * e + 2 * k; }

const char* const kPresets[] = {"auto", "off", "weak", "medium", "strong", "custom"};
const Text kGroupNames[kEffectGroups] = {Text::FxGroupFixes,  Text::FxGroupLight, Text::FxGroupShadows,
                                         Text::FxGroupSurfaces, Text::FxGroupNature, Text::FxGroupWater,
                                         Text::FxGroupAir,    Text::FxGroupLife,  Text::FxGroupSkills,
                                         Text::FxGroupPicture};

// 2.2.1: the groups opened to their effects - kept while the app runs (a page is made anew at every change)
bool g_open[kEffectGroups] = {};

std::string groupKey(int g) { return std::string("effects.group_") + effect_groups::groupWord(g); }

const IniSetting& setting(const char* key) { return *findSetting(key); }
const IniSetting& setting(const std::string& key) { return *findSetting(key); }

// The layout's entry of a group's member (its box, its own settings), -1 none.
int layoutOf(const effect_groups::Member& m) {
    const std::string box = std::string("effects.") + m.key;
    for (int e = 0; e < kEffectCount; ++e)
        if (box == kEffects[e].box) return e;
    return -1;
}

// The [effects] section (0.18), 2.2: the set for the card, then nine groups - each on or off with its strength, and
// opened (its button) to its effects, each a box with its settings under it, grey while it is off. They apply in a
// running game: a box at once, a slider when it is let go.
class EffectsPage : public Page {
public:
    explicit EffectsPage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        shown_ = effect_groups::shown(m.pendingConfig().effects, cardLevel());  // the switches as they work
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
        if (word == "custom") {  // 1.8: the mode - a set decides it (the strong one's the remaster's)
            const IniSetting& tier = setting("effects.tier");
            const std::string token = m.token(tier);
            p.choice(kTier, t(tier.label), {t(Text::FxTierAuto), t(Text::FxTierClassic), t(Text::FxTierRemaster)},
                     token == "classic" ? 1 : token == "remaster" ? 2 : 0, t(tier.hint));
        }
        p.note(t(Text::FxDetailsNote), Tone::Muted);
        const bool any = word != "off";
        for (int g = 0; g < kEffectGroups; ++g) {
            p.gap(12);
            const bool on = m.token(setting(groupKey(g))) == "1";
            // its effects' costs, each timed pass once (the motes and the weather are one pass)
            double ms = 0.0;
            int count = 0;
            std::set<std::string> passes;
            for (int i = 0; i < effect_groups::memberCount(); ++i) {
                const effect_groups::Member& x = effect_groups::members()[i];
                if (x.group != g) continue;
                ++count;
                const std::string pass = effect_costs::passOf(x.key);
                if (pass.empty() || !passes.insert(pass).second) continue;
                const double c = effect_costs::costOf(costs, x.key);
                if (c > 0.0) ms += c;
            }
            p.check(groupBox(g), withCost(t(kGroupNames[g]), ms), on, t(setting(groupKey(g)).hint), any);
            if (effect_groups::hasStrength(g))
                slider(p, groupSlider(g), (groupKey(g) + "_strength").c_str(), any && on);
            p.buttons({{groupOpen(g),
                        g_open[g] ? t(Text::FxDetailsHide) : tf(Text::FxDetails, {std::to_string(count)})}});
            if (!g_open[g]) continue;
            for (int i = 0; i < effect_groups::memberCount(); ++i) {
                const effect_groups::Member& x = effect_groups::members()[i];
                const int e = x.group == g ? layoutOf(x) : -1;
                if (e < 0) continue;
                const Effect& fx = kEffects[e];
                const IniSetting& box = setting(fx.box);
                const bool fxOn = isOn(fx.box);
                const bool live = any && on;
                p.check(boxId(e), withCost(t(box.label), effect_costs::costOf(costs, fx.box)), fxOn, t(box.hint),
                        live, true);
                for (int k = 0; k < kLayoutChecks && fx.checks[k]; ++k) {
                    const IniSetting& c = setting(fx.checks[k]);
                    p.check(checkId(e, k), t(c.label), isOn(fx.checks[k]), t(c.hint), live && fxOn, true);
                }
                for (int k = 0; k < 5 && fx.sliders[k]; ++k)
                    slider(p, sliderId(e, k), fx.sliders[k], live && fxOn && (!fx.needs[k] || isOn(fx.needs[k])));
            }
        }
        p.gap();
        p.note(t(m.now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
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
        for (int g = 0; g < kEffectGroups; ++g) {
            if (id == groupOpen(g)) {
                g_open[g] = !g_open[g];
                host_.rebuild();
                return;
            }
            if (id == groupBox(g)) {
                const bool checked = SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED;
                set(setting(groupKey(g)), checked ? "1" : "0", true);
                host_.rebuild();
                return;
            }
        }
        for (int e = 0; e < kEffectCount; ++e) {
            const char* key = id == boxId(e) ? kEffects[e].box : nullptr;
            for (int k = 0; k < kLayoutChecks && !key && kEffects[e].checks[k]; ++k)
                if (id == checkId(e, k)) key = kEffects[e].checks[k];
            if (!key) continue;
            const bool checked = SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            const char* token = checked ? "1" : "0";
            // 2.2: a switch of its own under a set - the set made the player's, with it, in one write
            if (!effect_groups::memberOf(key) || !makeCustom(key, token)) set(setting(key), token, true);
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
    std::wstring withCost(const std::wstring& label, double ms) const {
        if (ms <= 0.0) return label;
        char v[16];
        std::snprintf(v, sizeof v, "%.1f", std::max(ms, 0.1));
        return label + tf(Text::FxCostMs, {v});
    }

    // An effect's switch as it works: a set other than custom decides them (shown as it has them), else the file's.
    bool isOn(const char* key) const {
        if (const effect_groups::Member* m = effect_groups::memberOf(key)) return shown_.*m->on;
        return host_.model().token(setting(key)) == "1";
    }
    EffectSettings shown_;  // effect_groups::shown, as build() found them

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

    // 2.2: a set made the player's own (effect_groups::materialize) - each switch as the set has it now, the mode,
    // "custom", and `key` the switch the player changed: one write of the file in a running game. False: custom
    // already, nothing done.
    bool makeCustom(const char* key = nullptr, const char* token = nullptr) {
        Model& m = host_.model();
        const effect_groups::Member* changed = key ? effect_groups::memberOf(key) : nullptr;
        std::vector<std::pair<const IniSetting*, std::string>> changes;
        for (const auto& [k, v] : effect_groups::materialize(m.pendingConfig().effects, cardLevel(),
                                                             changed ? changed->key : ""))
            changes.push_back({&setting("effects." + k), v});
        if (changes.empty()) return false;
        if (changed) changes.push_back({&setting(key), token});
        for (const auto& [s, word] : changes) m.setToken(*s, word);
        if (m.now().gameRunning) {
            const std::wstring err = m.writeNow(changes, host_.lang());
            if (!err.empty()) host_.message(err, true);
        }
        host_.changed();
        return true;
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

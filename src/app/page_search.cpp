#include <utility>

#include "app/page.hpp"
#include "app/setting_ids.hpp"
#include "core/effect_groups.hpp"
#include "core/settings/effects_layout.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/search.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kGoFirst = 3000;  // + the hit's number: "to its page"

bool isEffects(const IniSetting& s) { return std::string(s.section) == "effects"; }

// The Effects page's group of an [effects] key: the group's own box and strength, an effect's switch, or a check or
// a slider of an effect (its box's group); -1 none.
int effectGroupOf(const std::string& key) {
    for (int g = 0; g < kEffectGroups; ++g) {
        const std::string word = std::string("effects.group_") + effect_groups::groupWord(g);
        if (key == word || key == word + "_strength") return g;
    }
    if (const effect_groups::Member* m = effect_groups::memberOf(key)) return m->group;
    const EffectLayout* fx = effectLayout();
    for (int e = 0; e < effectLayoutCount(); ++e) {
        bool mine = false;
        for (int k = 0; k < kLayoutChecks && fx[e].checks[k]; ++k) mine = mine || key == fx[e].checks[k];
        for (int k = 0; k < 5 && fx[e].sliders[k]; ++k) mine = mine || key == fx[e].sliders[k];
        if (mine)
            if (const effect_groups::Member* m = effect_groups::memberOf(fx[e].box)) return m->group;
    }
    return -1;
}

// 2.6: what the header's search found (core/settings/search) - each row as its page makes it, changed in place
// (the Effects page's rows by that page, the rest by a page of the schema's rows), or its name; under each the
// button to its page, "View › group".
class SearchPage : public Page {
public:
    SearchPage(PageHost& host, std::string query)
        : Page(host), query_(std::move(query)), rows_(makeSettingsPage(host, {})), effects_(makeEffectsPage(host)) {}

    void build(Panel& p) override {
        hits_ = findSettings(query_, host_.lang(), host_.mode());
        if (hits_.empty()) {
            p.heading(tf(Text::SearchNothing, {query_}));
            return;
        }
        p.heading(tf(Text::SearchFound, {std::to_string(hits_.size()), query_}));
        p.note(t(Text::SearchHow));
        if (hits_.size() >= 80) p.note(tf(Text::SearchMany, {std::to_string(hits_.size())}), Tone::Warn);
        for (size_t n = 0; n < hits_.size(); ++n) {
            const Hit& h = hits_[n];
            p.gap(8);
            if (!buildHit(p, h)) p.note(t(labelOf(h)), Tone::Normal);
            p.buttons({{kGoFirst + int(n), whereOf(h)}});
        }
    }

    void command(int id, int code) override {
        if (id >= kGoFirst && id < kGoFirst + int(hits_.size())) {
            const Place to = hits_[size_t(id - kGoFirst)].place;
            if (code == BN_CLICKED) host_.showView(to.view, to.tab);
            return;
        }
        const IniSetting* s = settingOfId(id);
        (s && isEffects(*s) ? effects_ : rows_)->command(id, code);
    }

    void scroll(int id, int code, int pos) override {
        const IniSetting* s = settingOfId(id);
        (s && isEffects(*s) ? effects_ : rows_)->scroll(id, code, pos);
    }

private:
    bool buildHit(Panel& p, const Hit& h) {
        if (h.kind == HitKind::Reg) return rows_->buildRow(p, "reg:" + regKey(regSettings()[h.index]));
        if (h.kind != HitKind::Ini) return false;
        const IniSetting& s = iniSettings()[h.index];
        const std::string key = sectionKey(s);
        if (isEffects(s)) return effects_->buildRow(p, key);
        return key != "stats.send" && rows_->buildRow(p, key);  // the answer goes through its own page (its id)
    }

    static Text labelOf(const Hit& h) {
        switch (h.kind) {
            case HitKind::Ini: return iniSettings()[h.index].label;
            case HitKind::Reg: return regSettings()[h.index].label;
            case HitKind::Action: break;
        }
        return actions()[h.index].label;
    }

    std::wstring whereOf(const Hit& h) const {
        std::wstring group;
        const View v = h.place.view;
        if (tabCount(v) > 1) {
            group = t(tabName(v, h.place.tab));
        } else if (h.kind == HitKind::Ini && isEffects(iniSettings()[h.index])) {
            const int g = effectGroupOf(sectionKey(iniSettings()[h.index]));
            if (g >= 0) group = t(effectGroupName(g));
        } else if (partsOf(v, 0).size() > 1 && h.kind != HitKind::Action) {  // Graphics, Game and chat, Service
            group = t(pageName(h.kind == HitKind::Ini ? iniSettings()[h.index].page : regSettings()[h.index].page));
        }
        return t(viewName(v)) + (group.empty() ? L"" : L" › " + group) + L"  →";
    }

    std::string query_;
    std::unique_ptr<Page> rows_;     // the schema's and the game key's rows, built and changed as their pages do
    std::unique_ptr<Page> effects_;  // the Effects page's
    std::vector<Hit> hits_;          // this build's
};

}  // namespace

std::unique_ptr<Page> makeSearchPage(PageHost& host, const std::string& query) {
    return std::make_unique<SearchPage>(host, query);
}

}  // namespace uagfx::app

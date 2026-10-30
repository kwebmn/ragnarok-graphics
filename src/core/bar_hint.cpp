#include "core/bar_hint.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>

#include "core/reference_pages.hpp"

namespace uagfx::barhint {
namespace {

struct Element {
    const char* skill;
    int element;
};
constexpr Element kElements[] = {
#include "core/skill_elements.inc"
};

// A multiplier as the player reads it: ×2, ×1,5 (×1.5 in English), ×0,25
std::string times(int percent, settings::Lang l) {
    std::string n = std::to_string(percent / 100);
    int rest = percent % 100;
    if (rest) {
        std::string tail = (rest < 10 ? "0" : "") + std::to_string(rest);
        while (!tail.empty() && tail.back() == '0') tail.pop_back();
        n += (l == settings::Lang::En ? "." : ",") + tail;
    }
    return "\xC3\x97" + n;
}

}  // namespace

int elementOf(const std::string& skill) {
    const Element* e = std::lower_bound(std::begin(kElements), std::end(kElements), skill,
                                        [](const Element& x, const std::string& v) { return v.compare(x.skill) > 0; });
    return e != std::end(kElements) && skill == e->skill ? e->element : -1;
}

std::vector<Hit> against(const std::vector<Skill>& bar, int element, int level) {
    std::vector<Hit> out;
    std::vector<std::string> seen;
    for (const Skill& s : bar) {
        const int e = elementOf(s.id);
        if (e < 0 || std::find(seen.begin(), seen.end(), s.id) != seen.end()) continue;
        seen.push_back(s.id);
        out.push_back(Hit{s.name, refpages::elementDamage(e, element, level)});
    }
    std::stable_sort(out.begin(), out.end(), [](const Hit& a, const Hit& b) { return a.percent > b.percent; });
    return out;
}

std::string line(const std::vector<Hit>& hits, settings::Lang l, size_t most) {
    std::string out;
    for (size_t i = 0; i < hits.size() && i < most; ++i) {
        const Hit& h = hits[i];
        out += (out.empty() ? "" : " \xC2\xB7 ") + h.name + " " +
               (h.percent < 0 ? settings::tr(l, settings::Text::RefBarHeals) : times(h.percent, l));
    }
    return out;
}

}  // namespace uagfx::barhint

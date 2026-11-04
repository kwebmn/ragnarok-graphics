#include "core/skill_tree_view.hpp"

#include <algorithm>
#include <functional>
#include <map>

#include "core/skill_window.hpp"

namespace uagfx::skilltreeview {
namespace {

const skillwin::Skill* find(const skillwin::Data& d, int id) {
    for (const skillwin::Tab& t : d.tabs)
        for (const skillwin::Skill& k : t.skills)
            if (k.id == id) return &k;
    return nullptr;
}

}  // namespace

int levelOf(const skillwin::Data& d, int id) {
    const skillwin::Skill* k = find(d, id);
    return k ? k->level : 0;
}

int pathCost(const skillwin::Data& d, int id) {
    std::map<int, int> want;  // a skill to the level the path needs
    const std::function<void(int, int)> visit = [&](int at, int level) {
        if (levelOf(d, at) >= level) return;
        int& w = want[at];
        if (w >= level) return;
        w = level;
        if (const skillwin::Skill* k = find(d, at))
            for (const auto& [need, lv] : k->req) visit(need, lv);
    };
    visit(id, 1);
    int cost = 0;
    for (const auto& [at, level] : want) cost += std::max(0, level - levelOf(d, at));
    return cost;
}

std::set<int> chain(const skillwin::Data& d, int id) {
    std::set<int> out;
    const std::function<void(int)> visit = [&](int at) {
        if (const skillwin::Skill* k = find(d, at))
            for (const auto& [need, lv] : k->req)
                if (out.insert(need).second) visit(need);
    };
    visit(id);
    return out;
}

Plan arrange(const skillwin::Data& d, int tab, float scale) {
    Plan p;
    if (d.tabs.empty()) return p;
    const float s = scale > 0 ? scale : 1.0f;
    const skillwin::Tab& t = d.tabs[size_t(std::clamp(tab, 0, int(d.tabs.size()) - 1))];
    std::map<int, int> onTab;  // a skill's id to its index on the tab
    for (size_t i = 0; i < t.skills.size(); ++i) onTab[t.skills[i].id] = int(i);
    // the depth on the tab: one more than the deepest requirement on it
    std::vector<int> depth(t.skills.size(), -1);
    const std::function<int(int)> depthOf = [&](int i) -> int {
        if (depth[size_t(i)] >= 0) return depth[size_t(i)];
        depth[size_t(i)] = 0;  // a loop (never in the server's tree) stops here
        int most = 0;
        for (const auto& [need, lv] : t.skills[size_t(i)].req) {
            const auto at = onTab.find(need);
            if (at != onTab.end()) most = std::max(most, depthOf(at->second) + 1);
        }
        return depth[size_t(i)] = most;
    };
    // the chips: the requirements from the jobs before, in the order their first dependants come
    std::map<int, int> chipOf;
    for (size_t i = 0; i < t.skills.size(); ++i)
        for (const auto& [need, lv] : t.skills[i].req)
            if (!onTab.count(need) && !chipOf.count(need)) {
                chipOf[need] = int(p.chips.size());
                Chip c;
                c.id = need;
                const skillwin::Skill* k = find(d, need);
                c.name = k ? k->name : "#" + std::to_string(need);
                c.level = k ? k->level : 0;
                p.chips.push_back(c);
            }
    const float left = (p.chips.empty() ? kMargin : kChipsW) * s;
    for (size_t i = 0; i < p.chips.size(); ++i)
        p.chips[i].box = panel::Box{kMargin * s, (kMargin + float(i) * kChipStep) * s, kChipW * s, kChipH * s};
    // the rows: each column top down; a skill as high as its first requirement's row, else the next free
    std::map<int, std::set<int>> taken;  // a column's rows
    std::vector<int> rowOf(t.skills.size(), 0);
    std::vector<int> order(t.skills.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = int(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return depthOf(a) < depthOf(b); });
    for (int i : order) {
        int want = 0;
        bool found = false;
        for (const auto& [need, lv] : t.skills[size_t(i)].req) {
            int r = -1;
            if (const auto at = onTab.find(need); at != onTab.end() && depthOf(at->second) < depthOf(i))
                r = rowOf[size_t(at->second)];
            else if (const auto c = chipOf.find(need); c != chipOf.end())
                r = int(p.chips[size_t(c->second)].box.y / (kRowStep * s) + 0.5f);
            if (r >= 0 && (!found || r < want)) {
                want = r;
                found = true;
            }
        }
        std::set<int>& rows = taken[depthOf(i)];
        while (rows.count(want)) ++want;
        rows.insert(want);
        rowOf[size_t(i)] = want;
        Node n;
        n.index = i;
        n.col = depthOf(i);
        n.row = want;
        n.box = panel::Box{left + float(n.col) * kColStep * s, (kMargin + float(n.row) * kRowStep) * s, kNodeW * s,
                           kNodeH * s};
        p.nodes.push_back(n);
    }
    std::sort(p.nodes.begin(), p.nodes.end(), [](const Node& a, const Node& b) { return a.index < b.index; });
    for (const Node& n : p.nodes) {
        for (const auto& [need, lv] : t.skills[size_t(n.index)].req) {
            Link l;
            l.to = n.index;
            l.level = lv;
            l.met = levelOf(d, need) >= lv;
            if (const auto at = onTab.find(need); at != onTab.end()) {
                l.from = at->second;
            } else {
                l.from = chipOf[need];
                l.chip = true;
            }
            p.links.push_back(l);
        }
        p.w = std::max(p.w, n.box.x + n.box.w + kMargin * s + kPlus * s / 2);
        p.h = std::max(p.h, n.box.y + n.box.h + kMargin * s);
    }
    for (const Chip& c : p.chips) p.h = std::max(p.h, c.box.y + c.box.h + kMargin * s);
    return p;
}

}  // namespace uagfx::skilltreeview

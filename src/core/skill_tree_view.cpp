#include "core/skill_tree_view.hpp"

#include <algorithm>
#include <functional>
#include <map>

#include "core/skill_window.hpp"

namespace uagfx::skilltreeview {
const skillwin::Skill* find(const skillwin::Data& d, int id) {
    for (const skillwin::Tab& t : d.tabs)
        for (const skillwin::Skill& k : t.skills)
            if (k.id == id) return &k;
    return nullptr;
}

Route route(const Plan& p, const Link& l, float scale) {
    Route r;
    if (l.chip || l.from < 0 || l.to < 0 || size_t(l.from) >= p.nodes.size() || size_t(l.to) >= p.nodes.size())
        return r;
    const float s = scale > 0 ? scale : 1.0f;
    const Node& a = p.nodes[size_t(l.from)];
    const Node& b = p.nodes[size_t(l.to)];
    const float gutter = (kColStep - kNodeW) * s / 2, gap = (kRowStep - kNodeH) * s / 2;
    const float x1 = a.box.x + a.box.w, y1 = a.box.y + a.box.h / 2, y2 = b.box.y + b.box.h / 2;
    const auto add = [&](float x, float y) {
        r.x[r.n] = x;
        r.y[r.n] = y;
        ++r.n;
    };
    add(x1, y1);
    add(x1 + gutter, y1);
    if (b.col == a.col + 1) {  // the next column: down the gutter between them
        add(x1 + gutter, y2);
    } else {  // farther: up or down the gutter to the gap over its row, along it, down the gutter before it
        const float lane = b.box.y - gap;
        add(x1 + gutter, lane);
        add(b.box.x - gutter, lane);
        add(b.box.x - gutter, y2);
    }
    add(b.box.x, y2);
    return r;
}

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
    const float left = kMargin * s;
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
            const auto at = onTab.find(need);
            if (at == onTab.end()) continue;  // a job before's: the side panel lists it
            Link l;
            l.to = n.index;
            l.from = at->second;
            l.level = lv;
            l.met = levelOf(d, need) >= lv;
            p.links.push_back(l);
        }
        p.w = std::max(p.w, n.box.x + n.box.w + kMargin * s + kPlus * s / 2);
        p.h = std::max(p.h, n.box.y + n.box.h + kMargin * s);
    }
    return p;
}

}  // namespace uagfx::skilltreeview

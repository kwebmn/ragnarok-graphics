#include "core/reference_route.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <functional>
#include <queue>
#include <tuple>

namespace uagfx::refroute {
namespace {

constexpr int kWarpReach = 5;   // a warp's area around its cell (most are 1-3 cells each way)
constexpr int kNpcReach = 15;   // talking to an NPC: the server's AREA_SIZE (14) + 1 cells away, walls or not
constexpr int kLandReach = 3;   // a move putting you on a wall: the nearest walkable cell

const walkgrid::Grid* gridOf(const refindex::Index& ix, const std::string& map) {
    if (!ix.walk) return nullptr;
    const auto it = ix.walk->find(map);
    return it == ix.walk->end() ? nullptr : &it->second;
}

// Who costs less: fewer moves with a condition, fewer moves, fewer zeny
using Cost = std::tuple<int, int, long long>;

Cost costOf(const refindex::Move& m) {
    return Cost{m.conditional ? 1 : 0, 1, m.paid && m.price > 0 ? m.price : 0};
}

Cost operator+(const Cost& a, const Cost& b) {
    return Cost{std::get<0>(a) + std::get<0>(b), std::get<1>(a) + std::get<1>(b), std::get<2>(a) + std::get<2>(b)};
}

}  // namespace

std::shared_ptr<const Graph> prepare(const refindex::Index& ix) {
    auto g = std::make_shared<Graph>();
    auto nodesOf = [&](const std::string& map) -> std::pair<uint32_t, uint32_t> {
        const auto it = g->ofMap.find(map);
        if (it != g->ofMap.end()) return it->second;
        const walkgrid::Grid* grid = gridOf(ix, map);
        const uint32_t first = uint32_t(g->nodes.size()), count = grid && grid->partCount > 0 ? grid->partCount : 1;
        for (uint32_t p = 0; p < count; ++p)
            g->nodes.push_back(Graph::Node{map, grid && grid->partCount > 0 ? uint8_t(p) : walkgrid::kWall});
        g->out.resize(g->nodes.size());
        g->ofMap.emplace(map, std::make_pair(first, count));
        return {first, count};
    };
    for (const auto& [map, hints] : ix.maps) {
        if (hints.moves.empty()) continue;
        const walkgrid::Grid* grid = gridOf(ix, map);
        const auto from = nodesOf(map);
        for (const refindex::Move& m : hints.moves) {
            if (m.depends) continue;  // it may lead elsewhere: never a step to follow
            const auto to = nodesOf(m.to);
            const walkgrid::Grid* there = gridOf(ix, m.to);
            uint32_t land = to.first;
            if (there && there->partCount > 0) {
                const std::vector<uint8_t> parts = there->around(m.dx, m.dy, kLandReach);
                if (parts.empty()) continue;  // it puts you in a wall (or past the map's edge): not a way
                land = to.first + parts.front();
            }
            std::vector<uint32_t> starts;
            if (grid && grid->partCount > 0) {
                for (uint8_t p : grid->around(m.x, m.y, m.how == refindex::How::Warp ? kWarpReach : kNpcReach))
                    starts.push_back(from.first + p);
            } else {
                starts.push_back(from.first);
            }
            for (uint32_t s : starts)
                if (s != land) g->out[s].push_back(Graph::Edge{land, &m, map});
        }
    }
    return g;
}

Route find(const refindex::Index& ix, const std::string& from, int x, int y, const std::string& to, bool walk) {
    Route r;
    if (from == to) {
        r.found = true;
        return r;
    }
    if (!ix.graph) return r;
    const Graph& g = *ix.graph;
    const auto start = g.ofMap.find(from), goal = g.ofMap.find(to);
    if (start == g.ofMap.end() || goal == g.ofMap.end()) return r;
    const size_t n = g.nodes.size();
    const Cost none{INT_MAX, INT_MAX, LLONG_MAX};
    std::vector<Cost> best(n, none);
    std::vector<std::pair<uint32_t, const Graph::Edge*>> prev(n, {UINT32_MAX, nullptr});
    using Item = std::pair<Cost, uint32_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    const walkgrid::Grid* grid = gridOf(ix, from);
    std::vector<uint8_t> mine;  // the part the player stands in
    if (grid && grid->partCount > 0 && x >= 0 && y >= 0) mine = grid->around(x, y, kLandReach);
    for (uint32_t i = 0; i < start->second.second; ++i) {
        const uint32_t node = start->second.first + i;
        if (!mine.empty() && g.nodes[node].part != mine.front()) continue;
        best[node] = Cost{0, 0, 0};
        open.push({best[node], node});
    }
    uint32_t reached = UINT32_MAX;
    while (!open.empty()) {
        const auto [cost, node] = open.top();
        open.pop();
        if (cost != best[node]) continue;
        if (g.nodes[node].map == to) {
            reached = node;
            break;
        }
        for (const Graph::Edge& e : g.out[node]) {
            if (walk && e.move->paid) continue;
            const Cost next = cost + costOf(*e.move);
            if (next < best[e.to]) {
                best[e.to] = next;
                prev[e.to] = {node, &e};
                open.push({next, e.to});
            }
        }
    }
    if (reached == UINT32_MAX) return r;
    std::vector<std::pair<uint32_t, const Graph::Edge*>> path;  // the node left and the edge taken
    for (uint32_t at = reached; prev[at].second; at = prev[at].first) path.push_back(prev[at]);
    std::reverse(path.begin(), path.end());
    int ax = x, ay = y;  // where the player is when taking the next move
    for (const auto& [node, edge] : path) {
        const refindex::Move* pick = edge->move;
        if (ax >= 0 && ay >= 0) {  // of the moves alike the nearest
            int nearest = INT_MAX;
            for (const Graph::Edge& e : g.out[node]) {
                const refindex::Move& m = *e.move;
                if (e.to != edge->to || m.paid != pick->paid || m.conditional != pick->conditional ||
                    m.price != pick->price || m.how != pick->how)
                    continue;
                const int d = std::max(std::abs(m.x - ax), std::abs(m.y - ay));
                if (d < nearest) {
                    nearest = d;
                    pick = &m;
                }
            }
        }
        r.steps.push_back(Step{edge->from, *pick});
        ax = pick->dx;
        ay = pick->dy;
        r.paid = r.paid || pick->paid;
        r.kafra = r.kafra || pick->how == refindex::How::Kafra;
        r.conditional = r.conditional || pick->conditional;
        if (pick->paid && pick->price > 0) r.zeny += pick->price;
        if (pick->paid && pick->price < 0) r.priceUnknown = true;
    }
    r.found = true;
    return r;
}

}  // namespace uagfx::refroute

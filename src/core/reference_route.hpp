#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/reference_index.hpp"

namespace uagfx::refroute {

// 3.0.11-beta.8 (the owner: «сделай по максимум»; the concept: «пешком - число переходов, через Kafra - переходы и
// цена, без автохода», both ways to choose): routes from map to map over the pack's moves (refindex::Move). A node
// is a part of a map one walks within (core/walk_grid; a whole map when its cells are not known): a warp or an NPC
// is a way only from the part it stands in or by. Fewest moves with a condition first (a quest, an item - only when
// there is no other way, and marked), then fewest moves, then fewest zeny. No Windows here.

struct Graph {
    struct Node {
        std::string map;
        uint8_t part = walkgrid::kWall;  // kWall: the map's cells are not known, the whole map is one node
    };
    struct Edge {
        uint32_t to = 0;                     // a node
        const refindex::Move* move = nullptr;  // into the index's moves (the graph lives in that index)
        std::string from;                    // the map it leaves
    };
    std::vector<Node> nodes;
    std::vector<std::vector<Edge>> out;                                    // by node
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> ofMap;  // a map's first node and their count
};

// The graph of the index's moves on its maps' parts (ix.walk) - once a pack, the index's own (ix.graph)
std::shared_ptr<const Graph> prepare(const refindex::Index& ix);

struct Step {
    std::string from;
    refindex::Move move;
};
struct Route {
    bool found = false;           // a way (none steps: the same map)
    std::vector<Step> steps;
    long long zeny = 0;           // the known prices summed
    bool priceUnknown = false;    // a paid move with a price that varies
    bool conditional = false, kafra = false, paid = false;
};
// From the player's map (x, y: the player's cell, -1 not known: from any part of it) to the map `to`; `walk`: the
// free moves alone. Of several moves alike (one map to another, one price) the nearest to where the last one put you.
Route find(const refindex::Index& ix, const std::string& from, int x, int y, const std::string& to, bool walk);

}  // namespace uagfx::refroute

#include "core/skill_info.hpp"

#include <algorithm>
#include <iterator>

namespace uagfx::skillinfo {
namespace {

#include "core/skill_info.inc"

const Info* find(int id) {
    const auto at = std::lower_bound(std::begin(kInfo), std::end(kInfo), id,
                                     [](const Info& e, int v) { return int(e.id) < v; });
    return at != std::end(kInfo) && int(at->id) == id ? &*at : nullptr;
}

bool has(int id, uint8_t flag) {
    const Info* e = find(id);
    return e && (e->inf2 & flag);
}

}  // namespace

int type(int id) {
    const Info* e = find(id);
    return e ? int(e->inf) : -1;
}

bool quest(int id) { return has(id, kQuest); }
bool wedding(int id) { return has(id, kWedding); }
bool soulLink(int id) { return has(id, kSoulLink); }

}  // namespace uagfx::skillinfo

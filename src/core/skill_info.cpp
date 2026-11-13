#include "core/skill_info.hpp"

#include <algorithm>
#include <cstring>
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

const char* name(int id) {
    const Info* e = find(id);
    return e && e->name ? e->name : "";
}

const char* aegis(int id) {
    const Info* e = find(id);
    return e ? e->aegis : "";
}

int most(int id) {
    const Info* e = find(id);
    return e ? int(e->most) : 0;
}

int element(int id) {
    const Info* e = find(id);
    return e ? int(e->element) : 0;
}

const char* kind(int id) {
    const Info* e = find(id);
    return e ? e->kind : "";
}

int idOf(const char* aegis) {
    if (!aegis || !*aegis) return 0;
    for (const Info& e : kInfo)
        if (std::strcmp(e.aegis, aegis) == 0) return int(e.id);
    return 0;
}

std::vector<int> all() {
    std::vector<int> out;
    for (const Info& e : kInfo) out.push_back(int(e.id));
    return out;
}

}  // namespace uagfx::skillinfo

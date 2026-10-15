#include "core/gpu_order.hpp"

#include <algorithm>

namespace uagfx {

std::vector<size_t> adapterOrder(const std::vector<AdapterFacts>& adapters, bool byPreference) {
    std::vector<size_t> out;
    for (size_t i = 0; i < adapters.size(); ++i)
        if (!adapters[i].software) out.push_back(i);
    if (!byPreference)
        std::stable_sort(out.begin(), out.end(),
                         [&](size_t a, size_t b) { return adapters[a].vramMb > adapters[b].vramMb; });
    return out;
}

std::string adaptersText(const std::vector<AdapterFacts>& adapters) {
    std::string out;
    for (const AdapterFacts& a : adapters) {
        if (!out.empty()) out += ", ";
        out += a.name + " (" + std::to_string(a.vramMb) + " MB" + (a.software ? ", software" : "") + ")";
    }
    return out;
}

}  // namespace uagfx

#include "core/gpu_order.hpp"

#include <algorithm>
#include <cctype>

namespace uagfx {

namespace {

bool sameName(const std::string& a, const std::string& b) {  // a card's name typed by hand: any case
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

}  // namespace

std::vector<size_t> adapterOrder(const std::vector<AdapterFacts>& adapters, bool byPreference,
                                 const std::string& chosen) {
    std::vector<size_t> out;
    for (size_t i = 0; i < adapters.size(); ++i)
        if (!adapters[i].software) out.push_back(i);
    if (!byPreference)
        std::stable_sort(out.begin(), out.end(),
                         [&](size_t a, size_t b) { return adapters[a].vramMb > adapters[b].vramMb; });
    if (!chosen.empty())
        std::stable_partition(out.begin(), out.end(), [&](size_t i) { return sameName(adapters[i].name, chosen); });
    return out;
}

GpuRow gpuRow(size_t cards, bool chosen) {
    const bool choosable = cards > 1 || chosen;
    return {choosable || cards == 1, choosable};
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

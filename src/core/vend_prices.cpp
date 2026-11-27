#include "core/vend_prices.hpp"

#include <cstdlib>
#include <sstream>

namespace uagfx::vendprices {

void Prices::read(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        char* end = nullptr;
        const long id = std::strtol(line.c_str(), &end, 10);
        if (end != line.c_str() + eq || id <= 0) continue;
        const long long price = std::strtoll(line.c_str() + eq + 1, &end, 10);
        if (price > 0) prices_[int(id)] = price;
    }
}

std::string Prices::text() const {
    std::string out = "# uaRO Graphics: the prices put on things in one's shop, by the thing's number\n";
    for (const auto& [id, price] : prices_) out += std::to_string(id) + "=" + std::to_string(price) + "\n";
    return out;
}

void Prices::remember(int id, long long price) {
    if (id > 0 && price > 0) prices_[id] = price;
}

long long Prices::priceOf(int id) const {
    const auto it = prices_.find(id);
    return it == prices_.end() ? 0 : it->second;
}

}  // namespace uagfx::vendprices

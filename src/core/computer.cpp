#include "core/computer.hpp"

#include <cctype>
#include <cstring>

namespace uagfx {

std::string cpuBrand(const uint32_t regs[12]) {
    char bytes[49] = {};
    std::memcpy(bytes, regs, 48);
    std::string out;
    for (const char* c = bytes; *c; ++c) {
        const bool space = std::isspace(static_cast<unsigned char>(*c)) != 0;
        if (space && (out.empty() || out.back() == ' ')) continue;
        out += space ? ' ' : *c;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string powerPlanName(const std::string& guid) {
    if (guid.empty()) return "";
    std::string g;
    for (const char c : guid) g += char(std::tolower(static_cast<unsigned char>(c)));
    static const char* const kPlans[][2] = {{"381b4222-f694-41f0-9685-ff5bb260df2e", "Balanced"},
                                            {"8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c", "High performance"},
                                            {"a1841308-3541-4fab-bc81-f71556f20b4a", "Power saver"},
                                            {"e9a42b02-d5df-448d-aa00-03f14749eb61", "Ultimate Performance"}};
    for (const auto& p : kPlans)
        if (g == p[0]) return p[1];
    return "other";
}

std::string processorText(const Computer& m) {
    std::string out = m.cpu;
    const auto add = [&out](const std::string& part) { out += (out.empty() ? "" : ", ") + part; };
    if (m.cores > 0) add(std::to_string(m.cores) + (m.cores == 1 ? " core" : " cores"));
    if (m.threads > 0) add(std::to_string(m.threads) + (m.threads == 1 ? " thread" : " threads"));
    if (m.mhz > 0) add(std::to_string(m.mhz) + " MHz");
    return out.empty() ? "unknown" : out;
}

std::string memoryText(const Computer& m) {
    if (!m.ramMb) return "unknown";
    return std::to_string(m.ramMb) + " MB, " + std::to_string(m.freeMb) + " MB free";
}

std::string powerText(const Computer& m) {
    std::string out = m.mains == 1 ? "on the mains" : m.mains == 0 ? "on battery" : "";
    if (!m.plan.empty()) out += (out.empty() ? "plan " : ", plan ") + m.plan;
    return out.empty() ? "unknown" : out;
}

}  // namespace uagfx

#include "core/merc_ai.hpp"

#include <cctype>

namespace uagfx::mercai {
namespace {

bool word(const std::string& t, size_t at, const std::string& w) {
    if (t.compare(at, w.size(), w) != 0) return false;
    const bool before = at == 0 || !(std::isalnum(static_cast<unsigned char>(t[at - 1])) || t[at - 1] == '_');
    const size_t end = at + w.size();
    const bool after = end >= t.size() || !(std::isalnum(static_cast<unsigned char>(t[end])) || t[end] == '_');
    return before && after;
}

size_t skipSpace(const std::string& t, size_t at) {
    while (at < t.size() && std::isspace(static_cast<unsigned char>(t[at]))) ++at;
    return at;
}

}  // namespace

bool needsOurs(const std::string& text) {
    for (size_t i = 0; i < text.size(); ++i) {
        if (word(text, i, "function")) {  // function AI(
            const size_t n = skipSpace(text, i + 8);
            if (word(text, n, "AI") && skipSpace(text, n + 2) < text.size() && text[skipSpace(text, n + 2)] == '(')
                return false;
        } else if (word(text, i, "AI")) {  // AI = function
            const size_t eq = skipSpace(text, i + 2);
            if (eq < text.size() && text[eq] == '=' && word(text, skipSpace(text, eq + 1), "function")) return false;
        }
    }
    return true;
}

std::string text() {
    return "-- uaRO Graphics: the mercenary's AI. The uaRO server leads mercenaries itself (it takes no orders\n"
           "-- from the client), so the game's call each tick has nothing to do. Your own AI may replace this file.\n"
           "function AI(myid)\n"
           "end\n";
}

}  // namespace uagfx::mercai

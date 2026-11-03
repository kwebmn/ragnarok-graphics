#include "core/weight_rules.hpp"

#include <cctype>

namespace uagfx::weightrules {

bool seventy(std::string& text) {
    for (size_t i = 0; i + 2 < text.size(); ++i) {
        if (text[i] != '5' || text[i + 1] != '0') continue;
        if (i > 0 && std::isdigit(static_cast<unsigned char>(text[i - 1]))) continue;  // another number's
        size_t after = i + 2;
        while (after < text.size() && text[after] == ' ') ++after;
        if (after < text.size() && text[after] == '%') {
            text[i] = '7';
            return true;
        }
    }
    return false;
}

}  // namespace uagfx::weightrules

// 3.0.24-beta.20 (docs/superpowers/plans/2026-11-27-item-windows-old-layout.md; R-WP985Q «непонятные символы в
// карточке»): every letter the item windows' markup writes itself - its words and its &#x...; entities - is one the
// interface's font has; crosses, triangles and stars are drawn (core/item_ui fontHas)
#include <cstdlib>
#include <string>

#include "core/item_ui.hpp"
#include "test.hpp"
#include "uikit/equip_live.hpp"
#include "uikit/item_grid_live.hpp"

using namespace uagfx;

namespace {

// The markup with its &#x...; entities as UTF-8 letters
std::string decoded(const std::string& rml) {
    std::string out;
    for (size_t i = 0; i < rml.size(); ++i) {
        if (rml.compare(i, 3, "&#x") == 0) {
            const size_t end = rml.find(';', i);
            if (end != std::string::npos) {
                const uint32_t c = uint32_t(std::strtoul(rml.substr(i + 3, end - i - 3).c_str(), nullptr, 16));
                if (c < 0x80) out += char(c);
                else if (c < 0x800) out += char(0xC0 | c >> 6), out += char(0x80 | (c & 0x3F));
                else out += char(0xE0 | c >> 12), out += char(0x80 | (c >> 6 & 0x3F)), out += char(0x80 | (c & 0x3F));
                i = end;
                continue;
            }
        }
        out += rml[i];
    }
    return out;
}

}  // namespace

TEST(item_markup_letters_are_the_fonts) {
    CHECK(!itemui::fontHas(decoded("<span>&#x2715;</span>")));  // the check sees an entity's letter
    CHECK(itemui::fontHas(decoded(uikit::itemgrid::rml())));
    CHECK(itemui::fontHas(decoded(uikit::equip::rml())));
}

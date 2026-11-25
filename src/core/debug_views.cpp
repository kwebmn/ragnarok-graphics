#include "core/debug_views.hpp"

namespace uagfx::debugviews {

std::string note(const Config& c, settings::Lang lang) {
    std::string names;
    const auto add = [&](settings::Text t) {
        if (!names.empty()) names += ", ";
        names += std::string("«") + settings::tr(lang, t) + "»";
    };
    if (c.seamsView > 0) add(settings::Text::DebugSeams);
    if (c.xray) add(settings::Text::DiagXray);
    if (names.empty()) return {};
    return settings::fmt(settings::tr(lang, settings::Text::DebugViewsOnNote), {names});
}

}  // namespace uagfx::debugviews

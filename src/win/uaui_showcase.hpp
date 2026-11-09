#pragma once

namespace uagfx::win::uaui_showcase {

// 3.0.18-beta.9, a part of its own since 3.0.18-beta.11: uaUI's components in one window for the developers - tabs,
// the bag's items with their icons, a list that scrolls, buttons, switches, a bar ([interface] uaui_showcase, drawn
// while [interface] uaui is on). On the game's thread, between uaui::begin and uaui::end.
void frame(bool on);

}  // namespace uagfx::win::uaui_showcase

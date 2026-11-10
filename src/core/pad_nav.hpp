#pragma once
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::padnav {

// 3.0.18-beta.60 (docs/superpowers/plans/2026-11-10-uaui-possibilities.md, stage 4: «геймпад во всех окнах»): a
// window's clickable boxes walked by the D-pad - the next one the way pressed: ahead of the current box's edge, the
// nearest by its distance along the way plus twice its distance across (RmlUi's own nav rule); none ahead: stays
enum class Dir { Up, Down, Left, Right };
int next(const std::vector<panel::Box>& targets, int from, Dir d);
// The one to start at: the top left (the first by rows, then columns); -1 none
int first(const std::vector<panel::Box>& targets);

}  // namespace uagfx::padnav

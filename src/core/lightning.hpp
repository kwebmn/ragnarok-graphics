#pragma once

namespace uagfx::lightning {

// 0.80: a thunderstorm's flashes (effects.storms, where it rains): now and then - in about half of each 9 seconds -
// the sky flashes twice in quick succession and fades. How bright now, 0-1, at this time of the effects' clock.
float flash(double seconds);

}  // namespace uagfx::lightning

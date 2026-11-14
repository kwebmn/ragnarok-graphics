#include "core/weather3d.hpp"

#include <algorithm>

namespace uagfx::weather3d {

Field fieldOf(int kind, float amount) {
    Field f;
    const float a = std::clamp(amount, 0.0f, 2.0f);
    uint32_t most = 0;
    switch (kind) {
    case 1:  // snow: soft flakes swaying down
        most = 4000, f.side = 70, f.high = 45, f.fall = 6.0f, f.blown = 1.0f, f.half = 0.06f;
        break;
    case 2:  // rain: thin streaks, fast
        most = 6000, f.side = 60, f.high = 40, f.fall = 48.0f, f.blown = 0.5f, f.half = 0.012f, f.streak = 1.1f;
        break;
    case 3:  // leaves: few, tumbling slowly
        most = 320, f.side = 80, f.high = 40, f.fall = 3.2f, f.blown = 1.6f, f.half = 0.16f;
        break;
    case 5:  // pollen: drifting in the air
        most = 700, f.side = 50, f.high = 30, f.fall = 0.15f, f.blown = 0.4f, f.half = 0.025f;
        break;
    case 6:  // dust indoors: no wind
        most = 600, f.side = 40, f.high = 30, f.fall = 0.1f, f.blown = 0.0f, f.half = 0.02f;
        break;
    default:
        return Field{};
    }
    f.count = uint32_t(float(most) * a + 0.5f);
    return f;
}

}  // namespace uagfx::weather3d

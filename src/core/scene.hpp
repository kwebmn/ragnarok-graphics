#pragma once
#include <cstdint>

namespace uagfx {

// The client draws its interface last in every frame, after SetRenderState(ZENABLE, 0) (spec §2, L11851): from
// that call until the next BeginScene every draw is interface (stage 3: sharp filtering of magnified pictures).
class SceneTracker {
public:
    void beginScene() { ui_ = false; }
    void renderState(uint32_t state, uint32_t value);
    bool ui() const { return ui_; }

private:
    bool ui_ = false;
};

}  // namespace uagfx

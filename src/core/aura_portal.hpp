#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::auraportal {

// 3.0.13-beta.15 (the owner's pick 8 of «50 идей»: «Аура не закрывает порталы»): [effects] aura_portals - where a
// warp portal was drawn the frame before (its draws' screen boxes, the game's own vertices: x, y the first two
// floats), a level 99 aura's piece whose middle falls on one drawn at kOverPercent - the portal seen through it
struct Box {
    float x0, y0, x1, y1;
};
constexpr int kOverPercent = 20;

class Portals {
public:
    void frame();  // the frame's boxes become the ones asked about, a new frame's gathered anew
    void portal(const uint8_t* vertices, uint32_t count, uint32_t stride);
    bool covers(const uint8_t* vertices, uint32_t count, uint32_t stride) const;  // the draw's middle on one
    size_t known() const { return last_.size(); }

private:
    std::vector<Box> now_, last_;
};

}  // namespace uagfx::auraportal

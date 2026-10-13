#pragma once
#include <array>
#include <cstdint>

#include "core/d3d7.hpp"

namespace uagfx {

// The device's render and texture stage states as Direct3D 7 numbers, starting from D3D7's defaults.
class DeviceStates {
public:
    DeviceStates();
    void setRender(uint32_t state, uint32_t value);
    uint32_t render(uint32_t state) const;
    void setStage(uint32_t stage, uint32_t type, uint32_t value);
    uint32_t stage(uint32_t stage, uint32_t type) const;

private:
    std::array<uint32_t, d3d7::kRenderStateCount> rs_{};
    std::array<std::array<uint32_t, d3d7::kStageStateCount>, d3d7::kStages> ts_{};
};

// One texture stage, resolved for the pixel shader and the sampler.
struct StageSetup {
    uint8_t colorOp = d3d7::TOP_DISABLE;
    uint8_t colorArg1 = 0;
    uint8_t colorArg2 = 0;
    uint8_t alphaOp = d3d7::TOP_DISABLE;
    uint8_t alphaArg1 = 0;
    uint8_t alphaArg2 = 0;
    uint8_t texCoord = 0;
    uint8_t addressU = 0;
    uint8_t addressV = 0;
    uint8_t magFilter = 0;
    uint8_t minFilter = 0;
    uint8_t mipFilter = 0;
    uint32_t borderColor = 0;
    bool operator==(const StageSetup& o) const;
};

// What one draw needs from the state: the GPU pipeline (depth, cull, blending) and the pixel shader's inputs.
// Parts that are switched off are normalised, so equal-looking draws compare equal. (Not "DrawState": windows.h
// turns that name into DrawStateA in the files that include it.)
struct DrawSetup {
    bool zEnable = false;
    bool zWrite = false;
    uint8_t zFunc = d3d7::CMP_LESSEQUAL;
    uint8_t cull = d3d7::CULL_NONE;
    bool blend = false;
    uint8_t srcBlend = d3d7::BLEND_ONE;
    uint8_t dstBlend = d3d7::BLEND_ZERO;
    bool alphaTest = false;
    uint8_t alphaFunc = d3d7::CMP_ALWAYS;
    uint8_t alphaRef = 0;
    bool fog = false;
    uint32_t fogColor = 0;
    bool colorKey = false;
    StageSetup stage[2];
    bool ui = false;  // an interface draw (SceneTracker): magnified pictures get the [display] ui_filter
    bool operator==(const DrawSetup& o) const;
};

// targetHasAlpha: the client's screen formats have no alpha, so DESTALPHA reads 1 there.
// stageHasTexture: which of stages 0 and 1 have a texture bound.
DrawSetup resolveDrawSetup(const DeviceStates& s, bool targetHasAlpha, const bool stageHasTexture[2]);

}  // namespace uagfx

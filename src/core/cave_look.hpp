#pragma once
#include <cstdint>
#include <string>

namespace uagfx::cave_look {

// 3.0.24-beta.11 (the owner 26.11, the sketches https://claude.ai/artifact/GmhWBkmVYVW61UbkyaJJT9: «Небо в
// разломе», «и сделай пещеру покрасивее», «саму пещеру можно еще с вариантами 1 и 2 сделать»; design
// docs/superpowers/specs/2026-11-26-comodo-rift-design.md): the Comodo town's cave in the 3D view - over its bay a
// rift in the vault with the night sky and fireworks (the legend's «eternal night» with fireworks always seen), pink
// and blue crystals on the vault (the light the town's pink comes from), the grotto: the water's light running over
// the rock and stalactites. The vault's own shader draws them (sky3d_ps vaultSeen)
constexpr uint32_t kRift = 1, kCrystals = 2, kGrotto = 4;
struct Parts {
    bool rift = true, crystals = true, grotto = true;  // [view3d] cave_rift, cave_crystals, cave_grotto
};
struct Look {
    uint32_t parts = 0;
    // the rift's middle and half sizes in the ground's cells (the vault's grid, core cave_vault::Vault)
    int riftX = 0, riftZ = 0, radiusX = 0, radiusZ = 0;
    bool on() const { return parts != 0; }
};
// Comodo's (its bay: comodo.gnd's water under its level 15 fills rows 0-50 along the whole width, 180 x 190 cells);
// none for another map
Look forMap(const std::string& map, const Parts& wanted);
// For the shader, exact in floats: place = x + z * 1024, size = radius x + radius z * 256 + parts * 65536; 0 none
void pack(const Look& look, float& place, float& size);

}  // namespace uagfx::cave_look

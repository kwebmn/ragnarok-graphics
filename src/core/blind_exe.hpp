#pragma once
#include <cstdint>

namespace uagfx::blind_exe {

// 2.6.18: the darkness of blindness (effects 334; 475-484, the darkness's levels): primitive 0x47 drawn by FUN_0060e1a0
// - a square facing the camera round the player (FUN_0060f110 turns it by the camera's angles, FUN_005fb390 takes them
// a frame), its clear middle effect\fullb.tga (half side 50; 140 - 10 a level), round it a frame of four strips of
// effect\white02.bmp near black reaching kGameReach units further (fadds [kGameReachAt] each). From a camera far off
// that frame no longer reaches the sides of a wide screen (R-MCVUC8: "the black square is not whole"); read from
// kWideReachAt instead it reaches kWideReach - the game's own 1000.0 - and covers the screen from as far as the game
// draws anything. It lies in the camera's plane: no strip goes behind the camera however far it reaches.
constexpr uint32_t kRender = 0x60E1A0;     // FUN_0060e1a0
constexpr uint32_t kRenderEnd = 0x60E9E0;  // the next function
constexpr uint32_t kSites[4] = {0x60E5E3, 0x60E6D6, 0x60E7D9, 0x60E8DB};
constexpr uint32_t kGameReachAt = 0x69F5B8;  // .rdata: 50.0
constexpr float kGameReach = 50.0f;
constexpr uint32_t kWideReachAt = 0x695770;  // .rdata: 1000.0
constexpr float kWideReach = 1000.0f;
void original(uint8_t out[6]);  // fadds dword [kGameReachAt]: what this build has at each site
void widened(uint8_t out[6]);   // fadds dword [kWideReachAt]

}  // namespace uagfx::blind_exe

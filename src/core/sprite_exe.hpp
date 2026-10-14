#pragma once
#include <cstdint>

namespace uagfx::sprite_exe {

// AI textures (0.13): uaRO.exe's sprite cache. Every sprite - monsters, players, NPCs, items on the ground - is drawn
// from 256x256 textures cut into cells of one size (32, 64, 128 or 256 a side, by the frame's size); FUN_0040dcb0
// puts a frame into a free or the oldest cell, and its one call of FUN_00417ae0 (CTexture::UpdateSprite, thiscall:
// x, y, the cell's width and height, the image, its palette) writes it: the frame in the cell's top left corner, the
// rest of the cell cleared, through Lock of the whole surface and Unlock.
constexpr uint32_t kWrite = 0x417AE0;
constexpr uint32_t kWriteCall = 0x40E0D8;  // E8 rel32: that call

// Whether these 5 bytes, at kWriteCall, are the expected call of kWrite: else the exe is another build.
bool isWriteCall(const uint8_t bytes[5]);

// 0.16: which sprite a frame is of. CSprRes::Load (FUN_004385f0, thiscall, the file's name) is a virtual method: its
// vtable entry at kLoadSlot holds kLoad. It reads the frames into two lists of SprImg* (std::vector: begin, end):
// the palette ones and the 32-bit ones; FUN_00417ae0 gets one of those SprImg* as its image.
constexpr uint32_t kLoad = 0x4385F0;
constexpr uint32_t kLoadSlot = 0x695A0C;
constexpr uint32_t kImages = 0x514;      // palette frames: begin, then end at +4
constexpr uint32_t kRgbaImages = 0x524;  // 32-bit frames

// What a sprite file shows, by its folder (the names are EUC-KR bytes in data\sprite\): what the AI page's boxes
// choose. Homunculi and mercenaries look like monsters; heads, bodies, headgear, shields, garments and weapons make
// players.
// 0.23: the map torches' two layers (torch.hpp), frames of effect\torch_01 - effects to the AI page.
enum class Source : uint8_t { Unknown, Monster, Player, Npc, Item, Effect, Interface, TorchGlow, TorchFlame };
Source sourceOf(const char* path);
bool isTorch(const char* path);  // effect\torch_01.spr: its 32-bit frame 0 is the glow, the rest the flame
inline bool isEffect(Source s) { return s == Source::Effect || s == Source::TorchGlow || s == Source::TorchFlame; }

}  // namespace uagfx::sprite_exe

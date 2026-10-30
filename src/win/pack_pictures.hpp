#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::win::packpictures {

// 3.0.11-beta.74 (the owner: «иконки скилов, предметов, и всего остального также по возможности грузить в текстур
// которые обработаны»): the AI texture pack's pictures of the game's own interface pictures and textures, for the
// driver's panels - the pack's interface and world pictures' directory read once on a thread of its own (the first ask
// starts it; win/map_hd's minimaps share it). A picture is read and decoded when asked (WebP, 4x the game's).

// 0 while the directory is not read (asked: started); then a number that changes when it is read - pictures made
// before it (the game's own) are made again from the pack
int stamp();
// [interface] pack_icons - the panels' icons from the pack (set each frame, win/context)
void setIconsOn(bool on);
bool iconsOn();
// What an icon made now is made of: -1 the game's own (the switch off), else stamp() - the icon made again when it
// changes
int iconsStamp();

// The pack's picture of a game interface picture (a .bmp's bytes as the game's files have it) by the key its builder
// gave it (core/pack_source interfacePicture; a big power of two one as a texture too) - ARGB, straight alpha. False:
// none in the pack, or its directory not read yet. `name`: the pack's entry (for the log)
bool ofInterface(const uint8_t* bmp, size_t size, std::vector<uint32_t>& argb, int& width, int& height,
                 std::string* name = nullptr);
// The pack's picture of a game texture (a status icon's .tga) by its texture key (core/pack_source worldTexture)
bool ofTexture(const uint8_t* data, size_t size, bool tga, std::vector<uint32_t>& argb, int& width, int& height,
               std::string* name = nullptr);
// 3.0.11-beta.76: the pack's picture of a sprite's frame in its cache cell, by its key (core/pack_source spriteFrames:
// "64x64\0123456789abcdef") - the sprites' entries kept in a table of their own, 24 bytes each (~290 000 of them)
bool ofSprite(const std::string& key, std::vector<uint32_t>& argb, int& width, int& height);
// A worker thread's: waits for the directory (at most `ms`); false - no pack, or not read in time
bool waitRead(unsigned ms);

}  // namespace uagfx::win::packpictures

#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::win::clienttables {

// 3.0.11-beta.41: the client's name tables, read once on the game's thread (the hotbar's, the chat's): a skill's
// name (data\skillnametable.txt «AC_DOUBLE#Double_Strafe#») and an item's number (data\idnum2itemdisplaynametable.txt)
// to their words, an item's number to its icon's file (data\idnum2itemresnametable.txt); the words as the tables have
// them (their «_» kept - hotbar::spaced)
const std::map<std::string, std::string>& skillNames();
const std::map<std::string, std::string>& itemNames();
const std::map<std::string, std::string>& itemFiles();
// 3.0.11-beta.42: an unidentified item's words by its number (data\num2itemdisplaynametable.txt)
const std::map<std::string, std::string>& unknownNames();
// 3.0.11-beta.42: an item's or a skill's icon by its file (data\texture\유저인터페이스\item\<file>.bmp, its magenta
// keyed out) at `side` square, its corners rounded `radius`, premultiplied; read once a file (none: null, logged)
const panel::Picture* icon(const std::string& file, int side, float radius);
// 3.0.11-beta.74 (the owner: «иконки скилов, предметов, и всего остального также по возможности грузить в текстур
// которые обработаны»): an item's or a skill's icon's pixels - the AI texture pack's (4x the game's, its own alpha)
// when it has one and [interface] pack_icons is on (win/pack_pictures), else the game's .bmp with its magenta
// see-through; straight alpha. `from`: where from, or why none (for the log)
bool iconPixels(const std::string& file, std::vector<uint32_t>& argb, int& width, int& height,
                std::string* from = nullptr);
// A client's file as the game finds it (its zips and updates too), else from the game's folder's archives
bool readClientFile(const std::string& entry, std::string& out);

}  // namespace uagfx::win::clienttables

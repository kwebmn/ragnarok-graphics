#pragma once
#include <map>
#include <string>

namespace uagfx::win::clienttables {

// 3.0.11-beta.41: the client's name tables, read once on the game's thread (the hotbar's, the chat's): a skill's
// name (data\skillnametable.txt «AC_DOUBLE#Double_Strafe#») and an item's number (data\idnum2itemdisplaynametable.txt)
// to their words, an item's number to its icon's file (data\idnum2itemresnametable.txt); the words as the tables have
// them (their «_» kept - hotbar::spaced)
const std::map<std::string, std::string>& skillNames();
const std::map<std::string, std::string>& itemNames();
const std::map<std::string, std::string>& itemFiles();
// A client's file as the game finds it (its zips and updates too), else from the game's folder's archives
bool readClientFile(const std::string& entry, std::string& out);

}  // namespace uagfx::win::clienttables

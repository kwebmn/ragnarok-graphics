#pragma once
#include <string>

#include "core/char_status.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::window_title {

// 3.0.3-beta.5 (the user: «надо писать в окне ник - uaroHD», then «уровень проценты и что-то еще»): the game
// window's title, UTF-8 - "Lena · 99/70 · 45,2 % · HP 85 % · SP 40 % · prt_fild08 - uaRO HD": the levels, the
// experience to the next level (the job's once the base is at its last), HP and SP, the weight from 50 % (the game
// stops healing by itself there), the map and the client's name; the client's name alone with no character.
// `status` nullptr: not read (another build of the exe); a number the game has not had is left out.
std::string title(const std::string& name, const charstatus::Status* status, const std::string& map,
                  const std::string& client, settings::Lang lang);

}  // namespace uagfx::window_title

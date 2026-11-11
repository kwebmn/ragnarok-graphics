#pragma once
#include <string>
#include <vector>

#include "core/game_packets.hpp"

namespace uagfx::win::deathrecapwin {

// 3.0.11-beta.277 (the owner's feature 8): the hits on the character as the server tells them (0x008a, 0x01de,
// 0x0114), each with its source's name (a monster's from the reference by its class, else the name the server told,
// 0x0095); at the death the lines for the death screen
void packet(const packets::Event& e);
// «Останні удари · 1 234 шкоди» and the lines, the last first; empty: nothing hit it the last 10 s
std::vector<std::string> lines(std::string& title);
// 3.0.18-beta.120 (the concept «Ігровий інтерфейс»: «Що мене вбило» on its own): the last death's, as the death screen
// showed them (this game's run); false - no death yet
bool last(std::string& title, std::vector<std::string>& lines);

}  // namespace uagfx::win::deathrecapwin

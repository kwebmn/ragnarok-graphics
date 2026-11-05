#pragma once
#include <string>
#include <vector>

#include "core/item_window.hpp"
#include "core/stash_book.hpp"

namespace uagfx::win::stashwatch {

// 3.0.11-beta.286 (the owner's feature 15): the bag, the cart and the storage noted as «Речі», the cart's window and
// «Склад» read them (core/stash_book, uaro-graphics\stash.txt); the search's «Ще» line for those windows - where else
// the things with the words typed lie (the game's own names, any case), the places the window shows left out
void note(stashbook::Kind kind, const std::vector<uagfx::itemwin::Item>& items, unsigned codePage);
// 3.0.11-beta.287 (the owner's feature 14): each thing's line of the site's market in its tip (its cheapest lot)
void prices(std::vector<uagfx::itemwin::Item>& items);
std::string also(const std::string& search, unsigned codePage, const std::vector<stashbook::Kind>& shownHere);

}  // namespace uagfx::win::stashwatch

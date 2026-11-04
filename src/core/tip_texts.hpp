#pragma once
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::tiptexts {

// 3.0.11-beta.219 (the tester: «вікно tips ... треба декомунізувати ... а то зараз українська + російська»): the uaRO
// client's tips of the day (uaRO-HD\tipoftheday.txt, 68 in Russian) in the interface's language - the driver's own
// words in the file's order (weight thresholds as uaRO's: 70 %), the game's list told by its count and its first tip
constexpr int kCount = 68;
bool isUaroList(int count, const std::string& firstTip);  // `firstTip` in any code page
std::string tip(int number, settings::Lang l);           // from 0; "" past them (UTF-8, lines by \n)

}  // namespace uagfx::tiptexts

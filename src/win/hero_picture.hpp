#pragma once
#include <cstdint>
#include <string>

#include "core/window_panel.hpp"

namespace uagfx::win::heropicture {

// 3.0.18-beta.111 (the owner: «из спрайтов, настоящего», «еще есть анманут типа пеко, фалкона»): the character as
// the game's own equipment window draws it (uaRO.exe FUN_0046fd40): its body by the job it shows (FUN_0063b080:
// riding - the Peco's job, 7 -> 13 ...), its head in its hair colour, its three headgears; besides the original, a
// falcon on its shoulder and its cart behind it when it has them. The session's: job 0x771F8C, sex, head 0x7718F4,
// headgears 0x771900 (low), 0x771904 (top), 0x771908 (middle), hair colour 0x771910, options 0x77191C (0x20 riding,
// 0x10 falcon, carts).
struct Looks {
    int job = 0, sex = 0, head = 0, colour = 0, low = 0, top = 0, middle = 0;
    int cloth = 0;  // 3.0.18-beta.126: the clothes' dye (0x77190C - the body's palette, FUN_006399e0)
    uint32_t options = 0;
    bool operator==(const Looks& o) const;
};
Looks own();  // the player's character now (the game's session)
int shownJob(int job, uint32_t options);  // FUN_0063b080's: the riding one's
// The standing frame facing `direction` (0 down, 1 down-left ... 7 down-right: the game's) at the whole step that
// covers w x h (sharp pixels; the caller shows it fitted into the box), premultiplied. `from` what it was made of,
// or why not (for the log)
bool make(const Looks& l, int direction, int w, int h, panel::Picture& out, std::string* from = nullptr);

}  // namespace uagfx::win::heropicture

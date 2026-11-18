#pragma once
#include <string>

// 3.0.22-beta.2 (the owner: «когда нажимаю на нпс, оно какбуто подлагивает ... какуюто прогрузку сделать еще когда
// грузится»): the game's slow archive reads kept by map (core/read_ahead, uaro-graphics\cache\read-ahead.txt) and read
// ahead on a thread of its own at the lowest priority - the login screens' and the common ones at the start, a map's
// two seconds into it - so the game finds them in Windows' cache, not on the disk. Each file once a game.
namespace uagfx::win::readaheadwin {

// Each frame, the game's thread: `map` the map now ("" the login screens); its slow reads noted, the file written a
// minute after a change, a map's read ahead begun
void frame(const std::string& map);

}  // namespace uagfx::win::readaheadwin

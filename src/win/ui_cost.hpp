#pragma once
#include <string>

namespace uagfx::win::uicost {

// 3.0.11-beta.55 (R-9PP79T: «подтормаживает графика с новым интерфейсом»): what painting the new interface's pictures
// costs the game's thread - each part's paintings (its shapes and its words, while what it shows changes) counted and
// timed, told with the frames every 30 s; a long one (over 8 ms) told at once (its part, its size). The game's thread.
class Paint {
public:
    Paint(const char* part, int width, int height);
    ~Paint();
    Paint(const Paint&) = delete;
    Paint& operator=(const Paint&) = delete;

private:
    const char* part_;
    long long start_;
    int width_, height_;
};
// "char block 14 x 2.3 ms (4.1 at most), chat 3 x 5.0 ms (7.2 at most) - 61 ms in all" since the last; "" none
std::string take();

}  // namespace uagfx::win::uicost

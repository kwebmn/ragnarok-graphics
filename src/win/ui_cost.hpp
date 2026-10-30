#pragma once
#include <cstddef>
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
    double words_;  // 3.0.11-beta.57: the words' time so far, at its start
    int width_, height_;
};
// 3.0.11-beta.57 (R-T942DZ: the minimap 15.6 ms a painting, the character block 13-19 - their shapes 2-3 of it):
// drawWords timed apart - each part's paintings tell the words' share; and how many words GDI drew anew, how many
// were kept from before
class Words {
public:
    Words();
    ~Words();
    Words(const Words&) = delete;
    Words& operator=(const Words&) = delete;
    void counted(int fresh, int kept, size_t keptBytes);

private:
    long long start_;
    int fresh_ = 0, kept_ = 0;
    size_t bytes_ = 0;
};
// "char block 14 x 2.3 ms (4.1 at most, words 0.3), chat 3 x 5.0 ms (7.2 at most, words 1.1) - 61 ms in all; words:
// 12 drawn anew, 830 kept (0.4 MB)" since the last; "" none
std::string take();

}  // namespace uagfx::win::uicost

#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/settings/strings.hpp"
#include "core/window_panel.hpp"

namespace uagfx::keyhint {

// 3.0.21-beta.13 (the owner: «надо еще подсказку сделать что мышка работает при нажатии на альт. вообще у нас с
// подобными подсказками проблема. надо пройтись и в таких моментах наделать подсказки, так как люди не разберутся как
// пользоваться»): the moments the driver changes what the keys and the mouse do - a line at the bottom middle (the
// controller's hint line's glass, the keys in caps) says what to press there. The moments in the order they win when
// two come together: the one where a player is stuck first. No Windows here.
// 3.0.22-beta.24 (the owner 19.11 «делай 1-4»): Flight - the free camera with its points set (Ctrl+Shift+P, O, L),
// TimeLapse - Ctrl+Shift+T's time running or stopped
enum class Moment : uint8_t { View3dWindow, View3dSkill, Flight, FreeCamera, TimeLapse, View3d, ItemPicks, Count };
constexpr int kMoments = int(Moment::Count);
const char* keyOf(Moment m);  // "view3d_window", "view3d_skill", "free_flight", "free_camera_points", "time_lapse",
                              // "view3d", "item_moves"

// What the game is in this frame
struct Now {
    bool view3d = false;      // the 3D view on
    bool behind = false;      // from behind (else from the eyes)
    bool looking = false;     // the mouse turns the view (no Alt held, the game's window in front)
    bool windowOpen = false;  // a window of the interface open
    bool altHeld = false;
    bool freeCamera = false;  // Ctrl+Shift+C
    int skillWait = 0;        // the game waits for a skill's target: 2 an enemy's, 4 a friend's; 0 none
    bool itemsShared = false; // the storage or the cart open (Alt+right click; their picks: Ctrl, Shift, Ctrl+A)
    int flightPoints = 0;     // 3.0.22-beta.24: the free camera's flight points set
    bool flying = false;      // and its flight on
    int lapse = 0;            // Ctrl+Shift+T: 0 off, 1 the time running, 2 stopped
};
std::array<bool, kMoments> momentsOf(const Now& n);

struct Part {
    std::string keys;   // «Alt», «Ctrl+Shift+V», «W A S D» - each key a cap (capsOf)
    std::string words;  // what it does
};
// A moment's line in `l`
std::vector<Part> partsOf(Moment m, const Now& n, settings::Lang l);

// How many times each has shown and which the player has shown he knows - kept between the games
constexpr int kTimes = 5;
struct Seen {
    std::array<int, kMoments> times{};
    std::array<bool, kMoments> learned{};
};
Seen read(const std::string& text);
std::string write(const Seen& s);

// A moment's line at most this long an episode (0: while the moment lasts - a window open in the 3D view, a skill
// waiting); counted once it has shown kCountMs
constexpr uint32_t kCountMs = 1000;
uint32_t capOf(Moment m);

// Each frame: the moments now, the ones the player has just done what they say (learned at once); the moment whose
// line shows (Count: none). An episode is a moment from its start to its end; its line shows the first kTimes
// episodes, its time spent only while it is the one shown; `changed`: `seen` changed (to be kept)
class Shower {
public:
    Moment frame(uint32_t nowMs, const std::array<bool, kMoments>& active, const std::array<bool, kMoments>& done,
                 Seen& seen, bool& changed);

private:
    struct Episode {
        bool on = false, counted = false, over = false;
        uint32_t shownMs = 0;
    };
    std::array<Episode, kMoments> episodes_{};
    bool started_ = false;
    uint32_t last_ = 0;
};

// The line's look: the glass of the controller's hint line, each part its keys in caps, then its words
std::vector<std::string> capsOf(const std::string& keys);
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // in the target
    panel::Box glass;                // in its own pixels
    struct Placed {
        std::vector<panel::Box> caps;
        panel::Box words;
    };
    std::vector<Placed> parts;
};
// Its bottom middle at (centreX, bottom)
Layout layout(const std::vector<Part>& parts, int centreX, int bottom, float scale);
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;  // a cap's key
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Part>& parts);
constexpr uint32_t kCapFill = 0x2A3140, kCapEdge = 0xFFFFFF;  // a cap's ground, its edge (at 0.28)

}  // namespace uagfx::keyhint

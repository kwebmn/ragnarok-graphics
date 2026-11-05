#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::hudlayout {

// 3.0.11-beta.75 (the owner: «некоторые интерфейсы хотелось бы чтобы можно было бы двигать и настраивать»): the new
// interface's parts placed by the player in «Розміщення інтерфейсу» (win/hud_edit) - each moved to a place of its own,
// kept to the screen's nearest corner, edge's middle or centre (a new resolution or size keeps it there), and sized
// 60-160 %; a part not moved stands where it always does (some by others: the buffs under the minimap). The file:
// uaro-graphics\hud-layout.txt, a line a part.
enum class Part : uint8_t { Character, Minimap, Buffs, Tracker, Hotbar, Target, Party, Chat, Hunt, kCount };  // .278
constexpr int kParts = int(Part::kCount);
const char* keyOf(Part p);  // its word in the file: "character", "minimap", ...

constexpr float kLeastSize = 0.6f, kMostSize = 1.6f, kSizeStep = 0.05f;

struct Place {
    bool moved = false;
    int anchor = 0;        // the screen's point it keeps to: its column (0 left, 1 middle, 2 right) + 3 x its row
    // 3.0.11-beta.261 (the tester: «бафи все ще ростуть від центра вверх та вниз і залазять на мапу»): 9-11 - the
    // screen's middle row, the part's TOP kept to it (it grows down only); placeOf gives these for the middle row
    float dx = 0, dy = 0;  // the part's own matching point from it, in the game's pixels
    float size = 1;        // kLeastSize..kMostSize
};
using Layout = std::array<Place, kParts>;

// "minimap 2 -12.5 8.0 1.10" - a part's word, its anchor (-1 not moved), dx, dy, size; any other line skipped
Layout parse(const std::string& text);
std::string text(const Layout& layout);  // the parts moved or sized alone

struct Box {
    int x = 0, y = 0, w = 0, h = 0;
};
// A part's top left in the picture (k its pixels a game pixel): its own (x, y) unless moved; kept on the screen
void apply(const Place& p, float k, int targetW, int targetH, int w, int h, int& x, int& y);
// The place of a part at `b` now: kept to the screen's ninth its middle is in
Place placeOf(const Box& b, float k, int targetW, int targetH, float size);
// A box being dragged snapped - its edges or middle to the screen's edges or middle, or its edges to the others'
// edges, nearer than `within` pixels; the lines it snapped to (-1 none) for guides
struct Snapped {
    int x = 0, y = 0;
    int guideX = -1, guideY = -1;
};
Snapped snap(const Box& moving, const std::vector<Box>& others, int targetW, int targetH, int within);
float stepped(float size, int steps);  // the wheel's: by kSizeStep, kept within
// 3.0.11-beta.77 (the owner: «некоторые окна налазят одно на другое и мешают»): a part let go over others - moved the
// least way out of them all (`gap` apart), kept on the screen; as it is when clear, or when no place is free
Box clearOf(const Box& moving, const std::vector<Box>& others, int targetW, int targetH, int gap);
// 3.0.11-beta.147 (the owner: «иногда панель сама двигается»; R-VYV3AG 10:01:13 - a click on the hotbar whose pointer
// slipped 6 game pixels in 148 ms dragged the whole bar away): a press held on a part drags it only once held kHoldMs
// and moved kMoveGame game pixels (`movedPx` the target's, k of them a game pixel); a quicker one stays the part's
// click
constexpr float kMoveGame = 6;
constexpr uint64_t kHoldMs = 300;
bool dragStarts(float movedPx, float k, uint64_t heldMs);

}  // namespace uagfx::hudlayout

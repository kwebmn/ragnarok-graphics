#include "core/skill_area.hpp"

namespace uagfx::skillarea {
namespace {

constexpr uint16_t kSafetyWall = 12, kFireWall = 18, kThunderStorm = 21, kPneuma = 25, kWarp = 27;
constexpr uint16_t kSanctuary = 70, kMagnus = 79, kFirePillar = 80, kMeteor = 83, kVermilion = 85, kIceWall = 87;
constexpr uint16_t kStormGust = 89, kHeavenDrive = 91, kQuagmire = 92, kTalkieBox = 125, kVenomDust = 140;
constexpr uint16_t kVolcano = 285, kDeluge = 286, kGale = 287, kLandProtector = 288, kSnap = 264;

void square(std::vector<Cell>& out, Cell at, int r) {
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) out.push_back(Cell{at.x + dx, at.y + dy});
}

void layout(std::vector<Cell>& out, Cell at, const int* dx, const int* dy, int n) {
    for (int i = 0; i < n; ++i) out.push_back(Cell{at.x + dx[i], at.y + dy[i]});
}

bool trap(uint16_t skill) { return (skill >= 115 && skill <= 123) || skill == kTalkieBox; }

}  // namespace

int dirOf(int dx, int dy) {
    if (dx >= 0 && dy >= 0) return dx * 2 - 1 < dy ? 0 : dx > dy * 2 ? 6 : 7;  // upper right
    if (dx >= 0 && dy <= 0) return dx * 2 - 1 < -dy ? 4 : dx > -dy * 2 ? 6 : 5;  // lower right
    if (dx <= 0 && dy <= 0) return dx * 2 + 1 > dy ? 4 : dx < dy * 2 ? 2 : 3;  // lower left
    return -dx * 2 - 1 < dy ? 0 : -dx > dy * 2 ? 2 : 1;                         // upper left
}

Cell step(int dir) {
    static const Cell kSteps[8] = {{0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}};
    return kSteps[((dir % 8) + 8) % 8];
}

std::vector<Cell> cells(uint16_t skill, int lv, Cell caster, Cell at) {
    std::vector<Cell> out;
    // skill_get_unit_layout: the caster's own cell counts as the side 2 (left)
    const int dir = caster == at ? 2 : dirOf(at.x - caster.x, at.y - caster.y);
    switch (skill) {
        case kFireWall:
            if (dir & 1) {
                static const int dxA[] = {1, 1, 0, 0, -1}, dyA[] = {1, 0, 0, -1, -1};
                static const int dxB[] = {-1, -1, 0, 0, 1}, dyB[] = {1, 0, 0, -1, -1};
                layout(out, at, dir & 2 ? dxB : dxA, dir & 2 ? dyB : dyA, 5);
            } else {
                static const int dxA[] = {-1, 0, 1}, dyA[] = {0, 0, 0}, dxB[] = {0, 0, 0}, dyB[] = {-1, 0, 1};
                layout(out, at, dir % 4 == 0 ? dxA : dxB, dir % 4 == 0 ? dyA : dyB, 3);
            }
            break;
        case kIceWall:
            if (dir & 1) {
                static const int dxA[] = {2, 1, 0, -1, -2}, dyA[] = {2, 1, 0, -1, -2};
                static const int dxB[] = {-2, -1, 0, 1, 2}, dyB[] = {2, 1, 0, -1, -2};
                layout(out, at, dir & 2 ? dxB : dxA, dir & 2 ? dyB : dyA, 5);
            } else {
                static const int dxA[] = {-2, -1, 0, 1, 2}, dyA[] = {0, 0, 0, 0, 0};
                static const int dxB[] = {0, 0, 0, 0, 0}, dyB[] = {-2, -1, 0, 1, 2};
                layout(out, at, dir % 4 == 0 ? dxA : dxB, dir % 4 == 0 ? dyA : dyB, 5);
            }
            break;
        case kSanctuary: {
            static const int dx[] = {-1, 0, 1, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2, -2, -1, 0, 1, 2, -1, 0, 1};
            static const int dy[] = {-2, -2, -2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2};
            layout(out, at, dx, dy, 21);
            break;
        }
        case kMagnus: {
            static const int dx[] = {-1, 0, 1, -1, 0, 1, -3, -2, -1, 0, 1, 2, 3, -3, -2, -1, 0,
                                     1,  2, 3, -3, -2, -1, 0, 1, 2, 3, -1, 0, 1, -1, 0, 1};
            static const int dy[] = {-3, -3, -3, -2, -2, -2, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0,
                                     0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2, 3, 3, 3};
            layout(out, at, dx, dy, 33);
            break;
        }
        case kVenomDust: {
            static const int dx[] = {-1, 0, 0, 0, 1}, dy[] = {0, -1, 0, 1, 0};
            layout(out, at, dx, dy, 5);
            break;
        }
        case kPneuma: square(out, at, 1); break;
        case kQuagmire: case kThunderStorm: case kHeavenDrive: square(out, at, 2); break;
        case kMeteor: case kVolcano: case kDeluge: case kGale: square(out, at, 3); break;
        case kStormGust: square(out, at, 5); break;
        case kVermilion: square(out, at, 6); break;
        case kLandProtector: square(out, at, lv <= 2 ? 3 : lv <= 4 ? 4 : 5); break;
        case kSafetyWall: case kWarp: case kFirePillar: case kSnap: out.push_back(at); break;
        default:
            if (trap(skill)) out.push_back(at);
            break;
    }
    return out;
}

Kind kindOf(uint16_t skill) {
    switch (skill) {
        case kFireWall: case kIceWall: return Kind::Wall;
        case kThunderStorm: case kMagnus: case kMeteor: case kVermilion: case kStormGust: case kHeavenDrive:
            return Kind::Crowd;
        case kSafetyWall: case kPneuma: case kSanctuary: return Kind::Friend;
        case kFirePillar: return Kind::Ahead;
        default: return trap(skill) ? Kind::Trap : Kind::Other;
    }
}

int crowdRadius(uint16_t skill) {
    switch (skill) {
        case kThunderStorm: case kHeavenDrive: return 2;
        case kMagnus: case kMeteor: return 3;
        case kStormGust: return 5;
        case kVermilion: return 6;
        default: return 0;
    }
}

int most(uint16_t skill) {
    switch (skill) {
        case kFireWall: case kQuagmire: return 3;
        case kFirePillar: return 5;
        default: return 0;
    }
}

}  // namespace uagfx::skillarea

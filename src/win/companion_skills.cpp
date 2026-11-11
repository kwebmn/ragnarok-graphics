#include "win/companion_skills.hpp"

#include <windows.h>

#include "win/game_camera.hpp"
#include "win/game_memory.hpp"

namespace uagfx::win::companionwin {
namespace {

constexpr uint32_t kHomunList = 0x771438, kMercList = 0x771444, kSession = 0x771148;
constexpr uint32_t kHomunUses = kSession + 0x574, kMercUses = kSession + 0x584;
constexpr int kHomunFirst = 8000, kMercFirst = 0x2008, kMostUses = 256;
constexpr uint32_t kModeNow = 0x74B57C, kModeMsgSlot = 0x14, kModeMsg = 0x5707D0;
constexpr int kUseMsg = 0x7E, kHomunDrag = 0xCB, kMercDrag = 0x10B, kDropMsg = 0x12;
constexpr size_t kMostSkills = 40;
using ModeMsgFn = int(__attribute__((thiscall)) *)(uint32_t mode, int msg, int a, int b, int c);

bool modeMsg(int msg, int a, int b, int c) {
    const uint32_t mode = read32(kModeNow);
    if (!mode || !inGameWorld() || read32(read32(mode) + kModeMsgSlot) != kModeMsg) return false;
    reinterpret_cast<ModeMsgFn>(uintptr_t(kModeMsg))(mode, msg, a, b, c);
    return true;
}

// Where its use level is kept (0: none - the array not made, or the id out of it)
uint32_t useAt(Who w, int id) {
    const int i = id - (w == Who::Homun ? kHomunFirst : kMercFirst);
    const uint32_t array = read32(w == Who::Homun ? kHomunUses : kMercUses);
    return i >= 0 && i < kMostUses && array ? array + uint32_t(i) * 4 : 0;
}

}  // namespace

std::vector<GameSkill> read(Who w) {
    std::vector<GameSkill> out;
    const uint32_t head = read32(w == Who::Homun ? kHomunList : kMercList);
    if (!head) return out;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostSkills; ++n, node = read32(node)) {
        int32_t r[7] = {};
        if (!readGame(node + 8, r, sizeof r)) break;
        out.push_back(GameSkill{node, r[1], r[3], r[4], (r[5] & 0xFF) != 0});
    }
    return out;
}

int storedUse(Who w, int id) {
    const uint32_t at = useAt(w, id);
    return at ? int32_t(read32(at)) : 0;
}

bool setUse(Who w, int id, int level) {
    const uint32_t at = useAt(w, id);
    return at && write32(at, uint32_t(level));
}

bool use(Who w, const GameSkill& k, int level) {
    (void)w;
    return k.node && level > 0 && modeMsg(kUseMsg, int(k.node + 8), level, 0);
}

const char* drop(Who w, const GameSkill& k, int level) {
    if (!k.node || level <= 0) return "not dropped (not a skill to use)";
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy)) return "not dropped (no cursor)";
    if (!modeMsg(w == Who::Homun ? kHomunDrag : kMercDrag, k.id, int(k.node + 0x24), level))
        return "not dropped (the game mode not found)";
    modeMsg(kDropMsg, cx, cy, 0);
    return "let go through the game's own drop";
}

}  // namespace uagfx::win::companionwin

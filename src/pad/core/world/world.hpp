#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "pad/core/proto/events.hpp"

namespace uapad {

struct Actor {
    uint32_t id = 0;
    uint16_t cls = 0;
    uint16_t hair = 0;
    uint16_t speed = 150;  // ms per cell
    Cell from;
    Cell to;
    uint32_t moveStartMs = 0;
    uint16_t opt1 = 0;  // its state as the server last showed it (the pad's own target mark aside)
    uint16_t opt2 = 0;
    uint32_t option = 0;
};

struct GroundItem {
    uint32_t id = 0;
    uint16_t item = 0;
    Cell pos;
};

constexpr int kHotkeySlots = 27;
// "F3" for the first row, "F3 of row 2" for the others (for the log).
inline std::string hotkeyName(int slot) {
    const std::string key = "F" + std::to_string(slot % 9 + 1);
    return slot < 9 ? key : key + " of row " + std::to_string(slot / 9 + 1);
}
constexpr int kClientBarSlots = kHotkeySlots;  // all three rows of F1-F9

// A hotkey slot as the client itself keeps it. The uaRO client never sends its bar (no 0x02b9 / 0x02ba),
// so the pad reads it from the client's memory.
struct ClientBarSlot {
    enum class Kind { Empty, Skill, Item };
    Kind kind = Kind::Empty;
    std::string name;        // Skill: the skill_db name ("AC_DOUBLE"); Item: the client's item name
    uint16_t level = 0;      // Skill: the level on the bar, 0 = as learned
    uint16_t itemIndex = 0;  // Item: inventory index of an identified item of that name, 0 = none left
};
inline bool operator==(const ClientBarSlot& a, const ClientBarSlot& b) {
    return a.kind == b.kind && a.name == b.name && a.level == b.level && a.itemIndex == b.itemIndex;
}

// What the client knows about the map, rebuilt from the map server's packets.
class WorldState {
public:
    void reset(uint32_t selfId);  // new map server connection
    void apply(const Event& ev, uint32_t nowMs);
    void setHotkey(int slot, const Hotkey& key);

    uint32_t selfId() const { return selfId_; }
    bool inGame() const { return inGame_; }
    Cell selfPos(uint32_t nowMs) const;
    bool selfMoving(uint32_t nowMs) const { return selfPos(nowMs) != selfTo_; }
    Cell selfDest() const { return selfTo_; }  // where the server is walking us (our cell when standing)
    bool sitting() const { return sitting_; }
    // Our last step on this map as a vector (destination - start), nothing before the first one.
    std::optional<Cell> lastMove() const { return lastMove_; }
    bool dead() const { return dead_; }

    const std::unordered_map<uint32_t, Actor>& actors() const { return actors_; }
    std::optional<Cell> actorPos(uint32_t id, uint32_t nowMs) const;
    bool isMonster(const Actor& a) const;
    // 0.7: an NPC one can talk to - a sprite of the NPC range, not hidden; NPCs dressed as players come as 0x022a,
    // which the pad does not read, and those dressed as monsters look like monsters.
    bool isNpc(const Actor& a) const;
    std::optional<uint32_t> lastHitMs(uint32_t target) const;  // last time *we* hit `target`

    const std::unordered_map<uint32_t, GroundItem>& groundItems() const { return items_; }
    std::optional<InvEntry> findUsable(uint16_t itemId) const;
    std::optional<SkillInfo> skill(uint16_t id) const;
    std::optional<SkillInfo> skillByName(const std::string& name) const;
    const Hotkey& hotkey(int slot) const;
    // Why a slot from the client's bar could not be used (an unknown skill, no item left); empty otherwise.
    const std::string& hotkeyNote(int slot) const;
    void setClientBar(const std::array<ClientBarSlot, kClientBarSlots>& bar);
    // What each slot of the client's bar holds (skill or item name), empty when unknown: for the menu.
    std::array<std::string, kHotkeySlots> clientBarNames() const;
    // False until the server sends 0x02b9; uAthena only does with HOTKEY_SAVING enabled.
    bool hotkeysKnown() const { return hotkeysKnown_; }

private:
    static Cell interpolate(Cell from, Cell to, uint16_t speed, uint32_t startMs, uint32_t nowMs);
    void clearMap();

    uint32_t selfId_ = 0;
    bool inGame_ = false;
    Cell selfFrom_;
    Cell selfTo_;
    uint32_t selfMoveStartMs_ = 0;
    uint16_t selfSpeed_ = 150;  // ms per cell, from 0x00b0 SP_SPEED
    std::optional<Cell> lastMove_;
    bool sitting_ = false;
    bool dead_ = false;
    uint32_t ownPet_ = 0;
    std::unordered_map<uint32_t, Actor> actors_;
    std::unordered_map<uint32_t, uint32_t> lastHit_;
    std::unordered_map<uint32_t, GroundItem> items_;
    std::unordered_map<uint16_t, InvEntry> inventory_;  // by protocol index
    std::unordered_map<uint16_t, SkillInfo> skills_;
    std::array<Hotkey, kHotkeySlots> hotkeys_{};
    std::array<std::string, kHotkeySlots> hotkeyNotes_{};
    std::array<ClientBarSlot, kClientBarSlots> clientBar_{};
    bool hotkeysKnown_ = false;
};

}  // namespace uapad

#include "pad/core/world/world.hpp"

#include <type_traits>

namespace uapad {

namespace {
constexpr uint16_t kDefaultSpeed = 150;  // DEFAULT_WALK_SPEED in uAthena
}  // namespace

void WorldState::reset(uint32_t selfId) {
    *this = WorldState();
    selfId_ = selfId;
}

void WorldState::clearMap() {
    actors_.clear();
    lastHit_.clear();
    items_.clear();
    lastMove_.reset();
    sitting_ = false;
    dead_ = false;
}

Cell WorldState::interpolate(Cell from, Cell to, uint16_t speed, uint32_t startMs, uint32_t nowMs) {
    const int steps = cellDistance(from, to);
    if (steps == 0) return to;
    const uint32_t elapsed = nowMs - startMs;
    const uint32_t total = uint32_t(steps) * (speed ? speed : kDefaultSpeed);
    if (elapsed >= total) return to;
    const int done = int(uint64_t(elapsed) * uint64_t(steps) / total);
    return Cell{from.x + (to.x - from.x) * done / steps, from.y + (to.y - from.y) * done / steps};
}

Cell WorldState::selfPos(uint32_t nowMs) const {
    return interpolate(selfFrom_, selfTo_, selfSpeed_, selfMoveStartMs_, nowMs);
}

std::optional<Cell> WorldState::actorPos(uint32_t id, uint32_t nowMs) const {
    auto it = actors_.find(id);
    if (it == actors_.end()) return std::nullopt;
    const Actor& a = it->second;
    return interpolate(a.from, a.to, a.speed, a.moveStartMs, nowMs);
}

bool WorldState::isNpc(const Actor& a) const {
    // 45 a warp, 111 and 139 the invisible NPC and warp; option: hiding, cloaking, invisible
    const bool sprite = a.cls >= 46 && a.cls <= 999 && a.cls != 111 && a.cls != 139;
    return sprite && a.id != selfId_ && (a.option & 0x46) == 0;
}

bool WorldState::isMonster(const Actor& a) const {
    // Mob db ids; pets carry battle_config.pet_hair_style (100) in the hair field.
    return a.cls >= 1001 && a.cls <= 3999 && a.hair == 0 && a.id != ownPet_;
}

std::optional<uint32_t> WorldState::lastHitMs(uint32_t target) const {
    auto it = lastHit_.find(target);
    if (it == lastHit_.end()) return std::nullopt;
    return it->second;
}

std::optional<InvEntry> WorldState::findUsable(uint16_t itemId) const {
    std::optional<InvEntry> best;
    for (const auto& entry : inventory_) {
        const InvEntry& e = entry.second;
        if (e.item != itemId || e.amount == 0 || e.equip) continue;
        if (!best || e.index < best->index) best = e;
    }
    return best;
}

std::optional<SkillInfo> WorldState::skill(uint16_t id) const {
    auto it = skills_.find(id);
    if (it == skills_.end()) return std::nullopt;
    return it->second;
}

std::optional<SkillInfo> WorldState::skillByName(const std::string& name) const {
    for (const auto& entry : skills_)
        if (!name.empty() && entry.second.name == name) return entry.second;
    return std::nullopt;
}

const std::string& WorldState::hotkeyNote(int slot) const {
    static const std::string kNone;
    if (slot < 0 || slot >= kHotkeySlots) return kNone;
    return hotkeyNotes_[size_t(slot)];
}

std::array<std::string, kHotkeySlots> WorldState::clientBarNames() const {
    std::array<std::string, kHotkeySlots> names{};
    for (size_t i = 0; i < clientBar_.size() && i < names.size(); ++i) names[i] = clientBar_[i].name;
    return names;
}

void WorldState::setClientBar(const std::array<ClientBarSlot, kClientBarSlots>& bar) {
    hotkeysKnown_ = true;
    clientBar_ = bar;
    for (size_t i = 0; i < bar.size(); ++i) {
        const ClientBarSlot& slot = bar[i];
        Hotkey key;
        std::string note;
        if (slot.kind == ClientBarSlot::Kind::Skill) {
            if (auto sk = skillByName(slot.name))
                key = Hotkey{1, sk->id, slot.level};
            else
                note = "skill " + slot.name + " from the bar is not in our skill list";
        } else if (slot.kind == ClientBarSlot::Kind::Item) {
            auto it = inventory_.find(slot.itemIndex);
            if (slot.itemIndex != 0 && it != inventory_.end())
                key = Hotkey{0, it->second.item, 0};
            else
                note = "item " + slot.name + " from the bar is not in the inventory";
        }
        hotkeys_[i] = key;
        hotkeyNotes_[i] = note;
    }
}

const Hotkey& WorldState::hotkey(int slot) const {
    static const Hotkey kEmpty{};
    if (slot < 0 || slot >= kHotkeySlots) return kEmpty;
    return hotkeys_[size_t(slot)];
}

void WorldState::setHotkey(int slot, const Hotkey& key) {
    if (slot >= 0 && slot < kHotkeySlots) hotkeys_[size_t(slot)] = key;
}

void WorldState::apply(const Event& ev, uint32_t nowMs) {
    std::visit(
        [&](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, EvAuthOk> || std::is_same_v<T, EvMapChange>) {
                clearMap();
                inGame_ = true;
                selfFrom_ = selfTo_ = e.pos;
            } else if constexpr (std::is_same_v<T, EvSelfWalk>) {
                if (!(e.to == e.from)) lastMove_ = Cell{e.to.x - e.from.x, e.to.y - e.from.y};
                selfFrom_ = e.from;
                selfTo_ = e.to;
                selfMoveStartMs_ = nowMs;
            } else if constexpr (std::is_same_v<T, EvSetPos>) {
                if (e.id == selfId_) {
                    selfFrom_ = selfTo_ = e.pos;
                } else if (auto it = actors_.find(e.id); it != actors_.end()) {
                    it->second.from = it->second.to = e.pos;
                }
            } else if constexpr (std::is_same_v<T, EvVanish>) {
                if (e.id == selfId_) {
                    if (e.type == 1) dead_ = true;
                } else {
                    actors_.erase(e.id);
                    lastHit_.erase(e.id);
                }
            } else if constexpr (std::is_same_v<T, EvActor>) {
                if (e.id == selfId_) return;
                Actor a;
                a.id = e.id;
                a.cls = e.cls;
                a.hair = e.hair;
                a.speed = e.speed ? e.speed : kDefaultSpeed;
                a.from = e.from;
                a.to = e.to;
                a.moveStartMs = nowMs;
                a.opt1 = e.opt1;
                a.opt2 = e.opt2;
                a.option = e.option;
                actors_[e.id] = a;
            } else if constexpr (std::is_same_v<T, EvActorMove>) {
                if (auto it = actors_.find(e.id); it != actors_.end()) {
                    it->second.from = e.from;
                    it->second.to = e.to;
                    it->second.moveStartMs = nowMs;
                }
            } else if constexpr (std::is_same_v<T, EvActorOption>) {
                if (auto it = actors_.find(e.id); it != actors_.end()) {
                    it->second.opt1 = e.opt1;
                    it->second.opt2 = e.opt2;
                    it->second.option = e.option;
                }
            } else if constexpr (std::is_same_v<T, EvPetInfo>) {
                if (e.type == 0) ownPet_ = e.id;
            } else if constexpr (std::is_same_v<T, EvAction>) {
                if (e.src != selfId_) return;
                if (e.type == 2 || e.type == 3) {
                    sitting_ = e.type == 2;
                } else if (e.dst != 0) {
                    lastHit_[e.dst] = nowMs;
                }
            } else if constexpr (std::is_same_v<T, EvSkillHit>) {
                if (e.src == selfId_ && e.dst != selfId_) lastHit_[e.dst] = nowMs;
            } else if constexpr (std::is_same_v<T, EvGroundItem>) {
                items_[e.id] = GroundItem{e.id, e.item, e.pos};
            } else if constexpr (std::is_same_v<T, EvGroundItemGone>) {
                items_.erase(e.id);
            } else if constexpr (std::is_same_v<T, EvInventory>) {
                for (auto it = inventory_.begin(); it != inventory_.end();) {
                    if (!it->second.equip)
                        it = inventory_.erase(it);
                    else
                        ++it;
                }
                for (const InvEntry& item : e.items) inventory_[item.index] = item;
            } else if constexpr (std::is_same_v<T, EvItemAdd>) {
                auto it = inventory_.find(e.entry.index);
                if (it != inventory_.end() && it->second.item == e.entry.item)
                    it->second.amount = uint16_t(it->second.amount + e.entry.amount);
                else
                    inventory_[e.entry.index] = e.entry;
            } else if constexpr (std::is_same_v<T, EvItemRemove>) {
                if (auto it = inventory_.find(e.index); it != inventory_.end()) {
                    it->second.amount = it->second.amount > e.amount ? uint16_t(it->second.amount - e.amount) : 0;
                    if (it->second.amount == 0) inventory_.erase(it);
                }
            } else if constexpr (std::is_same_v<T, EvItemUsed>) {
                if ((e.user != 0 && e.user != selfId_) || !e.ok) return;
                if (auto it = inventory_.find(e.index); it != inventory_.end()) {
                    it->second.amount = e.remaining;
                    if (e.remaining == 0) inventory_.erase(it);
                }
            } else if constexpr (std::is_same_v<T, EvSkills>) {
                skills_.clear();
                for (const SkillInfo& s : e.skills) skills_[s.id] = s;
            } else if constexpr (std::is_same_v<T, EvSkillUp>) {
                SkillInfo& s = skills_[e.id];
                s.id = e.id;
                s.lv = e.lv;
                s.range = e.range;
            } else if constexpr (std::is_same_v<T, EvHotkeys>) {
                hotkeys_ = e.keys;
                hotkeysKnown_ = true;
            } else if constexpr (std::is_same_v<T, EvSelfSpeed>) {
                selfFrom_ = selfPos(nowMs);  // re-anchor the current walk at the old speed
                selfMoveStartMs_ = nowMs;
                selfSpeed_ = e.speed ? e.speed : kDefaultSpeed;
            } else if constexpr (std::is_same_v<T, EvResurrect>) {
                if (e.id == selfId_) dead_ = false;
            }
        },
        ev);
}

}  // namespace uapad

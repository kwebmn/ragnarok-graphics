#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "pad/core/i18n.hpp"
#include "pad/core/logic/pad.hpp"

namespace uapad {

// What a gamepad button does.
struct Action {
    // Reference (3.0.11-beta.22): uaRO Graphics' «Довідник» window opened or closed (win/reference_window)
    enum class Kind : uint8_t { None, Attack, Slot, Pickup, SitStand, ClearTarget, CameraToggle, Talk, Reference };
    Kind kind = Kind::None;
    uint8_t slot = 0;  // Kind::Slot: hotkey index 0-26 (row slot / 9 + 1, key F(slot % 9 + 1))
};
inline bool operator==(const Action& a, const Action& b) {
    return a.kind == b.kind && (a.kind != Action::Kind::Slot || a.slot == b.slot);
}
inline bool operator!=(const Action& a, const Action& b) { return !(a == b); }

constexpr size_t kBindableCount = 14;
// The buttons the player may rebind, in menu order. Start (menu), Back (pad on/off) and the sticks are fixed.
constexpr std::array<uint16_t, kBindableCount> kBindableButtons = {
    kPadA, kPadB, kPadX, kPadY, kPadLB, kPadRB, kPadLT, kPadRT, kPadUp, kPadRight, kPadDown, kPadLeft, kPadL3, kPadR3};
using Bindings = std::array<Action, kBindableCount>;  // by position in kBindableButtons

Bindings defaultBindings();  // the 0.2 hunter layout
// The action after (dir > 0) or before `a` in the menu's ring: none, attack, slots 1-27, pickup, sit, untarget,
// camera (the right stick: targets <-> camera), talk (0.7: to the nearest NPC), reference (3.0.11-beta.22: the
// «Довідник» window).
Action nextAction(Action a, int dir);

const char* buttonToken(size_t i);  // in the file: "A", "LB", "UP"...
const char* buttonLabel(size_t i);  // in the menu (UTF-8): "A", "D-pad ↑"...
void useSonyLabels(bool on);        // 0.6: "Cross", "L1"... while a Sony controller is the one read
std::string actionToken(Action a);  // in the file: "attack", "slot 12"...
std::optional<Action> parseAction(const std::string& token);
std::string actionLabel(Action a, Lang lang);  // in the menu (UTF-8): "Attack", "Slot F3, row 2"...

// Every character's layout, as kept in uaro-pad-binds.ini.
struct Profiles {
    struct Character {
        std::string name;  // as the server spells it, for people reading the file
        Bindings bindings{};
    };
    Bindings fallback = defaultBindings();  // [default]: characters without a section of their own
    std::map<uint32_t, Character> characters;  // [char <id>]
    // Layouts saved before 0.5 (a file without [file] version 2) that had R3 clearing the target: R3 now
    // switches the right stick to the camera, and an unused L3 clears the target, as the 0.5 defaults do.
    int movedToCamera = 0;
    // 0.7: layouts saved before it (file version < 3) with D-pad down on its default slot F9 and no talk anywhere
    // get talk there, as the 0.7 defaults have it.
    int movedToTalk = 0;
    Bindings forCharacter(uint32_t charId) const;
};

// Missing keys fall back to [default], then to defaultBindings(); unknown lines are skipped. Layouts from
// before 0.5 are moved to the camera on R3 (see Profiles::movedToCamera).
Profiles parseProfiles(const std::string& text);
// `onDisk` with one layout put in (charId 0: [default]) and every other one left as it is, so that two
// clients saving different characters do not erase each other's layouts.
Profiles withLayout(Profiles onDisk, uint32_t charId, const std::string& name, const Bindings& bindings);
std::string formatProfiles(const Profiles& profiles);

}  // namespace uapad

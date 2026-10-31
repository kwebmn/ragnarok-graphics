#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "pad/core/bytes.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/core/config/config.hpp"
#include "pad/core/logic/camera.hpp"
#include "pad/core/logic/pad.hpp"
#include "pad/core/logic/slots.hpp"
#include "pad/core/world/world.hpp"

namespace uapad {

// 0.7: the NPC window the client shows, read from its memory: the text window with its button (Next, Close or none
// while the NPC is still talking), the menu to choose from, the number field.
enum class NpcDialog : uint8_t { None, Next, Close, Waiting, Menu, Number };

// What the pad asks of an NPC window, done through the client's own buttons.
enum class DialogCommand : uint8_t { Next, Close, MenuUp, MenuDown, MenuOk, MenuCancel, NumberOk };

struct ViewInfo {
    bool focused = false;
    int width = 0;  // client area, px
    int height = 0;
    bool mouseClicked = false;  // the player pressed the real left mouse button since the last tick
    bool paused = false;        // the settings menu is open: the pad belongs to it
    NpcDialog dialog = NpcDialog::None;  // 0.7: an NPC window is open: the buttons answer it
};

struct VirtualMouse {
    bool active = false;  // false: the real mouse passes through untouched
    int x = 0;            // client-area coordinates
    int y = 0;
    bool lmb = false;
};

// What the right stick asks of the camera this tick, in camera mode.
struct CameraMove {
    float turnDeg = 0;  // add to the camera's turn (the way the right mouse button drags it)
    int zoomSteps = 0;  // wheel notches: > 0 closer, < 0 further
};

struct TickOutput {
    VirtualMouse mouse;
    CameraMove camera;
    std::vector<Bytes> packets;  // requests for the map server, in order
    uint32_t mark = 0;           // a newly chosen monster to flag with "!" for the player; 0: none
    Rumble rumble = Rumble::None;
    std::vector<std::string> notes;  // decisions worth a log line
    std::vector<DialogCommand> dialog;  // 0.7: for the NPC window, in order
    std::optional<int> number;          // 0.7: the number to put into the NPC's number field
    bool reference = false;             // 3.0.11-beta.22: «Довідник» opened or closed (win/reference_window)
};

// The left stick walks, the right stick picks a monster, Back switches the pad off; every other button does
// what its Bindings say (by default the 0.2 hunter layout).
class Controller {
public:
    explicit Controller(const Config& cfg);
    TickOutput tick(const PadState& pad, const WorldState& world, const ViewInfo& view, uint32_t nowMs);
    // The client asked to walk to `dest` (0x00a7). While the pad holds the walking point steady this
    // shows which way the camera is turned.
    void onClientWalk(Cell dest, const WorldState& world, uint32_t nowMs);
    // The client asked for a skill itself (0x0072 / 0x0113, e.g. from the keyboard): the server ends our
    // attack for it, so ask for the attack again.
    void onClientSkill(uint32_t nowMs);
    // A map was loaded. The client may have set its camera back, and one walk teaches the turn again.
    void onMapLoaded() { camera_.reset(); }

    void setBindings(const Bindings& bindings);
    const Bindings& bindings() const { return bindings_; }
    // 3.0.11-beta.85 (the concept's «Выбор цели навыка»): the party's members on this map, in the party panel's order
    // (partywin::hereIds). A support skill pressed with any of them in sight asks whom: the D-pad goes round them and
    // the character, A casts on the one chosen, the skill's own button again on the character, B or Start cancels
    void setFriends(std::vector<uint32_t> ids) { friends_ = std::move(ids); }
    uint32_t choosing() const { return choosing_ ? choosing_->who[choosing_->at] : 0; }  // the one it would go on
    uint16_t choosingButton() const;  // the skill's own button (again: on the character); 0 while none asks

    bool enabled() const { return enabled_; }
    bool walking() const { return walking_; }  // the left stick holds the virtual mouse
    uint32_t target() const { return target_; }
    // 3.0.11-beta.87 (the concept's stage 3): the monster a light tilt of the right stick points at - where a push
    // would take the aim (a dashed ring under it); 0 none
    uint32_t preview() const { return preview_; }
    bool attacking() const { return wantAttack_; }
    bool approaching() const { return approach_.has_value(); }
    bool cameraMode() const { return cameraMode_; }  // the right stick turns the camera instead of picking
    NpcDialog dialog() const { return dialog_; }      // 0.7: the NPC window the pad answered last tick
    void setWalkRadius(float fraction) { cfg_.walkRadius = fraction; }  // [stick] walk_radius, from the menu
    // The client's own camera turn (radians, its own count) when it can be read from its memory; nothing
    // otherwise. Once the walk samples show how it maps to the screen, the directions follow it at once.
    void setClientTurn(std::optional<double> radians) {
        if (radians)
            camera_.setClientTurn(*radians);
        else
            camera_.clearClientTurn();
    }
    const CameraYaw& camera() const { return camera_; }

private:
    struct Choosing {  // 3.0.11-beta.85
        int slot = -1;
        std::vector<uint32_t> who;  // the members in sight, the character last
        size_t at = 0;
        uint32_t sinceMs = 0;
    };
    struct Approach {
        bool pickup = false;  // floor item pickup, otherwise a skill
        uint32_t id = 0;      // floor item id or actor id
        bool ground = false;
        uint16_t skill = 0;
        uint16_t lv = 0;
        int range = 1;
        int slot = -1;  // the hotkey slot that asked for the skill
        uint32_t startMs = 0;
        std::optional<uint32_t> lastStepMs;
    };

    uint16_t readButtons(const PadState& pad);
    void walk(const PadState& pad, const WorldState& world, const ViewInfo& view, uint32_t now, TickOutput& out);
    void aim(const PadState& pad, const WorldState& world, uint32_t now, TickOutput& out);
    void handleButtons(uint16_t held, uint16_t down, const WorldState& world, uint32_t now, TickOutput& out);
    void fire(const Action& a, bool repeat, const WorldState& world, uint32_t now, TickOutput& out);
    bool ownsApproach(const Action& a) const;
    bool attackHeld() const;
    void attack(const WorldState& world, uint32_t now, TickOutput& out);
    void useSlot(int slot, bool repeat, const WorldState& world, uint32_t now, TickOutput& out);
    // What a slot's decision does: sent, a walk up, or denied (`nearest`: the monster decideSlot was offered)
    void act(SlotDecision d, int slot, bool repeat, const Hotkey& key, const WorldState& world, uint32_t nearest,
             uint32_t now, TickOutput& out);
    void choose(uint16_t down, const WorldState& world, uint32_t now, TickOutput& out);
    void pickup(bool repeat, const WorldState& world, uint32_t now, TickOutput& out);
    void sendTake(uint32_t item, uint32_t now, TickOutput& out);
    void clearTarget(const WorldState& world, uint32_t now, TickOutput& out);
    void stopAttacking();
    void attackAgainSoon(uint32_t now);
    void stepApproach(const WorldState& world, uint32_t now, TickOutput& out);
    void maintainTarget(bool attackHeld, const WorldState& world, uint32_t now, TickOutput& out);
    uint32_t nearestTarget(const WorldState& world, uint32_t now) const;
    // The target a press goes for as it is: alive, and being fought or picked with the right stick. Any
    // other remembered target gives way to the nearest monster.
    bool keepsTarget(const WorldState& world) const;
    // Takes the nearest monster as the target, telling the log what the pad saw; 0 when there is none.
    uint32_t takeNearest(const WorldState& world, uint32_t now, const char* why, TickOutput& out);
    void dropFarTarget(const WorldState& world, uint32_t now, TickOutput& out);
    void toggleCamera(TickOutput& out);
    void talk(const WorldState& world, uint32_t now, TickOutput& out);
    void answerDialog(NpcDialog d, uint16_t held, uint16_t down, uint32_t now, TickOutput& out);
    bool dialogRepeat(uint16_t button, uint16_t held, uint16_t down, uint32_t now);
    void steerCamera(const PadState& pad, uint32_t now, TickOutput& out);
    void setTarget(uint32_t id, uint32_t now, TickOutput& out);
    bool targetAlive(const WorldState& world) const;
    void deny(TickOutput& out, const std::string& why);

    Config cfg_;
    Bindings bindings_ = defaultBindings();
    bool enabled_ = true;
    uint16_t prevButtons_ = 0;
    uint16_t armed_ = 0;  // held buttons whose press we saw while active: only these repeat
    bool ltHeld_ = false;
    bool rtHeld_ = false;
    std::array<uint32_t, 16> repeatAtMs_{};  // by button bit: when a held button fires again
    bool walking_ = false;
    int ringX_ = 0;  // the walking point's offset from the centre, px
    int ringY_ = 0;
    double ringAngle_ = 0;
    std::optional<uint32_t> ringSteadyFromMs_;  // walk requests from then on tell the camera's turn
    std::optional<Cell> stickDest_;  // the client's latest walk request while the stick walks
    bool paused_ = false;  // the settings menu had the pad last tick
    bool aiming_ = false;
    bool aimLatched_ = false;  // R3 dropped the target: no aiming until the stick comes back
    uint32_t target_ = 0;
    uint32_t preview_ = 0;  // 3.0.11-beta.87
    bool picked_ = false;      // target_ was chosen with the right stick: presses keep it
    bool cameraMode_ = false;
    bool loggedCalibration_ = false;
    std::optional<uint32_t> cameraMs_;  // the last camera tick, for the turn and zoom rates
    float zoomCarry_ = 0;               // notches building up between ticks
    bool wantAttack_ = false;  // we want the server to be attacking target_
    bool attackSent_ = false;  // an attack request for target_ is in effect
    std::optional<uint32_t> engagedSinceMs_;  // our first attack request on target_: the give-up clock
    std::optional<uint32_t> lastAttackMs_;
    std::array<std::optional<uint32_t>, kHotkeySlots> lastSlotMs_{};
    std::optional<uint32_t> lastPickupMs_;
    std::unordered_map<uint32_t, uint32_t> pickupTriedMs_;  // floor item id -> our first takeitem for it
    std::unordered_map<uint32_t, uint32_t> blacklist_;  // actor id -> blacklisted until (ms)
    std::unordered_map<uint32_t, uint32_t> markedMs_;   // actor id -> when we last showed its "!"
    std::optional<Approach> approach_;
    std::vector<uint32_t> friends_;     // 3.0.11-beta.85
    std::optional<Choosing> choosing_;  // a support skill waits for whom
    NpcDialog dialog_ = NpcDialog::None;
    int number_ = 0;                       // what the pad put into the number field
    std::optional<uint32_t> bHeldSinceMs_;  // B held in a window without a button: closes it after a second
    double talkDirX_ = 0;                  // the right stick's direction on the map when it points (talk)
    double talkDirY_ = 0;
    CameraYaw camera_;
};

}  // namespace uapad

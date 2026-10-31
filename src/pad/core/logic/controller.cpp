#include "pad/core/logic/controller.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "pad/core/logic/slots.hpp"
#include "pad/core/logic/targeting.hpp"
#include "pad/core/proto/c2s.hpp"

namespace uapad {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kTriggerOn = 0.5f;
constexpr float kTriggerOff = 0.3f;
constexpr double kAimOn = 0.6;
constexpr double kAimOff = 0.35;
constexpr double kAimPreview = 0.3;  // 3.0.11-beta.87: a lighter tilt than kAimOn shows the next target
constexpr double kAimSwitchMargin = 8.0;  // aim score points a new monster must win by
constexpr uint32_t kSlotIntervalMs = 150;
constexpr uint32_t kChooseMs = 8000;  // 3.0.11-beta.85: a support skill waits this long for whom
// 3.0.11-beta.88: walking without the mouse - 6 cells along the stick in half-cell steps; another way asked for 150 ms
// on, the same again every 350 ms (the server walks to a cell and stops)
constexpr float kCameraDeadZone = 0.2f;
constexpr float kCameraTurnDegPerSec = 120.0f;   // the right stick fully pushed
constexpr float kCameraZoomStepsPerSec = 6.0f;   // wheel notches a second, fully pushed
constexpr uint32_t kCameraMaxDtMs = 100;         // a stalled frame does not turn the camera in one jump
constexpr uint32_t kApproachStepMs = 250;
constexpr uint32_t kApproachTimeoutMs = 5000;
constexpr uint32_t kRingSettleMs = 150;
constexpr double kRingTurn = 10.0 * kPi / 180.0;
constexpr uint32_t kMarkAgainMs = 1000;
constexpr uint32_t kReattackMs = 1000;  // standing still without a hit of ours this long: ask again
constexpr uint32_t kPickupGiveUpMs = 1000;  // still on the ground a second after we asked: not ours to take
constexpr int kStopAhead = 2;
constexpr int kPickupRadius = 10;
constexpr int kPickupReach = 1;
constexpr uint32_t kDialogRepeatFirstMs = 400;  // a held D-pad in an NPC window: first repeat, then every
constexpr uint32_t kDialogRepeatMs = 120;
constexpr uint32_t kDialogForceCloseMs = 1000;  // B held this long closes a window the NPC left without a button
constexpr int kNumberMax = 99999999;

bool due(const std::optional<uint32_t>& last, uint32_t now, uint32_t interval) {
    return !last || now - *last >= interval;
}

// Where to stop a walk in progress. Our estimate trails the server by a step or two by the time a stop
// arrives, and a stop behind the server walks us back: stop two cells on, never past where we were going.
Cell stopCell(Cell here, Cell heading) {
    if (cellDistance(here, heading) <= kStopAhead) return heading;
    return stepsToward(here, Cell{heading.x - here.x, heading.y - here.y}, kStopAhead);
}

size_t bitIndex(uint16_t button) {
    size_t i = 0;
    while (i < 15 && !(button & (1u << i))) ++i;
    return i;
}

}  // namespace

Controller::Controller(const Config& cfg) : cfg_(cfg), enabled_(cfg.enabled) {}

void Controller::setBindings(const Bindings& bindings) {
    bindings_ = bindings;
    armed_ = 0;  // a button held right now may mean something else: it acts again on its next press
}

bool Controller::attackHeld() const {
    for (size_t i = 0; i < kBindableButtons.size(); ++i)
        if (bindings_[i].kind == Action::Kind::Attack && (armed_ & kBindableButtons[i])) return true;
    return false;
}

void Controller::deny(TickOutput& out, const std::string& why) {
    if (out.rumble == Rumble::None) out.rumble = Rumble::Denied;
    out.notes.push_back("denied: " + why);
}

bool Controller::targetAlive(const WorldState& world) const {
    return target_ != 0 && world.actors().count(target_) != 0;
}

bool Controller::keepsTarget(const WorldState& world) const {
    const bool walkingUp = approach_ && !approach_->pickup && approach_->id == target_;
    return targetAlive(world) && (wantAttack_ || picked_ || walkingUp);
}

namespace {

std::string cellsText(const Cell& a, const Cell& b) {
    char text[16];
    std::snprintf(text, sizeof text, "%.1f", std::hypot(double(a.x - b.x), double(a.y - b.y)));
    return text;
}

}  // namespace

uint32_t Controller::takeNearest(const WorldState& world, uint32_t now, const char* why, TickOutput& out) {
    const std::vector<uint32_t> c = targetCandidates(world, now, blacklist_);
    if (c.empty()) return 0;
    const Cell me = world.selfPos(now);
    std::string note = std::string(why) + ": nearest of " + std::to_string(c.size()) + " within reach:";
    for (size_t i = 0; i < c.size() && i < 3; ++i) {
        const std::optional<Cell> pos = world.actorPos(c[i], now);
        if (!pos) continue;
        note += std::string(i == 0 ? " " : ", then ") + std::to_string(c[i]) + " at " + cellText(*pos) + " " +
                cellsText(*pos, me);
    }
    out.notes.push_back(note + "; we are at " + cellText(me));
    setTarget(c.front(), now, out);
    return c.front();
}

uint32_t Controller::nearestTarget(const WorldState& world, uint32_t now) const {
    auto c = targetCandidates(world, now, blacklist_);
    return c.empty() ? 0 : c.front();
}

void Controller::stopAttacking() {
    wantAttack_ = false;
    attackSent_ = false;
    engagedSinceMs_.reset();
}

// Every skill but a combo ends the server's attack (uAthena unit_skilluse_id2 / _pos2): ask for it again.
void Controller::attackAgainSoon(uint32_t now) {
    if (!wantAttack_ || target_ == 0) return;
    attackSent_ = false;
    lastAttackMs_ = now;  // the request waits attack_interval_ms after the skill
}

void Controller::onClientSkill(uint32_t now) { attackAgainSoon(now); }

void Controller::setTarget(uint32_t id, uint32_t now, TickOutput& out) {
    if (id == target_) return;
    target_ = id;
    picked_ = false;
    attackSent_ = false;
    engagedSinceMs_.reset();
    if (approach_ && !approach_->pickup && approach_->id != id) approach_.reset();  // a skill for the old one
    if (id == 0) return;
    if (out.rumble == Rumble::None) out.rumble = Rumble::TargetSwitch;
    out.notes.push_back("target " + std::to_string(id));
    for (auto it = markedMs_.begin(); it != markedMs_.end();) {
        if (now - it->second >= kMarkAgainMs)
            it = markedMs_.erase(it);
        else
            ++it;
    }
    if (markedMs_.count(id) == 0) {  // its "!" from a moment ago would still be showing
        markedMs_[id] = now;
        out.mark = id;
    }
}

uint16_t Controller::readButtons(const PadState& pad) {
    if (!pad.connected) {
        ltHeld_ = rtHeld_ = false;
        return 0;
    }
    ltHeld_ = pad.lt > (ltHeld_ ? kTriggerOff : kTriggerOn);
    rtHeld_ = pad.rt > (rtHeld_ ? kTriggerOff : kTriggerOn);
    uint16_t b = uint16_t(pad.buttons & ~(kPadLT | kPadRT));
    if (ltHeld_) b = uint16_t(b | kPadLT);
    if (rtHeld_) b = uint16_t(b | kPadRT);
    return b;
}

TickOutput Controller::tick(const PadState& pad, const WorldState& world, const ViewInfo& view, uint32_t now) {
    TickOutput out;
    preview_ = 0;  // 3.0.11-beta.87: aim() tells it anew
    const uint16_t buttons = readButtons(pad);
    const uint16_t down = uint16_t(buttons & ~prevButtons_);
    prevButtons_ = buttons;

    if (choosing_ && (view.paused || view.dialog != NpcDialog::None || !enabled_ || !pad.connected || !view.focused)) {
        out.notes.push_back(hotkeyName(choosing_->slot) + ": the choice cancelled");  // 3.0.11-beta.85: a window came
        choosing_.reset();
    }
    if (view.paused) {  // the settings menu has the pad: the character stands, buttons only get remembered
        if (!paused_) {
            paused_ = true;
            if (wantAttack_ || attackSent_) out.packets.push_back(buildStopAttack());
            stopAttacking();
            if (!walking_ && (approach_ || world.selfMoving(now)))  // a walk up to something: stop it too
                out.packets.push_back(buildWalk(stopCell(world.selfPos(now), world.selfDest())));
        }
        PadState still = pad;
        still.lx = still.ly = 0;
        walk(still, world, view, now, out);  // the stick walk stops as usual
        out.mouse = VirtualMouse{};
        armed_ = 0;
        aiming_ = false;
        approach_.reset();
        return out;
    }
    paused_ = false;
    if (down & kPadBack) {
        enabled_ = !enabled_;
        out.rumble = enabled_ ? Rumble::PadOn : Rumble::PadOff;
        out.notes.push_back(enabled_ ? "pad on" : "pad off");
        if (!enabled_) stopAttacking();
    }
    if (!enabled_ || !pad.connected || !view.focused || !world.inGame() || world.dead()) {
        walking_ = false;
        ringSteadyFromMs_.reset();
        aiming_ = false;
        armed_ = 0;  // buttons pressed meanwhile do nothing until pressed again
        approach_.reset();
        dialog_ = NpcDialog::None;  // the pad does not answer an NPC window now
        return out;
    }
    if (view.dialog != NpcDialog::None) {  // 0.7: an NPC window: the character stands, the buttons answer it
        if (dialog_ == NpcDialog::None) {
            if (wantAttack_ || attackSent_) out.packets.push_back(buildStopAttack());
            stopAttacking();
            approach_.reset();
            aiming_ = false;
            out.notes.push_back("NPC window: the buttons answer it");
        }
        if (view.dialog == NpcDialog::Number && dialog_ != NpcDialog::Number) number_ = 0;  // the client's "0"
        dialog_ = view.dialog;
        PadState still = pad;
        still.lx = still.ly = 0;
        walk(still, world, view, now, out);
        answerDialog(view.dialog, buttons, down, now, out);
        armed_ = 0;  // a button still held when the window closes does nothing until pressed again
        return out;
    }
    if (dialog_ != NpcDialog::None) out.notes.push_back("NPC window closed");
    dialog_ = NpcDialog::None;
    bHeldSinceMs_.reset();
    if (view.mouseClicked && (wantAttack_ || approach_)) {  // the player took over with the mouse
        stopAttacking();
        approach_.reset();
        out.notes.push_back("mouse click: the pad stops its attack");
    }
    dropFarTarget(world, now, out);
    if (camera_.calibrated() && !loggedCalibration_) {
        loggedCalibration_ = true;
        char text[96];
        std::snprintf(text, sizeof text, "camera: follows the client's turn (sign %d, offset %.0f degrees)",
                      camera_.calibratedSign(), camera_.calibratedOffset() * 180.0 / kPi);
        out.notes.push_back(text);
    }
    walk(pad, world, view, now, out);
    for (size_t i = 0; i < kBindableButtons.size(); ++i)  // before the stick: this tick's push goes to the new mode
        if ((down & kBindableButtons[i]) && bindings_[i].kind == Action::Kind::CameraToggle) toggleCamera(out);
    if (cameraMode_)
        steerCamera(pad, now, out);
    else
        aim(pad, world, now, out);
    talkDirX_ = talkDirY_ = 0;  // talk: the NPC the right stick points at, or the nearest
    if (!cameraMode_ && std::hypot(double(pad.rx), double(pad.ry)) > kAimOn)
        camera_.toWorld(pad.rx, -pad.ry, talkDirX_, talkDirY_);
    if (choosing_) {  // 3.0.11-beta.85: the buttons choose whom the support skill goes on; the sticks go on
        choose(down, world, now, out);
        armed_ = 0;  // nothing held then repeats or attacks
    } else {
        handleButtons(buttons, down, world, now, out);
    }
    stepApproach(world, now, out);
    maintainTarget(attackHeld(), world, now, out);
    return out;
}

void Controller::onClientWalk(Cell dest, const WorldState& world, uint32_t now) {
    if (directWalk_) return;  // 3.0.11-beta.88: the pad walks without the client's cursor: nothing to learn
    if (walking_) stickDest_ = dest;
    if (!walking_ || !ringSteadyFromMs_ || int32_t(now - *ringSteadyFromMs_) < 0) return;
    const Cell me = world.selfPos(now);
    camera_.addSample(ringX_, ringY_, dest.x - me.x, dest.y - me.y);
}

void Controller::walk(const PadState& pad, const WorldState& world, const ViewInfo& view, uint32_t now,
                      TickOutput& out) {
    const float mag = std::sqrt(pad.lx * pad.lx + pad.ly * pad.ly);
    if (directWalk_) {  // 3.0.11-beta.88: no mouse
        walkDirect(pad, mag, world, now, out);
        return;
    }
    if (mag > cfg_.deadzone && view.width > 0 && view.height > 0) {
        const bool starting = !walking_;
        if (starting) {
            walking_ = true;
            approach_.reset();
            stopAttacking();  // walking ends a fight; a held A starts it again afterwards
            ringSteadyFromMs_.reset();
            stickDest_.reset();
        }
        const float radius = cfg_.walkRadius * float(std::min(view.width, view.height));
        const int cx = view.width / 2;
        const int cy = view.height / 2;
        out.mouse.active = true;
        out.mouse.lmb = true;
        if (starting && cfg_.press == PressMode::Center) {
            out.mouse.x = cx;
            out.mouse.y = cy;
            return;
        }
        ringX_ = int(std::lround(pad.lx / mag * radius));
        ringY_ = -int(std::lround(pad.ly / mag * radius));
        out.mouse.x = cx + ringX_;
        out.mouse.y = cy + ringY_;
        // The client's walk requests show the camera's turn once it has acted on this point: right away
        // for the first press, a moment later after the stick turned.
        const double angle = std::atan2(double(pad.ly), double(pad.lx));
        if (!ringSteadyFromMs_ || std::fabs(std::remainder(angle - ringAngle_, 2.0 * kPi)) > kRingTurn) {
            ringSteadyFromMs_ = (starting ? now : now + kRingSettleMs);
            ringAngle_ = angle;
        }
        return;
    }
    if (walking_) {
        walking_ = false;
        ringSteadyFromMs_.reset();
        const Cell here = world.selfPos(now);
        const Cell heading = stickDest_ ? *stickDest_ : world.selfDest();
        const Cell stop = stopCell(here, heading);
        stickDest_.reset();
        out.packets.push_back(buildWalk(stop));
        out.notes.push_back("walk stop at " + cellText(stop) + ": we are at about " + cellText(here) +
                            ", heading for " + cellText(heading));
    }
}

void Controller::walkDirect(const PadState& pad, float mag, const WorldState& world, uint32_t now, TickOutput& out) {
    const Cell me = selfCell_ ? *selfCell_ : world.selfPos(now);
    if (mag > cfg_.deadzone) {
        if (!walking_) {
            walking_ = true;
            approach_.reset();
            stopAttacking();  // walking ends a fight; a held A starts it again afterwards
            stickDest_.reset();
            asked_.reset();
        }
        if (look_) {  // 3.0.11-beta.102 (core/stick_walk): the cell along the way, asked again before it is reached
            way_ = stickwalk::wayOf(pad.lx, pad.ly, look_);
        } else {
            double east = 0, north = 0;
            camera_.toWorld(pad.lx / mag, -pad.ly / mag, east, north);
            way_ = stickwalk::wayOf(east, north, std::make_pair(0.0, 1.0));
        }
        const int lead = stickwalk::lead(world.selfSpeed());
        const Cell dest = stickwalk::destination(me, way_, lead, ground_);
        walkTo_ = dest;
        if (dest == me || !stickwalk::askAgain(asked_, me, dest, way_, lead, now)) return;
        out.packets.push_back(buildWalk(dest));
        if (!asked_) out.notes.push_back("walk without the mouse: to " + cellText(dest) + ", " + std::to_string(lead) +
                                         " cells ahead");
        asked_ = stickwalk::Asked{dest, way_, now};
        stickDest_ = dest;
        return;
    }
    walkTo_.reset();
    if (!walking_) return;
    walking_ = false;  // let go: the cell it steps into along its way, no further and no step back
    const Cell stop = stickwalk::stopCell(me, way_, ground_);
    stickDest_.reset();
    asked_.reset();
    out.packets.push_back(buildWalk(stop));
    out.notes.push_back("walk stop at " + cellText(stop) + ": the character at " + cellText(me));
}

void Controller::aim(const PadState& pad, const WorldState& world, uint32_t now, TickOutput& out) {
    const double mag = std::hypot(double(pad.rx), double(pad.ry));
    if (aimLatched_) {
        if (mag < kAimOff) aimLatched_ = false;
        return;
    }
    if (!aiming_ && mag >= kAimPreview && mag < kAimOn) {  // 3.0.11-beta.87: where a push would take the aim
        double dirX = 0;
        double dirY = 0;
        camera_.toWorld(pad.rx, -pad.ry, dirX, dirY);
        const uint32_t best = pickInDirection(world, now, dirX, dirY, blacklist_);
        if (best != target_) preview_ = best;
    }
    if (mag < (aiming_ ? kAimOff : kAimOn)) {
        aiming_ = false;
        return;
    }
    aiming_ = true;
    double dirX = 0;
    double dirY = 0;
    camera_.toWorld(pad.rx, -pad.ry, dirX, dirY);  // the stick's +y is up, the screen's is down
    const uint32_t best = pickInDirection(world, now, dirX, dirY, blacklist_);
    if (best == 0) return;
    if (best == target_) {  // pointing at the target makes it a choice
        picked_ = true;
        return;
    }
    if (targetAlive(world)) {
        const auto current = aimScore(world, now, target_, dirX, dirY);
        const auto challenger = aimScore(world, now, best, dirX, dirY);
        if (current && challenger && *challenger + kAimSwitchMargin > *current) return;
    }
    setTarget(best, now, out);  // a running attack follows: setTarget asks for a new attack request
    picked_ = true;
}

void Controller::handleButtons(uint16_t held, uint16_t down, const WorldState& world, uint32_t now,
                               TickOutput& out) {
    armed_ = uint16_t((armed_ & held) | down);
    for (size_t i = 0; i < kBindableButtons.size(); ++i) {
        const Action::Kind k = bindings_[i].kind;
        if ((down & kBindableButtons[i]) && k != Action::Kind::None && k != Action::Kind::CameraToggle) {
            approach_.reset();  // a new press overrides walking up to something (turning the camera does not)
            break;
        }
    }
    for (size_t i = 0; i < kBindableButtons.size(); ++i) {
        const uint16_t button = kBindableButtons[i];
        const Action& a = bindings_[i];
        if (a.kind == Action::Kind::None) continue;
        const size_t bit = bitIndex(button);
        if (down & button) {
            fire(a, false, world, now, out);
            repeatAtMs_[bit] = now + uint32_t(cfg_.repeatMs);
            continue;
        }
        const bool repeats = a.kind == Action::Kind::Slot || a.kind == Action::Kind::Pickup;
        if (repeats && cfg_.repeatMs > 0 && (armed_ & button) && !ownsApproach(a) &&
            int32_t(now - repeatAtMs_[bit]) >= 0) {
            fire(a, true, world, now, out);
            repeatAtMs_[bit] = now + uint32_t(cfg_.repeatMs);
        }
    }
}

// The button whose press is walking us up to something waits for that walk; the others keep repeating.
bool Controller::ownsApproach(const Action& a) const {
    if (!approach_) return false;
    if (a.kind == Action::Kind::Slot) return !approach_->pickup && approach_->slot == int(a.slot);
    return a.kind == Action::Kind::Pickup && approach_->pickup;
}

void Controller::fire(const Action& a, bool repeat, const WorldState& world, uint32_t now, TickOutput& out) {
    switch (a.kind) {
        case Action::Kind::None:
            break;
        case Action::Kind::Attack:
            attack(world, now, out);
            break;
        case Action::Kind::Slot:
            useSlot(a.slot, repeat, world, now, out);
            break;
        case Action::Kind::Pickup:
            pickup(repeat, world, now, out);
            break;
        case Action::Kind::SitStand:
            out.packets.push_back(buildAction(0, world.sitting() ? kActionStand : kActionSit));
            stopAttacking();
            break;
        case Action::Kind::ClearTarget:
            clearTarget(world, now, out);
            break;
        case Action::Kind::CameraToggle:  // switched before the stick was read this tick
            break;
        case Action::Kind::Talk:
            if (!repeat) talk(world, now, out);
            break;
        case Action::Kind::Reference:
            if (!repeat) out.reference = true;
            break;
    }
}

// 0.7: a click on the NPC, as the client sends it; the server opens the dialog the way it does for a mouse click.
void Controller::talk(const WorldState& world, uint32_t now, TickOutput& out) {
    const uint32_t npc = pickNpc(world, now, talkDirX_, talkDirY_);
    if (npc == 0) {
        deny(out, talkDirX_ != 0 || talkDirY_ != 0 ? "no NPC that way" : "no NPC within reach");
        return;
    }
    if (wantAttack_ || attackSent_) out.packets.push_back(buildStopAttack());
    stopAttacking();
    approach_.reset();
    out.packets.push_back(buildNpcClick(npc));
    const std::optional<Cell> pos = world.actorPos(npc, now);
    out.notes.push_back("talk to NPC " + std::to_string(npc) + (pos ? " at " + cellText(*pos) : std::string()));
}

// A press, or a held D-pad's repeat, in an NPC window.
bool Controller::dialogRepeat(uint16_t button, uint16_t held, uint16_t down, uint32_t now) {
    const size_t bit = bitIndex(button);
    if (down & button) {
        repeatAtMs_[bit] = now + kDialogRepeatFirstMs;
        return true;
    }
    if (!(held & button) || int32_t(now - repeatAtMs_[bit]) < 0) return false;
    repeatAtMs_[bit] = now + kDialogRepeatMs;
    return true;
}

// A: the window's button (Next, Close, OK); B: Close, the menu's Cancel; the D-pad: the menu's choice; the number:
// up / down by 1, RB / LB by 10, right / left by 100.
void Controller::answerDialog(NpcDialog d, uint16_t held, uint16_t down, uint32_t now, TickOutput& out) {
    const bool a = down & kPadA;
    const bool b = down & kPadB;
    if (!(held & kPadB)) bHeldSinceMs_.reset();
    else if (b) bHeldSinceMs_ = now;
    switch (d) {
        case NpcDialog::None:
            break;
        case NpcDialog::Next:
            if (a) out.dialog.push_back(DialogCommand::Next);
            break;
        case NpcDialog::Close:
            if (a || b) out.dialog.push_back(DialogCommand::Close);
            break;
        case NpcDialog::Waiting:  // the NPC is still talking, or left the window without a button
            if (bHeldSinceMs_ && now - *bHeldSinceMs_ >= kDialogForceCloseMs) {
                bHeldSinceMs_.reset();
                out.dialog.push_back(DialogCommand::Close);
                out.notes.push_back("NPC window without a button: closed with B held");
            }
            break;
        case NpcDialog::Menu:
            if (dialogRepeat(kPadUp, held, down, now)) out.dialog.push_back(DialogCommand::MenuUp);
            if (dialogRepeat(kPadDown, held, down, now)) out.dialog.push_back(DialogCommand::MenuDown);
            if (a) out.dialog.push_back(DialogCommand::MenuOk);
            if (b) out.dialog.push_back(DialogCommand::MenuCancel);
            break;
        case NpcDialog::Number: {
            int n = number_;
            const std::pair<uint16_t, int> steps[] = {{kPadUp, 1},    {kPadDown, -1},   {kPadRB, 10},
                                                      {kPadLB, -10},  {kPadRight, 100}, {kPadLeft, -100}};
            for (const auto& [button, step] : steps)
                if (dialogRepeat(button, held, down, now)) n = std::clamp(n + step, 0, kNumberMax);
            if (n != number_) {
                number_ = n;
                out.number = n;
            }
            if (a) out.dialog.push_back(DialogCommand::NumberOk);
            break;
        }
    }
}

void Controller::toggleCamera(TickOutput& out) {
    cameraMode_ = !cameraMode_;
    cameraMs_.reset();
    zoomCarry_ = 0;
    aiming_ = false;
    aimLatched_ = true;  // the stick may still be pushed: back in target mode, no pick until it comes back
    out.rumble = cameraMode_ ? Rumble::CameraOn : Rumble::CameraOff;
    out.notes.push_back(cameraMode_ ? "right stick: camera" : "right stick: targets");
}

// Camera mode: left / right turns the camera like a right-button drag, up / down zooms like the wheel
// (up: closer). Past a small dead zone the response grows with the square of the push.
void Controller::steerCamera(const PadState& pad, uint32_t now, TickOutput& out) {
    const uint32_t dt = cameraMs_ ? std::min<uint32_t>(now - *cameraMs_, kCameraMaxDtMs) : 0;
    cameraMs_ = now;
    auto curve = [](float v) {
        const float m = std::fabs(v);
        if (m <= kCameraDeadZone) return 0.0f;
        const float t = std::min(1.0f, (m - kCameraDeadZone) / (1.0f - kCameraDeadZone));
        return (v < 0 ? -1.0f : 1.0f) * t * t;
    };
    const float seconds = float(dt) / 1000.0f;
    out.camera.turnDeg = curve(pad.rx) * kCameraTurnDegPerSec * seconds;
    const float zy = curve(pad.ry);
    if (zy == 0) {
        zoomCarry_ = 0;
        return;
    }
    zoomCarry_ += zy * kCameraZoomStepsPerSec * seconds;
    const int steps = int(zoomCarry_);  // whole notches; the rest waits for the next tick
    zoomCarry_ -= float(steps);
    out.camera.zoomSteps = steps;
}

void Controller::attack(const WorldState& world, uint32_t now, TickOutput& out) {
    if (keepsTarget(world)) {
        const std::optional<Cell> pos = world.actorPos(target_, now);
        out.notes.push_back("attack: keeps " + std::to_string(target_) +
                            (picked_ ? " (picked with the right stick)" : " (being fought)") +
                            (pos ? " at " + cellText(*pos) : std::string()));
    } else if (takeNearest(world, now, "attack", out) == 0) {
        deny(out, "no monsters around");
        return;
    }
    wantAttack_ = true;
    attackSent_ = false;
}

void Controller::useSlot(int slot, bool repeat, const WorldState& world, uint32_t now, TickOutput& out) {
    const Hotkey& key = world.hotkey(slot);
    if (key.id == 0) {
        if (repeat) return;
        const std::string& why = world.hotkeyNote(slot);
        if (!why.empty())
            deny(out, hotkeyName(slot) + ": " + why);
        else
            deny(out, "slot " + hotkeyName(slot) + " is empty" +
                          (world.hotkeysKnown() ? std::string()
                                                : std::string(" (the pad could not read the hotkey bar)")));
        return;
    }
    std::optional<uint32_t>& last = lastSlotMs_[size_t(slot)];
    if (!due(last, now, kSlotIntervalMs)) return;
    last = now;
    if (!repeat && key.type != 0) {  // 3.0.11-beta.85: a support skill with party members in sight asks whom
        auto sk = world.skill(uint16_t(key.id));
        if (sk && sk->lv > 0 && choosesWhom(*sk)) {
            std::vector<uint32_t> who;
            for (uint32_t id : friends_)
                if (id != world.selfId() && world.actorPos(id, now)) who.push_back(id);
            if (!who.empty()) {
                who.push_back(world.selfId());
                choosing_ = Choosing{slot, std::move(who), 0, now};
                out.notes.push_back(hotkeyName(slot) + ": skill " + std::to_string(sk->id) + " - whom? " +
                                    std::to_string(choosing_->who.size()) + " to choose");
                return;
            }
        }
    }
    const uint32_t target = keepsTarget(world) ? target_ : 0;
    const uint32_t nearest = target != 0 ? 0 : nearestTarget(world, now);
    act(decideSlot(key, world, target, nearest, cfg_, now), slot, repeat, key, world, nearest, now, out);
}

uint16_t Controller::choosingButton() const {
    if (!choosing_) return 0;
    for (size_t i = 0; i < kBindableButtons.size(); ++i)
        if (bindings_[i].kind == Action::Kind::Slot && int(bindings_[i].slot) == choosing_->slot)
            return kBindableButtons[i];
    return 0;
}

void Controller::choose(uint16_t down, const WorldState& world, uint32_t now, TickOutput& out) {
    Choosing& ch = *choosing_;
    const std::string name = hotkeyName(ch.slot);
    if (down & (kPadB | kPadStart)) {
        out.notes.push_back(name + ": the choice cancelled");
        choosing_.reset();
        return;
    }
    if (int32_t(now - ch.sinceMs) >= int32_t(kChooseMs)) {
        out.notes.push_back(name + ": the choice timed out");
        choosing_.reset();
        return;
    }
    const size_t n = ch.who.size();
    if (down & kPadDown) ch.at = (ch.at + 1) % n;
    if (down & kPadUp) ch.at = (ch.at + n - 1) % n;
    uint16_t again = 0;  // the skill's own buttons: on the character
    for (size_t i = 0; i < kBindableButtons.size(); ++i)
        if (bindings_[i].kind == Action::Kind::Slot && int(bindings_[i].slot) == ch.slot) again |= kBindableButtons[i];
    if (!(down & (again | kPadA))) return;
    const uint32_t whom = (down & again) ? world.selfId() : ch.who[ch.at];
    out.notes.push_back(name + ": chosen " + (whom == world.selfId() ? std::string("the character")
                                                                      : "member " + std::to_string(whom)));
    const int slot = ch.slot;
    choosing_.reset();
    const Hotkey& key = world.hotkey(slot);
    act(decideSupport(key, world, whom, now), slot, false, key, world, 0, now, out);
}

void Controller::act(SlotDecision d, int slot, bool repeat, const Hotkey& key, const WorldState& world,
                     uint32_t nearest, uint32_t now, TickOutput& out) {
    if (d.kind == SlotDecision::Kind::Approach && walking_) {
        // The stick has the legs: no walking up against it. A held button casts once it is in range.
        if (!repeat) deny(out, "skill " + std::to_string(d.skill) + ": out of range while walking");
        return;
    }
    if (d.kind == SlotDecision::Kind::Approach && repeat && approach_) return;  // a press started that walk
    if (d.newTarget != 0 && d.newTarget == nearest)
        takeNearest(world, now, hotkeyName(slot).c_str(), out);  // the same monster, with what the pad saw
    else if (d.newTarget != 0)
        setTarget(d.newTarget, now, out);
    switch (d.kind) {
        case SlotDecision::Kind::Deny:
            if (!repeat) deny(out, d.reason);
            break;
        case SlotDecision::Kind::Send:
            out.packets.push_back(std::move(d.packet));
            if (!repeat)
                out.notes.push_back(hotkeyName(slot) + ": " +
                                    (key.type == 0 ? "item " + std::to_string(key.id)
                                                   : "skill " + std::to_string(d.skill) + " lv " +
                                                         std::to_string(d.lv)));
            attackAgainSoon(now);
            break;
        case SlotDecision::Kind::Approach: {
            Approach ap;
            ap.id = d.actor;
            ap.ground = d.ground;
            ap.skill = d.skill;
            ap.lv = d.lv;
            ap.range = d.range;
            ap.slot = slot;
            ap.startMs = now;
            approach_ = ap;
            out.notes.push_back("approaching for skill " + std::to_string(d.skill));
            break;
        }
    }
}

void Controller::sendTake(uint32_t item, uint32_t now, TickOutput& out) {
    out.packets.push_back(buildTakeItem(item));
    lastPickupMs_ = now;
    pickupTriedMs_.emplace(item, now);
}

void Controller::pickup(bool repeat, const WorldState& world, uint32_t now, TickOutput& out) {
    if (repeat && !due(lastPickupMs_, now, kSlotIntervalMs)) return;  // e.g. right behind a walk-up's take
    if (!repeat) pickupTriedMs_.clear();  // a new press gives every item another try
    for (auto it = pickupTriedMs_.begin(); it != pickupTriedMs_.end();) {
        if (world.groundItems().count(it->first) == 0)
            it = pickupTriedMs_.erase(it);
        else
            ++it;
    }
    const Cell me = world.selfPos(now);
    const GroundItem* best = nullptr;
    int bestDist = 0;
    for (const auto& entry : world.groundItems()) {
        const GroundItem& item = entry.second;
        const int d = cellDistance(me, item.pos);
        if (d > kPickupRadius) continue;
        auto tried = pickupTriedMs_.find(item.id);  // someone else's loot or too heavy: move on
        if (tried != pickupTriedMs_.end() && now - tried->second >= kPickupGiveUpMs) continue;
        if (!best || d < bestDist || (d == bestDist && item.id < best->id)) {
            best = &item;
            bestDist = d;
        }
    }
    if (!best) {
        if (!repeat) deny(out, "nothing to pick up");
        return;
    }
    if (bestDist <= kPickupReach) {
        sendTake(best->id, now, out);
        return;
    }
    if (walking_) {  // no walking up against the stick; a held LT picks things up as we pass them
        if (!repeat) deny(out, "item " + std::to_string(best->id) + " is out of reach while walking");
        return;
    }
    if (repeat && approach_) return;  // a press started that walk
    Approach ap;
    ap.pickup = true;
    ap.id = best->id;
    ap.range = kPickupReach;
    ap.startMs = now;
    approach_ = ap;
    out.notes.push_back("walking to item " + std::to_string(best->id));
}

void Controller::clearTarget(const WorldState& world, uint32_t now, TickOutput& out) {
    aiming_ = false;
    aimLatched_ = true;  // clicking the stick may push it too: no new pick until it comes back
    if (target_ == 0 && !wantAttack_) return;
    out.packets.push_back(buildStopAttack());
    // 0x0118 does not stop a walk up to the target (ours, or the client's after "too far"): stop it too.
    if (!walking_ && world.selfMoving(now))
        out.packets.push_back(buildWalk(stopCell(world.selfPos(now), world.selfDest())));
    target_ = 0;
    stopAttacking();
    approach_.reset();
    out.rumble = Rumble::TargetCleared;
    out.notes.push_back("target cleared");
}

// A target left far behind is not what A or a slot should go for: drop it as if it were gone. A held A then
// takes the nearest one (maintainTarget). The server may still be chasing it for our attack: stop that.
void Controller::dropFarTarget(const WorldState& world, uint32_t now, TickOutput& out) {
    if (target_ == 0) return;
    const std::optional<Cell> pos = world.actorPos(target_, now);
    if (!pos || cellDistance(world.selfPos(now), *pos) <= kTargetReach) return;
    out.notes.push_back("target " + std::to_string(target_) + " is too far, dropped");
    if (wantAttack_ || attackSent_) out.packets.push_back(buildStopAttack());
    target_ = 0;
    attackSent_ = false;
    engagedSinceMs_.reset();
    if (approach_ && !approach_->pickup) approach_.reset();
}

void Controller::stepApproach(const WorldState& world, uint32_t now, TickOutput& out) {
    if (!approach_) return;
    Approach& ap = *approach_;
    if (now - ap.startMs >= kApproachTimeoutMs) {
        out.notes.push_back("approach timed out");
        approach_.reset();
        return;
    }
    std::optional<Cell> goal;
    if (ap.pickup) {
        auto it = world.groundItems().find(ap.id);
        if (it != world.groundItems().end()) goal = it->second.pos;
    } else {
        goal = world.actorPos(ap.id, now);
    }
    if (!goal) {
        out.notes.push_back("approach target is gone");
        approach_.reset();
        return;
    }
    if (cellDistance(world.selfPos(now), *goal) <= ap.range) {
        if (ap.pickup)
            sendTake(ap.id, now, out);
        else if (ap.ground)
            out.packets.push_back(buildSkillToPos(ap.skill, ap.lv, *goal));
        else
            out.packets.push_back(buildSkillToId(ap.skill, ap.lv, ap.id));
        if (ap.slot >= 0) lastSlotMs_[size_t(ap.slot)] = now;  // a held button waits its turn again
        const bool skill = !ap.pickup;
        approach_.reset();
        if (skill) attackAgainSoon(now);
        return;
    }
    if (due(ap.lastStepMs, now, kApproachStepMs)) {
        out.packets.push_back(buildWalk(*goal));
        ap.lastStepMs = now;
    }
}

void Controller::maintainTarget(bool attackHeld, const WorldState& world, uint32_t now, TickOutput& out) {
    for (auto it = blacklist_.begin(); it != blacklist_.end();) {
        if (int32_t(now - it->second) >= 0)
            it = blacklist_.erase(it);
        else
            ++it;
    }
    if (target_ != 0 && !targetAlive(world)) {
        out.notes.push_back("target " + std::to_string(target_) + " is gone");
        target_ = 0;
        attackSent_ = false;
        engagedSinceMs_.reset();
    }
    if (target_ != 0 && engagedSinceMs_ &&
        shouldGiveUp(world, target_, *engagedSinceMs_, now, uint32_t(cfg_.giveUpSeconds * 1000.0f))) {
        blacklist_[target_] = now + uint32_t(cfg_.blacklistSeconds * 1000.0f);
        out.notes.push_back("giving up on " + std::to_string(target_));
        target_ = 0;
        attackSent_ = false;
        engagedSinceMs_.reset();
    }
    // A press fights the target until it is gone; holding A goes on to the next one.
    if (attackHeld)
        wantAttack_ = true;
    else if (target_ == 0)
        stopAttacking();
    const bool idle = !walking_ && !approach_ && !world.sitting();
    if (wantAttack_ && target_ == 0 && idle) {
        if (nearestTarget(world, now) != 0) takeNearest(world, now, "held attack", out);
    }
    // The server drops our attack for many reasons (a skill, a cast, a click elsewhere). Standing still with
    // no hit of ours for a while means it is not attacking: ask again. Not while moving: every attack
    // request stops the walk first (clif_parse_ActionRequest_sub).
    if (target_ != 0 && wantAttack_ && attackSent_ && idle && !world.selfMoving(now)) {
        uint32_t ref = lastAttackMs_ ? *lastAttackMs_ : now;
        if (auto hit = world.lastHitMs(target_); hit && int32_t(*hit - ref) > 0) ref = *hit;
        if (now - ref >= kReattackMs) attackSent_ = false;
    }
    if (target_ != 0 && wantAttack_ && !attackSent_ && idle &&
        due(lastAttackMs_, now, uint32_t(cfg_.attackIntervalMs))) {
        out.packets.push_back(buildAction(target_, kActionAttack));
        lastAttackMs_ = now;
        attackSent_ = true;
        if (!engagedSinceMs_) engagedSinceMs_ = now;
    }
}

}  // namespace uapad

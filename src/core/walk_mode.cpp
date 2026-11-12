#include "core/walk_mode.hpp"

#include "core/key_walk.hpp"

namespace uagfx::walkmode {

using keywalk::kDown;
using keywalk::kLeft;
using keywalk::kRight;
using keywalk::kUp;

uint8_t dirOfScan(unsigned scan, bool extended) {
    switch (scan) {
    case 0x11: return kUp;  // W
    case 0x1E: return kLeft;  // A
    case 0x1F: return kDown;  // S
    case 0x20: return kRight;  // D
    case 0x48: return kUp;  // the arrows, the numpad's 8 2 4 6
    case 0x50: return kDown;
    case 0x4B: return kLeft;
    case 0x4D: return kRight;
    default: break;
    }
    if (extended) return 0;  // Home, End, Page Up, Page Down: the game's
    switch (scan) {
    case 0x47: return uint8_t(kUp | kLeft);  // the numpad's 7
    case 0x49: return uint8_t(kUp | kRight);
    case 0x4F: return uint8_t(kDown | kLeft);
    case 0x51: return uint8_t(kDown | kRight);
    case 0x4C: return kStop;  // its 5
    default: return 0;
    }
}

bool printable(unsigned vk) {
    return vk == 0x20 || (vk >= 0x30 && vk <= 0x39) || (vk >= 0x41 && vk <= 0x5A) || (vk >= 0x60 && vk <= 0x6F) ||
           (vk >= 0xBA && vk <= 0xC0) || (vk >= 0xDB && vk <= 0xDF) || vk == 0xE2;
}

void FileFollow::wrote(uint64_t now) {
    wrote_ = true;
    wroteAt_ = now;
}

bool FileFollow::follow(bool value, uint64_t now) {
    if (known_ && value == value_) return false;
    const bool first = !known_;
    known_ = true;
    value_ = value;
    return first || !wrote_ || now - wroteAt_ >= kEchoMs;
}

void Mode::set(bool on) {
    on_ = on;
    typing_ = false;
    held_.clear();
}

uint8_t Mode::dirs() const {
    uint8_t d = 0;
    for (const Held& h : held_) d = uint8_t(d | h.dir);
    return d;
}

bool Mode::verify(keywalk::KeyDown down) {
    const size_t before = held_.size();
    for (size_t i = held_.size(); i-- > 0;)
        if (!down(held_[i].vk)) held_.erase(held_.begin() + long(i));
    return held_.size() != before;
}

Act Mode::key(const Key& k) {
    const bool walkingNow = on_ && !typing_;
    if (!k.down) {
        const uint8_t d = walkingNow ? dirOfScan(k.scan, k.extended) : 0;
        if (d != 0 && d != kStop) {
            for (size_t i = held_.size(); i-- > 0;)
                if (held_[i].scan == k.scan && held_[i].extended == k.extended) held_.erase(held_.begin() + long(i));
            return Act::Swallow;
        }
        // the let go of a key whose press was the mode's (a letter, Tab): the game never saw it pressed
        return walkingNow && !k.ctrl && !k.alt && (printable(k.vk) || k.scan == 0x0F) ? Act::Swallow : Act::Pass;
    }
    const bool modifier = k.ctrl || k.alt;
    if (k.scan == kToggleScan && !modifier && (walkingNow || (!on_ && k.chatEmpty))) {
        if (k.repeat) return Act::Swallow;  // 3.0.18-beta.215: held - once a press, its letters not typed
        on_ = !on_;
        typing_ = false;
        held_.clear();
        return Act::Toggle;
    }
    if (!on_) return Act::Pass;
    if (typing_) {
        if (k.vk == 0x0D) {
            typing_ = false;
            return Act::Send;
        }
        if (k.vk == 0x1B) {
            typing_ = false;
            return Act::CancelChat;
        }
        return Act::Pass;
    }
    if (modifier) return Act::Pass;
    if (k.vk == 0x0D) {
        typing_ = true;
        held_.clear();
        return Act::OpenChat;
    }
    const uint8_t d = dirOfScan(k.scan, k.extended);
    if (d == kStop) {
        held_.clear();
        return Act::Swallow;
    }
    if (d) {
        bool known = false;
        for (const Held& h : held_) known = known || (h.scan == k.scan && h.extended == k.extended);
        if (!known && held_.size() < 8) held_.push_back(Held{k.vk, k.scan, k.extended, d});
        return Act::Swallow;
    }
    // 3.0.18-beta.209: the quick actions, by the keys' places (any layout)
    Do act = Do::None;
    switch (k.scan) {
    case 0x39: act = Do::Attack; break;  // Space
    case 0x21: act = Do::Pickup; break;  // F
    case 0x13: act = Do::Talk; break;    // R
    case 0x2E: act = Do::Sit; break;     // C
    case 0x0F: act = k.shift ? Do::PrevTarget : Do::NextTarget; break;  // Tab
    case 0x10: act = Do::CameraLeft; break;   // Q
    case 0x12: act = Do::CameraRight; break;  // E
    default:
        if (k.scan >= 0x02 && k.scan <= 0x0A) {  // 1-9
            act = Do::Slot;
            slot_ = int(k.scan) - 0x02;
        }
        break;
    }
    if (act != Do::None) {
        action_ = act;
        return Act::Action;
    }
    return printable(k.vk) ? Act::Swallow : Act::Pass;
}

}  // namespace uagfx::walkmode

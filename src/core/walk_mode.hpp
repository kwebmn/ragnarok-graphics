#pragma once
#include <cstdint>

namespace uagfx::walkmode {

// 3.0.18-beta.208 (the keyboard concept https://claude.ai/artifact/SxvWxL5BM2tHtSWAvYPh9J, stage 2; the owner: «а что
// если мы специальный режим сделаем для ходьбы на клавиатуре?»): the walk mode. ` (Ё, the key left of 1, by its scan
// code: any layout) switches it with the chat line empty; while it is on and the chat closed the letters, digits and
// signs do not type, W A S D, the arrows and the numpad walk (its 7 9 1 3 diagonals, 5 stop), Enter opens the chat
// line and sends it, Esc closes it. Alt and Ctrl keys, F-keys, Esc, Insert stay the game's.

struct Key {
    unsigned vk = 0;
    unsigned scan = 0;      // WM_KEYDOWN's lParam bits 16-23: the same key in any layout
    bool extended = false;  // its bit 24: the arrows' and Home's own keys (the numpad's are not)
    bool down = true;       // false: let go
    bool ctrl = false, alt = false, shift = false;
    bool chatEmpty = true;  // the chat line holds no words
};

enum class Act : uint8_t {
    Pass,        // the game's (or the chat line's)
    Swallow,     // the mode's: the game sees neither the key nor its letter
    Toggle,      // the mode switched (its letter not typed)
    OpenChat,    // Enter: the chat line opened for typing (the Enter itself not the game's)
    Send,        // Enter while typing: the game sends the line, the mode walks again
    CancelChat,  // Esc while typing: the line cleared, the mode walks again
    Action,      // 3.0.18-beta.209: a quick action's key (Mode::action) - not typed
};

// 3.0.18-beta.209 (stage 3): the walk mode's quick actions - Space attack (the pad's one button: the target, else
// loot beside, else the nearest monster, else the NPC near), F pick up, R talk, C sit, Tab / Shift+Tab the next /
// the previous target, Q / E the camera turned, 1-9 the shown bar row's slots
enum class Do : uint8_t { None, Attack, Pickup, Talk, Sit, NextTarget, PrevTarget, CameraLeft, CameraRight, Slot };

constexpr unsigned kToggleScan = 0x29;

// 3.0.18-beta.214 (R-CX44S5's log: «off (`)», «on by the settings», «off by the settings» within 0.3 s): the settings'
// [interface] walk_mode followed when its value changes - not the toggle's own writes read back (within kEchoMs of
// the last one), which once turned the mode back on and off again
class FileFollow {
public:
    static constexpr uint64_t kEchoMs = 3000;
    void wrote(uint64_t now);               // the toggle wrote the file
    bool follow(bool value, uint64_t now);  // the file's value this frame: true - the mode is to be set to it

private:
    bool known_ = false, value_ = false, wrote_ = false;
    uint64_t wroteAt_ = 0;
};

class Mode {
public:
    bool on() const { return on_; }
    bool typing() const { return typing_; }
    uint8_t dirs() const { return dirs_; }  // keywalk::Dir bits the mode's keys hold
    void set(bool on);                      // from the settings: typing and the held keys end
    void lost() { dirs_ = 0; }              // the window lost the keys
    Act key(const Key& k);
    Do action() const { return action_; }  // the last Act::Action's
    int slot() const { return slot_; }     // Do::Slot's: 0-8 in the shown row

private:
    bool on_ = false, typing_ = false;
    uint8_t dirs_ = 0;
    Do action_ = Do::None;
    int slot_ = 0;
};

// The walk of a key in the mode: W A S D, the arrows (any), the numpad's 7 9 1 3 (not Home's own keys); kStop its 5;
// 0 none
constexpr uint8_t kStop = 0xFF;
uint8_t dirOfScan(unsigned scan, bool extended);
bool printable(unsigned vk);  // letters, digits, the numpad's digits and signs, Space, the OEM signs

}  // namespace uagfx::walkmode

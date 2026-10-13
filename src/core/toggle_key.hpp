#pragma once

namespace uagfx {

// A key that switches something: pressed() is true once per press, however many frames the key is held down.
class ToggleKey {
public:
    bool pressed(bool down) {
        const bool edge = down && !wasDown_;
        wasDown_ = down;
        return edge;
    }

private:
    bool wasDown_ = false;
};

}  // namespace uagfx

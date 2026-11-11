#pragma once
#include <map>
#include <string>

namespace uagfx::windowsizes {

// 3.0.18-beta.151 (the owner: the equipment window and others not sized by
// pulling): a window's own size, its bottom right corner pulled (win/window_stack) - kept by the
// window's name (uaro-graphics\window-sizes.txt: «equip 0.85» a line), 70-150 % over the interface's
constexpr float kLeast = 0.7f, kMost = 1.5f;
// The size after the corner went from (w0, h0) to (w, h) - the box's width and height shares together - from `from`
float pulled(float from, float w0, float h0, float w, float h);

class Sizes {
public:
    float of(const std::string& window) const;  // 1 not kept
    void set(const std::string& window, float size);
    std::string text() const;
    static Sizes parse(const std::string& text);

private:
    std::map<std::string, float> sizes_;
};

}  // namespace uagfx::windowsizes

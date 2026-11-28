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

// 3.0.24-beta.48 (the audit 28.11, the owner's «сделать»: «когда растягиваешь инвентарь то становится больше итемов а
// не просто размер окошка»): a grid window's («Речі», «Склад», «Візок») own columns and rows - its corner adds whole
// cells, their size kept; the scale (any sized window's) by Ctrl and the wheel, 5 % a notch
struct Cells {
    int columns = 0, rows = 0;
};
constexpr int kLeastColumns = 5, kMostColumns = 12, kLeastRows = 3, kMostRows = 10;
// the cells after the corner went (dx, dy) pixels from where it was pressed with `from`; `step` a cell with its gap
Cells cellsPulled(Cells from, float step, float dx, float dy);
float wheeled(float from, int notches);  // +1 a notch up (bigger)
// 3.0.24-beta.52 (the audit 28.11, item 10: the lists still scaled by their corner): a list window's («Навички», the
// NPC's shop) corner adds whole rows, their size kept; `step` a row's height (0: unknown - as it was)
constexpr int kLeastListRows = 4, kMostListRows = 16;
int rowsPulled(int from, float step, float dy);

class Sizes {
public:
    float of(const std::string& window) const;  // 1 not kept
    void set(const std::string& window, float size);
    Cells cells(const std::string& window, Cells fallback) const;  // 3.0.24-beta.48: «items.cells 10x6» a line
    void setCells(const std::string& window, Cells c);
    int rows(const std::string& window, int fallback) const;  // 3.0.24-beta.52: «skills.rows 12» a line
    void setRows(const std::string& window, int rows);
    std::string text() const;
    static Sizes parse(const std::string& text);

private:
    std::map<std::string, float> sizes_;
    std::map<std::string, Cells> cells_;
    std::map<std::string, int> rows_;
};

}  // namespace uagfx::windowsizes

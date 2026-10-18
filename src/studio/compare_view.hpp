#pragma once
#include <windows.h>

#include <memory>
#include <vector>

#include "core/map_textures.hpp"
#include "core/studio_view.hpp"
#include "studio/pictures.hpp"

namespace uagfx::studio {

// 2.3 (T2): the inspector's compare control - the original against a picture (the pack's, or one made again), at
// one zoom and pan: side by side, a wipe between them, the picture tiled 3x3, both laid as the map lays them, or their
// difference. The wheel zooms about the cursor (a quarter to 16 times the original's texels), a drag pans, a drag
// with the right button (or Shift) moves the wipe; nearest texels or smooth.
class CompareView {
public:
    enum class Mode { Pair, Wipe, Tiled, Map, Difference, Single };  // Single: the first picture alone
    static bool registerClass(HINSTANCE instance);
    HWND create(HWND parent, int id);
    HWND hwnd() const { return hwnd_; }
    void set(std::shared_ptr<const Image> original, std::shared_ptr<const Image> other);  // the view fitted
    void setMode(Mode m);
    void setSmooth(bool smooth);
    void setPatch(std::vector<map_textures::PatchCell> cells);  // the map's cells taking the texture
    void fit();

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void paint(HDC dc);
    // The colour of a picture at an original's texel position (ox, oy), nearest or smooth; false past it.
    bool sample(const Image& img, float ox, float oy, bool wrap, uint32_t& out) const;
    bool sampleMap(const Image& img, float px, float py, uint32_t& out) const;  // (px, py) in cells

    HWND hwnd_ = nullptr;
    std::shared_ptr<const Image> a_, b_;
    std::vector<uint32_t> diff_;
    Mode mode_ = Mode::Pair;
    bool smooth_ = false;
    View view_;
    float wipe_ = 0.5f;  // of the pane's width
    std::vector<map_textures::PatchCell> patch_;
    int patchW_ = 0, patchH_ = 0;  // cells
    std::vector<int> patchAt_;     // patchW_ * patchH_: the cell's index in patch_, or -1
    bool dragging_ = false, wiping_ = false;
    POINT last_{};
};

}  // namespace uagfx::studio

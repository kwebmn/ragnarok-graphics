#pragma once
#include <windows.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/settings/strings.hpp"
#include "core/texture_catalog.hpp"
#include "studio/compare_view.hpp"
#include "studio/pictures.hpp"
#include "studio/textures_tab.hpp"

namespace uagfx::studio {

class CatalogWork;

// What the inspector shows of a row, made on a thread: the pictures, the map's cells, the information.
struct Shown {
    Row row;
    std::shared_ptr<const Image> original, packed;
    std::vector<map_textures::PatchCell> patch;
    std::wstring info;
    bool own = false;  // the game's own always
    std::shared_ptr<const Image> made;  // made again before (studio\made\), not kept yet
    std::string key, file;
};

// 2.3 (T2, T3, T4), the inspector: the compare control with its modes, the information of the texture shown (its
// archive and those it shadows, its keys, its picture, what it was made from, its maps, its atlas, its quality
// numbers, its material) and the actions on it.
class Inspector {
public:
    Inspector(HWND parent, settings::Lang lang, HFONT font, int dpi, std::wstring gameDir, CatalogWork& catalog);
    void show(bool on);
    void layout(const RECT& area);
    void showRow(const Row& row, const std::vector<std::string>& orphans);
    void shownReady(uint64_t generation, Shown* shown);
    void setMade(std::shared_ptr<const Image> made);  // a picture made again: compared at once
    void refresh(const std::vector<std::string>& orphans);  // the row shown, read again
    bool command(int id, int code);
    const Shown* shown() const { return shown_.get(); }
    std::shared_ptr<const Image> made() const { return made_; }
    // The actions (the window's): make again, keep, discard, take out, the game's own always (or not).
    std::function<void()> onMake, onKeep, onDiscard, onTakeOut, onOwn;

private:
    enum Id : int {
        kPair = 400, kWipe, kTiled, kMap, kDiff, kSmooth, kVersus, kView, kInfo, kMake, kKeep, kDiscard, kTakeOut,
        kOwn
    };
    void showCompare();
    std::wstring t(settings::Text x) const;

    HWND parent_;
    settings::Lang lang_;
    HFONT font_;
    int dpi_;
    std::wstring gameDir_;
    CatalogWork& catalog_;
    std::vector<HWND> controls_;
    HWND modes_[5] = {}, smooth_ = nullptr, versus_ = nullptr, info_ = nullptr, actions_[5] = {};
    CompareView view_;
    std::unique_ptr<Shown> shown_;
    std::shared_ptr<const Image> made_;
    uint64_t generation_ = 0;
};

}  // namespace uagfx::studio

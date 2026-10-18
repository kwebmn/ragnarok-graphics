#pragma once
#include <windows.h>

#include <commctrl.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/settings/strings.hpp"
#include "core/studio_view.hpp"
#include "core/texture_catalog.hpp"

namespace uagfx::studio {

class CatalogWork;
class Thumbs;

// A row of the list: a catalog picture (file, picture), or (file kNoFile) a pack picture no texture names.
struct Row {
    static constexpr uint32_t kNoFile = 0xFFFFFFFFu;
    uint32_t file = 0, picture = 0;
    uint8_t state = 0;  // catalog::State; 3 an orphan
    uint8_t model = 0;  // 0 none, 1 general, 2 anime, 3 anime6b, 4 another
    int64_t made = 0;   // the picture's time in the pack
};

struct RowsResult {
    std::vector<Row> rows;
    std::vector<std::string> orphans;  // the pack's pictures no texture names (Row::kNoFile rows point here)
};

// 2.3 (T1, T2), the Textures tab: the navigator (maps, folders, states, models, the game's reports), the filter and
// the list of every picture of the catalog - its thumbnail pair, file, state, model, flags, detail kept, size, date.
// The rows are made on a thread (300 000 of them) and sorted there.
class TexturesTab {
public:
    TexturesTab(HWND parent, settings::Lang lang, HFONT font, int dpi, std::wstring gameDir, CatalogWork& catalog,
                Thumbs& thumbs);
    ~TexturesTab();  // its rows' threads done first: they read the thumbnails
    void show(bool on);
    void layout(const RECT& area);
    void catalogChanged();  // the navigator and the rows again
    void thumbsReady();     // the rows in view drawn again
    void rowsReady(uint64_t generation, RowsResult* result);  // takes it
    bool notify(const NMHDR* n, LRESULT& result);
    void command(int id, int code);
    void timer(UINT_PTR id);
    std::function<void(const Row& row)> onSelect;  // the inspector's
    const std::vector<std::string>& orphans() const { return orphans_; }
    std::vector<Row> selection() const;  // the rows selected in the list
    void showKey(const std::string& key);  // L2: every texture, filtered by the key; its first row shown

private:
    enum Id : int { kTree = 300, kFilter, kList, kCount };
    void fillTree();
    void rebuild();  // the rows on a thread, for the navigator's place and the filter
    std::wstring cell(const Row& r, int column) const;
    void drawThumb(const NMLVCUSTOMDRAW* d, const Row& r);
    std::wstring t(settings::Text x) const;

    HWND parent_;
    settings::Lang lang_;
    HFONT font_;
    int dpi_;
    std::wstring gameDir_;
    CatalogWork& catalog_;
    Thumbs& thumbs_;
    HWND tree_ = nullptr, filter_ = nullptr, list_ = nullptr, count_ = nullptr;
    HIMAGELIST rowHeight_ = nullptr;
    std::vector<std::string> navs_;  // a tree item's lParam: its index here
    RowFilter filter_now_;
    std::vector<Row> rows_;
    std::vector<std::string> orphans_;
    std::shared_ptr<const catalog::Catalog> shown_;
    uint64_t generation_ = 0;
    std::atomic<int> working_{0};  // rows being made on threads
    bool selectFirst_ = false;  // the next rows: the first one selected and shown (a pick from the game)
    HTREEITEM all_ = nullptr;
    int sortColumn_ = 1;
    bool sortDown_ = false;
};

}  // namespace uagfx::studio

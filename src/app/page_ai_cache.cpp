#include <set>

#include "app/ai_cache.hpp"
#include "app/page.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kList = 1001;
constexpr int kKinds = 1010;  // + the kind: which kinds the ticked maps lose
constexpr int kDelete = 1020;
constexpr int kDeleteOld = 1021;
constexpr int kDeleteAll = 1022;
const char kAllMaps[] = "*";  // the first row: every map

// What the page's ticks were: the page is made again after every change and keeps them across.
std::set<std::string> g_ticked;
bool g_kinds[ai::kKinds] = {true, true, true, true, true};

// What the texture pack holds by map and kind (0.12; 0.48: the pack's index, upscaled.grf until 0.47), and deleting
// some or all of it while the game and the pack's builder are closed.
class AiCachePage : public Page {
public:
    explicit AiCachePage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        path_ = m.paths().dir + L"\\uaro-textures.zip";
        oldPath_ = m.paths().dir + L"\\upscaled.grf";
        busy_ = m.now().gameRunning || packInUse();
        // 2.2.5: not read while its builder or a game writes it (it has no directory then)
        const bool writing = packInUse();
        file_ = writing ? PackFile() : readPack(path_);
        oldBytes_ = fileBytes(oldPath_);
        contents_ = ai::CacheContents(file_.entries);
        index_ = contents_.index();
        const Lang l = host_.lang();
        p.heading(t(Text::PageAiCache));
        if (writing)
            p.note(t(Text::AiCacheRunning), Tone::Warn);
        else if (!file_.exists)
            p.note(t(Text::AiCacheNone));
        else if (!file_.error.empty())
            p.note(tf(Text::AiCacheUnreadable, {file_.error}), Tone::Bad);
        else
            p.note(tf(Text::AiCacheSummary, {num(index_.all.total), sizeText(index_.all.bytes, l),
                                              sizeText(file_.bytes, l)}));
        list_ = p.listView(kList,
                           {{t(Text::AiCacheMap), 150}, {t(Text::AiCacheColGround), 60},
                            {t(Text::AiCacheColObjects), 64}, {t(Text::AiCacheColSprites), 76},
                            {t(Text::AiCacheColEffects), 62}, {t(Text::AiCacheColIcons), 58},
                            {t(Text::AiCacheColTotal), 56}, {t(Text::AiCacheColSize), 80}},
                           8, true);
        maps_.clear();
        if (!index_.rows.empty()) {
            maps_.push_back(kAllMaps);
            addRow(t(Text::AiCacheAllMaps), index_.all);
        }
        for (const ai::CacheRow& r : index_.rows) {
            maps_.push_back(r.map);
            addRow(r.map.empty() ? t(Text::AiCacheNoMap) : wide(r.map), r);
        }
        for (size_t i = 0; i < maps_.size(); ++i) tick(int(i), g_ticked.count(maps_[i]) > 0);
        p.note(t(Text::AiCacheDeleteKinds), Tone::Normal);
        std::vector<CheckItem> kinds;
        const Text names[] = {Text::AiCacheColGround, Text::AiCacheColObjects, Text::AiCacheColSprites,
                              Text::AiCacheColEffects, Text::AiCacheColIcons};
        for (int k = 0; k < ai::kKinds; ++k) kinds.push_back({kKinds + k, t(names[k]), g_kinds[k], L"", true});
        p.checks(kinds, 5);
        const bool usable = file_.exists && file_.error.empty() && !busy_;
        std::vector<ButtonItem> buttons = {{kDelete, t(Text::AiCacheDelete), usable && !g_ticked.empty()},
                                           {kDeleteAll, t(Text::AiCacheDeleteAll), file_.exists && !busy_}};
        if (oldBytes_)  // 0.48: upscaled.grf of 0.11-0.47, not used any more
            buttons.push_back({kDeleteOld, tf(Text::AiCacheDeleteOld, {sizeText(oldBytes_, l)}), !busy_});
        p.buttons(buttons);
        if (busy_ && !writing) p.note(t(Text::AiCacheRunning), Tone::Warn);
        p.note(t(Text::AiCacheOldNote));
    }

    void command(int id, int code) override {
        if (code != BN_CLICKED) return;
        if (id >= kKinds && id < kKinds + ai::kKinds) {
            g_kinds[id - kKinds] = SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED;
            return;
        }
        if (id == kDelete) {
            ai::CacheDeletion d;
            for (const std::string& map : g_ticked)
                if (map != kAllMaps) d.maps.insert(map);
            if (g_ticked.count(kAllMaps))
                for (const ai::CacheRow& r : index_.rows) d.maps.insert(r.map);
            for (int k = 0; k < ai::kKinds; ++k) d.kinds[k] = g_kinds[k];
            const std::vector<std::string> keep = contents_.keep(d);
            const size_t gone = contents_.pictures() - keep.size();
            if (!gone) {
                host_.message(t(Text::AiCacheNothing), false);
                return;
            }
            if (!host_.ask(tf(Text::AiCacheAskDelete, {num((long long)gone)}))) return;
            if (busy()) return;
            const std::string error = rewritePack(path_, keep);
            if (!error.empty())
                host_.message(tf(Text::AiCacheError, {error}), true);
            else
                host_.message(tf(Text::AiCacheDeleted, {num((long long)gone), sizeText(fileBytes(path_),
                                                                                       host_.lang())}),
                              false);
            g_ticked.clear();
            host_.rebuild();
            return;
        }
        if (id == kDeleteOld) {  // 0.48: upscaled.grf of 0.11-0.47
            if (!host_.ask(tf(Text::AiCacheAskOld, {sizeText(oldBytes_, host_.lang())}))) return;
            if (busy()) return;
            const std::string error = removeFile(oldPath_);
            host_.message(error.empty() ? t(Text::AiCacheOldDeleted) : tf(Text::AiCacheError, {error}),
                          !error.empty());
            host_.rebuild();
            return;
        }
        if (id == kDeleteAll) {
            if (!host_.ask(tf(Text::AiCacheAskAll, {sizeText(file_.bytes, host_.lang())}))) return;
            if (busy()) return;
            const std::string error = removeFile(path_);
            host_.message(error.empty() ? t(Text::AiCacheDeletedAll) : tf(Text::AiCacheError, {error}), !error.empty());
            g_ticked.clear();
            host_.rebuild();
        }
    }

    LRESULT notify(const NMHDR* n) override {
        if (n->idFrom != UINT_PTR(kList) || n->code != LVN_ITEMCHANGED || updating_) return 0;
        const auto* v = reinterpret_cast<const NMLISTVIEW*>(n);
        if (!(v->uChanged & LVIF_STATE) || ((v->uNewState ^ v->uOldState) & LVIS_STATEIMAGEMASK) == 0) return 0;
        if (v->iItem < 0 || size_t(v->iItem) >= maps_.size()) return 0;
        const bool on = ((v->uNewState & LVIS_STATEIMAGEMASK) >> 12) == 2;
        if (maps_[size_t(v->iItem)] == kAllMaps) {  // every row with it
            updating_ = true;
            for (size_t i = 0; i < maps_.size(); ++i) {
                tick(int(i), on);
                if (on)
                    g_ticked.insert(maps_[i]);
                else
                    g_ticked.erase(maps_[i]);
            }
            updating_ = false;
        } else if (on) {
            g_ticked.insert(maps_[size_t(v->iItem)]);
        } else {
            g_ticked.erase(maps_[size_t(v->iItem)]);
            g_ticked.erase(kAllMaps);
            updating_ = true;
            if (!maps_.empty() && maps_[0] == kAllMaps) tick(0, false);
            updating_ = false;
        }
        const bool usable = file_.exists && file_.error.empty() && !busy_;
        EnableWindow(GetDlgItem(host_.window(), kDelete), usable && !g_ticked.empty());
        return 0;
    }

private:
    void addRow(const std::wstring& name, const ai::CacheRow& r) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = int(SendMessageW(list_, LVM_GETITEMCOUNT, 0, 0));
        item.pszText = const_cast<wchar_t*>(name.c_str());
        const int row = int(SendMessageW(list_, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item)));
        std::vector<std::wstring> cells;
        for (int k = 0; k < ai::kKinds; ++k) cells.push_back(wide(num(r.count[k])));
        cells.push_back(wide(num(r.total)));
        cells.push_back(wide(sizeText(r.bytes, host_.lang())));
        for (size_t c = 0; c < cells.size(); ++c) {
            item.iSubItem = int(c) + 1;
            item.pszText = const_cast<wchar_t*>(cells[c].c_str());
            SendMessageW(list_, LVM_SETITEMTEXTW, WPARAM(row), reinterpret_cast<LPARAM>(&item));
        }
    }

    void tick(int row, bool on) {
        LVITEMW item{};
        item.stateMask = LVIS_STATEIMAGEMASK;
        item.state = INDEXTOSTATEIMAGEMASK(on ? 2 : 1);
        SendMessageW(list_, LVM_SETITEMSTATE, WPARAM(row), reinterpret_cast<LPARAM>(&item));
    }

    // Again just before writing: the game may have started while the question was open.
    bool busy() {
        if (!host_.model().now().gameRunning && !packInUse()) return false;
        host_.message(t(Text::AiCacheRunning), true);
        host_.rebuild();
        return true;
    }

    std::wstring path_, oldPath_;
    PackFile file_;
    uint64_t oldBytes_ = 0;  // upscaled.grf's, 0 when it is gone
    ai::CacheContents contents_{std::vector<ai::CacheEntry>()};
    ai::CacheIndex index_;
    std::vector<std::string> maps_;  // the list's rows: kAllMaps, the maps, "" for no map
    HWND list_ = nullptr;
    bool busy_ = false;
    bool updating_ = false;
};

}  // namespace

std::unique_ptr<Page> makeAiCachePage(PageHost& host) { return std::make_unique<AiCachePage>(host); }

}  // namespace uagfx::app

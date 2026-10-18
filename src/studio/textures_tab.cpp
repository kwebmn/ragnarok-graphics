#include "studio/textures_tab.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "app/text.hpp"
#include "core/ai/picture_metrics.hpp"
#include "core/studio_link.hpp"
#include "studio/catalog_work.hpp"
#include "studio/messages.hpp"
#include "studio/thumbs.hpp"
#include "win/paths.hpp"

namespace uagfx::studio {
namespace {

constexpr UINT_PTR kFilterTimer = 20;  // the filter applied a moment after the last key
constexpr int kColumns = 8;
const char* const kModels[] = {"", "general", "anime", "anime6b"};

std::wstring fromCp949(const std::string& s) {
    const int n = MultiByteToWideChar(949, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(949, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

std::string toCp949(const std::wstring& w) {
    const int n = WideCharToMultiByte(949, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(size_t(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(949, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

uint8_t modelCode(const std::string& m) {
    for (uint8_t i = 1; i < 4; ++i)
        if (m == kModels[i]) return i;
    return m.empty() ? 0 : 4;
}

// What a rebuild works from: copies and shared parts, so that the window goes on meanwhile.
struct Rebuild {
    HWND notify = nullptr;
    uint64_t generation = 0;
    std::shared_ptr<const catalog::Catalog> cat;
    std::shared_ptr<const catalog::PackView> view;
    RowFilter filter;
    Thumbs* thumbs = nullptr;
    int sortColumn = 1;
    bool sortDown = false;
};

DWORD WINAPI rebuildMain(void* arg) {
    std::unique_ptr<Rebuild> job(static_cast<Rebuild*>(arg));
    auto out = std::make_unique<RowsResult>();
    const catalog::Catalog& c = *job->cat;
    static const catalog::PackView none;
    const catalog::PackView& v = job->view ? *job->view : none;
    const auto modelOf = [&v](const std::string& name) {
        const auto e = v.index.pictures.find(name);
        if (e != v.index.pictures.end() && !e->second.model.empty()) return e->second.model;
        const auto m = v.index.made.find(name);
        return m == v.index.made.end() ? std::string() : m->second.model;
    };
    const auto timeOf = [&v](const std::string& name) {
        const auto t = v.pictures.find(name);
        return t == v.pictures.end() ? int64_t(0) : t->second;
    };
    if (job->filter.nav == "orphans") {
        out->orphans = catalog::orphans(c, v);
        for (uint32_t i = 0; i < out->orphans.size(); ++i) {
            const std::string& name = out->orphans[i];
            if (!job->filter.text.empty() && name.find(job->filter.text) == std::string::npos) continue;
            out->rows.push_back(Row{Row::kNoFile, i, 3, modelCode(modelOf(name)), timeOf(name)});
        }
    } else {
        for (uint32_t i = 0; i < c.files.size(); ++i)
            for (uint32_t j = 0; j < c.files[i].pictures.size(); ++j) {
                const catalog::PictureOf& p = c.files[i].pictures[j];
                const std::string name = pack::pictureName(pack::Group(p.group), p.key);
                RowFacts facts;
                facts.state = catalog::stateOf(p, v);
                facts.model = modelOf(name);
                facts.flags = job->thumbs->flagsOf(p.key);
                facts.overrides = v.index.materials.count(p.key) != 0;
                if (rowShown(c.files[i], p, facts, job->filter))
                    out->rows.push_back(Row{i, j, uint8_t(facts.state), modelCode(facts.model), timeOf(name)});
            }
    }
    // The order asked for; the file's name breaks ties (and is the order of the orphans' names).
    const auto name = [&](const Row& r) -> const std::string& {
        return r.file == Row::kNoFile ? out->orphans[r.picture] : c.files[r.file].name;
    };
    const auto size = [&](const Row& r) {
        if (r.file == Row::kNoFile) return 0;
        const catalog::PictureOf& p = c.files[r.file].pictures[r.picture];
        return int(p.width) * int(p.height);
    };
    const int col = job->sortColumn;
    std::stable_sort(out->rows.begin(), out->rows.end(), [&](const Row& a, const Row& b) {
        if (col == 2 && a.state != b.state) return a.state < b.state;
        if (col == 3 && a.model != b.model) return a.model < b.model;
        if (col == 6 && size(a) != size(b)) return size(a) < size(b);
        if (col == 7 && a.made != b.made) return a.made < b.made;
        if (name(a) != name(b)) return name(a) < name(b);
        return a.picture < b.picture;
    });
    if (job->sortDown) std::reverse(out->rows.begin(), out->rows.end());
    PostMessageW(job->notify, kRowsReady, WPARAM(job->generation), LPARAM(out.release()));
    return 0;
}

}  // namespace

TexturesTab::TexturesTab(HWND parent, settings::Lang lang, HFONT font, int dpi, std::wstring gameDir,
                         CatalogWork& catalog, Thumbs& thumbs)
    : parent_(parent), lang_(lang), font_(font), dpi_(dpi), gameDir_(std::move(gameDir)), catalog_(catalog),
      thumbs_(thumbs) {
    const auto make = [&](const wchar_t* cls, DWORD style, DWORD ex, int id) {
        HWND h = CreateWindowExW(ex, cls, L"", WS_CHILD | style, 0, 0, 10, 10, parent_,
                                 reinterpret_cast<HMENU>(INT_PTR(id)), nullptr, nullptr);
        SendMessageW(h, WM_SETFONT, WPARAM(font_), TRUE);
        return h;
    };
    tree_ = make(WC_TREEVIEWW, WS_TABSTOP | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                 WS_EX_CLIENTEDGE, kTree);
    filter_ = make(L"EDIT", WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, kFilter);
    SendMessageW(filter_, EM_SETCUEBANNER, TRUE, LPARAM(t(settings::Text::StudioFilterCue).c_str()));
    list_ = make(WC_LISTVIEWW, WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, kList);
    count_ = make(L"STATIC", SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE | SS_ENDELLIPSIS, 0, kCount);
    SetWindowTextW(count_, t(settings::Text::StudioCatalogEmpty).c_str());
    ListView_SetExtendedListViewStyle(list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);
    const int rowH = Thumbs::kSide + 4;  // the rows as tall as a thumbnail: by an image list that tall
    rowHeight_ = ImageList_Create(1, rowH, ILC_COLOR32, 0, 1);
    ListView_SetImageList(list_, rowHeight_, LVSIL_SMALL);
    const settings::Text names[kColumns] = {
        settings::Text::StudioColPicture, settings::Text::StudioColFile,  settings::Text::StudioColState,
        settings::Text::StudioColModel,   settings::Text::StudioColFlags, settings::Text::StudioColDetail,
        settings::Text::StudioColSize,    settings::Text::StudioColDate};
    const int widths[kColumns] = {Thumbs::kSide * 2 + 8, 330, 90, 70, 150, 80, 80, 120};
    for (int i = 0; i < kColumns; ++i) {
        std::wstring text = t(names[i]);
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = &text[0];
        col.cx = i ? MulDiv(widths[i], dpi_, 96) : widths[i];
        ListView_InsertColumn(list_, i, &col);
    }
}

std::wstring TexturesTab::t(settings::Text x) const { return app::wtr(lang_, x); }

void TexturesTab::show(bool on) {
    for (HWND h : {tree_, filter_, list_, count_}) ShowWindow(h, on ? SW_SHOW : SW_HIDE);
}

void TexturesTab::layout(const RECT& a) {
    const int navW = MulDiv(240, dpi_, 96), gap = MulDiv(6, dpi_, 96), editH = MulDiv(24, dpi_, 96);
    MoveWindow(tree_, a.left, a.top, navW, a.bottom - a.top, TRUE);
    const int listW = a.right - a.left - navW - gap, editW = std::min(listW / 2, MulDiv(360, dpi_, 96));
    MoveWindow(filter_, a.left + navW + gap, a.top, editW, editH, TRUE);
    MoveWindow(count_, a.left + navW + gap * 3 + editW, a.top, listW - editW - gap * 2, editH, TRUE);
    MoveWindow(list_, a.left + navW + gap, a.top + editH + gap, a.right - a.left - navW - gap,
               a.bottom - a.top - editH - gap, TRUE);
}

void TexturesTab::catalogChanged() {
    const auto c = catalog_.catalog();
    if (c != shown_) {
        shown_ = c;
        thumbs_.forget();
        fillTree();
    }
    rebuild();
}

void TexturesTab::fillTree() {
    TreeView_DeleteAllItems(tree_);
    navs_.clear();
    const auto add = [&](HTREEITEM parent, const std::wstring& text, const std::string& nav) {
        navs_.push_back(nav);
        TVINSERTSTRUCTW in{};
        in.hParent = parent;
        in.hInsertAfter = TVI_LAST;
        in.item.mask = TVIF_TEXT | TVIF_PARAM;
        in.item.pszText = const_cast<wchar_t*>(text.c_str());
        in.item.lParam = LPARAM(navs_.size() - 1);
        return TreeView_InsertItem(tree_, &in);
    };
    const HTREEITEM all = add(TVI_ROOT, t(settings::Text::StudioNavAll), "");
    all_ = all;
    const HTREEITEM states = add(TVI_ROOT, t(settings::Text::StudioNavStates), "");
    add(states, t(settings::Text::StudioNavMissing), "state:missing");
    add(states, t(settings::Text::StudioNavInPack), "state:inpack");
    add(states, t(settings::Text::StudioNavOwn), "state:own");
    add(states, t(settings::Text::StudioNavFlagged), "flagged");
    add(states, t(settings::Text::StudioNavOverrides), "overrides");
    add(states, t(settings::Text::StudioNavOrphans), "orphans");
    const HTREEITEM models = add(TVI_ROOT, t(settings::Text::StudioNavModels), "");
    for (int i = 1; i < 4; ++i) add(models, app::wide(kModels[i]), std::string("model:") + kModels[i]);
    // What the game drew (uaro-graphics\seen\<map>.txt, the driver's reports)
    const HTREEITEM seen = add(TVI_ROOT, t(settings::Text::StudioNavReports), "");
    WIN32_FIND_DATAW f{};
    HANDLE find = FindFirstFileW((gameDir_ + L"\\uaro-graphics\\seen\\*.txt").c_str(), &f);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            std::wstring map = f.cFileName;
            map.resize(map.size() - 4);
            add(seen, map, "seen:" + app::utf8(map));
        } while (FindNextFileW(find, &f));
        FindClose(find);
    }
    if (shown_) {
        const HTREEITEM maps = add(TVI_ROOT, t(settings::Text::StudioNavMaps), "");
        for (size_t i = 0; i < shown_->maps.size(); ++i)
            add(maps, fromCp949(shown_->maps[i]), "map:" + std::to_string(i));
        // the folders right under data\texture\ and data\sprite\ (the game's names: CP949)
        std::vector<std::string> folders;
        for (const catalog::FileEntry& file : shown_->files)
            for (const char* top : {"data\\texture\\", "data\\sprite\\"}) {
                const size_t len = std::char_traits<char>::length(top);
                if (file.name.compare(0, len, top) != 0) continue;
                const size_t slash = file.name.find('\\', len);
                if (slash != std::string::npos) folders.push_back(file.name.substr(0, slash + 1));
            }
        std::sort(folders.begin(), folders.end());
        folders.erase(std::unique(folders.begin(), folders.end()), folders.end());
        const HTREEITEM tops = add(TVI_ROOT, t(settings::Text::StudioNavFolders), "");
        for (const std::string& folder : folders) add(tops, fromCp949(folder.substr(5)), "folder:" + folder);
    }
    TreeView_SelectItem(tree_, all);
}

void TexturesTab::rebuild() {
    if (!shown_) {
        rows_.clear();
        ListView_SetItemCountEx(list_, 0, 0);
        return;
    }
    auto job = std::make_unique<Rebuild>();
    job->notify = parent_;
    job->generation = ++generation_;
    job->cat = shown_;
    job->view = catalog_.packView();
    job->filter = filter_now_;
    job->thumbs = &thumbs_;
    job->sortColumn = sortColumn_;
    job->sortDown = sortDown_;
    if (HANDLE h = CreateThread(nullptr, 0, rebuildMain, job.get(), 0, nullptr)) {
        job.release();
        CloseHandle(h);
    }
}

void TexturesTab::rowsReady(uint64_t generation, RowsResult* result) {
    std::unique_ptr<RowsResult> r(result);
    if (generation != generation_) return;  // an older filter's
    rows_ = std::move(r->rows);
    orphans_ = std::move(r->orphans);
    ListView_SetItemCountEx(list_, int(rows_.size()), LVSICF_NOSCROLL);
    if (selectFirst_ && !rows_.empty()) {  // a texture picked in the game: shown at once
        selectFirst_ = false;
        ListView_SetItemState(list_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_SetItemState(list_, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(list_, 0, FALSE);
        if (onSelect) onSelect(rows_[0]);
    }
    size_t all = 0;
    for (const catalog::FileEntry& f : shown_->files) all += f.pictures.size();
    SetWindowTextW(count_, app::wtrf(lang_, settings::Text::StudioRowsCount,
                                     {std::to_string(rows_.size()), std::to_string(all)}).c_str());
    InvalidateRect(list_, nullptr, FALSE);
}

void TexturesTab::thumbsReady() { InvalidateRect(list_, nullptr, FALSE); }

std::vector<Row> TexturesTab::selection() const {
    std::vector<Row> out;
    int i = -1;
    while ((i = ListView_GetNextItem(list_, i, LVNI_SELECTED)) >= 0)
        if (size_t(i) < rows_.size()) out.push_back(rows_[size_t(i)]);
    return out;
}

std::wstring TexturesTab::cell(const Row& r, int column) const {
    const bool orphan = r.file == Row::kNoFile;
    if (!shown_ || (!orphan && r.file >= shown_->files.size()) || (orphan && r.picture >= orphans_.size())) return L"";
    const catalog::FileEntry* f = orphan ? nullptr : &shown_->files[r.file];
    const catalog::PictureOf* p = orphan ? nullptr : &f->pictures[r.picture];
    switch (column) {
        case 1: {
            if (orphan) return app::wide(orphans_[r.picture]);
            std::wstring name = fromCp949(f->name.compare(0, 5, "data\\") == 0 ? f->name.substr(5) : f->name);
            if (!p->palette.empty()) {  // a colour of the server's: its palette's number
                const size_t under = p->palette.rfind('_'), dot = p->palette.rfind('.');
                if (under != std::string::npos && dot > under)
                    name += L" #" + app::wide(p->palette.substr(under + 1, dot - under - 1));
            }
            return name;
        }
        case 2: {
            const settings::Text s[4] = {settings::Text::StudioStateMissing, settings::Text::StudioStateInPack,
                                         settings::Text::StudioStateOwn, settings::Text::StudioStateOrphan};
            return t(s[std::min<int>(r.state, 3)]);
        }
        case 3:
            return r.model == 4 ? L"?" : app::wide(kModels[r.model]);
        case 4:
        case 5: {
            if (orphan) return L"";
            const auto thumb = thumbs_.get(r.file, r.picture);
            if (!thumb || !thumb->packed) return L"";
            if (column == 5) return std::to_wstring(int(thumb->metrics.detail * 100.0f + 0.5f)) + L"%";
            const settings::Text names[ai::kFlags] = {
                settings::Text::StudioFlagSmoothed, settings::Text::StudioFlagWashed, settings::Text::StudioFlagBanded,
                settings::Text::StudioFlagRinging,  settings::Text::StudioFlagFringe, settings::Text::StudioFlagAlpha,
                settings::Text::StudioFlagSeams};
            std::wstring s;
            for (int i = 0; i < ai::kFlags; ++i)
                if (thumb->flags & (1u << i)) s += (s.empty() ? L"" : L", ") + t(names[i]);
            return s;
        }
        case 6:
            return orphan ? L"" : std::to_wstring(p->width) + L"×" + std::to_wstring(p->height);
        case 7: {
            if (!r.made) return L"";
            const time_t when = time_t(r.made);
            tm local{};
            localtime_s(&local, &when);
            wchar_t s[32];
            wcsftime(s, 32, L"%Y-%m-%d %H:%M", &local);
            return s;
        }
    }
    return L"";
}

void TexturesTab::drawThumb(const NMLVCUSTOMDRAW* d, const Row& r) {
    RECT cellRect{};
    cellRect.top = 0;  // the subitem's own rectangle (column 0: its label part)
    cellRect.left = LVIR_LABEL;
    SendMessageW(list_, LVM_GETSUBITEMRECT, d->nmcd.dwItemSpec, LPARAM(&cellRect));
    FillRect(d->nmcd.hdc, &cellRect, GetSysColorBrush(COLOR_WINDOW));
    if (r.file == Row::kNoFile) return;
    const auto thumb = thumbs_.get(r.file, r.picture);
    if (!thumb) return;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = Thumbs::kSide * 2;
    bi.bmiHeader.biHeight = -Thumbs::kSide;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(d->nmcd.hdc, cellRect.left + 2, cellRect.top + 2, Thumbs::kSide * 2, Thumbs::kSide, 0, 0,
                  Thumbs::kSide * 2, Thumbs::kSide, thumb->pair.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
}

bool TexturesTab::notify(const NMHDR* n, LRESULT& result) {
    if (n->hwndFrom == tree_ && n->code == TVN_SELCHANGEDW) {
        const auto* tv = reinterpret_cast<const NMTREEVIEWW*>(n);
        const size_t i = size_t(tv->itemNew.lParam);
        filter_now_.nav = i < navs_.size() ? navs_[i] : "";
        filter_now_.keys.clear();
        if (filter_now_.nav.compare(0, 5, "seen:") == 0) {  // the report's keys
            std::string text, map;
            std::vector<Seen> seen;
            const std::wstring report =
                gameDir_ + L"\\uaro-graphics\\seen\\" + app::wide(filter_now_.nav.substr(5)) + L".txt";
            if (win::readTextFile(app::ansi(report), text) && parseSeen(text, map, seen))
                for (const Seen& s : seen) filter_now_.keys.insert(s.key);
        }
        rebuild();
        result = 0;
        return true;
    }
    if (n->hwndFrom != list_) return false;
    switch (n->code) {
        case LVN_GETDISPINFOW: {
            auto* di = reinterpret_cast<NMLVDISPINFOW*>(const_cast<NMHDR*>(n));
            if ((di->item.mask & LVIF_TEXT) && di->item.iItem >= 0 && size_t(di->item.iItem) < rows_.size()) {
                const std::wstring text = di->item.iSubItem ? cell(rows_[size_t(di->item.iItem)], di->item.iSubItem)
                                                            : std::wstring();
                lstrcpynW(di->item.pszText, text.c_str(), di->item.cchTextMax);
            }
            result = 0;
            return true;
        }
        case NM_CUSTOMDRAW: {
            const auto* d = reinterpret_cast<const NMLVCUSTOMDRAW*>(n);
            if (d->nmcd.dwDrawStage == CDDS_PREPAINT) result = CDRF_NOTIFYITEMDRAW;
            else if (d->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) result = CDRF_NOTIFYSUBITEMDRAW;
            else if (d->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM) && d->iSubItem == 0 &&
                     d->nmcd.dwItemSpec < rows_.size()) {
                drawThumb(d, rows_[d->nmcd.dwItemSpec]);
                result = CDRF_SKIPDEFAULT;
            } else {
                result = CDRF_DODEFAULT;
            }
            return true;
        }
        case LVN_ITEMCHANGED: {
            const auto* c = reinterpret_cast<const NMLISTVIEW*>(n);
            if (c->iItem >= 0 && size_t(c->iItem) < rows_.size() && (c->uNewState & LVIS_FOCUSED) &&
                !(c->uOldState & LVIS_FOCUSED) && onSelect)
                onSelect(rows_[size_t(c->iItem)]);
            result = 0;
            return true;
        }
        case LVN_COLUMNCLICK: {
            const int col = reinterpret_cast<const NMLISTVIEW*>(n)->iSubItem;
            sortDown_ = col == sortColumn_ ? !sortDown_ : false;
            sortColumn_ = col;
            rebuild();
            result = 0;
            return true;
        }
    }
    return false;
}

void TexturesTab::showKey(const std::string& key) {
    if (all_) TreeView_SelectItem(tree_, all_);  // its row wherever the navigator was
    filter_now_.nav.clear();
    filter_now_.keys.clear();
    filter_now_.text = key;
    selectFirst_ = true;
    SetWindowTextW(filter_, app::wide(key).c_str());
    KillTimer(parent_, kFilterTimer);  // the edit's change asks for a rebuild; this one is it
    rebuild();
}

void TexturesTab::command(int id, int code) {
    if (id == kFilter && code == EN_CHANGE) SetTimer(parent_, kFilterTimer, 250, nullptr);
}

void TexturesTab::timer(UINT_PTR id) {
    if (id != kFilterTimer) return;
    KillTimer(parent_, kFilterTimer);
    wchar_t buf[256] = {};
    GetWindowTextW(filter_, buf, 256);
    filter_now_.text = toCp949(buf);
    rebuild();
}

}  // namespace uagfx::studio

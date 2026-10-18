#include "studio/inspector.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "app/text.hpp"
#include "core/ai/picture_metrics.hpp"
#include "core/studio_link.hpp"
#include "core/webp_image.hpp"
#include "studio/catalog_work.hpp"
#include "studio/making.hpp"
#include "studio/messages.hpp"

namespace uagfx::studio {
namespace {

std::wstring fromCp949(const std::string& s) {
    const int n = MultiByteToWideChar(949, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(949, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

std::string number(double v, int digits) {
    char s[32];
    std::snprintf(s, sizeof s, "%.*f", digits, v);
    return s;
}

std::string dateOf(int64_t t) {
    if (t <= 0) return "?";
    const time_t when = time_t(t);
    tm local{};
    localtime_s(&local, &when);
    char s[32];
    std::strftime(s, sizeof s, "%Y-%m-%d %H:%M", &local);
    return s;
}

struct Load {
    HWND notify = nullptr;
    uint64_t generation = 0;
    Row row;
    std::string orphan;
    settings::Lang lang = settings::Lang::En;
    CatalogWork* catalog = nullptr;
};

DWORD WINAPI loadMain(void* arg) {
    std::unique_ptr<Load> job(static_cast<Load*>(arg));
    auto out = std::make_unique<Shown>();
    out->row = job->row;
    CatalogWork& work = *job->catalog;
    const settings::Lang l = job->lang;
    const auto cat = work.catalog();
    const auto view = work.packView();
    pack::Reader reader;
    std::string error;
    reader.open(app::ansi(work.packPath()), error, false);
    std::wstring info;
    const auto line = [&info](const std::wstring& s) { info += s + L"\r\n"; };
    if (job->row.file == Row::kNoFile) {  // a picture no texture names
        line(app::wtrf(l, settings::Text::StudioInfoOrphan, {job->orphan}));
        std::vector<uint8_t> bytes;
        auto packed = std::make_shared<Image>();
        if (reader.read(job->orphan, bytes) && webp::decode(bytes.data(), bytes.size(), packed->argb, packed->width,
                                                            packed->height))
            out->packed = packed;
    } else if (cat && job->row.file < cat->files.size()) {
        const catalog::FileEntry& f = cat->files[job->row.file];
        const catalog::PictureOf& p = f.pictures[job->row.picture];
        auto original = std::make_shared<Image>();
        if (originalOf(work, *cat, f, p, *original)) out->original = original;
        out->key = p.key;
        out->file = f.name;
        std::string bytes;  // a picture made again before and not kept or discarded yet
        auto made = std::make_shared<Image>();
        if (std::FILE* m = _wfopen(madeFile(p.key).c_str(), L"rb")) {
            char buf[1 << 16];
            size_t n = 0;
            while ((n = std::fread(buf, 1, sizeof buf, m)) > 0) bytes.append(buf, n);
            std::fclose(m);
            if (parseRaw(bytes, made->width, made->height, made->argb)) out->made = made;
        }
        auto packed = std::make_shared<Image>();
        if (view && reader.isOpen() && packPictureOf(reader, p, view->index, *packed)) out->packed = packed;
        line(app::wtrf(l, settings::Text::StudioInfoFile, {app::utf8(fromCp949(f.name))}));
        if (!f.archives.empty() && f.archives[0] < cat->archives.size())
            line(app::wtrf(l, settings::Text::StudioInfoArchive, {cat->archives[f.archives[0]].path}));
        std::string shadowed;
        for (size_t i = 1; i < f.archives.size(); ++i)
            if (f.archives[i] < cat->archives.size())
                shadowed += (shadowed.empty() ? "" : ", ") + cat->archives[f.archives[i]].path;
        if (!shadowed.empty()) line(app::wtrf(l, settings::Text::StudioInfoShadowed, {shadowed}));
        if (!p.palette.empty()) line(app::wtrf(l, settings::Text::StudioInfoColour, {app::utf8(fromCp949(p.palette))}));
        line(app::wtrf(l, settings::Text::StudioInfoKey, {p.key}));
        std::string low;
        for (const std::string& k : p.lowKeys) low += (low.empty() ? "" : ", ") + k;
        if (!low.empty()) line(app::wtrf(l, settings::Text::StudioInfoLow, {low}));
        const std::string name = pack::pictureName(pack::Group(p.group), p.key);
        out->own = view && view->index.ownAlways.count(p.key);
        if (out->own) line(app::wtr(l, settings::Text::StudioInfoOwn));
        if (out->packed) {
            line(app::wtrf(l, settings::Text::StudioInfoPicture,
                           {name, std::to_string(out->packed->width), std::to_string(out->packed->height)}));
            std::string model, chain = "?";
            const auto e = view->index.pictures.find(name);
            if (e != view->index.pictures.end()) model = e->second.model;
            const auto m = view->index.made.find(name);
            if (m != view->index.made.end()) {
                model = m->second.model;
                chain = m->second.chain;
            }
            const auto at = view->pictures.find(name);
            line(app::wtrf(l, settings::Text::StudioInfoMade,
                           {dateOf(at == view->pictures.end() ? 0 : at->second), model.empty() ? "?" : model, chain}));
            if (m != view->index.made.end())
                line(app::wtrf(l, settings::Text::StudioInfoFrom,
                               {app::utf8(fromCp949(m->second.file)), m->second.archive}));
            if (out->original) {
                const ai::Metrics x = ai::measure(out->original->argb.data(), out->original->width,
                                                  out->original->height, out->packed->argb.data(), out->packed->width,
                                                  out->packed->height, p.group == uint8_t(pack::Group::World));
                line(app::wtrf(l, settings::Text::StudioInfoMetrics,
                               {number(x.detail * 100, 0), number(x.tone, 3), number(x.banding * 100, 0),
                                number(x.ringing * 100, 1), number(x.fringe * 100, 1), number(x.alpha * 100, 1),
                                number(x.seams, 2)}));
            }
        } else {
            line(app::wtr(l, settings::Text::StudioInfoNoPicture));
        }
        if (view) {
            const auto mat = view->index.materials.find(p.key);
            if (mat != view->index.materials.end())
                line(app::wtrf(l, settings::Text::StudioInfoMaterial,
                               {number(mat->second.gloss, 2), number(mat->second.relief, 2),
                                number(mat->second.height, 2)}));
        }
        if (!f.maps.empty()) {
            std::string maps;
            for (size_t i = 0; i < f.maps.size() && i < 30; ++i) maps += (i ? ", " : "") + cat->maps[f.maps[i]];
            if (f.maps.size() > 30) maps += ", ...";
            line(app::wtrf(l, settings::Text::StudioInfoMaps, {std::to_string(f.maps.size()), maps}));
            // the map's own cells where it lies most (its .gnd): the compare's "as on the map"
            std::vector<uint8_t> gnd;
            if (work.read("data\\" + cat->maps[f.maps[0]] + ".gnd", gnd))
                out->patch = map_textures::groundPatch(gnd.data(), gnd.size(), f.name, 8);
        }
        if (f.grid.cols > 1 || f.grid.rows > 1)
            line(app::wtrf(l, settings::Text::StudioInfoAtlas,
                           {std::to_string(f.grid.cols), std::to_string(f.grid.rows)}));
        else if (!f.maps.empty() && f.type == uint8_t(pack::Source::Texture))
            line(app::wtr(l, settings::Text::StudioInfoOnePicture));
    }
    out->info = std::move(info);
    PostMessageW(job->notify, kShownReady, WPARAM(job->generation), LPARAM(out.release()));
    return 0;
}

}  // namespace

Inspector::Inspector(HWND parent, settings::Lang lang, HFONT font, int dpi, std::wstring gameDir,
                     CatalogWork& catalog)
    : parent_(parent), lang_(lang), font_(font), dpi_(dpi), gameDir_(std::move(gameDir)), catalog_(catalog) {
    const auto make = [&](const wchar_t* cls, const std::wstring& text, DWORD style, DWORD ex, int id) {
        HWND h = CreateWindowExW(ex, cls, text.c_str(), WS_CHILD | style, 0, 0, 10, 10, parent_,
                                 reinterpret_cast<HMENU>(INT_PTR(id)), nullptr, nullptr);
        SendMessageW(h, WM_SETFONT, WPARAM(font_), TRUE);
        controls_.push_back(h);
        return h;
    };
    const settings::Text modes[5] = {settings::Text::StudioViewPair, settings::Text::StudioViewWipe,
                                     settings::Text::StudioViewTiled, settings::Text::StudioViewMap,
                                     settings::Text::StudioViewDiff};
    for (int i = 0; i < 5; ++i)
        modes_[i] = make(L"BUTTON", t(modes[i]), BS_AUTORADIOBUTTON | BS_PUSHLIKE | (i ? 0 : WS_GROUP), 0, kPair + i);
    SendMessageW(modes_[0], BM_SETCHECK, BST_CHECKED, 0);
    smooth_ = make(L"BUTTON", t(settings::Text::StudioViewSmooth), BS_AUTOCHECKBOX | WS_GROUP, 0, kSmooth);
    versus_ = make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL, 0, kVersus);
    SendMessageW(versus_, CB_ADDSTRING, 0, LPARAM(t(settings::Text::StudioVsPack).c_str()));
    SendMessageW(versus_, CB_ADDSTRING, 0, LPARAM(t(settings::Text::StudioVsMade).c_str()));
    SendMessageW(versus_, CB_SETCURSEL, 0, 0);
    CompareView::registerClass(GetModuleHandleW(nullptr));
    controls_.push_back(view_.create(parent_, kView));
    info_ = make(L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, kInfo);
    const settings::Text actions[5] = {settings::Text::StudioMakeAgain, settings::Text::StudioKeep,
                                       settings::Text::StudioDiscard, settings::Text::StudioTakeOut,
                                       settings::Text::StudioOwnAlways};
    for (int i = 0; i < 5; ++i) actions_[i] = make(L"BUTTON", t(actions[i]), BS_PUSHBUTTON, 0, kMake + i);
    for (int i = 1; i < 3; ++i) EnableWindow(actions_[i], FALSE);  // Keep, Discard: with a picture made again
}

std::wstring Inspector::t(settings::Text x) const { return app::wtr(lang_, x); }

void Inspector::show(bool on) {
    for (HWND h : controls_) ShowWindow(h, on ? SW_SHOW : SW_HIDE);
}

void Inspector::layout(const RECT& r) {
    const auto at = [&](int v) { return MulDiv(v, dpi_, 96); };
    const int left = int(r.left), top = int(r.top), right = int(r.right), bottom = int(r.bottom);
    const int w = right - left, row = at(24), gap = at(6);
    int x = left, y = top;
    const int modeW = std::max(at(60), (w - at(220)) / 5);
    for (HWND m : modes_) {
        MoveWindow(m, x, y, modeW - at(2), row, TRUE);
        x += modeW;
    }
    MoveWindow(smooth_, x + gap, y, at(90), row, TRUE);
    MoveWindow(versus_, x + gap + at(92), y, std::max(at(60), right - x - gap - at(92)), at(200), TRUE);
    y += row + gap;
    const int actionsH = row, infoH = std::max(at(120), (bottom - top) / 4);
    const int viewH = std::max(at(100), bottom - y - infoH - actionsH - gap * 2);
    MoveWindow(view_.hwnd(), left, y, w, viewH, TRUE);
    y += viewH + gap;
    MoveWindow(info_, left, y, w, infoH, TRUE);
    y += infoH + gap;
    const int actionW = w / 5;
    for (int i = 0; i < 5; ++i) MoveWindow(actions_[i], left + i * actionW, y, actionW - at(4), row, TRUE);
}

void Inspector::showRow(const Row& row, const std::vector<std::string>& orphans) {
    auto job = std::make_unique<Load>();
    job->notify = parent_;
    job->generation = ++generation_;
    job->row = row;
    if (row.file == Row::kNoFile && row.picture < orphans.size()) job->orphan = orphans[row.picture];
    job->lang = lang_;
    job->catalog = &catalog_;
    made_.reset();
    for (int i = 1; i < 3; ++i) EnableWindow(actions_[i], FALSE);
    if (HANDLE h = CreateThread(nullptr, 0, loadMain, job.get(), 0, nullptr)) {
        job.release();
        CloseHandle(h);
    }
}

void Inspector::shownReady(uint64_t generation, Shown* shown) {
    std::unique_ptr<Shown> s(shown);
    if (generation != generation_) return;  // another row was asked for meanwhile
    shown_ = std::move(s);
    SetWindowTextW(info_, shown_->info.c_str());
    const settings::Text own = shown_->own ? settings::Text::StudioOwnNot : settings::Text::StudioOwnAlways;
    SetWindowTextW(actions_[4], t(own).c_str());
    view_.setPatch(shown_->patch);
    setMade(shown_->made);
}

void Inspector::refresh(const std::vector<std::string>& orphans) {
    if (shown_) showRow(shown_->row, orphans);
}

void Inspector::setMade(std::shared_ptr<const Image> made) {
    made_ = std::move(made);
    for (int i = 1; i < 3; ++i) EnableWindow(actions_[i], made_ != nullptr);
    SendMessageW(versus_, CB_SETCURSEL, made_ ? 1 : 0, 0);
    showCompare();
}

void Inspector::showCompare() {
    if (!shown_) return;
    const bool versusMade = SendMessageW(versus_, CB_GETCURSEL, 0, 0) == 1 && made_;
    view_.set(shown_->original ? shown_->original : shown_->packed, versusMade ? made_ : shown_->packed);
}

bool Inspector::command(int id, int code) {
    if (id >= kPair && id <= kDiff && code == BN_CLICKED) {
        view_.setMode(CompareView::Mode(id - kPair));
        return true;
    }
    if (id == kSmooth && code == BN_CLICKED) {
        view_.setSmooth(SendMessageW(smooth_, BM_GETCHECK, 0, 0) == BST_CHECKED);
        return true;
    }
    if (id == kVersus && code == CBN_SELCHANGE) {
        showCompare();
        return true;
    }
    if (code != BN_CLICKED) return false;
    const std::function<void()>* action = id == kMake      ? &onMake
                                          : id == kKeep    ? &onKeep
                                          : id == kDiscard ? &onDiscard
                                          : id == kTakeOut ? &onTakeOut
                                          : id == kOwn     ? &onOwn
                                                           : nullptr;
    if (action && *action) (*action)();
    return action != nullptr;
}

}  // namespace uagfx::studio

#include "studio/pack_tab.hpp"

#include <cstdio>
#include <ctime>

#include "app/text.hpp"
#include "core/pack_overview.hpp"
#include "studio/catalog_work.hpp"
#include "studio/jobs.hpp"
#include "studio/messages.hpp"

namespace uagfx::studio {
namespace {

struct Count {
    HWND notify = nullptr;
    settings::Lang lang = settings::Lang::En;
    CatalogWork* catalog = nullptr;
};

std::string listOf(const std::map<std::string, uint32_t>& m) {
    std::string s;
    for (const auto& e : m) s += (s.empty() ? "" : ", ") + e.first + " " + std::to_string(e.second);
    return s;
}

DWORD WINAPI countMain(void* arg) {
    std::unique_ptr<Count> job(static_cast<Count*>(arg));
    const settings::Lang l = job->lang;
    auto text = std::make_unique<std::wstring>();
    const std::string path = app::ansi(job->catalog->packPath());
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        *text = app::wtr(l, settings::Text::StudioPackNone);
    } else {
        _fseeki64(f, 0, SEEK_END);
        const uint64_t size = uint64_t(_ftelli64(f));
        std::vector<pack::ZipEntry> entries;
        std::string error;
        const auto read = [f](uint64_t at, uint8_t* out, size_t n) {
            return _fseeki64(f, int64_t(at), SEEK_SET) == 0 && std::fread(out, 1, n, f) == n;
        };
        pack::centralDirectoryOf(size, read, entries, error);
        std::fclose(f);
        const auto cat = job->catalog->catalog();
        const auto view = job->catalog->packView();
        uint32_t orphans = 0, missing = 0;
        if (cat && view) {
            orphans = uint32_t(catalog::orphans(*cat, *view).size());
            for (const catalog::FileEntry& e : cat->files)
                for (const catalog::PictureOf& p : e.pictures)
                    missing += catalog::stateOf(p, *view) == catalog::State::Missing;
        }
        const pack::Overview o = pack::overviewOf(entries, size, view ? view->index : pack::Index(), orphans, missing);
        const auto line = [&](settings::Text x, const std::vector<std::string>& args) {
            *text += app::wtrf(l, x, args) + L"\r\n";
        };
        line(settings::Text::StudioPackPictures,
             {std::to_string(o.pictures), std::to_string(o.orphans), std::to_string(o.missing)});
        line(settings::Text::StudioPackSize, {app::sizeText(o.bytes, l), app::sizeText(o.unread, l)});
        line(settings::Text::StudioPackByGroup, {listOf(o.byGroup)});
        line(settings::Text::StudioPackByModel, {listOf(o.byModel)});
        line(settings::Text::StudioPackByKind, {listOf(o.byKind)});
    }
    PostMessageW(job->notify, kPackReady, 0, LPARAM(text.release()));
    return 0;
}

}  // namespace

PackTab::PackTab(HWND parent, settings::Lang lang, HFONT font, int dpi, CatalogWork& catalog, Jobs& jobs)
    : parent_(parent), lang_(lang), font_(font), dpi_(dpi), catalog_(catalog), jobs_(jobs) {
    const auto make = [&](const wchar_t* cls, const std::wstring& text, DWORD style, DWORD ex, int id) {
        HWND h = CreateWindowExW(ex, cls, text.c_str(), WS_CHILD | style, 0, 0, 10, 10, parent_,
                                 reinterpret_cast<HMENU>(INT_PTR(id)), nullptr, nullptr);
        SendMessageW(h, WM_SETFONT, WPARAM(font_), TRUE);
        return h;
    };
    text_ = make(L"EDIT", L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, WS_EX_CLIENTEDGE, kText);
    read_ = make(L"BUTTON", t(settings::Text::StudioPackRead), BS_PUSHBUTTON | WS_TABSTOP, 0, kRead);
    compact_ = make(L"BUTTON", t(settings::Text::StudioPackCompact), BS_PUSHBUTTON | WS_TABSTOP, 0, kCompact);
    note_ = make(L"STATIC", t(settings::Text::StudioPackCompactNote), SS_LEFT, 0, kNote);
}

std::wstring PackTab::t(settings::Text x) const { return app::wtr(lang_, x); }

void PackTab::show(bool on) {
    for (HWND h : {text_, read_, compact_, note_}) ShowWindow(h, on ? SW_SHOW : SW_HIDE);
    if (on) refresh();
}

void PackTab::layout(const RECT& a) {
    const auto at = [&](int v) { return MulDiv(v, dpi_, 96); };
    const int w = int(a.right - a.left), row = at(26);
    MoveWindow(text_, a.left, a.top, w, at(140), TRUE);
    MoveWindow(read_, a.left, a.top + at(150), at(160), row, TRUE);
    MoveWindow(compact_, a.left + at(170), a.top + at(150), at(160), row, TRUE);
    MoveWindow(note_, a.left, a.top + at(186), std::min(w, at(700)), at(60), TRUE);
}

void PackTab::refresh() {
    if (busy_) return;
    auto job = std::make_unique<Count>();
    job->notify = parent_;
    job->lang = lang_;
    job->catalog = &catalog_;
    if (HANDLE h = CreateThread(nullptr, 0, countMain, job.get(), 0, nullptr)) {
        job.release();
        busy_ = true;
        CloseHandle(h);
    }
}

void PackTab::ready(std::wstring* text) {
    std::unique_ptr<std::wstring> t0(text);
    busy_ = false;
    SetWindowTextW(text_, t0->c_str());
}

bool PackTab::command(int id, int code) {
    if (code != BN_CLICKED) return false;
    if (id == kRead) {
        catalog_.readPack();
        refresh();
        return true;
    }
    if (id == kCompact) {
        jobs_.add("compact", json::Value::object(), true, app::utf8(t(settings::Text::StudioJobCompact)));
        return true;
    }
    return false;
}

LogTab::LogTab(HWND parent, HFONT font) {
    edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                            WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 10, 10, parent,
                            nullptr, nullptr, nullptr);
    SendMessageW(edit_, WM_SETFONT, WPARAM(font), TRUE);
    SendMessageW(edit_, EM_SETLIMITTEXT, 1 << 22, 0);
}

void LogTab::show(bool on) { ShowWindow(edit_, on ? SW_SHOW : SW_HIDE); }

void LogTab::layout(const RECT& a) { MoveWindow(edit_, a.left, a.top, a.right - a.left, a.bottom - a.top, TRUE); }

void LogTab::add(const std::wstring& line) {
    if (++lines_ > 3000) {  // the oldest thousand go
        SetWindowTextW(edit_, L"");
        lines_ = 1;
    }
    const time_t now = std::time(nullptr);
    tm local{};
    localtime_s(&local, &now);
    wchar_t stamp[16];
    wcsftime(stamp, 16, L"%H:%M:%S ", &local);
    const std::wstring text = stamp + line + L"\r\n";
    const int end = GetWindowTextLengthW(edit_);
    SendMessageW(edit_, EM_SETSEL, WPARAM(end), LPARAM(end));
    SendMessageW(edit_, EM_REPLACESEL, FALSE, LPARAM(text.c_str()));
}

}  // namespace uagfx::studio

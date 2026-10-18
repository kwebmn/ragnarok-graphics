#include "app/palettes.hpp"

#include <windows.h>

#include <memory>
#include <vector>

#include "app/files.hpp"
#include "app/text.hpp"
#include "core/grf/grf.hpp"
#include "core/zip.hpp"

namespace uagfx::app {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
PalettesState g_state;

void set(const PalettesState& s) {
    AcquireSRWLockExclusive(&g_lock);
    g_state = s;
    ReleaseSRWLockExclusive(&g_lock);
}

struct Job {
    std::wstring source, out;
    bool folder = false;
};

// The client's names are cp949 (Korean Windows'): a folder's, from UTF-16; "" when one cannot be said so.
std::string cp949(const std::wstring& w) {
    if (w.empty()) return {};
    BOOL lost = FALSE;
    const int n = WideCharToMultiByte(949, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, &lost);
    std::string out(size_t(n > 0 ? n : 0), '\0');
    if (n <= 0 || WideCharToMultiByte(949, 0, w.c_str(), int(w.size()), &out[0], n, nullptr, &lost) != n || lost)
        return {};
    return out;
}

// A zip's name: UTF-8 when it reads as UTF-8 with more than ASCII in it (zips made on another system), else the
// client's own cp949 already.
std::string zipName(const std::string& raw) {
    bool ascii = true;
    for (char c : raw) ascii = ascii && (unsigned char)c < 0x80;
    if (ascii) return raw;
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, raw.data(), int(raw.size()), nullptr, 0);
    if (n <= 0) return raw;
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, raw.data(), int(raw.size()), &w[0], n);
    const std::string c = cp949(w);
    return c.empty() ? raw : c;
}

bool readWhole(const std::wstring& path, std::vector<uint8_t>& out) {
    out.clear();
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart < (LONGLONG(64) << 20);
    if (ok && size.QuadPart > 0) {
        out.resize(size_t(size.QuadPart));
        DWORD got = 0;
        ok = ReadFile(f, out.data(), DWORD(out.size()), &got, nullptr) && got == out.size();
    }
    CloseHandle(f);
    return ok;
}

// Every file under dir; rel is its path from the folder chosen (with "palette\" first when that is the folder).
void walk(const std::wstring& dir, const std::wstring& rel, palettes::Collector& c) {
    WIN32_FIND_DATAW d{};
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::wstring> folders;
    do {
        const std::wstring name = d.cFileName;
        if (name == L"." || name == L"..") continue;
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            folders.push_back(name);
            continue;
        }
        const std::string path = cp949(rel + name);
        if (path.empty() || !c.wants(path)) continue;
        std::vector<uint8_t> bytes;
        c.take(path, readWhole(dir + L"\\" + name, bytes) ? bytes : std::vector<uint8_t>());
    } while (FindNextFileW(h, &d));
    FindClose(h);
    for (const std::wstring& f : folders) walk(dir + L"\\" + f, rel + f + L"\\", c);
}

bool collect(const Job& j, palettes::Collector& c, std::string& error) {
    if (j.folder) {
        std::wstring dir = j.source;
        while (!dir.empty() && (dir.back() == L'\\' || dir.back() == L'/')) dir.pop_back();
        const size_t slash = dir.find_last_of(L"\\/");
        const std::wstring last = slash == std::wstring::npos ? dir : dir.substr(slash + 1);
        walk(dir, _wcsicmp(last.c_str(), L"palette") == 0 ? L"palette\\" : L"", c);
        return true;
    }
    const std::wstring ext = j.source.size() > 4 ? j.source.substr(j.source.size() - 4) : L"";
    if (_wcsicmp(ext.c_str(), L".zip") == 0) {
        zip::Index z;
        if (!z.open(ansi(j.source), [](const std::string&) { return true; }, error)) return false;
        std::vector<uint8_t> bytes;
        for (const std::string& raw : z.names()) {
            const std::string name = zipName(raw);
            if (!c.wants(name)) continue;
            std::string why;
            c.take(name, z.read(raw, bytes, why) ? bytes : std::vector<uint8_t>());
        }
        return true;
    }
    grf::Archive a;
    if (!a.openToRead(ansi(j.source), error)) return false;
    std::vector<uint8_t> bytes;
    for (const std::string& name : a.names()) {
        if (!c.wants(name)) continue;
        std::string why;
        c.take(name, a.read(name, bytes, why) ? bytes : std::vector<uint8_t>());
    }
    return true;
}

DWORD WINAPI work(void* arg) {
    const std::unique_ptr<Job> j(static_cast<Job*>(arg));
    const ULONGLONG started = GetTickCount64();
    PalettesState s;
    s.out = j->out;
    try {
        palettes::Collector c;
        std::string error;
        if (!collect(*j, c, error)) {
            s.kind = PalettesState::Failed;
            s.error = error.empty() ? "the source cannot be read" : error;
        } else if (!c.kept()) {
            s.kind = PalettesState::Failed;
            s.error = "no palette to keep in it";
        } else {
            const std::vector<uint8_t> grf = c.grf(s.blocks);
            if (!writeFileAtomic(j->out, std::string(grf.begin(), grf.end()))) {
                s.kind = PalettesState::Failed;
                s.error = "the file cannot be written (the game holds it while it runs)";
            } else {
                s.kind = PalettesState::Done;
                s.kept = uint32_t(c.kept());
                s.hair = uint32_t(c.hair());
                s.left = c.left();
                s.bytes = grf.size();
            }
        }
    } catch (...) {  // no memory: the settings app is a 32-bit program
        s.kind = PalettesState::Failed;
        s.error = "out of memory";
    }
    s.seconds = double(GetTickCount64() - started) / 1000.0;
    set(s);
    return 0;
}

}  // namespace

void startPalettes(const std::wstring& source, bool folder, const std::wstring& out) {
    if (palettesState().kind == PalettesState::Working) return;
    PalettesState s;
    s.kind = PalettesState::Working;
    s.out = out;
    set(s);
    auto* j = new Job{source, out, folder};
    HANDLE t = CreateThread(nullptr, 0, work, j, 0, nullptr);
    if (t) {
        CloseHandle(t);
        return;
    }
    delete j;
    s.kind = PalettesState::Failed;
    s.error = "no thread for it";
    set(s);
}

PalettesState palettesState() {
    AcquireSRWLockShared(&g_lock);
    const PalettesState s = g_state;
    ReleaseSRWLockShared(&g_lock);
    return s;
}

bool samePath(const std::wstring& a, const std::wstring& b) {
    wchar_t fa[MAX_PATH] = {}, fb[MAX_PATH] = {};
    if (!GetFullPathNameW(a.c_str(), MAX_PATH, fa, nullptr) || !GetFullPathNameW(b.c_str(), MAX_PATH, fb, nullptr))
        return _wcsicmp(a.c_str(), b.c_str()) == 0;
    return _wcsicmp(fa, fb) == 0;
}

}  // namespace uagfx::app

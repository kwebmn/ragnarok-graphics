#include "win/ui_font.hpp"

#include <cstring>
#include <utility>
#include <vector>

#include "win/log.hpp"

namespace uagfx::win::uifont {
namespace {

constexpr const wchar_t* kPlex = L"IBM Plex Sans";
constexpr const wchar_t* kPlexBold = L"IBM Plex Sans SmBld";  // the SemiBold's GDI family (its name table's 1)

struct Plex {
    bool ok = false;
    std::vector<std::pair<wchar_t, wchar_t>> letters;  // the ranges both faces have (GetFontUnicodeRanges)
};

Plex& plex() {
    static Plex p;
    return p;
}

// A TTF of the driver's resources added for this process; false: none there, or Windows would not take it
bool added(HMODULE module, const wchar_t* name) {
    HRSRC found = FindResourceW(module, name, MAKEINTRESOURCEW(10));  // RT_RCDATA
    HGLOBAL loaded = found ? LoadResource(module, found) : nullptr;
    void* data = loaded ? LockResource(loaded) : nullptr;
    const DWORD size = found ? SizeofResource(module, found) : 0;
    DWORD fonts = 0;
    return data && size && AddFontMemResourceEx(data, size, nullptr, &fonts) && fonts > 0;
}

// The face GDI gives for a name - Plex's own, not a stand-in for it
bool facesAs(HDC dc, const wchar_t* name) {
    HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, name);
    if (!font) return false;
    const HGDIOBJ was = SelectObject(dc, font);
    wchar_t face[LF_FACESIZE] = {};
    const bool same = GetTextFaceW(dc, LF_FACESIZE, face) > 0 && std::wcscmp(face, name) == 0;
    SelectObject(dc, was);
    DeleteObject(font);
    return same;
}

BOOL CALLBACK load(PINIT_ONCE, PVOID, PVOID*) {
    Plex& p = plex();
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&plex), &module);
    const bool regular = module && added(module, L"PLEX_REGULAR"), semibold = module && added(module, L"PLEX_SEMIBOLD");
    HDC dc = regular && semibold ? CreateCompatibleDC(nullptr) : nullptr;
    if (dc && facesAs(dc, kPlex) && facesAs(dc, kPlexBold)) {
        HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, kPlex);
        const HGDIOBJ was = SelectObject(dc, font);
        const DWORD size = GetFontUnicodeRanges(dc, nullptr);
        std::vector<uint8_t> bytes(size);
        auto* set = reinterpret_cast<GLYPHSET*>(bytes.data());
        if (size >= sizeof(GLYPHSET) && GetFontUnicodeRanges(dc, set))
            for (DWORD i = 0; i < set->cRanges; ++i)
                if (set->ranges[i].cGlyphs)
                    p.letters.emplace_back(set->ranges[i].wcLow,
                                           wchar_t(set->ranges[i].wcLow + set->ranges[i].cGlyphs - 1));
        SelectObject(dc, was);
        DeleteObject(font);
        p.ok = !p.letters.empty();
    }
    if (dc) DeleteDC(dc);
    if (p.ok)
        LOG_INFO("interface: the words in IBM Plex Sans (in the driver, SIL OFL 1.1) - Regular and SemiBold added, "
                 "%zu ranges of letters; a word with another letter in Segoe UI", p.letters.size());
    else
        LOG_INFO("interface: IBM Plex Sans could not be added (resources %s, %s) - the words in Segoe UI as before",
                 regular ? "regular" : "no regular", semibold ? "semibold" : "no semibold");
    return TRUE;
}

bool covered(const Plex& p, const std::wstring& text) {
    for (const wchar_t c : text) {
        bool in = false;
        for (const auto& [low, high] : p.letters)
            if (c >= low && c <= high) {
                in = true;
                break;
            }
        if (!in) return false;
    }
    return true;
}

}  // namespace

Face faceFor(const std::wstring& text, bool bold) {
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&once, load, nullptr, nullptr);
    const Plex& p = plex();
    if (p.ok && covered(p, text)) return Face{bold ? kPlexBold : kPlex, FW_NORMAL};
    return Face{L"Segoe UI", bold ? FW_SEMIBOLD : FW_NORMAL};
}

}  // namespace uagfx::win::uifont

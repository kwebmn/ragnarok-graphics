#include "app/status_info.hpp"

#include <windows.h>

#include "app/files.hpp"
#include "app/text.hpp"
#include "core/settings/whats_new.hpp"
#include "core/texture_pack.hpp"
#include "core/version.hpp"

namespace uagfx::app {

PackSummary packSummary(const std::wstring& dir) {
    PackSummary s;
    const std::wstring path = dir + L"\\uaro-textures.zip";
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &d)) return s;
    s.exists = true;
    s.bytes = uint64_t(d.nFileSizeHigh) << 32 | d.nFileSizeLow;
    const uint64_t stamp = uint64_t(d.ftLastWriteTime.dwHighDateTime) << 32 | d.ftLastWriteTime.dwLowDateTime;
    static uint64_t countedBytes = 0, countedStamp = 0;
    static pack::Counted counted;
    static bool countedOk = false;
    if (s.bytes != countedBytes || stamp != countedStamp) {
        std::string error;
        countedOk = pack::countOf(ansi(path), counted, error);
        countedBytes = s.bytes;
        countedStamp = stamp;
    }
    s.counted = countedOk;
    s.pictures = countedOk ? counted.pictures : 0;
    return s;
}

bool dlssInstalled(const std::wstring& dir) { return fileExists(dir + L"\\uaro-graphics\\dlss\\version.txt"); }

std::wstring notesText(const std::wstring& dir, settings::Lang l) {
    std::string md;
    std::string notes;
    if (readFile(dir + L"\\TEST." + wide(settings::langToken(l)) + L".md", md)) notes = settings::notesOf(md, kVersion);
    if (notes.empty() && readFile(dir + L"\\TEST.en.md", md)) notes = settings::notesOf(md, kVersion);
    return notes.empty() ? wtrf(l, settings::Text::NotesNone, {kVersion}) : wide(notes);
}

}  // namespace uagfx::app

#include "app/mnemonic.hpp"

namespace uagfx::app {
namespace {

// Lower case for the letters labels have: Latin, Cyrillic with the Ukrainian ones; anything else as it is.
wchar_t fold(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return wchar_t(c + 32);
    if (c >= 0x0410 && c <= 0x042F) return wchar_t(c + 0x20);  // А-Я
    if (c >= 0x0400 && c <= 0x040F) return wchar_t(c + 0x50);  // Ё, Є, І, Ї...
    if (c == 0x0490) return wchar_t(0x0491);                    // Ґ
    return c;
}

bool letter(wchar_t c) {
    const wchar_t f = fold(c);
    return (f >= L'a' && f <= L'z') || (f >= L'0' && f <= L'9') || (f >= 0x0430 && f <= 0x045F) || f == 0x0491;
}

}  // namespace

std::wstring withMnemonic(const std::wstring& text, std::wstring& taken) {
    size_t at = std::wstring::npos;
    for (int pass = 0; pass < 2 && at == std::wstring::npos; ++pass)  // the first letter of a word, then any
        for (size_t i = 0; i < text.size(); ++i) {
            const bool starts = i == 0 || !letter(text[i - 1]);
            if (letter(text[i]) && (pass == 1 || starts) && taken.find(fold(text[i])) == std::wstring::npos) {
                at = i;
                break;
            }
        }
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (i == at) out += L'&';
        out += text[i];
        if (text[i] == L'&') out += L'&';
    }
    if (at != std::wstring::npos) taken += fold(text[at]);
    return out;
}

void takeMnemonic(const std::wstring& text, std::wstring& taken) {
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        if (text[i] != L'&') continue;
        if (text[i + 1] != L'&') {
            taken += fold(text[i + 1]);
            return;
        }
        ++i;  // "&&" is an '&'
    }
}

}  // namespace uagfx::app

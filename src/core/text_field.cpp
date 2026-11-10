#include "core/text_field.hpp"

namespace uagfx::textfield {

bool type(Field& f, const std::string& utf8, size_t most) {
    if (utf8.empty()) return false;
    if (f.all) f.text.clear(), f.all = false;
    size_t take = 0;  // whole letters while they fit
    for (size_t i = 0; i < utf8.size();) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        const size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        if (f.text.size() + i + n > most || i + n > utf8.size()) break;
        i += n;
        take = i;
    }
    if (take == 0) return false;
    f.text += utf8.substr(0, take);
    return true;
}

bool backspace(Field& f) {
    if (f.all) {
        const bool had = !f.text.empty();
        f.text.clear(), f.all = false;
        return had;
    }
    if (f.text.empty()) return false;
    size_t at = f.text.size() - 1;
    while (at > 0 && (static_cast<unsigned char>(f.text[at]) & 0xC0) == 0x80) --at;
    f.text.erase(at);
    return true;
}

bool clear(Field& f) {
    const bool had = !f.text.empty();
    f.text.clear(), f.all = false;
    return had;
}

std::string oneLine(const std::string& utf8) {
    std::string out;
    for (const char ch : utf8) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '\n' || c == '\r' || c == '\t') out += ' ';
        else if (c >= 0x20 && c != 0x7F) out += ch;
    }
    return out;
}

}  // namespace uagfx::textfield

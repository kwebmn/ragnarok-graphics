#include "core/login_hints.hpp"

#include <cstddef>
#include <vector>

namespace uagfx::loginhint {
namespace {

// A Cyrillic letter (lower case) and the key it sits on: the key's own letter and its Shift's
struct Key {
    char32_t letter;
    char lower, upper;
};
constexpr Key kKeys[] = {
    {U'й', 'q', 'Q'}, {U'ц', 'w', 'W'}, {U'у', 'e', 'E'}, {U'к', 'r', 'R'}, {U'е', 't', 'T'}, {U'н', 'y', 'Y'},
    {U'г', 'u', 'U'}, {U'ш', 'i', 'I'}, {U'щ', 'o', 'O'}, {U'з', 'p', 'P'}, {U'х', '[', '{'}, {U'ї', ']', '}'},
    {U'ъ', ']', '}'}, {U'ф', 'a', 'A'}, {U'і', 's', 'S'}, {U'ы', 's', 'S'}, {U'в', 'd', 'D'}, {U'а', 'f', 'F'},
    {U'п', 'g', 'G'}, {U'р', 'h', 'H'}, {U'о', 'j', 'J'}, {U'л', 'k', 'K'}, {U'д', 'l', 'L'}, {U'ж', ';', ':'},
    {U'є', '\'', '"'}, {U'э', '\'', '"'}, {U'я', 'z', 'Z'}, {U'ч', 'x', 'X'}, {U'с', 'c', 'C'}, {U'м', 'v', 'V'},
    {U'и', 'b', 'B'}, {U'т', 'n', 'N'}, {U'ь', 'm', 'M'}, {U'б', ',', '<'}, {U'ю', '.', '>'}, {U'ґ', '`', '~'},
    {U'ё', '`', '~'},
};

// UTF-8 to code points (a broken byte kept as itself)
std::vector<char32_t> decode(const std::string& s) {
    std::vector<char32_t> out;
    for (size_t i = 0; i < s.size();) {
        const auto b = static_cast<unsigned char>(s[i]);
        const int more = b < 0x80 ? 0 : (b & 0xE0) == 0xC0 ? 1 : (b & 0xF0) == 0xE0 ? 2 : (b & 0xF8) == 0xF0 ? 3 : -1;
        if (more < 0 || i + size_t(more) >= s.size() + (more == 0 ? 1 : 0)) {
            out.push_back(b);
            ++i;
            continue;
        }
        char32_t c = more == 0 ? b : more == 1 ? (b & 0x1F) : more == 2 ? (b & 0x0F) : (b & 0x07);
        for (int k = 1; k <= more; ++k) c = (c << 6) | (static_cast<unsigned char>(s[i + size_t(k)]) & 0x3F);
        out.push_back(c);
        i += size_t(more) + 1;
    }
    return out;
}

void encode(char32_t c, std::string& out) {
    if (c < 0x80) {
        out += char(c);
    } else if (c < 0x800) {
        out += char(0xC0 | (c >> 6));
        out += char(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += char(0xE0 | (c >> 12));
        out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    } else {
        out += char(0xF0 | (c >> 18));
        out += char(0x80 | ((c >> 12) & 0x3F));
        out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    }
}

// The lower case of a Cyrillic capital (А-Я, Ѐ-Џ, Ґ), else the letter itself; `upper` told
char32_t lowerOf(char32_t c, bool& upper) {
    upper = false;
    if (c >= 0x410 && c <= 0x42F) {
        upper = true;
        return c + 0x20;
    }
    if (c >= 0x400 && c <= 0x40F) {
        upper = true;
        return c + 0x50;
    }
    if (c == 0x490) {
        upper = true;
        return 0x491;
    }
    return c;
}

bool cyrillic(char32_t c) { return c >= 0x400 && c <= 0x4FF; }

}  // namespace

std::string latinGuess(const std::string& utf8) {
    std::string out;
    for (char32_t c : decode(utf8)) {
        bool upper = false;
        const char32_t low = lowerOf(c, upper);
        const Key* key = nullptr;
        for (const Key& k : kKeys)
            if (k.letter == low) key = &k;
        if (key)
            out += upper ? key->upper : key->lower;
        else
            encode(c, out);
    }
    return out;
}

Hint forAccount(const std::string& utf8, bool caps) {
    Hint h;
    for (char32_t c : decode(utf8))
        if (cyrillic(c)) {
            h.kind = Kind::Layout;
            h.guess = latinGuess(utf8);
            return h;
        }
    if (caps) h.kind = Kind::CapsLock;
    return h;
}

Hint forPassword(bool caps) {
    Hint h;
    if (caps) h.kind = Kind::CapsLock;
    return h;
}

}  // namespace uagfx::loginhint

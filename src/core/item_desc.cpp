#include "core/item_desc.hpp"

#include <cctype>
#include <cstring>

namespace uagfx::itemdesc {
namespace {

bool hexAt(const std::string& s, size_t at) {
    if (at + 7 > s.size() || s[at] != '^') return false;
    for (size_t k = 1; k <= 6; ++k)
        if (!std::isxdigit(static_cast<unsigned char>(s[at + k]))) return false;
    return true;
}

// The words without their colour marks, the line breaks as spaces, the spaces one at a time, trimmed
std::string clean(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        if (hexAt(s, i)) {
            i += 7;
            continue;
        }
        const char ch = s[i] == '\n' || s[i] == '\r' || s[i] == '\t' ? ' ' : s[i];
        if (!(ch == ' ' && (out.empty() || out.back() == ' '))) out += ch;
        ++i;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

bool sameColour(const std::string& s, size_t at, const char* rgb) {  // «^777777», any case
    if (!hexAt(s, at)) return false;
    for (size_t k = 0; k < 6; ++k)
        if (std::tolower(static_cast<unsigned char>(s[at + 1 + k])) != rgb[k]) return false;
    return true;
}

// A line «Key: value» from `at` (a line's start): its key's end and where its value starts; false none
bool keyAt(const std::string& s, size_t at, size_t& keyFrom, size_t& keyTo, size_t& valueFrom) {
    size_t i = at;
    while (i < s.size() && (s[i] == ' ' || hexAt(s, i))) i += s[i] == ' ' ? 1 : 7;  // a colour before it
    if (i >= s.size() || !std::isalpha(static_cast<unsigned char>(s[i]))) return false;
    keyFrom = i;
    while (i < s.size() && i - keyFrom < 32 &&
           (std::isalpha(static_cast<unsigned char>(s[i])) || s[i] == ' ' || s[i] == '.'))
        ++i;
    if (i >= s.size() || s[i] != ':') return false;
    keyTo = i;
    while (keyTo > keyFrom && s[keyTo - 1] == ' ') --keyTo;
    ++i;
    while (i < s.size() && s[i] == ' ') ++i;
    valueFrom = i;
    return keyTo > keyFrom;
}

// Whether a line's words go on in the next: the next starts small, or this one - cut by the table's width (20 letters
// or more) - ends in a word (not in a full stop, a colon, a figure, a «%»: an effect's line, «VIT +1» over «Max HP
// +100»); a set's things one under the other («Bison Set», «Thorny Buckler») stay apart
bool goesOn(const std::string& line, const std::string& next) {
    if (line.empty() || next.empty()) return false;
    if (std::islower(static_cast<unsigned char>(next[0]))) return true;
    const unsigned char last = static_cast<unsigned char>(line.back());
    if (line.size() < 20 || std::strchr(".!?:%+)", last) || std::isdigit(last)) return false;
    return std::isalpha(last) || last == ',' || last == '-';
}

}  // namespace

Read read(const std::vector<std::string>& lines) {
    Read r;
    std::string all;
    for (size_t k = 0; k < lines.size(); ++k) all += (k ? "\n" : "") + lines[k];
    std::vector<std::string> words;  // the lines of words in order, "" a gap (a fact, an empty line, a white «_»)
    size_t at = 0;
    while (at <= all.size()) {
        size_t end = all.find('\n', at);
        if (end == std::string::npos) end = all.size();
        size_t keyFrom = 0, keyTo = 0, valueFrom = 0;
        if (keyAt(all, at, keyFrom, keyTo, valueFrom) && valueFrom < all.size()) {
            const std::string key = all.substr(keyFrom, keyTo - keyFrom);
            if (sameColour(all, valueFrom, "777777")) {  // a value of its own colour - to its end, over lines
                size_t stop = valueFrom + 7;
                while (stop < all.size() && !sameColour(all, stop, "000000")) ++stop;
                r.facts.push_back(Fact{key, clean(all.substr(valueFrom, stop - valueFrom))});
                words.emplace_back();
                const size_t after = stop < all.size() ? stop + 7 : stop;
                size_t lineEnd = all.find('\n', after);
                if (lineEnd == std::string::npos) lineEnd = all.size();
                // the rest of its line: another fact («Weapon Class : … Attack Strength : …»), else words
                at = clean(all.substr(after, lineEnd - after)).empty() ? lineEnd + 1 : after;
                continue;
            }
            const std::string value = clean(all.substr(valueFrom, end > valueFrom ? end - valueFrom : 0));
            if (keyOf(key) != Key::Unknown && !value.empty() && value.size() < 60) {  // a known key, plain
                r.facts.push_back(Fact{key, value});
                words.emplace_back();
                at = end + 1;
                continue;
            }
        }
        std::string line = clean(all.substr(at, end - at));
        if (line == "_") line.clear();
        words.push_back(line);
        at = end + 1;
    }
    std::string paragraph;
    for (size_t k = 0; k < words.size(); ++k) {
        if (words[k].empty()) {
            if (!paragraph.empty()) r.paragraphs.push_back(paragraph);
            paragraph.clear();
            continue;
        }
        paragraph += (paragraph.empty() ? "" : " ") + words[k];
        if (k + 1 < words.size() && goesOn(words[k], words[k + 1])) continue;
        r.paragraphs.push_back(paragraph);
        paragraph.clear();
    }
    if (!paragraph.empty()) r.paragraphs.push_back(paragraph);
    return r;
}

Key keyOf(const std::string& key) {
    std::string k;
    for (const char ch : key) {
        const char low = char(std::tolower(static_cast<unsigned char>(ch)));
        if (low == '.') continue;
        if (!(low == ' ' && (k.empty() || k.back() == ' '))) k += low;
    }
    while (!k.empty() && k.back() == ' ') k.pop_back();
    struct Name {
        const char* words;
        Key key;
    };
    static const Name kNames[] = {
        {"weight", Key::Weight},          {"class", Key::Class},
        {"item class", Key::Class},       {"weapon class", Key::Class},
        {"type", Key::Class},             {"weapon type", Key::Class},
        {"item type", Key::Class},        {"attack", Key::Attack},
        {"attack strength", Key::Attack}, {"atk", Key::Attack},
        {"power", Key::Attack},           {"defense", Key::Defense},
        {"defence", Key::Defense},        {"defense rate", Key::Defense},
        {"def", Key::Defense},            {"weapon level", Key::WeaponLevel},
        {"weapon lv", Key::WeaponLevel},  {"weapon lvl", Key::WeaponLevel},
        {"required level", Key::RequiredLevel}, {"required lv", Key::RequiredLevel},
        {"required lvl", Key::RequiredLevel},   {"level requirement", Key::RequiredLevel},
        {"lvl required", Key::RequiredLevel},   {"jobs", Key::Jobs},
        {"job", Key::Jobs},               {"applicable job", Key::Jobs},
        {"applicable jobs", Key::Jobs},   {"location", Key::Location},
        {"position", Key::Location},      {"head position", Key::Location},
        {"item position", Key::Location}, {"slot", Key::Slots},
        {"slots", Key::Slots},            {"property", Key::Element},
        {"element", Key::Element},        {"compound on", Key::CompoundOn},
    };
    for (const Name& n : kNames)
        if (k == n.words) return n.key;
    return Key::Unknown;
}

}  // namespace uagfx::itemdesc

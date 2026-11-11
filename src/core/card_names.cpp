#include "core/card_names.hpp"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace uagfx::cardnames {
namespace {

std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back();
    size_t a = 0;
    while (a < s.size() && s[a] == ' ') ++a;
    return s.substr(a);
}

const char* times(int n) {
    return n == 2 ? "Double " : n == 3 ? "Triple " : n >= 4 ? "Quadruple " : "";
}

}  // namespace

Tables read(const std::string& prefixText, const std::string& postfixText) {
    Tables t;
    std::istringstream a(prefixText);
    for (std::string line; std::getline(a, line);) {
        line = trimmed(line);
        if (line.empty() || line.rfind("//", 0) == 0) continue;
        const size_t hash = line.find('#');
        if (hash == std::string::npos) continue;
        const int id = std::atoi(line.substr(0, hash).c_str());
        std::string word = line.substr(hash + 1);
        if (const size_t end = word.find('#'); end != std::string::npos) word.resize(end);
        word = trimmed(word);
        if (id > 0 && !word.empty()) t.words[id] = word;
    }
    std::istringstream b(postfixText);
    for (std::string line; std::getline(b, line);) {
        line = trimmed(line);
        if (line.empty() || line.rfind("//", 0) == 0) continue;
        const int id = std::atoi(line.c_str());
        if (id > 0) t.after.insert(id);
    }
    return t;
}

std::string named(const std::string& name, const int cards[4], const Tables& t) {
    if (cards[0] == 0xFF || cards[0] == 0xFE || cards[0] == 0xFF00 || cards[0] < 0) return name;
    std::vector<std::pair<int, int>> counted;  // a card, how many times - in the order they sit
    for (int i = 0; i < 4; ++i) {
        if (cards[i] <= 0) continue;
        bool seen = false;
        for (auto& c : counted)
            if (c.first == cards[i]) ++c.second, seen = true;
        if (!seen) counted.emplace_back(cards[i], 1);
    }
    std::string before, after;
    for (const auto& [card, n] : counted) {
        const auto w = t.words.find(card);
        if (w == t.words.end()) continue;
        if (t.after.count(card)) {  // 3.0.18-beta.113: its word has its «of» already («of Champion»)
            std::string word = w->second;
            const bool of = word.rfind("of ", 0) == 0;
            if (of) word = word.substr(3);
            after += std::string(of ? " of " : " ") + times(n) + word;
        } else {
            before += std::string(times(n)) + w->second + " ";
        }
    }
    return before + name + after;
}

void cardsOf(const uint8_t* itemInfo, int cards[4]) {
    for (int c = 0; c < 4; ++c) {
        uint32_t v = 0;
        for (int b = 0; b < 4; ++b) v |= uint32_t(itemInfo[0x1C + 4 * c + b]) << (8 * b);
        cards[c] = int32_t(v);
    }
}

std::string namedFromInfo(const std::string& name, const uint8_t* itemInfo, const Tables& t) {
    if (!itemInfo || !itemInfo[0x3C]) return name;
    int cards[4];
    cardsOf(itemInfo, cards);
    return named(name, cards, t);
}

}  // namespace uagfx::cardnames

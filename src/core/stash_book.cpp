#include "core/stash_book.hpp"

#include <algorithm>
#include <sstream>

namespace uagfx::stashbook {
namespace {

const char* kKinds[] = {"bag", "cart", "storage"};

std::string escaped(const std::string& s) {
    std::string out;
    for (const char c : s) out += c == ' ' ? std::string("%20") : c == '%' ? std::string("%25") : std::string(1, c);
    return out;
}

std::string unescaped(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s.compare(i, 3, "%20") == 0) {
            out += ' ';
            i += 2;
        } else if (s.compare(i, 3, "%25") == 0) {
            out += '%';
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

}  // namespace

bool Book::note(const Place& p, const std::map<int, int>& counts) {
    if (p.owner.empty()) return false;
    auto& have = places_[p];
    if (have == counts) return false;
    have = counts;
    return true;
}

std::string Book::text() const {
    std::string out;
    for (const auto& [p, counts] : places_) {
        out += std::string(kKinds[int(p.kind)]) + ' ' + escaped(p.owner) + ' ';
        bool first = true;
        for (const auto& [id, n] : counts) {
            if (!first) out += ',';
            first = false;
            out += std::to_string(id) + ':' + std::to_string(n);
        }
        out += '\n';
    }
    return out;
}

Book Book::parse(const std::string& text) {
    Book b;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        std::string kind, owner, list;
        if (!(l >> kind >> owner)) continue;
        l >> list;
        Place p;
        const auto k = std::find_if(std::begin(kKinds), std::end(kKinds), [&](const char* x) { return kind == x; });
        if (k == std::end(kKinds)) continue;
        p.kind = Kind(k - std::begin(kKinds));
        p.owner = unescaped(owner);
        std::map<int, int> counts;
        std::istringstream items(list);
        std::string one;
        while (std::getline(items, one, ',')) {
            const size_t colon = one.find(':');
            if (colon == std::string::npos) continue;
            const int id = std::atoi(one.substr(0, colon).c_str()), n = std::atoi(one.substr(colon + 1).c_str());
            if (id > 0 && n > 0) counts[id] = n;
        }
        b.places_[p] = counts;
    }
    return b;
}

std::vector<std::pair<Place, int>> Book::find(const std::function<bool(int)>& wanted,
                                               const std::vector<Place>& here) const {
    std::vector<std::pair<Place, int>> out;
    for (const auto& [p, counts] : places_) {
        if (std::find_if(here.begin(), here.end(), [&](const Place& h) { return !(h < p) && !(p < h); }) != here.end())
            continue;
        int sum = 0;
        for (const auto& [id, n] : counts)
            if (wanted(id)) sum += n;
        if (sum > 0) out.emplace_back(p, sum);
    }
    std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    return out;
}

std::string line(const std::vector<std::pair<Place, int>>& found, const Texts& t, size_t most) {
    if (found.empty()) return "";
    std::string list;
    for (size_t i = 0; i < found.size() && i < most; ++i) {
        const Place& p = found[i].first;
        const std::string name = p.kind == Kind::Storage ? t.storage
                                 : p.kind == Kind::Cart  ? p.owner + ", " + t.cart
                                                         : p.owner;
        list += (list.empty() ? "" : " \xC2\xB7 ") + name + " \xC3\x97" + std::to_string(found[i].second);
    }
    if (found.size() > most) list += " \xE2\x80\xA6";
    const size_t at = t.also.find("{1}");
    std::string out = t.also;
    if (at != std::string::npos) out.replace(at, 3, list);
    return out;
}

}  // namespace uagfx::stashbook

#include "core/studio_link.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace uagfx::studio {
namespace {

// "a\tb\tc" or "k=v" lines of a text, without their '\r'
std::vector<std::string> linesOf(const std::string& text) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(std::move(line));
        at = end + 1;
    }
    return out;
}

const char* const kHows[] = {"pack", "made", "missing", "soft", "alpha", "size", "kind-off", "marked", "budget",
                             "hidden"};

void put32(std::string& s, uint32_t v) {
    for (int i = 0; i < 4; ++i) s += char(v >> (8 * i) & 0xFF);
}
uint32_t get32(const std::string& s, size_t at) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= uint32_t(uint8_t(s[at + size_t(i)])) << (8 * i);
    return v;
}

}  // namespace

std::string lineOf(const Message& m) {
    json::Value v = json::Value::object();
    v.set("id", m.id);
    v.set("op", m.op);
    if (m.body.type() != json::Value::Null) v.set("body", m.body);
    return json::dump(v) + "\n";
}

void LineReader::feed(const char* p, size_t n) { buffer_.append(p, n); }

bool LineReader::next(Message& out) {
    for (;;) {
        const size_t end = buffer_.find('\n');
        if (end == std::string::npos) return false;
        std::string line = buffer_.substr(0, end);
        buffer_.erase(0, end + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        json::Value v;
        std::string error;
        if (!json::parse(line, v, error) || v.type() != json::Value::Object || !v["op"].asString().size()) {
            ++skipped_;
            continue;
        }
        out.id = v["id"].asInt();
        out.op = v["op"].asString();
        out.body = v["body"];
        return true;
    }
}

std::string pickText(const Pick& p) {
    return "key=" + p.key + "\nkind=" + p.kind + "\nhow=" + p.how + "\nmap=" + p.map + "\npicture=" + p.picture +
           "\nx=" + std::to_string(p.x) + "\ny=" + std::to_string(p.y) + "\n";
}

bool parsePick(const std::string& text, Pick& out) {
    Pick p;
    for (const std::string& line : linesOf(text)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "key") p.key = v;
        else if (k == "kind") p.kind = v;
        else if (k == "how") p.how = v;
        else if (k == "map") p.map = v;
        else if (k == "picture") p.picture = v;
        else if (k == "x") p.x = std::atoi(v.c_str());
        else if (k == "y") p.y = std::atoi(v.c_str());
    }
    if (p.key.empty()) return false;
    out = p;
    return true;
}

std::string changedText(const std::vector<std::string>& keys) {
    std::string t;
    for (const std::string& k : keys) t += k + "\n";
    return t;
}

std::vector<std::string> parseChanged(const std::string& text) {
    std::vector<std::string> out;
    for (std::string& line : linesOf(text))
        if (!line.empty()) out.push_back(std::move(line));
    return out;
}

const char* howName(How h) { return size_t(h) < std::size(kHows) ? kHows[size_t(h)] : "missing"; }

bool parseHow(const std::string& word, How& out) {
    for (size_t i = 0; i < std::size(kHows); ++i)
        if (word == kHows[i]) {
            out = How(i);
            return true;
        }
    return false;
}

std::string seenText(const std::string& map, const std::vector<Seen>& seen) {
    std::string t = "map=" + map + "\n";
    for (const Seen& s : seen)
        t += s.key + "\t" + std::to_string(s.kind) + "\t" + howName(s.how) + "\t" + std::to_string(s.count) + "\n";
    return t;
}

bool parseSeen(const std::string& text, std::string& map, std::vector<Seen>& out) {
    const std::vector<std::string> lines = linesOf(text);
    if (lines.empty() || lines[0].compare(0, 4, "map=") != 0) return false;
    map = lines[0].substr(4);
    out.clear();
    for (size_t i = 1; i < lines.size(); ++i) {
        std::vector<std::string> f;
        for (size_t at = 0;;) {
            const size_t tab = lines[i].find('\t', at);
            f.push_back(lines[i].substr(at, tab == std::string::npos ? std::string::npos : tab - at));
            if (tab == std::string::npos) break;
            at = tab + 1;
        }
        Seen s;
        if (f.size() < 4 || f[0].empty() || !parseHow(f[2], s.how)) continue;
        s.key = f[0];
        s.kind = uint8_t(std::atoi(f[1].c_str()));
        s.count = uint32_t(std::strtoul(f[3].c_str(), nullptr, 10));
        out.push_back(std::move(s));
    }
    return true;
}

std::vector<std::string> seenToDelete(std::vector<std::pair<std::string, int64_t>> files, size_t keep) {
    std::vector<std::string> out;
    if (files.size() <= keep) return out;
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    for (size_t i = 0; i + keep < files.size(); ++i) out.push_back(files[i].first);
    return out;
}

std::string rawOf(int w, int h, const std::vector<uint32_t>& argb) {
    if (w <= 0 || h <= 0 || argb.size() < size_t(w) * size_t(h)) return {};
    std::string s = "URAW";
    put32(s, uint32_t(w));
    put32(s, uint32_t(h));
    const size_t n = size_t(w) * size_t(h);
    s.reserve(12 + n * 4);
    for (size_t i = 0; i < n; ++i) put32(s, argb[i]);
    return s;
}

bool parseRaw(const std::string& bytes, int& w, int& h, std::vector<uint32_t>& argb) {
    if (bytes.size() < 12 || bytes.compare(0, 4, "URAW") != 0) return false;
    const uint32_t width = get32(bytes, 4), height = get32(bytes, 8);
    if (!width || !height || width > 16384 || height > 16384 ||
        uint64_t(width) * height * 4 != uint64_t(bytes.size() - 12))
        return false;
    w = int(width);
    h = int(height);
    argb.resize(size_t(width) * height);
    for (size_t i = 0; i < argb.size(); ++i) argb[i] = get32(bytes, 12 + i * 4);
    return true;
}

}  // namespace uagfx::studio

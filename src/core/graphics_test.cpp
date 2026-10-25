#include "core/graphics_test.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>

namespace uagfx::graphics_test {
namespace {

using settings::Text;

constexpr double kPi = 3.14159265358979323846;
constexpr int kDearest = 3;  // the parts in the table's last column

// UTF-8 text's letters (a fixed-width font gives each one place: Cyrillic is two bytes)
size_t letters(const std::string& s) {
    size_t n = 0;
    for (const char c : s) n += (static_cast<unsigned char>(c) & 0xC0) != 0x80 ? 1 : 0;
    return n;
}

std::string padded(const std::string& s, size_t width) {
    const size_t n = letters(s);
    return s + std::string(n < width ? width - n : 0, ' ');
}

std::string number(const char* format, double v) {
    char text[32];
    std::snprintf(text, sizeof text, format, v);
    return text;
}

// "name 1.23, name 0.45": the first `most`
std::string listed(const std::vector<std::pair<std::string, double>>& parts, int most) {
    std::string out;
    for (int i = 0; i < int(parts.size()) && i < most; ++i)
        out += (out.empty() ? "" : ", ") + parts[size_t(i)].first + number(" %.2f", parts[size_t(i)].second);
    return out;
}

}  // namespace

const char* setWord(Set s) {
    switch (s) {
        case Set::Off: return "off";
        case Set::Low: return "low";
        case Set::Medium: return "medium";
        case Set::High: return "high";
        case Set::Own: return "own";
    }
    return "";
}

std::string tagOf(int phase) {
    return "test " + std::to_string(phase + 1) + "/" + std::to_string(kSets) + " " + setWord(Set(phase));
}

Pose poseAt(float startYaw, float from, float to, double u) {
    u = std::clamp(u, 0.0, 1.0);
    Pose p;
    p.yaw = float(std::fmod(double(startYaw) + 360.0 * u, 360.0));
    if (p.yaw < 0.0f) p.yaw += 360.0f;
    p.distance = float(double(from) + double(to - from) * std::sin(kPi * u));
    return p;
}

void zoomRange(bool outdoor, bool ownLimits, int limitFrom, int limitSpan, float& from, float& to) {
    if (ownLimits && limitFrom > 0 && limitSpan >= 0 && limitFrom + limitSpan <= 5000) {
        from = float(limitFrom);
        to = float(limitFrom + limitSpan);
    } else if (outdoor) {
        from = 300.0f;
        to = 1400.0f;
    } else {  // the game's own on a building map (uaRO.exe 0x565f34: 230 to 300)
        from = 230.0f;
        to = 300.0f;
    }
}

void Run::start(double nowS) {
    *this = Run{};
    running_ = true;
    startS_ = nowS;
}

Step Run::frame(double nowS, double frameMs) {
    Step s;
    if (!running_) return s;
    const double elapsed = std::max(0.0, nowS - startS_);
    const int phase = int(elapsed / kPhaseS);
    if (phase != phase_) {
        if (phase_ >= 0 && measuring_) {
            s.measureEnds = true;
            s.ended = phase_;
        }
        measuring_ = false;
        if (phase >= kSets) {
            s.phase = kSets - 1;
            s.finished = true;
            running_ = false;
            return s;
        }
        phase_ = phase;
        s.setBegins = true;
    }
    s.phase = phase_;
    const double within = elapsed - double(phase_) * kPhaseS;
    if (within >= kSettleS) {
        s.measureBegins = !measuring_;
        measuring_ = true;
        s.measuring = true;
        s.u = std::clamp((within - kSettleS) / kMeasureS, 0.0, 1.0);
        frameMs_[phase_].push_back(frameMs);
    }
    return s;
}

void Run::gpu(double nowS, double gpuMs, const std::map<std::string, double>& effects) {
    if (!running_ || gpuMs < 0.0) return;
    const double elapsed = nowS - startS_;
    const int phase = elapsed >= 0.0 ? int(elapsed / kPhaseS) : -1;
    if (phase < 0 || phase >= kSets || elapsed - double(phase) * kPhaseS < kSettleS + kGpuSkipS) return;
    gpuSum_[phase] += gpuMs;
    ++gpuSamples_[phase];
    for (const auto& [name, ms] : effects) effectSums_[phase][name] += ms;
}

PhaseResult Run::result(int phase) const {
    PhaseResult r;
    if (phase < 0 || phase >= kSets) return r;
    const std::vector<double>& ms = frameMs_[phase];
    r.frames = int(ms.size());
    double sum = 0.0;
    for (const double m : ms) sum += m;
    r.seconds = sum / 1000.0;
    if (r.frames && sum > 0.0) {
        r.fps = double(r.frames) * 1000.0 / sum;
        std::vector<double> slowest = ms;
        const size_t tenth = std::max<size_t>(1, slowest.size() / 10);
        std::partial_sort(slowest.begin(), slowest.begin() + std::ptrdiff_t(tenth), slowest.end(),
                          std::greater<double>());
        double slow = 0.0;
        for (size_t i = 0; i < tenth; ++i) slow += slowest[i];
        r.slowFps = slow > 0.0 ? double(tenth) * 1000.0 / slow : 0.0;
    }
    if (gpuSamples_[phase]) {
        r.gpuMs = gpuSum_[phase] / double(gpuSamples_[phase]);
        for (const auto& [name, total] : effectSums_[phase]) r.effects[name] = total / double(gpuSamples_[phase]);
    }
    return r;
}

std::map<int, Parts> profileParts(const std::string& log) {
    static const std::string kTag = "profile [test ", kCard = "]: the card ", kList = " a frame - ";
    std::map<int, Parts> out;
    size_t from = 0;
    while (from < log.size()) {
        size_t end = log.find('\n', from);
        if (end == std::string::npos) end = log.size();
        std::string line = log.substr(from, end - from);
        from = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t tag = line.find(kTag);
        const size_t card = tag == std::string::npos ? tag : line.find(kCard, tag);
        if (card == std::string::npos) continue;
        const int n = std::atoi(line.c_str() + tag + kTag.size());
        if (n < 1 || n > kSets) continue;
        Parts p;
        p.cardMs = std::atof(line.c_str() + card + kCard.size());
        const size_t list = line.find(kList, card);
        std::string rest = list == std::string::npos ? "" : line.substr(list + kList.size());
        if (rest.size() >= 5 && rest.compare(rest.size() - 5, 5, " (ms)") == 0) rest.resize(rest.size() - 5);
        size_t at = 0;
        while (at < rest.size()) {
            size_t comma = rest.find(", ", at);
            if (comma == std::string::npos) comma = rest.size();
            const std::string item = rest.substr(at, comma - at);
            at = comma + 2;
            const size_t space = item.rfind(' ');
            if (space == std::string::npos || space == 0) continue;
            char* stop = nullptr;
            const double ms = std::strtod(item.c_str() + space + 1, &stop);
            if (stop && *stop == '\0' && stop != item.c_str() + space + 1)
                p.parts.emplace_back(item.substr(0, space), ms);
        }
        out[n - 1] = p;
    }
    return out;
}

std::string summary(const Head& h, const PhaseResult (&r)[kSets], const std::map<int, Parts>& parts,
                    settings::Lang lang) {
    const auto t = [&](Text text) { return std::string(settings::tr(lang, text)); };
    std::string head = t(Text::TestTitle);
    const auto add = [&](const std::string& part) {
        if (!part.empty()) head += " · " + part;
    };
    add(h.version);
    add(h.map);
    add(h.api);
    add(h.gpu);
    if (h.width > 0 && h.height > 0) add(std::to_string(h.width) + "×" + std::to_string(h.height));
    add(h.fpsLimit > 0 ? settings::fmt(t(Text::TestLimit), {std::to_string(h.fpsLimit)}) : t(Text::TestNoLimit));
    if (h.vsync) add(t(Text::TestVsync));
    add(h.mode);
    static const Text kNames[kSets] = {Text::TestSetOff, Text::FxPresetWeak, Text::FxPresetMedium,
                                       Text::FxPresetStrong, Text::TestSetOwn};
    // the columns: as wide as their heading or their widest value, two spaces between
    const std::string heads[4] = {t(Text::TestColSet), t(Text::TestColFps), t(Text::TestColSlow), t(Text::TestColCard)};
    std::vector<std::string> cells[4];
    std::vector<std::string> dearest;
    for (int p = 0; p < kSets; ++p) {
        cells[0].push_back(t(kNames[p]));
        if (!r[p].frames) {
            cells[1].push_back(t(Text::TestNotMeasured));
            cells[2].push_back("");
            cells[3].push_back("");
            dearest.push_back("");
            continue;
        }
        cells[1].push_back(number("%.0f", r[p].fps));
        cells[2].push_back(number("%.0f", r[p].slowFps));
        const auto found = parts.find(p);
        const double card = r[p].gpuMs >= 0.0 ? r[p].gpuMs : found != parts.end() ? found->second.cardMs : -1.0;
        cells[3].push_back(card >= 0.0 ? number("%.1f", card) : "—");
        if (found != parts.end() && !found->second.parts.empty()) {
            dearest.push_back(listed(found->second.parts, kDearest));
        } else {  // no profile lines: the card's timed passes
            std::vector<std::pair<std::string, double>> passes(r[p].effects.begin(), r[p].effects.end());
            std::stable_sort(passes.begin(), passes.end(),
                             [](const auto& a, const auto& b) { return a.second > b.second; });
            dearest.push_back(passes.empty() ? "—" : listed(passes, kDearest));
        }
    }
    size_t width[4];
    for (int c = 0; c < 4; ++c) {
        width[c] = letters(heads[c]);
        for (int p = 0; p < kSets; ++p)  // a set not measured: its name, then the words alone
            if (c == 0 || r[p].frames) width[c] = std::max(width[c], letters(cells[c][size_t(p)]));
    }
    std::string out = head + "\n\n";
    std::string row;
    for (int c = 0; c < 4; ++c) row += padded(heads[c], width[c] + 2);
    out += row + t(Text::TestColDearest) + "\n";
    for (int p = 0; p < kSets; ++p) {
        row.clear();
        if (!r[p].frames) {
            out += padded(cells[0][size_t(p)], width[0] + 2) + cells[1][size_t(p)] + "\n";
            continue;
        }
        for (int c = 0; c < 4; ++c) row += padded(cells[c][size_t(p)], width[c] + 2);
        out += row + dearest[size_t(p)] + "\n";
    }
    return out;
}

bool parseSummary(const std::string& text, Table& out) {
    std::vector<std::string> lines;
    size_t from = 0;
    while (from < text.size()) {
        size_t end = text.find('\n', from);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(from, end - from);
        from = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    // the cells of a row: two spaces or more between them
    const auto cells = [](const std::string& line) {
        std::vector<std::string> out;
        size_t at = 0;
        while (at < line.size()) {
            const size_t gap = line.find("  ", at);
            const std::string cell = line.substr(at, gap == std::string::npos ? std::string::npos : gap - at);
            if (!cell.empty()) out.push_back(cell);
            if (gap == std::string::npos) break;
            at = line.find_first_not_of(' ', gap);
            if (at == std::string::npos) break;
        }
        return out;
    };
    Table t;
    if (lines.size() < 3) return false;
    t.head = lines[0];
    t.columns = cells(lines[1]);
    if (t.columns.size() != 5) return false;
    for (size_t i = 2; i < lines.size() && t.rows.size() < size_t(kSets); ++i) {
        std::vector<std::string> row = cells(lines[i]);
        if (row.size() != 2 && row.size() != t.columns.size()) return false;
        t.rows.push_back(std::move(row));
    }
    out = std::move(t);
    return true;
}

}  // namespace uagfx::graphics_test

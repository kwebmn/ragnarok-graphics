#include "core/play_limit.hpp"

#include <algorithm>
#include <cstdlib>

namespace uagfx::playlimit {
namespace {

// The number right after `word` in `text` (-1: none there)
int numberAfter(const std::string& text, const std::string& word) {
    const size_t at = text.find(word);
    if (at == std::string::npos) return -1;
    const size_t from = at + word.size();
    if (from >= text.size() || text[from] < '0' || text[from] > '9') return -1;
    return std::atoi(text.c_str() + from);
}

void readHint(const std::string& text, Notice& n) {
    n.canBuy = text.find("@buyplaytime") != std::string::npos;
    n.zenyPerMinute = numberAfter(text, "@autopay on (");
}

}  // namespace

bool read(const std::string& text, Notice& n) {
    Notice out;
    if (text.rfind("Daily play-time limit (", 0) == 0) {
        out.reached = true;
        out.hours = numberAfter(text, "Daily play-time limit (");
        out.kickInS = numberAfter(text, "disconnected in ");
        const int h = numberAfter(text, "server time (in ");
        const size_t hAt = text.find("server time (in ");
        const int m = hAt == std::string::npos ? -1 : numberAfter(text.substr(hAt), " h ");
        if (out.hours < 0 || out.kickInS < 0) return false;
        if (h >= 0 && m >= 0) out.resetMin = h * 60 + m;
    } else if (text.rfind("Daily play time: ", 0) == 0) {
        out.minutesLeft = numberAfter(text, "Daily play time: ");
        if (out.minutesLeft < 0) return false;
    } else {
        return false;
    }
    readHint(text, out);
    n = out;
    return true;
}

Words wordsIn(settings::Lang l) {
    using settings::Text;
    Words w;
    w.reached = settings::tr(l, Text::PlayLimitReached);
    w.kicked = settings::tr(l, Text::PlayLimitKicked);
    w.left = settings::tr(l, Text::PlayLimitLeft);
    w.reset = settings::tr(l, Text::PlayLimitReset);
    w.buy = settings::tr(l, Text::PlayLimitBuy);
    w.autopay = settings::tr(l, Text::PlayLimitAutopay);
    return w;
}

std::string words(const Notice& n, const Words& w, bool kicked, int pastMin) {
    std::string out;
    const auto add = [&out](const std::string& part) {
        if (part.empty()) return;
        if (!out.empty()) out += ' ';
        out += part;
    };
    if (kicked) add(settings::fmt(w.kicked, {std::to_string(n.hours)}));
    else if (n.reached) add(settings::fmt(w.reached, {std::to_string(n.hours), std::to_string(n.kickInS)}));
    else add(settings::fmt(w.left, {std::to_string(n.minutesLeft)}));
    if (n.reached && n.resetMin >= 0) {
        const int r = std::max(0, n.resetMin - std::max(0, pastMin));
        add(settings::fmt(w.reset, {std::to_string(r / 60), std::to_string(r % 60)}));
    }
    if (n.canBuy) add(w.buy);
    if (n.zenyPerMinute >= 0) add(settings::fmt(w.autopay, {std::to_string(n.zenyPerMinute)}));
    return out;
}

std::string blockedWords(settings::Lang l) { return settings::tr(l, settings::Text::PlayLimitBlocked); }

bool kickOf(const nettrail::Drop& d, uint32_t saidMs, const Notice& n) {
    if (!n.reached || !d.notice || d.noticeCode != kKickCode || d.role != nettrail::Role::Zone) return false;
    const uint32_t after = d.ms - saidMs;  // the words after the drop: a wrap, too far
    return after <= uint32_t(n.kickInS) * 1000 + kKickSlackMs;
}

}  // namespace uagfx::playlimit

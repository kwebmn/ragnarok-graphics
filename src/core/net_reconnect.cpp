#include "core/net_reconnect.hpp"

namespace uagfx::reconnect {

Cause causeOf(const nettrail::Drop& d) {
    if (d.notice) {
        if (d.noticeCode == nettrail::kNoticeOtherLogin) return Cause::OtherLogin;
        if (d.noticeCode == 15) return Cause::Gm;
        if (d.noticeCode == 8) return Cause::StillOnline;
        return Cause::ServerNotice;
    }
    if (d.closedByServer) return Cause::ServerClosed;
    if (d.error) return Cause::Network;
    return Cause::Unknown;
}

bool worthy(const nettrail::Drop& d) {
    const Cause c = causeOf(d);
    return (c == Cause::Network || c == Cause::ServerClosed) && d.mode == nettrail::Mode::Game &&
           d.role == nettrail::Role::Zone && d.ageMs >= kSettledMs;
}

bool Tries::allow(uint64_t nowMs) const {
    int recent = 0;
    for (uint64_t t : at_)
        if (nowMs < t + kTriesWindowMs) ++recent;
    return recent < kMostTries;
}

void Tries::tried(uint64_t nowMs) {
    at_.push_back(nowMs);
    std::vector<uint64_t> kept;
    for (uint64_t t : at_)
        if (nowMs < t + kTriesWindowMs) kept.push_back(t);
    at_.swap(kept);
}

std::string words(const nettrail::Drop& d, settings::Lang l) {
    using settings::Text;
    switch (causeOf(d)) {
        case Cause::OtherLogin: return settings::tr(l, Text::DropOtherLogin);
        case Cause::Gm: return settings::tr(l, Text::DropGm);
        case Cause::StillOnline: return settings::tr(l, Text::DropStillOnline);
        case Cause::ServerNotice:
            return settings::fmt(settings::tr(l, Text::DropServerNotice), {std::to_string(d.noticeCode)});
        case Cause::ServerClosed: return settings::tr(l, Text::DropServerClosed);
        case Cause::Network: return settings::fmt(settings::tr(l, Text::DropNetwork), {std::to_string(d.error)});
        case Cause::Unknown: break;
    }
    return {};
}

std::string countdown(int seconds, settings::Lang l) {
    return settings::fmt(settings::tr(l, settings::Text::ReconnectIn), {std::to_string(seconds)});
}

}  // namespace uagfx::reconnect

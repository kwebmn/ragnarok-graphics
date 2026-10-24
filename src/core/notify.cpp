#include "core/notify.hpp"

namespace uagfx::notify {

bool wanted(const NotifySettings& s, Kind k, bool inFront) {
    if (!s.on || inFront) return false;
    switch (k) {
        case Kind::Whisper: return s.whisper;
        case Kind::Party: return s.party;
        case Kind::Trade: return s.trade;
        case Kind::Guild: return s.guild;
        case Kind::Friend: return s.friendAsk;
        case Kind::Death: return s.death;
    }
    return false;
}

std::string text(settings::Lang lang, Kind k, const std::string& who, const std::string& what) {
    using settings::Text;
    switch (k) {
        case Kind::Whisper: return settings::fmt(settings::tr(lang, Text::NotifyWhisperText), {who, what});
        case Kind::Party: return settings::fmt(settings::tr(lang, Text::NotifyPartyText), {who});
        case Kind::Trade: return settings::fmt(settings::tr(lang, Text::NotifyTradeText), {who});
        case Kind::Guild: return settings::fmt(settings::tr(lang, Text::NotifyGuildText), {who});
        case Kind::Friend: return settings::fmt(settings::tr(lang, Text::NotifyFriendText), {who});
        case Kind::Death: return settings::tr(lang, Text::NotifyDeathText);
    }
    return {};
}

bool Gate::pass(Kind k, uint64_t nowMs) {
    const int i = int(k);
    if (seen_[i] && nowMs - last_[i] < kGapMs) return false;
    seen_[i] = true;
    last_[i] = nowMs;
    return true;
}

}  // namespace uagfx::notify

#include "core/notify.hpp"

#include <initializer_list>
#include <string>

namespace uagfx::notify {

bool wanted(const NotifySettings& s, Kind k, bool inFront, bool main) {
    if (!s.on || inFront) return false;
    switch (k) {
        case Kind::Whisper: return s.whisper;
        case Kind::Party: return s.party;
        case Kind::Trade: return s.trade;
        case Kind::Guild: return s.guild;
        case Kind::Friend: return s.friendAsk;
        case Kind::Death: return s.death;
        case Kind::Attack: return s.attack;
        case Kind::BuffEnding: return s.buffEnding;
        case Kind::PartyDeath: return s.partyDeath && main;
        case Kind::PartyLowHp: return s.partyLowHp && main;
        case Kind::PartyChat: return s.partyChat && main;
        case Kind::GuildChat: return s.guildChat && main;
        case Kind::Mention: return s.mention;
        case Kind::Disconnect: return s.disconnect;
        case Kind::Recovered: return s.recovered;
    }
    return false;
}

std::string text(settings::Lang lang, Kind k, const std::string& who, const std::string& what) {
    using settings::Text;
    const auto say = [&](Text t, std::initializer_list<std::string> args) {
        return settings::fmt(settings::tr(lang, t), args);
    };
    switch (k) {
        case Kind::Whisper: return say(Text::NotifyWhisperText, {who, what});
        case Kind::Party: return say(Text::NotifyPartyText, {who});
        case Kind::Trade: return say(Text::NotifyTradeText, {who});
        case Kind::Guild: return say(Text::NotifyGuildText, {who});
        case Kind::Friend: return say(Text::NotifyFriendText, {who});
        case Kind::Death: return settings::tr(lang, Text::NotifyDeathText);
        case Kind::Attack:
            return what.empty() ? settings::tr(lang, Text::NotifyAttackText) : say(Text::NotifyAttackHurtText, {what});
        case Kind::BuffEnding: return say(Text::NotifyBuffEndingText, {who, what});
        case Kind::PartyDeath: return say(Text::NotifyPartyDeathText, {who});
        case Kind::PartyLowHp: return say(Text::NotifyPartyLowHpText, {who, what});
        case Kind::PartyChat: return say(Text::NotifyPartyChatText, {who, what});
        case Kind::GuildChat: return say(Text::NotifyGuildChatText, {who, what});
        case Kind::Mention: return say(Text::NotifyMentionText, {who, what});
        case Kind::Disconnect: return settings::tr(lang, Text::NotifyDisconnectText);
        case Kind::Recovered: return say(Text::NotifyRecoveredText, {what});
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

bool Attacks::hit(uint64_t nowMs) {
    const bool tell = !seen_ || nowMs - last_ >= kCalmMs;
    seen_ = true;
    last_ = nowMs;
    return tell;
}

bool Recovery::status(uint16_t type, uint32_t value, int percent) {
    switch (type) {
        case 5: hp_ = value; break;
        case 6: hpMost_ = value; break;
        case 7: sp_ = value; break;
        case 8: spMost_ = value; break;
        default: return false;
    }
    known_ |= 1u << (type - 5);
    if (known_ != 15 || !hpMost_ || !spMost_) return false;  // not all four told yet: nothing to judge
    const int64_t hp = int64_t(hp_) * 100 / hpMost_, sp = int64_t(sp_) * 100 / spMost_;
    if (hp < percent - kRearm || sp < percent - kRearm) down_ = true;
    if (!down_ || hp < percent || sp < percent) return false;
    down_ = false;
    return true;
}

void speaker(const std::string& line, std::string& who, std::string& what) {
    const size_t at = line.find(" : ");
    if (at == std::string::npos) {
        who.clear();
        what = line;
        return;
    }
    who = line.substr(0, at);
    what = line.substr(at + 3);
}

namespace {

// UTF-8 into code points (a broken byte as itself)
std::u32string points(const std::string& s) {
    std::u32string out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        int n = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : 0;
        if (i + size_t(n) >= s.size()) n = 0;  // cut short: the byte as itself
        char32_t p = n == 0 ? c : n == 1 ? (c & 0x1F) : n == 2 ? (c & 0x0F) : (c & 0x07);
        for (int k = 1; k <= n; ++k) p = (p << 6) | (static_cast<unsigned char>(s[i + size_t(k)]) & 0x3F);
        out.push_back(p);
        i += size_t(n) + 1;
    }
    return out;
}

char32_t lowered(char32_t c) {
    if (c >= U'A' && c <= U'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;      // Latin-1
    if (c >= 0x410 && c <= 0x42F) return c + 32;                  // Cyrillic А-Я
    if (c >= 0x400 && c <= 0x40F) return c + 80;                  // Ѐ-Џ (Є, І, Ї ...)
    if (c == 0x490) return 0x491;                                 // Ґ
    return c;
}

bool wordy(char32_t c) {  // a letter or a digit of a name
    const char32_t l = lowered(c);
    return (l >= U'a' && l <= U'z') || (l >= U'0' && l <= U'9') || (l >= 0xDF && l <= 0xFF && l != 0xF7) ||
           (l >= 0x430 && l <= 0x45F) || l == 0x491 || c == U'_';
}

}  // namespace

bool mentions(const std::string& text, const std::string& name) {
    std::u32string t = points(text), n = points(name);
    if (n.empty() || t.size() < n.size()) return false;
    for (char32_t& c : t) c = lowered(c);
    for (char32_t& c : n) c = lowered(c);
    for (size_t at = t.find(n); at != std::u32string::npos; at = t.find(n, at + 1)) {
        const bool before = at == 0 || !wordy(t[at - 1]);
        const bool after = at + n.size() == t.size() || !wordy(t[at + n.size()]);
        if (before && after) return true;
    }
    return false;
}

}  // namespace uagfx::notify

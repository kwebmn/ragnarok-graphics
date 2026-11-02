#include "core/server_status.hpp"

namespace uagfx::srvstatus {
namespace {

constexpr uint32_t kUp = 0x4ED38A, kDown = 0xF2C46B, kUnknown = 0x9AA3B5;

}  // namespace

Line line(State s, int pingMs, int online, int retryIn, settings::Lang lang) {
    using settings::Text;
    Line l;
    if (s == State::Unknown) {
        l.rgb = kUnknown;
        return l;
    }
    if (s == State::Down) {
        l.rgb = kDown;
        l.text = retryIn > 0 ? settings::fmt(settings::tr(lang, Text::LoginStatusDown), {std::to_string(retryIn)})
                             : settings::tr(lang, Text::LoginStatusDownIdle);
        return l;
    }
    l.rgb = kUp;
    if (online >= 0 && pingMs >= 0)
        l.text = settings::fmt(settings::tr(lang, Text::LoginStatusUp),
                               {std::to_string(online), std::to_string(pingMs)});
    else if (online >= 0)
        l.text = settings::fmt(settings::tr(lang, Text::LoginStatusUpCount), {std::to_string(online)});
    else
        l.text = settings::fmt(settings::tr(lang, Text::LoginStatusUpNoCount),
                               {std::to_string(pingMs < 0 ? 0 : pingMs)});
    return l;
}

bool retryLogin(bool lastFailedToConnect, bool playerPressed, int secsSinceFail) {
    return lastFailedToConnect && playerPressed && secsSinceFail >= kProbeSec;
}

}  // namespace uagfx::srvstatus

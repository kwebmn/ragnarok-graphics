#include "core/report_talk.hpp"

namespace uagfx::talk {
namespace {

long long seconds(const std::string& iso) {  // UTC as one number; -1 not a time
    reports::Civil c;
    if (!reports::utcOf(iso, c)) return -1;
    return ((((c.year * 13LL + c.month) * 32 + c.day) * 24 + c.hour) * 60 + c.minute) * 60 + c.second;
}

}  // namespace

std::vector<Item> timeline(const reports::Conversation& c) {
    std::vector<Item> out;
    if (c.automatic) out.push_back(Item{Item::AutoSent, 0});
    // where «Розробник прочитав» goes: after the last message older than the opening, when it is the player's (an
    // answer of the developer's after it says so by itself)
    const long long seen = c.devSeenAt.empty() ? -1 : seconds(c.devSeenAt);
    size_t readAfter = c.messages.size();
    if (seen >= 0)
        for (size_t i = c.messages.size(); i-- > 0;) {
            const long long at = seconds(c.messages[i].at);
            if (at < 0 || at > seen) continue;
            if (!c.messages[i].fromDev) readAfter = i;
            break;
        }
    for (size_t i = 0; i < c.messages.size(); ++i) {
        const reports::Message& m = c.messages[i];
        if (!m.text.empty() || !m.shots.empty()) out.push_back(Item{Item::Message, i});
        if (m.hasStatus) out.push_back(Item{Item::Status, i});
        if (i == readAfter) out.push_back(Item{Item::DevRead, 0});
    }
    return out;
}

std::vector<Step> steps(const reports::Conversation& c) {
    using reports::Status;
    bool asked = c.status == Status::NeedInfo;
    for (const reports::Message& m : c.messages) asked = asked || (m.hasStatus && m.status == Status::NeedInfo);
    std::vector<Status> path{Status::New, Status::Read};
    if (asked) path.push_back(Status::NeedInfo);
    for (Status s : {Status::Fixing, Status::Fixed, Status::Closed}) path.push_back(s);
    size_t current = 0;
    for (size_t i = 0; i < path.size(); ++i)
        if (path[i] == c.status) current = i;
    std::vector<Step> out;
    for (size_t i = 0; i < path.size(); ++i) out.push_back(Step{path[i], i < current, i == current});
    return out;
}

bool closedTab(reports::Status s) { return s == reports::Status::Closed; }

}  // namespace uagfx::talk

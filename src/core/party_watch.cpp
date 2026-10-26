#include "core/party_watch.hpp"

namespace uagfx::party {

void Watch::members(const std::vector<std::pair<uint32_t, std::string>>& list, uint32_t own) {
    own_ = own;
    std::map<uint32_t, Member> next;
    for (const auto& [account, name] : list) {
        if (account == own) continue;
        const auto was = members_.find(account);
        Member m = was != members_.end() ? was->second : Member{};  // sent again (a map change): what was told stays
        m.name = name;
        next[account] = m;
    }
    members_.swap(next);
}

void Watch::joined(uint32_t account, const std::string& name) {
    if (account != own_) members_[account].name = name;
}

void Watch::left(uint32_t account) { members_.erase(account); }

void Watch::hp(uint32_t account, uint16_t hp, uint16_t most, int threshold, std::vector<News>& out) {
    const auto it = members_.find(account);
    if (it == members_.end() || most == 0) return;
    Member& m = it->second;
    if (hp == 0) {
        if (!m.dead) out.push_back(News{News::Dead, m.name, 0});
        m.dead = true;
        return;
    }
    m.dead = false;
    const int percent = int(uint32_t(hp) * 100u / most);
    if (percent < threshold) {
        if (!m.low) out.push_back(News{News::Low, m.name, percent});
        m.low = true;
    } else if (percent >= threshold + kRearm) {
        m.low = false;
    }
}

void Watch::died(uint32_t account, std::vector<News>& out) {
    const auto it = members_.find(account);
    if (it == members_.end() || it->second.dead) return;
    it->second.dead = true;
    out.push_back(News{News::Dead, it->second.name, 0});
}

}  // namespace uagfx::party

#include "core/guild_skills.hpp"

#include <algorithm>

namespace uagfx::guildskills {
namespace {

const char* const kTimes = "\xC3\x97";  // ×
const char* const kDot = " \xC2\xB7 ";  // ·

std::string fill(std::string pattern, const std::string& value) {
    if (const size_t at = pattern.find("{1}"); at != std::string::npos) pattern.replace(at, 3, value);
    return pattern;
}

}  // namespace

const std::vector<Def>& table() {
    static const std::vector<Def> t = {
        {10000, "GD_APPROVAL", "Official Guild Approval", 1, Kind::Passive, Group::Castle, {}},
        {10001, "GD_KAFRACONTRACT", "Kafra Contract", 1, Kind::Passive, Group::Castle, {{10000, 1}}},
        {10002, "GD_GUARDRESEARCH", "Guardian Research", 1, Kind::Passive, Group::Castle, {{10000, 1}}},
        {10003, "GD_GUARDUP", "Strengthen Guardians", 3, Kind::Passive, Group::Castle, {}},
        {10004, "GD_EXTENSION", "Guild Extension", 10, Kind::Passive, Group::Growth, {}},
        {10014, "GD_DEVELOPMENT", "Permanent Development", 1, Kind::Passive, Group::Growth, {}},
        {10006, "GD_LEADERSHIP", "Great Leadership", 5, Kind::Aura, Group::Auras, {}},
        {10007, "GD_GLORYWOUNDS", "Glorious Wounds", 5, Kind::Aura, Group::Auras, {}},
        {10008, "GD_SOULCOLD", "Cold Heart", 5, Kind::Aura, Group::Auras, {{10007, 1}}},
        {10009, "GD_HAWKEYES", "Sharp Gaze", 5, Kind::Aura, Group::Auras, {{10006, 1}}},
        {10010, "GD_BATTLEORDER", "Battle Orders", 1, Kind::Active, Group::War, {{10000, 1}, {10004, 2}}},
        {10011, "GD_REGENERATION", "Regeneration", 3, Kind::Active, Group::War, {{10000, 1}, {10004, 5}, {10010, 1}}},
        {10012, "GD_RESTORE", "Restoration", 1, Kind::Active, Group::War, {{10011, 1}}},
        {10013, "GD_EMERGENCYCALL", "Urgent Call", 1, Kind::Active, Group::War,
         {{10000, 1}, {10002, 1}, {10004, 5}, {10010, 1}, {10011, 1}}},
    };
    return t;
}

int indexOf(int id) {
    const std::vector<Def>& t = table();
    for (size_t i = 0; i < t.size(); ++i)
        if (t[i].id == id) return int(i);
    return -1;
}

const Def* def(int id) {
    const int i = indexOf(id);
    return i < 0 ? nullptr : &table()[size_t(i)];
}

bool shared(int id) { return id >= 10010 && id <= 10013; }

std::vector<Row> rows(const std::vector<Sent>& sent, int points, bool master, int coolMs) {
    const auto sentOf = [&](int id) -> const Sent* {
        for (const Sent& k : sent)
            if (k.id == id) return &k;
        return nullptr;
    };
    const auto levelOf = [&](int id) {
        const Sent* k = sentOf(id);
        return k ? k->level : 0;
    };
    std::vector<Row> out;
    for (const Def& d : table()) {
        Row r;
        r.id = d.id;
        r.most = d.most;
        r.kind = d.kind;
        r.group = d.group;
        const Sent* k = sentOf(d.id);
        r.level = k ? std::clamp(k->level, 0, d.most) : 0;
        for (const Need& n : d.needs)
            if (levelOf(n.id) < n.level) r.missing.push_back(n);
        r.locked = !k && !r.missing.empty();
        r.plus = master && points > 0 && !r.locked && r.missing.empty() && r.level < d.most && (!k || k->up);
        r.usable = master && d.kind == Kind::Active && r.level > 0;
        r.coolMs = d.kind == Kind::Active && r.level > 0 ? std::max(0, coolMs) : 0;
        out.push_back(std::move(r));
    }
    return out;
}

int coolLeft(uint64_t usedAt, uint64_t now) {
    if (!usedAt || now < usedAt) return usedAt ? kCooldownMs : 0;
    const uint64_t gone = now - usedAt;
    return gone >= uint64_t(kCooldownMs) ? 0 : int(uint64_t(kCooldownMs) - gone);
}

std::string clock(int ms) {
    const int s = std::max(0, (ms + 999) / 1000);
    const int sec = s % 60;
    return std::to_string(s / 60) + (sec < 10 ? ":0" : ":") + std::to_string(sec);
}

int places(int extension) { return 16 + 6 * std::clamp(extension, 0, 10); }

std::string auraBonus(int id, int level) {
    const char* stat = id == 10006 ? "STR" : id == 10007 ? "VIT" : id == 10008 ? "AGI" : id == 10009 ? "DEX" : nullptr;
    if (!stat || level <= 0) return {};
    return "+" + std::to_string(level) + " " + stat;
}

std::vector<LevelLine> levels(int id, int level, const std::string& placesWords) {
    std::vector<LevelLine> out;
    const Def* d = def(id);
    if (!d) return out;
    if (d->kind == Kind::Aura) {
        for (int n = 1; n <= d->most; ++n) out.push_back(LevelLine{n, auraBonus(id, n), n == level});
    } else if (id == 10011) {
        const std::string x = kTimes;
        out.push_back(LevelLine{1, "HP " + x + "2", level == 1});
        out.push_back(LevelLine{2, "HP " + x + "2" + kDot + "SP " + x + "2", level == 2});
        out.push_back(LevelLine{3, "HP " + x + "3" + kDot + "SP " + x + "3", level == 3});
    } else if (id == 10010) {
        out.push_back(LevelLine{1, std::string("+5 STR") + kDot + "INT" + kDot + "DEX", level == 1});
    } else if (id == 10012) {
        out.push_back(LevelLine{1, std::string("90 % HP") + kDot + "SP", level == 1});
    } else if (id == 10004) {
        out.push_back(LevelLine{level, fill(placesWords, std::to_string(places(level))), true});
        if (level < d->most) out.push_back(LevelLine{level + 1, fill(placesWords, std::to_string(places(level + 1)))});
    }
    return out;
}

}  // namespace uagfx::guildskills

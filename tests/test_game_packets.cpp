#include <cstdint>
#include <cstring>
#include <string>
#include <variant>
#include <vector>

#include "core/game_packets.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.3: the zone server's packets the driver reads as uaRO.exe takes them (win/packet_tap.cpp)
namespace {
std::vector<uint8_t> packet(uint16_t id, size_t size) {
    std::vector<uint8_t> p(size, 0);
    p[0] = uint8_t(id), p[1] = uint8_t(id >> 8);
    return p;
}
void put16(std::vector<uint8_t>& p, size_t at, uint16_t v) { p[at] = uint8_t(v), p[at + 1] = uint8_t(v >> 8); }
void put32(std::vector<uint8_t>& p, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[at + size_t(i)] = uint8_t(v >> (8 * i));
}
void putText(std::vector<uint8_t>& p, size_t at, const std::string& s) { std::memcpy(&p[at], s.data(), s.size()); }
}  // namespace

TEST(game_packets_a_whisper) {
    const std::string text = "\xef\xf0\xe8\xe2\xb3\xf2, are you there?";  // cp1251 bytes kept as they came
    std::vector<uint8_t> p = packet(0x0097, 28 + text.size() + 1);
    put16(p, 2, uint16_t(p.size()));
    putText(p, 4, "Priest");
    putText(p, 28, text);
    const packets::Event e = packets::parse(p.data(), p.size());
    const auto* w = std::get_if<packets::Whisper>(&e);
    CHECK(w != nullptr);
    if (!w) return;
    CHECK_EQ(w->from, std::string("Priest"));
    CHECK_EQ(w->text, text);
}

TEST(game_packets_a_whisper_cut_short_is_nothing) {
    std::vector<uint8_t> p = packet(0x0097, 40);
    put16(p, 2, 400);  // says more than there is
    CHECK(std::holds_alternative<std::monostate>(packets::parse(p.data(), p.size())));
    put16(p, 2, 20);  // less than its own head
    CHECK(std::holds_alternative<std::monostate>(packets::parse(p.data(), p.size())));
}

TEST(game_packets_invitations_and_a_trade) {
    std::vector<uint8_t> party = packet(0x00fe, 30);
    put32(party, 2, 2000123);
    putText(party, 6, "Heroes");
    const packets::Event pe = packets::parse(party.data(), party.size());
    CHECK(std::holds_alternative<packets::PartyInvite>(pe));
    if (const auto* x = std::get_if<packets::PartyInvite>(&pe)) CHECK_EQ(x->party, std::string("Heroes"));
    std::vector<uint8_t> guild = packet(0x016a, 30);
    put32(guild, 2, 77);
    putText(guild, 6, "Kyiv Knights");
    const packets::Event ge = packets::parse(guild.data(), guild.size());
    if (const auto* x = std::get_if<packets::GuildInvite>(&ge)) CHECK_EQ(x->guild, std::string("Kyiv Knights"));
    else CHECK(false);
    std::vector<uint8_t> trade = packet(0x00e5, 26);
    putText(trade, 2, "Merchant");
    const packets::Event te = packets::parse(trade.data(), trade.size());
    if (const auto* x = std::get_if<packets::TradeRequest>(&te)) CHECK_EQ(x->from, std::string("Merchant"));
    else CHECK(false);
    std::vector<uint8_t> friendAsk = packet(0x0207, 34);
    put32(friendAsk, 2, 2000555);
    put32(friendAsk, 6, 150001);
    putText(friendAsk, 10, "Sage");
    const packets::Event fe = packets::parse(friendAsk.data(), friendAsk.size());
    if (const auto* x = std::get_if<packets::FriendRequest>(&fe)) CHECK_EQ(x->from, std::string("Sage"));
    else CHECK(false);
}

TEST(game_packets_a_name_without_its_end_stops_at_24) {
    std::vector<uint8_t> trade = packet(0x00e5, 26);
    std::memset(&trade[2], 'A', 24);
    const packets::Event te = packets::parse(trade.data(), trade.size());
    if (const auto* x = std::get_if<packets::TradeRequest>(&te)) CHECK_EQ(x->from.size(), size_t(24));
    else CHECK(false);
}

TEST(game_packets_status_icons_deaths_and_buffs_cast) {
    std::vector<uint8_t> icon = packet(0x0196, 9);
    put16(icon, 2, 10);  // SI_BLESSING
    put32(icon, 4, 2000123);
    icon[8] = 1;
    const packets::Event ie = packets::parse(icon.data(), icon.size());
    const auto* i = std::get_if<packets::StatusIcon>(&ie);
    CHECK(i && i->icon == 10 && i->id == 2000123 && i->on);
    std::vector<uint8_t> gone = packet(0x0080, 7);
    put32(gone, 2, 2000123);
    gone[6] = 1;
    const packets::Event ve = packets::parse(gone.data(), gone.size());
    const auto* v = std::get_if<packets::Vanish>(&ve);
    CHECK(v && v->id == 2000123 && v->type == 1);
    std::vector<uint8_t> cast = packet(0x011a, 15);
    put16(cast, 2, 34);  // AL_BLESSING
    put16(cast, 4, 10);  // its level
    put32(cast, 6, 2000123);
    put32(cast, 10, 2000456);
    cast[14] = 1;
    const packets::Event ce = packets::parse(cast.data(), cast.size());
    const auto* c = std::get_if<packets::SkillCast>(&ce);
    CHECK(c && c->skill == 34 && c->level == 10 && c->target == 2000123 && c->source == 2000456);
    CHECK(std::holds_alternative<std::monostate>(packets::parse(icon.data(), 8)));  // cut short
    std::vector<uint8_t> other = packet(0x0088, 10);
    CHECK(std::holds_alternative<std::monostate>(packets::parse(other.data(), other.size())));
}

TEST(game_packets_hits_on_a_unit) {
    // 3.0.4-beta.4: a helper window's character hit - marked on the main window's panel
    std::vector<uint8_t> act = packet(0x008a, 29);
    put32(act, 2, 1100001);  // a monster
    put32(act, 6, 2000123);  // the character
    put16(act, 22, 57);
    act[26] = 0;  // damage
    const packets::Event ae = packets::parse(act.data(), act.size());
    const auto* a = std::get_if<packets::Hit>(&ae);
    CHECK(a && a->source == 1100001 && a->target == 2000123 && a->damage == 57);
    act[26] = 10;  // a critical one
    CHECK(std::holds_alternative<packets::Hit>(packets::parse(act.data(), act.size())));
    act[26] = 2;  // sitting down: no hit
    CHECK(std::holds_alternative<std::monostate>(packets::parse(act.data(), act.size())));
    act[26] = 0;
    put16(act, 22, 0);  // a miss
    CHECK(std::holds_alternative<std::monostate>(packets::parse(act.data(), act.size())));
    std::vector<uint8_t> skill = packet(0x01de, 33);
    put16(skill, 2, 19);  // a bolt
    put32(skill, 4, 1100001);
    put32(skill, 8, 2000123);
    put32(skill, 24, 1234);
    const packets::Event se = packets::parse(skill.data(), skill.size());
    const auto* s = std::get_if<packets::Hit>(&se);
    CHECK(s && s->target == 2000123 && s->damage == 1234);
    std::vector<uint8_t> old = packet(0x0114, 31);
    put32(old, 8, 2000123);
    put16(old, 24, 88);
    const packets::Event oe = packets::parse(old.data(), old.size());
    const auto* o = std::get_if<packets::Hit>(&oe);
    CHECK(o && o->target == 2000123 && o->damage == 88);
}

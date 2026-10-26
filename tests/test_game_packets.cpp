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
    put16(act, 22, 0);  // 3.0.7-beta.6: a miss - an attack all the same (the notification), no damage (no mark)
    const packets::Event me = packets::parse(act.data(), act.size());
    const auto* m = std::get_if<packets::Hit>(&me);
    CHECK(m && m->target == 2000123 && m->damage == 0);
    act[26] = 11;  // dodged by luck: an attack too
    CHECK(std::holds_alternative<packets::Hit>(packets::parse(act.data(), act.size())));
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

TEST(game_packets_the_party_its_members_and_their_hp) {
    // 3.0.7-beta.6 (the user's notification 6): who is in the party (its list, one joining), one leaving, their HP
    std::vector<uint8_t> list = packet(0x00fb, 28 + 46 * 2);
    put16(list, 2, uint16_t(list.size()));
    putText(list, 4, "Heroes");
    put32(list, 28, 2000123);
    putText(list, 32, "Sora");
    put32(list, 28 + 46, 2000456);
    putText(list, 28 + 46 + 4, "Priest");
    const packets::Event le = packets::parse(list.data(), list.size());
    const auto* l = std::get_if<packets::PartyList>(&le);
    CHECK(l && l->members.size() == 2);
    if (l && l->members.size() == 2) {
        CHECK(l->members[0].first == 2000123 && l->members[0].second == "Sora");
        CHECK(l->members[1].first == 2000456 && l->members[1].second == "Priest");
    }
    put16(list, 2, uint16_t(list.size() + 46));  // naming more than there is
    CHECK(std::holds_alternative<std::monostate>(packets::parse(list.data(), list.size())));
    std::vector<uint8_t> joined = packet(0x01e9, 81);
    put32(joined, 2, 2000789);
    putText(joined, 15, "Heroes");
    putText(joined, 39, "Sage");
    const packets::Event je = packets::parse(joined.data(), joined.size());
    const auto* j = std::get_if<packets::PartyMember>(&je);
    CHECK(j && j->account == 2000789 && j->name == "Sage");
    std::vector<uint8_t> left = packet(0x0105, 31);
    put32(left, 2, 2000789);
    putText(left, 6, "Sage");
    const packets::Event ge = packets::parse(left.data(), left.size());
    const auto* g = std::get_if<packets::PartyLeft>(&ge);
    CHECK(g && g->account == 2000789);
    std::vector<uint8_t> hp = packet(0x0106, 10);
    put32(hp, 2, 2000456);
    put16(hp, 6, 312);
    put16(hp, 8, 4120);
    const packets::Event he = packets::parse(hp.data(), hp.size());
    const auto* h = std::get_if<packets::PartyHp>(&he);
    CHECK(h && h->account == 2000456 && h->hp == 312 && h->most == 4120);
    CHECK(std::holds_alternative<std::monostate>(packets::parse(hp.data(), 9)));  // cut short
}

TEST(game_packets_the_party_s_the_guild_s_and_the_near_chat) {
    // 3.0.7-beta.6 (the user's notifications 7, 8): "Name : words" as the server sends them
    const std::string said = "Priest : heal?";
    std::vector<uint8_t> party = packet(0x0109, 8 + said.size() + 1);
    put16(party, 2, uint16_t(party.size()));
    put32(party, 4, 2000456);
    putText(party, 8, said);
    const packets::Event pe = packets::parse(party.data(), party.size());
    const auto* p = std::get_if<packets::PartyChat>(&pe);
    CHECK(p && p->account == 2000456 && p->text == said);
    std::vector<uint8_t> guild = packet(0x017f, 4 + said.size() + 1);
    put16(guild, 2, uint16_t(guild.size()));
    putText(guild, 4, said);
    const packets::Event ge = packets::parse(guild.data(), guild.size());
    const auto* g = std::get_if<packets::GuildChat>(&ge);
    CHECK(g && g->text == said);
    std::vector<uint8_t> near = packet(0x008d, 8 + said.size() + 1);
    put16(near, 2, uint16_t(near.size()));
    put32(near, 4, 2000999);
    putText(near, 8, said);
    const packets::Event ne = packets::parse(near.data(), near.size());
    const auto* n = std::get_if<packets::Chat>(&ne);
    CHECK(n && n->unit == 2000999 && n->text == said);
    put16(near, 2, uint16_t(near.size() + 10));  // longer than it is
    CHECK(std::holds_alternative<std::monostate>(packets::parse(near.data(), near.size())));
}

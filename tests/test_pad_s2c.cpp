#include <cstring>

#include "pad/core/proto/packet_tables.hpp"
#include "pad/core/proto/s2c_build.hpp"
#include "pad/core/proto/s2c_parser.hpp"
#include "test.hpp"

using namespace uapad;

static Bytes pkt(uint16_t id, size_t len) {
    Bytes b(len, 0);
    wr16(b, 0, id);
    return b;
}

template <class T>
static const T* as(const std::optional<Event>& e) {
    return e ? std::get_if<T>(&*e) : nullptr;
}

TEST(s2c_self_position_packets) {
    Bytes a = pkt(0x0073, 11);
    encodePos3(a, 6, Cell{53, 111}, 4);
    auto e1 = parseS2C(a);
    auto* auth = as<EvAuthOk>(e1);
    CHECK(auth && auth->pos == (Cell{53, 111}));

    Bytes m = pkt(0x0091, 22);
    const char name[] = "prontera.gat";
    for (size_t i = 0; i + 1 < sizeof(name); ++i) m[2 + i] = uint8_t(name[i]);
    wr16(m, 18, 150);
    wr16(m, 20, 180);
    auto e2 = parseS2C(m);
    auto* mc = as<EvMapChange>(e2);
    CHECK(mc && mc->map == "prontera.gat" && mc->pos == (Cell{150, 180}));

    Bytes w = pkt(0x0087, 12);
    encodePos6(w, 6, Cell{150, 180}, Cell{155, 183});
    auto e3 = parseS2C(w);
    auto* sw = as<EvSelfWalk>(e3);
    CHECK(sw && sw->from == (Cell{150, 180}) && sw->to == (Cell{155, 183}));

    Bytes f = pkt(0x01ff, 10);
    wr32(f, 2, 2000001);
    wr16(f, 6, 40);
    wr16(f, 8, 41);
    auto e4 = parseS2C(f);
    auto* sp = as<EvSetPos>(e4);
    CHECK(sp && sp->id == 2000001u && sp->pos == (Cell{40, 41}));
}

TEST(s2c_monster_walking_into_view_0x007b) {  // uAthena clif_set007b for a non-player, PACKETVER < 7
    Bytes p = pkt(0x007b, 60);
    wr32(p, 2, 110000503);
    wr16(p, 6, 300);
    wr16(p, 14, 1063);  // Lunatic
    encodePos6(p, 50, Cell{30, 40}, Cell{35, 44});
    auto e = parseS2C(p);
    auto* a = as<EvActor>(e);
    CHECK(a && a->id == 110000503u && a->cls == 1063 && a->hair == 0 && a->speed == 300);
    CHECK(a && a->from == (Cell{30, 40}) && a->to == (Cell{35, 44}));
}

TEST(s2c_unit_state_is_read_from_every_spawn_packet) {
    Bytes idle = pkt(0x0078, 54);
    wr32(idle, 2, 110000600);
    wr16(idle, 8, 3);       // opt1
    wr16(idle, 10, 0x10);   // opt2
    wr16(idle, 12, 0x4);    // option
    wr16(idle, 14, 1002);
    auto e1 = parseS2C(idle);
    auto* a = as<EvActor>(e1);
    CHECK(a && a->opt1 == 3 && a->opt2 == 0x10 && a->option == 0x4);
    Bytes walk = pkt(0x022c, 65);
    wr32(walk, 2, 110000601);
    wr16(walk, 8, 1);
    wr16(walk, 10, 2);
    wr32(walk, 12, 0x2000);  // a 4-byte option in the PACKETVER 7 packet
    wr16(walk, 16, 1113);
    auto e2 = parseS2C(walk);
    auto* w = as<EvActor>(e2);
    CHECK(w && w->opt1 == 1 && w->opt2 == 2 && w->option == 0x2000u);
    Bytes spawn = pkt(0x007c, 41);
    wr32(spawn, 2, 110000602);
    wr16(spawn, 12, 0x8);
    wr16(spawn, 20, 1002);
    auto e3 = parseS2C(spawn);
    auto* sp = as<EvActor>(e3);
    CHECK(sp && sp->option == 0x8);
}

TEST(s2c_unit_state_change) {
    Bytes p = pkt(0x0229, 15);  // clif_changeoption, PACKETVER 7
    wr32(p, 2, 110000600);
    wr16(p, 6, 0);
    wr16(p, 8, 0x1);
    wr32(p, 10, 0x40);
    auto e = parseS2C(p);
    auto* o = as<EvActorOption>(e);
    CHECK(o && o->id == 110000600u && o->opt1 == 0 && o->opt2 == 1 && o->option == 0x40u);
    Bytes old = pkt(0x0119, 13);  // the older packet, still understood
    wr32(old, 2, 110000601);
    wr16(old, 10, 0x2);
    auto e2 = parseS2C(old);
    auto* o2 = as<EvActorOption>(e2);
    CHECK(o2 && o2->id == 110000601u && o2->option == 0x2u);
}

TEST(s2c_build_unit_state_for_the_client) {
    const Bytes b = buildUnitState(110000123, 1, 2, 0x2001);
    CHECK_EQ(b.size(), size_t(s2cLength(0x0229)));
    CHECK_EQ(rd16(b.data(), 0), 0x0229);
    CHECK_EQ(rd32(b.data(), 2), 110000123u);
    CHECK_EQ(rd16(b.data(), 6), 1);
    CHECK_EQ(rd16(b.data(), 8), 2);
    CHECK_EQ(rd32(b.data(), 10), 0x2001u);
    CHECK_EQ(b[14], 0);
}

TEST(s2c_monster_packets) {
    Bytes idle = pkt(0x0078, 54);
    wr32(idle, 2, 110000500);
    wr16(idle, 6, 400);
    wr16(idle, 14, 1002);  // Poring
    encodePos3(idle, 46, Cell{60, 70}, 0);
    auto e1 = parseS2C(idle);
    auto* a = as<EvActor>(e1);
    CHECK(a && a->id == 110000500u && a->cls == 1002 && a->hair == 0 && a->speed == 400);
    CHECK(a && a->from == (Cell{60, 70}) && a->to == (Cell{60, 70}));

    Bytes spawn = pkt(0x007c, 41);
    wr32(spawn, 2, 110000501);
    wr16(spawn, 14, 100);  // battle_config.pet_hair_style: this is a pet
    wr16(spawn, 20, 1002);
    encodePos3(spawn, 36, Cell{61, 71}, 0);
    auto e2 = parseS2C(spawn);
    auto* s = as<EvActor>(e2);
    CHECK(s && s->cls == 1002 && s->hair == 100 && s->from == (Cell{61, 71}));

    Bytes walk = pkt(0x022c, 64);
    wr32(walk, 2, 110000502);
    wr16(walk, 6, 200);
    wr16(walk, 16, 1113);  // Drops
    encodePos6(walk, 54, Cell{10, 20}, Cell{14, 20});
    auto e3 = parseS2C(walk);
    auto* wk = as<EvActor>(e3);
    CHECK(wk && wk->cls == 1113 && wk->from == (Cell{10, 20}) && wk->to == (Cell{14, 20}));

    Bytes mv = pkt(0x0086, 16);
    wr32(mv, 2, 110000502);
    encodePos6(mv, 6, Cell{14, 20}, Cell{14, 25});
    auto e4 = parseS2C(mv);
    auto* am = as<EvActorMove>(e4);
    CHECK(am && am->id == 110000502u && am->to == (Cell{14, 25}));

    Bytes gone = pkt(0x0080, 7);
    wr32(gone, 2, 110000500);
    gone[6] = 1;
    auto e5 = parseS2C(gone);
    auto* v = as<EvVanish>(e5);
    CHECK(v && v->id == 110000500u && v->type == 1);
}

TEST(s2c_combat_packets) {
    Bytes hit = pkt(0x008a, 29);
    wr32(hit, 2, 2000001);
    wr32(hit, 6, 110000500);
    auto e1 = parseS2C(hit);
    auto* act = as<EvAction>(e1);
    CHECK(act && act->src == 2000001u && act->dst == 110000500u && act->type == 0);

    const uint16_t ids[] = {0x01de, 0x0114, 0x0115};
    const size_t lens[] = {33, 31, 35};
    for (int i = 0; i < 3; ++i) {
        Bytes sk = pkt(ids[i], lens[i]);
        wr16(sk, 2, 19);  // Fire Bolt
        wr32(sk, 4, 2000001);
        wr32(sk, 8, 110000500);
        auto e = parseS2C(sk);
        auto* h = as<EvSkillHit>(e);
        CHECK(h && h->skill == 19 && h->src == 2000001u && h->dst == 110000500u);
    }

    Bytes pet = pkt(0x01a4, 11);
    wr32(pet, 3, 110000777);
    auto e2 = parseS2C(pet);
    auto* pi = as<EvPetInfo>(e2);
    CHECK(pi && pi->type == 0 && pi->id == 110000777u);

    Bytes res = pkt(0x0148, 8);
    wr32(res, 2, 2000001);
    auto e3 = parseS2C(res);
    auto* r = as<EvResurrect>(e3);
    CHECK(r && r->id == 2000001u);
}

TEST(s2c_ground_items) {
    Bytes seen = pkt(0x009d, 17);
    wr32(seen, 2, 4242);
    wr16(seen, 6, 501);  // Red Potion
    wr16(seen, 9, 33);
    wr16(seen, 11, 44);
    wr16(seen, 13, 3);
    auto e1 = parseS2C(seen);
    auto* g1 = as<EvGroundItem>(e1);
    CHECK(g1 && g1->id == 4242u && g1->item == 501 && g1->pos == (Cell{33, 44}));

    Bytes drop = pkt(0x009e, 17);
    wr32(drop, 2, 4243);
    wr16(drop, 6, 909);  // Jellopy
    wr16(drop, 9, 35);
    wr16(drop, 11, 46);
    wr16(drop, 15, 1);
    auto e2 = parseS2C(drop);
    auto* g2 = as<EvGroundItem>(e2);
    CHECK(g2 && g2->id == 4243u && g2->item == 909 && g2->pos == (Cell{35, 46}));

    Bytes gone = pkt(0x00a1, 6);
    wr32(gone, 2, 4242);
    auto e3 = parseS2C(gone);
    auto* gg = as<EvGroundItemGone>(e3);
    CHECK(gg && gg->id == 4242u);
}

TEST(s2c_inventory_packets) {
    Bytes list = pkt(0x01ee, 4 + 2 * 18);
    wr16(list, 2, 4 + 2 * 18);
    wr16(list, 4, 2);     // protocol index (slot + 2)
    wr16(list, 6, 501);   // Red Potion
    wr16(list, 10, 15);   // amount
    wr16(list, 22, 3);
    wr16(list, 24, 601);  // Fly Wing
    wr16(list, 28, 4);
    auto e1 = parseS2C(list);
    auto* inv = as<EvInventory>(e1);
    CHECK(inv && inv->items.size() == 2);
    CHECK(inv && inv->items[0].index == 2 && inv->items[0].item == 501 && inv->items[0].amount == 15);
    CHECK(inv && inv->items[1].index == 3 && inv->items[1].item == 601 && inv->items[1].amount == 4);

    Bytes add = pkt(0x00a0, 23);
    wr16(add, 2, 5);
    wr16(add, 4, 2);
    wr16(add, 6, 502);  // Orange Potion
    auto e2 = parseS2C(add);
    auto* ad = as<EvItemAdd>(e2);
    CHECK(ad && ad->entry.index == 5 && ad->entry.item == 502 && ad->entry.amount == 2 && !ad->entry.equip);
    Bytes failed = add;
    failed[22] = 1;  // "inventory full": nothing was added
    CHECK(!parseS2C(failed).has_value());

    Bytes del = pkt(0x00af, 6);
    wr16(del, 2, 2);
    wr16(del, 4, 1);
    auto e3 = parseS2C(del);
    auto* rm = as<EvItemRemove>(e3);
    CHECK(rm && rm->index == 2 && rm->amount == 1);

    Bytes used = pkt(0x01c8, 13);
    wr16(used, 2, 2);
    wr16(used, 4, 501);
    wr32(used, 6, 2000001);
    wr16(used, 10, 14);
    used[12] = 1;
    auto e4 = parseS2C(used);
    auto* u = as<EvItemUsed>(e4);
    CHECK(u && u->index == 2 && u->remaining == 14 && u->user == 2000001u && u->ok);
}

TEST(s2c_skills_and_hotkeys) {
    Bytes sk = pkt(0x010f, 4 + 2 * 37);
    wr16(sk, 2, 4 + 2 * 37);
    wr16(sk, 4, 5);    // Bash
    wr32(sk, 6, 1);    // inf: attack
    wr16(sk, 10, 10);  // lv
    wr16(sk, 14, 1);   // range
    wr16(sk, 41, 28);  // Heal
    wr32(sk, 43, 16);  // inf: support
    wr16(sk, 47, 5);
    wr16(sk, 51, 9);
    auto e1 = parseS2C(sk);
    auto* sl = as<EvSkills>(e1);
    CHECK(sl && sl->skills.size() == 2);
    CHECK(sl && sl->skills[0].id == 5 && sl->skills[0].inf == 1u && sl->skills[0].lv == 10 && sl->skills[0].range == 1);
    CHECK(sl && sl->skills[1].id == 28 && sl->skills[1].inf == 16u && sl->skills[1].lv == 5 &&
          sl->skills[1].range == 9);

    Bytes up = pkt(0x010e, 11);
    wr16(up, 2, 28);
    wr16(up, 4, 6);
    wr16(up, 8, 9);
    auto e2 = parseS2C(up);
    auto* su = as<EvSkillUp>(e2);
    CHECK(su && su->id == 28 && su->lv == 6 && su->range == 9);

    Bytes hk = pkt(0x02b9, 191);
    hk[2 + 0 * 7] = 1;  // slot 0: skill
    wr32(hk, 2 + 0 * 7 + 1, 5);
    wr16(hk, 2 + 0 * 7 + 5, 10);
    hk[2 + 26 * 7] = 0;  // slot 26: item
    wr32(hk, 2 + 26 * 7 + 1, 501);
    auto e3 = parseS2C(hk);
    auto* hks = as<EvHotkeys>(e3);
    CHECK(hks && hks->keys[0] == (Hotkey{1, 5, 10}));
    CHECK(hks && hks->keys[26] == (Hotkey{0, 501, 0}));
    CHECK(hks && hks->keys[1].id == 0u);
}

TEST(s2c_short_or_irrelevant_packets_are_ignored) {
    CHECK(!parseS2C(pkt(0x0073, 5)).has_value());
    CHECK(!parseS2C(pkt(0x00b1, 8)).has_value());
    CHECK(!parseS2C(Bytes{0x73}).has_value());
}

TEST(s2c_self_speed_status) {
    Bytes b = pkt(0x00b0, 8);
    wr16(b, 2, 0);    // SP_SPEED
    wr32(b, 4, 112);  // e.g. with Increase AGI
    auto e = parseS2C(b);
    auto* sp = as<EvSelfSpeed>(e);
    CHECK(sp && sp->speed == 112);
    Bytes hp = pkt(0x00b0, 8);
    wr16(hp, 2, 5);  // SP_HP: not ours to track
    wr32(hp, 4, 300);
    CHECK(!parseS2C(hp).has_value());
    CHECK(!parseS2C(pkt(0x00b0, 8)).has_value());  // speed 0 is not a real value
}

TEST(s2c_build_emotion) {
    Bytes e = buildEmotion(110000123, kEmotionGasp);
    CHECK_EQ(int(e.size()), s2cLength(0x00c0));
    CHECK_EQ(rd16(e.data(), 0), 0x00c0);
    CHECK_EQ(rd32(e.data(), 2), 110000123u);
    CHECK_EQ(e[6], 0);
}

TEST(s2c_skill_names) {
    Bytes sk = pkt(0x010f, 4 + 2 * 37);
    wr16(sk, 2, 4 + 2 * 37);
    wr16(sk, 4, 46);
    std::memcpy(sk.data() + 16, "AC_DOUBLE", 9);
    wr16(sk, 41, 5);
    std::memcpy(sk.data() + 53, "ABCDEFGHIJKLMNOPQRSTUVWX", 24);  // all 24 bytes used, no terminator
    auto e = parseS2C(sk);
    auto* sl = as<EvSkills>(e);
    CHECK(sl && sl->skills.size() == 2 && sl->skills[0].name == "AC_DOUBLE");
    CHECK(sl && sl->skills.size() == 2 && sl->skills[1].name == "ABCDEFGHIJKLMNOPQRSTUVWX");
}

TEST(s2c_actor_names) {
    Bytes n = pkt(0x0095, 30);
    wr32(n, 2, 2000014);
    std::memcpy(n.data() + 6, "Hunter", 6);
    auto e = parseS2C(n);
    auto* name = as<EvActorName>(e);
    CHECK(name && name->id == 2000014u && name->name == "Hunter");
    Bytes full = pkt(0x0195, 102);  // with party and guild
    wr32(full, 2, 2000014);
    std::memcpy(full.data() + 6, "Hunter", 6);
    std::memcpy(full.data() + 30, "Party", 5);
    auto e2 = parseS2C(full);
    auto* name2 = as<EvActorName>(e2);
    CHECK(name2 && name2->name == "Hunter");
}

// 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», the live check of its HP bars): uAthena with
// show_mob_info 6 sends every one near a monster its name with «Lv. 24 | HP: 57%» in the party's field (0x0195, at 30)
// on each hit, heal and spawn; with show_mob_info 1 «HP: 300/600»
TEST(s2c_monster_name_carries_its_hp) {
    Bytes b = pkt(0x0195, 102);
    wr32(b, 2, 110000001);
    std::memcpy(b.data() + 6, "Eggyra", 6);
    std::memcpy(b.data() + 30, "Lv. 24 | HP: 57%", 16);
    std::memcpy(b.data() + 78, "Lv. 24 | HP: 57%", 16);
    const auto* n = as<EvActorName>(parseS2C(b));
    CHECK(n && n->id == 110000001u && n->name == "Eggyra" && n->hp == 57);
    std::memset(b.data() + 30, 0, 24);
    std::memcpy(b.data() + 30, "HP: 300/600", 11);
    n = as<EvActorName>(parseS2C(b));
    CHECK(n && n->hp == 50);
    std::memset(b.data() + 30, 0, 24);
    std::memcpy(b.data() + 30, "My Party", 8);  // a player's party: no HP
    n = as<EvActorName>(parseS2C(b));
    CHECK(n && n->hp == -1);
    Bytes small = pkt(0x0095, 30);
    wr32(small, 2, 5);
    n = as<EvActorName>(parseS2C(small));
    CHECK(n && n->hp == -1);
}

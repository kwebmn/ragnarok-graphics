// 3.0.24-beta.27 (docs/superpowers/plans/2026-11-27-other-windows-old-layout.md, task 5; the spec's «Група»:
// «Запросити»): a friend's account by its name, from the server's list (0x0201 - its length, then 32 bytes a friend:
// the account, the character, the name 24); a friend gone (0x020a) forgotten. The server's «friend added» (0x0209)
// writes the account's low 16 bits alone (uAthena clif_friendslist_reqack: WFIFOW) - not taken. The invite: 0x00fc
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core/friend_ids.hpp"
#include "core/item_packets.hpp"
#include "core/social_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

void put32(std::vector<uint8_t>& p, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[at + size_t(i)] = uint8_t(v >> (8 * i));
}

std::vector<uint8_t> list(const std::vector<std::pair<uint32_t, std::string>>& friends) {
    std::vector<uint8_t> p(4 + 32 * friends.size(), 0);
    p[0] = 0x01, p[1] = 0x02, p[2] = uint8_t(p.size()), p[3] = uint8_t(p.size() >> 8);
    for (size_t i = 0; i < friends.size(); ++i) {
        put32(p, 4 + 32 * i, friends[i].first);
        put32(p, 8 + 32 * i, 150000 + uint32_t(i));
        std::memcpy(p.data() + 12 + 32 * i, friends[i].second.data(), friends[i].second.size());
    }
    return p;
}

}  // namespace

TEST(friend_ids_from_the_list_and_an_add) {
    friendids::Ids ids;
    const std::vector<uint8_t> l = list({{2000123, "Tester"}, {2000777, "Big Mike"}});
    ids.feed(0x0201, l.data(), l.size());
    CHECK_EQ(ids.accountOf("Tester"), 2000123u);
    CHECK_EQ(ids.accountOf("Big Mike"), 2000777u);
    CHECK_EQ(ids.accountOf("Nobody"), 0u);
    std::vector<uint8_t> added(36, 0);  // the server's «added»: its account cut to 16 bits - not taken
    added[0] = 0x09, added[1] = 0x02;
    put32(added, 4, 0x0000ABCD);
    std::memcpy(added.data() + 12, "Newcomer", 8);
    ids.feed(0x0209, added.data(), added.size());
    CHECK_EQ(ids.accountOf("Newcomer"), 0u);
    std::vector<uint8_t> gone(10, 0);  // 0x020a: the account, the character
    gone[0] = 0x0a, gone[1] = 0x02;
    put32(gone, 2, 2000777);
    ids.feed(0x020a, gone.data(), gone.size());
    CHECK_EQ(ids.accountOf("Big Mike"), 0u);
    CHECK_EQ(ids.accountOf("Tester"), 2000123u);
}

TEST(party_invite_packet) {
    CHECK(itempackets::partyInvite(0x01020304) == (itempackets::Bytes{0xfc, 0x00, 0x04, 0x03, 0x02, 0x01}));
}

TEST(party_invite_only_online_friends_not_members) {
    CHECK(friendids::canInvite(true, true, false));
    CHECK(!friendids::canInvite(false, true, false));  // no party of ours
    CHECK(!friendids::canInvite(true, false, false));  // not in the game
    CHECK(!friendids::canInvite(true, true, true));    // ours already
    socialcard::Entry e;
    e.online = true;
    const std::vector<socialcard::Item> m = socialcard::menu(socialcard::Tab::Friends, e, false, false, true);
    CHECK(m.size() >= 2u && m[0] == socialcard::Item::Whisper && m[1] == socialcard::Item::Invite);
    const std::vector<socialcard::Item> no = socialcard::menu(socialcard::Tab::Friends, e, false, false, false);
    CHECK(std::find(no.begin(), no.end(), socialcard::Item::Invite) == no.end());
}

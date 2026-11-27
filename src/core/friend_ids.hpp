#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace uagfx::friendids {

// 3.0.24-beta.27 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Група»: «Запросити»): the
// friends' accounts by their names (the game's code page), for the party's invite (0x00fc takes an account; the
// invite by a name, 0x02c4, has no handler on the server) - from the server's list 0x0201 (its length, then 32 bytes a
// friend: the account, the character, the name 24); 0x020a (the account, the character) a friend gone. The server's
// «friend added» 0x0209 writes the account's low 16 bits alone (uAthena clif_friendslist_reqack: WFIFOW) - not taken:
// a new friend is known after the next login's list
class Ids {
public:
    void feed(uint16_t id, const uint8_t* p, size_t size);
    uint32_t accountOf(const std::string& name) const;  // 0 none known

private:
    std::map<std::string, uint32_t> accounts_;
};
// A friend may be invited: we lead or are in a party, they are in the game and not of it
bool canInvite(bool inParty, bool online, bool member);

}  // namespace uagfx::friendids

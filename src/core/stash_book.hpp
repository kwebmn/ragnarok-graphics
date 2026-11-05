#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace uagfx::stashbook {

// 3.0.11-beta.286 (the owner's feature 15): what each of the player's characters carries and keeps, as last seen -
// a bag and a cart by the character, the storage by the account (the server's is the account's) - kept in
// uaro-graphics\stash.txt, one line a place: «<kind> <owner> <id>:<count>,...» (kind bag, cart or storage; the owner
// a character's name or an account's number, its spaces as %20). Searched from «Речі» and «Склад»: where else a thing
// with those words lies.
enum class Kind : int { Bag, Cart, Storage };
struct Place {
    Kind kind = Kind::Bag;
    std::string owner;
    bool operator<(const Place& o) const { return kind != o.kind ? kind < o.kind : owner < o.owner; }
};
class Book {
public:
    bool note(const Place& p, const std::map<int, int>& counts);  // true: changed
    std::string text() const;
    static Book parse(const std::string& text);
    // The places other than `here`s holding things `wanted` says yes to, each with their count, the most first
    std::vector<std::pair<Place, int>> find(const std::function<bool(int id)>& wanted,
                                             const std::vector<Place>& here) const;

private:
    std::map<Place, std::map<int, int>> places_;
};

struct Texts {
    std::string also, storage, cart;  // «Ще: {1}», «Склад», «візок»
};
// «Ще: Склад ×12 · Анна ×3 · Анна, візок ×1» (a bag by its character's name alone); "" none
std::string line(const std::vector<std::pair<Place, int>>& found, const Texts& t, size_t most = 4);

}  // namespace uagfx::stashbook

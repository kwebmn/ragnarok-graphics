#include "core/equip_sets.hpp"

#include <cstdio>
#include <sstream>

namespace uagfx::equipsets {
namespace ew = uagfx::equipwin;

namespace {
bool sameCards(const int a[4], const int b[4]) {
    for (int i = 0; i < 4; ++i)
        if (a[i] != b[i]) return false;
    return true;
}
}  // namespace

std::string Set::text() const {
    std::string out;
    char line[96];
    for (const Thing& t : things) {
        std::snprintf(line, sizeof line, "%d %d %d %d %d %d %d\n", t.slot, t.id, t.refine, t.cards[0], t.cards[1],
                      t.cards[2], t.cards[3]);
        out += line;
    }
    return out;
}

Set Set::parse(const std::string& text) {
    Set s;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        Thing t;
        std::istringstream l(line);
        if (!(l >> t.slot >> t.id >> t.refine >> t.cards[0] >> t.cards[1] >> t.cards[2] >> t.cards[3])) continue;
        if (t.slot < 0 || t.slot >= ew::kSlots || t.id <= 0) continue;
        s.things.push_back(t);
    }
    return s;
}

Set capture(const ew::Worn now[ew::kSlots]) {
    Set s;
    for (int i = 0; i < ew::kSlots; ++i) {
        const ew::Worn& w = now[i];
        if (w.index <= 0 || w.spill || w.id <= 0) continue;  // a two-handed weapon once, at the weapon's slot
        Thing t;
        t.slot = i, t.id = w.id, t.refine = w.refine;
        for (int c = 0; c < 4; ++c) t.cards[c] = w.cards[c];
        s.things.push_back(t);
    }
    return s;
}

namespace {
bool setHasSlot(const Set& set, int slot) {
    for (const Thing& t : set.things)
        if (t.slot == slot) return true;
    return false;
}
// 3.0.18-beta.220 (the owner: «снимать»): a thing worn now at `slot` (shown there first - not a two-hander's other
// half) where the set has nothing
bool extraAt(const Set& set, const ew::Worn now[ew::kSlots], int slot) {
    const ew::Worn& w = now[slot];
    return w.index > 0 && !w.spill && !setHasSlot(set, slot);
}
}  // namespace

std::vector<Step> plan(const Set& set, const std::vector<Owned>& bag, const ew::Worn now[ew::kSlots]) {
    std::vector<Step> out;
    for (int slot = 0; slot < ew::kSlots; ++slot)  // 3.0.18-beta.220: what the set has not taken off first
        if (extraAt(set, now, slot)) out.push_back(Step{now[slot].index, ew::kBits[slot], slot, true});
    std::vector<bool> used(bag.size(), false);
    for (const Thing& t : set.things) {
        const ew::Worn& w = now[t.slot];
        if (w.index > 0 && w.id == t.id && w.refine == t.refine && sameCards(w.cards, t.cards)) continue;
        int found = -1;
        for (int pass = 0; pass < 2 && found < 0; ++pass)
            for (size_t i = 0; i < bag.size(); ++i) {
                const Owned& o = bag[i];
                if (used[i] || o.worn || o.id != t.id || o.refine != t.refine) continue;
                if (pass == 0 && !sameCards(o.cards, t.cards)) continue;
                found = int(i);
                break;
            }
        if (found < 0) continue;
        used[size_t(found)] = true;
        out.push_back(Step{bag[size_t(found)].index, ew::kBits[t.slot], t.slot});
    }
    return out;
}

namespace {
bool wornAt(const Thing& t, const ew::Worn now[ew::kSlots]) {
    const ew::Worn& w = now[t.slot];
    return w.index > 0 && w.id == t.id && w.refine == t.refine && sameCards(w.cards, t.cards);
}
}  // namespace

bool wornNow(const Set& set, const ew::Worn now[ew::kSlots]) {
    if (set.things.empty()) return false;
    for (const Thing& t : set.things)
        if (!wornAt(t, now)) return false;
    for (int slot = 0; slot < ew::kSlots; ++slot)  // 3.0.18-beta.220: and nothing else - just that set
        if (extraAt(set, now, slot)) return false;
    return true;
}

Click clickOf(const Set& set, const ew::Worn now[ew::kSlots]) {
    if (set.things.empty()) return Click::Keep;
    return wornNow(set, now) ? Click::AlreadyOn : Click::PutOn;
}

std::vector<Thing> missing(const Set& set, const std::vector<Owned>& bag, const ew::Worn now[ew::kSlots]) {
    std::vector<Thing> out;
    std::vector<bool> used(bag.size(), false);
    for (const Thing& t : set.things) {
        if (wornAt(t, now)) continue;
        bool found = false;
        for (size_t i = 0; i < bag.size() && !found; ++i)  // as plan: the same number and refine serve
            if (!used[i] && !bag[i].worn && bag[i].id == t.id && bag[i].refine == t.refine) found = used[i] = true;
        if (!found) out.push_back(t);
    }
    return out;
}

}  // namespace uagfx::equipsets

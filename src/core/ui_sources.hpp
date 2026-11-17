#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace uagfx::panel {
struct Picture;
}

// 3.0.21-beta.14 (R-DP7PVN: a game gone a second after «Довідник»'s card of Prontera opened, RmlUi's «Could not load
// texture: pic:1dc49ab8:1937876632» twice before it): a picture of the driver's shown by RmlUi is named by a source,
// «pic:<address>:<generation>» (win/uaui::picture). RmlUi loads a source when an element first needs it - frames
// later, when a row comes into view - and again after its release; the source used to be read back as the address,
// and a picture gone by then gave garbage (a size of gigabytes: std::bad_alloc out of the driver's frame, the game
// ended through std::terminate). Here a source keeps a copy of its picture's pixels from the moment it is named,
// while a place (a part's `previous`) holds it; RmlUi's loads read the copy, never the address.
namespace uagfx::uisources {

struct Kept {
    int width = 0, height = 0;
    std::shared_ptr<const std::vector<uint32_t>> px;  // null: the picture had no pixels when named (does not load)
    int places = 0;                                   // the places holding the source now
};

class Sources {
public:
    // The source of `p` at `generation` for a place that held `previous` (set to it; "" and `previous` cleared for
    // no picture). A source new to its place is kept from now (its pixels copied when no other place holds it); the
    // one it held is let go when no place holds it any more. `release` gets the sources RmlUi must release: the ones
    // let go (their textures) and the ones kept anew (a failed load of theirs from before cleared).
    std::string name(const panel::Picture* p, int generation, std::string& previous,
                     std::vector<std::string>& release);
    // What RmlUi's load of `source` gets: null for a source not kept (never named, or let go)
    const Kept* find(const std::string& source) const;
    size_t size() const { return kept_.size(); }

private:
    void leave(const std::string& source, std::vector<std::string>& release);
    std::unordered_map<std::string, Kept> kept_;
};

}  // namespace uagfx::uisources

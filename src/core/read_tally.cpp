#include "core/read_tally.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace uagfx::readtally {

void Tally::add(uint32_t from, double ms) {
    ++reads_;
    ms_ += ms;
    if (!from) {
        ++unknown_;
        return;
    }
    // open addressing: the place's slot by its hash, the next free one when taken; a full table - elsewhere
    uint32_t slot = ((from >> 2) * 2654435761u) >> 26;  // 6 bits: kPlaces
    for (int probe = 0; probe < kPlaces; ++probe, slot = (slot + 1) & (kPlaces - 1)) {
        Place& p = places_[slot];
        if (p.at == from || p.at == 0) {
            p.at = from;
            ++p.reads;
            p.ms += ms;
            return;
        }
    }
    ++elsewhere_;
}

std::string Tally::take(uint32_t base, int top) {
    std::string line;
    if (frames_) {
        const double f = double(frames_);
        char head[96];
        std::snprintf(head, sizeof head, "game memory: %.0f reads a frame (%.2f ms)", double(reads_) / f, ms_ / f);
        line = head;
        std::vector<const Place*> kept;
        for (const Place& p : places_)
            if (p.at) kept.push_back(&p);
        std::sort(kept.begin(), kept.end(), [](const Place* a, const Place* b) {
            return a->reads != b->reads ? a->reads > b->reads : a->at < b->at;
        });
        const char* sep = " - the most from ";
        for (int k = 0; k < top && k < int(kept.size()); ++k) {
            const Place& p = *kept[size_t(k)];
            char part[80];
            if (p.at >= base)
                std::snprintf(part, sizeof part, "%sddraw+0x%06x %.0f (%.2f ms)", sep, unsigned(p.at - base),
                              p.reads / f, p.ms / f);
            else
                std::snprintf(part, sizeof part, "%s0x%08x %.0f (%.2f ms)", sep, unsigned(p.at), p.reads / f,
                              p.ms / f);
            line += part;
            sep = ", ";
        }
        char rest[64] = "";
        if (elsewhere_) std::snprintf(rest, sizeof rest, "%selsewhere %.0f", sep, elsewhere_ / f), sep = ", ";
        line += rest;
        if (unknown_) std::snprintf(rest, sizeof rest, "%sunknown %.0f", sep, unknown_ / f), line += rest;
    }
    *this = Tally{};
    return line;
}

}  // namespace uagfx::readtally

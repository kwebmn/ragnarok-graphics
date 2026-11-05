#include "core/fight_trace.hpp"

#include <algorithm>
#include <cstdio>

namespace uagfx::fighttrace {

void way(const uint8_t* p, int& fromX, int& fromY, int& toX, int& toY) {
    fromX = (p[0] << 2) | (p[1] >> 6);
    fromY = ((p[1] & 0x3F) << 4) | (p[2] >> 4);
    toX = ((p[2] & 0x0F) << 6) | (p[3] >> 2);
    toY = ((p[3] & 0x03) << 8) | p[4];
}

Trace::Seen* Trace::find(uint32_t unit) {
    for (Seen& s : units_)
        if (s.unit == unit) return &s;
    return nullptr;
}

void Trace::add(const Event& e) {
    events_.push_back(e);
    while (!events_.empty() && (e.ms - events_.front().ms > kKeepMs || events_.size() > 400)) events_.pop_front();
    if (e.kind == Kind::HitIt || e.kind == Kind::HitsMe) {
        Seen* s = find(e.unit);
        if (!s) {
            units_.push_back(Seen{e.unit, 0, 0});
            if (units_.size() > 32) units_.pop_front();
            s = &units_.back();
        }
        s->hitMs = e.ms;
    }
}

bool Trace::fought(uint32_t unit, uint64_t ms) const {
    for (const Seen& s : units_)
        if (s.unit == unit) return ms - s.hitMs <= kFoughtMs;
    return false;
}

std::string line(const Event& e, uint64_t first) {
    char shown[40] = "";
    if (e.shownX >= 0) std::snprintf(shown, sizeof shown, " (shown at %d, %d)", e.shownX, e.shownY);
    char buf[160];
    const unsigned at = unsigned(e.ms - first);
    switch (e.kind) {
        case Kind::Move:
            std::snprintf(buf, sizeof buf, "+%u ms walks %d, %d -> %d, %d%s", at, e.a, e.b, e.c, e.d, shown);
            break;
        case Kind::Fix: std::snprintf(buf, sizeof buf, "+%u ms put on %d, %d%s", at, e.a, e.b, shown); break;
        case Kind::HitIt:
            std::snprintf(buf, sizeof buf, "+%u ms hit by the character (swing %d ms, flinch %d ms, %d)%s", at, e.a,
                          e.b, e.c, shown);
            break;
        case Kind::HitsMe:
            std::snprintf(buf, sizeof buf, "+%u ms hits the character (swing %d ms, flinch %d ms, %d)%s", at, e.a, e.b,
                          e.c, shown);
            break;
        case Kind::Died: std::snprintf(buf, sizeof buf, "+%u ms died%s", at, shown); break;
    }
    return buf;
}

std::string Trace::dump(uint32_t unit, uint64_t ms, const char* why) {
    Seen* s = find(unit);
    if (!s || dumps_ >= kDumpsMost || (s->toldMs && ms - s->toldMs < 1000)) return {};
    std::string out;
    uint64_t first = 0;
    for (const Event& e : events_) {
        if (e.unit != unit || ms - e.ms > kKeepMs) continue;
        if (out.empty()) first = e.ms;
        out += "\n    " + line(e, first);
    }
    if (out.empty()) return {};
    ++dumps_;
    s->toldMs = ms;
    char head[160];
    std::snprintf(head, sizeof head, "fight trace: %u %s - its last %u ms:", unit, why,
                  unsigned(std::min<uint64_t>(ms - first, kKeepMs)));
    return head + out;
}

}  // namespace uagfx::fighttrace

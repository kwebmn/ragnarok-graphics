#include "core/bridge_wire.hpp"

#include <cstring>

namespace uagfx::bridge {

void WireWriter::str(const std::string& s) {
    u32(uint32_t(s.size()));
    bytes(s.data(), s.size());
}

const uint8_t* WireReader::take(size_t n) {
    if (!ok_ || size_t(end_ - p_) < n) {
        ok_ = false;
        return nullptr;
    }
    const uint8_t* at = p_;
    p_ += n;
    return at;
}

uint8_t WireReader::u8() {
    const uint8_t* b = take(1);
    return b ? *b : 0;
}

uint16_t WireReader::u16() {
    uint16_t v = 0;
    if (const uint8_t* b = take(2)) std::memcpy(&v, b, 2);
    return v;
}

uint32_t WireReader::u32() {
    uint32_t v = 0;
    if (const uint8_t* b = take(4)) std::memcpy(&v, b, 4);
    return v;
}

uint64_t WireReader::u64() {
    uint64_t v = 0;
    if (const uint8_t* b = take(8)) std::memcpy(&v, b, 8);
    return v;
}

float WireReader::f32() {
    float v = 0;
    if (const uint8_t* b = take(4)) std::memcpy(&v, b, 4);
    return v;
}

double WireReader::f64() {
    double v = 0;
    if (const uint8_t* b = take(8)) std::memcpy(&v, b, 8);
    return v;
}

std::string WireReader::str() {
    const uint32_t n = u32();
    const uint8_t* b = take(n);
    return b ? std::string(reinterpret_cast<const char*>(b), n) : std::string();
}

Rect WireReader::rect() {
    Rect r;
    r.left = i32();
    r.top = i32();
    r.right = i32();
    r.bottom = i32();
    return r;
}

namespace {

// 3.0.6-beta.3: a value into its place and the place after it - as WireWriter writes it (little-endian as it is)
template <typename T>
uint8_t* put(uint8_t* p, T v) {
    std::memcpy(p, &v, sizeof v);
    return p + sizeof v;
}

uint8_t* putRect(uint8_t* p, const Rect& r) {
    p = put(p, int32_t(r.left));
    p = put(p, int32_t(r.top));
    p = put(p, int32_t(r.right));
    return put(p, int32_t(r.bottom));
}

// The bytes of each kind's item, as written: the reader counts with them too
constexpr size_t kCommandBytes = 5, kClearBytes = 28, kViewportBytes = 24, kDrawBytes = 62, kUploadBytes = 37,
                 kSlotBytes = 21, kWriteBytes = 20, kStageBytes = 16, kSetupBytes = 17 + 2 * kStageBytes;

uint8_t* putStage(uint8_t* p, const StageSetup& s) {
    const uint8_t b[12] = {s.colorOp,   s.colorArg1, s.colorArg2, s.alphaOp,   s.alphaArg1, s.alphaArg2,
                           s.texCoord,  s.addressU,  s.addressV,  s.magFilter, s.minFilter, s.mipFilter};
    std::memcpy(p, b, sizeof b);
    return put(p + sizeof b, s.borderColor);
}

const uint8_t* getRect(const uint8_t* p, Rect& r) {
    int32_t v[4];
    std::memcpy(v, p, sizeof v);
    r.left = v[0];
    r.top = v[1];
    r.right = v[2];
    r.bottom = v[3];
    return p + sizeof v;
}

template <typename T>
const uint8_t* get(const uint8_t* p, T& v) {
    std::memcpy(&v, p, sizeof v);
    return p + sizeof v;
}

const uint8_t* getStage(const uint8_t* p, StageSetup& s) {
    s.colorOp = p[0];
    s.colorArg1 = p[1];
    s.colorArg2 = p[2];
    s.alphaOp = p[3];
    s.alphaArg1 = p[4];
    s.alphaArg2 = p[5];
    s.texCoord = p[6];
    s.addressU = p[7];
    s.addressV = p[8];
    s.magFilter = p[9];
    s.minFilter = p[10];
    s.mipFilter = p[11];
    return get(p + 12, s.borderColor);
}

const uint8_t* getSetup(const uint8_t* p, DrawSetup& s) {
    s.zEnable = p[0] != 0;
    s.zWrite = p[1] != 0;
    s.zFunc = p[2];
    s.cull = p[3];
    s.blend = p[4] != 0;
    s.srcBlend = p[5];
    s.dstBlend = p[6];
    s.alphaTest = p[7] != 0;
    s.alphaFunc = p[8];
    s.alphaRef = p[9];
    s.fog = p[10] != 0;
    s.colorKey = p[11] != 0;
    s.ui = p[12] != 0;
    p = get(p + 13, s.fogColor);
    p = getStage(p, s.stage[0]);
    return getStage(p, s.stage[1]);
}

// A count before its items: never more items than the bytes left could hold (`least` bytes each).
bool count(WireReader& rd, size_t least, uint32_t& n) {
    n = rd.u32();
    return rd.ok() && size_t(n) <= rd.left() / least;
}

}  // namespace

// 3.0.6-beta.3 (R-ZSDWYU: WireWriter::bytes the driver's busiest place in a FirePro laptop's game thread): each kind's
// items into their room at once - the same bytes as written field by field before
void writeRecord(WireWriter& w, const FrameRecord& r) {
    w.u32(uint32_t(r.commands().size()));
    uint8_t* p = w.grow(r.commands().size() * kCommandBytes);
    for (const Command& c : r.commands()) {
        *p++ = uint8_t(c.type);
        p = put(p, c.index);
    }
    w.u32(uint32_t(r.clears().size()));
    p = w.grow(r.clears().size() * kClearBytes);
    for (const ClearCmd& c : r.clears()) {
        p = put(p, c.flags);
        p = put(p, c.argb);
        p = put(p, c.z);
        p = putRect(p, c.rect);
    }
    w.u32(uint32_t(r.viewports().size()));
    p = w.grow(r.viewports().size() * kViewportBytes);
    for (const ViewportCmd& v : r.viewports()) {
        p = putRect(p, v.rect);
        p = put(p, v.minZ);
        p = put(p, v.maxZ);
    }
    w.u32(uint32_t(r.draws().size()));
    p = w.grow(r.draws().size() * kDrawBytes);
    for (const DrawCmd& d : r.draws()) {
        p = put(p, d.state);
        p = put(p, d.texture[0]);
        p = put(p, d.texture[1]);
        *p++ = uint8_t(d.keyed[0]);
        *p++ = uint8_t(d.keyed[1]);
        *p++ = d.effect;
        *p++ = d.effectSeed;
        p = put(p, d.effectId);  // 2.7.21
        p = put(p, d.primitive);
        p = put(p, d.fvf);
        p = put(p, d.vertexOffset);
        p = put(p, d.vertexCount);
        p = put(p, d.firstIndex);
        p = put(p, d.indexCount);
        p = put(p, d.twin);
        p = put(p, int32_t(d.twinTile[0]));
        p = put(p, int32_t(d.twinTile[1]));
        p = put(p, d.twinScale);
    }
    w.u32(uint32_t(r.uploads().size()));
    p = w.grow(r.uploads().size() * kUploadBytes);
    for (const UploadCmd& u : r.uploads()) {
        p = put(p, u.texture);
        p = put(p, u.level);
        p = putRect(p, u.rect);
        p = put(p, u.pixels);
        p = put(p, u.firstSlot);
        p = put(p, u.slotCount);
        *p++ = u.fullFrom;
    }
    w.u32(uint32_t(r.slots().size()));
    p = w.grow(r.slots().size() * kSlotBytes);
    for (const SpriteCell& s : r.slots()) {
        p = putRect(p, s.rect);
        *p++ = s.source;
        p = put(p, s.own);  // 2.5.23
    }
    w.u32(uint32_t(r.writes().size()));
    p = w.grow(r.writes().size() * kWriteBytes);
    for (const WriteTargetCmd& t : r.writes()) {
        p = putRect(p, t.rect);
        p = put(p, t.pixels);
    }
    w.u32(uint32_t(r.states().size()));
    p = w.grow(r.states().size() * kSetupBytes);
    for (const DrawSetup& s : r.states()) {  // as writeSetup
        const uint8_t b[13] = {uint8_t(s.zEnable), uint8_t(s.zWrite), s.zFunc,      s.cull,         uint8_t(s.blend),
                               s.srcBlend,         s.dstBlend,        uint8_t(s.alphaTest), s.alphaFunc, s.alphaRef,
                               uint8_t(s.fog),     uint8_t(s.colorKey), uint8_t(s.ui)};
        std::memcpy(p, b, sizeof b);
        p = put(p + sizeof b, s.fogColor);
        p = putStage(p, s.stage[0]);
        p = putStage(p, s.stage[1]);
    }
    w.u32(uint32_t(r.vertexBytes().size()));
    w.bytes(r.vertexBytes().data(), r.vertexBytes().size());
    w.u32(uint32_t(r.indices().size()));
    w.bytes(r.indices().data(), r.indices().size() * sizeof(uint16_t));
    w.u32(uint32_t(r.pixels().size()));
    w.bytes(r.pixels().data(), r.pixels().size() * sizeof(uint32_t));
}

bool readRecord(WireReader& rd, FrameRecord& r) {
    r.reset();
    uint32_t n = 0;
    const auto fail = [&r]() {
        r.reset();
        return false;
    };
    // 3.0.6-beta.3: each kind's items taken at once (count() made sure the bytes are there), read from their place
    const uint8_t* p = nullptr;
    const auto items = [&](size_t bytes) {
        if (!count(rd, bytes, n)) return false;
        p = rd.take(size_t(n) * bytes);
        return p != nullptr || n == 0;
    };
    if (!items(kCommandBytes)) return fail();
    r.commands_.resize(n);
    for (Command& c : r.commands_) {
        if (p[0] > uint8_t(Cmd::WriteTarget)) return fail();
        c.type = Cmd(p[0]);
        p = get(p + 1, c.index);
    }
    if (!items(kClearBytes)) return fail();
    r.clears_.resize(n);
    for (ClearCmd& c : r.clears_) {
        p = get(p, c.flags);
        p = get(p, c.argb);
        p = get(p, c.z);
        p = getRect(p, c.rect);
    }
    if (!items(kViewportBytes)) return fail();
    r.viewports_.resize(n);
    for (ViewportCmd& v : r.viewports_) {
        p = getRect(p, v.rect);
        p = get(p, v.minZ);
        p = get(p, v.maxZ);
    }
    if (!items(kDrawBytes)) return fail();
    r.draws_.resize(n);
    for (DrawCmd& d : r.draws_) {
        p = get(p, d.state);
        p = get(p, d.texture[0]);
        p = get(p, d.texture[1]);
        d.keyed[0] = p[0] != 0;
        d.keyed[1] = p[1] != 0;
        d.effect = p[2];
        d.effectSeed = p[3];
        p = get(p + 4, d.effectId);  // 2.7.21
        p = get(p, d.primitive);
        p = get(p, d.fvf);
        p = get(p, d.vertexOffset);
        p = get(p, d.vertexCount);
        p = get(p, d.firstIndex);
        p = get(p, d.indexCount);
        p = get(p, d.twin);
        p = get(p, d.twinTile[0]);
        p = get(p, d.twinTile[1]);
        p = get(p, d.twinScale);
    }
    if (!items(kUploadBytes)) return fail();
    r.uploads_.resize(n);
    for (UploadCmd& u : r.uploads_) {
        p = get(p, u.texture);
        p = get(p, u.level);
        p = getRect(p, u.rect);
        p = get(p, u.pixels);
        p = get(p, u.firstSlot);
        p = get(p, u.slotCount);
        u.fullFrom = *p++;
    }
    if (!items(kSlotBytes)) return fail();
    r.slots_.resize(n);
    for (SpriteCell& s : r.slots_) {
        p = getRect(p, s.rect);
        s.source = *p++;
        p = get(p, s.own);
    }
    if (!items(kWriteBytes)) return fail();
    r.writes_.resize(n);
    for (WriteTargetCmd& t : r.writes_) {
        p = getRect(p, t.rect);
        p = get(p, t.pixels);
    }
    if (!items(kSetupBytes)) return fail();
    r.states_.resize(n);
    for (DrawSetup& s : r.states_) p = getSetup(p, s);
    if (!count(rd, 1, n)) return fail();
    if (const uint8_t* b = rd.take(n)) r.vertices_.assign(b, b + n);
    if (!count(rd, 2, n)) return fail();
    r.indices_.resize(n);
    if (const uint8_t* b = rd.take(size_t(n) * 2)) std::memcpy(r.indices_.data(), b, size_t(n) * 2);
    if (!count(rd, 4, n)) return fail();
    r.pixels_.resize(n);
    if (const uint8_t* b = rd.take(size_t(n) * 4)) std::memcpy(r.pixels_.data(), b, size_t(n) * 4);
    if (!rd.ok()) return fail();

    // Consistency: every command names an item that exists, every item stays inside the data it points into.
    for (const Command& c : r.commands_) {
        const size_t size = c.type == Cmd::Clear      ? r.clears_.size()
                            : c.type == Cmd::Viewport ? r.viewports_.size()
                            : c.type == Cmd::Draw     ? r.draws_.size()
                            : c.type == Cmd::Upload   ? r.uploads_.size()
                                                      : r.writes_.size();
        if (c.index >= size) return fail();
    }
    for (const DrawCmd& d : r.draws_) {
        const uint64_t end = uint64_t(d.vertexOffset) + uint64_t(d.vertexCount) * FrameRecord::stride(d.fvf);
        if (d.state >= r.states_.size() || end > r.vertices_.size() ||
            uint64_t(d.firstIndex) + d.indexCount > r.indices_.size())
            return fail();
    }
    const auto pixelsFit = [&r](const Rect& rect, uint32_t at) {
        if (rect.empty()) return true;
        return uint64_t(at) + uint64_t(rect.width()) * uint64_t(rect.height()) <= r.pixels_.size();
    };
    for (const UploadCmd& u : r.uploads_)
        if (!pixelsFit(u.rect, u.pixels) || uint64_t(u.firstSlot) + u.slotCount > r.slots_.size()) return fail();
    for (const SpriteCell& s : r.slots_)  // 2.5.23: a dyed frame's own colours within the pixels
        if (s.own && !pixelsFit(s.rect, s.own - 1)) return fail();
    for (const WriteTargetCmd& t : r.writes_)
        if (!pixelsFit(t.rect, t.pixels)) return fail();
    return true;
}

void writeStudioKeys(WireWriter& w, const StudioKeys& k) {
    w.u8(k.flags);
    w.i32(k.x);
    w.i32(k.y);
}

bool readStudioKeys(WireReader& rd, StudioKeys& k) {
    StudioKeys got;
    got.flags = rd.u8();
    got.x = rd.i32();
    got.y = rd.i32();
    if (!rd.ok()) return false;
    k = got;
    return true;
}

}  // namespace uagfx::bridge

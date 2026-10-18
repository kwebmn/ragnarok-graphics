#include "core/bridge_wire.hpp"

#include <cstring>

namespace uagfx::bridge {

void WireWriter::u32(uint32_t v) { bytes(&v, 4); }  // x86 on both sides: little-endian as it is
void WireWriter::u64(uint64_t v) { bytes(&v, 8); }
void WireWriter::f32(float v) { bytes(&v, 4); }
void WireWriter::f64(double v) { bytes(&v, 8); }

void WireWriter::bytes(const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    out_.insert(out_.end(), b, b + n);
}

void WireWriter::str(const std::string& s) {
    u32(uint32_t(s.size()));
    bytes(s.data(), s.size());
}

void WireWriter::rect(const Rect& r) {
    i32(r.left);
    i32(r.top);
    i32(r.right);
    i32(r.bottom);
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

void writeStage(WireWriter& w, const StageSetup& s) {
    const uint8_t b[12] = {s.colorOp,   s.colorArg1, s.colorArg2, s.alphaOp,   s.alphaArg1, s.alphaArg2,
                           s.texCoord,  s.addressU,  s.addressV,  s.magFilter, s.minFilter, s.mipFilter};
    w.bytes(b, sizeof b);
    w.u32(s.borderColor);
}

void readStage(WireReader& rd, StageSetup& s) {
    s.colorOp = rd.u8();
    s.colorArg1 = rd.u8();
    s.colorArg2 = rd.u8();
    s.alphaOp = rd.u8();
    s.alphaArg1 = rd.u8();
    s.alphaArg2 = rd.u8();
    s.texCoord = rd.u8();
    s.addressU = rd.u8();
    s.addressV = rd.u8();
    s.magFilter = rd.u8();
    s.minFilter = rd.u8();
    s.mipFilter = rd.u8();
    s.borderColor = rd.u32();
}

void writeSetup(WireWriter& w, const DrawSetup& s) {
    const uint8_t b[13] = {uint8_t(s.zEnable), uint8_t(s.zWrite), s.zFunc,      s.cull,         uint8_t(s.blend),
                           s.srcBlend,         s.dstBlend,        uint8_t(s.alphaTest), s.alphaFunc, s.alphaRef,
                           uint8_t(s.fog),     uint8_t(s.colorKey), uint8_t(s.ui)};
    w.bytes(b, sizeof b);
    w.u32(s.fogColor);
    writeStage(w, s.stage[0]);
    writeStage(w, s.stage[1]);
}

void readSetup(WireReader& rd, DrawSetup& s) {
    s.zEnable = rd.u8() != 0;
    s.zWrite = rd.u8() != 0;
    s.zFunc = rd.u8();
    s.cull = rd.u8();
    s.blend = rd.u8() != 0;
    s.srcBlend = rd.u8();
    s.dstBlend = rd.u8();
    s.alphaTest = rd.u8() != 0;
    s.alphaFunc = rd.u8();
    s.alphaRef = rd.u8();
    s.fog = rd.u8() != 0;
    s.colorKey = rd.u8() != 0;
    s.ui = rd.u8() != 0;
    s.fogColor = rd.u32();
    readStage(rd, s.stage[0]);
    readStage(rd, s.stage[1]);
}

// A count before its items: never more items than the bytes left could hold (`least` bytes each).
bool count(WireReader& rd, size_t least, uint32_t& n) {
    n = rd.u32();
    return rd.ok() && size_t(n) <= rd.left() / least;
}

}  // namespace

void writeRecord(WireWriter& w, const FrameRecord& r) {
    w.u32(uint32_t(r.commands().size()));
    for (const Command& c : r.commands()) {
        w.u8(uint8_t(c.type));
        w.u32(c.index);
    }
    w.u32(uint32_t(r.clears().size()));
    for (const ClearCmd& c : r.clears()) {
        w.u32(c.flags);
        w.u32(c.argb);
        w.f32(c.z);
        w.rect(c.rect);
    }
    w.u32(uint32_t(r.viewports().size()));
    for (const ViewportCmd& v : r.viewports()) {
        w.rect(v.rect);
        w.f32(v.minZ);
        w.f32(v.maxZ);
    }
    w.u32(uint32_t(r.draws().size()));
    for (const DrawCmd& d : r.draws()) {
        w.u32(d.state);
        w.u32(d.texture[0]);
        w.u32(d.texture[1]);
        w.u8(uint8_t(d.keyed[0]));
        w.u8(uint8_t(d.keyed[1]));
        w.u8(d.effect);
        w.u8(d.effectSeed);
        w.u32(d.primitive);
        w.u32(d.fvf);
        w.u32(d.vertexOffset);
        w.u32(d.vertexCount);
        w.u32(d.firstIndex);
        w.u32(d.indexCount);
        w.u32(d.twin);
        w.i32(d.twinTile[0]);
        w.i32(d.twinTile[1]);
        w.f64(d.twinScale);
    }
    w.u32(uint32_t(r.uploads().size()));
    for (const UploadCmd& u : r.uploads()) {
        w.u32(u.texture);
        w.u32(u.level);
        w.rect(u.rect);
        w.u32(u.pixels);
        w.u32(u.firstSlot);
        w.u32(u.slotCount);
    }
    w.u32(uint32_t(r.slots().size()));
    for (const SpriteCell& s : r.slots()) {
        w.rect(s.rect);
        w.u8(s.source);
    }
    w.u32(uint32_t(r.writes().size()));
    for (const WriteTargetCmd& t : r.writes()) {
        w.rect(t.rect);
        w.u32(t.pixels);
    }
    w.u32(uint32_t(r.states().size()));
    for (const DrawSetup& s : r.states()) writeSetup(w, s);
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
    if (!count(rd, 5, n)) return fail();
    r.commands_.resize(n);
    for (Command& c : r.commands_) {
        const uint8_t type = rd.u8();
        if (type > uint8_t(Cmd::WriteTarget)) return fail();
        c.type = Cmd(type);
        c.index = rd.u32();
    }
    if (!count(rd, 28, n)) return fail();
    r.clears_.resize(n);
    for (ClearCmd& c : r.clears_) {
        c.flags = rd.u32();
        c.argb = rd.u32();
        c.z = rd.f32();
        c.rect = rd.rect();
    }
    if (!count(rd, 24, n)) return fail();
    r.viewports_.resize(n);
    for (ViewportCmd& v : r.viewports_) {
        v.rect = rd.rect();
        v.minZ = rd.f32();
        v.maxZ = rd.f32();
    }
    if (!count(rd, 62, n)) return fail();
    r.draws_.resize(n);
    for (DrawCmd& d : r.draws_) {
        d.state = rd.u32();
        d.texture[0] = rd.u32();
        d.texture[1] = rd.u32();
        d.keyed[0] = rd.u8() != 0;
        d.keyed[1] = rd.u8() != 0;
        d.effect = rd.u8();
        d.effectSeed = rd.u8();
        d.primitive = rd.u32();
        d.fvf = rd.u32();
        d.vertexOffset = rd.u32();
        d.vertexCount = rd.u32();
        d.firstIndex = rd.u32();
        d.indexCount = rd.u32();
        d.twin = rd.u32();
        d.twinTile[0] = rd.i32();
        d.twinTile[1] = rd.i32();
        d.twinScale = rd.f64();
    }
    if (!count(rd, 36, n)) return fail();
    r.uploads_.resize(n);
    for (UploadCmd& u : r.uploads_) {
        u.texture = rd.u32();
        u.level = rd.u32();
        u.rect = rd.rect();
        u.pixels = rd.u32();
        u.firstSlot = rd.u32();
        u.slotCount = rd.u32();
    }
    if (!count(rd, 17, n)) return fail();
    r.slots_.resize(n);
    for (SpriteCell& s : r.slots_) {
        s.rect = rd.rect();
        s.source = rd.u8();
    }
    if (!count(rd, 20, n)) return fail();
    r.writes_.resize(n);
    for (WriteTargetCmd& t : r.writes_) {
        t.rect = rd.rect();
        t.pixels = rd.u32();
    }
    if (!count(rd, 49, n)) return fail();
    r.states_.resize(n);
    for (DrawSetup& s : r.states_) readSetup(rd, s);
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

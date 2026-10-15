#include "win/gpu/ai11.hpp"

#include <d3d11.h>
#include <dxgi1_4.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

#include "core/ai/texture_cache.hpp"
#include "core/ai/vram.hpp"
#include "core/sprite_exe.hpp"
#include "core/webp_image.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

// 0.14.1: the interface's big pictures - the loading screen, the login's backgrounds - up to 1024 a side, from 128.
constexpr int kUiPictureSide = 128;
constexpr int kUiPictureMax = 1024;
// The game's process has 2 GB of address space (4 GB on a 64-bit Windows) for everything: a picture larger than this
// a side is halved on its way in - a loading screen's 4096 x 4096 would take 85 MB with its levels, twice over while
// it is made.
constexpr int kLargestSide = 2048;
constexpr double kCellTexels = 4.0 * 1024 * 1024;  // the sprite frames' pictures put in a frame, at most

template <typename T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

Ai11::Ai11(ID3D11Device* device, ID3D11DeviceContext* context, IDXGIAdapter* adapter)
    : device_(device), ctx_(context), adapter_(adapter) {
    if (adapter_) adapter_->AddRef();
}

Ai11::~Ai11() {
    if (ready_) logNow(true);
    if (thread_) {
        EnterCriticalSection(&lock_);
        stop_ = true;
        in_.clear();  // what was asked for is not wanted any more
        LeaveCriticalSection(&lock_);
        SetEvent(wake_);
        WaitForSingleObject(thread_, 15000);  // a picture it is making
        CloseHandle(thread_);
    }
    if (wake_) CloseHandle(wake_);
    for (Result& r : out_) {
        release(r.srv);
        release(r.texture);
    }
    for (Result& r : cellsWaiting_) {
        release(r.srv);
        release(r.texture);
    }
    for (auto& c : copies_) {
        release(c.second.srv);
        release(c.second.texture);
    }
    if (lockMade_) DeleteCriticalSection(&lock_);
    release(adapter_);
}

bool Ai11::start(const std::string& gameDir, const AiSettings& s, std::string& error) {
    settings_ = s;
    scale_ = s.scale == 2 ? 2 : 4;
    minSide_ = std::min(s.minSize, s.maxSize);
    maxSide_ = std::max(s.minSize, s.maxSize);
    atlasLevels_ = scale_ == 4 ? 4 : 3;  // down to half the game's texels, as Direct3D 12's: sprites seldom get smaller
    pictures_ = std::make_unique<ai::ReadyPictures>(s);
    bool anyKind = false;
    for (int k = 0; k < ai::kKinds; ++k) anyKind = anyKind || pictures_->kindOn(ai::Kind(k));
    if (!anyKind) {
        error = "no kind of texture is chosen (the AI textures page)";
        return false;
    }
    const std::string packPath = gameDir + "\\uaro-textures.zip";
    std::string why;
    if (!pack_.open(packPath, why) && GetFileAttributesA(packPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        LOG_ERROR("AI textures: the texture pack cannot be read (%s)", why.c_str());
    if (!pack_.isOpen()) {
        error = "no texture pack (uaro-textures.zip) in the game's folder - there is nothing to show (its builder is "
                "on the Texture pack page of the settings)";
        return false;
    }
    std::vector<uint8_t> index;
    if (!pack_.read(pack::kIndexName, index)) index.clear();
    const pack::Reader* reader = &pack_;
    pictures_->addPack([reader](const std::string& name) { return reader->has(name); },
                       std::string(index.begin(), index.end()));
    systemBudget_ = videoMemory(nullptr);
    vramBudget_ = ai::copiesBudget(systemBudget_);
    InitializeCriticalSection(&lock_);
    lockMade_ = true;
    wake_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    thread_ = wake_ ? CreateThread(nullptr, 0, ioMain, this, 0, nullptr) : nullptr;
    if (!thread_) {
        error = "no thread to read the pictures";
        return false;
    }
    SetThreadPriority(thread_, THREAD_PRIORITY_BELOW_NORMAL);  // the game's own thread first
    ready_ = true;
    std::string kinds;
    for (int k = 0; k < ai::kKinds; ++k)
        kinds += std::string(k ? ", " : "") + ai::kindName(ai::Kind(k)) +
                 (pictures_->kindOn(ai::Kind(k)) ? " on" : " off");
    LOG_INFO("AI textures: on with Direct3D 11 - %s; x%d, textures %d-%d, ahead %s; the pack's pictures only%s",
             kinds.c_str(), scale_, minSide_, maxSide_, s.ahead ? "on" : "off",
             s.generate ? " (making new ones in the game needs Direct3D 12)" : "");
    LOG_INFO("AI textures: the texture pack uaro-textures.zip has %u pictures (%u keys known ahead, %u of a lower "
             "texture detail)%s",
             static_cast<unsigned>(pack_.count() - (pack_.has(pack::kIndexName) ? 1 : 0)),
             static_cast<unsigned>(pictures_->ahead()), static_cast<unsigned>(pictures_->aliases()),
             pack_.recovered() ? " - cut short, read by its pictures' headers, without its index" : "");
    LOG_INFO("AI textures: the copies keep to %llu MB of video memory (the system gives the game %llu MB)",
             static_cast<unsigned long long>(vramBudget_ >> 20), static_cast<unsigned long long>(systemBudget_ >> 20));
    return true;
}

// DXGI's budget for the process (Windows 10), else the card's own memory (7 and 8: DXGI 1.4 is not there).
uint64_t Ai11::videoMemory(uint64_t* used) const {
    if (used) *used = 0;
    if (!adapter_) return 0;
    IDXGIAdapter3* a3 = nullptr;
    if (SUCCEEDED(adapter_->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&a3)))) {
        DXGI_QUERY_VIDEO_MEMORY_INFO info{};
        const bool ok = SUCCEEDED(a3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info));
        a3->Release();
        if (ok) {
            if (used) *used = info.CurrentUsage;
            return info.Budget;
        }
    }
    DXGI_ADAPTER_DESC d{};
    return SUCCEEDED(adapter_->GetDesc(&d)) ? uint64_t(d.DedicatedVideoMemory) : 0;
}

void Ai11::noteUpload(uint32_t id, int width, int height, const UploadCmd& u, const uint32_t* argb,
                      const SpriteCell* cells, uint32_t cellCount) {
    if (!ready_ || u.level != 0 || !argb) return;
    if (cellCount || atlases_.count(id)) {  // the game's sprite cache: cell by cell
        noteCells(id, width, height, u, argb, cells, cellCount);
        return;
    }
    dropCopy(id);  // the game changed the texture: its own pixels again
    const bool whole = u.rect.left == 0 && u.rect.top == 0 && u.rect.width() == width && u.rect.height() == height;
    const int most = std::max(maxSide_, kUiPictureMax);  // the world's textures get the setting's at begin()
    if (!whole || width < minSide_ || height < minSide_ || width > most || height > most) {
        items_.erase(id);
        return;
    }
    Item& item = items_[id];
    item.state = Item::Kept;  // drawn again, it gets its turn again (a picture of the old pixels is not taken)
    item.ui = false;
    item.serial = ++serial_;
    if (++item.uploads > 3) {  // changed again and again (an effect): not worth it
        item.state = Item::Skipped;
        return;
    }
    item.width = width;
    item.height = height;
    item.key = ai::keyName(ai::textureKey(argb, width, height));
    item.soft = ai::softPicture(argb, width, height) || ai::softAlpha(argb, width, height);
    item.alike.clear();
    if (pictures_->hasAlike(width, height)) pictures_->alikeName(argb, width, height, item.alike);
    ai::Kind kind = ai::Kind::None;
    if (pictures_->knownKind(item.key, kind)) begin(id, item, kind);  // ahead: on its way before it is drawn
}

void Ai11::begin(uint32_t id, Item& item, ai::Kind kind) {
    item.kind = kind;
    item.ui = kind == ai::Kind::Icons;  // through noteUpload only the interface's big pictures have this kind
    item.state = Item::Skipped;
    if (!pictures_->kindOn(kind)) return;  // a lightmap, no world texture, or a kind that is off
    if (!item.ui && (item.width > maxSide_ || item.height > maxSide_)) return;  // past the page's texture sides
    // 0.22.2, 0.39.2: soft light alone, or a shape in a soft alpha, stays the game's.
    if (item.soft) {
        ++soft_;
        return;
    }
    Request r;
    if (!pictures_->texture(item.key, kind, r.name)) {
        if (item.alike.empty() || !pictures_->alikeFits(item.alike, kind)) {  // none made: the game's
            ++missing_[int(kind)];
            return;
        }
        r.name = item.alike;  // 0.49: a .jpg's, by its likeness
    }
    item.state = Item::Waiting;
    r.id = id;
    r.serial = item.serial;
    r.width = item.width;
    r.height = item.height;
    request(std::move(r));
}

void Ai11::noteWorldUse(uint32_t id, ai::Kind kind) {
    if (kind == ai::Kind::None) return;  // a second stage of a draw that is no ground: says nothing of the texture
    const auto it = items_.find(id);
    if (it == items_.end()) return;
    Item& item = it->second;
    item.lastUse = frame_;
    if (item.state == Item::Away) {  // 0.44: its copy was put away - back from the pack
        bringBack(id, item);
        return;
    }
    if (item.state != Item::Kept) return;
    ++seen_[size_t(kind)];
    begin(id, item, kind);
}

void Ai11::noteUiUse(uint32_t id) {
    const auto it = items_.find(id);
    if (it == items_.end()) return;
    Item& item = it->second;
    item.lastUse = frame_;
    if (item.state == Item::Away) {
        bringBack(id, item);
        return;
    }
    if (item.state != Item::Kept || item.width < kUiPictureSide || item.height < kUiPictureSide) return;
    ++seen_[int(ai::Kind::Icons)];
    begin(id, item, ai::Kind::Icons);
}

void Ai11::bringBack(uint32_t id, Item& item) {
    Request r;
    if (!pictures_->texture(item.key, item.kind, r.name)) {
        if (item.alike.empty() || !pictures_->alikeFits(item.alike, item.kind)) {
            item.state = Item::Skipped;
            return;
        }
        r.name = item.alike;
    }
    item.state = Item::Waiting;
    r.id = id;
    r.serial = item.serial;
    r.width = item.width;
    r.height = item.height;
    request(std::move(r));
    ++broughtBack_;
}

void Ai11::forget(uint32_t id) {
    if (!ready_) return;
    dropCopy(id);
    items_.erase(id);
    atlases_.erase(id);  // its copy went with dropCopy
    EnterCriticalSection(&lock_);
    if (!in_.empty()) gone_.insert(id);  // its pictures still asked for are not read
    LeaveCriticalSection(&lock_);
}

void Ai11::dropCopy(uint32_t id) {
    const auto c = copies_.find(id);
    if (c == copies_.end()) return;
    aiBytes_ -= std::min(aiBytes_, c->second.bytes);
    release(c->second.srv);  // Direct3D 11 keeps it while queued draws still read it
    release(c->second.texture);
    copies_.erase(c);
}

ID3D11ShaderResourceView* Ai11::copyOf(uint32_t id, bool ui) const {
    if (!shown || !id) return nullptr;
    const auto c = copies_.find(id);
    if (c == copies_.end()) return nullptr;
    return (ui ? c->second.ui : c->second.world) ? c->second.srv : nullptr;
}

// 0.44: over the budget, the copies of the textures not drawn for longest (two seconds and more) go.
void Ai11::putAwayOld() {
    if (!vramBudget_ || aiBytes_ <= vramBudget_ || frame_ - lastPutAway_ < 30) return;
    lastPutAway_ = frame_;
    std::vector<ai::Copy> list;
    for (const auto& e : items_) {
        if (e.second.state != Item::Done) continue;
        const auto c = copies_.find(e.first);
        if (c != copies_.end() && c->second.bytes) list.push_back({e.first, c->second.bytes, e.second.lastUse});
    }
    for (uint32_t id : ai::putAway(list, aiBytes_, vramBudget_, frame_, 120)) {
        const auto it = items_.find(id);
        if (it == items_.end()) continue;
        dropCopy(id);
        it->second.state = Item::Away;
        ++putAway_;
    }
}

// The game's sprite cache (0.13): its enlarged copy takes each new frame at once as the game has it, every texel
// scale x scale, and the frame's picture when the pack has it and the cell still holds that frame.
bool Ai11::cellOn(uint8_t source) const {
    switch (sprite_exe::Source(source)) {
        case sprite_exe::Source::Monster: return settings_.monsters;
        case sprite_exe::Source::Player: return settings_.players;
        case sprite_exe::Source::Npc: return settings_.npcs;
        case sprite_exe::Source::Item: return settings_.items;
        case sprite_exe::Source::Effect: return settings_.effects;
        case sprite_exe::Source::Interface: return settings_.icons;  // the cursor
        case sprite_exe::Source::Unknown: break;
    }
    return settings_.monsters;  // a file the loader's hook did not see: as most sprites are
}

void Ai11::noteCells(uint32_t id, int width, int height, const UploadCmd& u, const uint32_t* argb,
                     const SpriteCell* cells, uint32_t count) {
    const auto item = items_.find(id);
    if (item != items_.end()) {  // taken for a whole texture before it was seen to be a sprite cache
        if (!atlases_.count(id)) dropCopy(id);
        items_.erase(item);
    }
    if (!pictures_->anyCells()) return;
    auto a = atlases_.find(id);
    const bool made = a == atlases_.end();
    if (made) {
        if (!makeAtlas(id, width, height)) return;
        a = atlases_.find(id);
    }
    if (!count) {  // a change of no frame: as it is
        standIn(id, a->second, u.rect, argb, u.rect.width());
        return;
    }
    if (made) {  // what it held already, as it is: where it has anything (the copy starts cleared)
        Rect box{u.rect.right, u.rect.bottom, u.rect.left, u.rect.top};
        for (int y = u.rect.top; y < u.rect.bottom; ++y)
            for (int x = u.rect.left; x < u.rect.right; ++x)
                if (argb[size_t(y - u.rect.top) * size_t(u.rect.width()) + size_t(x - u.rect.left)]) {
                    box.left = std::min(box.left, x);
                    box.top = std::min(box.top, y);
                    box.right = std::max(box.right, x + 1);
                    box.bottom = std::max(box.bottom, y + 1);
                }
        // A texel more on each side: the enlargement blends with its neighbours.
        box = Rect{std::max(box.left - 1, u.rect.left), std::max(box.top - 1, u.rect.top),
                   std::min(box.right + 1, u.rect.right), std::min(box.bottom + 1, u.rect.bottom)};
        if (!box.empty())
            standIn(id, a->second, box,
                    argb + size_t(box.top - u.rect.top) * size_t(u.rect.width()) + size_t(box.left - u.rect.left),
                    u.rect.width());
    }
    for (uint32_t i = 0; i < count; ++i) {
        const Rect c = cells[i].rect;
        if (c.empty() || c.left < u.rect.left || c.top < u.rect.top || c.right > u.rect.right ||
            c.bottom > u.rect.bottom)
            continue;
        const int w = c.width(), h = c.height();
        std::vector<uint32_t> pixels(size_t(w) * size_t(h));
        for (int y = 0; y < h; ++y)
            std::memcpy(pixels.data() + size_t(y) * size_t(w),
                        argb + size_t(c.top - u.rect.top + y) * size_t(u.rect.width()) + size_t(c.left - u.rect.left),
                        size_t(w) * 4);
        if (!made) standIn(id, a->second, c, pixels.data(), w);  // the frame as the game has it, at once
        ++cellsWritten_;
        // Every new frame counts: a picture asked for the one before it is not taken any more.
        const uint32_t frame = ++a->second.frames[uint32_t(c.top) << 16 | uint32_t(c.left)];
        if (!cellOn(cells[i].source)) continue;  // a kind that is off: it stays as the game has it
        if (ai::softPicture(pixels.data(), w, h) || ai::softAlpha(pixels.data(), w, h)) {
            ++soft_;
            continue;
        }
        Request r;
        if (!pictures_->cell(ai::keyName(ai::textureKey(pixels.data(), w, h)), r.name)) {
            ++cellsMissing_;
            continue;
        }
        r.want = Want::Cell;
        r.id = id;
        r.serial = frame;
        r.width = w;
        r.height = h;
        r.cellX = c.left;
        r.cellY = c.top;
        request(std::move(r));
    }
}

bool Ai11::makeAtlas(uint32_t id, int width, int height) {
    const int w = width * scale_, h = height * scale_;
    D3D11_TEXTURE2D_DESC d{};
    d.Width = UINT(w);
    d.Height = UINT(h);
    d.MipLevels = UINT(atlasLevels_);
    d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    // Cleared: Direct3D 11 leaves a new texture's texels undefined, and a cell the game has not written shows nothing.
    const std::vector<uint32_t> zeros(size_t(w) * size_t(h));
    std::vector<D3D11_SUBRESOURCE_DATA> init(static_cast<size_t>(atlasLevels_));
    for (int i = 0; i < atlasLevels_; ++i) init[size_t(i)] = {zeros.data(), UINT(std::max(1, w >> i)) * 4, 0};
    Copy c;
    if (FAILED(device_->CreateTexture2D(&d, init.data(), &c.texture)) ||
        FAILED(device_->CreateShaderResourceView(c.texture, nullptr, &c.srv))) {
        release(c.srv);
        release(c.texture);
        return false;
    }
    dropCopy(id);  // a picture of it as a whole, made before it was seen to be a sprite cache
    c.world = c.ui = true;  // the cursor and the interface's other sprites come from the same cache
    copies_[id] = c;
    Atlas a;
    a.width = width;
    a.height = height;
    atlases_[id] = std::move(a);
    return true;
}

void Ai11::standIn(uint32_t id, const Atlas& a, const Rect& r, const uint32_t* argb, int pitch) {
    const int w = r.width(), h = r.height();
    if (w <= 0 || h <= 0) return;
    std::vector<uint32_t> src(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        std::memcpy(src.data() + size_t(y) * size_t(w), argb + size_t(y) * size_t(pitch), size_t(w) * 4);
    std::vector<uint32_t> level, next;
    ai::enlargeNearest(src.data(), w, h, scale_, level);  // the game's own look until the frame's picture comes
    int lw = w * scale_, lh = h * scale_;
    for (int i = 0; i < atlasLevels_; ++i) {
        writeLevel(id, a, i, (r.left * scale_) >> i, (r.top * scale_) >> i, lw, lh, level.data());
        if (i + 1 == atlasLevels_) break;
        int nw = 0, nh = 0;
        ai::halve(level, lw, lh, next, nw, nh);
        level.swap(next);
        lw = nw;
        lh = nh;
    }
}

void Ai11::writeLevel(uint32_t id, const Atlas& a, int level, int x, int y, int w, int h, const uint32_t* argb) {
    const auto c = copies_.find(id);
    if (c == copies_.end() || !c->second.texture || x < 0 || y < 0) return;
    const int dw = std::max(1, (a.width * scale_) >> level), dh = std::max(1, (a.height * scale_) >> level);
    const int cw = std::min(w, dw - x), ch = std::min(h, dh - y);
    if (cw <= 0 || ch <= 0) return;
    const D3D11_BOX box{UINT(x), UINT(y), 0, UINT(x + cw), UINT(y + ch), 1};
    ctx_->UpdateSubresource(c->second.texture, UINT(level), &box, argb, UINT(w) * 4, 0);
}

bool Ai11::cellCurrent(const Request& r) const {
    const auto a = atlases_.find(r.id);
    if (a == atlases_.end() || !copies_.count(r.id)) return false;
    const auto f = a->second.frames.find(uint32_t(r.cellY) << 16 | uint32_t(r.cellX));
    return f != a->second.frames.end() && f->second == r.serial;
}

void Ai11::noteIcon(uint64_t key, int width, int height, const uint32_t* argb) {
    if (!ready_ || !pictures_->kindOn(ai::Kind::Icons) || width <= 0 || height <= 0 || !argb ||
        !icons_.insert(key).second)
        return;
    Request r;
    if (!pictures_->icon(ai::keyName(ai::textureKey(argb, width, height)), r.name)) {
        noPicture_.push_back(key);  // told at the next frame: not from inside the window's drawing that asks
        return;
    }
    r.want = Want::Icon;
    r.width = width;
    r.height = height;
    r.iconKey = key;
    request(std::move(r));
}

void Ai11::work() {
    if (!ready_) return;
    ++frame_;
    putAwayOld();
    std::deque<Result> results;
    EnterCriticalSection(&lock_);
    results.swap(out_);
    waiting_ = static_cast<uint32_t>(in_.size());
    LeaveCriticalSection(&lock_);
    for (Result& r : results) {
        if (r.request.want == Want::Cell && r.ok)
            cellsWaiting_.push_back(std::move(r));
        else
            take(r);
    }
    // The sprite frames' pictures go into their caches through the game's thread: a crowd's many a few frames apart.
    double budget = kCellTexels;
    while (!cellsWaiting_.empty() && budget > 0) {
        Result& r = cellsWaiting_.front();
        for (const Level& l : r.levels) budget -= double(l.argb.size());
        take(r);
        cellsWaiting_.pop_front();
    }
    // The icons there is no picture of: crisp text makes them sharp by xBR instead (window_art = xbr).
    for (uint64_t key : noPicture_) {
        if (iconSink_) iconSink_(iconContext_, key, 0, 0, nullptr);
        ++iconsNone_;
    }
    noPicture_.clear();
    logNow(false);
}

void Ai11::take(Result& r) {
    const Request& q = r.request;
    if (q.want == Want::Icon) {
        if (r.ok && !r.levels.empty() && iconSink_) {
            const Level& l = r.levels[0];
            iconSink_(iconContext_, q.iconKey, l.width, l.height, l.argb.data());
            ++iconsTaken_;
        } else if (!r.ok) {
            ++failed_;
            noPicture_.push_back(q.iconKey);
        }
        return;
    }
    if (q.want == Want::Cell) {
        if (!r.ok) {
            ++failed_;
            return;
        }
        if (!cellCurrent(q)) return;  // the cell has another frame by now
        const Atlas& a = atlases_.find(q.id)->second;
        for (size_t i = 0; i < r.levels.size(); ++i)
            writeLevel(q.id, a, int(i), (q.cellX * scale_) >> i, (q.cellY * scale_) >> i, r.levels[i].width,
                       r.levels[i].height, r.levels[i].argb.data());
        ++cellsTaken_;
        return;
    }
    const auto it = items_.find(q.id);
    const bool current = it != items_.end() && it->second.state == Item::Waiting && it->second.serial == q.serial;
    if (!current || !r.ok) {
        release(r.srv);
        release(r.texture);
        if (current) {  // not usable: the game's
            it->second.state = Item::Skipped;
            ++failed_;
        }
        return;
    }
    dropCopy(q.id);
    Copy c;
    c.texture = r.texture;
    c.srv = r.srv;
    c.world = !it->second.ui;
    c.ui = it->second.ui;
    c.bytes = r.bytes;
    r.texture = nullptr;
    r.srv = nullptr;
    copies_[q.id] = c;
    aiBytes_ += c.bytes;
    it->second.state = Item::Done;
    it->second.lastUse = frame_;
    ++texturesTaken_;
}

void Ai11::request(Request r) {
    EnterCriticalSection(&lock_);
    in_.push_back(std::move(r));
    LeaveCriticalSection(&lock_);
    SetEvent(wake_);
}

DWORD WINAPI Ai11::ioMain(void* self) {
    static_cast<Ai11*>(self)->ioLoop();
    return 0;
}

void Ai11::ioLoop() {
    for (;;) {
        WaitForSingleObject(wake_, INFINITE);
        for (;;) {
            Request r;
            EnterCriticalSection(&lock_);
            if (stop_) {
                LeaveCriticalSection(&lock_);
                return;
            }
            if (in_.empty()) {
                gone_.clear();  // nothing asked for them any more
                LeaveCriticalSection(&lock_);
                break;
            }
            r = std::move(in_.front());
            in_.pop_front();
            const bool gone = r.want != Want::Icon && gone_.count(r.id) != 0;
            LeaveCriticalSection(&lock_);
            if (gone) continue;
            Result res;
            res.request = r;
            try {
                serve(r, res);
            } catch (...) {  // no memory for it (the game's address space): the game's own texture
                release(res.srv);
                release(res.texture);
                res.levels.clear();
                res.ok = false;
            }
            EnterCriticalSection(&lock_);
            out_.push_back(std::move(res));
            LeaveCriticalSection(&lock_);
        }
    }
}

bool Ai11::load(const Request& r, std::vector<uint32_t>& argb, int& width, int& height) {
    std::vector<uint8_t> blob;
    if (!pack_.read(r.name, blob) || !webp::decode(blob.data(), blob.size(), argb, width, height)) return false;
    std::vector<uint8_t>().swap(blob);
    // 4 times the game's texture - twice at [ai] scale = 2; (0.45) a whole texture's picture for one at a lower
    // texture detail, 8 or 16 times its size.
    while (width > r.width * scale_ && height > r.height * scale_ && width % 2 == 0 && height % 2 == 0)
        ai::halve(argb, width, height);
    return width == r.width * scale_ && height == r.height * scale_;
}

void Ai11::serve(const Request& r, Result& out) {
    std::vector<uint32_t> argb;
    int w = 0, h = 0;
    if (!load(r, argb, w, h)) return;
    if (r.want == Want::Icon) {
        out.levels.push_back(Level{w, h, std::move(argb)});
        out.ok = true;
        return;
    }
    if (r.want == Want::Cell) {  // the sprite cache copy's levels, put in by the game's thread
        out.levels.push_back(Level{w, h, std::move(argb)});
        while (int(out.levels.size()) < atlasLevels_) {
            Level next;
            const Level& last = out.levels.back();
            ai::halve(last.argb, last.width, last.height, next.argb, next.width, next.height);
            out.levels.push_back(std::move(next));
        }
        out.ok = true;
        return;
    }
    // A texture's picture, at most kLargestSide a side, with all its levels: its texture is made here.
    while ((w > kLargestSide || h > kLargestSide) && w % 2 == 0 && h % 2 == 0) ai::halve(argb, w, h);
    std::vector<Level> levels;
    levels.push_back(Level{w, h, std::move(argb)});
    while (levels.back().width > 1 || levels.back().height > 1) {
        Level next;
        const Level& last = levels.back();
        ai::halve(last.argb, last.width, last.height, next.argb, next.width, next.height);
        levels.push_back(std::move(next));
    }
    std::vector<D3D11_SUBRESOURCE_DATA> init(levels.size());
    uint64_t bytes = 0;
    for (size_t i = 0; i < levels.size(); ++i) {
        init[i] = {levels[i].argb.data(), UINT(levels[i].width) * 4, 0};
        bytes += uint64_t(levels[i].argb.size()) * 4;
    }
    D3D11_TEXTURE2D_DESC d{};
    d.Width = UINT(levels[0].width);
    d.Height = UINT(levels[0].height);
    d.MipLevels = UINT(levels.size());
    d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_IMMUTABLE;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&d, init.data(), &out.texture)) ||
        FAILED(device_->CreateShaderResourceView(out.texture, nullptr, &out.srv))) {
        release(out.srv);
        release(out.texture);
        return;
    }
    out.bytes = bytes;
    out.ok = true;
}

void Ai11::logNow(bool final) {
    const ULONGLONG now = GetTickCount64();
    if (!final && now - lastLog_ < 30000) return;
    lastLog_ = now;
    uint32_t seen = 0;
    for (uint32_t n : seen_) seen += n;
    if (!seen && !texturesTaken_ && !cellsWritten_ && icons_.empty() && !failed_) return;
    std::string kinds, missing;
    for (int k = 0; k < ai::kKinds; ++k) {
        kinds += std::string(k ? ", " : "") + ai::kindName(ai::Kind(k)) + " " + std::to_string(seen_[k]) +
                 (!pictures_->kindOn(ai::Kind(k)) ? " (off)" : "");
        missing += std::string(k ? ", " : "") + ai::kindName(ai::Kind(k)) + " " + std::to_string(missing_[k]);
    }
    LOG_INFO("AI textures: %u from the pack, %u waiting, %u not usable; seen %s, lightmaps %u; %u of soft light left "
             "as the game's",
             texturesTaken_, waiting_, failed_, kinds.c_str(), seen_[int(ai::Kind::Lightmap)], soft_);
    LOG_INFO("AI textures: not in the pack - %s; sprite frames %u, icons %u", missing.c_str(), cellsMissing_,
             iconsNone_);
    if (cellsWritten_)
        LOG_INFO("AI textures: sprite frames - %u written by the game into %u caches, %u from the pack", cellsWritten_,
                 static_cast<unsigned>(atlases_.size()), cellsTaken_);
    if (!icons_.empty())
        LOG_INFO("AI textures: icons - %u asked, %u from the pack, %u of none (xBR's)",
                 static_cast<unsigned>(icons_.size()), iconsTaken_, iconsNone_);
    uint64_t used = 0;
    const uint64_t system = videoMemory(&used);
    unsigned copies = 0;
    for (const auto& c : copies_) copies += c.second.bytes ? 1 : 0;
    LOG_INFO("AI textures: video memory - the copies %llu MB of their %llu MB (%u textures), %u put away, %u brought "
             "back; the game %llu MB of the %llu MB the system gives it",
             static_cast<unsigned long long>(aiBytes_ >> 20), static_cast<unsigned long long>(vramBudget_ >> 20),
             copies, putAway_, broughtBack_,
             static_cast<unsigned long long>(used >> 20), static_cast<unsigned long long>(system >> 20));
}

}  // namespace uagfx::gpu

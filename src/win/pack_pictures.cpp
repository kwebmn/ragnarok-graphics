#include "win/pack_pictures.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>
#include <utility>

#include "core/layout.hpp"
#include "core/pack_source.hpp"
#include "core/texture_pack.hpp"
#include "core/webp_image.hpp"
#include "core/zip.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::packpictures {
namespace {

// 3.0.11-beta.76: a sprite frame's picture in the pack - "sprites\<w>x<h>\<16 hex>.webp", stored
struct SpriteAt {
    uint64_t hash = 0;
    uint32_t side = 0;  // w << 16 | h
    uint32_t bytes = 0;
    uint64_t local = 0;
    bool operator<(const SpriteAt& o) const { return side != o.side ? side < o.side : hash < o.hash; }
};
using Sprites = std::vector<SpriteAt>;

struct Pack {
    CRITICAL_SECTION lock;  // as the driver's other threads (no std::thread: no winpthread beside the game)
    Pack() { InitializeCriticalSection(&lock); }
    bool started = false, done = false;
    std::shared_ptr<const zip::Index> index;  // read once, then only read
    std::shared_ptr<const Sprites> sprites;
    int stamp = 0;
};

// "64x64\0123456789abcdef" (a key) or "sprites\64x64\0123456789abcdef.webp" (a name): its side and hash
bool spriteKey(const std::string& text, size_t at, SpriteAt& out) {
    unsigned w = 0, h = 0;
    int used = 0;
    if (std::sscanf(text.c_str() + at, "%ux%u%n", &w, &h, &used) != 2 || w > 0xFFFF || h > 0xFFFF) return false;
    at += size_t(used);
    if (at >= text.size() || text[at] != '\\' || text.size() < at + 17) return false;
    uint64_t hash = 0;
    for (size_t i = at + 1; i < at + 17; ++i) {
        const char c = text[i];
        const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (d < 0) return false;
        hash = hash << 4 | uint64_t(d);
    }
    out.hash = hash;
    out.side = w << 16 | h;
    return true;
}

Pack& pack() {
    static Pack p;
    return p;
}

std::atomic<bool> g_icons{true};

// The directory thread's sprite table until it is published with the index
std::shared_ptr<const Sprites>& madeSprites() {
    static std::shared_ptr<const Sprites> made;
    return made;
}

DWORD WINAPI readDirectory(void*) {
    const std::string path = ourFile(gameDir(), layout::File::Pack);
    std::shared_ptr<const zip::Index> read;
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        LOG_INFO("pack pictures: no AI texture pack (%s) - the game's own pictures", path.c_str());
    } else {
        const std::string ui = std::string(pack::groupName(pack::Group::Ui)) + "\\";
        const std::string world = std::string(pack::groupName(pack::Group::World)) + "\\";
        const std::string sprites = std::string(pack::groupName(pack::Group::Sprites)) + "\\";
        auto index = std::make_shared<zip::Index>();
        auto table = std::make_shared<Sprites>();
        std::string why;
        const ULONGLONG t0 = GetTickCount64();
        if (index->open(
                path,
                [&](const std::string& n) {
                    return n.compare(0, ui.size(), ui) == 0 || n.compare(0, world.size(), world) == 0;
                },
                why,
                [&](const std::string& n, const zip::Place& p) {  // 3.0.11-beta.76: the sprites' frames, compact
                    SpriteAt at;
                    if (p.method != 0 || p.compressed != p.size || p.compressed > 0xFFFFFFFFu ||
                        n.compare(0, sprites.size(), sprites) != 0 || !spriteKey(n, sprites.size(), at))
                        return;
                    at.bytes = uint32_t(p.compressed);
                    at.local = p.local;
                    table->push_back(at);
                })) {
            std::sort(table->begin(), table->end());
            table->shrink_to_fit();
            LOG_INFO("pack pictures: the AI texture pack's %zu interface and world pictures and %zu sprite frames "
                     "known in %llu ms",
                     index->count(), table->size(), static_cast<unsigned long long>(GetTickCount64() - t0));
            read = std::move(index);
            madeSprites() = std::move(table);
        } else {
            LOG_INFO("pack pictures: the AI texture pack cannot be read (%s)", why.c_str());
        }
    }
    Pack& p = pack();
    EnterCriticalSection(&p.lock);
    p.index = std::move(read);
    p.sprites = std::move(madeSprites());
    p.done = true;
    p.stamp = p.index ? 1 : 0;
    LeaveCriticalSection(&p.lock);
    return 0;
}

// The directory once read (null: not yet, or no pack); the first ask starts its thread
std::shared_ptr<const zip::Index> directory() {
    Pack& p = pack();
    EnterCriticalSection(&p.lock);
    if (!p.started) {
        p.started = true;
        const HANDLE t = CreateThread(nullptr, 0, &readDirectory, nullptr, 0, nullptr);
        if (t) {
            CloseHandle(t);
        } else {
            p.done = true;
            LOG_INFO("pack pictures: no thread to read the pack (%lu)", GetLastError());
        }
    }
    std::shared_ptr<const zip::Index> out = p.index;
    LeaveCriticalSection(&p.lock);
    return out;
}

bool readPicture(const zip::Index& index, pack::Group group, const std::string& key, std::vector<uint32_t>& argb,
                 int& width, int& height, std::string* name) {
    const std::string entry = zip::normalName(pack::pictureName(group, key));
    std::vector<uint8_t> file;
    std::string why;
    if (!index.has(entry) || !index.read(entry, file, why)) return false;
    if (!webp::decode(file.data(), file.size(), argb, width, height) || width <= 0 || height <= 0) {
        LOG_INFO("pack pictures: %s is not a picture the driver reads", entry.c_str());
        return false;
    }
    if (name) *name = entry;
    return true;
}

}  // namespace

int stamp() {
    directory();
    Pack& p = pack();
    EnterCriticalSection(&p.lock);
    const int s = p.stamp;
    LeaveCriticalSection(&p.lock);
    return s;
}

void setIconsOn(bool on) { g_icons = on; }
bool iconsOn() { return g_icons; }
int iconsStamp() { return g_icons ? stamp() : -1; }

bool ofInterface(const uint8_t* bmp, size_t size, std::vector<uint32_t>& argb, int& width, int& height,
                 std::string* name) {
    const std::shared_ptr<const zip::Index> index = directory();
    if (!index || !bmp || !size) return false;
    pack::Picture p;
    if (pack::interfacePicture(bmp, size, p) && readPicture(*index, pack::Group::Ui, p.key, argb, width, height, name))
        return true;
    // a big one the game draws as a texture (a minimap): its 16-bit pixels' key too, as the builder makes both
    return pack::worldTexture(bmp, size, false, p) &&
           readPicture(*index, pack::Group::Ui, p.key, argb, width, height, name);
}

bool ofTexture(const uint8_t* data, size_t size, bool tga, std::vector<uint32_t>& argb, int& width, int& height,
               std::string* name) {
    const std::shared_ptr<const zip::Index> index = directory();
    pack::Picture p;
    return index && data && size && pack::worldTexture(data, size, tga, p) &&
           readPicture(*index, pack::Group::World, p.key, argb, width, height, name);
}

bool ofSprite(const std::string& key, std::vector<uint32_t>& argb, int& width, int& height) {
    const std::shared_ptr<const zip::Index> index = directory();
    Pack& p = pack();
    EnterCriticalSection(&p.lock);
    const std::shared_ptr<const Sprites> table = p.sprites;
    LeaveCriticalSection(&p.lock);
    SpriteAt want;
    if (!index || !table || !spriteKey(key, 0, want)) return false;
    const auto it = std::lower_bound(table->begin(), table->end(), want);
    if (it == table->end() || it->side != want.side || it->hash != want.hash) return false;
    std::vector<uint8_t> file;
    std::string why;
    if (!index->read(zip::Place{it->local, it->bytes, it->bytes, 0}, file, why) ||
        !webp::decode(file.data(), file.size(), argb, width, height) || width <= 0 || height <= 0)
        return false;
    return true;
}

bool waitRead(unsigned ms) {
    for (unsigned waited = 0;; waited += 20) {
        directory();
        Pack& p = pack();
        EnterCriticalSection(&p.lock);
        const bool done = p.done, there = p.index != nullptr;
        LeaveCriticalSection(&p.lock);
        if (done) return there;
        if (waited >= ms) return false;
        Sleep(20);
    }
}

}  // namespace uagfx::win::packpictures

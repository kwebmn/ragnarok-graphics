#pragma once
#include <windows.h>

#include <atomic>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/ai/picture_metrics.hpp"
#include "core/texture_catalog.hpp"

namespace uagfx::studio {

class CatalogWork;

// 2.3 (T2, T3): the list's thumbnails - the original and the pack's picture side by side - and each picture's quality
// numbers, made on a thread of their own, the rows in view first; kThumbReady when some are done.
struct Thumb {
    std::vector<uint32_t> pair;  // kSide * 2 x kSide: the original, then the pack's picture
    bool packed = false;         // there is a picture in the pack
    ai::Metrics metrics;
    uint16_t flags = 0;
};

class Thumbs {
public:
    static constexpr int kSide = 32;
    Thumbs(HWND notify, CatalogWork& files);
    ~Thumbs();
    // The thumb of a row (null: not made yet - asked for, the newest asks first).
    std::shared_ptr<const Thumb> get(size_t file, size_t picture);
    void forget();  // all of them (the catalog or the pack changed)
    // A picture's flags known so far (for the list's filter), by key.
    uint16_t flagsOf(const std::string& key) const;

private:
    struct Ask {
        size_t file = 0, picture = 0;
    };
    static DWORD WINAPI threadMain(void* self);
    void run();
    static uint64_t keyOf(size_t file, size_t picture) { return uint64_t(file) << 20 | picture; }

    HWND notify_;
    CatalogWork& files_;
    mutable SRWLOCK lock_ = SRWLOCK_INIT;
    CONDITION_VARIABLE more_ = CONDITION_VARIABLE_INIT;
    std::vector<Ask> asks_;  // a stack: the last asked first
    std::unordered_map<uint64_t, std::shared_ptr<const Thumb>> done_;
    std::unordered_map<std::string, uint16_t> flags_;
    std::list<uint64_t> order_;  // the oldest made first: past kKeep they go
    uint64_t generation_ = 0;
    std::atomic<bool> quit_{false};
    HANDLE thread_ = nullptr;
};

}  // namespace uagfx::studio

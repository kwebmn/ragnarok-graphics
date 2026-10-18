#pragma once
#include <windows.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "core/pack_build.hpp"
#include "core/texture_catalog.hpp"

namespace uagfx::studio {

// 2.3 (T1, N2): the texture catalog in the studio - read from studio\catalog.bin and brought up to date on threads of
// its own (only the files of archives that changed are decoded again), with the pack as it is now; and the game's
// files read on demand (thumbnails, the inspector, making a picture) - their archives open only while they are used
// (closed after 30 idle seconds, and at Pause), so that the game's patcher can write them.
class CatalogWork {
public:
    explicit CatalogWork(std::wstring gameDir) : gameDir_(std::move(gameDir)) {}
    ~CatalogWork();
    CatalogWork(const CatalogWork&) = delete;
    CatalogWork& operator=(const CatalogWork&) = delete;

    void update(HWND notify);  // kCatalogReady when done (or stopped)
    void stop();               // the update ends; the archives close
    bool busy() const { return thread_ != nullptr && !finished_; }
    std::shared_ptr<const catalog::Catalog> catalog() const;
    std::shared_ptr<const catalog::PackView> packView() const;
    void readPack();  // the pack's names, times and index again (after a write)
    // Where the update is: its stage (cache, archives, textures, maps, pack) and its count.
    std::string stage() const;
    void count(uint64_t& done, uint64_t& total) const;
    pack::Choice choice() const;  // every kind, the server's colours, the [ai] sides
    std::wstring packPath() const { return gameDir_ + L"\\uaro-textures.zip"; }

    // A file of the game (its normal name); thread-safe, one read at a time.
    bool read(const std::string& name, std::vector<uint8_t>& out);
    void closeIdle();  // the archives closed after 30 s unused (the window's timer)

private:
    static DWORD WINAPI threadMain(void* self);
    void run();
    void setStage(const char* s);

    std::wstring gameDir_;
    HWND notify_ = nullptr;
    HANDLE thread_ = nullptr;
    std::atomic<bool> stop_{false}, finished_{false};
    mutable SRWLOCK lock_ = SRWLOCK_INIT;  // catalog_, pack_, stage_
    std::shared_ptr<const catalog::Catalog> catalog_;
    std::shared_ptr<const catalog::PackView> pack_;
    std::string stage_;
    std::atomic<uint64_t> done_{0}, total_{0};
    SRWLOCK filesLock_ = SRWLOCK_INIT;  // files_
    std::unique_ptr<pack::GameFiles> files_;
    ULONGLONG filesUsed_ = 0;
};

}  // namespace uagfx::studio

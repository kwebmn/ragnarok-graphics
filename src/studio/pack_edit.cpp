#include "studio/pack_edit.hpp"

#include <windows.h>

#include <cstdio>

#include "app/text.hpp"
#include "core/studio_link.hpp"
#include "studio/places.hpp"

namespace uagfx::studio {

namespace {

// The pack's writer for as long as it lives: the builder's mutex (a builder or another change: busy) and the pack
// writer's (a game making pictures: busy).
class PackLock {
public:
    bool take(std::string& error) {
        builder_ = CreateMutexW(nullptr, FALSE, L"Local\\uaro-texture-pack-builder");
        if (!builder_ || GetLastError() == ERROR_ALREADY_EXISTS) {
            error = "the pack is busy";
            return false;
        }
        writer_ = CreateMutexW(nullptr, FALSE, app::wide(pack::kWriterMutex).c_str());
        const DWORD got = writer_ ? WaitForSingleObject(writer_, 0) : WAIT_FAILED;
        held_ = got == WAIT_OBJECT_0 || got == WAIT_ABANDONED;
        if (!held_) error = "the pack is busy";
        return held_;
    }
    ~PackLock() {
        if (held_) ReleaseMutex(writer_);
        if (writer_) CloseHandle(writer_);
        if (builder_) CloseHandle(builder_);
    }

private:
    HANDLE builder_ = nullptr, writer_ = nullptr;
    bool held_ = false;
};

}  // namespace

bool compactPack(const std::string& pack, std::string& error, const std::string& gameOpen) {
    PackLock lock;
    if (!lock.take(error)) return false;
    const std::string fresh = pack + ".new";
    size_t kept = 0;
    if (!pack::rewrite(pack, fresh, [](const std::string&) { return true; }, kept, error)) {
        DeleteFileA(fresh.c_str());
        return false;
    }
    if (!MoveFileExA(fresh.c_str(), pack.c_str(), MOVEFILE_REPLACE_EXISTING)) {  // a game reading it holds it
        DeleteFileA(fresh.c_str());
        error = gameOpen;
        return false;
    }
    return true;
}

bool packEdit(const std::string& pack, const PackChange& change, std::string& error) {
    PackLock lock;
    if (!lock.take(error)) return false;
    pack::Index index;
    {
        pack::Reader r;  // its index, before the writer drops it to go on
        std::string why;
        std::vector<uint8_t> bytes;
        if (r.open(pack, why) && r.read(pack::kIndexName, bytes))
            index = pack::parseIndexFile(std::string(bytes.begin(), bytes.end()));
    }
    pack::Writer w;
    bool ok = w.open(pack, true, error);
    if (ok) {  // whatever the change did, the pack is whole again (its directory, its index)
        std::string why, closing;
        const bool changed = change(w, index, why);
        ok = w.close(pack::indexText(index), closing) && changed;
        error = !changed ? why : closing;
    }
    return ok;
}

void tellGame(const std::vector<std::string>& keys) {
    const std::wstring folder = linkFolder(), path = folder + L"\\changed.txt";
    makeFolder(folder);
    const std::string text = changedText(keys);
    if (std::FILE* f = _wfopen((path + L".part").c_str(), L"wb")) {
        const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
        if (std::fclose(f) == 0 && ok) MoveFileExW((path + L".part").c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    // The game's driver made the event (it looks at it once a frame); no game running: no event, nothing to tell.
    if (HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, app::wide(kChangedEvent).c_str())) {
        SetEvent(e);
        CloseHandle(e);
    }
}

}  // namespace uagfx::studio

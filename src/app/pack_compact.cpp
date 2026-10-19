#include "app/pack_compact.hpp"

#include "app/text.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::app {

bool PackLock::take(std::string& error) {
    builder_ = CreateMutexW(nullptr, FALSE, L"Local\\uaro-texture-pack-builder");
    if (!builder_ || GetLastError() == ERROR_ALREADY_EXISTS) {
        error = "the pack is busy";
        return false;
    }
    writer_ = CreateMutexW(nullptr, FALSE, wide(pack::kWriterMutex).c_str());
    const DWORD got = writer_ ? WaitForSingleObject(writer_, 0) : WAIT_FAILED;
    held_ = got == WAIT_OBJECT_0 || got == WAIT_ABANDONED;
    if (!held_) error = "the pack is busy";
    return held_;
}

PackLock::~PackLock() {
    if (held_) ReleaseMutex(writer_);
    if (writer_) CloseHandle(writer_);
    if (builder_) CloseHandle(builder_);
}

bool compactPack(const std::string& pack, std::string& error, const std::string& gameOpen,
                 const std::function<void(size_t done, size_t total)>& progress) {
    PackLock lock;
    if (!lock.take(error)) return false;
    const std::string fresh = pack + ".new";
    size_t kept = 0;
    if (!pack::rewrite(pack, fresh, [](const std::string&) { return true; }, kept, error, progress)) {
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

namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
CompactState g_state;
struct Job {
    std::wstring pack;
    uint64_t needFree = 0;
    std::string gameOpen;
};

void set(const std::function<void(CompactState&)>& change) {
    AcquireSRWLockExclusive(&g_lock);
    change(g_state);
    ReleaseSRWLockExclusive(&g_lock);
}

uint64_t sizeOfFile(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &d)) return 0;
    return uint64_t(d.nFileSizeHigh) << 32 | d.nFileSizeLow;
}

DWORD WINAPI work(void* p) {
    Job* job = static_cast<Job*>(p);
    std::string error;
    const uint64_t before = sizeOfFile(job->pack);
    ULARGE_INTEGER free{};
    const size_t slash = job->pack.find_last_of(L'\\');
    const std::wstring folder = slash == std::wstring::npos ? L"." : job->pack.substr(0, slash + 1);
    bool ok = true;
    if (GetDiskFreeSpaceExW(folder.c_str(), &free, nullptr, nullptr) && free.QuadPart < job->needFree) {
        error = "the disk has " + std::to_string(free.QuadPart >> 20) + " MB free, the copy needs " +
                std::to_string(job->needFree >> 20) + " MB";
        ok = false;
    }
    if (ok)
        ok = compactPack(ansi(job->pack), error, job->gameOpen, [](size_t done, size_t total) {
            set([&](CompactState& s) {
                s.done = done;
                s.total = total;
            });
        });
    const uint64_t after = sizeOfFile(job->pack);
    set([&](CompactState& s) {
        s.kind = ok ? CompactState::Done : CompactState::Failed;
        s.freed = ok && before > after ? before - after : 0;
        s.error = error;
    });
    delete job;
    return 0;
}

}  // namespace

void startCompact(const std::wstring& pack, uint64_t needFree, const std::string& gameOpen) {
    if (compactRunning()) return;
    set([](CompactState& s) {
        s = CompactState();
        s.kind = CompactState::Working;
    });
    Job* job = new Job{pack, needFree, gameOpen};
    if (HANDLE t = CreateThread(nullptr, 0, work, job, 0, nullptr)) {
        CloseHandle(t);
        return;
    }
    delete job;
    set([](CompactState& s) {
        s.kind = CompactState::Failed;
        s.error = "no thread for it";
    });
}

CompactState compactState() {
    AcquireSRWLockShared(&g_lock);
    const CompactState s = g_state;
    ReleaseSRWLockShared(&g_lock);
    return s;
}

bool compactRunning() { return compactState().kind == CompactState::Working; }

}  // namespace uagfx::app

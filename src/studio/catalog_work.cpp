#include "studio/catalog_work.hpp"

#include <algorithm>
#include <cstdio>

#include "app/text.hpp"
#include "core/config.hpp"
#include "studio/messages.hpp"
#include "studio/places.hpp"
#include "win/paths.hpp"

namespace uagfx::studio {
namespace {

bool readWhole(const std::wstring& path, std::string& out) {
    std::FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    out.clear();
    char buf[1 << 16];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

bool writeWhole(const std::wstring& path, const std::string& bytes) {
    const std::wstring part = path + L".part";
    std::FILE* f = _wfopen(part.c_str(), L"wb");
    if (!f) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (std::fclose(f) != 0 || !ok) return false;
    return MoveFileExW(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

// A decoding thread's share of the files: every step-th from first.
struct Share {
    CatalogWork* work = nullptr;
    catalog::Catalog* c = nullptr;
    const std::vector<size_t>* todo = nullptr;
    const std::vector<std::string>* names = nullptr;
    const pack::GameFiles* files = nullptr;
    pack::Choice choice;
    size_t first = 0, step = 1;
    std::atomic<bool>* stop = nullptr;
    std::atomic<uint64_t>* done = nullptr;
};

DWORD WINAPI decodeMain(void* arg) {
    Share& s = *static_cast<Share*>(arg);
    pack::GameFiles files;  // its own handles, the tables shared
    std::string error;
    if (!files.openLike(*s.files, error)) return 1;
    const catalog::Read read = [&files](const std::string& n, std::vector<uint8_t>& out) { return files.read(n, out); };
    for (size_t i = s.first; i < s.todo->size() && !*s.stop; i += s.step) {
        catalog::decode(s.c->files[(*s.todo)[i]], *s.names, read, s.choice, s.stop);
        ++*s.done;
    }
    return 0;
}

}  // namespace

CatalogWork::~CatalogWork() { stop(); }

std::wstring CatalogWork::packPath() const {
    return app::wide(win::ourFile(app::ansi(gameDir_), layout::File::Pack));
}

pack::Choice CatalogWork::choice() const {
    pack::Choice c;  // every kind; the server's colours (hair 0-20, clothes 0-77)
    std::string ini;
    if (win::readTextFile(win::ourFile(app::ansi(gameDir_), layout::File::Ini), ini)) {
        const Config config = parseConfig(ini);
        c.minSide = std::min(config.ai.minSize, config.ai.maxSize);
        c.maxSide = std::max(config.ai.minSize, config.ai.maxSize);
    }
    return c;
}

void CatalogWork::update(HWND notify) {
    if (busy()) return;
    if (thread_) {
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
    }
    notify_ = notify;
    stop_ = false;
    finished_ = false;
    thread_ = CreateThread(nullptr, 0, &CatalogWork::threadMain, this, 0, nullptr);
}

void CatalogWork::stop() {
    stop_ = true;
    if (thread_) {
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    AcquireSRWLockExclusive(&filesLock_);
    files_.reset();
    ReleaseSRWLockExclusive(&filesLock_);
}

std::shared_ptr<const catalog::Catalog> CatalogWork::catalog() const {
    AcquireSRWLockShared(&lock_);
    auto c = catalog_;
    ReleaseSRWLockShared(&lock_);
    return c;
}

std::shared_ptr<const catalog::PackView> CatalogWork::packView() const {
    AcquireSRWLockShared(&lock_);
    auto p = pack_;
    ReleaseSRWLockShared(&lock_);
    return p;
}

std::string CatalogWork::stage() const {
    AcquireSRWLockShared(&lock_);
    std::string s = stage_;
    ReleaseSRWLockShared(&lock_);
    return s;
}

void CatalogWork::count(uint64_t& done, uint64_t& total) const {
    done = done_;
    total = total_;
}

void CatalogWork::setStage(const char* s) {
    AcquireSRWLockExclusive(&lock_);
    stage_ = s;
    ReleaseSRWLockExclusive(&lock_);
    done_ = total_ = 0;
    if (notify_) PostMessageW(notify_, kJobsChanged, 0, 0);
}

void CatalogWork::readPack() {
    auto view = std::make_shared<catalog::PackView>();
    pack::Reader r;
    std::string error;
    if (r.open(app::ansi(packPath()), error, false)) {  // a pack being written: none until its checkpoint
        for (const std::string& n : r.names()) view->pictures[n] = r.timeOf(n);
        std::vector<uint8_t> index;
        if (r.read(pack::kIndexName, index))
            view->index = pack::parseIndexFile(std::string(index.begin(), index.end()));
    }
    AcquireSRWLockExclusive(&lock_);
    pack_ = std::move(view);
    ReleaseSRWLockExclusive(&lock_);
}

bool CatalogWork::read(const std::string& name, std::vector<uint8_t>& out) {
    AcquireSRWLockExclusive(&filesLock_);
    if (!files_) {
        auto f = std::make_unique<pack::GameFiles>();
        std::string error;
        if (f->open(app::ansi(gameDir_), error)) files_ = std::move(f);
    }
    const bool ok = files_ && files_->read(name, out);
    filesUsed_ = GetTickCount64();
    ReleaseSRWLockExclusive(&filesLock_);
    return ok;
}

void CatalogWork::closeIdle() {
    AcquireSRWLockExclusive(&filesLock_);
    if (files_ && GetTickCount64() - filesUsed_ > 30000) files_.reset();
    ReleaseSRWLockExclusive(&filesLock_);
}

DWORD WINAPI CatalogWork::threadMain(void* self) {
    static_cast<CatalogWork*>(self)->run();
    return 0;
}

void CatalogWork::run() {
    const std::wstring cachePath = studioRoot() + L"\\catalog.bin";
    const pack::Choice want = choice();
    // The cache first: shown at once, the update after it.
    setStage("cache");
    auto old = std::make_shared<catalog::Catalog>();
    std::string bytes;
    if (!catalog() && readWhole(cachePath, bytes) && catalog::load(bytes, *old)) {
        AcquireSRWLockExclusive(&lock_);
        catalog_ = old;
        ReleaseSRWLockExclusive(&lock_);
        readPack();
        PostMessageW(notify_, kCatalogReady, 0, 0);
    } else if (auto shown = catalog()) {
        *old = *shown;
    } else {
        old.reset();
    }
    setStage("archives");
    pack::GameFiles files;
    std::string error;
    if (stop_ || !files.open(app::ansi(gameDir_), error)) {
        setStage("");
        finished_ = true;
        PostMessageW(notify_, kCatalogReady, 0, 0);
        return;
    }
    std::vector<catalog::ArchiveStamp> stamps;
    for (const std::string& p : files.paths()) {
        catalog::ArchiveStamp s;
        s.path = p;
        win::fileStamp(p.find(':') != std::string::npos ? p : app::ansi(gameDir_) + "\\" + p, s.size, s.time);
        stamps.push_back(s);
    }
    const bool current = old && old->archives == stamps && old->maxHair == want.maxHairColour &&
                         old->maxCloth == want.maxClothColour && old->minSide == want.minSide &&
                         old->maxSide == want.maxSide && old->bodyColours == want.bodyColours;
    if (!current) {
        std::vector<std::pair<std::string, std::vector<uint16_t>>> names;
        names.reserve(files.names().size());
        for (const std::string& n : files.names()) {
            std::vector<uint16_t> in;
            for (int a : files.archivesOf(n)) in.push_back(uint16_t(a));
            names.emplace_back(n, std::move(in));
        }
        std::vector<size_t> todo;
        auto c = std::make_shared<catalog::Catalog>(catalog::start(stamps, names, want, old.get(), todo));
        setStage("textures");
        total_ = todo.size();
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        const size_t threads = std::clamp<size_t>(si.dwNumberOfProcessors > 1 ? si.dwNumberOfProcessors - 1 : 1, 1, 16);
        std::vector<Share> shares(threads);
        std::vector<HANDLE> handles;
        for (size_t t = 0; t < threads; ++t) {
            Share& s = shares[t];
            s.work = this;
            s.c = c.get();
            s.todo = &todo;
            s.names = &files.names();
            s.files = &files;
            s.choice = want;
            s.first = t;
            s.step = threads;
            s.stop = &stop_;
            s.done = &done_;
            if (HANDLE h = CreateThread(nullptr, 0, decodeMain, &s, 0, nullptr)) handles.push_back(h);
        }
        for (HANDLE h : handles) {
            WaitForSingleObject(h, INFINITE);
            CloseHandle(h);
        }
        if (!stop_) {
            setStage("maps");
            const catalog::Read read = [&files](const std::string& n, std::vector<uint8_t>& out) {
                return files.read(n, out);
            };
            catalog::mapsOf(*c, files.maps(), read, &stop_);
        }
        if (!stop_) {
            writeWhole(cachePath, catalog::save(*c));
            AcquireSRWLockExclusive(&lock_);
            catalog_ = c;
            ReleaseSRWLockExclusive(&lock_);
        }
    }
    setStage("pack");
    readPack();
    setStage("");
    finished_ = true;
    PostMessageW(notify_, kCatalogReady, 0, 0);
}

}  // namespace uagfx::studio

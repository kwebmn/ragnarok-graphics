#include "studio/thumbs.hpp"

#include <algorithm>

#include "app/text.hpp"
#include "studio/catalog_work.hpp"
#include "studio/messages.hpp"
#include "studio/pictures.hpp"

namespace uagfx::studio {
namespace {

constexpr size_t kKeep = 6000;  // thumbs kept (8 KB each)
constexpr size_t kAsks = 400;   // asks kept: the rows scrolled past long ago are not made

}  // namespace

Thumbs::Thumbs(HWND notify, CatalogWork& files) : notify_(notify), files_(files) {
    thread_ = CreateThread(nullptr, 0, &Thumbs::threadMain, this, 0, nullptr);
}

Thumbs::~Thumbs() {
    quit_ = true;
    AcquireSRWLockExclusive(&lock_);
    WakeAllConditionVariable(&more_);
    ReleaseSRWLockExclusive(&lock_);
    if (thread_) {
        WaitForSingleObject(thread_, 10000);
        CloseHandle(thread_);
    }
}

std::shared_ptr<const Thumb> Thumbs::get(size_t file, size_t picture) {
    AcquireSRWLockExclusive(&lock_);
    const auto it = done_.find(keyOf(file, picture));
    std::shared_ptr<const Thumb> t = it == done_.end() ? nullptr : it->second;
    if (!t) {
        asks_.push_back(Ask{file, picture});
        if (asks_.size() > kAsks) asks_.erase(asks_.begin(), asks_.begin() + std::ptrdiff_t(asks_.size() - kAsks));
        WakeConditionVariable(&more_);
    }
    ReleaseSRWLockExclusive(&lock_);
    return t;
}

void Thumbs::forget() {
    AcquireSRWLockExclusive(&lock_);
    done_.clear();
    order_.clear();
    asks_.clear();
    ++generation_;
    ReleaseSRWLockExclusive(&lock_);
}

uint16_t Thumbs::flagsOf(const std::string& key) const {
    AcquireSRWLockShared(&lock_);
    const auto it = flags_.find(key);
    const uint16_t f = it == flags_.end() ? 0 : it->second;
    ReleaseSRWLockShared(&lock_);
    return f;
}

DWORD WINAPI Thumbs::threadMain(void* self) {
    static_cast<Thumbs*>(self)->run();
    return 0;
}

void Thumbs::run() {
    pack::Reader reader;
    std::shared_ptr<const catalog::PackView> readerOf;  // the pack view the reader was opened for
    ULONGLONG lastPost = 0;
    while (!quit_) {
        AcquireSRWLockExclusive(&lock_);
        while (!quit_ && asks_.empty()) SleepConditionVariableSRW(&more_, &lock_, INFINITE, 0);
        if (quit_) {
            ReleaseSRWLockExclusive(&lock_);
            break;
        }
        const Ask ask = asks_.back();
        asks_.pop_back();
        const uint64_t generation = generation_;
        const bool known = done_.count(keyOf(ask.file, ask.picture)) != 0;
        ReleaseSRWLockExclusive(&lock_);
        const auto cat = files_.catalog();
        const auto view = files_.packView();
        if (known || !cat || ask.file >= cat->files.size() || ask.picture >= cat->files[ask.file].pictures.size())
            continue;
        if (view != readerOf) {  // the pack written since: its directory again
            std::string error;
            reader.open(app::ansi(files_.packPath()), error, false);
            readerOf = view;
        }
        const catalog::FileEntry& f = cat->files[ask.file];
        const catalog::PictureOf& p = f.pictures[ask.picture];
        Image original, packed;
        originalOf(files_, *cat, f, p, original);
        auto t = std::make_shared<Thumb>();
        t->packed = view && reader.isOpen() && packPictureOf(reader, p, view->index, packed);
        if (!original.empty() && t->packed) {
            t->metrics = ai::measure(original.argb.data(), original.width, original.height, packed.argb.data(),
                                     packed.width, packed.height, p.group == uint8_t(pack::Group::World));
            t->flags = ai::flagsOf(t->metrics);
        }
        const std::vector<uint32_t> a = fitted(original, kSide), b = fitted(packed, kSide);
        t->pair.resize(size_t(kSide) * 2 * kSide);
        for (int y = 0; y < kSide; ++y)
            for (int x = 0; x < kSide; ++x) {
                t->pair[size_t(y) * kSide * 2 + size_t(x)] = a[size_t(y * kSide + x)];
                t->pair[size_t(y) * kSide * 2 + size_t(kSide + x)] = b[size_t(y * kSide + x)];
            }
        AcquireSRWLockExclusive(&lock_);
        if (generation == generation_) {
            const uint64_t k = keyOf(ask.file, ask.picture);
            done_[k] = t;
            flags_[p.key] = t->flags;
            order_.push_back(k);
            while (order_.size() > kKeep) {
                done_.erase(order_.front());
                order_.pop_front();
            }
        }
        const bool more = !asks_.empty();
        ReleaseSRWLockExclusive(&lock_);
        const ULONGLONG now = GetTickCount64();
        if (!more || now - lastPost > 100) {  // the list redrawn ten times a second at most
            lastPost = now;
            PostMessageW(notify_, kThumbReady, 0, 0);
        }
    }
}

}  // namespace uagfx::studio

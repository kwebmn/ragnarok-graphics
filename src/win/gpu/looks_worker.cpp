#include "win/gpu/looks_worker.hpp"

#include <utility>

#include "win/log.hpp"

namespace uagfx::gpu {

LooksWorker::~LooksWorker() {
    if (!thread_) return;
    AcquireSRWLockExclusive(&lock_);
    stop_ = true;
    ReleaseSRWLockExclusive(&lock_);
    WakeConditionVariable(&wake_);
    // a texture's work at most (the process' end: the thread gone already); as long as the renderer's other threads
    // are given - it reads this object until it ends
    WaitForSingleObject(thread_, 15000);
    CloseHandle(thread_);
}

bool LooksWorker::running() {
    if (!tried_) {
        tried_ = true;
        thread_ = CreateThread(nullptr, 0, &run, this, 0, nullptr);
        if (thread_) {
            SetThreadPriority(thread_, THREAD_PRIORITY_BELOW_NORMAL);
            LOG_INFO("textures: their looks and the AI pack's keys worked out on a thread of their own");
        } else {
            LOG_ERROR("textures: no thread for their looks (%lu) - worked out on the renderer's",
                      static_cast<unsigned long>(GetLastError()));
        }
    }
    return thread_ != nullptr;
}

void LooksWorker::give(uploadlater::Job&& j, bool aiKey) {
    Work w;
    w.job = std::move(j);
    w.aiKey = aiKey;
    AcquireSRWLockExclusive(&lock_);
    todo_.push_back(std::move(w));
    ReleaseSRWLockExclusive(&lock_);
    WakeConditionVariable(&wake_);
    if (!given_) progressAt_ = GetTickCount64();
    ++given_;
}

bool LooksWorker::take(uploadlater::Looks& out) {
    AcquireSRWLockExclusive(&lock_);
    const bool any = !done_.empty();
    if (any) {
        out = std::move(done_.front());
        done_.pop_front();
    }
    ReleaseSRWLockExclusive(&lock_);
    if (any && given_) --given_;
    if (any) {
        progressAt_ = GetTickCount64();
    } else if (given_ && !raised_ && thread_ && GetTickCount64() - progressAt_ > kLateMs) {
        // below the game's and the renderer's it got no turn at all: textures would wait for their fire, light, leaves
        // and AI pictures as long as the processor stays busy
        raised_ = true;
        SetThreadPriority(thread_, THREAD_PRIORITY_NORMAL);
        LOG_INFO("textures: their looks' thread got no turn for %llu ms with %zu textures in hand - its priority the "
                 "renderer's from now on", kLateMs, given_);
    }
    return any;
}

DWORD WINAPI LooksWorker::run(void* self) {
    static_cast<LooksWorker*>(self)->loop();
    return 0;
}

void LooksWorker::loop() {
    for (;;) {
        Work w;
        AcquireSRWLockExclusive(&lock_);
        while (todo_.empty() && !stop_) SleepConditionVariableSRW(&wake_, &lock_, INFINITE, 0);
        if (stop_) {
            ReleaseSRWLockExclusive(&lock_);
            return;
        }
        w = std::move(todo_.front());
        todo_.pop_front();
        ReleaseSRWLockExclusive(&lock_);
        uploadlater::Looks l;
        try {
            uploadlater::lookAt(std::move(w.job), w.aiKey, l);
        } catch (...) {  // no memory for its copies: a result of nothing (serial 0 is no texture's), the count kept
            l = uploadlater::Looks{};
        }
        AcquireSRWLockExclusive(&lock_);
        done_.push_back(std::move(l));
        ReleaseSRWLockExclusive(&lock_);
    }
}

}  // namespace uagfx::gpu

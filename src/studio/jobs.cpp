#include "studio/jobs.hpp"

#include <cstdio>

#include "app/text.hpp"
#include "core/texture_pack.hpp"
#include "studio/messages.hpp"
#include "studio/places.hpp"

namespace uagfx::studio {
namespace {

bool readWhole(const std::wstring& path, std::string& out) {
    std::FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    out.clear();
    char buf[1 << 14];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

bool writeWhole(const std::wstring& path, const std::string& text) {
    const std::wstring part = path + L".part";
    std::FILE* f = _wfopen(part.c_str(), L"wb");
    if (!f) return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 || !ok) return false;
    return MoveFileExW(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

}  // namespace

bool JobContext::engine(std::string& why) {
    usedEngine = true;
    if (jobs.engine_.running()) return true;
    std::wstring error;
    if (jobs.engine_.start(gameDir, error)) return true;
    why = app::utf8(error);
    return false;
}

Engine& JobContext::engineRef() { return jobs.engine_; }

void JobContext::progress(const JobProgress& p) {
    AcquireSRWLockExclusive(&jobs.lock_);
    jobs.progress_ = p;
    jobs.progress_.id = job.id;
    jobs.progress_.op = job.op;
    jobs.progress_.label = jobs.labels_[job.id];
    ReleaseSRWLockExclusive(&jobs.lock_);
    jobs.changed();
}

bool JobContext::stopAsked() const { return jobs.stopRunning_ || jobs.paused_ || jobs.quit_; }

Jobs::Jobs(HWND notify, std::wstring gameDir) : notify_(notify), gameDir_(std::move(gameDir)), engine_(notify) {
    std::string text;
    json::Value v;
    std::string error;
    if (readWhole(studioRoot() + L"\\queue.json", text) && json::parse(text, v, error)) queue_ = Queue::fromJson(v);
    thread_ = CreateThread(nullptr, 0, &Jobs::threadMain, this, 0, nullptr);
}

Jobs::~Jobs() {
    quit_ = true;
    AcquireSRWLockExclusive(&lock_);
    WakeAllConditionVariable(&more_);
    ReleaseSRWLockExclusive(&lock_);
    engine_.stop();  // a job waiting for the engine ends
    if (thread_) {
        WaitForSingleObject(thread_, 10000);
        CloseHandle(thread_);
    }
    save();
}

void Jobs::on(const std::string& op, Work work) {
    AcquireSRWLockExclusive(&lock_);
    work_[op] = std::move(work);
    ReleaseSRWLockExclusive(&lock_);
}

int64_t Jobs::add(const std::string& op, json::Value args, bool writesPack, const std::string& label) {
    AcquireSRWLockExclusive(&lock_);
    const int64_t id = queue_.add(op, std::move(args), writesPack);
    labels_[id] = label;
    WakeAllConditionVariable(&more_);
    ReleaseSRWLockExclusive(&lock_);
    save();
    changed();
    return id;
}

void Jobs::stop(int64_t id) {
    AcquireSRWLockExclusive(&lock_);
    if (running_ == id) stopRunning_ = true;
    else queue_.failed(id, "stopped");
    ReleaseSRWLockExclusive(&lock_);
    if (running_ == id) engine_.tell("stop", json::Value());  // a build hears it between its pictures
    save();
    changed();
}

void Jobs::pause() {
    paused_ = true;
    engine_.stop();  // the job in it fails as interrupted (run() sees paused_)
    save();
    changed();
}

void Jobs::resume() {
    paused_ = false;
    AcquireSRWLockExclusive(&lock_);
    WakeAllConditionVariable(&more_);
    ReleaseSRWLockExclusive(&lock_);
    changed();
}

std::vector<Job> Jobs::snapshot() const {
    AcquireSRWLockShared(&lock_);
    std::vector<Job> out = queue_.jobs();
    ReleaseSRWLockShared(&lock_);
    return out;
}

JobProgress Jobs::progress() const {
    AcquireSRWLockShared(&lock_);
    JobProgress p = running_ ? progress_ : JobProgress();
    ReleaseSRWLockShared(&lock_);
    return p;
}

bool Jobs::resultOf(int64_t id, json::Value& result, std::string& why) {
    AcquireSRWLockExclusive(&lock_);
    const auto it = results_.find(id);
    const bool found = it != results_.end();
    bool ok = false;
    if (found) {
        ok = it->second.first;
        result = std::move(it->second.second.first);
        why = it->second.second.second;
        results_.erase(it);
    }
    ReleaseSRWLockExclusive(&lock_);
    return found && ok;
}

std::string Jobs::engineName() { return engine_.name(); }

bool Jobs::engineWaiting() { return engine_.running() && engine_.newerInstalled(gameDir_); }

bool Jobs::packFree() {
    if (HANDLE b = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\uaro-texture-pack-builder")) {  // a builder runs
        CloseHandle(b);
        return false;
    }
    HANDLE w = OpenMutexW(SYNCHRONIZE, FALSE, app::wide(pack::kWriterMutex).c_str());
    if (!w) return true;
    const DWORD r = WaitForSingleObject(w, 0);  // a game making pictures holds it while it runs
    const bool free = r == WAIT_OBJECT_0 || r == WAIT_ABANDONED;
    if (free) ReleaseMutex(w);
    CloseHandle(w);
    return free;
}

void Jobs::save() {
    AcquireSRWLockShared(&lock_);
    const std::string text = json::dump(queue_.toJson());
    ReleaseSRWLockShared(&lock_);
    makeFolder(studioRoot());
    writeWhole(studioRoot() + L"\\queue.json", text);
}

DWORD WINAPI Jobs::threadMain(void* self) {
    static_cast<Jobs*>(self)->run();
    return 0;
}

void Jobs::run() {
    while (!quit_) {
        // The next job it may run: a pack's writer only while the pack is free (looked at every second).
        AcquireSRWLockExclusive(&lock_);
        Job* next = nullptr;
        while (!quit_ && (paused_ || !(next = queue_.next(packFree()))))
            SleepConditionVariableSRW(&more_, &lock_, 1000, 0);
        if (quit_) {
            ReleaseSRWLockExclusive(&lock_);
            break;
        }
        const Job job = *next;
        queue_.started(job.id);
        running_ = job.id;
        stopRunning_ = false;
        progress_ = JobProgress();
        progress_.id = job.id;
        progress_.op = job.op;
        progress_.label = labels_[job.id];
        const auto w = work_.find(job.op);
        const Work work = w == work_.end() ? nullptr : w->second;
        ReleaseSRWLockExclusive(&lock_);
        changed();
        json::Value result;
        std::string why;
        JobContext ctx{*this, job, gameDir_};
        const bool ok = work ? work(ctx, result, why) : (why = "no work of this kind: " + job.op, false);
        const bool lost = !ok && ctx.usedEngine && !engine_.running();
        AcquireSRWLockExclusive(&lock_);
        if (ok) queue_.done(job.id);
        else if (paused_ || quit_) queue_.interrupted(job.id);  // Pause: whole again after Resume
        else if (lost) queue_.engineLost(job.id);               // once more, on a new engine
        else queue_.failed(job.id, why);
        if (ok || (!paused_ && !lost)) results_[job.id] = {ok, {std::move(result), why}};
        running_ = 0;
        ReleaseSRWLockExclusive(&lock_);
        save();
        if (ok || (!paused_ && !lost)) PostMessageW(notify_, kJobDone, WPARAM(job.id), 0);
        changed();
        // G3: between two jobs, a newer installed engine takes over
        if (engine_.running() && engine_.newerInstalled(gameDir_)) engine_.stop();
    }
}

}  // namespace uagfx::studio

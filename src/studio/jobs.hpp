#pragma once
#include <windows.h>

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "core/studio_state.hpp"
#include "studio/engine.hpp"
#include "studio/messages.hpp"

namespace uagfx::studio {

// 2.3 (G3, G4, N1): the studio's jobs, one at a time on a thread of their own, from the queue (core/studio_state.hpp):
// a job that writes the pack waits while a game or a builder writes it; a job the engine was lost in runs once more
// on a new engine; between two jobs a newer installed engine takes over. Pause ends the engine and keeps the queue
// (studio\queue.json); Resume goes on.
struct JobProgress {  // what the bar shows of the job running
    int64_t id = 0;
    std::string op, label, stage, current;
    uint64_t done = 0, total = 0;
    double rate = 0;  // things a second
    int eta = -1;     // seconds left
    std::vector<std::string> threads;
};

class Jobs;
// What a job's work gets: the engine (started when first wanted), its progress, a stop asked.
struct JobContext {
    Jobs& jobs;
    const Job& job;
    std::wstring gameDir;
    bool usedEngine = false;        // a failure with the engine gone meanwhile: the engine was lost
    bool engine(std::string& why);  // the engine running (started now when it is not)
    Engine& engineRef();
    void progress(const JobProgress& p);
    bool stopAsked() const;
};

class Jobs {
public:
    using Work = std::function<bool(JobContext& ctx, json::Value& result, std::string& why)>;
    Jobs(HWND notify, std::wstring gameDir);
    ~Jobs();
    void on(const std::string& op, Work work);  // the work of a kind of job
    int64_t add(const std::string& op, json::Value args, bool writesPack, const std::string& label);
    void stop(int64_t id);       // a queued one goes; the running one is asked to stop
    void pause();                // the engine ends, the running job goes back to the queue, the queue is written
    void resume();
    bool paused() const { return paused_; }
    std::vector<Job> snapshot() const;  // the queue as it is now
    JobProgress progress() const;       // the job running
    bool resultOf(int64_t id, json::Value& result, std::string& why);  // a finished job's, taken once
    std::string engineName();
    bool engineWaiting();  // a newer engine is installed and takes over after the job running
    static bool packFree();  // no builder runs and no game writes the pack (its mutexes)

private:
    friend struct JobContext;
    static DWORD WINAPI threadMain(void* self);
    void run();
    void save();  // studio\queue.json
    void changed() { PostMessageW(notify_, kJobsChanged, 0, 0); }

    HWND notify_;
    std::wstring gameDir_;
    Engine engine_;
    std::map<std::string, Work> work_;
    mutable SRWLOCK lock_ = SRWLOCK_INIT;  // queue_, progress_, results_
    CONDITION_VARIABLE more_ = CONDITION_VARIABLE_INIT;
    Queue queue_;
    std::map<int64_t, std::string> labels_;
    JobProgress progress_;
    std::map<int64_t, std::pair<bool, std::pair<json::Value, std::string>>> results_;
    std::atomic<bool> paused_{false}, quit_{false}, stopRunning_{false};
    std::atomic<int64_t> running_{0};
    HANDLE thread_ = nullptr;
};

}  // namespace uagfx::studio

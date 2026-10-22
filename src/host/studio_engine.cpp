#include "host/studio_engine.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <memory>
#include <vector>

#include "core/ai/groups.hpp"
#include "core/config.hpp"
#include "host/pack_builder.hpp"
#include "core/studio_link.hpp"
#include "core/studio_status.hpp"
#include "core/version.hpp"
#include "win/gpu/renderer.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::host {
namespace {

// The studio's paths come as UTF-8 (its folder may be out of the ANSI code page): opened by wide names.
std::wstring wide(const std::string& utf8) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring w(size_t(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), w.data(), n);
    return w;
}

bool readFile(const std::string& path, std::string& out) {
    std::FILE* f = _wfopen(wide(path).c_str(), L"rb");
    if (!f) return false;
    out.clear();
    char buf[1 << 16];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

// Whole into a file of its own, then put in its place: the studio never reads half of one.
bool writeFile(const std::string& path, const std::string& bytes) {
    const std::wstring to = wide(path), part = to + L".part";
    std::FILE* f = _wfopen(part.c_str(), L"wb");
    if (!f) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (std::fclose(f) != 0 || !ok) {
        DeleteFileW(part.c_str());
        return false;
    }
    return MoveFileExW(part.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

// The pipe's other end: lines in on a thread of their own, lines out from the main one. Overlapped: a handle opened
// for synchronous I/O lets one call at a time through, and the reader's wait would hold every reply back.
class Pipe {
public:
    bool connect(const std::string& name) {
        const std::string path = "\\\\.\\pipe\\" + name;
        for (int tries = 0; tries < 50; ++tries) {
            handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED, nullptr);
            if (handle_ != INVALID_HANDLE_VALUE) break;
            if (GetLastError() == ERROR_PIPE_BUSY) WaitNamedPipeA(path.c_str(), 100);
            else Sleep(100);
        }
        if (handle_ == INVALID_HANDLE_VALUE) return false;
        reader_ = CreateThread(nullptr, 0, &Pipe::readMain, this, 0, nullptr);
        return reader_ != nullptr;
    }
    ~Pipe() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CancelIoEx(handle_, nullptr);  // the reader's read ends
            CloseHandle(handle_);
        }
        if (reader_) {
            WaitForSingleObject(reader_, 2000);
            CloseHandle(reader_);
        }
    }
    void send(const studio::Message& m) {
        const std::string line = studio::lineOf(m);
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        DWORD n = 0;
        if (!WriteFile(handle_, line.data(), DWORD(line.size()), nullptr, &ov) &&
            (GetLastError() != ERROR_IO_PENDING || !GetOverlappedResult(handle_, &ov, &n, TRUE)))
            gone_ = true;
        CloseHandle(ov.hEvent);
    }
    bool stopAsked() const { return stop_ || gone_; }
    void clearStop() { stop_ = false; }
    // The next request; false when the pipe broke (the studio is gone) and nothing is left.
    bool next(studio::Message& out) {
        AcquireSRWLockExclusive(&lock_);
        while (queue_.empty() && !gone_) SleepConditionVariableSRW(&more_, &lock_, INFINITE, 0);
        const bool some = !queue_.empty();
        if (some) {
            out = std::move(queue_.front());
            queue_.pop_front();
        }
        ReleaseSRWLockExclusive(&lock_);
        return some;
    }

private:
    static DWORD WINAPI readMain(void* self) {
        static_cast<Pipe*>(self)->read();
        return 0;
    }
    void read() {
        studio::LineReader lines;
        char buf[4096];
        DWORD n = 0;
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        const auto readSome = [&]() {
            ResetEvent(ov.hEvent);
            if (ReadFile(handle_, buf, sizeof buf, nullptr, &ov) || GetLastError() == ERROR_IO_PENDING)
                return GetOverlappedResult(handle_, &ov, &n, TRUE) && n;
            return false;
        };
        while (readSome()) {
            lines.feed(buf, n);
            studio::Message m;
            while (lines.next(m)) {
                if (m.op == "stop" || m.op == "quit") stop_ = true;  // heard in the middle of a build too
                if (m.op == "stop") continue;
                AcquireSRWLockExclusive(&lock_);
                queue_.push_back(std::move(m));
                WakeConditionVariable(&more_);
                ReleaseSRWLockExclusive(&lock_);
            }
        }
        CloseHandle(ov.hEvent);
        AcquireSRWLockExclusive(&lock_);
        gone_ = true;
        WakeConditionVariable(&more_);
        ReleaseSRWLockExclusive(&lock_);
    }

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    HANDLE reader_ = nullptr;
    SRWLOCK lock_ = SRWLOCK_INIT;
    CONDITION_VARIABLE more_ = CONDITION_VARIABLE_INIT;
    std::deque<studio::Message> queue_;
    std::atomic<bool> gone_{false}, stop_{false};
};

// 2.3 (N1): a build's status as the engine's progress replies to its request.
class PipeSink : public BuildSink {
public:
    PipeSink(Pipe& pipe, int64_t id) : pipe_(pipe), id_(id) {}
    void write(const pack::Status& s, bool now) override {
        const ULONGLONG t = GetTickCount64();
        if (!now && t - last_ < 500) return;
        last_ = t;
        pipe_.send(studio::Message{id_, "progress", studio::statusJson(s)});
    }
    bool stopAsked() override { return pipe_.stopAsked(); }

private:
    Pipe& pipe_;
    int64_t id_;
    ULONGLONG last_ = 0;
};

class Engine {
public:
    Engine(std::string gameDir, Pipe& pipe) : dir_(std::move(gameDir)), pipe_(pipe) {}
    void run() {
        studio::Message m;
        while (pipe_.next(m)) {
            if (m.op == "quit") break;
            if (m.op == "hello") hello(m);
            else if (m.op == "enlarge") enlarge(m);
            else if (m.op == "build") build(m);
            else if (m.op != "stop") fail(m, "no such request: " + m.op);
        }
    }

private:
    void reply(const studio::Message& to, const char* op, json::Value body) {
        pipe_.send(studio::Message{to.id, op, std::move(body)});
    }
    void fail(const studio::Message& to, const std::string& why) {
        LOG_ERROR("studio engine: %s: %s", to.op.c_str(), why.c_str());
        json::Value b = json::Value::object();
        b.set("why", why);
        reply(to, "error", b);
    }
    void hello(const studio::Message& m) {
        json::Value b = json::Value::object();
        b.set("version", kVersion);
        b.set("adapter", renderer_ ? renderer_->adapterName() : std::string());
        json::Value models = json::Value::array();
        for (int i = 0; i < ai::kModels; ++i) models.push(ai::modelToken(i));
        b.set("models", models);
        reply(m, "result", b);
    }
    // The card and the networks' pipelines, at the first picture asked for.
    bool ready(std::string& error) {
        if (renderer_) return true;
        auto r = std::make_unique<gpu::Renderer>();
        r->setShaderCache(win::ourFile(dir_, layout::File::StudioShaders64));
        if (!r->create(error) || !r->startAiForStudio(dir_, error)) return false;
        LOG_INFO("studio engine: %s", r->adapterName().c_str());
        renderer_ = std::move(r);
        return true;
    }
    void enlarge(const studio::Message& m) {
        const ULONGLONG start = GetTickCount64();
        std::string bytes, error;
        int w = 0, h = 0;
        std::vector<uint32_t> argb, out;
        if (!readFile(m.body["in"].asString(), bytes) || !studio::parseRaw(bytes, w, h, argb))
            return fail(m, "no picture in " + m.body["in"].asString());
        if (!ready(error)) return fail(m, "the graphics card: " + error);
        const std::string model = m.body["model"].asString().empty() ? "general" : m.body["model"].asString();
        const int denoise = int(m.body["denoise"].asInt(-1));
        const uint32_t border = uint32_t(m.body["border"].asInt(0));
        if (!renderer_->enlargeByModel(model, denoise, argb.data(), w, h, border, out, error))
            return fail(m, model + ": " + error);
        if (!writeFile(m.body["out"].asString(), studio::rawOf(w * 4, h * 4, out)))
            return fail(m, "cannot write " + m.body["out"].asString());
        json::Value b = json::Value::object();
        b.set("width", w * 4);
        b.set("height", h * 4);
        b.set("ms", int64_t(GetTickCount64() - start));
        reply(m, "result", b);
    }

    // 2.3 (N1, N2): a texture pack build (host/pack_builder.hpp) - its status as progress, its catalog the studio's.
    void build(const studio::Message& m) {
        const json::Value& b = m.body;
        PackArgs a;
        a.mode = b["mode"].asString().empty() ? "build" : b["mode"].asString();
        a.dir = dir_;
        a.choice = pack::parseChoice(b["kinds"].asString());
        a.ai = parseAiArgument(b["ai"].asString());
        a.choice.minSide = std::min(a.ai.minSize, a.ai.maxSize);
        a.choice.maxSide = std::max(a.ai.minSize, a.ai.maxSize);
        a.quality = int(std::clamp<int64_t>(b["quality"].asInt(90), 1, 100));
        a.fresh = b["fresh"].asBool();
        a.map = b["map"].asString();
        if (b.has("mapKinds")) {
            a.mapGround = b["mapKinds"].asString().find("ground") != std::string::npos;
            a.mapModels = b["mapKinds"].asString().find("models") != std::string::npos;
        }
        a.remake = b["remake"].asBool();
        if (b["colours"].items().size() == 2) {
            a.choice.maxHairColour = int(b["colours"][size_t(0)].asInt(20));
            a.choice.maxClothColour = int(b["colours"][size_t(1)].asInt(77));
        }
        if (!b["catalog"].asString().empty() && !readFile(b["catalog"].asString(), a.catalogData))
            a.catalogData.clear();
        renderer_.reset();  // the build takes the card itself
        pipe_.clearStop();
        PipeSink sink(pipe_, m.id);
        const int code = runPack(a, &sink);
        json::Value r = json::Value::object();
        r.set("code", code);
        reply(m, "result", r);
    }

    std::string dir_;
    Pipe& pipe_;
    std::unique_ptr<gpu::Renderer> renderer_;
};

}  // namespace

int runStudio(const std::string& gameDir, const std::string& pipe) {
    Pipe p;
    if (!p.connect(pipe)) {
        LOG_ERROR("studio engine: no studio at the pipe %s", pipe.c_str());
        return 2;
    }
    LOG_INFO("studio engine %s: connected", kVersion);
    Engine(gameDir, p).run();
    LOG_INFO("studio engine: done");
    return 0;
}

}  // namespace uagfx::host

#include "host/pack_builder.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <deque>
#include <exception>
#include <map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/ai/kinds.hpp"
#include "core/ai/texture_cache.hpp"
#include "core/grf/grf.hpp"
#include "core/texture_pack.hpp"
#include "core/webp_image.hpp"
#include "win/gpu/renderer.hpp"
#include "win/log.hpp"

namespace uagfx::host {
namespace {

// The game's file names are Korean in CP949 (the GRF's bytes): UTF-8 for the status and the log.
std::string utf8Of(const std::string& cp949) {
    if (cp949.empty()) return cp949;
    const int n = MultiByteToWideChar(949, 0, cp949.data(), int(cp949.size()), nullptr, 0);
    if (n <= 0) return cp949;
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(949, 0, cp949.data(), int(cp949.size()), w.data(), n);
    const int m = WideCharToMultiByte(CP_UTF8, 0, w.data(), n, nullptr, 0, nullptr, nullptr);
    std::string out(size_t(std::max(m, 0)), '\0');
    if (m > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), n, out.data(), m, nullptr, nullptr);
    return out;
}

bool exists(const std::string& path) { return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

// A file broken past what the readers check (no memory for what its header claims): none of its pictures, a line in
// the log - the rest of the game's files go on.
std::vector<pack::Picture> picturesOf(const std::string& name, const pack::Source& s, const std::vector<uint8_t>& data,
                                      const pack::Choice& c) {
    try {
        return pack::picturesOf(s, data.data(), data.size(), c);
    } catch (const std::exception& e) {
        LOG_ERROR("texture pack: %s left out (%s)", utf8Of(name).c_str(), e.what());
        return {};
    }
}

// The status file, replaced whole (the settings app never reads half of one); at most every half second unless now.
class StatusFile {
public:
    explicit StatusFile(std::string path) : path_(std::move(path)) { DeleteFileA((path_ + ".stop").c_str()); }
    void write(const pack::Status& s, bool now = false) {
        const ULONGLONG t = GetTickCount64();
        if (!now && t - last_ < 500) return;
        last_ = t;
        const std::string text = pack::statusText(s), part = path_ + ".part";
        std::FILE* f = std::fopen(part.c_str(), "wb");
        if (!f) return;
        const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
        std::fclose(f);
        if (ok) MoveFileExA(part.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    // The settings app's stop: its file, gone once seen. Looked for at most every 300 ms.
    bool stopAsked() {
        const ULONGLONG t = GetTickCount64();
        if (t - looked_ < 300) return false;
        looked_ = t;
        const std::string stop = path_ + ".stop";
        if (!exists(stop)) return false;
        DeleteFileA(stop.c_str());
        return true;
    }

private:
    std::string path_;
    ULONGLONG last_ = 0, looked_ = 0;
};

// A picture the network made, to be WebP and in the pack.
struct Made {
    std::string name;
    int width = 0, height = 0;
    std::vector<uint32_t> argb;
};

// The WebP encoders: threads of their own (WebP takes longer than the network), each writing what it made into the
// pack under the writer's lock.
class Encoders {
public:
    Encoders(pack::Writer& writer, int quality, int threads) : writer_(writer), quality_(quality) {
        for (int i = 0; i < threads; ++i)
            if (HANDLE h = CreateThread(nullptr, 0, &Encoders::main, this, 0, nullptr)) threads_.push_back(h);
    }
    ~Encoders() { finish(); }
    size_t threads() const { return threads_.size(); }
    // Waits while the queue is full: a picture of 2048 x 2048 is 16 MB.
    void push(Made&& m) {
        const size_t most = std::max<size_t>(4, threads_.size() * 2);
        AcquireSRWLockExclusive(&lock_);
        while (queue_.size() >= most) SleepConditionVariableSRW(&room_, &lock_, INFINITE, 0);
        queue_.push_back(std::move(m));
        WakeConditionVariable(&more_);
        ReleaseSRWLockExclusive(&lock_);
    }
    void drain() {  // until every picture pushed is in the pack
        AcquireSRWLockExclusive(&lock_);
        while (!queue_.empty() || busy_) SleepConditionVariableSRW(&room_, &lock_, INFINITE, 0);
        ReleaseSRWLockExclusive(&lock_);
    }
    void finish() {
        AcquireSRWLockExclusive(&lock_);
        stop_ = true;
        WakeAllConditionVariable(&more_);
        ReleaseSRWLockExclusive(&lock_);
        for (HANDLE h : threads_) {
            WaitForSingleObject(h, INFINITE);
            CloseHandle(h);
        }
        threads_.clear();
    }
    // The writer, for a checkpoint or the pack's size: the encoders wait meanwhile.
    void lockWriter() { AcquireSRWLockExclusive(&write_); }
    void unlockWriter() { ReleaseSRWLockExclusive(&write_); }
    uint32_t written() {
        lockWriter();
        const uint32_t n = written_;
        unlockWriter();
        return n;
    }
    std::string error() {
        lockWriter();
        const std::string e = error_;
        unlockWriter();
        return e;
    }

private:
    static DWORD WINAPI main(void* self) {
        static_cast<Encoders*>(self)->run();
        return 0;
    }
    void run() {
        for (;;) {
            AcquireSRWLockExclusive(&lock_);
            while (queue_.empty() && !stop_) SleepConditionVariableSRW(&more_, &lock_, INFINITE, 0);
            if (queue_.empty()) {
                ReleaseSRWLockExclusive(&lock_);
                return;
            }
            Made m = std::move(queue_.front());
            queue_.pop_front();
            ++busy_;
            WakeConditionVariable(&room_);
            ReleaseSRWLockExclusive(&lock_);
            const std::vector<uint8_t> data = webp::encode(m.argb.data(), m.width, m.height, quality_);
            lockWriter();
            std::string why;
            if (!data.empty() && writer_.add(m.name, data.data(), data.size(), why))
                ++written_;
            else if (error_.empty())
                error_ = data.empty() ? "WebP did not take " + m.name : why;
            unlockWriter();
            AcquireSRWLockExclusive(&lock_);
            --busy_;
            WakeAllConditionVariable(&room_);
            ReleaseSRWLockExclusive(&lock_);
        }
    }

    pack::Writer& writer_;
    const int quality_;
    std::vector<HANDLE> threads_;
    SRWLOCK lock_ = SRWLOCK_INIT;   // the queue
    SRWLOCK write_ = SRWLOCK_INIT;  // the writer
    CONDITION_VARIABLE more_ = CONDITION_VARIABLE_INIT, room_ = CONDITION_VARIABLE_INIT;
    std::deque<Made> queue_;
    int busy_ = 0;
    bool stop_ = false;
    uint32_t written_ = 0;
    std::string error_;
};

// Pictures a second over the last minute, and what is left of the run at that rate.
class Pace {
public:
    void note(uint32_t made) {
        const ULONGLONG t = GetTickCount64();
        if (!samples_.empty() && t - samples_.back().first < 1000) return;
        samples_.emplace_back(t, made);
        while (samples_.size() > 2 && t - samples_.front().first > 60000) samples_.pop_front();
    }
    double rate() const {
        if (samples_.size() < 2) return 0;
        const double seconds = double(samples_.back().first - samples_.front().first) / 1000.0;
        return seconds > 0 ? double(samples_.back().second - samples_.front().second) / seconds : 0;
    }
    int eta(uint32_t left) const {
        const double r = rate();
        return r > 0 ? int(double(left) / r + 0.5) : -1;
    }

private:
    std::deque<std::pair<ULONGLONG, uint32_t>> samples_;
};

uint32_t borderOf(pack::Group g) { return g == pack::Group::World ? 0u : g == pack::Group::Sprites ? 1u : 2u; }

class Builder {
public:
    explicit Builder(const PackArgs& a) : a_(a), status_(a.status) {
        s_.pid = GetCurrentProcessId();
        s_.check = a.mode == "check";
    }
    int run();

private:
    int fail(const std::string& why) {
        LOG_ERROR("texture pack: %s", why.c_str());
        s_.state = pack::Status::Failed;
        s_.message = why;
        status_.write(s_, true);
        return 3;
    }
    bool listFiles();  // the game's files of the choice
    int check();
    int build();
    // The index and the central directory after the last picture: a whole ZIP (last: and the file closed).
    bool checkpoint(Encoders* enc, bool last);
    int stopped();  // in the middle: the pack whole again, the status says so

    const PackArgs& a_;
    StatusFile status_;
    pack::Status s_;
    pack::GameFiles files_;
    std::vector<std::pair<std::string, pack::Source>> list_;
    pack::Writer writer_;
    std::map<std::string, ai::Kind> kinds_;  // every picture's of this run and of the pack's index
    // 0.45: the keys of the world's textures at the game's lower texture details, to their whole textures' pictures.
    std::map<std::string, std::string> aliases_;
    void noteLowerDetails(const pack::Picture& p, const std::string& name);
};

void Builder::noteLowerDetails(const pack::Picture& p, const std::string& name) {
    if (p.group != pack::Group::World) return;
    for (int div : {2, 4}) {
        pack::Picture low;
        if (pack::lowerDetail(p, div, low) && low.key != p.key) aliases_.emplace(low.key, name);
    }
}

bool Builder::listFiles() {
    std::string error;
    if (!files_.open(a_.dir, error)) {
        fail("the game's files: " + error);
        return false;
    }
    LOG_INFO("texture pack: %s", files_.report().c_str());
    for (const std::string& n : files_.names()) {
        const pack::Source src = pack::sourceOf(n, a_.choice);
        if (src.type != pack::Source::None) list_.emplace_back(n, src);
    }
    s_.filesTotal = uint32_t(list_.size());
    return true;
}

int Builder::run() {
    s_.state = pack::Status::Starting;
    status_.write(s_, true);
    if (a_.mode == "check") return check();
    if (a_.mode == "build") return build();
    return fail("no such work: " + a_.mode);
}

// How the builder's keys stand against the pictures the game made into upscaled.grf: whether the pack's pictures are
// the ones the game will look for.
int Builder::check() {
    if (!listFiles()) return 3;
    s_.state = pack::Status::Checking;
    std::unordered_set<std::string> keys, lower[2];  // 0.45: and the world's at half and a quarter a side
    uint32_t groups[3] = {};
    std::vector<uint8_t> data;
    for (const auto& f : list_) {
        if (status_.stopAsked()) {
            s_.state = pack::Status::Stopped;
            status_.write(s_, true);
            return 0;
        }
        s_.current = utf8Of(f.first);
        if (files_.read(f.first, data))
            for (const pack::Picture& p : picturesOf(f.first, f.second, data, a_.choice)) {
                if (keys.insert(p.key).second) ++groups[int(p.group)];
                pack::Picture low;
                for (int i = 0; i < 2 && p.group == pack::Group::World; ++i)
                    if (pack::lowerDetail(p, 2 << i, low)) lower[i].insert(low.key);
            }
        ++s_.filesDone;
        status_.write(s_);
    }
    s_.current.clear();
    std::vector<std::string> cache;
    const std::string grfPath = a_.dir + "\\upscaled.grf";
    std::string text;
    if (std::FILE* f = std::fopen(grfPath.c_str(), "rb")) {
        grf::Archive archive;
        std::string error;
        if (archive.open(f, false, error))  // the archive closes the file, also when it does not open
            cache = archive.names();
        else
            text = "upscaled.grf: " + error + "; ";
    } else {
        text = "no upscaled.grf to check against; ";
    }
    const pack::KeyCheck k = pack::checkKeys(cache, keys);
    // The ones not found: made at a lower texture detail of the game?
    uint32_t atLower[2] = {};
    std::unordered_set<std::string> counted;
    for (const std::string& name : cache) {
        const ai::ParsedEntry e = ai::parseEntry(name);
        if (e.type != ai::ParsedEntry::Picture || keys.count(e.key) || !counted.insert(e.key).second) continue;
        if (lower[0].count(e.key))
            ++atLower[0];
        else if (lower[1].count(e.key))
            ++atLower[1];
    }
    text += "found " + pack::keyCheckText(k) + "; the pack would hold " + std::to_string(keys.size()) +
            " pictures (world " + std::to_string(groups[0]) + ", sprites " + std::to_string(groups[1]) +
            ", interface " + std::to_string(groups[2]) + "); of the rest, at a lower texture detail: half " +
            std::to_string(atLower[0]) + ", a quarter " + std::to_string(atLower[1]);
    LOG_INFO("texture pack, the keys: %s", text.c_str());
    for (const std::string& m : k.missing) LOG_INFO("texture pack: no file gives %s", m.c_str());
    s_.state = pack::Status::Done;
    s_.message = text;
    s_.missing = k.missing;
    status_.write(s_, true);
    return 0;
}

bool Builder::checkpoint(Encoders* enc, bool last) {
    if (enc) enc->lockWriter();
    std::map<std::string, ai::Kind> index;
    for (const std::string& n : writer_.names()) {
        const auto it = kinds_.find(n);
        index[n] = it == kinds_.end() ? ai::Kind::None : it->second;
    }
    std::map<std::string, std::string> aliases;  // to the pictures in the pack
    for (const auto& a : aliases_)
        if (index.count(a.second)) aliases.insert(a);
    std::string error;
    const std::string text = pack::indexText(index, aliases);
    const bool ok = last ? writer_.close(text, error) : writer_.checkpoint(text, error);
    s_.inPack = uint32_t(index.size());
    s_.bytes = writer_.bytes();
    if (enc) enc->unlockWriter();
    if (!ok) LOG_ERROR("texture pack: %s", error.c_str());
    return ok;
}

int Builder::stopped() {
    checkpoint(nullptr, true);
    s_.state = pack::Status::Stopped;
    s_.current.clear();
    s_.eta = -1;
    status_.write(s_, true);
    LOG_INFO("texture pack: stopped - %u pictures made, %u in the pack", s_.made, s_.inPack);
    return 0;
}

int Builder::build() {
    if (!pack::anyChosen(a_.choice)) return fail("nothing is chosen to make");
    if (!listFiles()) return 3;
    const std::string out = a_.out;
    // The kinds of what the pack holds: its index, before the writer drops it to go on.
    if (!a_.fresh && exists(out)) {
        pack::Reader old;
        std::string error;
        std::vector<uint8_t> index;
        if (old.open(out, error) && old.read(pack::kIndexName, index)) {
            kinds_ = pack::parseIndex(std::string(index.begin(), index.end()));
            aliases_ = pack::parseAliases(std::string(index.begin(), index.end()));
        }
    }
    std::string error;
    if (!writer_.open(out, !a_.fresh, error)) return fail(out + ": " + error);
    // From here on the pack has no central directory until checkpoint(): every way out goes through one.
    std::unordered_set<std::string> done;  // in the pack, or on their way to it
    for (const std::string& n : writer_.names()) done.insert(n);
    LOG_INFO("texture pack: %s, %zu pictures there%s", out.c_str(), done.size(), a_.fresh ? " (a new one)" : "");
    // First what is to be made: the progress and the time left are the pictures', not the files'.
    s_.state = pack::Status::Reading;
    s_.inPack = uint32_t(done.size());
    std::vector<size_t> todo;  // the files with pictures to make
    std::vector<uint8_t> data;
    std::unordered_set<std::string> seen;
    for (size_t i = 0; i < list_.size(); ++i) {
        if (status_.stopAsked()) return stopped();
        s_.current = utf8Of(list_[i].first);
        bool some = false;
        if (files_.read(list_[i].first, data))
            for (const pack::Picture& p : picturesOf(list_[i].first, list_[i].second, data, a_.choice)) {
                const std::string name = pack::pictureName(p.group, p.key);
                if (!seen.insert(name).second) continue;
                kinds_[name] = list_[i].second.kind;
                noteLowerDetails(p, name);
                if (!done.count(name)) {
                    ++s_.toMake;
                    some = true;
                }
            }
        if (some) todo.push_back(i);
        ++s_.filesDone;
        status_.write(s_);
    }
    LOG_INFO("texture pack: %u pictures to make of %zu files; %zu keys of the world's textures at a lower texture "
             "detail", s_.toMake, todo.size(), aliases_.size());
    if (!s_.toMake) {
        checkpoint(nullptr, true);
        s_.state = pack::Status::Done;
        s_.current.clear();
        status_.write(s_, true);
        return 0;
    }
    // The networks on the graphics card.
    s_.state = pack::Status::Starting;
    s_.current.clear();
    status_.write(s_, true);
    gpu::Renderer renderer;
    renderer.setShaderCache(a_.dir + "\\uaro-graphics\\shaders-64.bin");
    if (!renderer.create(error) || !renderer.startAiForPack(a_.dir, a_.ai, error)) {
        checkpoint(nullptr, true);
        return fail("the graphics card: " + error);
    }
    LOG_INFO("texture pack: %s; the ground and objects by %s, the rest by %s, WebP quality %d",
             renderer.adapterName().c_str(), renderer.packNetName(int(ai::Kind::Ground)).c_str(),
             renderer.packNetName(int(ai::Kind::Sprites)).c_str(), a_.quality);
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const int threads = std::clamp(int(si.dwNumberOfProcessors) - 1, 1, 16);
    Encoders enc(writer_, a_.quality, threads);
    s_.state = pack::Status::Building;
    s_.filesDone = 0;
    s_.filesTotal = uint32_t(todo.size());
    Pace pace;
    ULONGLONG saved = GetTickCount64();
    uint32_t failed = 0;
    std::vector<uint32_t> big;
    bool stop = false;
    for (size_t i : todo) {
        s_.current = utf8Of(list_[i].first);
        if (!files_.read(list_[i].first, data)) data.clear();
        for (pack::Picture& p : picturesOf(list_[i].first, list_[i].second, data, a_.choice)) {
            const std::string name = pack::pictureName(p.group, p.key);
            if (!done.insert(name).second) continue;
            std::string why;
            if (!renderer.enlargeForPack(int(list_[i].second.kind), p.argb.data(), p.width, p.height,
                                         borderOf(p.group), big, why)) {
                if (renderer.lost()) {
                    enc.drain();
                    checkpoint(&enc, true);
                    return fail("the graphics card: " + why);
                }
                if (failed++ < 20)
                    LOG_ERROR("texture pack: %s (%dx%d): %s", name.c_str(), p.width, p.height, why.c_str());
                continue;
            }
            Made m;
            m.name = name;
            m.width = p.width * 4;
            m.height = p.height * 4;
            m.argb = std::move(big);
            big.clear();
            enc.push(std::move(m));
            ++s_.made;
            pace.note(s_.made);
            s_.rate = pace.rate();
            s_.eta = pace.eta(s_.toMake - std::min(s_.made, s_.toMake));
            if (GetTickCount64() - saved > 60000) {  // a whole ZIP now and then: a crash loses a minute at most
                if (!checkpoint(&enc, false)) return fail("the pack could not be written");
                saved = GetTickCount64();
            }
            status_.write(s_);
            if (status_.stopAsked()) {
                stop = true;
                break;
            }
        }
        if (stop) break;
        ++s_.filesDone;
        const std::string encError = enc.error();
        if (!encError.empty()) {
            enc.drain();
            checkpoint(&enc, true);
            return fail(encError);
        }
    }
    enc.finish();  // every picture pushed in the pack
    if (stop) return stopped();
    if (!checkpoint(nullptr, true)) return fail("the pack could not be written");
    s_.current.clear();
    s_.state = pack::Status::Done;
    s_.eta = 0;
    status_.write(s_, true);
    const std::string missed = failed ? ", " + std::to_string(failed) + " the graphics card did not make" : "";
    LOG_INFO("texture pack: done - %u pictures made, %u in the pack, %.1f MB%s", s_.made, s_.inPack,
             double(s_.bytes) / 1048576.0, missed.c_str());
    return 0;
}

}  // namespace

int runPack(const PackArgs& args) {
    PackArgs a = args;
    if (a.out.empty()) a.out = a.dir + "\\uaro-textures.zip";
    if (a.status.empty()) a.status = a.dir + "\\uaro-graphics\\pack-status.txt";
    // One builder at a time: a second would write the same pack.
    HANDLE one = CreateMutexA(nullptr, FALSE, "Local\\uaro-texture-pack-builder");
    if (one && GetLastError() == ERROR_ALREADY_EXISTS) {
        LOG_ERROR("texture pack: another builder is running");
        CloseHandle(one);
        return 4;
    }
    LOG_INFO("texture pack: %s, kinds %s, quality %d", a.mode.c_str(), pack::choiceText(a.choice).c_str(), a.quality);
    Builder b(a);
    const int code = b.run();
    if (one) CloseHandle(one);
    return code;
}

}  // namespace uagfx::host

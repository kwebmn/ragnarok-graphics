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
#include "core/ai/pack_models.hpp"
#include "core/ai/texture_cache.hpp"
#include "core/near_picture.hpp"
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
// 0.49: a head's frames in every hair colour the game has too (palettesOf) - the game draws it with one of them.
std::vector<pack::Picture> picturesOf(pack::GameFiles& files, const std::string& name, const pack::Source& s,
                                      const std::vector<uint8_t>& data, const pack::Choice& c) {
    try {
        std::vector<pack::Picture> out = pack::picturesOf(s, data.data(), data.size(), c);
        if (s.type != pack::Source::Sprite) return out;
        std::vector<uint8_t> palette;
        for (const std::string& p : pack::palettesOf(name, files.names(), c.bodyColours))
            if (files.read(p, palette) && palette.size() >= 1024)
                for (pack::Picture& f : pack::picturesOf(s, data.data(), data.size(), c, palette.data()))
                    out.push_back(std::move(f));
        return out;
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
    bool replace = false;  // 1.6: made again, into its place
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
            if (!data.empty() && writer_.add(m.name, data.data(), data.size(), why, m.replace))
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
    int build();
    int preview();  // 1.6: one picture of a group through every model, for the AI page
    // The index and the central directory after the last picture: a whole ZIP (last: and the file closed).
    bool checkpoint(Encoders* enc, bool last);
    int stopped();  // in the middle: the pack whole again, the status says so

    const PackArgs& a_;
    StatusFile status_;
    pack::Status s_;
    pack::GameFiles files_;
    std::vector<std::pair<std::string, pack::Source>> list_;
    pack::Writer writer_;
    // Every picture's kind of this run and of the pack's index (0.48: with the maps the game's own making noted), and
    // (0.45) the keys of the world's textures at the game's lower texture details, to their whole textures' pictures.
    pack::Index index_;
    std::unordered_set<std::string> redo_;  // 1.6: pictures made again by their group's model
    void noteLowerDetails(const pack::Picture& p, const std::string& name);
};

void Builder::noteLowerDetails(const pack::Picture& p, const std::string& name) {
    if (p.group != pack::Group::World) return;
    for (int div : {2, 4}) {
        pack::Picture low;
        if (pack::lowerDetail(p, div, low) && low.key != p.key) index_.aliases.emplace(low.key, name);
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
    if (a_.mode == "build") return build();
    if (a_.mode == "preview") return preview();
    return fail("no such work: " + a_.mode);
}

// 1.6: the AI page's preview - a picture of the group (a well-known one where the game has it: a poring, a Kafra),
// at most 128 a side (its middle), through each model; into a_.out: source.bmp (the game's, 4 times by its pixels),
// general.bmp, anime.bmp, anime6b.bmp - on a grey ground where it is transparent - and preview.txt (where it is from).
int Builder::preview() {
    ai::Group want = ai::Group::Monsters;
    for (int g = 0; g < ai::kGroups; ++g)
        if (a_.redo[size_t(g)]) want = ai::Group(g);  // the group comes as --redo's one
    std::string error;
    if (!files_.open(a_.dir, error)) return fail("the game's files: " + error);
    const pack::Choice all;
    const char* const hints[ai::kGroups] = {"prontera", "poring", "novice", "kafra",
                                            "apple",    "fire",   "item",   "loading"};
    pack::Picture found;
    std::string from;
    std::vector<uint8_t> data;
    for (int pass = 0; pass < 2 && from.empty(); ++pass) {  // the hint's files first, then any of the group
        int tried = 0;
        for (const std::string& n : files_.names()) {
            const pack::Source src = pack::sourceOf(n, all);
            if (src.type == pack::Source::None) continue;
            if (pass == 0 && n.find(hints[int(want)]) == std::string::npos) continue;
            if (++tried > 400 || !files_.read(n, data)) continue;
            for (pack::Picture& p : picturesOf(files_, n, src, data, all))
                if (ai::groupOfPicture(n, src, p.width, p.height) == want && p.width >= 24 && p.height >= 24) {
                    found = std::move(p);
                    from = n;
                    break;
                }
            if (!from.empty()) break;
        }
    }
    if (from.empty()) return fail(std::string("no picture of ") + ai::groupName(want) + " among the game's files");
    // Its middle, 128 a side at most: the big ones would take the anime 6B model long.
    const int w = std::min(found.width, 128), h = std::min(found.height, 128);
    const int x0 = (found.width - w) / 2, y0 = (found.height - h) / 2;
    std::vector<uint32_t> part(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            part[size_t(y) * size_t(w) + size_t(x)] = found.argb[size_t(y0 + y) * size_t(found.width) + size_t(x0 + x)];
    // Every model, each as the network of a group (the pack's own way of loading them).
    AiSettings s = a_.ai;
    const char* const models[3] = {"general", "anime", "anime6b"};
    for (int g = 0; g < ai::kGroups; ++g) s.models[size_t(g)] = models[std::min(g, 2)];
    s.smallPictures = false;  // 1.6.3: each model as it is, also on a small picture
    gpu::Renderer renderer;
    renderer.setShaderCache(a_.dir + "\\uaro-graphics\\shaders-64.bin");
    if (!renderer.create(error) || !renderer.startAiForPack(a_.dir, s, error))
        return fail("the graphics card: " + error);
    CreateDirectoryA(a_.out.c_str(), nullptr);
    const auto save = [&](const char* name, const std::vector<uint32_t>& argb, int sw, int sh) {
        std::vector<uint32_t> shown(argb);  // on a grey ground: the page shows no alpha
        for (uint32_t& p : shown)
            if ((p >> 24) < 128) p = 0xFF34383Eu;
        // 24-bit, bottom-up: the plainest BMP, which LoadImage takes everywhere
        const uint32_t row = uint32_t(sw * 3 + 3) & ~3u, size = 54 + row * uint32_t(sh);
        std::vector<uint8_t> bmp(size, 0);
        const auto put = [&](size_t at, uint32_t v, int n) {
            for (int i = 0; i < n; ++i) bmp[at + size_t(i)] = uint8_t(v >> (8 * i));
        };
        bmp[0] = 'B';
        bmp[1] = 'M';
        put(2, size, 4);
        put(10, 54, 4);
        put(14, 40, 4);
        put(18, uint32_t(sw), 4);
        put(22, uint32_t(sh), 4);
        put(26, 1, 2);
        put(28, 24, 2);
        for (int y = 0; y < sh; ++y)
            for (int x = 0; x < sw; ++x) {
                const uint32_t p = shown[size_t(sh - 1 - y) * size_t(sw) + size_t(x)];
                const size_t at = 54 + size_t(y) * row + size_t(x) * 3;
                bmp[at] = uint8_t(p);
                bmp[at + 1] = uint8_t(p >> 8);
                bmp[at + 2] = uint8_t(p >> 16);
            }
        std::FILE* f = std::fopen((a_.out + "\\" + name).c_str(), "wb");
        if (!f) return false;
        const bool ok = std::fwrite(bmp.data(), 1, bmp.size(), f) == bmp.size();
        std::fclose(f);
        return ok;
    };
    std::vector<uint32_t> big(size_t(w) * 4 * size_t(h) * 4);
    for (int y = 0; y < h * 4; ++y)
        for (int x = 0; x < w * 4; ++x)
            big[size_t(y) * size_t(w * 4) + size_t(x)] = part[size_t(y / 4) * size_t(w) + size_t(x / 4)];
    if (!save("source.bmp", big, w * 4, h * 4)) return fail("the preview cannot be written into " + a_.out);
    for (int m = 0; m < 3; ++m) {
        std::string why;
        if (!renderer.enlargeForPack(m, part.data(), w, h, borderOf(found.group), big, why))
            return fail(std::string(models[m]) + ": " + why);
        save((std::string(models[m]) + ".bmp").c_str(), big, w * 4, h * 4);
    }
    std::FILE* f = std::fopen((a_.out + "\\preview.txt").c_str(), "wb");
    if (f) {
        std::fprintf(f, "%s\t%s\t%d\t%d\n", ai::groupName(want), utf8Of(from).c_str(), found.width, found.height);
        std::fclose(f);
    }
    s_.state = pack::Status::Done;
    status_.write(s_, true);
    LOG_INFO("texture pack: preview of %s from %s (%dx%d)", ai::groupName(want), utf8Of(from).c_str(), w, h);
    return 0;
}

bool Builder::checkpoint(Encoders* enc, bool last) {
    if (enc) enc->lockWriter();
    pack::Index index;  // of the pictures in the pack
    for (const std::string& n : writer_.names()) {
        const auto it = index_.pictures.find(n);
        index.pictures[n] = it == index_.pictures.end() ? pack::IndexEntry() : it->second;
    }
    for (const auto& a : index_.aliases)
        if (index.pictures.count(a.second)) index.aliases.insert(a);
    for (const auto& n : index_.alike)
        if (index.pictures.count(n.first)) index.alike.insert(n);
    std::string error;
    const std::string text = pack::indexText(index);
    const bool ok = last ? writer_.close(text, error) : writer_.checkpoint(text, error);
    s_.inPack = uint32_t(index.pictures.size());
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
        if (old.open(out, error) && old.read(pack::kIndexName, index))
            index_ = pack::parseIndexFile(std::string(index.begin(), index.end()));
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
            for (const pack::Picture& p : picturesOf(files_, list_[i].first, list_[i].second, data, a_.choice)) {
                const std::string name = pack::pictureName(p.group, p.key);
                if (!seen.insert(name).second) continue;
                pack::IndexEntry& entry = index_.pictures[name];
                entry.kind = list_[i].second.kind;
                // 1.6: its group, written for every picture; one another model made is made again when asked
                const ai::Group g = ai::groupOfPicture(list_[i].first, list_[i].second, p.width, p.height);
                entry.group = ai::groupName(g);
                if (a_.redo[size_t(g)] && done.count(name) &&
                    ai::modelOfEntry(entry, g, a_.ai) != a_.ai.modelForPicture(g, p.width, p.height)) {
                    done.erase(name);
                    redo_.insert(name);
                }
                if (p.contentWidth)  // 0.49: a .jpg's - the game finds it by its likeness
                    index_.alike[name] = pack::likenessText(
                        pack::likenessOf(p.argb.data(), p.width, p.height, p.contentWidth, p.contentHeight));
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
    LOG_INFO("texture pack: %u pictures to make of %zu files (%zu made again by another model); %zu keys of the "
             "world's textures at a lower texture detail", s_.toMake, todo.size(), redo_.size(), index_.aliases.size());
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
    std::string nets;  // 1.6: each group by its own model
    for (int g = 0; g < ai::kGroups; ++g)
        nets += std::string(g ? ", " : "") + ai::groupName(ai::Group(g)) + " " + renderer.packNetName(g);
    LOG_INFO("texture pack: %s; %s; WebP quality %d", renderer.adapterName().c_str(), nets.c_str(), a_.quality);
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
        for (pack::Picture& p : picturesOf(files_, list_[i].first, list_[i].second, data, a_.choice)) {
            const std::string name = pack::pictureName(p.group, p.key);
            if (!done.insert(name).second) continue;
            std::string why;
            const ai::Group g = ai::groupOfPicture(list_[i].first, list_[i].second, p.width, p.height);
            if (!renderer.enlargeForPack(int(g), p.argb.data(), p.width, p.height, borderOf(p.group), big, why)) {
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
            m.replace = redo_.count(name) != 0;
            pack::IndexEntry& made = index_.pictures[name];  // 1.6: the model that made it
            made.model = a_.ai.modelForPicture(g, p.width, p.height);  // 1.6.3
            made.group = ai::groupName(g);
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
    if (a.mode == "preview") {  // 1.6: writes no pack - neither mutex; its pictures into uaro-graphics\preview
        a.out = a.dir + "\\uaro-graphics\\preview";
        if (args.status.empty()) a.status = a.out + "-status.txt";
        Builder b(a);
        return b.run();
    }
    // One builder at a time: a second would write the same pack.
    HANDLE one = CreateMutexA(nullptr, FALSE, "Local\\uaro-texture-pack-builder");
    if (one && GetLastError() == ERROR_ALREADY_EXISTS) {
        LOG_ERROR("texture pack: another builder is running");
        CloseHandle(one);
        return 4;
    }
    // 0.48: and no game writes it meanwhile (making pictures while playing, [ai] generate).
    HANDLE writer = CreateMutexA(nullptr, FALSE, pack::kWriterMutex);
    if (!writer || WaitForSingleObject(writer, 0) != WAIT_OBJECT_0) {
        LOG_ERROR("texture pack: a game is writing it (making textures while playing): close the game first");
        pack::Status s;
        s.state = pack::Status::Failed;
        s.message = "a game is writing the pack (making textures while playing): close the game first";
        s.pid = GetCurrentProcessId();
        StatusFile(a.status).write(s, true);
        if (writer) CloseHandle(writer);
        if (one) CloseHandle(one);
        return 4;
    }
    LOG_INFO("texture pack: %s, kinds %s, quality %d", a.mode.c_str(), pack::choiceText(a.choice).c_str(), a.quality);
    Builder b(a);
    const int code = b.run();
    ReleaseMutex(writer);
    CloseHandle(writer);
    if (one) CloseHandle(one);
    return code;
}

}  // namespace uagfx::host

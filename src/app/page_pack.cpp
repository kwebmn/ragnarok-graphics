#include <algorithm>
#include <map>
#include <cstdio>
#include <string>

#include "app/ai_group_texts.hpp"
#include "app/app_disk.hpp"
#include "app/files.hpp"
#include "app/pack_compact.hpp"
#include "app/page.hpp"
#include "app/ui.hpp"
#include "core/config.hpp"
#include "core/map_textures.hpp"
#include "core/pack_build.hpp"
#include "core/settings/log_digest.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kKinds = 1001;  // + the kind's place in kKindBoxes
constexpr int kQuality = 1010;
constexpr int kMake = 1020;
constexpr int kStop = 1021;
constexpr int kAnew = 1023;
constexpr int kCopy = 1024;
constexpr int kBar = 1030;
constexpr int kCompact = 1040;  // 2.5.28 (the site's download, 0.53, gone)
constexpr int kRedoBox = 1050;  // 1.6
constexpr int kMapName = 1060;  // 2.2.5: one map's textures
constexpr int kMapGround = 1061;
constexpr int kMapModels = 1062;
constexpr int kMapModel = 1063;
constexpr int kMapRemake = 1064;
constexpr int kMapDrop = 1065;

struct KindBox {
    Text label, hint;
};
constexpr int kBoxes = 8;
const KindBox kKindBoxes[kBoxes] = {
    {Text::PackWorld, Text::PackWorldHint},     {Text::PackEffects, Text::PackEffectsHint},
    {Text::PackMonsters, Text::PackMonstersHint}, {Text::PackPlayers, Text::PackPlayersHint},
    {Text::PackNpcs, Text::PackNpcsHint},       {Text::PackItems, Text::PackItemsHint},
    {Text::PackInterface, Text::PackInterfaceHint}, {Text::PackBodyColours, Text::PackBodyColoursHint}};
const int kQualities[3] = {85, 92, 100};

// What the page's choices were: it is made again after every change and keeps them across.
pack::Choice g_choice;
int g_quality = 1;
// The builder just started: its status file is not there yet (the old one is deleted before).
DWORD g_launched = 0;
ULONGLONG g_launchedAt = 0;
// 2.2.5: one map's textures - its name as typed (the last map of the game's log at first), what of it, the model
std::wstring g_map;
bool g_mapFromLog = false;
bool g_mapGround = true, g_mapModels = false;
int g_mapModel = 0;
const char* const kModelWords[4] = {"general", "anime", "anime6b", "gameai"};  // 2.5.27: Game AI

// 2.2.5: a run of the builder on one map's textures.
struct MapRun {
    std::string map;
    bool ground = true, models = false;
    std::string model;  // "": the models the settings have (taken out, nothing made)
};

bool* kindOf(pack::Choice& c, int i) {
    bool* const fields[kBoxes] = {&c.world, &c.effects, &c.monsters, &c.players,
                                  &c.npcs,  &c.items,   &c.interface, &c.bodyColours};
    return fields[i];
}

bool readStatus(const std::wstring& path, pack::Status& s) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    std::string text(65536, '\0');
    DWORD got = 0;
    const BOOL ok = ReadFile(f, text.data(), DWORD(text.size()), &got, nullptr);
    CloseHandle(f);
    if (!ok) return false;
    text.resize(got);
    return pack::parseStatus(text, s);
}

// The builder's process is still there - uaro-render64.exe, not another one with its number since.
bool builderAlive(uint32_t pid) {
    if (!pid) return false;
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    if (alive) {
        wchar_t name[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, name, &size)) {
            const std::wstring n(name, size);
            alive = n.size() >= 17 && _wcsicmp(n.c_str() + n.size() - 17, L"uaro-render64.exe") == 0;
        }
    }
    CloseHandle(h);
    return alive;
}

bool working(pack::Status::State s) {
    return s == pack::Status::Starting || s == pack::Status::Reading || s == pack::Status::Building;
}

// The texture pack (0.43): what goes into it, its making in the background (uaro-render64.exe --pack) with its
// progress. 0.48: the keys' check against upscaled.grf is gone with the file - the game's log says what the pack lacks.
class PackPage : public Page {
public:
    explicit PackPage(PageHost& host) : Page(host) {}

    bool ticks() const override { return true; }

    void build(Panel& p) override {
        Model& m = host_.model();
        dir_ = m.paths().dir;
        statusPath_ = dir_ + L"\\uaro-graphics\\pack-status.txt";
        const bool host = GetFileAttributesW(builderExe().c_str()) != INVALID_FILE_ATTRIBUTES;
        look();
        p.heading(t(Text::PagePack));
        p.note(packText(), Tone::Normal);
        // 2.5.28: the room pictures made again or taken out hold, and Compact - no site's pack any more
        compacting_ = compactRunning();
        compactLine_ = nullptr;  // this build's, if any
        const bool developer = host_.mode() == settings::Mode::Developer;  // 2.6: Compact and one map are its
        if (developer && (packUnread_ || compacting_ || compactState().kind != CompactState::Idle)) {
            if (packUnread_) p.note(tf(Text::PackUnread, {sizeText(packUnread_, host_.lang())}));
            p.buttons({{kCompact, t(Text::PackCompact), packUnread_ && !compacting_ && !running_}});
            p.note(t(Text::PackCompactNote));
            compactLine_ = p.note(compactText() + L"\n ");
        }
        // 2.15: making the pack is the developer's (it comes with the full client); a build under way shows
        if (!developer && !running_) {
            p.note(t(Text::PackFromClient));
            return;
        }
        p.gap();
        p.heading(t(Text::PackMakeHeading));
        p.note(t(Text::PackIntro));
        p.note(t(Text::PackWhat), Tone::Normal);
        std::vector<CheckItem> kinds;
        for (int i = 0; i < kBoxes; ++i)
            kinds.push_back({kKinds + i, t(kKindBoxes[i].label), *kindOf(g_choice, i), t(kKindBoxes[i].hint),
                             !running_});
        p.checks(kinds, 3);
        p.choice(kQuality, t(Text::PackQuality),
                 {t(Text::PackQualitySmall), t(Text::PackQualityBalanced), t(Text::PackQualityLossless)}, g_quality,
                 t(Text::PackQualityHint), !running_);
        const AiSettings a = m.pendingConfig().ai;
        // 1.6: each group by its own model; 2.5.28: a section a part - its model, and its groups that differ
        const auto modelName = [](const std::string& token) {
            return token == "anime6b" ? std::string("anime 6B") : token == "gameai" ? std::string("Game AI") : token;
        };
        std::string nets;
        for (int s = 0; s < ai::kSections; ++s) {
            std::map<std::string, int> uses;  // the section's models, how many groups each
            for (int g = 0; g < ai::kGroups; ++g)
                if (ai::sectionOf(ai::Group(g)) == s) ++uses[a.modelFor(ai::Group(g))];
            const std::string most = std::max_element(uses.begin(), uses.end(), [](const auto& x, const auto& y) {
                                         return x.second < y.second;
                                     })->first;
            nets += std::string(s ? "; " : "") + utf8(t(kAiSectionLabels[s])) + " - " + modelName(most);
            std::string apart;  // its groups of another model
            for (int g = 0; g < ai::kGroups; ++g)
                if (ai::sectionOf(ai::Group(g)) == s && a.modelFor(ai::Group(g)) != most)
                    apart += std::string(apart.empty() ? "" : ", ") + utf8(t(kAiGroupLabels[g])) + " - " +
                             modelName(a.modelFor(ai::Group(g)));
            if (!apart.empty()) nets += " (" + apart + ")";
        }
        p.note(tf(Text::PackNetsGroups, {nets}));
        p.check(kRedoBox, t(Text::PackRedo), packRedo(), t(Text::PackRedoHint), !running_);
        const bool chosen = pack::anyChosen(g_choice);
        const bool busy = running_ || compacting_;  // 2.5.28: Compact holds the pack too
        p.buttons({{kMake, t(Text::PackMake), host && chosen && !busy, true}, {kStop, t(Text::PackStop), running_}});
        bar_ = p.progress(kBar);
        // The lines as they are now; while it works, room for two of each (tick() changes their text alone).
        std::wstring line, detail;
        Tone tone = Tone::Normal;
        texts(line, detail, &tone);
        line_ = p.note(running_ ? line + L"\n " : line, tone);
        detail_ = p.note(running_ ? detail + L"\n " : detail);
        const bool result = !running_ && haveStatus_ && !working(status_.state);
        p.buttons({{kAnew, t(Text::PackAnew), host && chosen && !busy}, {kCopy, t(Text::PackCopy), result}});
        if (!host) p.note(t(Text::PackNoHost), Tone::Bad);
        if (!chosen) p.note(t(Text::PackNothing), Tone::Warn);
        if (running_) p.note(t(Text::PackBackground));
        if (running_ && m.now().gameRunning) p.note(t(Text::PackGameRunning), Tone::Warn);
        // 2.2.5: one map's textures - a model tried on one place - 2.6: the Developer mode's
        if (developer) {
            p.gap();
            p.heading(t(Text::PackMapHeading));
            p.note(t(Text::PackMapNote));
            if (!g_mapFromLog) {
                g_mapFromLog = true;
                std::string log;
                if (readFile(m.paths().log, log)) g_map = wide(settings::digestLog(log, 0).map);
            }
            if (HWND name = p.line(kMapName, t(Text::PackMapName), t(Text::PackMapCue), 40, t(Text::PackMapNameHint)))
                SetWindowTextW(name, g_map.c_str());
            p.checks({{kMapGround, t(Text::PackMapGround), g_mapGround, t(Text::PackMapGroundHint), !running_},
                      {kMapModels, t(Text::PackMapModels), g_mapModels, t(Text::PackMapModelsHint), !running_}},
                     2);
            p.choice(kMapModel, t(Text::PackMapModel),
                     {t(Text::AiModelGeneral), t(Text::AiModelAnime), t(Text::AiModelAnime6b), t(Text::AiModelGameAi)},
                     g_mapModel,
                     t(Text::PackMapModelHint), !running_);
            const bool mapOk = host && !busy && (g_mapGround || g_mapModels);
            p.buttons({{kMapRemake, t(Text::PackMapRemake), mapOk},
                       {kMapDrop, t(Text::PackMapDrop), mapOk && packCount_}});
        }
        show();
    }

    void tick() override {
        if (compacting_ != compactRunning()) {
            host_.rebuild();  // its buttons and the pack's numbers
            return;
        }
        if (compacting_ && compactLine_) setText(compactLine_, compactText() + L"\n ");
        const bool was = running_;
        const pack::Status::State before = status_.state;
        look();
        if (was != running_ || before != status_.state) {
            host_.rebuild();  // its buttons and notes go with it
            return;
        }
        show();
    }

    void command(int id, int code) override {
        HWND ctl = GetDlgItem(host_.window(), id);
        if (id >= kKinds && id < kKinds + kBoxes && code == BN_CLICKED) {
            *kindOf(g_choice, id - kKinds) = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED;
            host_.rebuild();  // Make with at least one kind
            return;
        }
        if (id == kRedoBox && code == BN_CLICKED) {  // 1.6
            packRedo() = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED;
            return;
        }
        if (id == kQuality && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < 3) g_quality = int(sel);
            return;
        }
        if (id == kMapName && code == EN_CHANGE) {  // 2.2.5
            wchar_t text[64] = {};
            GetWindowTextW(ctl, text, 64);
            g_map = text;
            return;
        }
        if ((id == kMapGround || id == kMapModels) && code == BN_CLICKED) {
            (id == kMapGround ? g_mapGround : g_mapModels) = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED;
            host_.rebuild();  // the buttons want one of them
            return;
        }
        if (id == kMapModel && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < 4) g_mapModel = int(sel);  // 2.5.27: Game AI the fourth
            return;
        }
        if (code != BN_CLICKED) return;
        if (id == kMapRemake || id == kMapDrop) {
            MapRun run;
            run.map = map_textures::mapName(utf8(g_map));
            if (run.map.empty()) {
                host_.message(t(Text::PackMapBadName), true);
                return;
            }
            run.ground = g_mapGround;
            run.models = g_mapModels;
            const Text models[4] = {Text::AiModelGeneral, Text::AiModelAnime, Text::AiModelAnime6b,
                                    Text::AiModelGameAi};
            if (id == kMapDrop ? !host_.ask(tf(Text::PackMapAskDrop, {run.map}))
                               : !host_.ask(tf(Text::PackMapAskRemake, {run.map, utf8(t(models[g_mapModel]))})))
                return;
            if (id == kMapRemake) run.model = kModelWords[g_mapModel];
            start(id == kMapDrop ? "drop" : "build", pack::parseChoice("world"), false, &run);
            return;
        }
        if (id == kCompact) {  // 2.5.28
            const std::wstring path = ourFile(dir_, layout::File::Pack);
            const uint64_t need = packBytes_ > packUnread_ ? packBytes_ - packUnread_ : 0;
            const Lang l = host_.lang();
            if (!host_.ask(tf(Text::PackCompactAsk, {sizeText(packUnread_, l), sizeText(need, l)}))) return;
            startCompact(path, need + (64u << 20), utf8(t(Text::StudioPackGameOpen)));
            host_.rebuild();
            return;
        }
        if (id == kMake) {
            start("build", g_choice, false);
        } else if (id == kAnew) {
            if (!host_.ask(tf(Text::PackAskAnew, {num((long long)packCount_)}))) return;
            start("build", g_choice, true);
        } else if (id == kStop) {
            const std::wstring stop = statusPath_ + L".stop";
            HANDLE f = CreateFileW(stop.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
            if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
        } else if (id == kCopy) {  // how it ended, for the developers
            std::wstring line, detail;
            texts(line, detail);
            copyToClipboard(host_.window(), detail.empty() ? line : line + L"\r\n" + detail);
        }
    }

private:
    // 2.5.28: Compact's line as it is now.
    std::wstring compactText() const {
        const CompactState c = compactState();
        switch (c.kind) {
            case CompactState::Idle: return L"";
            case CompactState::Working:
                return tf(Text::PackCompacting, {num((long long)c.done), num((long long)c.total)});
            case CompactState::Done: return tf(Text::PackCompacted, {sizeText(c.freed, host_.lang())});
            case CompactState::Failed: return tf(Text::PackCompactFailed, {c.error});
        }
        return L"";
    }
    bool compacting_ = false;
    HWND compactLine_ = nullptr;
    uint64_t packBytes_ = 0, packUnread_ = 0;  // 2.5.28: the pack's size, and of it the old pictures' room

    // The builder: the 64-bit renderer in the uaro-graphics folder, as the driver starts it (context.cpp).
    std::wstring builderExe() const { return dir_ + L"\\uaro-graphics\\uaro-render64.exe"; }

    // The status file and whether its builder is still at work.
    void look() {
        pack::Status s;
        const bool read = readStatus(statusPath_, s);
        if (g_launched && GetTickCount64() - g_launchedAt < 15000 && !(read && s.pid == g_launched)) {
            // Just started: its own status is not there yet.
            status_ = pack::Status();
            status_.state = pack::Status::Starting;
            haveStatus_ = true;
            running_ = builderAlive(g_launched);
            return;
        }
        haveStatus_ = read;
        if (read) status_ = s;
        running_ = read && working(s.state) && builderAlive(s.pid);
    }

    std::wstring packText() {
        if (running_ && haveStatus_ && status_.inPack)
            return tf(Text::PackHas, {num(status_.inPack), sizeText(status_.bytes, host_.lang())});
        const std::wstring path = ourFile(dir_, layout::File::Pack);
        packCount_ = 0;
        packBytes_ = packUnread_ = 0;
        WIN32_FILE_ATTRIBUTE_DATA d{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &d)) return t(Text::PackNone);
        const unsigned long long bytes = (unsigned long long)d.nFileSizeHigh << 32 | d.nFileSizeLow;
        const unsigned long long stamp =
            (unsigned long long)d.ftLastWriteTime.dwHighDateTime << 32 | d.ftLastWriteTime.dwLowDateTime;
        // 2.2.5: counted by its directory alone, once a change of the file - the whole pack opened here at every
        // rebuild, and every picture's header read of one its builder was writing, froze the window (Stop, Make)
        static unsigned long long countedBytes = 0, countedStamp = 0;
        static pack::Counted counted;
        static bool countedOk = false;
        if (bytes != countedBytes || stamp != countedStamp) {
            std::string error;
            countedOk = pack::countOf(ansi(path), counted, error);
            countedBytes = bytes;
            countedStamp = stamp;
        }
        if (!countedOk) return tf(Text::PackWriting, {sizeText(bytes, host_.lang())});
        packCount_ = size_t(counted.pictures);
        packBytes_ = bytes;
        packUnread_ = counted.unread;
        return tf(Text::PackHas, {num((long long)counted.pictures), sizeText(bytes, host_.lang())});
    }

    std::string timeText(int seconds) const {
        const Lang l = host_.lang();
        if (seconds >= 3600)
            return utf8(wtrf(l, Text::TimeHoursMinutes, {num(seconds / 3600), num(seconds / 60 % 60)}));
        if (seconds >= 60) return utf8(wtrf(l, Text::TimeMinutes, {num((seconds + 30) / 60)}));
        return utf8(wtrf(l, Text::TimeSeconds, {num(seconds)}));
    }

    // The progress bar and the two lines under it.
    void show() {
        std::wstring line, detail;
        const int permille = texts(line, detail);
        if (bar_) SendMessageW(bar_, PBM_SETPOS, WPARAM(permille), 0);
        setText(line_, line);
        setText(detail_, detail);
    }

    int texts(std::wstring& line, std::wstring& detail, Tone* tone = nullptr) const {
        int permille = 0;
        Tone how = Tone::Normal;
        const pack::Status& s = status_;
        const auto part = [](uint32_t done, uint32_t total) {
            return total ? int(std::min<unsigned long long>(1000, 1000ull * done / total)) : 0;
        };
        if (!haveStatus_ && !running_) {
            line = g_launched ? t(Text::PackGone) : std::wstring(L" ");  // the one started went before its status
            how = g_launched ? Tone::Warn : Tone::Normal;
        } else if (!running_ && working(s.state)) {
            line = t(Text::PackGone);
            how = Tone::Warn;
        } else {
            switch (s.state) {
                case pack::Status::Starting: line = t(Text::PackStarting); break;
                case pack::Status::Reading:
                    line = tf(Text::PackReading, {num(s.filesDone), num(s.filesTotal)});
                    permille = part(s.filesDone, s.filesTotal);
                    detail = tf(Text::PackFile, {s.current});
                    break;
                case pack::Status::Building: {
                    char rate[32];
                    std::snprintf(rate, sizeof rate, s.rate >= 10 ? "%.0f" : "%.1f", s.rate);
                    line = s.eta >= 0 && s.rate > 0
                               ? tf(Text::PackBuilding, {num(s.made), num(s.toMake), rate, timeText(s.eta)})
                               : tf(Text::PackBuildingNoEta, {num(s.made), num(s.toMake)});
                    permille = part(s.made, s.toMake);
                    detail = tf(Text::PackFile, {s.current});
                    break;
                }
                case pack::Status::Done:
                    permille = 1000;
                    how = Tone::Good;
                    line = s.work == "drop" ? tf(Text::PackDropped, {num(s.made), num(s.inPack)})  // 2.2.5
                                            : tf(Text::PackDone, {num(s.inPack), sizeText(s.bytes, host_.lang())});
                    break;
                case pack::Status::Stopped:
                    line = tf(Text::PackStopped, {num(s.made)});
                    permille = part(s.made, s.toMake);
                    break;
                case pack::Status::Failed:
                    line = tf(Text::PackFailed, {s.message});
                    how = Tone::Bad;
                    break;
            }
        }
        if (tone) *tone = how;
        return permille;
    }

    static void setText(HWND w, const std::wstring& text) {
        if (!w) return;
        const int n = GetWindowTextLengthW(w);
        std::wstring now(size_t(n) + 1, L'\0');
        GetWindowTextW(w, now.data(), n + 1);
        now.resize(size_t(n));
        if (now != text) SetWindowTextW(w, text.c_str());
    }

    void start(const char* mode, const pack::Choice& choice, bool fresh, const MapRun* run = nullptr) {
        AiSettings a = host_.model().pendingConfig().ai;
        if (run && !run->model.empty()) {  // 2.2.5: the map's pictures by the model asked (2.5.28: every world group)
            for (int g = 0; g < ai::kGroups; ++g)
                if (ai::familyOf(ai::Group(g)) == ai::Family::World) a.models[size_t(g)] = run->model;
            a.model = run->model;
        }
        const std::wstring exe = builderExe();
        const std::string kinds = pack::choiceText(choice);
        std::wstring line = L"\"" + exe + L"\" --pack " + wide(mode) + L" --dir \"" + dir_ + L"\" --ai " +
                            wide(aiArgument(a)) + L" --kinds " + wide(kinds.empty() ? "none" : kinds) +
                            L" --quality " + wide(num(kQualities[g_quality])) + L" --fresh " + (fresh ? L"1" : L"0") +
                            L" --log 1";
        if (packRedo()) {  // 1.6: every group - the builder touches only the files of the kinds chosen
            std::string groups;
            for (int g = 0; g < ai::kGroups; ++g) groups += std::string(g ? "," : "") + ai::groupName(ai::Group(g));
            line += L" --redo " + wide(groups);
            packRedo() = false;
        }
        if (run) {  // 2.2.5: one map's textures
            line += L" --map " + wide(run->map) + L" --map-kinds " +
                    (run->ground && run->models ? L"ground,models" : run->ground ? L"ground" : L"models");
            if (!run->model.empty()) line += L" --remake 1";
        }
        const std::string card = host_.model().pendingConfig().gpu;  // 0.50.2: the card chosen on the Renderer page
        line += L" --gpu \"" + wide(card.empty() ? "auto" : card) + L"\"";
        DeleteFileW(statusPath_.c_str());
        DeleteFileW((statusPath_ + L".stop").c_str());
        STARTUPINFOW si{};
        si.cb = sizeof si;
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, FALSE,
                            BELOW_NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW, nullptr, dir_.c_str(), &si, &pi)) {
            host_.message(tf(Text::PackFailed, {"uaro-render64.exe: " + num((long long)GetLastError())}), true);
            return;
        }
        g_launched = pi.dwProcessId;
        g_launchedAt = GetTickCount64();
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        host_.rebuild();
    }

    std::wstring dir_, statusPath_;
    pack::Status status_;
    bool haveStatus_ = false, running_ = false;
    size_t packCount_ = 0;
    HWND bar_ = nullptr, line_ = nullptr, detail_ = nullptr;
};

}  // namespace

bool& packRedo() {
    static bool on = false;
    return on;
}

std::unique_ptr<Page> makePackPage(PageHost& host) { return std::make_unique<PackPage>(host); }

}  // namespace uagfx::app

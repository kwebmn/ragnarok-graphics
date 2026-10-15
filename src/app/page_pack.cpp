#include <algorithm>
#include <cstdio>
#include <string>

#include "app/pack_fetch.hpp"
#include "app/page.hpp"
#include "app/ui.hpp"
#include "core/config.hpp"
#include "core/pack_build.hpp"
#include "core/pack_download.hpp"
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
constexpr int kFetch = 1040;  // 0.53: the pack from the site
constexpr int kFetchStop = 1041;
constexpr int kFetchBar = 1042;

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
        // 0.53: first what a player does - the developers' pack from the site
        p.heading(t(Text::PackFetchHeading));
        p.note(t(Text::PackFetchNote));
        fetching_ = fetchRunning();
        p.buttons({{kFetch, t(Text::PackFetchButton), !fetching_ && !running_},
                   {kFetchStop, t(Text::PackFetchStop), fetching_}});
        fetchBar_ = p.progress(kFetchBar);
        fetchLine_ = p.note(fetchText() + L"\n ");
        showFetch();
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
        const auto modelName = [&](const std::string& token) {
            return utf8(t(token == "anime" ? Text::AiModelAnime : Text::AiModelGeneral));
        };
        p.note(tf(Text::PackNets, {modelName(a.model), modelName(a.spriteModel)}));
        const bool chosen = pack::anyChosen(g_choice);
        p.buttons({{kMake, t(Text::PackMake), host && chosen && !running_}, {kStop, t(Text::PackStop), running_}});
        bar_ = p.progress(kBar);
        // The lines as they are now; while it works, room for two of each (tick() changes their text alone).
        std::wstring line, detail;
        Tone tone = Tone::Normal;
        texts(line, detail, &tone);
        line_ = p.note(running_ ? line + L"\n " : line, tone);
        detail_ = p.note(running_ ? detail + L"\n " : detail);
        const bool result = !running_ && haveStatus_ && !working(status_.state);
        p.buttons({{kAnew, t(Text::PackAnew), host && chosen && !running_}, {kCopy, t(Text::PackCopy), result}});
        if (!host) p.note(t(Text::PackNoHost), Tone::Bad);
        if (!chosen) p.note(t(Text::PackNothing), Tone::Warn);
        if (running_) p.note(t(Text::PackBackground));
        if (running_ && m.now().gameRunning) p.note(t(Text::PackGameRunning), Tone::Warn);
        show();
    }

    void tick() override {
        if (fetching_ != fetchRunning()) {
            host_.rebuild();  // its buttons and the pack's numbers
            return;
        }
        showFetch();
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
        if (id == kQuality && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < 3) g_quality = int(sel);
            return;
        }
        if (code != BN_CLICKED) return;
        if (id == kFetch) {
            startFetch(dir_, pack::kPackUrl);
            host_.rebuild();
            return;
        }
        if (id == kFetchStop) {
            stopFetch();
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
    // 0.53: the download's line and bar as they are now.
    std::wstring fetchText() const {
        const FetchState f = fetchState();
        const auto mb = [&](uint64_t b) { return sizeText(b, host_.lang()); };
        switch (f.kind) {
            case FetchState::Idle: return L"";
            case FetchState::Listing: return t(Text::PackFetchListing);
            case FetchState::Fetching: {
                const double left = f.bytesPerSecond > 0 ? double(f.total - f.done) / f.bytesPerSecond / 60.0 : 0;
                return tf(Text::PackFetchProgress,
                          {num(f.fetched), num(f.pictures), mb(f.done), mb(f.total),
                           mb(uint64_t(f.bytesPerSecond)), num((long long)(left + 0.5))});
            }
            case FetchState::Done:
                return f.pictures ? tf(Text::PackFetchDone, {num(f.fetched), num(f.already)})
                                  : tf(Text::PackFetchUpToDate, {num(f.already)});
            case FetchState::Failed: return tf(Text::PackFetchFailed, {f.error});
            case FetchState::Stopped: return tf(Text::PackFetchStopped, {num(f.fetched)});
        }
        return L"";
    }
    void showFetch() {
        const FetchState f = fetchState();
        if (fetchBar_)
            SendMessageW(fetchBar_, PBM_SETPOS, WPARAM(f.total ? f.done * 1000 / f.total : (f.kind == FetchState::Done
                                                                                               ? 1000 : 0)), 0);
        if (fetchLine_) {
            const std::wstring text = fetchText();
            wchar_t now[512] = {};
            GetWindowTextW(fetchLine_, now, 512);
            if (text + L"\n " != now) SetWindowTextW(fetchLine_, (text + L"\n ").c_str());
        }
    }
    bool fetching_ = false;
    HWND fetchBar_ = nullptr, fetchLine_ = nullptr;

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
        pack::Reader r;
        std::string error;
        const std::string path = ansi(dir_ + L"\\uaro-textures.zip");
        packCount_ = 0;
        if (!r.open(path, error)) return t(Text::PackNone);
        const size_t n = r.count() - (r.has(pack::kIndexName) ? 1 : 0);
        packCount_ = n;
        WIN32_FILE_ATTRIBUTE_DATA d{};
        unsigned long long bytes = 0;
        if (GetFileAttributesExW((dir_ + L"\\uaro-textures.zip").c_str(), GetFileExInfoStandard, &d))
            bytes = (unsigned long long)d.nFileSizeHigh << 32 | d.nFileSizeLow;
        return tf(Text::PackHas, {num((long long)n), sizeText(bytes, host_.lang())});
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
                    line = tf(Text::PackDone, {num(s.inPack), sizeText(s.bytes, host_.lang())});
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

    void start(const char* mode, const pack::Choice& choice, bool fresh) {
        const AiSettings a = host_.model().pendingConfig().ai;
        const std::wstring exe = builderExe();
        const std::string kinds = pack::choiceText(choice);
        std::wstring line = L"\"" + exe + L"\" --pack " + wide(mode) + L" --dir \"" + dir_ + L"\" --ai " +
                            wide(aiArgument(a)) + L" --kinds " + wide(kinds.empty() ? "none" : kinds) +
                            L" --quality " + wide(num(kQualities[g_quality])) + L" --fresh " + (fresh ? L"1" : L"0") +
                            L" --log 1";
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

std::unique_ptr<Page> makePackPage(PageHost& host) { return std::make_unique<PackPage>(host); }

}  // namespace uagfx::app

#include <algorithm>
#include <cstdio>

#include "app/client_patch.hpp"
#include "app/files.hpp"
#include "app/launcher.hpp"
#include "app/page.hpp"
#include "app/setting_ids.hpp"
#include "app/status_info.hpp"
#include "app/update.hpp"
#include "core/graphics_test.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kPlayNow = 2101;
constexpr int kWhatsNew = 2102;
constexpr int kPicked = 2103;      // 2.12: «Як підібрано»
constexpr int kNormalMode = 2104;  // the safe mode off
constexpr int kSwitchOn = 2105;    // uaRO Graphics on again
constexpr int kChannel = 2106;     // 3.0.1-beta.2 (the user): «Реліз» or «Бета» on the first page
constexpr int kUpdateCheck = 2107;  // and «Перевірити оновлення»
constexpr int kUpdateNow = 2108;    // 3.0.1-beta.4 (the user): «Оновити зараз» - updated, then this app, no game
constexpr int kTestNow = 2109;    // 3.0.5-beta.7 (the user: «сделай»): «Запустити тест графіки»
constexpr int kTestList = 2130;   // and the last test's table
constexpr int kNewsFirst = 2110;  // + a news item's number: its "More"
constexpr int kNewsMost = 10;

// 3.0.5-beta.7: when a file was written ("" none) - its stamp, and its local time for the page
std::wstring stampOf(const std::wstring& file) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &a)) return L"";
    return std::to_wstring(a.ftLastWriteTime.dwHighDateTime) + L"." + std::to_wstring(a.ftLastWriteTime.dwLowDateTime) +
           L"." + std::to_wstring(a.nFileSizeLow);
}

std::string localTimeOf(const std::wstring& file) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    FILETIME local{};
    SYSTEMTIME t{};
    if (!GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &a) ||
        !FileTimeToLocalFileTime(&a.ftLastWriteTime, &local) || !FileTimeToSystemTime(&local, &t))
        return "";
    char text[32];
    std::snprintf(text, sizeof text, "%02u.%02u.%04u %02u:%02u", t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute);
    return text;
}

std::string megabytes(uint64_t bytes) {
    char text[32];
    std::snprintf(text, sizeof text, "%.1f", bytes ? std::max(0.1, double(bytes) / (1024.0 * 1024.0)) : 0.0);
    return text;
}

// 2.11: the launcher «Гра», the app's first page (the page QjTrLqpnyyw8qKiUZQ3i24's mockup): the site's news in the
// player's language, the server, the update, and the one big button - «Грати», or «Оновити й грати» when an update is
// offered (the bottom row's «Застосувати й грати» is the same way: PageHost::play).
class PlayPage : public Page {
public:
    explicit PlayPage(PageHost& host) : Page(host), rows_(makeSettingsPage(host, {})) {}

    void build(Panel& p) override {
        const LauncherState s = launcherState();
        const UpdateState u = channelUpdate(host_.model());
        const PatchState patch = clientPatchState();  // 2.14: the game's own new files
        asking_ = s.kind == LauncherState::Asking;
        checking_ = u.kind == UpdateState::Checking;
        patching_ = patch.kind == PatchState::Asking;
        const std::string offer = u.kind == UpdateState::Done ? u.offer : "";
        const bool patchNeeded = patch.kind == PatchState::Done && !patch.check.needed.empty();
        buildCards(p);
        p.heading(t(Text::PlayNews));
        p.buttons({{kWhatsNew, tf(Text::MainWhatsNew, {kVersion})}});
        urls_.clear();
        if (asking_ || s.kind == LauncherState::Idle)
            p.note(t(Text::PlayAsking));
        else if (s.kind == LauncherState::Failed)
            p.note(tf(Text::PlayNoSite, {utf8(t(Text::PlayButton))}), Tone::Warn);
        else if (s.info.news.empty())
            p.note(t(Text::PlayNoNews));
        for (const launcher::News& n : s.info.news) {
            p.note(n.pinned ? tf(Text::PlayPinned, {n.date}) : wide(n.date), n.pinned ? Tone::Accent : Tone::Muted);
            p.note(wide(n.title), Tone::Normal);
            if (!n.excerpt.empty()) p.note(wide(n.excerpt));
            if (!n.url.empty() && urls_.size() < kNewsMost) {
                p.buttons({{kNewsFirst + int(urls_.size()), t(Text::PlayMore)}});
                urls_.push_back(n.url);
            }
            p.gap(8);
        }
        p.gap();
        p.heading(t(Text::PlayServer));
        if (s.kind == LauncherState::Done && s.info.online >= 0)
            p.note(tf(Text::PlayOnline, {std::to_string(s.info.online)}), Tone::Good);
        else if (s.kind == LauncherState::Failed)
            p.note(tf(Text::UpdateFailed, {s.error}), Tone::Muted);  // why, for a report
        else
            p.note(t(Text::PlayAsking), Tone::Muted);
        p.gap();
        p.heading(t(Text::ServiceUpdate));
        buildChannel(p, host_, kChannel);  // 3.0.1-beta.2 (the user): the channel and the check here, in every mode
        if (!offer.empty()) {
            p.note(tf(Text::PlayUpdateLine, {offer, megabytes(u.branch.package.size)}), Tone::Warn);
            p.note(u.back ? tf(Text::UpdateBackNote, {offer, kVersion}) : t(Text::PlayUpdateNote));
        } else if (u.kind == UpdateState::Failed) {
            p.note(tf(Text::UpdateFailed, {u.error}), Tone::Bad);
        } else {
            p.note(t(checking_ ? Text::UpdateChecking : Text::MainUpdateNone));
        }
        std::vector<ButtonItem> updates{{kUpdateCheck, t(Text::UpdateCheck), !checking_}};
        if (!offer.empty()) {
            const std::wstring now = u.back ? tf(Text::UpdateBackButton, {offer}) : t(Text::UpdateNow);
            updates.push_back({kUpdateNow, now, true, true});
        }
        p.buttons(updates);
        if (patchNeeded) {
            p.note(tf(Text::PlayPatchLine, {std::to_string(patch.check.needed.size()), megabytes(patch.check.bytes)}),
                   Tone::Warn);
            p.note(t(Text::PlayPatchNote));
        }
        p.gap();
        Text play = offer.empty() && !patchNeeded ? Text::PlayButton : Text::PlayUpdateButton;
        if (!offer.empty() && u.back) play = Text::PlayBackButton;  // 3.0.5-beta.1: the release back, then the game
        p.buttons({{kPlayNow, t(play), true, true}});  // 3.0.2: blue
        p.note(t(Text::PlayKeysLine));  // 3.0.2-beta.4: the game's keys, all of them on «Головне»
        rows_->buildRow(p, "launcher.direct_start");
        p.gap();
        buildTest(p);
    }

    // 3.0.5-beta.7 (the user: «сделай»): «Тест графіки» - the game started for it alone (its first map runs the test,
    // win/context_test.cpp), and the last test's table under it (uaro-graphics\graphics-test.txt; its .sent: the
    // report's code, or why it did not go)
    void buildTest(Panel& p) {
        Model& m = host_.model();
        p.heading(t(Text::TestTitle));
        p.note(t(Text::PlayTestNote));
        const bool running = m.now().gameRunning;
        p.buttons({{kTestNow, t(Text::PlayTestButton), !running}});
        if (running) p.note(t(Text::PlayTestRunning), Tone::Warn);
        const std::wstring file = testFile();
        testStamp_ = stampOf(file) + L"|" + stampOf(file + L".sent");
        std::string text;
        graphics_test::Table table;
        if (!readFile(file, text) || !graphics_test::parseSummary(text, table)) {
            p.note(t(Text::PlayTestNone));
            return;
        }
        p.gap(4);
        p.note(tf(Text::PlayTestLast, {localTimeOf(file)}), Tone::Normal);
        p.note(wide(table.head));
        static const int kWidths[] = {130, 45, 150, 95, 330};
        std::vector<std::pair<std::wstring, int>> columns;
        for (size_t c = 0; c < table.columns.size() && c < 5; ++c)
            columns.emplace_back(wide(table.columns[c]), kWidths[c]);
        HWND list = p.listView(kTestList, columns, int(table.rows.size()));
        for (size_t r = 0; r < table.rows.size(); ++r) {
            std::vector<std::string> cells = table.rows[r];
            if (cells.size() == 2) cells = {cells[0], "—", "—", "—", cells[1]};  // not measured: the words, wide
            for (size_t c = 0; c < cells.size(); ++c) {
                const std::wstring cell = wide(cells[c]);
                LVITEMW item{};
                item.mask = LVIF_TEXT;
                item.iItem = int(r);
                item.iSubItem = int(c);
                item.pszText = const_cast<wchar_t*>(cell.c_str());
                SendMessageW(list, c == 0 ? LVM_INSERTITEMW : LVM_SETITEMTEXTW, c == 0 ? 0 : WPARAM(r),
                             reinterpret_cast<LPARAM>(&item));
            }
        }
        std::string said;
        if (!readFile(file + L".sent", said)) return;
        while (!said.empty() && (said.back() == '\n' || said.back() == '\r' || said.back() == ' ')) said.pop_back();
        if (said.rfind("R-", 0) == 0)
            p.note(tf(Text::PlayTestSent, {said}), Tone::Good);
        else if (!said.empty())
            p.note(tf(Text::PlayTestNotSent, {said.rfind("error: ", 0) == 0 ? said.substr(7) : said}), Tone::Warn);
    }

    std::wstring testFile() const { return host_.model().paths().dir + L"\\uaro-graphics\\graphics-test.txt"; }

    // 2.12: this start's own setup, the safe mode, uaRO Graphics switched off - each with its one press
    void buildCards(Panel& p) {
        Model& m = host_.model();
        bool any = false;
        if (m.lastAutoSetup()) {
            p.note(tf(Text::AutoPicked, {m.machine().adapter}), Tone::Good);
            p.buttons({{kPicked, t(Text::AutoPickedMore)}});
            any = true;
        }
        if (m.safeModeNoted() || m.now().config.safeMode) {
            p.note(t(m.safeModeNoted() ? Text::SafeModeAuto : Text::SafeModeAsked), Tone::Warn);
            p.buttons({{kNormalMode, t(Text::SafeModeOff)}});
            any = true;
        }
        if (m.now().config.api == Api::System) {
            p.note(t(Text::SwitchedOff), Tone::Warn);
            p.buttons({{kSwitchOn, t(Text::SwitchOn)}});
            any = true;
        }
        if (any) p.gap();
    }

    bool ticks() const override { return true; }  // 3.0.5-beta.7: a test's result may come while the page is open
    void tick() override {
        const bool asking = launcherState().kind == LauncherState::Asking;
        const bool checking = updateState().kind == UpdateState::Checking;
        const bool patching = clientPatchState().kind == PatchState::Asking;
        const std::wstring file = testFile();
        const bool tested = stampOf(file) + L"|" + stampOf(file + L".sent") != testStamp_;
        if (asking != asking_ || checking != checking_ || patching != patching_ || tested)
            host_.rebuild();  // an answer came, or a test's result
    }

    void command(int id, int code) override {
        if (settingOfId(id)) {
            rows_->command(id, code);
            return;
        }
        if (channelCommand(host_, id, code, kChannel)) return;
        if (code != BN_CLICKED) return;
        Model& m = host_.model();
        if (id == kPlayNow) {
            host_.play();
        } else if (id == kTestNow) {
            host_.playTest();
        } else if (id == kUpdateCheck) {
            startUpdateCheck();
            host_.rebuild();
        } else if (id == kUpdateNow) {
            updateAndReopen(host_, false);  // the button says it: no question
        } else if (id == kPicked && m.lastAutoSetup()) {
            host_.message(autoSetupText(m.machine(), *m.lastAutoSetup(), false, host_.lang()), false);
        } else if (id == kNormalMode || id == kSwitchOn) {
            const std::wstring err =
                id == kNormalMode ? m.setSafeMode(false, host_.lang()) : m.setSwitchedOff(false, host_.lang());
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        } else if (id == kWhatsNew) {
            const std::wstring notes = notesText(host_.model().paths().dir, host_.lang());
            if (notes.empty())
                host_.message(tf(Text::NotesNone, {kVersion}), false);
            else
                host_.showNotes(notes);
        } else if (id >= kNewsFirst && id < kNewsFirst + int(urls_.size())) {
            openInShell(wide(urls_[size_t(id - kNewsFirst)]));  // the site's pages only (core/launcher)
        }
    }

    void scroll(int id, int code, int pos) override { rows_->scroll(id, code, pos); }

private:
    std::unique_ptr<Page> rows_;
    std::vector<std::string> urls_;  // the news' "More", in the page's order
    bool asking_ = false, checking_ = false, patching_ = false;
    std::wstring testStamp_;  // the result's and its answer's, when the page was built
};

}  // namespace

std::unique_ptr<Page> makePlayPage(PageHost& host) { return std::make_unique<PlayPage>(host); }

}  // namespace uagfx::app

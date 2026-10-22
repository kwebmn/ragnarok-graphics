#include <cstdio>

#include "app/files.hpp"
#include "app/launcher.hpp"
#include "app/page.hpp"
#include "app/setting_ids.hpp"
#include "app/status_info.hpp"
#include "app/update.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kPlayNow = 2101;
constexpr int kWhatsNew = 2102;
constexpr int kNewsFirst = 2110;  // + a news item's number: its "More"
constexpr int kNewsMost = 10;

std::string megabytes(uint64_t bytes) {
    char text[32];
    std::snprintf(text, sizeof text, "%.1f", double(bytes) / (1024.0 * 1024.0));
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
        const UpdateState u = updateState();
        asking_ = s.kind == LauncherState::Asking;
        checking_ = u.kind == UpdateState::Checking;
        const std::string offer = u.kind == UpdateState::Done ? u.offer : "";
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
        if (!offer.empty()) {
            p.note(tf(Text::PlayUpdateLine, {offer, megabytes(u.site.package.size)}), Tone::Warn);
            p.note(t(Text::PlayUpdateNote));
        } else {
            p.note(t(checking_ ? Text::UpdateChecking : Text::MainUpdateNone));
        }
        p.gap();
        p.buttons({{kPlayNow, t(offer.empty() ? Text::PlayButton : Text::PlayUpdateButton)}});
        rows_->buildRow(p, "launcher.direct_start");
    }

    bool ticks() const override { return asking_ || checking_; }
    void tick() override {
        const bool asking = launcherState().kind == LauncherState::Asking;
        const bool checking = updateState().kind == UpdateState::Checking;
        if (asking != asking_ || checking != checking_) host_.rebuild();  // an answer came
    }

    void command(int id, int code) override {
        if (settingOfId(id)) {
            rows_->command(id, code);
            return;
        }
        if (code != BN_CLICKED) return;
        if (id == kPlayNow) {
            host_.play();
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
    bool asking_ = false, checking_ = false;
};

}  // namespace

std::unique_ptr<Page> makePlayPage(PageHost& host) { return std::make_unique<PlayPage>(host); }

}  // namespace uagfx::app

#include <utility>

#include "app/page.hpp"

namespace uagfx::app {
namespace {

// 2.6: a view of several pages one under another (Service: the service's tools and the diagnostics): each page builds
// its part; a control's command, scroll and notification go to the page that made it - their ids may meet.
class PartsPage : public Page {
public:
    PartsPage(PageHost& host, std::vector<std::unique_ptr<Page>> parts) : Page(host), parts_(std::move(parts)) {}

    void build(Panel& p) override {
        panel_ = &p;
        ranges_.clear();
        for (size_t i = 0; i < parts_.size(); ++i) {
            if (i) p.gap(16);
            const size_t first = p.madeCount();
            parts_[i]->build(p);
            ranges_.push_back({first, p.madeCount()});
        }
    }

    void command(int id, int code) override {
        if (Page* part = owner(GetDlgItem(host_.window(), id))) part->command(id, code);
    }

    void scroll(int id, int code, int pos) override {
        if (Page* part = owner(GetDlgItem(host_.window(), id))) part->scroll(id, code, pos);
    }

    LRESULT notify(const NMHDR* n) override {
        Page* part = owner(n->hwndFrom);
        return part ? part->notify(n) : 0;
    }

    bool ticks() const override {
        for (const auto& part : parts_)
            if (part->ticks()) return true;
        return false;
    }

    void tick() override {
        for (const auto& part : parts_)
            if (part->ticks()) part->tick();
    }

private:
    Page* owner(HWND ctl) const {
        if (!panel_ || !ctl) return nullptr;
        for (size_t i = 0; i < ranges_.size(); ++i)
            for (size_t c = ranges_[i].first; c < ranges_[i].second; ++c)
                if (panel_->madeAt(c) == ctl) return parts_[i].get();
        return nullptr;
    }

    std::vector<std::unique_ptr<Page>> parts_;
    std::vector<std::pair<size_t, size_t>> ranges_;  // each part's controls in the panel's order
    const Panel* panel_ = nullptr;                   // the panel they were built on: it outlives this page
};

}  // namespace

std::unique_ptr<Page> makePartsPage(PageHost& host, std::vector<std::unique_ptr<Page>> parts) {
    return std::make_unique<PartsPage>(host, std::move(parts));
}

}  // namespace uagfx::app

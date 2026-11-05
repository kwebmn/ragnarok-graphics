#include "win/stash_watch.hpp"

#include <map>
#include <unordered_map>

#include "core/hotbar_panel.hpp"
#include "core/layout.hpp"
#include "core/settings/search.hpp"
#include "core/settings/strings.hpp"
#include "win/client_tables.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"

namespace uagfx::win::stashwatch {
namespace {

constexpr uint32_t kOwnAccount = 0x771F7C;

struct State {
    bool loaded = false;
    stashbook::Book book;
    std::unordered_map<int, std::string> folded;  // the game's names, case folded
    unsigned foldedPage = 0;
    std::string lastSearch, lastLine;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string path() { return layout::join(gameDir(), layout::kStash); }

void load(State& s) {
    if (s.loaded) return;
    s.loaded = true;
    std::string text;
    if (readTextFile(path(), text)) s.book = stashbook::Book::parse(text);
}

std::string ownerOf(stashbook::Kind kind, unsigned codePage) {
    if (kind == stashbook::Kind::Storage) {
        uint32_t account = 0;
        return readGame(kOwnAccount, &account, 4) && account ? std::to_string(account) : std::string();
    }
    const std::string name = readOwnName();
    return name.empty() ? std::string() : utf8Of(widened(name, codePage));
}

std::string say(settings::Text t) { return settings::tr(language(), t); }

}  // namespace

void note(stashbook::Kind kind, const std::vector<uagfx::itemwin::Item>& items, unsigned codePage) {
    State& s = state();
    load(s);
    const std::string owner = ownerOf(kind, codePage);
    if (owner.empty()) return;
    std::map<int, int> counts;
    for (const auto& it : items)
        if (it.id > 0) counts[it.id] += std::max(1, it.count);
    if (!s.book.note(stashbook::Place{kind, owner}, counts)) return;
    s.lastSearch.clear();
    if (!writeFileAtomic(path(), s.book.text()) && s.told++ < 5)
        LOG_INFO("stash: %s cannot be written", path().c_str());
}

void prices(std::vector<uagfx::itemwin::Item>& items) {
    const auto live = reference::live();
    const std::string words = say(settings::Text::ItemsMarket);
    for (auto& it : items) {
        it.market.clear();
        if (!live) continue;
        const auto m = live->market.find(it.id);
        if (m == live->market.end() || m->second.cheapest <= 0) continue;
        std::string line = words;
        const std::string price = uagfx::itemwin::grouped(m->second.cheapest), lots = std::to_string(m->second.lots);
        if (const size_t at = line.find("{1}"); at != std::string::npos) line.replace(at, 3, price);
        if (const size_t at = line.find("{2}"); at != std::string::npos) line.replace(at, 3, lots);
        it.market = line;
    }
}

std::string also(const std::string& search, unsigned codePage, const std::vector<stashbook::Kind>& shownHere) {
    State& s = state();
    if (search.empty()) return "";
    load(s);
    const std::string key = search + '#' + std::to_string(shownHere.size()) + '#' + std::to_string(int(language()));
    if (key == s.lastSearch) return s.lastLine;
    if (s.foldedPage != codePage || s.folded.empty()) {
        s.folded.clear();
        s.foldedPage = codePage;
        for (const auto& [id, name] : clienttables::itemNames())
            s.folded[std::atoi(id.c_str())] = settings::foldCase(utf8Of(widened(hotbar::spaced(name), codePage)));
    }
    const std::string words = settings::foldCase(search);
    std::vector<stashbook::Place> here;
    for (const stashbook::Kind k : shownHere) here.push_back(stashbook::Place{k, ownerOf(k, codePage)});
    const auto found = s.book.find(
        [&](int id) {
            const auto n = s.folded.find(id);
            return n != s.folded.end() && n->second.find(words) != std::string::npos;
        },
        here);
    const stashbook::Texts t{say(settings::Text::StashAlso), say(settings::Text::StashStorage),
                             say(settings::Text::StashCart)};
    s.lastSearch = key;
    s.lastLine = stashbook::line(found, t);
    if (!s.lastLine.empty() && s.told++ < 10) LOG_INFO("stash: «%s» lies elsewhere too", search.c_str());
    return s.lastLine;
}

}  // namespace uagfx::win::stashwatch

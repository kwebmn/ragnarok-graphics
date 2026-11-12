#pragma once
#include <cstdint>

namespace uagfx::launcherview {

// 3.0.18-beta.193 (the owner: «первый экран... очень сложный, новый пользователь может испугаться... учти, что это
// игра, а не приложение»; docs/superpowers/specs/2026-11-12-launcher-design.md): the places of the launcher - the
// first screen of uaro-graphics.exe - at a window's client size in pixels and Windows' scale (1 at 96 DPI). Designed
// at 1280 x 720: a top bar, «Що нового» on the left (the update card, the site's news), four icons at the bottom left,
// the look line, the big button and the version at the bottom right; a plate over the icons when one is asked.

enum class Part : uint8_t {
    None, Play, PlayMore, Look, Changelog, News, Settings, Reports, Site, Forum, Version, Language, Plate,
    NoUpdate,  // 3.0.18-beta.199: «Грати без оновлення» under the big button while an update is offered
    Check,     // 3.0.18-beta.202: «Перевірити» beside the big button - the update check
    Channel    // 3.0.18-beta.202: «Реліз» (index 0) or «Бета» (1) beside the version
};

struct Box {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

constexpr int kMostNews = 3;
constexpr int kDockIcons = 4;  // «Налаштування», «Звернення», «Сайт», «Форум»

struct Layout {
    float s = 1;
    int w = 0, h = 0;
    Box bar, logo, server, language;
    Box feed, update;  // feed: its label's line and the cards; update: the first card
    Box news[kMostNews];
    int newsShown = 0;
    Box dock[kDockIcons];
    Box look, play, playMore, version;
    Box plate;     // w 0: none
    Box noUpdate;  // w 0: none (no update offered)
    // 3.0.18-beta.202 (the owner: «кнопку проверить обновления рядом с кнопкой играть», «кнопку выбора канала»): the
    // check a square at the big button's left, the channel's two halves at the version's left in its row
    Box check;
    Box channel[2];
};

// newsCount: the site's news at hand; plate: a plate is shown (the safe mode, uaRO Graphics off, a roll back);
// offered: an update offered - «Грати без оновлення» in sight under the big button (3.0.18-beta.199)
Layout layoutFor(int width, int height, float scale, int newsCount, bool plate, bool offered = false);

struct Hit {
    Part part = Part::None;
    int index = -1;  // a news card's
};
Hit hitAt(const Layout& L, float x, float y);

// 3.0.18-beta.196: the first start (no version seen, no game played here) - «Як ви хочете грати?», the four players'
// cards side by side (presets::Kind's order), «Пропустити — вибере сама програма» under them
constexpr int kKindCards = 4;
struct FirstRun {
    Box title, note, found, cards[kKindCards], skip;  // found: what was chosen for the card at once (.199)
};
FirstRun firstRunFor(const Layout& L);
int firstRunAt(const FirstRun& f, float x, float y);  // a card 0..3, kKindCards «Пропустити», -1 none

// What the big button does now: the game running - another window (never an update over it); an update coming in;
// one offered; else play
enum class PlayState : uint8_t { Update, Updating, Play, Running };
struct PlayInputs {
    bool offered = false, downloading = false, gameRunning = false;
};
PlayState playState(const PlayInputs& in);

}  // namespace uagfx::launcherview

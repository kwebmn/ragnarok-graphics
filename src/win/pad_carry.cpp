#include "win/pad_carry.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "core/window_drag.hpp"
#include "pad/core/config/bindings.hpp"
#include "win/equip_window.hpp"
#include "win/hotbar.hpp"
#include "win/item_window.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/storage_window.hpp"
#include "win/trade_window.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/words.hpp"

namespace uagfx::win::padcarrywin {
namespace {

namespace pc = uagfx::padcarry;

constexpr uint32_t kGold = 0xF3C653, kCard = 0x10141C, kMuted = 0x9AA3B5, kText = 0xEEF2F8;

struct Layer {  // a picture of its own, its texture
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    std::string key;
};

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.39: the carry drawn by uaUI, said again each frame
    int sheetGen = 0;
    bool sheetShown = false;
    std::vector<Words> hintWords;  // the hint's, its own pixels
    int hintW = 0, hintH = 0;
    bool active = false, shown = false;
    pc::Thing thing;
    std::string name;
    const panel::Picture* iconOf = nullptr;
    float fromX = 0, fromY = 0;  // where it was taken
    std::vector<pc::Place> places;
    int focus = -1;
    pc::To focusTo = pc::To::Cell;
    int focusIndex = -1;  // the focused place kept by what it is, the windows moving under it
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    Layer ring, icon, hint;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

const char* placeName(pc::To to) {
    switch (to) {
        case pc::To::Slot: return "the equipment's slot";
        case pc::To::Cell: return "the hotbar's cell";
        case pc::To::Bag: return "the bag";
        case pc::To::Storage: return "the storage";
        case pc::To::Trade: return "the trade";
    }
    return "?";
}

// The places the windows shown now give - the hotbar's cells, «Спорядження»'s slots, the bag, the storage, the trade -
// those that take the thing
std::vector<pc::Place> gather(const State& s) {
    std::vector<pc::Place> all = hotbarwin::cellPlaces();
    for (const pc::Place& p : equipwin::slotPlaces()) all.push_back(p);
    const auto whole = [&](pc::To to, bool (*box)(int&, int&, int&, int&)) {
        int x = 0, y = 0, w = 0, h = 0;
        if (!box(x, y, w, h)) return;
        pc::Place p;
        p.to = to;
        p.x = float(x);
        p.y = float(y);
        p.w = float(w);
        p.h = float(h);
        all.push_back(p);
    };
    whole(pc::To::Bag, itemswin::box);
    whole(pc::To::Storage, storagewin::box);
    whole(pc::To::Trade, tradewin::box);
    return pc::placesFor(s.thing, all);
}

void end(State& s, const char* why) {
    s.active = false;
    s.shown = false;
    windrag::clear();
    if (s.told++ < 40) LOG_INFO("pad carry: %s %s", s.thing.value.c_str(), why);
}

// What the mouse's drop does there, through the windows' own ways
bool put(State& s, const pc::Place& p) {
    switch (p.to) {
        case pc::To::Slot: return equipwin::wearThing(s.thing.index, p.index);
        case pc::To::Cell: return hotbarwin::assign(p.index, s.thing.value, s.thing.level);
        case pc::To::Bag: return storagewin::takeIndex(s.thing.index, s.thing.count);
        case pc::To::Storage: return storagewin::put(s.thing.index, s.thing.count);
        case pc::To::Trade: return tradewin::put(s.thing.index, s.thing.count);
    }
    return false;
}

void paintHint(State& s, bool nowhere) {
    const std::string text = nowhere ? say(settings::Text::PadCarryNowhere) + "  \xC2\xB7  " +
                                           std::string(uapad::buttonLabel(1)) + ": " +
                                           say(settings::Text::PadCarryCancel)
                                     : std::string(uapad::buttonLabel(0)) + ": " + say(settings::Text::PadCarryPut) +
                                           "  \xC2\xB7  " + std::string(uapad::buttonLabel(1)) + ": " +
                                           say(settings::Text::PadCarryCancel);
    const std::string all = s.name.empty() ? text : s.name + "  \xC2\xB7  " + text;
    const float px = 12.5f * s.scale;
    const int w = int(std::ceil(float(wordsWidth(utf8Wide(all), px, true)) + 28 * s.scale));
    const int h = int(std::ceil(28 * s.scale));
    const std::string key = all + '|' + std::to_string(w) + 'x' + std::to_string(h) + (uaui::ready() ? "u" : "");
    if (key == s.hint.key) return;
    s.hint.key = key;
    s.hintW = w, s.hintH = h;
    std::vector<Words> words;
    float x = 14 * s.scale;
    if (!s.name.empty()) {
        const std::string first = s.name + "  \xC2\xB7  ";
        const float fw = float(wordsWidth(utf8Wide(first), px, true));
        words.push_back(Words{panel::Box{x, 0, fw + 2, float(h)}, utf8Wide(first), kGold, px, true, DT_LEFT, false});
        x += fw;
    }
    words.push_back(Words{panel::Box{x, 0, float(w) - x, float(h)}, utf8Wide(text), nowhere ? kMuted : kText, px, true,
                          DT_LEFT, false});
    s.hintWords = std::move(words);  // 3.0.18-beta.39: uaUI's - the words kept (.158: no picture)
}

}  // namespace

bool active() { return state().active; }

void begin(const pc::Thing& t, float x, float y, const panel::Picture* icon, const std::string& name) {
    State& s = state();
    s.active = true;
    s.thing = t;
    s.name = name;
    s.iconOf = nullptr;
    s.icon.key.clear();
    if (icon && icon->width > 0) {
        s.icon.picture = *icon;
        s.iconOf = icon;
        s.icon.upload = true;
    }
    s.fromX = x;
    s.fromY = y;
    s.focus = -1;
    s.focusIndex = -1;
    windrag::Thing lit;  // the places that take it lit in gold, as the mouse's drag lights them
    lit.kind = t.from == pc::From::Skill ? windrag::Kind::Skill
               : t.from == pc::From::Storage ? windrag::Kind::Worn  // the bag lit: it takes it
                                             : windrag::Kind::Item;
    lit.id = t.id;
    lit.location = t.location;
    lit.type = t.type;
    windrag::set(lit);
    if (s.told++ < 40) LOG_INFO("pad carry: %s taken up (the controller's X)", t.value.c_str());
}

void command(Command c) {
    State& s = state();
    if (!s.active) return;
    if (c == Command::Cancel) {
        end(s, "put back (B)");
        return;
    }
    if (s.focus < 0 || s.focus >= int(s.places.size())) return;
    if (c == Command::Put) {
        const pc::Place p = s.places[size_t(s.focus)];
        const bool done = put(s, p);
        end(s, done ? (std::string("put on ") + placeName(p.to) + " " + std::to_string(p.index)).c_str()
                    : "not put - the place's window did not take it");
        return;
    }
    const pc::Dir d = c == Command::Up ? pc::Dir::Up : c == Command::Down ? pc::Dir::Down
                      : c == Command::Left                                ? pc::Dir::Left
                                                                          : pc::Dir::Right;
    s.focus = pc::nextPlace(s.places, s.focus, d);
    s.focusTo = s.places[size_t(s.focus)].to;
    s.focusIndex = s.places[size_t(s.focus)].index;
}

static void frameInner(HWND game, double k, int targetWidth, int targetHeight) {
    State& s = state();
    (void)game;
    if (!s.active || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        s.shown = false;
        return;
    }
    s.k = float(k);
    s.scale = uistyle::readingScale(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.places = gather(s);
    s.focus = -1;
    for (size_t i = 0; i < s.places.size(); ++i)  // the place focused, wherever its window moved
        if (s.places[i].to == s.focusTo && s.places[i].index == s.focusIndex) s.focus = int(i);
    if (s.focus < 0 && !s.places.empty()) {  // the first: the nearest to where it was taken
        s.focus = pc::nearestPlace(s.places, s.fromX, s.fromY);
        s.focusTo = s.places[size_t(s.focus)].to;
        s.focusIndex = s.places[size_t(s.focus)].index;
    }
    paintHint(s, s.focus < 0);
    s.shown = true;
}

// 3.0.18-beta.39 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the controller's carry in
// RmlUi's markup - the gold ring round the place focused, the thing over its top right corner, the hint's pill (under
// the place, or over it with no room) with its words
static void fillSheet(State& s, uisheet::Content& c) {
    c.w = float(s.targetW), c.h = float(s.targetH);
    float hx = s.fromX, hy = s.fromY;
    if (s.focus >= 0 && s.focus < int(s.places.size())) {
        const pc::Place& p = s.places[size_t(s.focus)];
        const float m = std::round(4 * s.scale), t = std::max(2.0f, std::round(2 * s.scale));
        const float rx = std::round(p.x - m), ry = std::round(p.y - m);
        const float w = std::ceil(p.w + 2 * m), h = std::ceil(p.h + 2 * m);
        c.rect(c.under, panel::Box{rx + t / 2, ry + t / 2, w - t, h - t}, 9 * s.scale, kGold, 0.10f, kGold, 0.95f, t);
        if (s.iconOf && s.icon.picture.width > 0)
            c.icons.push_back(uisheet::Icon{std::round(p.x + p.w - float(s.icon.picture.width) * 0.55f),
                                            std::round(p.y - float(s.icon.picture.height) * 0.45f),
                                            float(s.icon.picture.width), float(s.icon.picture.height), 1,
                                            s.sheet.icon(&s.icon.picture)});
        hx = p.x + p.w / 2;
        hy = p.y + p.h + 10 * s.scale;
        if (hy + float(s.hintH) > float(s.targetH) - 4) hy = p.y - 10 * s.scale - float(s.hintH);
    }
    const int w = s.hintW, h = s.hintH;
    const float x = float(std::clamp(int(std::lround(hx - float(w) / 2)), 0, std::max(0, s.targetW - w)));
    const float y = float(std::clamp(int(std::lround(hy)), 0, std::max(0, s.targetH - h)));
    c.rect(c.over, panel::Box{x, y, float(w), float(h)}, float(h) / 2, kCard, 0.94f, kGold, 0.6f,
           std::max(1.0f, s.scale));
    std::vector<Words> words = s.hintWords;
    for (Words& word : words) word.box.x += x, word.box.y += y;
    uisheet::sayGdi(c, words);
}

void frame(HWND game, double k, int targetWidth, int targetHeight) {
    frameInner(game, k, targetWidth, targetHeight);
    State& s = state();
    s.sheet.top = true;  // 3.0.18-beta.69: over the game's own windows, as its picture was
    s.sheetShown = s.sheet.frame("pad carry", s.shown && s.k > 0, true, 0, 0, ++s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    (void)record;  // the old edition (its pictures) gone: uaUI draws the ring and the hint
    (void)renderer;
}

}  // namespace uagfx::win::padcarrywin

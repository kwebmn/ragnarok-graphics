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

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

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

void paintRing(State& s, const pc::Place& p) {
    const float m = std::round(4 * s.scale), t = std::max(2.0f, std::round(2 * s.scale));
    const int w = int(std::ceil(p.w + 2 * m)), h = int(std::ceil(p.h + 2 * m));
    const std::string key = std::to_string(w) + 'x' + std::to_string(h);
    if (key == s.ring.key) return;
    s.ring.key = key;
    s.ring.picture = panel::Picture{};
    s.ring.picture.width = w;
    s.ring.picture.height = h;
    s.ring.picture.px.assign(size_t(w) * size_t(h), 0u);
    panel::roundRect(s.ring.picture, panel::Box{t / 2, t / 2, float(w) - t, float(h) - t}, 9 * s.scale, kGold, 0.10f,
                     kGold, 0.95f, t);
    s.ring.upload = true;
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
    const std::string key = all + '|' + std::to_string(w) + 'x' + std::to_string(h);
    if (key == s.hint.key) return;
    s.hint.key = key;
    s.hint.picture = panel::Picture{};
    s.hint.picture.width = w;
    s.hint.picture.height = h;
    s.hint.picture.px.assign(size_t(w) * size_t(h), 0u);
    panel::roundRect(s.hint.picture, panel::Box{0, 0, float(w), float(h)}, float(h) / 2, kCard, 0.94f, kGold, 0.6f,
                     std::max(1.0f, s.scale));
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
    drawWords(s.hint.picture, words);
    s.hint.upload = true;
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

void frame(HWND game, double k, int targetWidth, int targetHeight) {
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
    if (s.focus >= 0) paintRing(s, s.places[size_t(s.focus)]);
    paintHint(s, s.focus < 0);
    s.shown = true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0) return;
    float hx = s.fromX, hy = s.fromY;  // the hint under the focused place (or where it was taken)
    if (s.focus >= 0 && s.focus < int(s.places.size())) {
        const pc::Place& p = s.places[size_t(s.focus)];
        const float m = std::round(4 * s.scale);
        drawPart(record, renderer, s.ring.picture, s.ring.texture, s.ring.texW, s.ring.texH, s.ring.upload,
                 int(std::lround(p.x - m)), int(std::lround(p.y - m)), s.k);
        if (s.iconOf && s.icon.picture.width > 0) {  // the thing over the place's top right corner
            const int ix = int(std::lround(p.x + p.w - float(s.icon.picture.width) * 0.55f));
            const int iy = int(std::lround(p.y - float(s.icon.picture.height) * 0.45f));
            drawPart(record, renderer, s.icon.picture, s.icon.texture, s.icon.texW, s.icon.texH, s.icon.upload, ix,
                     iy, s.k);
        }
        hx = p.x + p.w / 2;
        hy = p.y + p.h + 10 * s.scale;
        if (hy + float(s.hint.picture.height) > float(s.targetH) - 4)  // no room under it: over it
            hy = p.y - 10 * s.scale - float(s.hint.picture.height);
    }
    const int w = s.hint.picture.width, h = s.hint.picture.height;
    const int x = std::clamp(int(std::lround(hx - float(w) / 2)), 0, std::max(0, s.targetW - w));
    const int y = std::clamp(int(std::lround(hy)), 0, std::max(0, s.targetH - h));
    drawPart(record, renderer, s.hint.picture, s.hint.texture, s.hint.texW, s.hint.texH, s.hint.upload, x, y, s.k);
}

}  // namespace uagfx::win::padcarrywin

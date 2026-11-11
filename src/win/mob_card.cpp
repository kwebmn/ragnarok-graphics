#include "win/mob_card.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/char_status.hpp"
#include "core/mob_battle.hpp"
#include "core/reference_pages.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/runtime.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/mob_hp.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/target_card.hpp"
#include "win/ui_cost.hpp"
#include "win/words.hpp"
#include "win/world_mouse.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"

namespace uagfx::win::mobcardwin {
namespace {

constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kText = 0xEEF2F8, kMuted = 0x9AA3B5, kGold = 0xE6CB8A;
constexpr uint32_t kGood = 0x8FD19E, kBad = 0xF09A8A;
constexpr float kW = 270, kLineH = 18, kPad = 9, kTitleH = 21;
constexpr uint64_t kShowAfter = 200;

struct State {
    uisheet::Sheet sheet;  // 3.0.18-beta.37: the card drawn by uaUI
    int sheetGen = 0;
    bool sheetShown = false;
    std::vector<Words> words;
    float scale = 1;
    bool on = false, shown = false, upload = false;
    uint32_t id = 0;
    uint64_t since = 0;
    float k = 0;
    int x = 0, y = 0, w = 0, h = 0;
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

static void frameInner(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        s.key.clear();
        LOG_INFO("mob card: [interface] mob_card = %d", on ? 1 : 0);
    }
    s.shown = false;
    if (!on || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) {
        s.id = 0;
        return;
    }
    const bool pad = uapad::runtime::look().shown;
    const uint32_t id = pad ? uapad::runtime::target() : pointedActor();
    const uint64_t now = GetTickCount64();
    if (id != s.id) {
        s.id = id;
        s.since = now;
    }
    if (!id || now - s.since < kShowAfter) return;
    const int cls = targetpanel::actorClass(id);
    const mobbattle::Mob* m = cls > 0 ? mobbattle::find(cls) : nullptr;
    if (!m) return;
    const auto index = reference::index();
    const refindex::Mob* ref = nullptr;
    if (index) {
        const auto at = index->mobs.find(cls);
        if (at != index->mobs.end()) ref = &at->second;
    }
    charstatus::Status st;
    if (!readOwnStatus(st)) return;
    const mobbattle::Own own{st.hit, st.flee, st.flee2, st.def, st.def2, st.atk, st.atk2};
    const mobbattle::Fight f = mobbattle::fight(*m, own, ref ? ref->hp : 0);
    const settings::Lang l = language();
    std::vector<std::string> lines;
    lines.push_back((ref ? ref->name : "#" + std::to_string(cls)) + " \xC2\xB7 Lv " + std::to_string(m->level));
    std::string about;
    if (ref) {
        const int pct = mobhpwin::hp(id);
        about = "HP " + refpages::number(ref->hp, l) + (pct >= 0 ? " (" + std::to_string(pct) + " %)" : "") +
                " \xC2\xB7 " + refpages::elementName(ref->element, settings::Lang::En) + " " +
                std::to_string(ref->elementLv) + " \xC2\xB7 " + refpages::raceName(ref->race, settings::Lang::En) +
                " \xC2\xB7 " + refpages::sizeName(ref->size, settings::Lang::En);
    }
    lines.push_back(about);
    const mobbattle::Texts t{say(settings::Text::MobCardHit), say(settings::Text::MobCardIts),
                             say(settings::Text::MobCardBlow), say(settings::Text::MobCardMine)};
    for (const std::string& line : mobbattle::lines(f, t)) lines.push_back(line);
    const float scale = uistyle::readingScale(k);
    s.k = float(k);
    s.w = int(std::lround(kW * scale));
    s.h = int(std::lround((2 * kPad + kTitleH + float(lines.size() - 1) * kLineH) * scale));
    int cx = 0, cy = 0;
    float box[4] = {};
    if (!pad && readGameCursor(cx, cy)) {
        s.x = int(float(cx) * float(k)) + int(22 * scale);
        s.y = int(float(cy) * float(k)) + int(22 * scale);
    } else if (readActorBox(id, box)) {
        s.x = int(box[2] * float(k)) + int(8 * scale);
        s.y = int(box[1] * float(k));
    } else {
        return;
    }
    if (s.x + s.w > targetWidth) s.x = std::max(0, s.x - s.w - int(44 * scale));
    if (s.y + s.h > targetHeight) s.y = std::max(0, targetHeight - s.h);
    std::string key = std::to_string(s.w) + 'x' + std::to_string(s.h) + '#' + std::to_string(uistyle::generation());
    for (const std::string& line : lines) key += '|' + line;
    s.scale = scale;
    if (key != s.key) {
        s.key = key;
        const uicost::Paint cost("the monster's card", s.w, s.h);
        {
            const float sc = scale;
            std::vector<Words> out;
            for (size_t i = 0; i < lines.size(); ++i) {
                const float y = i == 0 ? kPad * sc : (kPad + kTitleH + float(i - 1) * kLineH) * sc;
                const uint32_t colour = i == 0 ? kGold : i == 1 ? kMuted : i == 2 ? kGood : i == 3 || i == 4 ? kBad
                                                                                                            : kText;
                out.push_back(Words{panel::Box{kPad * sc, y, float(s.w) - 2 * kPad * sc, (i ? kLineH : kTitleH) * sc},
                                    utf8Wide(lines[i]), colour, (i == 0 ? 13 : 12) * sc, i == 0, DT_LEFT, false});
            }
            s.words = std::move(out);  // 3.0.18-beta.37: uaUI's card - its words (.158: no picture)
            ++s.sheetGen;
        }
        if (s.told++ < 10) LOG_INFO("mob card: %s - %s", lines[0].c_str(), lines.size() > 2 ? lines[2].c_str() : "");
    }
    s.shown = true;
}

// 3.0.18-beta.37 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 5 - by hand): the monster's card in
// RmlUi's markup - its ground and the lines as the card lays them out
static void fillSheet(State& s, uisheet::Content& c) {
    const float sc = s.scale;
    c.w = float(s.w), c.h = float(s.h);
    c.rect(c.under, panel::Box{0, 0, float(s.w), float(s.h)}, 8 * sc, kCard, uistyle::panel(), kWhite, 0.09f,
           1.0f * sc);
    uisheet::sayGdi(c, s.words);
}

void frame(HWND game, bool on, double k, int targetWidth, int targetHeight) {
    frameInner(game, on, k, targetWidth, targetHeight);
    State& s = state();
    s.sheetShown = s.sheet.frame("mob card", s.shown, true, s.x, s.y, s.sheetGen,
                                 [&](uisheet::Content& c) { fillSheet(s, c); });
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

}  // namespace uagfx::win::mobcardwin

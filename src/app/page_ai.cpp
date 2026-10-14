#include <algorithm>
#include <cstdlib>
#include <string>

#include "app/page.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kEnable = 1001;
constexpr int kPreset = 1002;
constexpr int kKinds = 1010;  // + the box's place in kBoxes
constexpr int kModel = 1020;
constexpr int kSpriteModel = 1021;
constexpr int kDenoise = 1030;  // its number is 1031
constexpr int kScale = 1040;
constexpr int kBudget = 1050;
constexpr int kSizes = 1060;  // from, and to at 1061
constexpr int kAhead = 1070;

// What to improve, three to a row: the map, the sprites by their files (0.16), the interface.
struct KindBox {
    const char* key;
    Text label, hint;
};
const KindBox kBoxes[] = {{"ai.ground", Text::AiGround, Text::AiGroundHint},
                          {"ai.objects", Text::AiObjects, Text::AiObjectsHint},
                          {"ai.effects", Text::AiEffects, Text::AiEffectsHint},
                          {"ai.monsters", Text::AiSprites, Text::AiSpritesHint},
                          {"ai.players", Text::AiPlayers, Text::AiPlayersHint},
                          {"ai.npcs", Text::AiNpcs, Text::AiNpcsHint},
                          {"ai.items", Text::AiItems, Text::AiItemsHint},
                          {"ai.icons", Text::AiIcons, Text::AiIconsHint},
                          {"ai.ui_pieces", Text::AiUiPieces, Text::AiUiPiecesHint}};
constexpr int kKindCount = int(sizeof kBoxes / sizeof kBoxes[0]);

const IniSetting& setting(const char* key) { return *findSetting(key); }

// The [ai] section (0.12): one switch for a newcomer, the set and every setting for the rest. Everything here starts
// with the 64-bit renderer: a note says so once.
class AiPage : public Page {
public:
    explicit AiPage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        const AiSettings a = m.pendingConfig().ai;
        const bool on = a.enabled;
        p.heading(t(Text::PageAi));
        p.check(kEnable, t(Text::AiEnable), on, t(Text::AiEnableHint));
        const AiPreset preset = aiPresetOf(a);
        p.choice(kPreset, t(Text::AiPreset),
                 {t(Text::AiPresetRecommended), t(Text::AiPresetAll), t(Text::AiPresetCustom)}, int(preset),
                 t(Text::AiPresetHint), on);
        p.note(t(Text::AiWhat), Tone::Normal);
        std::vector<CheckItem> kinds;
        for (int k = 0; k < kKindCount; ++k) {
            const bool ticked = m.token(setting(kBoxes[k].key)) == "1";
            // The windows' pieces go with the interface's box.
            const bool open = on && (std::string(kBoxes[k].key) != "ai.ui_pieces" || a.icons);
            kinds.push_back({kKinds + k, t(kBoxes[k].label), ticked, t(kBoxes[k].hint), open});
        }
        p.checks(kinds, 3);
        const bool maps = a.ground || a.objects, sprites = a.sprites() || a.effects || a.icons;
        choiceRow(p, kModel, "ai.model", on && maps);
        choiceRow(p, kSpriteModel, "ai.sprite_model", on && sprites);
        // The grain is the realistic model's: grey when no kind that is on goes through it.
        const bool general = (maps && a.model == "general") || (sprites && a.spriteModel == "general");
        const IniSetting& dn = setting("ai.denoise");
        const std::string denoise = m.token(dn);
        p.slider(kDenoise, t(dn.label), dn.min, dn.max, dn.step, std::atoi(denoise.c_str()), wide(denoise + dn.unit),
                 t(dn.hint), on && general);
        choiceRow(p, kScale, "ai.scale", on);
        p.number(kBudget, t(Text::AiBudget), wide(m.token(setting("ai.budget"))), t(Text::AiBudgetHint), on);
        p.pair(kSizes, t(Text::AiSizes), std::atoi(m.token(setting("ai.min_size")).c_str()),
               std::atoi(m.token(setting("ai.max_size")).c_str()), t(Text::AiSizesHint), on, L"–");
        p.check(kAhead, t(Text::AiAhead), m.token(setting("ai.ahead")) == "1", t(Text::AiAheadHint), on);
        p.note(t(Text::AfterRestart));
        if (!on) p.note(t(Text::AiOffNote));
    }

    void command(int id, int code) override {
        Model& m = host_.model();
        HWND ctl = GetDlgItem(host_.window(), id);
        const bool clicked = code == BN_CLICKED;
        const bool checked = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (id == kEnable && clicked) {
            m.setToken(setting("ai.enabled"), checked ? "1" : "0");
        } else if (id >= kKinds && id < kKinds + kKindCount && clicked) {
            m.setToken(setting(kBoxes[id - kKinds].key), checked ? "1" : "0");
        } else if (id == kAhead && clicked) {
            m.setToken(setting("ai.ahead"), checked ? "1" : "0");
        } else if (id == kPreset && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel == int(AiPreset::Custom) || sel < 0) return;  // Custom is what the rows hold: nothing to set
            applyPreset(AiPreset(sel));
        } else if ((id == kModel || id == kSpriteModel || id == kScale) && code == CBN_SELCHANGE) {
            const char* key = id == kModel ? "ai.model" : id == kSpriteModel ? "ai.sprite_model" : "ai.scale";
            const IniSetting& s = setting(key);
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel < 0 || size_t(sel) >= s.tokens.size()) return;
            m.setToken(s, s.tokens[size_t(sel)]);
        } else if ((id == kBudget || id == kSizes || id == kSizes + 1) && code == EN_KILLFOCUS) {
            const IniSetting& s = setting(id == kBudget ? "ai.budget" : id == kSizes ? "ai.min_size" : "ai.max_size");
            const std::string text = controlText(ctl);
            if (!validToken(s, text) || text == m.token(s)) {
                if (text != m.token(s)) host_.rebuild();  // back to the value it had
                return;
            }
            m.setToken(s, text);
            host_.changed();
            host_.rebuild();  // the set may change with it
            return;
        } else {
            return;
        }
        host_.changed();
        host_.rebuild();  // what is grey, and the set, follow
    }

    void scroll(int id, int code, int pos) override {
        if (id != kDenoise) return;
        const IniSetting& s = setting("ai.denoise");
        int v = (pos + s.step / 2) / s.step * s.step;
        v = std::clamp(v, s.min, s.max);
        if (v != pos) SendMessageW(GetDlgItem(host_.window(), id), TBM_SETPOS, TRUE, v);
        const std::string token = std::to_string(v);
        SetDlgItemTextW(host_.window(), id + 1, wide(token + s.unit).c_str());
        host_.model().setToken(s, token);
        host_.changed();
        if (code == TB_ENDTRACK) host_.rebuild();  // the set follows once it is let go
    }

private:
    void choiceRow(Panel& p, int id, const char* key, bool enabled) {
        const IniSetting& s = setting(key);
        const std::string token = host_.model().token(s);
        std::vector<std::wstring> items;
        int selected = 0;
        for (size_t c = 0; c < s.choices.size(); ++c) {
            items.push_back(t(s.choices[c]));
            if (token == s.tokens[c]) selected = int(c);
        }
        p.choice(id, t(s.label), items, selected, t(s.hint), enabled);
    }

    // Every key of [ai] but enabled as the set has it.
    void applyPreset(AiPreset preset) {
        Model& m = host_.model();
        Config c;
        c.ai = aiPresetSettings(preset, m.pendingConfig().ai.enabled);
        for (const IniSetting& s : iniSettings())
            if (std::string(s.section) == "ai" && std::string(s.key) != "enabled") m.setToken(s, tokenOf(s, c));
    }
};

}  // namespace

std::unique_ptr<Page> makeAiPage(PageHost& host) { return std::make_unique<AiPage>(host); }

}  // namespace uagfx::app

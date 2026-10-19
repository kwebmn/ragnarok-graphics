#include <algorithm>
#include <cstdlib>
#include <string>

#include "app/ai_cache.hpp"
#include "app/files.hpp"
#include "app/page.hpp"
#include "app/preview_window.hpp"
#include "app/update.hpp"
#include "core/ai/pack_models.hpp"
#include "core/pack_build.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kEnable = 1001;
constexpr int kPreset = 1002;
constexpr int kKinds = 1010;  // + the box's place in kBoxes
constexpr int kDenoise = 1030;  // its number is 1031
constexpr int kScale = 1040;
constexpr int kBudget = 1050;
constexpr int kSizes = 1060;  // from, and to at 1061
constexpr int kAhead = 1070;
constexpr int kGenerate = 1080;  // 0.42
constexpr int kModelSet = 1090;    // 1.6: the set of models
constexpr int kGroupModel = 1100;  // 1.6: + the group
constexpr int kRedo = 1120;
constexpr int kPreviewGroup = 1130;
constexpr int kPreviewShow = 1131;
constexpr int kPreviewZoom = 1132;  // 1.6.1
constexpr int kPreviewOpen = 1133;  // 1.6.2: the viewer window
constexpr int kSmall = 1140;  // 1.6.3: small pictures by their own model
constexpr int kSmallSide = 1141;
constexpr int kSmallModel = 1142;
constexpr int kGetModels = 1150;  // 2.5.27: get-models.bat - Game AI's file

const Text kGroupLabels[ai::kGroups] = {Text::AiGroupWorld,   Text::AiGroupMonsters, Text::AiGroupPlayers,
                                        Text::AiGroupNpcs,    Text::AiGroupItems,    Text::AiGroupEffects,
                                        Text::AiGroupIcons,   Text::AiGroupScreens};

std::string groupKey(int g) { return std::string("ai.model_") + ai::groupName(ai::Group(g)); }

// 1.6: the pack's index, read again only when the file changed (the counts by group and model).
struct PackLook {
    std::wstring path;
    uint64_t bytes = 0;
    FILETIME written{};
    bool have = false;
    pack::Index index;
};
PackLook g_pack;

const pack::Index* packIndex(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) {
        g_pack = PackLook{};
        return nullptr;
    }
    const uint64_t bytes = uint64_t(a.nFileSizeHigh) << 32 | a.nFileSizeLow;
    if (!g_pack.have || g_pack.path != path || g_pack.bytes != bytes ||
        CompareFileTime(&g_pack.written, &a.ftLastWriteTime) != 0) {
        g_pack = PackLook{};
        g_pack.path = path;
        g_pack.bytes = bytes;
        g_pack.written = a.ftLastWriteTime;
        g_pack.have = readPackIndex(path, g_pack.index);
    }
    return g_pack.have ? &g_pack.index : nullptr;
}

// 1.6: the preview - uaro-render64.exe --pack preview making uaro-graphics\preview\*.bmp.
struct Preview {
    int group = int(ai::Group::Monsters);
    int zoom = 0;  // 1.6.1: 0 whole, then x2, x4, x8
    HANDLE process = nullptr;
    bool shown = false;  // the pictures there are this run's
    std::string error;
};
Preview g_preview;
std::string g_getModelsError;  // 2.5.27: why get-models.bat did not start

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
                          {"ai.dyes", Text::AiDyes, Text::AiDyesHint},  // 2.5.23
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
        const bool on = a.on();  // 1.5.8: the pack's pictures or the network making them
        p.heading(t(Text::PageAi));
        p.check(kEnable, t(Text::AiEnable), a.enabled, t(Text::AiEnableHint));
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
        // The grain is the realistic model's: grey when no group that is on goes through it.
        bool general = false;
        for (int g = 0; g < ai::kGroups; ++g)
            general = general || (ai::groupOn(a, ai::Group(g)) && a.modelFor(ai::Group(g)) == "general");
        const IniSetting& dn = setting("ai.denoise");
        const std::string denoise = m.token(dn);
        p.slider(kDenoise, t(dn.label), dn.min, dn.max, dn.step, std::atoi(denoise.c_str()), wide(denoise + dn.unit),
                 t(dn.hint), on && general);
        choiceRow(p, kScale, "ai.scale", on);
        p.number(kBudget, t(Text::AiBudget), wide(m.token(setting("ai.budget"))), t(Text::AiBudgetHint), on);
        p.pair(kSizes, t(Text::AiSizes), std::atoi(m.token(setting("ai.min_size")).c_str()),
               std::atoi(m.token(setting("ai.max_size")).c_str()), t(Text::AiSizesHint), on, L"–");
        p.check(kAhead, t(Text::AiAhead), m.token(setting("ai.ahead")) == "1", t(Text::AiAheadHint), on);
        buildGroups(p, a, on);
        p.gap();
        p.note(t(Text::AiPackNote));  // 0.42
        // 1.5.8: a switch of its own - without the pack's pictures it makes them all anew and keeps none.
        p.check(kGenerate, t(Text::AiGenerate), a.generate, t(Text::AiGenerateHint));
        if (m.token(setting("renderer.api")) == "d3d11") p.note(t(Text::AiD3d11Note));  // 0.47
        p.note(t(Text::AfterRestart));
        if (!on) p.note(t(Text::AiOffNote));
        buildPreview(p);
    }

    bool ticks() const override { return g_preview.process != nullptr; }
    void tick() override {
        if (!g_preview.process || WaitForSingleObject(g_preview.process, 0) != WAIT_OBJECT_0) return;
        CloseHandle(g_preview.process);
        g_preview.process = nullptr;
        pack::Status s;
        std::string text;
        g_preview.error.clear();
        if (readFile(previewDir() + L"-status.txt", text) && pack::parseStatus(text, s) &&
            s.state == pack::Status::Failed)
            g_preview.error = s.message.empty() ? "?" : s.message;
        else if (!fileExists(previewDir() + L"\\preview.txt"))
            g_preview.error = "uaro-render64.exe made nothing (its log: uaro-graphics.log)";
        g_preview.shown = g_preview.error.empty();
        host_.rebuild();
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
        } else if (id == kGenerate && clicked) {
            m.setToken(setting("ai.generate"), checked ? "1" : "0");
        } else if (id == kPreset && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel == int(AiPreset::Custom) || sel < 0) return;  // Custom is what the rows hold: nothing to set
            applyPreset(AiPreset(sel));
        } else if (id == kSmall && clicked) {  // 1.6.3
            m.setToken(setting("ai.small_pictures"), checked ? "1" : "0");
        } else if (id == kSmallModel && code == CBN_SELCHANGE) {
            const IniSetting& s = setting("ai.small_model");
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel < 0 || size_t(sel) >= s.tokens.size()) return;
            m.setToken(s, s.tokens[size_t(sel)]);
        } else if (id == kSmallSide && code == EN_KILLFOCUS) {
            const IniSetting& s = setting("ai.small_side");
            const std::string text = controlText(ctl);
            if (!validToken(s, text) || text == m.token(s)) {
                if (text != m.token(s)) host_.rebuild();
                return;
            }
            m.setToken(s, text);
        } else if (id == kModelSet && code == CBN_SELCHANGE) {  // 1.6
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel < 0 || sel >= int(ai::ModelSet::Own)) return;  // Own is what the rows hold
            const std::array<std::string, ai::kGroups> set = ai::modelsOfSet(ai::ModelSet(sel));
            for (int g = 0; g < ai::kGroups; ++g) m.setToken(setting(groupKey(g).c_str()), set[size_t(g)]);
        } else if (id >= kGroupModel && id < kGroupModel + ai::kGroups && code == CBN_SELCHANGE) {
            const IniSetting& s = setting(groupKey(id - kGroupModel).c_str());
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel < 0 || size_t(sel) >= s.tokens.size()) return;
            m.setToken(s, s.tokens[size_t(sel)]);
        } else if (id == kGetModels && clicked) {  // 2.5.27: in its own console; the page looks again on its rebuild
            g_getModelsError = startGetModels(m.paths().dir);
            host_.rebuild();
            return;
        } else if (id == kRedo && clicked) {  // the Texture pack page, its "make again" ticked
            packRedo() = true;
            host_.showPage(settings::Page::Pack);
            return;
        } else if (id == kPreviewGroup && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < ai::kGroups) g_preview.group = int(sel);
            g_preview.shown = false;
            g_preview.error.clear();
            host_.rebuild();
            return;
        } else if (id == kPreviewZoom && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < 4) g_preview.zoom = int(sel);
            host_.rebuild();
            return;
        } else if (id == kPreviewOpen && clicked) {  // 1.6.2
            showPreviewWindow(GetAncestor(host_.window(), GA_ROOT), previewFiles(),
                              t(kGroupLabels[g_preview.group]) + L" - " + t(Text::AiPreviewWindowHint));
            return;
        } else if (id == kPreviewShow && clicked) {
            startPreview();
            host_.rebuild();
            return;
        } else if ((id == kScale) && code == CBN_SELCHANGE) {
            const char* key = "ai.scale";
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
    // 1.6: a model for each group, the set of them, what the pack holds of each and "make again".
    void buildGroups(Panel& p, const AiSettings& a, bool on) {
        p.gap();
        p.heading(t(Text::AiGroupsHeading));
        p.choice(kModelSet, t(Text::AiModelSet),
                 {t(Text::AiModelSetFast), t(Text::AiModelSetBalanced), t(Text::AiModelSetMaximum),
                  t(Text::AiModelSetCustom)},
                 int(ai::modelSetOf(a)), t(Text::AiModelSetHint), on);
        const pack::Index* index = packIndex(host_.model().paths().dir + L"\\uaro-textures.zip");
        const ai::PackCounts counts = index ? ai::countPack(*index, a) : ai::PackCounts{};
        uint32_t other = 0;
        for (int g = 0; g < ai::kGroups; ++g) {
            const bool groupUsed = on && ai::groupOn(a, ai::Group(g));
            choiceRow(p, kGroupModel + g, groupKey(g).c_str(), groupUsed);
            if (!index) continue;
            const ai::GroupCount& c = counts.groups[size_t(g)];
            if (c.other && groupUsed) {
                other += c.other;
                p.note(tf(Text::AiGroupInPackOther, {num(c.pictures), num(c.other)}), Tone::Warn);
            } else {
                p.note(tf(Text::AiGroupInPack, {num(c.pictures)}));
            }
        }
        // 1.6.3: the small pictures' own model, over their group's
        const bool small = a.smallPictures;
        p.check(kSmall, t(Text::AiSmall), small, t(Text::AiSmallHint), on);
        p.number(kSmallSide, t(Text::AiSmallSide), wide(host_.model().token(setting("ai.small_side"))),
                 t(Text::AiSmallHint), on && small);
        choiceRow(p, kSmallModel, "ai.small_model", on && small);
        // 2.5.27: Game AI comes apart from the package - its file downloaded when a group that is on wants it
        bool gameAi = on && small && a.smallModel == "gameai";
        for (int g = 0; g < ai::kGroups; ++g)
            gameAi = gameAi || (on && ai::groupOn(a, ai::Group(g)) && a.modelFor(ai::Group(g)) == "gameai");
        const std::wstring gameAiFile =
            host_.model().paths().dir + L"\\uaro-graphics\\ai\\" + wide(ai::modelFile("gameai")) + L".bin";
        if (gameAi && !fileExists(gameAiFile)) {
            p.note(t(Text::AiGameAiMissing), Tone::Warn);
            p.buttons({{kGetModels, t(Text::AiGetModels)}});
            if (!g_getModelsError.empty()) p.note(tf(Text::AiGetModelsFailed, {g_getModelsError}), Tone::Bad);
        }
        if (!index) p.note(t(Text::AiNoPackYet));
        if (counts.unknown) p.note(tf(Text::AiUnknownSprites, {num(counts.unknown)}));
        if (other) {
            p.buttons({{kRedo, tf(Text::AiRedo, {num(other)})}});
            p.note(t(Text::AiRedoHint));
        }
    }

    // 1.6: a picture of a group through each model, side by side.
    void buildPreview(Panel& p) {
        p.gap();
        p.heading(t(Text::AiPreviewHeading));
        std::vector<std::wstring> groups;
        for (Text label : kGroupLabels) groups.push_back(t(label));
        const bool working = g_preview.process != nullptr;
        p.choice(kPreviewGroup, t(Text::AiPreviewGroup), groups, g_preview.group, t(Text::AiPreviewHint), !working);
        const bool host = fileExists(host_.model().paths().dir + L"\\uaro-graphics\\uaro-render64.exe");
        p.buttons({{kPreviewShow, t(Text::AiPreviewShow), host && !working}});
        if (working) {
            p.note(t(Text::AiPreviewWorking), Tone::Warn);
        } else if (!g_preview.error.empty()) {
            p.note(tf(Text::AiPreviewFailed, {g_preview.error}), Tone::Bad);
        } else if (g_preview.shown) {
            std::string text;
            if (readFile(previewDir() + L"\\preview.txt", text)) {
                // "group\tfile\twidth\theight"
                std::vector<std::string> f;
                size_t from = 0;
                for (size_t tab; (tab = text.find_first_of("\t\r\n", from)) != std::string::npos; from = tab + 1)
                    f.push_back(text.substr(from, tab - from));
                if (f.size() >= 4) p.note(tf(Text::AiPreviewFrom, {f[1], f[2], f[3]}));
            }
            p.choice(kPreviewZoom, t(Text::AiPreviewZoom),
                     {t(Text::AiPreviewWhole), L"\u00D72", L"\u00D74", L"\u00D78"}, g_preview.zoom,
                     t(Text::AiPreviewZoomHint));
            p.buttons({{kPreviewOpen, t(Text::AiPreviewOpen)}});
            const int zoom = g_preview.zoom == 0 ? 1 : 1 << g_preview.zoom;  // 1, 2, 4, 8
            p.images(previewFiles(), zoom, zoom > 1 ? 2 : 0);
        }
    }

    std::vector<std::pair<std::wstring, std::wstring>> previewFiles() const {
        const std::wstring d = previewDir() + L"\\";
        std::vector<std::pair<std::wstring, std::wstring>> files = {{d + L"source.bmp", t(Text::AiPreviewGame)},
                                                                    {d + L"general.bmp", L"general"},
                                                                    {d + L"anime.bmp", L"anime"},
                                                                    {d + L"anime6b.bmp", L"anime 6B"}};
        if (fileExists(d + L"gameai.bmp")) files.push_back({d + L"gameai.bmp", L"Game AI"});  // 2.5.27
        return files;
    }

    std::wstring previewDir() const { return host_.model().paths().dir + L"\\uaro-graphics\\preview"; }

    void startPreview() {
        const std::wstring dir = host_.model().paths().dir;
        const std::wstring exe = dir + L"\\uaro-graphics\\uaro-render64.exe";
        const std::string card = host_.model().pendingConfig().gpu;
        std::wstring line = L"\"" + exe + L"\" --pack preview --dir \"" + dir + L"\" --ai " +
                            wide(aiArgument(host_.model().pendingConfig().ai)) + L" --redo " +
                            wide(ai::groupName(ai::Group(g_preview.group))) + L" --log 1 --gpu \"" +
                            wide(card.empty() ? "auto" : card) + L"\"";
        DeleteFileW((previewDir() + L"-status.txt").c_str());
        DeleteFileW((previewDir() + L"\\preview.txt").c_str());
        DeleteFileW((previewDir() + L"\\gameai.bmp").c_str());  // 2.5.27: made again only with its model there
        STARTUPINFOW si{};
        si.cb = sizeof si;
        PROCESS_INFORMATION pi{};
        g_preview.error.clear();
        g_preview.shown = false;
        if (!CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, dir.c_str(),
                            &si, &pi)) {
            g_preview.error = "uaro-render64.exe: " + std::to_string(GetLastError());
            return;
        }
        CloseHandle(pi.hThread);
        g_preview.process = pi.hProcess;
    }

    static std::string num(uint32_t n) {  // 12 345
        std::string s = std::to_string(n);
        for (int i = int(s.size()) - 3; i > 0; i -= 3) s.insert(size_t(i), " ");
        return s;
    }

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
            if (std::string(s.section) == "ai" && std::string(s.key) != "enabled" && std::string(s.key) != "generate" &&
                std::string(s.key).rfind("model_", 0) != 0 && std::string(s.key).rfind("small_", 0) != 0)
                m.setToken(s, tokenOf(s, c));  // 1.5.8: both switches as the player set them; 1.6: the groups' models
    }
};

}  // namespace

std::unique_ptr<Page> makeAiPage(PageHost& host) { return std::make_unique<AiPage>(host); }

}  // namespace uagfx::app

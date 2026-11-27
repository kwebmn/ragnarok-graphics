#include "core/settings/catalogue.hpp"

#include <cstring>
#include <deque>
#include <map>
#include <utility>

#include "core/effect_groups.hpp"
#include "core/effect_where.hpp"
#include "core/live_config.hpp"
#include "core/outdoors.hpp"
#include "core/settings/effects_layout.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::settings::catalogue {
namespace {

// A row as the lists below say it: B - basic, D - a detail (`of`: the row it is a part of), X - the technique,
// trial - with the version it came with, diag - a diagnostic; need - another row on; next - from the next map;
// start - read before the game starts
struct Said : Row {
    Said need(const char* key) && {
        needs.push_back(Need{NeedKind::Row, key});
        return std::move(*this);
    }
    Said next() && {
        nextMap = true;
        return std::move(*this);
    }
    Said start() && {
        atStart = true;
        return std::move(*this);
    }
};

Said said(const std::string& key, Level level, Status status, const char* since, const std::string& of) {
    Said s;
    s.key = key;
    s.level = level;
    s.status = status;
    s.since = since;
    s.under = of;
    return s;
}
Said B(const std::string& key) { return said(key, Level::Basic, Status::Permanent, "", ""); }
Said D(const std::string& key, const std::string& of = "") {
    return said(key, Level::Detail, Status::Permanent, "", of);
}
Said X(const std::string& key, const std::string& of = "") {
    return said(key, Level::Expert, Status::Permanent, "", of);
}
Said trial(const std::string& key, const char* since) { return said(key, Level::Detail, Status::Trial, since, ""); }
Said diag(const std::string& key) { return said(key, Level::Expert, Status::Diagnostic, "", ""); }

Group group(Area area, Text name, std::vector<Said> rows) {
    Group g{area, name, {}};
    for (Said& s : rows) g.rows.push_back(std::move(s));
    return g;
}

// «Ефекти»: the groups as the effects' own (core/effect_groups) - a group's switch and strength, the rows said for it
// by hand (the clock with the day and the night), then each effect: its box, its parts and its sliders under it
// (core/settings/effects_layout). The boxes below are details - a group shows seven basic rows at most.
const char* const kDetailBoxes[] = {"no_server_night", "morning_fog",  "aura_portals", "footprints",  "round_light",
                                    "hit_flash",       "death_flash",  "body_tints",   "body_stamp",  "map_glows",
                                    "glint",           "ground_seams", "tree_tints",   "dew"};
struct EffectNeed {
    const char* key;
    const char* needs;
};
const EffectNeed kEffectNeeds[] = {
    {"effects.footprints", "effects.actor_shadows"}, {"effects.skill_sparks", "effects.fire_light"},
    {"effects.flowers", "effects.grass"},            {"effects.dew", "effects.grass"},
    {"effects.swamp", "effects.water"},              {"effects.world_reflections", "effects.water"},
    {"effects.ripples", "effects.water"},            {"effects.rain_wet", "effects.weather"},
    {"effects.wet_ground", "effects.weather"},       {"effects.night_windows", "effects.materials"},
    {"effects.lit_dust", "effects.motes"},           {"effects.lit_dust", "effects.indoor_dust"},
    // 3.0.21-beta.12: the 3D view's icons stand on the character block
    {"interface.view3d_buttons", "interface.character"}};

// A key as a pointer that stays (a Need holds one): the strings made while the groups are built are kept here
const char* keep(const std::string& key) {
    static std::deque<std::string> kept;
    for (const std::string& k : kept)
        if (k == key) return k.c_str();
    kept.push_back(key);
    return kept.back().c_str();
}

bool detailBox(const char* key) {
    for (const char* k : kDetailBoxes)
        if (std::strcmp(k, key) == 0) return true;
    return false;
}

void effectGroups(std::vector<Group>& out) {
    out.push_back(group(Area::Picture, Text::FxPreset, {B("effects.preset"), B("effects.tier")}));
    const EffectLayout* fx = effectLayout();
    for (int g = 0; g < kEffectGroups; ++g) {
        const std::string word = effect_groups::groupWord(g);
        const std::string switchKey = "effects.group_" + word;
        const IniSetting* named = findSetting(switchKey);
        if (!named) continue;
        Group grp{Area::Picture, named->label, {}};
        grp.rows.push_back(B(switchKey));
        // 3.0.21-beta.4: a group off draws none of its effects (effect_groups::effective) - its strength and its
        // effects need it on; an effect's parts and sliders need the effect on
        if (effect_groups::hasStrength(g))
            grp.rows.push_back(D(switchKey + "_strength", switchKey).need(keep(switchKey)));
        if (word == "time") {
            grp.rows.push_back(B("effects.clock"));
            grp.rows.push_back(D("effects.night_strength", "effects.clock"));
            grp.rows.push_back(D("effects.timelapse_speed", "effects.clock"));  // 3.0.22-beta.24: Ctrl+Shift+T
            grp.rows.push_back(B("effects.season"));
        }
        if (word == "weather") grp.rows.push_back(B("effects.weather_kind"));
        if (word == "skill_effects") grp.rows.push_back(B("effects.skill_looks"));
        for (int i = 0; i < effect_groups::memberCount(); ++i) {
            const effect_groups::Member& m = effect_groups::members()[i];
            if (m.group != g) continue;
            const std::string box = std::string("effects.") + m.key;
            if (!findSetting(box)) continue;
            // a hidden one (behind «Вигляд умінь») is the file's alone: the technique
            const bool hidden = hiddenEffect(box.c_str());
            grp.rows.push_back((hidden ? X(box) : detailBox(m.key) ? D(box) : B(box)).need(keep(switchKey)));
            for (int e = 0; e < effectLayoutCount(); ++e) {
                if (box != fx[e].box) continue;
                for (int k = 0; k < kLayoutChecks && fx[e].checks[k]; ++k)
                    if (findSetting(fx[e].checks[k]))
                        grp.rows.push_back(D(fx[e].checks[k], box).need(keep(switchKey)).need(keep(box)));
                for (int k = 0; k < 5 && fx[e].sliders[k]; ++k) {
                    if (!findSetting(fx[e].sliders[k])) continue;
                    Said s = D(fx[e].sliders[k], box).need(keep(switchKey)).need(keep(box));
                    if (fx[e].needs[k]) s.needs.push_back(Need{NeedKind::Row, fx[e].needs[k]});
                    grp.rows.push_back(std::move(s));
                }
            }
        }
        out.push_back(std::move(grp));
    }
}

// «Нейромережа»'s models by group: every [ai] model_* row, in the schema's order
std::vector<Said> aiModels() {
    std::vector<Said> rows{X("ai.model"), X("ai.sprite_model"), X("ai.small_pictures"),
                           X("ai.small_side", "ai.small_pictures"), X("ai.small_model", "ai.small_pictures")};
    for (const IniSetting& s : iniSettings())
        if (std::strcmp(s.section, "ai") == 0 && std::strncmp(s.key, "model_", 6) == 0)
            rows.push_back(X(sectionKey(s)));
    return rows;
}

// the crowns' trials need «Моделі нашим рендером» and «Листя на відеокарті»
const char* const kCrowns = "view3d.own_models";

std::vector<Group> build() {
    std::vector<Group> out;
    effectGroups(out);
    // --- «3D-режим»
    out.push_back(group(Area::View3d, Text::CatView3dCamera,
                        {B("view3d.fov_first"), B("view3d.fov_behind"), B("view3d.mouse"), B("view3d.distance"),
                         D("view3d.shoulder"), D("view3d.face"), D("view3d.aim_pull"), D("view3d.threat"),
                         D("view3d.camera_walls"), D("view3d.near_fade"), B("view3d.free_speed"),
                         B("view3d.free_smooth")}));  // 3.0.22-beta.24: the cinema camera
    out.push_back(group(Area::View3d, Text::CatView3dRooms,
                        {B("view3d.ceiling"), D("view3d.one_room", "view3d.ceiling").need("view3d.ceiling"),
                         B("view3d.vault"), D("view3d.vault_rock", "view3d.vault").need("view3d.vault"),
                         D("view3d.cave_rift", "view3d.vault").need("view3d.vault"),  // 3.0.24-beta.11
                         D("view3d.cave_crystals", "view3d.vault").need("view3d.vault"),
                         D("view3d.cave_grotto", "view3d.vault").need("view3d.vault"),
                         D("view3d.room_walls").next(),  // 3.0.23-beta.6: «Своя земля» no longer needed
                         D("view3d.close_gaps").need("view3d.own_models").next()}));
    out.push_back(group(Area::View3d, Text::CatView3dSky,
                        {B("view3d.panorama"), B("view3d.ranges"), B("view3d.weather"), B("view3d.stars"),
                         B("view3d.moon"), B("view3d.dawn")}));
    out.push_back(group(Area::View3d, Text::FxGroupPlants,
                        {B("view3d.grass"), trial("view3d.shadows", "3.0.20-beta.50").need("effects.own_ground"),
                         trial("view3d.dense_crowns", "3.0.20-beta.51").need(kCrowns).need("effects.leaves_card"),
                         trial("view3d.crown_volume", "3.0.20-beta.73").need(kCrowns).need("effects.leaves_card"),
                         trial("view3d.crown_light", "3.0.20-beta.79").need(kCrowns).need("effects.leaves_card")}));
    // --- «Інтерфейс»
    // 3.0.24-beta.19 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md): the theme first, the groups
    // and the accent in the full mode
    out.push_back(group(Area::Interface, Text::InterfaceSecTheme,
                        {B("interface.theme"), D("interface.accent"), D("interface.theme_hud"),
                         D("interface.theme_chat"), D("interface.theme_items"), D("interface.theme_trade"),
                         D("interface.theme_windows"), D("interface.theme_settings")}));
    out.push_back(group(Area::Interface, Text::InterfaceSecMain,
                        {B("interface.minimap"), B("interface.size"), B("interface.text_size"),
                         D("interface.item_windows"), D("interface.panel_opacity"),
                         D("interface.window_opacity"), D("interface.animations"), D("interface.uaui_motion"),
                         D("interface.uaui_effects"), D("interface.cursor"),
                         D("interface.layout_per_char"), D("interface.pack_icons"), D("interface.low_hp_edge"),
                         D("interface.element_colours"), B("interface.reference"), D("interface.settings_full")}));
    out.push_back(group(Area::Interface, Text::InterfaceSecPanels,
                        {B("interface.character"), D("interface.view3d_buttons", "interface.character"),
                         B("interface.target"), B("interface.party"), B("interface.buffs"),
                         D("interface.buffs_overflow", "interface.buffs"), D("buffs.timers", "interface.buffs"),
                         B("interface.tracker"), B("interface.cast"), B("interface.exp_bars"),
                         D("interface.exp_gain"), D("interface.item_toast"), D("interface.hunt_panel"),
                         D("interface.mvp_timers"), D("interface.broken_gear"), D("interface.pet_hunger")}));
    out.push_back(group(Area::Interface, Text::InterfaceHotbar,
                        {B("interface.hotbar"), D("interface.hotbar_per_character"), D("interface.hotbar_ammo"),
                         D("interface.ammo_sound", "interface.hotbar_ammo"), D("interface.hotbar_supplies"),
                         D("interface.skill_needs"), D("interface.combo_window"), D("interface.delay_ring"),
                         D("interface.ground_counts")}));
    out.push_back(group(Area::Interface, Text::InterfaceChat,
                        {B("interface.chat"), B("interface.chat_fade"), D("interface.long_chat"),
                         D("interface.chat_log"), D("interface.skill_jokes"), D("interface.swear_filter"),
                         D("interface.chat_party"), D("interface.chat_guild"), D("interface.chat_own"),
                         D("interface.chat_others")}));
    out.push_back(group(Area::Interface, Text::InterfaceSecMinimap,
                        {B("interface.minimap_style"), B("interface.minimap_npcs"), B("interface.minimap_warps"),
                         B("interface.minimap_kafra"), B("interface.minimap_party"), B("interface.minimap_mobs"),
                         B("interface.minimap_way"), D("interface.minimap_hd"), D("interface.minimap_classic"),
                         D("interface.minimap_view"), D("interface.net_badge")}));
    out.push_back(group(Area::Interface, Text::InterfaceSecFight,
                        {B("interface.mob_hp"), D("interface.mob_hp_percent", "interface.mob_hp"),
                         D("interface.far_bars", "interface.mob_hp"), B("interface.boss_bars"),
                         B("interface.who_hits"), B("interface.target_ring"), B("interface.mob_card"),
                         B("interface.death_recap"), B("interface.screen_shake"), D("interface.quick_teleport"),
                         D("interface.esc_cutin"), D("interface.follow"), D("interface.skill_cells"),
                         D("interface.mouse_cell"), D("interface.ground_timers"), D("interface.potion_warn"),
                         D("interface.party_alarm")}));
    out.push_back(group(Area::Interface, Text::InterfaceSecGround,
                        {B("interface.drop_names"), B("interface.drop_highlight"), B("interface.signs")}));
    out.push_back(group(Area::Interface, Text::InterfaceSecLogin,
                        {B("interface.login"), D("interface.login_status"), D("interface.login_news"),
                         D("interface.login_view"), B("interface.char_select"), B("interface.char_make"),
                         D("interface.portraits"), D("interface.char_carousel")}));
    out.push_back(group(Area::Interface, Text::InterfaceSecHomun,
                        {B("interface.homun_ai"), D("interface.homun_aggressive", "interface.homun_ai"),
                         D("interface.homun_defend", "interface.homun_ai"),
                         D("interface.homun_skills", "interface.homun_ai")}));
    // each window's own switch a detail: the group shows behind a button
    out.push_back(group(Area::Interface, Text::InterfaceSecWindows,
                        {D("interface.items"),
                         trial("interface.items_packets", "3.0.24-beta.20").need("interface.items"),  // part 2
                         D("interface.status"), D("interface.skills"), D("interface.equip"),
                         D("interface.npc_talk"), D("interface.npc_remember", "interface.npc_talk"),
                         D("interface.shop"),
                         trial("interface.shop_packets", "3.0.24-beta.9").need("interface.shop"),
                         D("interface.storage"),
                         trial("interface.storage_packets", "3.0.24-beta.4").need("interface.storage"),
                         D("interface.trade"), D("interface.cart"),
                         trial("interface.cart_packets", "3.0.24-beta.8").need("interface.cart"),
                         D("interface.vending"),
                         trial("interface.vend_packets", "3.0.24-beta.18").need("interface.vending"),  // stage 4
                         D("interface.my_shop"), D("interface.sale_chime", "interface.my_shop"),  // .23
                         D("interface.vend_setup"),
                         trial("interface.vend_open_packets", "3.0.24-beta.23").need("interface.vend_setup"),
                         D("interface.menu"), D("interface.cards"), D("interface.asks"),
                         D("interface.messages"), D("interface.counts"), D("interface.picks"),
                         D("interface.party_setup"), D("interface.pet"), D("interface.homun"), D("interface.merc"),
                         D("interface.social"), D("interface.guild"), D("interface.servers"),
                         D("interface.whispers"), D("interface.whisper_setup"), D("interface.macros"),
                         D("interface.tips"), D("interface.loading_bar"), D("interface.emotes"),
                         D("interface.commands"), D("interface.chat_rooms"), D("interface.popups")}));
    out.push_back(group(Area::Interface, Text::PanelHeading,
                        {B("panel.show"), D("panel.layout", "panel.show"), D("panel.size", "panel.show"),
                         D("panel.opacity", "panel.show")}));
    // --- «Керування»
    out.push_back(group(Area::Controls, Text::InterfaceSecKeys,
                        {B("interface.control_hints"), B("interface.arrows_walk"), B("interface.walk_mode"),
                         B("interface.space_action"),
                         D("interface.key_hunt"), D("interface.key_mvp"), D("interface.key_tracker"),
                         D("interface.key_party"), D("interface.key_recap"), D("interface.detail_console")}));
    out.push_back(group(Area::Controls, Text::CameraHeading,
                        {B("camera.indoor"), B("camera.zoom_limit"), B("camera.remember"), B("camera.tilt")}));
    out.push_back(group(Area::Controls, Text::PagePad,
                        {B("pad.enabled"), B("interface.pad_hints"), B("interface.pad_tiles"),
                         B("interface.pad_walk"), D("interface.walk_arrow", "interface.pad_walk"),
                         D("interface.walk_mark", "interface.pad_walk")}));
    out.push_back(group(Area::Controls, Text::RoextHeading,
                        {B("roext.window_lock"), B("roext.window_on_top"), B("roext.mouse_buttons"),
                         D("roext.alt_f4"), D("roext.minimize_key"), D("roext.free_cpu"), X("roext.code_page")}));
    // --- «Звук і сповіщення»
    out.push_back(group(Area::Sound, Text::PageSound,
                        {B("sound.output"), B("sound.ambience"), D("sound.ambience_volume", "sound.ambience"),
                         B("sound.engine"), D("sound.rate"), D("sound.voices"), D("sound.background"),
                         D("sound.limiter"), D("sound.stereo"), D("sound.stereo_flip", "sound.stereo"),
                         D("sound.steal"), D("sound.crossfade"), D("sound.reverb"), D("sound.headphones"),
                         D("sound.duck")}));
    out.push_back(group(Area::Sound, Text::NotifyHeading,
                        {B("notify.on"), B("notify.whisper"), B("notify.party"), D("notify.trade"),
                         D("notify.guild"), D("notify.friend"), B("notify.death"), B("notify.attack"),
                         D("notify.recovered"), D("notify.recovered_percent", "notify.recovered"),
                         B("notify.buff_ending"), D("notify.buff_seconds", "notify.buff_ending"),
                         D("notify.party_death"), D("notify.party_low_hp"),
                         D("notify.party_hp", "notify.party_low_hp"), D("notify.party_chat"),
                         D("notify.guild_chat"), D("notify.mention"), D("notify.disconnect"), B("notify.sound"),
                         D("notify.report_answer")}));
    // --- «Швидкість і екран»
    out.push_back(group(Area::Speed, Text::PageQuality,
                        {B("quality.super_light"), B("quality.render_scale"), D("quality.world_scale"),
                         // «Змінене в грі діє на текстури, завантажені після зміни»
                         B("quality.sharpen"), X("quality.full_colour").next()}));
    out.push_back(group(Area::Speed, Text::PageSmoothing,
                        {B("quality.msaa"), B("quality.taa"), D("quality.alpha_to_coverage").need("quality.msaa"),
                         D("quality.anisotropy"), B("quality.dlss"),
                         D("quality.dlss_model", "quality.dlss").need("quality.dlss"),
                         D("quality.dlss_scale", "quality.dlss").need("quality.dlss"),
                         D("quality.dlss_width", "quality.dlss").need("quality.dlss"),
                         D("quality.dlss_height", "quality.dlss").need("quality.dlss")}));
    out.push_back(group(Area::Speed, Text::PageFrames,
                        {B("frames.vsync"), B("frames.fps_limit"), B("frames.fps_counter"),
                         B("frames.battery_saver"), B("frames.keep_effects"),  // 3.0.23-beta.2
                         D("frames.battery_fps", "frames.battery_saver").need("frames.battery_saver"),
                         D("frames.save_power"), D("frames.keep_fps"), D("frames.light_skills"),
                         B("frames.frame_generation").need("quality.dlss"), D("frames.reflex"),
                         D("frames.allow_tearing")}));
    out.push_back(group(Area::Speed, Text::PageScreen,
                        {B("display.high_resolution"), B("display.hdr"), D("display.ui_native"),
                         D("display.ui_filter"), D("display.crisp_text"), D("display.crisp_numbers"),
                         D("display.window_art").need("display.crisp_text"),
                         D("display.dpi_aware")}));  // 3.0.22-beta.6: the window in real pixels where it is needed
    out.push_back(group(Area::Speed, Text::PageRender,
                        {B("renderer.api"), B("renderer.gpu"), D("renderer.bridge"), D("renderer.safe_mode"),
                         // a laptop's two cards: set before the game is playable at all (R-T8FGMM: 5 frames a second)
                         X("renderer.cross_present").start(), trial("renderer.remix", "")}));
    // what our renderer draws in the game's place - in the top view too
    out.push_back(group(Area::Speed, Text::CatOwnWorld,
                        {X("view3d.own_models"), X("view3d.fast_ground"),
                         X("effects.leaves_card").need("view3d.own_models"), X("quality.batch_ground")}));
    // --- «Гра й мережа»
    out.push_back(group(Area::Game, Text::WindowsHeading,
                        {B("windows.title_name"), B("windows.remember_place"), B("windows.switch_keys"),
                         B("windows.start_count").start(), D("windows.others_light"), D("windows.others_music"),
                         D("windows.others_sounds")}));
    out.push_back(group(Area::Game, Text::InterfaceSecNet,
                        {B("interface.smooth_fixes"), D("interface.predict_walk"),  // 3.0.21-beta.16: a setting
                         B("connection.patient_entry"), D("connection.entry_pause", "connection.patient_entry"),
                         B("connection.auto_relogin")}));  // 3.0.22-beta.3
    out.push_back(group(Area::Game, Text::PageService,
                        // what is told the developers is agreed in the settings app (its first start asks)
                        {B("report.auto"), B("report.disconnects").start(), B("stats.send").start(),
                         B("launcher.direct_start")}));
    out.push_back(group(Area::Game, Text::CatClientFiles,
                        {B("client.updates"), B("client.exe_4gb"),
                         D("client.cps"), D("client.bink"), D("client.ijl"), D("client.mss"),
                         D("client.roext")}));
    // --- «Текстури»
    out.push_back(group(Area::Textures, Text::AiTabWhat,
                        {B("ai.enabled"), D("ai.ground", "ai.enabled"), D("ai.objects", "ai.enabled"),
                         D("ai.monsters", "ai.enabled"), D("ai.players", "ai.enabled"), D("ai.npcs", "ai.enabled"),
                         D("ai.items", "ai.enabled"), D("ai.dyes", "ai.enabled"), D("ai.effects", "ai.enabled"),
                         D("ai.icons", "ai.enabled"), D("ai.ui_pieces", "ai.enabled")}));
    out.push_back(group(Area::Textures, Text::AiTabModels, aiModels()));
    out.push_back(group(Area::Textures, Text::CatAiWork,
                        {D("ai.scale"), D("ai.denoise"), D("ai.budget"), D("ai.min_size"), D("ai.max_size"),
                         D("ai.compress"), D("ai.ahead")}));
    // --- «Діагностика»
    out.push_back(group(Area::Diagnostics, Text::PageDiagnostics,
                        {diag("debug.log"), diag("debug.profile"), diag("debug.xray"), diag("debug.seams_view"),
                         diag("interface.uaui_showcase"), diag("mcp.enabled"), diag("mcp.port"),
                         // 3.0.21-beta.7: «Налаштування» itself - turned off there, the window with every other
                         // row was gone; the console opens whatever is off
                         diag("interface.settings_window")}));
    out.push_back(group(Area::Diagnostics, Text::PageDebug,
                        {diag("debug.weak_card"), diag("debug.shader_variants"), diag("debug.record_scene"),
                         diag("debug.state_cache"), diag("debug.merge_draws"), diag("debug.fast_pick"),
                         diag("debug.fast_ftol"), diag("debug.fast_textures"), diag("debug.quick_acks"),
                         diag("quality.dlss_jitter"), diag("quality.dlss_motion"), diag("renderer.bridge_present"),
                         diag("renderer.bridge_dpi_scale"), diag("roext.window_x"), diag("roext.window_y"),
                         diag("ai.generate")}));
    // The effects hidden behind «Вигляд умінь» that no group's member is (the mages' parts): the file's alone, with
    // the skills
    std::map<std::string, bool> placed;
    Group* skills = nullptr;
    for (Group& g : out) {
        for (const Row& r : g.rows) placed[r.key] = true;
        if (g.area == Area::Picture && g.name == Text::FxGroupSkillEffects) skills = &g;
    }
    for (const IniSetting& s : iniSettings()) {
        const std::string key = sectionKey(s);
        if (skills && !placed[key] && hiddenEffect(key.c_str())) skills->rows.push_back(X(key));
    }
    // Direct3D 12 alone draws these; the 3D view's rows do nothing in the top view («Моделі нашим рендером» does)
    static const char* const kDirect3D12[] = {
        "view3d.room_walls",        "view3d.shadows",        "renderer.cross_present",
        "renderer.remix",         "display.hdr",              "display.ui_native",     "quality.taa",
        "effects.motes",          "effects.floor_reflections", "effects.shafts",       "effects.bloom",
        "effects.footprints",     "effects.storms",           "effects.grass",         "effects.skill_sparks",
        "effects.map_fade",       "effects.ripples",          "effects.wet_ground",    "effects.sand_drift",
        "effects.wet_walls",      "effects.butterflies",      "effects.birds",         "effects.night_fireflies",
        "effects.leaf_fall",      "effects.embers",           "effects.splashes",      "effects.cave_drips",
        "effects.underwater",     "effects.materials",        "effects.ground_seams",  "effects.smooth_hills",
        "effects.own_ground",     "effects.world_reflections", "effects.seasons",      "effects.rain_wet",
        "effects.night_windows",  "effects.shared_wind",      "effects.lit_dust",      "effects.rain_splashes",
        "effects.dragonflies",    "ai.generate"};
    for (Group& g : out)
        for (Row& r : g.rows) {
            for (const EffectNeed& n : kEffectNeeds)
                if (r.key == n.key) r.needs.push_back(Need{NeedKind::Row, n.needs});
            for (const char* k : kDirect3D12)
                if (r.key == k) r.needs.push_back(Need{NeedKind::Direct3D12});
            if (r.key.compare(0, 7, "view3d.") == 0 && r.key != "view3d.own_models")
                r.needs.push_back(Need{NeedKind::View3d});
            // 3.0.21-beta.10: «Роздільність світу» is Direct3D 11's (its hint's first words); the aura is made when
            // the character appears - «після телепорту чи нової карти»
            if (r.key == "quality.world_scale") r.needs.push_back(Need{NeedKind::Direct3D11});
            if (r.key == "effects.aura") r.nextMap = true;
        }
    return out;
}

struct Index {
    std::vector<Group> groups = build();
    std::map<std::string, std::pair<int, int>> at;
    Index() {
        for (size_t g = 0; g < groups.size(); ++g)
            for (size_t r = 0; r < groups[g].rows.size(); ++r) at.emplace(groups[g].rows[r].key, std::pair{g, r});
    }
};

const Index& index() {
    static const Index i;
    return i;
}

}  // namespace

Text areaName(Area a) {
    switch (a) {
        case Area::Picture: return Text::LookPicture;
        case Area::View3d: return Text::View3dHeading;
        case Area::Interface: return Text::ViewInterface;
        case Area::Controls: return Text::CatControls;
        case Area::Sound: return Text::CatSound;
        case Area::Speed: return Text::CatSpeed;
        case Area::Game: return Text::CatGame;
        case Area::Textures: return Text::LookTextures;
        case Area::Diagnostics: break;
    }
    return Text::PageDiagnostics;
}

const std::vector<Group>& groups() { return index().groups; }

const Row* find(const std::string& sectionKey) {
    const Index& i = index();
    const auto it = i.at.find(sectionKey);
    return it == i.at.end() ? nullptr : &i.groups[size_t(it->second.first)].rows[size_t(it->second.second)];
}

const Group* groupOf(const std::string& sectionKey) {
    const Index& i = index();
    const auto it = i.at.find(sectionKey);
    return it == i.at.end() ? nullptr : &i.groups[size_t(it->second.first)];
}

Home homeOf(const Row& r) {
    if (hiddenEffect(r.key.c_str())) return Home::File;
    if (r.status != Status::Permanent) return Home::Lab;
    return appliesNow(r.key) && !r.atStart ? Home::Game : Home::Start;
}

bool met(const Need& n, const Config& shown, bool direct3d12, bool view3d) {
    if (n.kind == NeedKind::Direct3D12) return direct3d12;
    if (n.kind == NeedKind::Direct3D11) return !direct3d12;
    if (n.kind == NeedKind::View3d) return view3d;
    const IniSetting* s = n.key ? findSetting(n.key) : nullptr;
    if (!s || s->tokens.empty()) return true;
    return tokenOf(*s, shown) != s->tokens[0];  // a check on, a list off its first word
}

std::vector<LabSection> trials() {
    std::vector<LabSection> out;
    for (const Group& g : groups())
        for (const Row& r : g.rows) {
            if (r.status != Status::Trial) continue;
            if (out.empty() || out.back().area != g.area) out.push_back(LabSection{g.area, {}});
            out.back().rows.push_back(&r);
        }
    return out;
}

std::vector<const Row*> liveDiagnostics() {
    std::vector<const Row*> out;
    for (const Group& g : groups())
        for (const Row& r : g.rows)
            if (r.status == Status::Diagnostic && appliesNow(r.key)) out.push_back(&r);
    return out;
}

bool About::allMet() const {
    for (const NeedWord& n : needs)
        if (!n.met) return false;
    return true;
}

About about(const Row& r, const Config& shown, bool direct3d12, bool view3d, Lang l) {
    About a;
    a.since = r.since;
    for (const Need& n : r.needs) {
        NeedWord w;
        w.met = met(n, shown, direct3d12, view3d);
        if (n.kind == NeedKind::Direct3D12) {
            w.name = "Direct3D 12";
        } else if (n.kind == NeedKind::Direct3D11) {
            w.name = "Direct3D 11";
        } else if (n.kind == NeedKind::View3d) {
            w.name = tr(l, Text::View3dHeading);
        } else {
            const IniSetting* s = n.key ? findSetting(n.key) : nullptr;
            if (!s) continue;
            w.name = tr(l, s->label);
        }
        a.needs.push_back(std::move(w));
    }
    a.afterRestart = !appliesNow(r.key);
    a.nextMap = r.nextMap;
    return a;
}

std::string aboutText(const About& a, Lang l, const char* yes, const char* no) {
    std::vector<std::string> parts;
    if (!a.since.empty()) parts.push_back(fmt(tr(l, Text::LabSince), {a.since}));
    if (!a.needs.empty()) {
        std::string list;
        for (const NeedWord& n : a.needs) list += (list.empty() ? "" : ", ") + std::string(n.met ? yes : no) + n.name;
        parts.push_back(fmt(tr(l, Text::LabNeeds), {list}));
    }
    if (a.afterRestart) parts.push_back(tr(l, Text::LabAfterRestart));
    if (a.nextMap) parts.push_back(tr(l, Text::LabNextMap));
    std::string out;
    for (const std::string& p : parts) out += (out.empty() ? "" : " \xC2\xB7 ") + p;
    return out;
}

std::string whyNot(const Row& r, const Config& shown, bool direct3d12, bool view3d, Lang l) {
    Now now;
    now.direct3d12 = direct3d12;
    now.view3d = view3d;
    return whyNot(r, shown, now, l);
}

namespace {

// The map cannot show what the row is about (core/effect_where: by the map's name - the open sky, the desert, a
// cave…): an effect's own switch, a part by the effect it stands under, the season by «Пори року». The weather's
// kind is a choice: whichever is chosen falls under the open sky alone - said indoors only
bool notHere(const Row& r, const std::string& map, const EffectSettings& e) {
    if (r.key == "effects.weather_kind") return !outdoors(map);
    std::string effect;
    if (r.key == "effects.season") effect = "effects.seasons";
    else if (effect_groups::memberOf(r.key)) effect = r.key;
    else if (!r.under.empty() && effect_groups::memberOf(r.under)) effect = r.under;
    return !effect.empty() && !effect_where::worksOn(effect, map, e);
}

}  // namespace

std::string whyNot(const Row& r, const Config& shown, const Now& now, Lang l) {
    std::vector<std::string> parts;
    std::string rows;
    bool card12 = false, card11 = false, view = false;
    const Group* g = groupOf(r.key);
    for (const Need& n : r.needs) {
        if (met(n, shown, now.direct3d12, now.view3d)) continue;
        if (n.kind == NeedKind::Direct3D12) {
            card12 = true;
        } else if (n.kind == NeedKind::Direct3D11) {
            card11 = true;
        } else if (n.kind == NeedKind::View3d) {
            view = !now.ownEntry || !g || g->area != Area::View3d;
        } else if (const IniSetting* s = n.key ? findSetting(n.key) : nullptr) {
            rows += (rows.empty() ? "" : ", ") + std::string("\xC2\xAB") + tr(l, s->label) + "\xC2\xBB";
        }
    }
    if (card12) parts.push_back(tr(l, Text::WhyDirect3D12));
    if (card11) parts.push_back(tr(l, Text::WhyDirect3D11));
    if (view) parts.push_back(tr(l, Text::WhyView3d));
    if (!rows.empty()) parts.push_back(fmt(tr(l, Text::WhyNeeds), {rows}));
    // 3.0.21-beta.10: the map cannot show it (the weather indoors, the year in the desert, grass in a cave)
    if (!now.map.empty() && notHere(r, now.map, shown.effects)) parts.push_back(tr(l, Text::WhyNotHere));
    if (r.nextMap) parts.push_back(tr(l, Text::WhyNextMap));
    std::string out;
    for (const std::string& p : parts) out += (out.empty() ? "" : " \xC2\xB7 ") + p;
    return out;
}

}  // namespace uagfx::settings::catalogue

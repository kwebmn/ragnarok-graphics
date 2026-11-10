#include "core/ui_theme_css.hpp"

namespace uagfx::uitheme {

namespace {

const char* kTheme = R"(
body { font-family: Plex; font-size: 14dp; color: #e9edf2; }
.ua-screen { width: 100%; height: 100%; }
.ua-card { position: absolute; left: 50%; top: 96dp; margin-left: -160dp; width: 320dp; padding: 12dp 14dp;
           border-radius: 8dp; border: 1dp #ffffff22; background-color: #161b22e0; drag: drag; cursor: move;
           transition: opacity transform background-color border-color 0.16s quadratic-out; }
.ua-card.away { opacity: 0; transform: scale(0.96); }
.ua-card:hover { background-color: #1e2633f0; border-color: #3d7fd8aa; }
.ua-name { font-weight: bold; font-size: 15dp; }
.ua-sub { color: #9aa4b2; font-size: 12dp; margin-left: 8dp; }
.ua-row { display: block; margin-top: 8dp; }
.ua-num { font-size: 12dp; color: #c9d1d9; }
.ua-bar { display: block; height: 8dp; border-radius: 4dp; background-color: #2a313c; margin-top: 4dp; }
.ua-fill { display: block; height: 8dp; border-radius: 4dp; background-color: #3fb950; }
.ua-fill.sp { background-color: #388bfd; }
.ua-tag { display: block; margin-top: 10dp; font-size: 11dp; color: #7d8590; }
.ua-window { position: absolute; width: 440dp; border-radius: 8dp; border: 1dp #ffffff22;
             background-color: #161b22eb; transition: opacity transform 0.12s quadratic-out; }
.ua-window.away { opacity: 0; transform: scale(0.96); }
.ua-title { display: block; height: 40dp; padding-left: 16dp; border-bottom: 1dp #ffffff14; cursor: move; }
.ua-title-name { font-size: 15dp; font-weight: bold; line-height: 40dp; }
.ua-close { position: absolute; right: 8dp; top: 6dp; width: 28dp; height: 28dp; line-height: 28dp;
            text-align: center; border-radius: 6dp; color: #8b949e; transition: background-color color 0.1s; }
.ua-close:hover { background-color: #ffffff18; color: #e9edf2; }
.ua-content { display: block; padding: 12dp 16dp; }
.ua-hint { display: block; padding: 8dp 16dp 10dp 16dp; border-top: 1dp #ffffff14; font-size: 11dp; color: #8b949e; }
tabset { display: block; }
tabset tabs { display: block; margin-bottom: 12dp; }
tab { display: inline-block; margin-right: 18dp; padding-bottom: 4dp; color: #8b949e; border-bottom: 2dp #00000000;
      transition: color border-color 0.12s; cursor: pointer; }
tab:hover { color: #c9d1d9; }
tab:selected { color: #e9edf2; font-weight: bold; border-bottom-color: #3d7fd8; }
panels { display: block; }
.ua-grid { display: block; }
.ua-slot { display: inline-block; position: relative; width: 52dp; height: 52dp; margin: 0 6dp 6dp 0;
           border-radius: 8dp; border: 1dp #ffffff1a; background-color: #ffffff06;
           transition: background-color border-color 0.1s; }
.ua-slot:hover { background-color: #ffffff14; border-color: #3d7fd8aa; }
.ua-slot img { position: absolute; left: 6dp; top: 6dp; width: 40dp; height: 40dp; }
.ua-count { position: absolute; right: 4dp; bottom: 2dp; font-size: 11dp; font-weight: bold; }
.ua-list { display: block; height: 220dp; overflow-y: auto; }
.ua-list p { display: block; padding: 3dp 6dp; border-radius: 4dp; }
.ua-list p:hover { background-color: #ffffff0e; }
.ua-label { display: block; color: #8b949e; font-size: 12dp; margin: 10dp 0 6dp 0; }
.ua-button { display: inline-block; padding: 6dp 14dp; margin: 0 8dp 8dp 0; border-radius: 6dp;
             background-color: #2a313c; transition: background-color 0.1s; cursor: pointer; }
.ua-button:hover { background-color: #34404f; }
.ua-button:active { background-color: #3d7fd8; }
input.checkbox { display: inline-block; width: 16dp; height: 16dp; margin-right: 8dp; border-radius: 4dp;
                 border: 1dp #ffffff50; vertical-align: middle; transition: background-color border-color 0.1s; }
input.checkbox:checked { background-color: #3d7fd8; border-color: #3d7fd8; }
/* 3.0.18-beta.59 (the possibilities' item 9): the form's parts in the game's look */
input.radio { display: inline-block; width: 16dp; height: 16dp; margin-right: 8dp; border-radius: 8dp;
              background-color: #0000004d; border: 1dp #ffffff40; vertical-align: middle; }
input.radio:checked { background-color: #c9a55b; border-color: #e6cb8a; }
input.text, input.password, textarea { display: inline-block; box-sizing: border-box; width: 100%; height: 30dp;
    padding: 6dp 10dp; border-radius: 6dp; background-color: #00000059; border: 1dp #ffffff1a; color: #eceff5;
    cursor: text; }
input.text:focus, input.password:focus, textarea:focus { border-color: #c9a55b99; }
textarea { height: 80dp; }
input.range { display: inline-block; width: 100%; height: 18dp; }
input.range slidertrack { margin-top: 7dp; height: 4dp; border-radius: 2dp; background-color: #ffffff1f; }
input.range sliderbar { width: 16dp; height: 16dp; margin-top: 1dp; border-radius: 8dp; background-color: #e6cb8a;
                        border: 1dp #00000066; }
input.range sliderbar:hover { background-color: #f3d998; }
input.range sliderarrowdec, input.range sliderarrowinc { width: 0; height: 0; }
select { display: inline-block; box-sizing: border-box; width: 100%; height: 30dp; border-radius: 6dp;
         background-color: #00000059; border: 1dp #ffffff1a; color: #eceff5; }
select selectvalue { margin: 6dp 30dp 0 10dp; height: 18dp; }
select selectarrow { width: 24dp; height: 28dp; }
select selectbox { margin-top: 2dp; padding: 4dp 0; border-radius: 6dp; background-color: #10141cf5;
                   border: 1dp #ffffff24; }
select selectbox option { display: block; padding: 4dp 10dp; }
select selectbox option:hover { background-color: #ffffff14; }
select selectbox option:checked { color: #e6cb8a; }
.ua-row2 { display: block; margin-bottom: 10dp; }
/* 3.0.18-beta.17: the character block (win/char_block) - the concept's v17_char, in concept pixels (dp) */
.cb { position: absolute; font-family: Plex; color: #eceff5; transform-origin: 0 0; }
.cb .card { position: absolute; left: 0; top: 0; border-radius: 10dp; border: 1dp #ffffff17;
            background-color: #10141ccc; transition: border-color 0.3s quadratic-out; }
.cb.flare .card { border-color: #e6cb8a; }
.cb .abs { position: absolute; }
.cb .portrait { position: absolute; border-radius: 999px; background-color: #1b1d24; overflow: hidden;
                cursor: pointer; }
.cb .portrait img { position: absolute; left: 7%; top: 7%; width: 86%; height: 86%; }
.cb .letter { display: block; width: 100%; text-align: center; font-weight: bold; color: #eceff5; }
.cb .name { font-size: 16dp; font-weight: bold; }
.cb .job { font-size: 13dp; color: #9aa3b5; margin-left: 8dp; }
.cb .levels { position: absolute; right: 12dp; top: 14dp; font-size: 13dp; font-weight: bold; color: #e6cb8a; }
.cb .line { position: absolute; height: 17dp; font-size: 12.5dp; white-space: nowrap; }
.cb .muted { color: #9aa3b5; }
.cb .gold { color: #e6cb8a; }
.cb .warn { color: #f0a050; }
.cb .bad { color: #ff5a4e; }
.cb .fight { color: #f3d27a; font-weight: bold; }
.cb .chip { margin-right: 12dp; }
.cb .label { position: absolute; font-size: 11dp; font-weight: bold; color: #9aa3b5; }
.cb .track { position: absolute; border-radius: 5dp; background-color: #00000073; }
.cb .fill { position: absolute; left: 0; top: 0; height: 100%; border-radius: 5dp; }
.cb .hp { background-color: #5bd36c; }
.cb .hp.low { background-color: #ff5a4e; }
.cb .sp { background-color: #4f8bff; }
.cb .base { background-color: #b7a6ff; }
.cb .jobx { background-color: #7fd3c3; }
.cb .weightfill { background-color: #c9a55b; }
.cb .num { position: absolute; font-size: 13dp; font-weight: bold; text-align: right; }
.cb .num.low { color: #ff9a8f; }
.cb .small { position: absolute; font-size: 11dp; white-space: nowrap; }
.cb .cell { position: absolute; border-radius: 6dp; background-color: #1b1d24b3; }
.cb .cell .k { position: absolute; left: 7dp; top: 2dp; font-size: 9.5dp; font-weight: bold; color: #9aa3b5; }
.cb .cell .v { position: absolute; left: 7dp; top: 14dp; font-size: 12.5dp; font-weight: bold; }
.cb .button { position: absolute; border-radius: 7dp; background-color: #1b1d24b3; cursor: pointer;
              transition: background-color 0.1s; }
.cb .button:hover { background-color: #2a2f3a; }
.cb .button:active { background-color: #3a3f4c; }
.cb .button img { position: absolute; left: 7dp; top: 4dp; width: 18dp; height: 18dp; }
.cb .badge { position: absolute; border-radius: 9dp; background-color: #10141c; border: 1dp #c9a55b;
             text-align: center; font-weight: bold; font-size: 12dp; }
.cb .zone { position: absolute; }
.cb .tip { position: absolute; padding: 4dp 10dp 4dp 10dp; border-radius: 8dp; background-color: #10141cee;
           border: 1dp #ffffff17; white-space: nowrap; font-size: 13dp;
           margin-right: -2000dp; }  /* 3.0.18-beta.55 (R-PWJXJ3): as wide as its longest line, past the block */
.cb .tip p { display: block; color: #9aa3b5; line-height: 17dp; }
.cb .tip p.first { color: #eceff5; font-weight: bold; }
.cb .warning { position: absolute; padding: 0 12dp; height: 30dp; line-height: 30dp; border-radius: 8dp;
               background-color: #3a1414; font-size: 13dp; font-weight: bold; white-space: nowrap;
               transition: opacity transform 0.18s quadratic-out; }
.cb .warning.away { opacity: 0; transform: translateY(-8dp); }
/* 3.0.18-beta.18: the target's card (win/target_card) - the concept's target16 */
.tc { position: absolute; font-family: Plex; color: #eceff5; text-align: center; transform-origin: 50% 0;
      transition: opacity transform 0.12s quadratic-out; }
.tc.away { opacity: 0; transform: translateY(-4dp); }
.tc .card { display: inline-block; position: relative; height: 48dp; padding: 0 8dp; border-radius: 10dp;
            border: 1dp #ffffff17; background-color: #10141ccc; text-align: left; white-space: nowrap;
            cursor: pointer; }
.tc .corner { position: absolute; width: 11dp; height: 11dp; border-color: #c9a55b; }
.tc .tl { left: 0; top: 0; border-left-width: 2dp; border-top-width: 2dp; }
.tc .tr { right: 0; top: 0; border-right-width: 2dp; border-top-width: 2dp; }
.tc .bl { left: 0; bottom: 0; border-left-width: 2dp; border-bottom-width: 2dp; }
.tc .br { right: 0; bottom: 0; border-right-width: 2dp; border-bottom-width: 2dp; }
.tc .round { display: inline-block; vertical-align: top; margin-top: 8dp; width: 32dp; height: 32dp;
             border-radius: 16dp; background-color: #1b1d24; }
.tc .round img { width: 32dp; height: 32dp; }
.tc .mid { display: inline-block; vertical-align: top; margin-left: 10dp; }
.tc .name { font-size: 16dp; font-weight: bold; }
.tc .lv { font-size: 13dp; color: #9aa3b5; margin-left: 6dp; }
.tc .chip { display: inline-block; margin-left: 8dp; padding: 0 8dp; height: 22dp; line-height: 22dp;
            border-radius: 6dp; border-width: 1dp; font-size: 12.5dp; font-weight: bold; }
.tc .top { display: block; height: 22dp; line-height: 22dp; margin-top: 13dp; }
.tc .top.withhp { margin-top: 4dp; }
.tc .hpline { display: block; height: 16dp; }
.tc .hptrack { display: inline-block; vertical-align: middle; width: 80dp; height: 5dp; border-radius: 3dp;
               background-color: #0000008c; }
.tc .hpfill { display: block; height: 5dp; border-radius: 3dp; background-color: #5bd36c; }
.tc .hpfill.low { background-color: #e5534b; }
.tc .pct { margin-left: 6dp; font-size: 12dp; font-weight: bold; }
.tc .book { display: inline-block; vertical-align: top; margin: 9dp 0 0 12dp; width: 30dp; height: 30dp;
            border-radius: 7dp; background-color: #ffffff0f; border: 1dp #c9a55b00;
            transition: background-color border-color 0.1s; }
.tc .book:hover { background-color: #ffffff24; border-color: #c9a55bcc; }
.tc .book img { margin: 5dp 0 0 5dp; width: 18dp; height: 18dp; }
.tc .book .lit { display: none; }
.tc .book:hover .lit { display: inline-block; }
.tc .book:hover .dim { display: none; }
.tc .hints { display: inline-block; vertical-align: top; margin: 6dp 0 0 13dp; padding-left: 7dp;
             border-left: 1dp #ffffff1a; font-size: 13dp; }
.tc .hints div { display: block; height: 19dp; }
.tc .hints img { width: 17dp; height: 17dp; margin-right: 7dp; vertical-align: middle; }
.tc .details { display: none; margin-top: 6dp; padding: 5dp 12dp 5dp 12dp; border-radius: 8dp;
               border: 1dp #ffffff1f; background-color: #10141cf0; text-align: left; white-space: nowrap; }
.tc:hover .details { display: inline-block; }
.tc .details p { display: block; height: 18dp; line-height: 18dp; font-size: 13dp; }
.tc .details p.first { font-weight: bold; }
/* 3.0.18-beta.19: the party's panel (win/party_panel) - the concept's v19_cave */
.pp { position: absolute; font-family: Plex; color: #eceff5; transform-origin: 0 0; }
.pp .glass { display: block; width: 260dp; padding: 0 8dp 8dp 8dp; border-radius: 10dp; border: 1dp #ffffff17;
             background-color: #10141ccc; }
.pp .head { display: block; position: relative; height: 26dp; line-height: 26dp; border-bottom: 1dp #ffffff1a;
            font-size: 12dp; color: #9aa3b5; }
.pp .head.folded { border-bottom-width: 0; }
.pp .title { position: absolute; left: 4dp; font-weight: bold; }
.pp .count { position: absolute; right: 56dp; }
.pp .hbtn { position: absolute; top: 3dp; width: 22dp; height: 20dp; line-height: 20dp; text-align: center;
            border-radius: 5dp; color: #eceff5; cursor: pointer; }
.pp .hbtn:hover { background-color: #ffffff18; }
.pp .viewb { right: 26dp; }
.pp .foldb { right: 2dp; }
.pp .row { display: block; position: relative; height: 40dp; border-radius: 7dp; border: 1dp #00000000;
    cursor: pointer; }
.pp .row.buffs { height: 58dp; }
.pp .row.low { background-color: #e5534b1a; border-color: #e5534bd9; }
.pp .row.target { background-color: #c9a55b14; border-color: #c9a55be6; }
.pp .row.chosen { background-color: #c9a55b38; border-color: #c9a55b; border-width: 2dp; }
.pp .row.dead { background-color: #3a14148c; border-color: #e5534b99; }
.pp .row.alarm { background-color: #e5534b4d; border-color: #e5534b; border-width: 2dp; }
.pp .buf.glow { border: 1.5dp #c9a55b; }
.pp .face { position: absolute; left: 2dp; top: 6dp; width: 28dp; height: 28dp; border-radius: 14dp;
    background-color: #1b1d24; }
.pp .face img { width: 28dp; height: 28dp; }
.pp .face.away img { opacity: 0.45; }
.pp .letter { display: block; text-align: center; line-height: 28dp; font-size: 10.5dp; font-weight: bold;
    color: #9aa3b5; }
.pp .name { position: absolute; left: 38dp; top: 5dp; height: 18dp; line-height: 18dp; font-size: 13dp;
            font-weight: bold; white-space: nowrap; }
.pp .name.bad { color: #ff9a8f; }
.pp .name.away { color: #9aa3b5; }
.pp .after { font-size: 11.5dp; font-weight: normal; color: #9aa3b5; margin-left: 6dp; }
.pp .after.cast { color: #7fc4ff; }
.pp .crown { width: 13dp; height: 13dp; margin-left: 4dp; vertical-align: middle; }
.pp .value { position: absolute; right: 4dp; top: 5dp; height: 18dp; line-height: 18dp; font-size: 13dp;
             font-weight: bold; white-space: nowrap; }
.pp .value.muted { color: #9aa3b5; font-size: 12dp; font-weight: normal; }
.pp .value.bad { color: #ff9a8f; }
.pp .value img { width: 12dp; height: 12dp; margin-right: 4dp; vertical-align: middle; }
.pp .bar { position: absolute; left: 38dp; top: 27dp; right: 48dp; height: 5dp; border-radius: 3dp;
           background-color: #00000073; }
.pp .bar .fill { display: block; height: 5dp; border-radius: 3dp; background-color: #5bd36c; }
.pp .bar .fill.bad { background-color: #e5534b; }
.pp .castline { position: absolute; left: 38dp; top: 33.5dp; right: 48dp; height: 2.5dp; border-radius: 1dp;
                background-color: #00000073; }
.pp .castline .fill { display: block; height: 2.5dp; background-color: #7fc4ff; }
.pp .where { position: absolute; right: 4dp; top: 22dp; width: 40dp; height: 14dp; font-size: 11dp; color: #9aa3b5;
             text-align: right; }
.pp .where img { position: absolute; left: 0; top: 0; width: 14dp; height: 14dp; }
.pp .bufrow { position: absolute; left: 38dp; top: 37dp; white-space: nowrap; }
.pp .buf { display: inline-block; position: relative; width: 16dp; height: 16dp; margin-right: 3dp;
           border-radius: 3dp; }
.pp .buf img { width: 16dp; height: 16dp; }
.pp .buf .time { position: absolute; left: 0; top: 17dp; width: 16dp; height: 2dp; background-color: #00000080; }
.pp .buf .time .fill { display: block; height: 2dp; background-color: #c9a55b; }
.pp .buf .time .fill.end { background-color: #e5534b; }
.pp .cells { display: block; }
.pp .cell { display: inline-block; position: relative; width: 125dp; height: 30dp; margin-right: 6dp; }
.pp .cell .face { left: 2dp; top: 6dp; width: 16dp; height: 16dp; border-radius: 8dp; }
.pp .cell .face img { width: 16dp; height: 16dp; }
.pp .cell .letter { line-height: 16dp; font-size: 9dp; }
.pp .cell .name { left: 23dp; top: 3dp; height: 15dp; line-height: 15dp; font-size: 11.5dp; }
.pp .cell .bar { left: 23dp; right: 3dp; top: 20dp; height: 4dp; }
.pp .cell .bar .fill { height: 4dp; }
/* 3.0.18-beta.20: the buffs' column (win/buff_column) - the concept's v19_vbuffs_ab; places in the target's pixels */
.bc { position: absolute; font-family: Plex; color: #eceff5; font-size: 1px; }
.bc .slot { position: absolute; cursor: pointer; }
.bc .shadow { position: absolute; background-color: #00000038; }
.bc .outline { position: absolute; background-color: #000000b3; }
.bc .icon { position: absolute; background-color: #1b1d24; }
.bc .icon img { width: 100%; height: 100%; }
.bc .bad { position: absolute; border: 1.6rem #ff5a4ef2; }
.bc .hover { position: absolute; border: 1.5rem #ffffffd9; }
.bc .time { position: absolute; text-align: center; font-size: 11.5rem; font-weight: bold;
            font-effect: outline(2px #000000d9); }
.bc .time.warn { color: #f0a050; }
.bc .time.bad { color: #ff9a8f; }
.bc .name { position: absolute; text-align: right; font-size: 13rem; font-weight: bold; white-space: nowrap;
            font-effect: outline(2px #000000d9); }
.bc .name.bad { color: #ff9a8f; }
.bc .more { position: absolute; text-align: center; font-size: 12rem; font-weight: bold; cursor: pointer;
            font-effect: outline(2px #000000d9); }
.bc .toggle { position: absolute; background-color: #00000073; border: 1rem #ffffff47; cursor: pointer;
              transition: background-color 0.1s; }
.bc .toggle:hover { background-color: #0000009e; }
.bc .toggle.on { border-color: #e6cb8ad9; }
.bc .toggle .ln { position: absolute; left: 50%; margin-left: -5rem; width: 10rem; height: 1.5rem; border-radius: 1rem;
                  background-color: #ffffffe6; }
.bc .toggle.on .ln { background-color: #e6cb8ae6; }
.bc .tip { position: absolute; padding: 4rem 10rem 0 10rem; border-radius: 8rem; border: 1rem #ffffff1f;
           background-color: #10141cf0; white-space: nowrap; }
.bc .tip .a { display: block; font-size: 13rem; font-weight: bold; height: 18rem; line-height: 18rem; }
.bc .tip .b { display: block; font-size: 11.5rem; color: #9aa3b5; height: 15rem; line-height: 15rem; }
/* 3.0.18-beta.21: the hotbar (win/hotbar) - the concept's cells; places in the target's pixels */
.xb { position: absolute; font-family: Plex; color: #eceff5; }
.xg { position: absolute; font-family: Plex; }
.it { position: absolute; font-family: Plex; }
.hp { position: absolute; font-family: Plex; border-color: #ffffff17; }
.mp { position: absolute; font-family: Plex; border-color: #ffffff17; }
.tr { position: absolute; font-family: Plex; }
.mm { position: absolute; font-family: Plex; }
/* 3.0.18-beta.52 (R-W9WZ4G: the signs off the round buttons' middles): an edge inside the box, as roundRect's */
.mm div, .sh div, .ig div, .tr div, .hb div, .hp, .mp { box-sizing: border-box; }
.ig { position: absolute; font-family: Plex; }
.sh { position: absolute; font-family: Plex; }
.sh div, .sh span, .sh .a { position: absolute; }
.sh .a { left: 0; top: 0; }
.sh .clip { overflow: hidden; }
.sh .glass { border-color: #ffffff17; }
.sh .mark { display: none; left: 0; top: 0; width: 100%; height: 100%; border: 1.5dp #e6cb8ad9; border-radius: 12dp; }
.sh.marked .mark { display: block; }
.sh .hot { background-color: #ffffff1f; }
.sh .rule { background-color: #ffffff14; }
.sh .cell { background-color: #00000059; border-color: #ffffff14; }
.sh .cell.hot { border-color: #c9a55bf2; }
.sh .cell.late { border-color: #ff7d6ef2; }
.sh .plate { background-color: #000000b3; }
.sh .field { background-color: #00000059; border-color: #ffffff1a; }
.sh .field.typing { border-color: #c9a55b99; }
.sh .caret { background-color: #eef2f8f2; }
.sh .warn { background-color: #ff7d6e24; border-color: #ff7d6e80; }
.sh .ready { background-color: #5fd08a38; border-color: #5fd08ab3; }
.sh .dropping { background-color: #c9a55b0f; border-color: #f3c653e6; }
.sh .btn { background-color: #ffffff14; border-color: #ffffff24; }
.sh .btn.hot { background-color: #ffffff29; }
.sh .btn.off { border-color: #ffffff0d; }
.sh .btn.main { background-color: #f3c653d9; }
.sh .btn.main.hot { background-color: #f3c653f2; }
.sh .thumb { background-color: #ffffff40; }
.sh .tip { background-color: #10141cf5; border-color: #ffffff24; }
.sh .slothot { background-color: #ffffff0f; }
.sh .focus { background-color: #c9a55b14; border-color: #f3c653e6; }
.sh .fits { background-color: #c9a55b1f; border-color: #f3c653f2; }
.sh .tile { background-color: #1b1d24e6; border-color: #ffffff14; }
.sh .tile.round { background-color: #1b1d24; }
.sh .dot { background-color: #ffffff29; }
.sh .dot.card { background-color: #f3c653; }
.sh .picks { background-color: #1b1d24f2; border-color: #c9a55b80; }
.sh .pickhot { background-color: #ffffff12; }
.sh .word { white-space: nowrap; overflow: hidden; }
.sh .padfocus { position: absolute; border-color: #e6cb8a; background-color: #e6cb8a14; }  /* 3.0.18-beta.60 */
.motion.sh .padfocus { transition: left top width height 0.1s cubic-out; }
.sh .word.bold { font-weight: bold; }
.sh .word.right { text-align: right; }
.sh .word.centre { text-align: center; }
.sh .word.wrap { white-space: normal; }
.sh .word.halo { font-effect: outline(2px #000000d9); }
.ig div, .ig span, .ig .a { position: absolute; }
.ig .glass { border-color: #ffffff17; }
.ig .mark { display: none; position: absolute; left: 0; top: 0; width: 100%; height: 100%; border: 1.5dp #e6cb8ad9;
            border-radius: 12dp; }
.ig.marked .mark { display: block; }
.ig .hot { background-color: #ffffff1f; }
.ig .rule { background-color: #ffffff14; }
.ig .tab { background-color: #f3c653f2; }
.ig .field { background-color: #00000059; border-color: #ffffff1a; }
.ig .field.typing { border-color: #c9a55b99; }
.ig .caret { background-color: #eef2f8f2; }
.ig .cell { background-color: #00000059; border-color: #ffffff14; }
.ig .cell.hot { border-color: #c9a55bf2; }
.ig .plate { background-color: #000000b3; }
.ig .thumb { background-color: #ffffff40; }
.ig .bar { background-color: #ffffff1a; }
.ig .fill.green { background-color: #5fd08af2; }
.ig .fill.gold { background-color: #f3c653f2; }
.ig .fill.red { background-color: #ff7d6ef2; }
.ig .fresh { background-color: #f3c653f2; }
.ig .dropping { background-color: #c9a55b0f; border-color: #f3c653e6; }
.ig .flash { background-color: #f3c6534d; }
.ig .ring { border-color: #f3c653f2; }
.ig .tip { background-color: #10141cf5; border-color: #ffffff24; }
.ig .word { white-space: nowrap; overflow: hidden; }
.ig .word.bold { font-weight: bold; }
.ig .word.right { text-align: right; }
.ig .word.centre { text-align: center; }
.mm div, .mm span, .mm .a { position: absolute; }
.mm .edge { border-color: #ffffff29; }
.mm .gold { background-color: #e6cb8af2; }
.mm .btn { background-color: #0b0e14b8; border-color: #ffffff2e; }
.mm .btn.hot { background-color: #c9a55b8c; }
.mm .btn.scheme { background-color: #c9a55bbf; border-color: #e6cb8acc; }
.mm .card { border-color: #ffffff1f; }
.mm .card.list { border-color: #ffffff1a; }
.mm .card.route { border-width: 0; }
.mm .route { background-color: #c9a55b29; border-color: #c9a55b8c; }
.mm .hot { background-color: #ffffff1a; }
.mm .clear { background-color: #ffffff14; }
.mm .base { background-color: #ffffff1a; }
.mm .word { white-space: nowrap; overflow: hidden; }
.mm .word.bold { font-weight: bold; }
.mm .word.right { text-align: right; }
.mm .word.centre { text-align: center; }
.tr div, .tr span { position: absolute; }
.tr .glass { border-color: #ffffff17; }
.tr .rule { background-color: #ffffff14; }
.tr .hot { background-color: #ffffff1f; }
.tr .entry { background-color: #ffffff0a; }
.tr .entry.over { background-color: #ffffff13; }
.tr .more { background-color: #ffffff0f; }
.tr .word { white-space: nowrap; overflow: hidden; }
.tr .word.bold { font-weight: bold; }
.tr .word.right { text-align: right; }
.tr .word.centre { text-align: center; }
.mp .title, .mp .name, .mp .left, .mp .lit { position: absolute; white-space: nowrap; }
.mp .title { font-weight: bold; color: #e6cb8a; }
.mp .lit { background-color: #ffffff14; }
.mp .name { color: #9aa3b5; }
.mp .name.hot { color: #eef2f8; }
.mp .left { text-align: right; font-weight: bold; color: #eef2f8; }
.mp .left.back { color: #8fd19e; }
.hp .title, .hp .line, .hp .reset { position: absolute; white-space: nowrap; }
.hp .title { font-weight: bold; color: #e6cb8a; }
.hp .line { color: #9aa3b5; }
.hp .line.first { font-weight: bold; color: #eef2f8; }
.hp .reset { text-align: center; font-weight: bold; color: #eef2f8; background-color: #ffffff0f; }
.hp .reset.hot { background-color: #ffffff29; }
.it .toast { position: absolute; box-sizing: border-box; text-align: center; font-weight: bold; color: #f3c653;
             white-space: nowrap; background-color: #10141ceb; border-color: #f3c653b3; }
.xg .gain { position: absolute; width: 600dp; height: 22dp; line-height: 22dp; text-align: center; font-size: 15dp;
            font-weight: bold; color: #f3d27a; white-space: nowrap; font-effect: outline(2px #000000d9); }
.xb .band { position: absolute; left: 0; width: 100%; background-color: #0000008c; }
.xb .row { position: absolute; left: 0; width: 100%; }
.xb .fill { position: absolute; left: 0; top: 0; height: 100%; }
.xb .fill.base { background-color: #b7a6ff; }
.xb .fill.job { background-color: #7fd3c3; }
.xb .tick { position: absolute; top: 0; width: 1dp; height: 100%; background-color: #00000099; }
.xb .gain { position: absolute; padding: 0 6dp; height: 20dp; line-height: 20dp; font-size: 13dp; font-weight: bold;
            color: #f3d27a; white-space: nowrap; font-effect: outline(2px #000000d9); }
.xb .tip { position: absolute; box-sizing: border-box; padding: 4dp 10dp; background-color: #10141cf0;
           border: 1dp #ffffff1f; border-radius: 8dp; }
.xb .tip .line { display: block; height: 17dp; line-height: 17dp; font-size: 13dp; font-weight: bold; color: #9aa3b5;
                 white-space: nowrap; }
.xb .tip .line:first-child { color: #eceff5; }
.hb { position: absolute; font-family: Plex; color: #eceff5; font-size: 1px; }
.hb .a { position: absolute; }
.hb .glass { position: absolute; border: 1rem #ffffff17; background-color: #10141ccc; }
.hb .cell { position: absolute; background-color: #ffffff0d; border: 1rem #ffffff1a; }
.hb .cell.hot { background-color: #ffffff1a; border-color: #ffffff33; }
.hb .cell.down { background-color: #0000004d; }
.hb .cell.lit { border-color: #c9a55bd9; border-width: 2rem; }
.hb .cell.chosen, .hb .cell.under { border-color: #c9a55b; border-width: 2rem; }
.hb .cell.combo { background-color: #e6cb8a1f; border-color: #c9a55b; border-width: 2rem; }
.hb .cell.ready { background-color: #4da3ff24; border-color: #4da3fff2; border-width: 2rem; }
.hb .flash { position: absolute; border: 2rem #f0a050; }
.hb .shade { position: absolute; background-color: #00000080; }
.hb .plate { position: absolute; background-color: #0000009e; text-align: center; font-size: 11rem; font-weight: bold;
             white-space: nowrap; }
.hb .plate.sp { background-color: #3d7eebf2; color: #ffffff; }
.hb .plate.lack { background-color: #c8443af2; color: #ffffff; }
.hb .plate.timer { background-color: #000000b8; font-size: 12rem; color: #c9a55b; }
.hb .bar { position: absolute; height: 3rem; }
.hb .bar.combo { background-color: #e6cb8a; }
.hb .bar.timer { background-color: #c9a55b; }
.hb .chip { position: absolute; background-color: #c0392be6; text-align: center; font-size: 12rem; font-weight: bold; }
.hb .hint { position: absolute; font-size: 11.5rem; color: #9aa3b5; white-space: nowrap; }
.hb .rowbox { position: absolute; background-color: #ffffff0a; }
.hb .rownum { position: absolute; text-align: center; font-size: 15rem; font-weight: bold; }
.hb .f12 { position: absolute; text-align: center; font-size: 10.5rem; color: #9aa3b5; }
.hb .label { position: absolute; text-align: center; font-size: 13rem; font-weight: bold; }
.hb .toggle { position: absolute; background-color: #ffffff0f; border: 1rem #c9a55b00; }
.hb .toggle.hot { background-color: #ffffff24; border-color: #c9a55bcc; }
.hb .toggle.battle { background-color: #c0392be6; }
.hb .toggle.battle.hot { background-color: #c0392b; }
.hb .profile { position: absolute; text-align: center; font-size: 10rem; font-weight: bold; color: #e6cb8a; }
.hb .pill { position: absolute; background-color: #10141cd9; border: 1rem #ffffff1f; }
.hb .pilltext { position: absolute; font-size: 12.5rem; font-weight: bold; white-space: nowrap; }
.hb .dot { position: absolute; background-color: #ffffff14; border: 1rem #ffffff4d; }
.hb .dot.on { background-color: #e6cb8a; border-width: 0; }
.hb .tip { position: absolute; border: 1rem #ffffff1f; background-color: #10141cf0; }
.hb .tipline { position: absolute; font-size: 11.5rem; white-space: nowrap; }
.hb .tipline.first { font-size: 13rem; font-weight: bold; }
.hb .ttip { position: absolute; border: 1rem #ffffff1f; background-color: #10141cf0; text-align: center;
            font-size: 12rem; white-space: nowrap; }
/* 3.0.18-beta.61: [interface] uaui_effects - gradients, soft shadows, glows (the body's class «fx»); off: none */
.fx.sh .glass, .fx.ig .glass, .fx.hb .glass, .fx .ua-window { decorator: ua-shadow(16dp #00000059); }
.fx.sh .padfocus { decorator: ua-shadow(8dp #e6cb8a59); }
.fx.hb .cell.ready { decorator: ua-shadow(6dp #4da3ff59); }
.fx.hb .cell.lit, .fx.hb .cell.combo { decorator: ua-shadow(6dp #e6cb8a4d); }
.fx.cb .hp, .fx.cb .sp, .fx.tc .hpfill, .fx.xb .fill, .fx.ig .fill, .fx .ua-fill {
    decorator: linear-gradient(180deg, #ffffff38 0%, #ffffff00 55%, #00000026 100%); }
.fx.sh .btn, .fx.ig .tab { decorator: linear-gradient(180deg, #ffffff14 0%, #ffffff00 100%); }
/* 3.0.18-beta.73: live windows (input through RmlUi; layout by flexbox) - .lw the frame, then each window's own */
.lw { position: absolute; font-family: Plex; color: #eceff5; padding: 14dp 16dp 12dp 16dp; border-radius: 12dp;
      border: 1dp #ffffff17; }
.lw.marked { border-color: #e6cb8ad9; }
.lw .head { display: block; height: 26dp; padding-right: 30dp; }
.lw .title { font-size: 15dp; font-weight: bold; }
.lw .x { position: absolute; right: 10dp; top: 10dp; width: 24dp; height: 24dp; line-height: 24dp; text-align: center;
         border-radius: 6dp; color: #969eae; font-size: 13dp; }
.lw .x:hover { background-color: #ffffff24; color: #ffffff; }
.lw .hint { display: block; margin-top: 6dp; font-size: 11.5dp; color: #9aa3b5; }
.lw .ring { position: absolute; display: none; border: 2dp #e6cb8a; border-radius: 8dp; background-color: #e6cb8a14; }
.lw .row { display: block; padding: 5dp 10dp; border-radius: 6dp; }
.lw .row:hover { background-color: #c9a55b33; }
.lw .tabs { display: flex; margin-bottom: 8dp; }
.lw .tab { display: block; margin-right: 14dp; padding-bottom: 3dp; color: #9aa3b5; border-bottom: 2dp #00000000; }
.lw .tab:hover { color: #eceff5; }
.lw .tab.on { color: #e6cb8a; font-weight: bold; border-bottom-color: #c9a55b; }
.ew { width: 424dp; }
.ew .grid { display: flex; flex-wrap: wrap; }
.ew .cell { display: block; width: 52dp; height: 62dp; margin: 0 4dp 4dp 0; border-radius: 8dp;
            background-color: #ffffff0a; border: 1dp #ffffff14; text-align: center; }
.ew .cell:hover { background-color: #c9a55b33; border-color: #c9a55bcc; }
.ew .cell img { display: block; margin: 3dp auto 0 auto; width: 40dp; height: 40dp; }
.ew .cell span { display: block; font-size: 10.5dp; color: #9aa3b5; white-space: nowrap; overflow: hidden; }
.cw { width: 520dp; }
.cw .grid { display: flex; flex-wrap: wrap; }
.cw .row { position: relative; width: 236dp; height: 24dp; line-height: 14dp; margin: 0 4dp 2dp 0;
           white-space: nowrap; overflow: hidden; }
.cw .key { color: #e6cb8a; font-weight: bold; font-size: 12dp; margin-right: 8dp; }
.cw .name { font-size: 12.5dp; }
.cw .state { position: absolute; right: 8dp; top: 5dp; font-size: 11dp; color: #9aa3b5; }
.cw .state.on { color: #eceff5; font-weight: bold; }
.mw { width: 420dp; }
.mw .msg { display: block; font-size: 13dp; line-height: 19dp; margin: 2dp 0 12dp 0; white-space: pre-line; }
.mw .btns { display: flex; justify-content: flex-end; }
.lw .btn { display: block; margin-left: 8dp; padding: 6dp 14dp; border-radius: 8dp; font-size: 12.5dp;
           background-color: #ffffff0f; border: 1dp #ffffff29; }
.lw .btn:hover { background-color: #ffffff1f; }
.lw .btn.main { background-color: #f3c65338; border-color: #f3c653cc; font-weight: bold; }
.lw .btn.main:hover { background-color: #f3c65357; }
.lw .btn .key { margin-left: 8dp; font-size: 10.5dp; color: #9aa3b5; font-weight: normal; }
.mw .field { display: block; height: 30dp; line-height: 18dp; padding: 6dp 10dp; margin-bottom: 8dp;
             border-radius: 7dp; background-color: #00000059; border: 1dp #f3c6538c; font-size: 13dp; }
.mw .caret { display: inline-block; width: 1.5dp; height: 15dp; margin-left: 1dp; background-color: #eef2f8;
             vertical-align: middle; }
.mw .quick { display: flex; margin-bottom: 10dp; }
.mw .quick .btn { margin-left: 0; margin-right: 6dp; }
.petw { width: 340dp; }
.lw .namerow { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8dp; }
.lw .name { font-size: 15dp; font-weight: bold; color: #e6cb8a; }
.lw .lv { font-size: 12.5dp; color: #9aa3b5; }
.lw .stat { display: flex; justify-content: space-between; font-size: 12dp; margin-top: 6dp; }
.lw .stat .lbl { color: #9aa3b5; }
.lw .stat .val { font-weight: bold; }
.lw .gauge { display: block; height: 7dp; margin-top: 4dp; border-radius: 4dp; background-color: #ffffff1a; }
.lw .gauge .fill { display: block; height: 7dp; border-radius: 4dp; background-color: #5fd08a; }
.lw .gauge .fill.love { background-color: #f07aa8; }
.lw .grid4 { display: flex; flex-wrap: wrap; margin-top: 12dp; }
.lw .grid4 .btn { width: 46%; margin: 0 4% 6dp 0; text-align: center; }
.lw .btn.off { color: #9aa3b5; background-color: #ffffff08; }
.macw { width: 460dp; }
.macw .mrow { display: flex; align-items: center; margin-bottom: 4dp; }
.macw .mrow .key { width: 52dp; font-size: 12dp; font-weight: bold; color: #e6cb8a; }
.lw .field { display: block; flex: 1; height: 26dp; line-height: 14dp; padding: 5dp 8dp; border-radius: 6dp;
             background-color: #00000059; border: 1dp #ffffff1a; font-size: 12.5dp; white-space: nowrap;
             overflow: hidden; }
.lw .field.typing { border-color: #c9a55b99; }
.lw .caret { display: inline-block; width: 1.5dp; height: 14dp; margin-left: 1dp; background-color: #eef2f8;
             vertical-align: middle; }
.crw { width: 380dp; }
.lw .lbl { display: block; margin: 8dp 0 4dp 0; font-size: 12dp; color: #9aa3b5; }
.lw .chips { display: flex; flex-wrap: wrap; margin-top: 4dp; }
.lw .chip { display: block; margin: 0 6dp 6dp 0; padding: 3dp 10dp; border-radius: 10dp; font-size: 12dp;
            background-color: #ffffff0f; border: 1dp #ffffff1f; }
.lw .chip:hover { background-color: #ffffff1f; }
.lw .chip.on { background-color: #c9a55b40; border-color: #e6cb8a; color: #e6cb8a; font-weight: bold; }
.lw .chips .tick { margin-right: 16dp; }
.lw .btns { display: flex; justify-content: flex-end; margin-top: 10dp; }
.roomw { width: 620dp; }
.roomw .count { margin-left: 10dp; font-size: 11.5dp; color: #9aa3b5; }
.roomw .cols { display: flex; }
.roomw .left { display: block; flex: 1; margin-right: 12dp; }
.roomw .lines { display: block; height: 240dp; overflow: hidden; margin-bottom: 6dp; }
.roomw .line { display: block; font-size: 12.5dp; line-height: 17dp; }
.roomw .right { display: block; width: 180dp; }
.lw .row.on { background-color: #c9a55b33; }
.roomw .acts { display: flex; flex-wrap: wrap; margin-top: 6dp; }
.roomw .crown { color: #e6cb8a; }
.socw { width: 400dp; }
.socw .list { display: block; height: 260dp; overflow-y: auto; margin: 4dp 0 8dp 0; }
.socw .row { display: flex; justify-content: space-between; font-size: 12.5dp; }
.socw .row.off { color: #9aa3b5; }
.socw .where { font-size: 11.5dp; color: #9aa3b5; }
.pkw { width: 400dp; }
.pkw .list { display: block; height: 280dp; overflow-y: auto; margin: 4dp 0 8dp 0; }
.pkw .row { display: flex; align-items: center; font-size: 12.5dp; min-height: 30dp; }
.pkw .row img { width: 26dp; height: 26dp; margin-right: 8dp; }
.npw { width: 560dp; }
.npw .count { margin-left: 10dp; font-size: 11.5dp; color: #9aa3b5; }
.npw .talk { display: block; margin: 4dp 0 10dp 0; }
.npw .tline { display: block; font-size: 13.5dp; line-height: 20dp; }
.npw .opts { display: flex; flex-wrap: wrap; }
.npw .opt { display: flex; align-items: center; width: 48%; margin: 0 2% 5dp 0; padding: 4dp 8dp; border-radius: 8dp;
            background-color: #ffffff0d; border: 1dp #ffffff0f; font-size: 12.5dp; }
.npw .opt:hover { background-color: #ffffff1a; }
.npw .opt.on { background-color: #c9a55b29; border-color: #f3c653d9; }
.npw .num { display: block; width: 22dp; height: 22dp; line-height: 22dp; margin-right: 8dp; text-align: center;
            border-radius: 6dp; background-color: #1b1d24f2; color: #e6cb8a; font-weight: bold; font-size: 11.5dp; }
.npw .otext { flex: 1; }
.npw .last { font-size: 10.5dp; color: #9aa3b5; margin-left: 6dp; }
.npw .foot { margin-top: 10dp; padding-top: 6dp; border-top: 1dp #ffffff12; }
.vsw { width: 480dp; }
.lw .list { display: block; max-height: 320dp; overflow-y: auto; margin: 4dp 0 8dp 0; }
.lw .ware { display: flex; align-items: center; padding: 4dp 6dp; border-radius: 6dp; margin-bottom: 2dp; }
.lw .ware.on { background-color: #c9a55b1f; }
.lw .ware img { width: 28dp; height: 28dp; margin-right: 8dp; }
.lw .wname { display: block; flex: 1; }
.lw .wname .name { display: block; font-size: 12.5dp; }
.lw .wname .stock { display: block; font-size: 10.5dp; color: #9aa3b5; }
.lw .price { font-size: 12dp; color: #e6cb8a; margin-right: 10dp; }
.lw .pm { display: block; width: 22dp; height: 22dp; line-height: 22dp; text-align: center; border-radius: 6dp;
          background-color: #ffffff14; font-weight: bold; }
.lw .pm:hover { background-color: #ffffff29; }
.lw .ware .n { display: block; width: 30dp; text-align: center; font-size: 12.5dp; font-weight: bold; }
.lw .sums { display: block; margin: 6dp 0; }
.lw .srow { display: flex; justify-content: space-between; font-size: 12.5dp; margin-bottom: 2dp; }
.lw .srow .lbl { color: #9aa3b5; }
.lw .srow .val { font-weight: bold; }
.lw .srow .val.bad { color: #ff7d6e; }
.vstw { width: 720dp; }
.vstw .cols { display: flex; }
.vstw .col { display: block; flex: 1; margin-right: 10dp; }
.vstw .list { height: 260dp; }
.vstw .field.price { flex: 0; width: 110dp; margin: 0 6dp; text-align: right; }
.ow { width: 380dp; }
.ow .orow { display: flex; align-items: center; justify-content: space-between; margin: 4dp 0; min-height: 24dp; }
.ow .orow input.range { width: 150dp; }
.tw { width: 460dp; }
.tw .count { margin-left: 10dp; font-size: 11.5dp; color: #9aa3b5; }
.tw .msg { display: block; margin: 2dp 0 12dp 0; font-size: 13dp; line-height: 19dp; }
.tw .msg p { display: block; margin-bottom: 6dp; }
.lw .foot { display: flex; justify-content: space-between; align-items: center; }
.lw .tick { display: block; font-size: 12dp; color: #9aa3b5; }
.lw .tick .box { display: inline-block; width: 13dp; height: 13dp; margin-right: 6dp; border-radius: 3dp;
                 border: 1dp #ffffff40; vertical-align: middle; }
.lw .tick.on .box { background-color: #c9a55b; border-color: #e6cb8a; }
.lw .foot .btns { display: flex; }
.pw { padding: 6dp; border-radius: 10dp; min-width: 150dp; }
.pw .row { font-size: 12.5dp; white-space: nowrap; }
.pw .sep { display: block; height: 1dp; margin: 4dp 6dp; background-color: #ffffff1f; }
.motion .lw .cell, .motion .lw .row, .motion .lw .x { transition: background-color border-color color 0.12s cubic-out; }
.glassy.lw { decorator: ua-glass(1); }
/* 3.0.18-beta.69: the screen's edges red at low HP (win/low_hp_edge) - the concept's .vign */
.lh { position: absolute; }
.lh .vign { position: absolute; left: 0; top: 0; width: 100%; height: 100%;
            decorator: radial-gradient(#ff2d2300 62%, #ff2d2366 100%); }
/* 3.0.18-beta.64: [interface] uaui_glass - the windows' frosted glass (the body's class «glassy», Direct3D 12) */
.glassy.win.sh .glass, .glassy.win.ig .glass, .glassy .ua-window { decorator: ua-glass(1); }
.fx.glassy.win.sh .glass, .fx.glassy.win.ig .glass, .fx.glassy .ua-window {
    decorator: ua-shadow(16dp #00000059), ua-glass(1); }
/* 3.0.18-beta.58: [interface] uaui_motion - the soft motion (the body's class «motion»); off: none of it */
.motion.win { transition: transform 0.16s cubic-out; }
.motion.win.away { transform: translateY(8dp); }
.motion.it { transition: opacity transform 0.2s cubic-out; }
.motion.it.away { opacity: 0; transform: translateY(-10dp); }
.motion.hb .cell, .motion.ig .cell, .motion.ig .hot, .motion.sh .cell, .motion.sh .btn, .motion.sh .tile,
.motion.sh .hot { transition: background-color border-color 0.12s cubic-out; }
.motion.hb .a { transition: transform 0.09s cubic-out; }
.motion.hb .a.hot { transform: scale(1.05); }
.motion.hb .a.down { transform: scale(0.92); }
@keyframes ua-pulse { from { opacity: 1; } to { opacity: 0.55; } }
.motion.cb .hp.low, .motion.tc .hpfill.low { animation: 0.7s sine-in-out infinite alternate ua-pulse; }
@keyframes ua-rise { from { opacity: 0; transform: translateY(10dp); } to { opacity: 1; transform: translateY(0dp); } }
.motion.xg .gain, .motion.xb .gain { animation: 0.35s cubic-out ua-rise; }
scrollbarvertical { width: 8dp; margin-left: 4dp; }
scrollbarvertical slidertrack { background-color: #ffffff0c; border-radius: 4dp; }
scrollbarvertical sliderbar { background-color: #ffffff40; border-radius: 4dp; min-height: 24dp; }
scrollbarvertical sliderbar:hover { background-color: #ffffff70; }
scrollbarvertical sliderarrowdec, scrollbarvertical sliderarrowinc { width: 0; height: 0; }
/* 3.0.18-beta.94: the things' grid live («Речі», «Склад», «Візок» - uikit/item_grid_live): its boxes are the
   window's hit places (refit), so the sizes here are the window's - 500dp wide, cells 48dp with 8dp between */
.lw.igl { width: 41.6667rem; padding: 0; font-size: 1rem; box-sizing: border-box; }
.igl .head { display: flex; align-items: center; height: 3.8333rem; padding: 0 4.3333rem 0 1.5rem; }
.igl .head .title { font-size: 1.25rem; font-weight: bold; color: #eef2f8; }
.igl .hk { margin-left: 0.6667rem; font-size: 0.9167rem; color: #7e889a; }
.igl .x { position: absolute; right: 1.5rem; top: 0.8333rem; width: 2.1667rem; height: 2.1667rem;
    line-height: 2.1667rem;
          text-align: center; border-radius: 0.5833rem; color: #9aa3b5; font-size: 1.0833rem; }
.igl .x.hot { background-color: #ffffff24; color: #ffffff; }
.igl .tabs { display: flex; height: 2.6667rem; margin: 0 1.5rem; border-bottom: 0.0833rem #ffffff1f; }
.igl .tab { display: block; position: relative; height: 2.6667rem; line-height: 2.4167rem; margin-right: 1.6667rem;
    padding: 0 0.1667rem;
            font-size: 1.0833rem; font-weight: bold; color: #7e889a; border-bottom: 0.1667rem #00000000; }
.igl .tab.hot { color: #c9d1de; }
.igl .tab.on { color: #eef2f8; border-bottom-color: #c9a55b; }
.igl .tab .new { position: absolute; right: -0.75rem; top: -0.1667rem; line-height: 1rem; font-size: 0.8333rem;
    color: #f3c653; }
.igl .search { display: flex; align-items: center; height: 2.8333rem; margin: 1rem 1.5rem 0 1.5rem;
    padding: 0 0.8333rem;
               border-radius: 0.6667rem; border: 0.0833rem #ffffff26; background-color: #0000002e; }
.igl .search.typing { border-color: #c9a55b99; }
.igl .search img { display: block; width: 1.25rem; height: 1.25rem; margin-right: 0.5833rem; }
.igl .search .q { font-size: 1.0417rem; color: #eef2f8; }
.igl .search .q.qh { font-size: 1rem; color: #7e889a; }
.igl .search .caret { display: block; width: 0.125rem; height: 1.3333rem; margin-left: 0.0833rem;
    background-color: #eef2f8; }
.igl .gridbox { display: block; position: relative; width: 36.6667rem; height: 22.6667rem;
    margin: 1.1667rem auto 0 auto; }
.igl .grid { display: flex; flex-wrap: wrap; width: 37.5rem; }
.igl .cell { display: flex; position: relative; justify-content: center; align-items: center; box-sizing: border-box;
             width: 4rem; height: 4rem; margin: 0 0.6667rem 0.6667rem 0; border-radius: 0.6667rem;
             border: 0.0833rem #ffffff14;
             background-color: #ffffff08;
             transition: background-color border-color 0.12s quadratic-out; }
.igl .cell.thing { background-color: #ffffff0d; }
.igl .cell.hot { border: 0.125rem #c9a55bcc; background-color: #c9a55b24; }
.igl .cell img { display: block; }
.igl .cell .cnt { position: absolute; right: 0.1667rem; bottom: 0.1667rem; height: 1rem; line-height: 1rem;
    padding: 0 0.25rem;
                  border-radius: 0.25rem; background-color: #000000a6; font-size: 0.875rem; font-weight: bold;
                  color: #eef2f8; }
.igl .cell .ref, .igl .cell .mk { position: absolute; left: 0.1667rem; top: 0.1667rem; height: 1rem; line-height: 1rem;
                  padding: 0 0.25rem; border-radius: 0.25rem; background-color: #000000a6; font-size: 0.875rem;
                  font-weight: bold; color: #f3c653; }
.igl .cell .mk.q { color: #9aa3b5; }
.igl .cell .mk.e { color: #5fd08a; }
.igl .cell .fresh { position: absolute; right: -0.25rem; top: -0.4167rem; height: 1.0833rem; line-height: 1.0833rem;
    padding: 0 0.3333rem;
                    border-radius: 0.5417rem; background-color: #f3c653; color: #10141c; font-size: 0.7917rem;
                    font-weight: bold; }
.igl .cell .star { position: absolute; left: 0.1667rem; bottom: 0.1667rem; width: 1.0833rem; height: 1.0833rem;
    line-height: 1.0833rem;
                   text-align: center; font-size: 0.9167rem; color: #9aa3b5; }
.igl .cell .star.on { color: #f3c653; }
.igl .cell .flash { position: absolute; left: 0; top: 0; right: 0; bottom: 0; border-radius: 0.6667rem;
                    background-color: #f3c6534d; }
.igl .cell .ring { display: block; position: absolute; left: -0.3333rem; top: -0.3333rem; right: -0.3333rem;
    bottom: -0.3333rem;
    border-radius: 0.9167rem;
                   border: 0.1667rem #f3c653; }
.igl .cell .rise { position: absolute; left: -2.5rem; right: -2.5rem; top: -1.3333rem; text-align: center;
    font-size: 1rem;
                   font-weight: bold; }
.igl .thumb { position: absolute; right: -0.6667rem; width: 0.25rem; border-radius: 0.125rem;
    background-color: #ffffff59; }
.igl .drop { position: absolute; left: -0.3333rem; top: -0.3333rem; right: -0.3333rem; bottom: -0.3333rem;
    border-radius: 0.8333rem;
             border: 0.1667rem #f3c653; background-color: #f3c65314; }
.igl .foot { display: flex; justify-content: flex-start; align-items: center; height: 3.6667rem; margin-top: 0.5rem;
    padding: 0 1.5rem;
             border-top: 0.0833rem #ffffff1f; font-size: 1rem; }
.igl .foot .w { color: #9aa3b5; margin-right: 1rem; }
.igl .foot .also { color: #f3c653; font-weight: bold; }
.igl .bar { display: block; width: 10.8333rem; height: 0.5rem; border-radius: 0.25rem; background-color: #ffffff1a; }
.igl .bar .fill { display: block; height: 0.5rem; border-radius: 0.25rem; background-color: #5fd08a; }
.igl .bar .fill.gold { background-color: #f3c653; }
.igl .bar .fill.red { background-color: #ff7d6e; }
.igl .foot .wt { margin-left: 1rem; color: #c9d1de; }
.igl .foot .z { margin-left: auto; font-weight: bold; color: #eef2f8; }
.igl .tip { position: absolute; padding: 0.3333rem 0.9167rem 0.4167rem 0.9167rem; border-radius: 0.6667rem;
    border: 0.0833rem #ffffff2e;
            background-color: #0b0e14f2; white-space: nowrap; }
.igl .tip .t1 { display: block; font-size: 1.0833rem; font-weight: bold; color: #eef2f8; line-height: 1.5833rem; }
.igl .tip .t2 { display: block; font-size: 0.9583rem; color: #9aa3b5; line-height: 1.4167rem; }
.igl .tip .t3 { display: block; font-size: 0.9583rem; font-weight: bold; color: #f2cf7a; line-height: 1.4167rem; }
.igl .tip .t4 { display: block; font-size: 0.9583rem; color: #9fe07c; line-height: 1.4167rem; }
/* 3.0.18-beta.95: «Обмін» live (uikit/trade_live) - two sides of five cells a row; its boxes are its hit places */
.lw.trl { width: 43.3333rem; padding: 0; font-size: 1rem; box-sizing: border-box; }
.trl .head { display: flex; align-items: center; height: 3.8333rem; padding: 0 4.3333rem 0 1.5rem; }
.trl .head .title { font-size: 1.25rem; font-weight: bold; color: #eef2f8; }
.trl .x { position: absolute; right: 1.5rem; top: 0.8333rem; width: 2.1667rem; height: 2.1667rem;
    line-height: 2.1667rem;
          text-align: center; border-radius: 0.5833rem; color: #9aa3b5; font-size: 1.0833rem; }
.trl .x.hot { background-color: #ffffff24; color: #ffffff; }
.trl .sides { display: flex; justify-content: space-between; padding: 0 1.5rem; }
.trl .side { display: block; width: 19.5rem; }
.trl .side.them { border-left: 0.0833rem #ffffff1f; padding-left: 0.6667rem; width: 20.1667rem;
    margin-left: -0.6667rem; }
.trl .shead { display: flex; align-items: center; justify-content: space-between; height: 2.1667rem; }
.trl .who { font-size: 1.0833rem; font-weight: bold; color: #9aa3b5; }
.trl .them .who { color: #eef2f8; }
.trl .ready { padding: 0.1667rem 0.5833rem; border-radius: 0.6667rem; border: 0.0833rem #5fd08a99;
    background-color: #5fd08a26;
              font-size: 0.9167rem; font-weight: bold; color: #5fd08a; }
.trl .cells { display: flex; flex-wrap: wrap; position: relative; width: 20.0833rem; height: 7.3333rem;
    margin: 0.5rem 0 0 0.0833rem; }
.trl .dropframe { position: absolute; left: -0.3333rem; top: -0.3333rem; width: 20rem; height: 8rem;
    border-radius: 0.8333rem;
                  border: 0.1667rem #f3c653; background-color: #f3c65314; box-sizing: border-box; }
.trl .cell { display: flex; position: relative; justify-content: center; align-items: center; box-sizing: border-box;
             width: 3.3333rem; height: 3.3333rem; margin: 0 0.6667rem 0.6667rem 0; border-radius: 0.5833rem;
             border: 0.0833rem #ffffff14;
             background-color: #ffffff08; transition: background-color border-color 0.12s quadratic-out; }
.trl .cell.thing { background-color: #ffffff0d; }
.trl .cell.hot { border: 0.125rem #c9a55bcc; background-color: #c9a55b24; }
.trl .cell.late { border: 0.125rem #ff7d6ecc; background-color: #ff7d6e24; }
.trl .cell img { display: block; }
.trl .cell .cnt { position: absolute; right: 0.1667rem; bottom: 0.1667rem; height: 0.9167rem; line-height: 0.9167rem;
    padding: 0 0.25rem;
                  border-radius: 0.25rem; background-color: #000000a6; font-size: 0.8333rem; font-weight: bold; }
.trl .cell .ref { position: absolute; left: 0.1667rem; top: 0.1667rem; height: 0.9167rem; line-height: 0.9167rem;
    padding: 0 0.25rem;
                  border-radius: 0.25rem; background-color: #000000a6; font-size: 0.8333rem; font-weight: bold;
                  color: #f3c653; }
.trl .zrow { display: flex; align-items: center; height: 2.8333rem; margin-top: 0.1667rem; }
.trl .zl { width: 4.6667rem; color: #9aa3b5; }
.trl .zfield { display: flex; flex: 1; align-items: center; justify-content: flex-end; height: 2.3333rem;
               padding: 0 0.8333rem 0 0.6667rem; border-radius: 0.5833rem; border: 0.0833rem #ffffff26;
               background-color: #0000002e; }
.trl .zfield.typing { border-color: #c9a55b99; }
.trl .zv { font-size: 1.0417rem; font-weight: bold; color: #eef2f8; }
.trl .zv.them { flex: 1; text-align: right; color: #f3c653; padding-right: 0.6667rem; }
.trl .zv.them.late { color: #ff7d6e; }
.trl .caret { display: block; width: 0.125rem; height: 1.1667rem; margin-left: 0.0833rem; background-color: #eef2f8; }
.trl .warn { display: block; height: 1.8333rem; line-height: 1.8333rem; margin: 0.3333rem 1.5rem 0 1.5rem;
    border-radius: 0.5rem;
             text-align: center; font-weight: bold; color: #ff7d6e; border: 0.0833rem #ff7d6e66;
             background-color: #ff7d6e1a; }
.trl .hide { visibility: hidden; }
.trl .btns { display: flex; justify-content: space-between; margin: 0.3333rem 1.5rem 0 1.5rem; }
.trl .btn { display: block; box-sizing: border-box; width: 12.8333rem; height: 2.8333rem; line-height: 2.6667rem;
    margin: 0;
            padding: 0; text-align: center; border-radius: 0.6667rem; border: 0.0833rem #ffffff26;
            background-color: #ffffff12;
            font-size: 1.0833rem; font-weight: bold; color: #eef2f8; }
.trl .btn.hot { background-color: #ffffff24; }
.trl .btn.main { background-color: #f3c653; border-color: #f3c653; color: #10141c; }
.trl .btn.main.hot { background-color: #f6d473; }
.trl .btn.off { color: #7e889a; background-color: #ffffff08; }
.trl .hint { display: block; height: 2rem; line-height: 2rem; margin: 0.3333rem 1.5rem 0.5rem 1.5rem;
    text-align: center;
             font-size: 0.9583rem; color: #7e889a; }
.trl .tip { position: absolute; padding: 0.3333rem 0.9167rem 0.4167rem 0.9167rem; border-radius: 0.6667rem;
    border: 0.0833rem #ffffff2e;
            background-color: #0b0e14f2; white-space: nowrap; }
.trl .tip .t1 { display: block; font-size: 1.0833rem; font-weight: bold; color: #eef2f8; line-height: 1.5833rem; }
.trl .tip .t2 { display: block; font-size: 0.9583rem; color: #9aa3b5; line-height: 1.4167rem; }
/* 3.0.18-beta.96: «Спорядження» live (uikit/equip_live) - 880dp: two columns of five slots (56dp every 64dp) round
   a 220dp middle; its boxes are its hit places */
.lw.eql { width: 73.3333rem; padding: 0; font-size: 1rem; box-sizing: border-box; }
.eql .head { display: flex; align-items: center; height: 3.8333rem; padding: 0 4.3333rem 0 1.3333rem; }
.eql .head .title { font-size: 1.25rem; font-weight: bold; color: #eef2f8; }
.eql .hk { margin-left: 0.6667rem; font-size: 0.9167rem; color: #7e889a; }
.eql .x { position: absolute; right: 1.3333rem; top: 0.8333rem; width: 2.1667rem; height: 2.1667rem;
    line-height: 2.1667rem;
          text-align: center; border-radius: 0.5833rem; color: #9aa3b5; font-size: 1.0833rem; }
.eql .x.hot { background-color: #ffffff24; color: #ffffff; }
.eql .cols { display: flex; justify-content: space-between; padding: 0.8333rem 1.3333rem 0 1.3333rem; height: 26rem; }
.eql .col { display: block; width: 26.1667rem; }
.eql .slot { display: flex; align-items: center; box-sizing: border-box; height: 4.6667rem; margin-bottom: 0.6667rem;
             padding: 0 0.5rem; border-radius: 0.6667rem; border: 0.125rem #00000000;
             transition: background-color border-color 0.12s quadratic-out; }
.eql .left .slot { flex-direction: row-reverse; }
.eql .slot.hot { background-color: #ffffff0f; }
.eql .slot.focus { border-color: #f3c653e6; background-color: #c9a55b14; }
.eql .slot.fits { border: 0.1667rem #f3c653f2; background-color: #c9a55b1f; }
.eql .tile { display: flex; justify-content: center; align-items: center; box-sizing: border-box; width: 3.6667rem;
             height: 3.6667rem; margin: 0 0.1667rem; border-radius: 0.6667rem; border: 0.0833rem #ffffff14;
             background-color: #1b1d24e6; }
.eql .tile img { display: block; }
.eql .txt { display: block; flex: 1; margin: 0 0.8333rem; overflow: hidden; }
.eql .left .txt { text-align: right; }
.eql .lbl { display: block; margin: 0; height: 1.3333rem; line-height: 1.3333rem; font-size: 0.9583rem;
    color: #7e889a; }
.eql .nm { display: block; height: 1.6667rem; line-height: 1.6667rem; font-size: 1.0833rem; color: #7e889a;
    white-space: nowrap; }
.eql .nm.worn { font-weight: bold; color: #eef2f8; }
.eql .nm.spill { font-weight: normal; color: #7e889a; }
.eql .dots { display: flex; height: 0.5833rem; margin-top: 0.0833rem; }
.eql .left .dots { justify-content: flex-end; }
.eql .dot { display: block; width: 0.5833rem; height: 0.5833rem; margin-right: 0.25rem; border-radius: 0.2917rem;
            border: 0.0833rem #ffffff40; box-sizing: border-box; }
.eql .dot.card { background-color: #f3c653; border-color: #f3c653; }
.eql .centre { display: block; position: relative; width: 18.3333rem; height: 26rem; }
.eql .me { display: flex; flex-direction: column; align-items: center; padding-top: 0.3333rem; }
.eql .face { display: flex; justify-content: center; align-items: center; width: 8rem; height: 8rem;
             border-radius: 4rem; background-color: #1b1d24e6; border: 0.0833rem #ffffff14; box-sizing: border-box; }
.eql .who { display: block; margin-top: 0.5rem; height: 1.6667rem; line-height: 1.6667rem; font-size: 1.1667rem;
    font-weight: bold;
            color: #eef2f8; }
.eql .job { display: block; height: 1.3333rem; line-height: 1.3333rem; font-size: 1rem; color: #8fb8f0; }
.eql .lv { display: block; height: 1.3333rem; line-height: 1.3333rem; font-size: 0.9583rem; color: #9aa3b5; }
.eql .hide { visibility: hidden; }
.eql .stats { display: flex; flex-wrap: wrap; width: 17rem; margin-top: 0.5rem; }
.eql .st { display: flex; justify-content: space-between; width: 7.8333rem; margin: 0 0.3333rem; height: 1.3333rem;
           line-height: 1.3333rem; }
.eql .sn { font-size: 0.9167rem; color: #7e889a; }
.eql .sv { font-size: 0.9583rem; font-weight: bold; color: #eef2f8; }
.eql .slot.ammo { position: absolute; left: 0.5rem; right: 0.5rem; bottom: 0.1667rem; height: 4rem; margin: 0;
    padding: 0 0.1667rem; }
.eql .ammo .nm { font-size: 1.0417rem; }
.eql .picks { display: block; position: absolute; left: 0; top: 0; right: 0; bottom: 0; z-index: 1;
              padding: 0 0.5rem; border-radius: 0.8333rem; border: 0.0833rem #ffffff1f; background-color: #10141cf5; }
.eql .ptitle { display: block; height: 2rem; line-height: 2rem; margin: 0.3333rem 0.3333rem 0.1667rem 0.3333rem;
    font-size: 1.0417rem;
               font-weight: bold; color: #eef2f8; }
.eql .pnone { display: block; margin: 0.5rem 0.3333rem; color: #7e889a; }
.eql .pick { display: flex; align-items: center; box-sizing: border-box; height: 2.6667rem; margin-bottom: 0.1667rem;
             border-radius: 0.5rem; border: 0.125rem #00000000; }
.eql .pick.hot { background-color: #ffffff12; }
.eql .pick.focus { border-color: #f3c653e6; }
.eql .pic { display: flex; justify-content: center; align-items: center; width: 2.5rem; height: 2.5rem; }
.eql .pn { margin-left: 0.5rem; font-size: 1.0417rem; color: #eef2f8; white-space: nowrap; }
.eql .foot { display: flex; align-items: center; justify-content: space-between; height: 3.3333rem; margin-top: 1rem;
             padding: 0 1.3333rem; border-top: 0.0833rem #ffffff1f; font-size: 0.9583rem; color: #7e889a; }
.eql .pads { display: flex; align-items: center; }
.eql .pad { display: flex; align-items: center; margin-left: 1.3333rem; font-size: 1rem; color: #eef2f8; }
.eql .pad img { display: block; margin-right: 0.5rem; }
/* 3.0.18-beta.97: «Навички» live (uikit/skills_live) - 982dp (the tree's 1000dp): the list's eight rows of 46dp or the
   tree's view, the side panel 268dp; the tree and the panel's lines keep core skillwin's places (absolute) */
.lw.skl { width: 81.8333rem; padding: 0; font-size: 1rem; box-sizing: border-box; }
.lw.skl.treeon { width: 83.3333rem; }
.skl .head { display: flex; align-items: center; height: 3.8333rem; padding: 0 4.3333rem 0 1.3333rem; }
.skl .head .title { font-size: 1.25rem; font-weight: bold; color: #eef2f8; }
.skl .hk { margin-left: 0.6667rem; font-size: 0.9167rem; color: #7e889a; }
.skl .gap { display: block; flex: 1; }
.skl .hbt { display: block; height: 2rem; line-height: 1.8333rem; padding: 0 0.9167rem; border-radius: 0.5833rem;
    font-size: 1rem;
           color: #9aa3b5; box-sizing: border-box; }
.skl .hbt.hot { background-color: #ffffff12; }
.skl .hbt.on { background-color: #ffffff24; color: #eef2f8; font-weight: bold; }
.skl .hbt.plan { margin-right: 0.8333rem; border: 0.0833rem #ffffff29; background-color: #ffffff08; }
.skl .hbt.v { border: 0.0833rem #ffffff29; background-color: #ffffff08; }
.skl .hbt.v.l { border-right-width: 0; border-top-right-radius: 0; border-bottom-right-radius: 0; }
.skl .hbt.v.r { border-left-width: 0; border-top-left-radius: 0; border-bottom-left-radius: 0; }
.skl .hbt.plan.on { background-color: #6ea3ff3d; border-color: #6ea3ffcc; }
.skl .x { position: absolute; right: 1.3333rem; top: 0.8333rem; width: 2.1667rem; height: 2.1667rem;
    line-height: 2.1667rem;
          text-align: center; border-radius: 0.5833rem; color: #9aa3b5; font-size: 1.0833rem; }
.skl .x.hot { background-color: #ffffff24; color: #ffffff; }
.skl .tabs { display: flex; height: 2.6667rem; margin: 0 1.3333rem; border-bottom: 0.0833rem #ffffff14; }
.skl .tab { display: block; height: 2.6667rem; line-height: 2.5rem; padding: 0 0.8333rem; margin-right: 0.3333rem;
    border-radius: 0.5rem;
            font-size: 1.0417rem; color: #9aa3b5; border-bottom: 0.1667rem #00000000; box-sizing: border-box; }
.skl .tab.hot { background-color: #ffffff0d; }
.skl .tab.on { color: #eef2f8; font-weight: bold; border-bottom-color: #f3c653; }
.skl .notice { display: block; height: 2.3333rem; line-height: 2.1667rem; margin: 0.5rem 1.3333rem 0 1.3333rem;
    padding: 0 0.8333rem;
               border-radius: 0.5833rem; border: 0.0833rem #c9a55b73; background-color: #c9a55b1f; color: #eef2f8;
               box-sizing: border-box; white-space: nowrap; overflow: hidden; }
.skl .choices { display: flex; margin: 0.5rem 1.3333rem 0 1.3333rem; }
.skl .choice { display: block; height: 2.1667rem; line-height: 2rem; padding: 0 1.0833rem; margin-right: 0.5rem;
    border-radius: 0.5833rem;
               border: 0.0833rem #ffffff29; background-color: #ffffff0a; color: #9aa3b5; box-sizing: border-box; }
.skl .choice.hot { background-color: #ffffff1a; }
.skl .choice.on { border-color: #f3c653cc; background-color: #c9a55b38; color: #eef2f8; font-weight: bold; }
.skl .body { display: flex; margin: 0.5rem 1.3333rem 0 1.3333rem; height: 30.6667rem; }
.skl .area { display: block; position: relative; flex: 1; height: 30.6667rem; }
.skl .rows { display: block; }
.skl .rows.scrolls { padding-right: 1rem; }
.skl .row { display: flex; align-items: center; position: relative; box-sizing: border-box; height: 3.8333rem;
            padding: 0 0.6667rem 0 0.5rem; border-radius: 0.5rem; border: 0.125rem #00000000;
            border-bottom: 0.0833rem #ffffff0d; transition: background-color 0.12s quadratic-out; }
.skl .row.last { border-bottom-color: #00000000; }
.skl .row.unused { visibility: hidden; }
.skl .row.hot { background-color: #ffffff0f; }
.skl .row.focus { border: 0.125rem #f3c653e6; background-color: #c9a55b14; }
.skl .row.locktop { border-top: 0.0833rem #c9a55b8c; }
.skl .ic { display: flex; justify-content: center; align-items: center; width: 2.6667rem; height: 2.6667rem; }
.skl .ic img { display: block; }
.skl .tx { display: block; flex: 1; margin-left: 0.8333rem; overflow: hidden; white-space: nowrap; }
.skl .nm { display: block; height: 1.5833rem; line-height: 1.5833rem; font-size: 1.125rem; font-weight: bold;
    color: #eef2f8; }
.skl .nm.un { color: #9aa3b5; }
.skl .nm.lock { color: #7e889a; }
.skl .inf { display: block; height: 1.3333rem; line-height: 1.3333rem; font-size: 0.9583rem; color: #9aa3b5; }
.skl .use { display: flex; align-items: center; margin-left: 0.8333rem; }
.skl .ar { display: block; width: 1.5rem; height: 1.8333rem; line-height: 1.8333rem; text-align: center;
    border-radius: 0.4167rem;
           font-size: 1.1667rem; font-weight: bold; color: #7e889a; background-color: #ffffff0f; }
.skl .ar.on { color: #eef2f8; }
.skl .ar.hot { background-color: #ffffff29; }
.skl .un { display: block; width: 2.1667rem; text-align: center; font-size: 1rem; font-weight: bold; color: #f3c653; }
.skl .minus { display: block; width: 2rem; height: 2rem; line-height: 1.8333rem; margin-left: 0.8333rem;
    text-align: center;
              border-radius: 0.5rem; border: 0.0833rem #6ea3ffb3; background-color: #ffffff1a; font-size: 1.1667rem;
              font-weight: bold; box-sizing: border-box; }
.skl .minus.hot { background-color: #ffffff38; }
.skl .dots { display: flex; justify-content: flex-end; width: 8.0833rem; margin-left: 1.1667rem; }
.skl .dot { display: block; width: 0.5833rem; height: 0.5833rem; margin-left: 0.25rem; border-radius: 0.2917rem;
            background-color: #ffffff24; }
.skl .dot.lv { background-color: #f3c653; }
.skl .dot.plan { background-color: #6ea3ff; }
.skl .lvl { display: block; width: 3.8333rem; margin-left: 0.8333rem; text-align: right; font-size: 1.0417rem;
    color: #7e889a; }
.skl .lvl.learned { color: #eef2f8; }
.skl .lvl.planned { color: #6ea3ff; font-weight: bold; }
.skl .pl { display: block; width: 2rem; height: 2rem; margin-left: 0.6667rem; }
.skl .plus { display: block; width: 2rem; height: 2rem; line-height: 1.8333rem; text-align: center;
    border-radius: 0.5rem;
             border: 0.0833rem #c9a55bb3; background-color: #ffffff1f; font-size: 1.25rem; font-weight: bold;
             box-sizing: border-box; }
.skl .plus.plan { border-color: #6ea3ffb3; }
.skl .plus.hot { background-color: #ffffff38; }
.skl .thumb { position: absolute; right: 0; top: 0.3333rem; bottom: 0.3333rem; width: 0.4167rem;
    border-radius: 0.2083rem;
              background-color: #ffffff0d; }
.skl .thumb .th { position: absolute; left: 0; width: 0.4167rem; border-radius: 0.2083rem;
    background-color: #ffffff47; }
.skl .treebox { display: block; position: relative; }
.skl .view { display: block; position: relative; overflow: hidden; }
.skl .ln, .skl .nd, .skl .bar { position: absolute; }
.skl .nd { box-sizing: border-box; }
.skl .ni img { position: absolute; }
.skl .w { position: absolute; white-space: nowrap; overflow: hidden; }
.skl .w.bold { font-weight: bold; }
.skl .w.right { text-align: right; }
.skl .w.centre { text-align: center; }
.skl .side { display: block; position: relative; width: 22.3333rem; height: 30.6667rem; margin-left: 1.1667rem;
    border-radius: 0.75rem;
             border: 0.0833rem #ffffff17; background-color: #ffffff09; box-sizing: border-box; }
.skl .foot { display: flex; justify-content: flex-start; align-items: center; height: 3.3333rem; margin-top: 0.5rem;
    padding: 0 1.3333rem;
             border-top: 0.0833rem #ffffff14; }
.skl .pts { font-size: 1rem; font-weight: bold; color: #7e889a; }
.skl .pts.gold { color: #f3c653; }
.skl .pts.plan { color: #6ea3ff; }
.skl .drag { margin-left: auto; font-size: 0.9583rem; color: #7e889a; }
.skl .pads { display: flex; align-items: center; margin-left: auto; }
.skl .pad { display: flex; align-items: center; margin-left: 1.3333rem; font-size: 1rem; color: #eef2f8; }
.skl .pad img { display: block; margin-right: 0.5rem; }
.skl .pbtns { display: flex; margin-left: auto; }
.skl .fb { display: block; height: 2.1667rem; line-height: 2rem; padding: 0 1rem; margin-left: 0.6667rem;
    border-radius: 0.5833rem;
           border: 0.0833rem #ffffff2e; background-color: #ffffff0d; font-size: 1rem; color: #9aa3b5;
           box-sizing: border-box; }
.skl .fb.hot { background-color: #ffffff1f; }
.skl .fb.go { border-color: #6ea3ff4d; background-color: #6ea3ff14; color: #7e889a; font-weight: bold; }
.skl .fb.go.on { border-color: #6ea3ffe6; background-color: #6ea3ff52; color: #eef2f8; }
/* 3.0.18-beta.98: the homunculus' and the mercenary's cards live (uikit/companion_live) - 320dp */
.lw.cmp { width: 26.6667rem; padding: 1.3333rem 1.3333rem 0.6667rem 1.3333rem; font-size: 1rem;
    box-sizing: border-box; }
.cmp .head { display: block; height: 2.5rem; line-height: 2.5rem; padding-right: 2.3333rem; }
.cmp .head .title { font-size: 1.25rem; font-weight: bold; color: #eef2f8; }
.cmp .x { position: absolute; right: 1.3333rem; top: 1.5833rem; width: 2rem; height: 2rem; line-height: 2rem;
          text-align: center; border-radius: 0.5rem; color: #eef2f8; font-size: 1rem; background-color: #ffffff0f; }
.cmp .x.hot { background-color: #ffffff24; }
.cmp .nrow { display: flex; justify-content: space-between; align-items: center; height: 2rem;
    margin: 0.1667rem 0 0.5rem 0; }
.cmp .cn { font-size: 1.25rem; font-weight: bold; color: #e6cb8a; }
.cmp .clv { font-size: 1.0417rem; color: #9aa3b5; }
.cmp .b { display: block; margin-bottom: 0.5833rem; }
.cmp .bl { display: flex; justify-content: space-between; height: 1.3333rem; line-height: 1.3333rem;
    margin-bottom: 0.0833rem;
           font-size: 0.9583rem; }
.cmp .k { color: #9aa3b5; }
.cmp .v { color: #eef2f8; }
.cmp .track { display: block; height: 0.5833rem; border-radius: 0.2917rem; background-color: #ffffff14; }
.cmp .track .fill { display: block; height: 0.5833rem; border-radius: 0.2917rem; }
.cmp .fill.bhp { background-color: #4cc777e6; }
.cmp .fill.bsp { background-color: #3d7eebe6; }
.cmp .fill.bexp { background-color: #c9a55be6; }
.cmp .fill.bfull { background-color: #8bc34ae6; }
.cmp .facts { display: block; margin-bottom: 0.6667rem; }
.cmp .f { display: flex; justify-content: space-between; height: 1.5rem; line-height: 1.5rem; font-size: 0.9583rem; }
.cmp .fv { font-size: 1rem; color: #eef2f8; }
.cmp .fv.love { font-weight: bold; color: #f06b8b; }
.cmp .fv.gold { font-weight: bold; color: #e6cb8a; }
.cmp .cstats { display: flex; flex-wrap: wrap; justify-content: space-between; margin-bottom: 0.8333rem; }
.cmp .cst { display: flex; justify-content: space-between; width: 11.6667rem; height: 1.5rem; line-height: 1.5rem;
            font-size: 0.9583rem; }
.cmp .cst .v { font-size: 1rem; font-weight: bold; }
.cmp .feed { display: block; height: 2.8333rem; line-height: 2.6667rem; margin-bottom: 0.8333rem; text-align: center;
             border-radius: 0.6667rem; border: 0.0833rem #c9a55b80; background-color: #c9a55b38; font-size: 1.0417rem;
             font-weight: bold; color: #eef2f8; box-sizing: border-box; }
.cmp .feed.hot { background-color: #c9a55b57; }
.cmp .feed.focus { border: 0.125rem #c9a55bf2; }
.cmp .shd { display: block; height: 1.6667rem; line-height: 1.5rem; margin-bottom: 0.3333rem; font-size: 1rem;
    font-weight: bold;
            color: #e6cb8a; border-bottom: 0.0833rem #c9a55b73; box-sizing: border-box; }
.cmp .shd.focus { color: #f3c653; }
.cmp .sks { display: block; }
.cmp .sk { display: flex; align-items: center; height: 1.6667rem; margin-bottom: 0.1667rem;
    padding: 0 0.3333rem 0 0.5rem;
           border-radius: 0.5rem; border: 0.125rem #00000000; box-sizing: border-box; }
.cmp .sk.hot { background-color: #ffffff12; }
.cmp .sk.pad { border-color: #c9a55be6; }
.cmp .skn { display: block; flex: 1; font-size: 1rem; color: #eef2f8; white-space: nowrap; overflow: hidden; }
.cmp .skn.dash { color: #9aa3b5; }
.cmp .ar { display: block; width: 1.3333rem; height: 1.3333rem; line-height: 1.3333rem; text-align: center;
    border-radius: 0.4167rem;
           font-size: 1rem; font-weight: bold; color: #eef2f8; background-color: #ffffff0f; }
.cmp .ar.hot { background-color: #ffffff29; }
.cmp .ar.off { visibility: hidden; }
.cmp .slv { display: block; width: 4.8333rem; text-align: center; font-size: 0.9583rem; color: #9aa3b5; }
.cmp .slv.gold { color: #e6cb8a; }
.cmp .plus { display: block; width: 1.8333rem; height: 1.5rem; line-height: 1.3333rem; margin-left: 0.3333rem;
    text-align: center;
             border-radius: 0.5rem; border: 0.0833rem #c9a55bd9; background-color: #c9a55b47; font-size: 1.0833rem;
             font-weight: bold; box-sizing: border-box; }
.cmp .plus.hot { background-color: #c9a55b73; }
.cmp .plus.off { visibility: hidden; }
.cmp .rm { display: block; width: 8.3333rem; height: 1.5rem; line-height: 1.5rem; margin: 1.3333rem auto 0 auto;
    text-align: center;
           border-radius: 0.5rem; font-size: 0.9167rem; color: #e0675c; }
.cmp .rm.hot { background-color: #e0675c1f; }
/* 3.0.18-beta.99: the NPC's shop live (uikit/shop_live) - 1000dp: the list 520dp, the cart 424dp beside it */
.lw.shl { width: 83.3333rem; padding: 0; font-size: 1rem; box-sizing: border-box; }
.shl .head { display: flex; align-items: center; height: 3.8333rem; padding: 0 4.3333rem 0 1.3333rem; }
.shl .head .title { font-size: 1.25rem; font-weight: bold; color: #eef2f8; }
.shl .pl { margin-left: 0.6667rem; font-size: 0.9583rem; color: #7e889a; }
.shl .x { position: absolute; right: 1.3333rem; top: 0.8333rem; width: 2.1667rem; height: 2.1667rem;
    line-height: 2.1667rem;
          text-align: center; border-radius: 0.5833rem; color: #9aa3b5; font-size: 1.0833rem; }
.shl .x.hot { background-color: #ffffff24; color: #ffffff; }
.shl .tabs { display: flex; align-items: center; height: 2.6667rem; margin: 0 1.3333rem;
    border-bottom: 0.0833rem #ffffff1f; }
.shl .tg { display: block; margin-right: 0.6667rem; }
.shl .tab { display: block; height: 2.6667rem; line-height: 2.4167rem; margin: 0 1.5rem 0 0; padding: 0 0.1667rem;
    font-size: 1.0833rem;
            font-weight: bold; color: #7e889a; border-bottom: 0.1667rem #00000000; box-sizing: border-box; }
.shl .tab.hot { color: #c9d1de; }
.shl .tab.on { color: #eef2f8; border-bottom-color: #f3c653f2; }
.shl .cols { display: flex; margin: 1rem 1.3333rem 0 1.3333rem; }
.shl .lcol { display: block; width: 43.3333rem; }
.shl .rcol { display: block; width: 35.3333rem; margin-left: 2rem; }
.shl .search { display: flex; align-items: center; height: 2.8333rem; padding: 0 0.8333rem; border-radius: 0.6667rem;
               border: 0.0833rem #ffffff26; background-color: #00000059; box-sizing: border-box; }
.shl .search.typing { border-color: #c9a55b99; }
.shl .search img { display: block; width: 1.25rem; height: 1.25rem; margin-right: 0.5833rem; }
.shl .search .q { font-size: 1.0417rem; color: #eef2f8; }
.shl .search .q.qh { font-size: 1rem; color: #7e889a; }
.shl .caret { display: block; width: 0.125rem; height: 1.3333rem; margin-left: 0.0833rem; background-color: #eef2f8; }
.shl .lh, .shl .ch { display: flex; align-items: center; height: 1.8333rem; margin-top: 0.6667rem;
    padding: 0 0.8333rem 0 0.5rem;
                     font-size: 0.9167rem; color: #7e889a; border-bottom: 0.0833rem #ffffff14; box-sizing: border-box; }
.shl .ch { padding-right: 0.5rem; }
.shl .hw { display: block; flex: 1; }
.shl .hp2 { display: block; width: 9.1667rem; text-align: right; margin-right: 1rem; }
.shl .hwt { display: block; width: 5.3333rem; text-align: right; }
.shl .ha { display: block; width: 10rem; text-align: center; }
.shl .hs { display: block; width: 7.5rem; text-align: right; margin: 0 3rem 0 0.8333rem; }
.shl .wlist { display: block; position: relative; height: 29.1667rem; }
.shl .cart { display: block; position: relative; height: 17.5rem; }
.shl .wait, .shl .none { display: block; margin: 0.5rem; font-size: 1.0417rem; color: #9aa3b5; }
.shl .none { font-size: 1rem; color: #7e889a; }
.shl .wr, .shl .line { display: flex; align-items: center; position: relative; height: 2.9167rem;
    padding: 0 0.8333rem 0 0.3333rem;
                       border-radius: 0.5rem; border: 0.125rem #00000000; border-bottom: 0.0833rem #ffffff0d;
                       box-sizing: border-box; }
.shl .line { padding-right: 0.5rem; }
.shl .unused { visibility: hidden; }
.shl .wr.hot, .shl .line.hot { background-color: #ffffff0f; }
.shl .wr.framed, .shl .line.framed { border: 0.125rem #f3c653e6; background-color: #c9a55b14; }
.shl .wr.incart { border-left: 0.2083rem #f3c653e6; }
.shl .ic { display: flex; justify-content: center; align-items: center; width: 2.5rem; height: 2.5rem;
    margin-right: 0.8333rem; }
.shl .ic img { display: block; }
.shl .wn, .shl .ln { display: block; flex: 1; font-size: 1.0417rem; color: #eef2f8; white-space: nowrap;
                     overflow: hidden; }
.shl .wn.unknown { color: #9aa3b5; }
.shl .mk { margin: 0 0.6667rem 0 0.5rem; font-size: 0.9583rem; font-weight: bold; color: #f3c653; }
.shl .pr { display: block; width: 9.1667rem; text-align: right; margin-right: 1rem; font-size: 1.0417rem;
    color: #eef2f8; }
.shl .wt { display: block; width: 5.3333rem; text-align: right; font-size: 1rem; color: #7e889a; }
.shl .st { display: block; width: 1.8333rem; height: 1.8333rem; line-height: 1.8333rem; text-align: center;
    border-radius: 0.4167rem;
           font-size: 1.0833rem; font-weight: bold; background-color: #ffffff12; }
.shl .st.hot { background-color: #ffffff29; }
.shl .am { display: flex; justify-content: center; align-items: center; width: 4.3333rem; height: 1.8333rem;
    margin: 0 0.3333rem;
           border-radius: 0.4167rem; border: 0.125rem #00000000; font-size: 1.0417rem; font-weight: bold;
           box-sizing: border-box; }
.shl .am.hot { background-color: #ffffff1a; }
.shl .am.typed { background-color: #00000073; border-color: #f3c653f2; }
.shl .sm { display: block; width: 7.5rem; text-align: right; margin: 0 0.6667rem 0 0.8333rem; font-size: 1.0417rem; }
.shl .dr { display: block; width: 1.8333rem; height: 1.8333rem; line-height: 1.8333rem; text-align: center;
    border-radius: 0.4167rem;
           font-size: 1.1667rem; color: #9aa3b5; }
.shl .dr.hot { background-color: #ff8a802e; color: #ff8a80; }
.shl .tots { display: block; margin-top: 0.8333rem; }
.shl .tot { display: flex; justify-content: space-between; height: 2rem; line-height: 2rem; padding-right: 0.5rem;
            font-size: 1.0833rem; }
.shl .tl { color: #9aa3b5; }
.shl .tv { font-weight: bold; color: #eef2f8; }
.shl .tv.red { color: #ff8a80; }
.shl .tv.amber { color: #f5bd4f; }
.shl .acts { display: flex; margin-top: 0.8333rem; }
.shl .go { display: flex; align-items: center; height: 3rem; padding: 0 1.1667rem 0 1.6667rem; border-radius: 0.6667rem;
           border: 0.0833rem #ffffff1a; background-color: #ffffff12; color: #7e889a; box-sizing: border-box; }
.shl .go.ok { border-color: #f3c653; background-color: #f3c653; color: #1a1408; }
.shl .go.ok.hot { background-color: #f8d57a; }
.shl .gw { font-size: 1.125rem; font-weight: bold; margin-right: 0.8333rem; }
.shl .go img { display: block; }
.shl .cap { display: block; height: 1.5rem; line-height: 1.5rem; padding: 0 0.5rem; border-radius: 0.3333rem;
    font-size: 0.9167rem;
            font-weight: bold; background-color: #ffffff1f; border: 0.0833rem #ffffff2e; box-sizing: border-box; }
.shl .go.ok .cap { background-color: #1a140829; border-color: #00000000; }
.shl .clr { display: block; height: 3rem; line-height: 3rem; margin-left: 0.8333rem; padding: 0 1.1667rem;
    border-radius: 0.6667rem;
            background-color: #ffffff12; font-size: 1.0833rem; font-weight: bold; color: #7e889a; }
.shl .clr.on { color: #eef2f8; }
.shl .clr.hot { background-color: #ffffff24; }
.shl .note { display: block; height: 1.5rem; line-height: 1.5rem; margin-top: 0.5rem; font-size: 0.9167rem;
    color: #7e889a; }
.shl .note.edit { color: #f3c653; }
.shl .foot { display: flex; justify-content: flex-start; align-items: center; height: 3.3333rem; margin-top: 0.8333rem;
             padding: 0 1.3333rem; border-top: 0.0833rem #ffffff14; }
.shl .foot.pad { justify-content: flex-end; }
.shl .key { display: flex; align-items: center; margin-right: 1.3333rem; }
.shl .foot.pad .key { margin: 0 0 0 1.3333rem; }
.shl .key img { display: block; margin-right: 0.5rem; }
.shl .key .cap { margin-right: 0.5rem; color: #eef2f8; }
.shl .kw { font-size: 1rem; color: #9aa3b5; }
.shl .foot.pad .kw { color: #eef2f8; }
)";

}  // namespace

const char* builtInCss() { return kTheme; }

}  // namespace uagfx::uitheme

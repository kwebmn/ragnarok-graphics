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
/* 3.0.18-beta.221 (R-BN9TJA): every length in rem - 1rem a 14th of the block's font (14dp x its size in
   «Розміщення інтерфейсу»): the letters drawn at their size, not a picture scaled */
.cb { position: absolute; font-family: Plex; color: #eceff5; transform-origin: 0 0; }
.cb .card { position: absolute; left: 0; top: 0; border-radius: 0.714286rem; border: 0.071429rem #ffffff17;
            background-color: #10141ccc; transition: border-color 0.3s quadratic-out; }
.cb.flare .card { border-color: #e6cb8a; }
.cb .abs { position: absolute; }
.cb .portrait { position: absolute; border-radius: 999px; background-color: #1b1d24; overflow: hidden;
                cursor: pointer; }
.cb .portrait img { position: absolute; left: 7%; top: 7%; width: 86%; height: 86%; }
.cb .letter { display: block; width: 100%; text-align: center; font-weight: bold; color: #eceff5; }
.cb .name { font-size: 1.142857rem; font-weight: bold; }
.cb .job { font-size: 0.928571rem; color: #9aa3b5; margin-left: 0.571429rem; }
.cb .levels { position: absolute; right: 0.857143rem; top: 1rem; font-size: 0.928571rem; font-weight: bold;
              color: #e6cb8a; }
.cb .line { position: absolute; height: 1.214286rem; font-size: 0.892857rem; white-space: nowrap; }
.cb .muted { color: #9aa3b5; }
.cb .gold { color: #e6cb8a; }
.cb .warn { color: #f0a050; }
.cb .bad { color: #ff5a4e; }
.cb .fight { color: #f3d27a; font-weight: bold; }
.cb .chip { margin-right: 0.857143rem; }
.cb .label { position: absolute; font-size: 0.785714rem; font-weight: bold; color: #9aa3b5; }
.cb .track { position: absolute; border-radius: 0.357143rem; background-color: #00000073; }
.cb .fill { position: absolute; left: 0; top: 0; height: 100%; border-radius: 0.357143rem; }
.cb .hp { background-color: #5bd36c; }
.cb .hp.low { background-color: #ff5a4e; }
.cb .sp { background-color: #4f8bff; }
.cb .base { background-color: #b7a6ff; }
.cb .jobx { background-color: #7fd3c3; }
.cb .weightfill { background-color: #c9a55b; }
.cb .num { position: absolute; font-size: 0.928571rem; font-weight: bold; text-align: right; }
.cb .num.low { color: #ff9a8f; }
.cb .small { position: absolute; font-size: 0.785714rem; white-space: nowrap; }
.cb .cell { position: absolute; border-radius: 0.428571rem; background-color: #1b1d24b3; }
.cb .cell .k { position: absolute; left: 0.5rem; top: 0.142857rem; font-size: 0.678571rem; font-weight: bold;
               color: #9aa3b5; }
.cb .cell .v { position: absolute; left: 0.5rem; top: 1rem; font-size: 0.892857rem; font-weight: bold; }
.cb .button { position: absolute; border-radius: 0.5rem; background-color: #1b1d24b3; cursor: pointer;
              transition: background-color 0.1s; }
.cb .button:hover { background-color: #2a2f3a; }
.cb .button:active { background-color: #3a3f4c; }
.cb .button img { position: absolute; left: 0.5rem; top: 0.285714rem; width: 1.285714rem; height: 1.285714rem; }
/* 3.0.18-beta.120 (the concept «Ігровий інтерфейс»): the second row - our windows, gold-edged while open */
.cb .button.own { border-width: 0.071429rem; border-color: #ffffff14; }
.cb .button.own.lit { border-color: #e6cb8a; background-color: #e6cb8a22; }
.cb .button.more .dots { display: block; text-align: center; line-height: 1.714286rem; font-size: 1.142857rem;
                         color: #eceff5; }
.cb .badge { position: absolute; border-radius: 0.642857rem; background-color: #10141c; border: 0.071429rem #c9a55b;
             text-align: center; font-weight: bold; font-size: 0.857143rem; }
.cb .zone { position: absolute; }
.cb .tip { position: absolute; padding: 0.285714rem 0.714286rem 0.285714rem 0.714286rem; border-radius: 0.571429rem;
           background-color: #10141cee;
           border: 0.071429rem #ffffff17; white-space: nowrap; font-size: 0.928571rem;
           margin-right: -142.857143rem; }  /* 3.0.18-beta.55 (R-PWJXJ3): as wide as its longest line, past the block */
.cb .tip p { display: block; color: #9aa3b5; line-height: 1.214286rem; }
.cb .tip p.first { color: #eceff5; font-weight: bold; }
.cb .warning { position: absolute; padding: 0 0.857143rem; height: 2.142857rem; line-height: 2.142857rem;
               border-radius: 0.571429rem;
               background-color: #3a1414; font-size: 0.928571rem; font-weight: bold; white-space: nowrap;
               transition: opacity transform 0.18s quadratic-out; }
.cb .warning.away { opacity: 0; transform: translateY(-0.571429rem); }
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
/* 3.0.18-beta.120 (the concept «Ігровий інтерфейс»): ✕ - the card hidden till the next target */
.tc .tcx { display: inline-block; vertical-align: top; margin: 13dp 0 0 8dp; width: 22dp; height: 22dp;
    line-height: 22dp; border-radius: 6dp; font-size: 12dp; font-weight: bold; color: #c9cfdb;
    background-color: #ffffff0f; }
.tc .tcx:hover { background-color: #ffffff29; color: #ffffff; }
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
.pp .count { position: absolute; right: 82dp; }
.pp .hbtn { position: absolute; top: 3dp; width: 22dp; height: 20dp; line-height: 20dp; text-align: center;
            border-radius: 5dp; color: #eceff5; cursor: pointer; }
.pp .hbtn:hover { background-color: #ffffff18; }
.pp .viewb { right: 52dp; }  /* 3.0.18-beta.123: room for ✕ (R-7TVEJU: it stood over the title) */
.pp .foldb { right: 28dp; }
.pp .closeb { right: 2dp; }
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
.hpn { position: absolute; font-family: Plex; border-color: #ffffff17; }
.mp { position: absolute; font-family: Plex; border-color: #ffffff17; }
.trk { position: absolute; font-family: Plex; }
.mm { position: absolute; font-family: Plex; }
/* 3.0.18-beta.52 (R-W9WZ4G: the signs off the round buttons' middles): an edge inside the box, as roundRect's */
.mm div, .sh div, .ig div, .trk div, .hb div, .hpn, .mp { box-sizing: border-box; }
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
.trk div, .trk span { position: absolute; }
.trk .glass { border-color: #ffffff17; }
.trk .rule { background-color: #ffffff14; }
.trk .hot { background-color: #ffffff1f; }
.trk .entry { background-color: #ffffff0a; }
.trk .entry.over { background-color: #ffffff13; }
.trk .more { background-color: #ffffff0f; }
.trk .word { white-space: nowrap; overflow: hidden; }
.trk .word.bold { font-weight: bold; }
.trk .word.right { text-align: right; }
.trk .word.centre { text-align: center; }
.mp .title, .mp .name, .mp .left, .mp .lit { position: absolute; white-space: nowrap; }
.mp .title { font-weight: bold; color: #e6cb8a; }
.mp .lit { background-color: #ffffff14; }
.mp .name { color: #9aa3b5; }
.mp .name.hot { color: #eef2f8; }
.mp .left { text-align: right; font-weight: bold; color: #eef2f8; }
.mp .left.back { color: #8fd19e; }
.hpn .title, .hpn .line, .hpn .reset { position: absolute; white-space: nowrap; }
.hpn .title { font-weight: bold; color: #e6cb8a; }
.hpn .line { color: #9aa3b5; }
.hpn .line.first { font-weight: bold; color: #eef2f8; }
.hpn .reset { text-align: center; font-weight: bold; color: #eef2f8; background-color: #ffffff0f; }
.hpn .reset.hot { background-color: #ffffff29; }
/* 3.0.18-beta.120 (the concept «Ігровий інтерфейс»: «✕ на каждом окне»): our panels' close, one look for them all */
.ownx { position: absolute; text-align: center; font-weight: bold; color: #eef2f8; background-color: #ffffff0f;
    white-space: nowrap; }
.ownx.hot { background-color: #ffffff29; }
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
/* 3.0.18-beta.140 (R-JA8NME, a clumsy look): two in a row - their padding inside the width (46% + 4% + padding was over
   100%: one a row, pushed right by .btns) */
.lw .grid4 { display: flex; flex-wrap: wrap; justify-content: space-between; margin-top: 12dp; }
.lw .grid4 .btn { box-sizing: border-box; width: 49%; margin: 0 0 6dp 0; text-align: center; }
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
.socw { width: 420dp; }
.socw .list { display: block; min-height: 40dp; max-height: 330dp; overflow-y: auto; margin: 4dp 0 8dp 0; }
.socw .row.off { color: #9aa3b5; }
/* 3.0.18-beta.141: a person a card - face, name ★ class, HP, where; quick buttons on hover; the menu under it */
.socw .shead { display: flex; align-items: center; margin: 2dp 0 4dp 0; font-size: 12dp; }
.socw .shead .lbl { flex: 1; font-weight: bold; color: #e6cb8a; }
.socw .cnt { color: #9aa3b5; margin-left: 8dp; }
.socw .sexp { color: #9aa3b5; margin-left: 8dp; }
.socw .sset { margin-left: 8dp; padding: 2dp 8dp; border-radius: 6dp; background-color: #ffffff14; }
.socw .sset:hover { background-color: #ffffff29; }
.socw .prow { display: block; margin-bottom: 3dp; }
.socw .row { display: flex; align-items: center; padding: 5dp 8dp; border-radius: 8dp; border: 1dp #00000000; }
.socw .row.pad { border-color: #f3c653d9; }
.socw .row.open { background-color: #c9a55b29; }
.socw .face { position: relative; width: 32dp; height: 32dp; margin-right: 9dp; border-radius: 16dp;
              background-color: #1b1d24f2; }
.socw .face img { width: 32dp; height: 32dp; }
.socw .face .lt { display: block; width: 32dp; line-height: 32dp; text-align: center; font-weight: bold;
                  color: #e6cb8a; font-size: 14dp; }
.socw .face .dot { position: absolute; right: -1dp; bottom: -1dp; width: 10dp; height: 10dp; border-radius: 5dp;
                   background-color: #5c6370; border: 1.5dp #10141c; }
.socw .face .dot.on { background-color: #4cc777; }
.socw .who { display: block; flex: 1; }
.socw .l1 { display: block; font-size: 12.5dp; }
.socw .l1 .name { font-size: 12.5dp; font-weight: bold; color: #eef2f8; }
.socw .row.off .l1 .name { color: #9aa3b5; }
.socw .l1 .crown { color: #e6cb8a; }
.socw .l1 .job { font-size: 11dp; color: #9aa3b5; margin-left: 6dp; }
.socw .hpb { display: block; height: 4dp; margin: 3dp 0 2dp 0; border-radius: 2dp; background-color: #ffffff14; }
.socw .hpf { display: block; height: 4dp; border-radius: 2dp; background-color: #4cc777; }
.socw .hpf.bad { background-color: #ff7d6e; }
.socw .l2 { display: block; font-size: 11dp; color: #9aa3b5; }
.socw .l2.bad { color: #ff9a8f; }
.socw .qa { display: none; }
.socw .row:hover .qa, .socw .row.pad .qa { display: flex; }
.socw .qb { margin-left: 4dp; padding: 3dp 7dp; border-radius: 6dp; font-size: 11dp; background-color: #ffffff14; }
.socw .qb:hover { background-color: #c9a55b4d; }
.socw .pmenu { display: flex; flex-wrap: wrap; padding: 4dp 6dp 2dp 47dp; }
.socw .mi { margin: 0 5dp 5dp 0; padding: 4dp 9dp; border-radius: 7dp; font-size: 11.5dp; background-color: #ffffff12;
            border: 1dp #ffffff12; }
.socw .mi:hover { background-color: #ffffff26; }
.socw .mi.on { border-color: #f3c653d9; background-color: #c9a55b29; }
.socw .addf { display: flex; align-items: center; margin-top: 4dp; }
.socw .fld { display: block; position: relative; flex: 1; margin-right: 8dp; }
.socw .fld .ph { position: absolute; left: 10dp; top: 0; line-height: 30dp; font-size: 12dp; color: #9aa3b5; }
.socw .addf .btn { margin: 0; }
.socw .sfoot { display: block; margin-top: 8dp; font-size: 10.5dp; color: #9aa3b5; }
.pkw { width: 400dp; }
.pkw .list { display: block; min-height: 40dp; max-height: 280dp; overflow-y: auto; margin: 4dp 0 8dp 0; }
.pkw .row { display: flex; align-items: center; font-size: 12.5dp; min-height: 30dp; }
.pkw .row img { width: 26dp; height: 26dp; margin-right: 8dp; }
.pkw .pwho { display: block; flex: 1; }  /* 3.0.18-beta.142: the name, what is inside under it */
.pkw .pwho .name { display: block; font-size: 12.5dp; font-weight: normal; color: #eef2f8; }
.pkw .pwho .inside { display: block; font-size: 10.5dp; color: #b8c8ff; }
.npw { width: 560dp; }
.npw .count { margin-left: 10dp; font-size: 11.5dp; color: #9aa3b5; }
.npw .talk { display: block; margin: 4dp 0 10dp 0; }
.npw .tline { display: block; font-size: 13.5dp; line-height: 20dp; }
/* 3.0.18-beta.140: two answers a row - padding and border inside the width (48% + 2% + 18dp was over half: one a
   row, the right half empty) */
.npw .opts { display: flex; flex-wrap: wrap; justify-content: space-between; }
.npw .opt { box-sizing: border-box; display: flex; align-items: center; width: 49%; margin: 0 0 5dp 0;
            padding: 4dp 8dp; border-radius: 8dp;
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
/* 3.0.18-beta.138: a ware's cards and free slots; its name opens its card */
.vsw .wname .inside, .vstw .wname .inside { display: block; font-size: 10.5dp; color: #b8c8ff; }
/* 3.0.18-beta.143: «Моя лавка» - sold counts, the sales */
.vsw .wname .sold { color: #9fe07c; }
.vsw .srow .val.good { color: #9fe07c; }
.vsw .sales { display: block; max-height: 150dp; overflow-y: auto; margin: 2dp 0 6dp 0; }
.vsw .sale { display: flex; font-size: 11.5dp; padding: 2dp 4dp; }
.vsw .sale .t { width: 40dp; color: #9aa3b5; }
.vsw .sale .what { flex: 1; }
.vsw .sale .z { color: #9fe07c; }
.vstw .all { display: block; margin-top: 4dp; font-size: 12dp; font-weight: bold; color: #e6cb8a; }
.vsw .wname:hover .name { color: #e6cb8a; }
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
/* 3.0.18-beta.104 (R-K5QG6Z «окно с ценой маленькое»): flex 0 had made the price field 0 wide (its basis 0) */
.vstw .field.price { flex: none; width: 140dp; height: 30dp; line-height: 18dp; margin: 0 8dp; text-align: right;
                     font-size: 14dp; font-weight: bold; color: #f3c653; }
.vstw .field.price.typing { background-color: #00000080; border-color: #f3c653e6; }
.vstw .wname .mkt { display: block; font-size: 10.5dp; color: #9fe07c; }
.ow { width: 380dp; }
.ow .orow { display: flex; align-items: center; justify-content: space-between; margin: 4dp 0; min-height: 24dp; }
.ow .orow input.range { width: 150dp; }
/* 3.0.18-beta.122 (the concept «Ігровий інтерфейс», mockup S5uscWfANyf6REjTWWm47C): «Налаштування» - the sections on
   the left, the search over them, a row a setting (a switch, a slider, choices) */
.ow.sw { width: 760dp; }
.sw .head .hk { margin-left: 8dp; font-size: 11dp; color: #9aa3b5; }
.sw .srch { display: block; position: relative; margin: 8dp 12dp 0 12dp; height: 30dp; border-radius: 8dp;
    border: 1dp #ffffff1a; background-color: #00000066; }
.sw .srch input { display: block; width: 100%; height: 30dp; padding: 0 10dp; line-height: 30dp; font-size: 13dp;
    color: #eef2f8; box-sizing: border-box; }
.sw .srch .ph { position: absolute; left: 10dp; top: 0; line-height: 30dp; font-size: 13dp; color: #9aa3b5; }
.sw .sbody { display: flex; flex-direction: row; height: 440dp; margin-top: 8dp; border-top: 1dp #ffffff1a; }
.sw .snav { display: block; width: 190dp; flex: none; padding: 6dp; overflow-y: auto; border-right: 1dp #ffffff1a; }
.sw .sn { display: flex; flex-direction: row; justify-content: space-between; padding: 7dp 9dp; border-radius: 7dp;
    font-size: 13dp; color: #cfd3dc; }
.sw .sn:hover { background-color: #ffffff10; }
.sw .sn.on { background-color: #e6cb8a22; color: #e6cb8a; }
.sw .sn .cnt { font-size: 11dp; color: #9aa3b5; }
.sw .spane { display: block; flex: 1; padding: 4dp 14dp 12dp 14dp; overflow-y: auto; }
.sw .sgrp { display: block; margin: 14dp 0 2dp 0; font-size: 11dp; color: #9aa3b5; }
.sw .sro { display: flex; flex-direction: row; align-items: center; padding: 7dp 0; border-bottom: 1dp #ffffff0c; }
.sw .st { display: block; flex: 1; padding-right: 12dp; }
.sw .sl { display: block; font-size: 13dp; color: #eef2f8; }
.sw .shn { display: block; font-size: 11.5dp; color: #9aa3b5; }
.sw .sctl { display: flex; flex-direction: row; align-items: center; flex: none; }
.sw .tg { display: block; position: relative; width: 38dp; height: 22dp; border-radius: 11dp;
    background-color: #ffffff22; }
.sw .tg .kn { position: absolute; left: 3dp; top: 3dp; width: 16dp; height: 16dp; border-radius: 8dp;
    background-color: #cfd3dc; }
.sw .tg.on { background-color: #3f7d53; }
.sw .tg.on .kn { left: 19dp; background-color: #ffffff; }
.sw .sctl input.range { width: 140dp; margin: 0 6dp 0 10dp; }
.sw .sv { display: block; width: 52dp; font-size: 12dp; color: #cfd3dc; text-align: center; }
.sw .chs { display: flex; flex-direction: row; flex-wrap: wrap; justify-content: flex-end; max-width: 330dp; }
.sw .ch { display: block; margin: 2dp 0 2dp 4dp; padding: 3dp 9dp; border-radius: 6dp; border: 1dp #ffffff1a;
    font-size: 12dp; color: #cfd3dc; }
.sw .ch:hover { background-color: #ffffff10; }
.sw .ch.on { border-color: #e6cb8a; color: #e6cb8a; background-color: #e6cb8a1a; }
.sw .snone { display: block; margin-top: 20dp; font-size: 13dp; color: #9aa3b5; }
.sw .sfoot { display: flex; flex-direction: row; justify-content: space-between; align-items: center;
    padding: 9dp 12dp; border-top: 1dp #ffffff1a; }
.sw .sfoot .sfh { font-size: 12dp; color: #9aa3b5; }
.sw .sfoot .btn { display: block; padding: 4dp 10dp; border-radius: 7dp; border: 1dp #e6cb8a66; color: #e6cb8a;
    font-size: 12dp; }
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
.popw { padding: 6dp; border-radius: 10dp; min-width: 150dp; }
.popw .row { font-size: 12.5dp; white-space: nowrap; }
.popw .sep { display: block; height: 1dp; margin: 4dp 6dp; background-color: #ffffff1f; }
.motion .lw .cell, .motion .lw .row, .motion .lw .x { transition: background-color border-color color 0.12s cubic-out; }
.glassy.lw { decorator: ua-glass(1); }
/* 3.0.18-beta.69: the screen's edges red at low HP (win/low_hp_edge) - the concept's .vign */
.lowhp { position: absolute; }
.lowhp .vign { position: absolute; left: 0; top: 0; width: 100%; height: 100%;
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
/* 3.0.18-beta.155: things picked with Ctrl / Shift, the footer's button for them */
.igl .cell.picked { border-color: #f3c653; background-color: #c9a55b2e; }
.igl .foot .pk { margin-right: 0.6667rem; padding: 0.1667rem 0.6667rem; border-radius: 0.5rem;
    background-color: #c9a55b40; color: #f3c653; font-weight: bold; }
/* 3.0.18-beta.144: a thing's element - a short line in its colour along the cell's top */
.igl .cell .ele { position: absolute; left: 25%; top: 0.0833rem; width: 50%; height: 0.1667rem;
                  border-radius: 0.0833rem; }
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
.lw.eql { width: 53.3333rem; padding: 0; font-size: 1rem; box-sizing: border-box; }
.eql .head { display: flex; align-items: center; height: 3.8333rem; padding: 0 4.3333rem 0 1.3333rem; }
.eql .head .title { font-size: 1.25rem; font-weight: bold; color: #eef2f8; }
.eql .hk { margin-left: 0.6667rem; font-size: 0.9167rem; color: #7e889a; }
.eql .x { position: absolute; right: 1.3333rem; top: 0.8333rem; width: 2.1667rem; height: 2.1667rem;
    line-height: 2.1667rem;
          text-align: center; border-radius: 0.5833rem; color: #9aa3b5; font-size: 1.0833rem; }
.eql .x.hot { background-color: #ffffff24; color: #ffffff; }
.eql .cols { display: flex; justify-content: space-between; padding: 0.8333rem 1.3333rem 0 1.3333rem;
    height: 21.6667rem; }
.eql .col { display: block; width: 19rem; }
.eql .slot { display: flex; align-items: center; box-sizing: border-box; height: 3.8333rem; margin-bottom: 0.5rem;
             padding: 0 0.5rem; border-radius: 0.6667rem; border: 0.125rem #00000000;
             transition: background-color border-color 0.12s quadratic-out; }
.eql .left .slot { flex-direction: row-reverse; }
.eql .slot.hot { background-color: #ffffff0f; }
.eql .slot.focus { border-color: #f3c653e6; background-color: #c9a55b14; }
.eql .slot.fits { border: 0.1667rem #f3c653f2; background-color: #c9a55b1f; }
.eql .tile { display: flex; justify-content: center; align-items: center; box-sizing: border-box; width: 3rem;
             height: 3rem; margin: 0 0.1667rem; border-radius: 0.6667rem; border: 0.0833rem #ffffff14;
             background-color: #1b1d24e6; }
.eql .tile img { display: block; }
.eql .txt { display: block; flex: 1; margin: 0 0.8333rem; overflow: hidden; }
.eql .left .txt { text-align: right; }
.eql .lbl { display: block; margin: 0; height: 1.3333rem; line-height: 1.3333rem; font-size: 0.9583rem;
    color: #7e889a; }
.eql .nm { display: block; height: 1.6667rem; line-height: 1.6667rem; font-size: 1.0833rem; color: #7e889a;
    white-space: nowrap; }
.eql .nm.worn { font-weight: bold; color: #eef2f8; }
/* 3.0.18-beta.154: the sets (the later clients' Equip Switch); .220 (the owner: «сделай явно чтобы было понятно»):
   rows - the number, the things' icons and names, «Надягти» / «✓ Надіто», «Зберегти надіте», «Очистити» */
.eql .sets { display: block; margin: 0.6667rem 1.3333rem 0 1.3333rem; }
.eql .setshead { display: flex; align-items: center; height: 1.6667rem; }
.eql .sht { display: block; font-size: 0.9583rem; font-weight: bold; color: #e6cb8a; }
.eql .setsnote { display: block; flex: 1; margin-left: 0.8333rem; font-size: 0.875rem; color: #9aa3b5;
    white-space: nowrap; overflow: hidden; }
.eql .setrow { display: flex; align-items: center; height: 2.5rem; margin-top: 0.3333rem; padding: 0 0.5rem;
    border-radius: 0.5rem; border: 0.0833rem #ffffff12; background-color: #ffffff08; }
.eql .setrow.on { border-color: #e6cb8a88; background-color: #c9a55b1a; }
.eql .setrow.focus { border-color: #f3c653e6; background-color: #c9a55b26; }  /* .225: the controller's, the keys' */
.eql .sn { display: block; width: 1.5rem; font-weight: bold; color: #e6cb8a; }
.eql .sicons { display: flex; align-items: center; }
.eql .si { display: block; margin-right: 0.1667rem; }
.eql .si img { display: block; }
.eql .stx { display: block; flex: 1; margin: 0 0.5rem; font-size: 0.9167rem; color: #e9e4ea; white-space: nowrap;
    overflow: hidden; }
.eql .stx.empty { color: #7e889a; }
.eql .sbtn { display: block; height: 1.8333rem; line-height: 1.8333rem; padding: 0 0.6667rem; margin-left: 0.4167rem;
    border-radius: 0.5rem; border: 0.0833rem #ffffff26; font-size: 0.9167rem; color: #e9e4ea; white-space: nowrap; }
.eql .sbtn.put { color: #f3d98f; border-color: #e6cb8a99; }
.eql .sbtn.worn { color: #9fdcae; border-color: #9fdcae55; }
.eql .sbtn.hot { background-color: #ffffff24; }
.eql .nm.spill { font-weight: normal; color: #7e889a; }
.eql .dots { display: flex; height: 0.5833rem; margin-top: 0.0833rem; }
.eql .left .dots { justify-content: flex-end; }
.eql .dot { display: block; width: 0.5833rem; height: 0.5833rem; margin-right: 0.25rem; border-radius: 0.2917rem;
            border: 0.0833rem #ffffff40; box-sizing: border-box; }
.eql .dot.card { background-color: #f3c653; border-color: #f3c653; }
.eql .centre { display: block; position: relative; width: 12.6667rem; height: 21.6667rem; }
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
/* 3.0.18-beta.224 (R-3ZE2X9: the Rogue's tree drawn under the side panel): RmlUi clips by overflow only when it counts
   content overflowing, and the tree's nodes are absolute - «clip: always» */
.skl .view { display: block; position: relative; overflow: hidden; clip: always; }
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
/* 3.0.18-beta.101: a layered body (uikit/layered) - words, highlights and pressable places at their window's places */
.lay { display: block; position: relative; }
.lay .lb { position: absolute; box-sizing: border-box; }
.lay .lh { position: absolute; }
.lay .li img { position: absolute; }
.lay .w { position: absolute; white-space: nowrap; overflow: hidden; }
.lay .w.bold { font-weight: bold; }
.lay .w.right { text-align: right; }
.lay .w.centre { text-align: center; }
/* «Характеристики» live (uikit/stats_live): the head by flexbox, the body layered */
.lw.stl { padding: 0; font-size: 1rem; box-sizing: border-box; }
.stl .head { display: flex; align-items: center; height: 3.8333rem; padding: 0 4.3333rem 0 1.5rem; }
.stl .head .title { font-size: 1.25rem; font-weight: bold; color: #eef2f8; }
.stl .hk { margin: 0 1.8333rem 0 0.6667rem; font-size: 0.9167rem; color: #7e889a; }
.stl .tab { display: block; height: 2.1667rem; line-height: 2rem; margin: 0 0.5rem 0 0; padding: 0 1rem;
    border-radius: 1.0833rem;
            border: 0.0833rem #ffffff1a; font-size: 1rem; color: #9aa3b5; box-sizing: border-box; }
.stl .tab.hot { background-color: #ffffff14; }
.stl .tab.on { background-color: #ffffff24; border-color: #c9a55bb3; color: #f3c653; font-weight: bold; }
.stl .gap { display: block; flex: 1; }
.stl .chip { display: block; height: 1.8333rem; line-height: 1.6667rem; padding: 0 0.75rem; border-radius: 0.5rem;
             border: 0.0833rem #00000000; font-size: 0.9583rem; font-weight: bold; color: #7e889a;
             box-sizing: border-box; }
.stl .chip.on { border-color: #f3c65380; background-color: #f3c6532e; color: #f3c653; }
.stl .x { position: absolute; right: 1.5rem; top: 0.8333rem; width: 2.1667rem; height: 2.1667rem;
    line-height: 2.1667rem;
          text-align: center; border-radius: 0.5833rem; color: #9aa3b5; font-size: 1.0833rem; }
.stl .x.hot { background-color: #ffffff24; color: #ffffff; }
/* 3.0.18-beta.102: the guild's card live (uikit/guild_live) - its head and tabs by flexbox, the body layered */
.lw.gdl { padding: 0; font-size: 1rem; box-sizing: border-box; }
.gdl .head { display: block; height: 2.3333rem; line-height: 2.3333rem; margin: 1.3333rem 3.6667rem 0 1.3333rem; }
.gdl .head .title { font-size: 1.25rem; font-weight: bold; color: #eef2f8; }
.gdl .x { position: absolute; right: 1.3333rem; top: 1.5rem; width: 2rem; height: 2rem; line-height: 2rem;
          text-align: center; border-radius: 0.5rem; color: #eef2f8; font-size: 1rem; background-color: #ffffff0f; }
.gdl .x.hot { background-color: #ffffff24; }
.gdl .gtabs { display: flex; flex-wrap: wrap; justify-content: space-between; margin: 0.3333rem 1.3333rem 0 1.3333rem; }
.gdl .gtab { display: block; width: 9.3333rem; height: 2.5rem; line-height: 2.3333rem; margin-bottom: 0.5rem;
    text-align: center;
             border-radius: 0.6667rem; border: 0.0833rem #ffffff1f; background-color: #ffffff0a; font-size: 1rem;
             color: #eef2f8; box-sizing: border-box; }
.gdl .gtab.hot { background-color: #ffffff1a; }
.gdl .gtab.on { border-color: #c9a55bcc; background-color: #c9a55b3d; font-weight: bold; }
.gdl .gtab.off { color: #5e6575; }
.gdl .lay { margin-top: 0.1667rem; }
/* 3.0.18-beta.106: the login screens live - the character make (win/char_make) */
.ckw { width: 620dp; padding: 22dp 22dp 18dp 22dp; }
.ckw .head .title { font-size: 18dp; }
.ckw .head { height: 40dp; }
.ckw .cols { display: flex; margin-top: 10dp; }
.ckw .lc { display: block; width: 200dp; }
.ckw .rc { display: block; flex: 1; margin-left: 20dp; }
.ckw .face { display: flex; justify-content: center; align-items: center; width: 128dp; height: 128dp;
             margin: 0 auto 14dp auto; border-radius: 64dp; background-color: #0000004d; border: 1dp #ffffff1f; }
.ckw .face img { display: block; }
.ckw .lc .lbl { text-align: center; margin: 4dp 0 2dp 0; }
.ckw .chooser { display: flex; justify-content: center; align-items: center; height: 34dp; border-radius: 9dp;
                border: 1.5dp #00000000; }
.ckw .chooser.focus { border-color: #c9a55bcc; }
.ckw .ar { display: block; width: 28dp; height: 28dp; line-height: 28dp; text-align: center; border-radius: 7dp;
           font-size: 16dp; font-weight: bold; background-color: #ffffff0f; }
.ckw .ar:hover { background-color: #ffffff24; }
.ckw .val { display: block; width: 60dp; text-align: center; font-size: 15dp; font-weight: bold; }
.ckw .field { height: 38dp; line-height: 26dp; font-size: 15dp; font-weight: bold; margin-bottom: 12dp; }
.ckw .field .ph { font-weight: normal; font-size: 14dp; color: #9aa3b5; }
.ckw .pair { display: flex; align-items: center; height: 34dp; margin-bottom: 6dp; padding: 0 4dp; border-radius: 9dp;
             border: 1.5dp #00000000; }
.ckw .pair.focus { border-color: #c9a55bcc; }
.ckw .pt { display: block; width: 28dp; height: 28dp; line-height: 28dp; text-align: center; border-radius: 7dp;
           font-size: 16dp; font-weight: bold; background-color: #ffffff0f; }
.ckw .pt:hover { background-color: #ffffff24; }
.ckw .pt.off { color: #9aa3b5; background-color: #ffffff05; }
.ckw .sl { display: block; width: 72dp; margin-left: 8dp; font-size: 13dp; font-weight: bold; color: #e6cb8a; }
.ckw .sr { display: block; width: 72dp; margin-right: 8dp; text-align: right; font-size: 13dp; font-weight: bold; }
.ckw .bar { display: block; flex: 1; height: 8dp; border-radius: 4dp; background-color: #3d7eebbf; }
.ckw .bar .fill { display: block; height: 8dp; border-radius: 4dp; background-color: #c9a55be6; }
.ckw .btns { margin-top: 14dp; }
.ckw .btn { padding: 10dp 22dp; font-size: 14dp; }
/* 3.0.18-beta.106: the character select live (uikit/charselect_live) - its head and buttons by flexbox */
.lw.csl { padding: 0; font-size: 1rem; box-sizing: border-box; }
.csl .head { display: flex; align-items: center; height: 3.3333rem; margin: 1.8333rem 1.8333rem 0 1.8333rem; }
.csl .head .title { font-size: 1.5rem; font-weight: bold; color: #eef2f8; }
.csl .gap { display: block; flex: 1; }
.csl .pa { display: block; width: 2.3333rem; height: 2.3333rem; line-height: 2.3333rem; text-align: center;
    border-radius: 0.5833rem;
           font-size: 1.3333rem; font-weight: bold; background-color: #ffffff0f; }
.csl .pg { display: block; width: 5.3333rem; text-align: center; font-size: 1.0417rem; color: #9aa3b5; }
.csl .lay { margin-top: 0.6667rem; }
.csl .cbtns { display: flex; justify-content: space-between; margin: 0 1.8333rem 1.8333rem 1.8333rem; }
.csl .csb { display: block; flex: 1; height: 3.5rem; line-height: 3.3333rem; margin-right: 1.1667rem;
    text-align: center;
           border-radius: 0.75rem; border: 0.0833rem #ffffff29; background-color: #ffffff0f; font-size: 1.1667rem;
           color: #eef2f8;
           box-sizing: border-box; }
.csl .csb:last-child { margin-right: 0; }
.csl .csb.hot { background-color: #ffffff1f; }
.csl .csb.main { border-color: #c9a55bd9; background-color: #c9a55b3d; font-weight: bold; }
.csl .csb.main.hot { background-color: #c9a55b5c; }
.csl .csb.off { color: #9aa3b5; background-color: #ffffff05; border-color: #ffffff14; }
/* 3.0.18-beta.107: the login card live (uikit/login_live) - a form 31.6667rem wide */
.lw.lgl { width: 31.6667rem; padding: 2rem 2rem 2rem 2rem; font-size: 1rem; box-sizing: border-box; }
.lgl .back { position: absolute; left: 1.5rem; top: 1.5rem; height: 2rem; line-height: 2rem; padding: 0 0.5rem;
             font-size: 1.0417rem; color: #8cb8ff; }
.lgl .back.hot { color: #e6cb8a; }
.lgl .ttl { display: block; height: 2.8333rem; line-height: 2.8333rem; text-align: center; font-size: 1.8333rem;
    font-weight: bold;
            color: #eef2f8; }
.lgl .sub { display: block; height: 1.6667rem; line-height: 1.6667rem; margin-bottom: 1rem; text-align: center;
            font-size: 1.0417rem; color: #9aa3b5; }
.lgl .flbl { display: block; height: 1.5rem; line-height: 1.5rem; font-size: 1rem; font-weight: bold; color: #9aa3b5; }
.lgl .fld { display: flex; align-items: center; position: relative; height: 3.1667rem; margin-bottom: 0.6667rem;
            padding: 0 1rem; border-radius: 0.6667rem; border: 0.0833rem #ffffff29; background-color: #00000059;
            box-sizing: border-box; }
.lgl .fld.pw { margin-bottom: 0.8333rem; }
.lgl .fld.hot { border-color: #ffffff4d; }
.lgl .fld.on { border: 0.125rem #c9a55bd9; }
.lgl .fld.bad { border: 0.125rem #ff5a4ee6; }
.lgl .fv { font-size: 1.1667rem; color: #eef2f8; white-space: nowrap; }
.lgl .caret { display: block; width: 0.125rem; height: 1.5rem; margin-left: 0.0833rem; background-color: #eef2f8; }
.lgl .drop, .lgl .eye { display: flex; justify-content: center; align-items: center; position: absolute; right: 0;
                        top: 0; width: 3rem; height: 100%; }
.lgl .eye { width: 3.1667rem; }
.lgl .drop img, .lgl .eye img { display: block; }
.lgl .errs { display: block; margin: -0.3333rem 0 0.3333rem 0; }
.lgl .err { display: block; height: 1.5rem; line-height: 1.5rem; font-size: 1rem; color: #ff5a4e; white-space: nowrap; }
.lgl .err.hint { color: #f5bd4f; }
.lgl .act { display: block; height: 1.5rem; line-height: 1.5rem; font-size: 1rem; color: #8cb8ff; }
.lgl .act.hot { color: #e6cb8a; }
.lgl .tk { display: flex; align-items: center; height: 2rem; margin-bottom: 0.3333rem; font-size: 1.0417rem;
    color: #eef2f8; }
.lgl .tk .box { display: block; width: 1.5rem; height: 1.5rem; line-height: 1.3333rem; margin-right: 0.8333rem;
    text-align: center;
                border-radius: 0.3333rem; border: 0.0833rem #ffffff59; background-color: #ffffff0f;
                font-size: 1.0833rem;
                font-weight: bold; color: #000000; box-sizing: border-box; }
.lgl .tk .box.on { border-color: #c9a55b; background-color: #c9a55bd9; }
.lgl .lbtns { display: flex; margin-top: 0.6667rem; }
.lgl .lb { display: block; flex: 1; height: 3.5rem; line-height: 3.3333rem; text-align: center; border-radius: 0.75rem;
           border: 0.0833rem #ffffff29; background-color: #ffffff0f; font-size: 1.1667rem; color: #eef2f8;
           box-sizing: border-box; }
.lgl .lb.go { margin-right: 1rem; border-color: #c9a55bd9; background-color: #c9a55b3d; font-weight: bold; }
.lgl .lb.hot { background-color: #ffffff1f; }
.lgl .lb.go.hot { background-color: #c9a55b5c; }
.lgl .links { display: flex; margin-top: 1rem; height: 1.6667rem; line-height: 1.6667rem; font-size: 1.0417rem; }
.lgl .lk { display: block; flex: 1; color: #8cb8ff; }
.lgl .lk.r { text-align: right; }
.lgl .lk.across { text-align: center; font-size: 1rem; }
.lgl .lk.hot { color: #e6cb8a; }
.lgl .note { display: block; margin-top: 0.5rem; text-align: center; font-size: 1rem; color: #9aa3b5; }
.lgl .site { display: block; height: 1.6667rem; line-height: 1.6667rem; margin-top: 0.6667rem; text-align: center;
    font-size: 1.0417rem;
             color: #8cb8ff; }
.lgl .site.hot { color: #e6cb8a; }
.lgl .site.chip { display: flex; text-align: left; }
.lgl .sc { display: block; flex: 1; color: #9aa3b5; }
.lgl .so { display: block; width: 5.8333rem; text-align: right; color: #8cb8ff; }
.lgl .so.hot { color: #e6cb8a; }
.lgl .lay { position: absolute; left: 0; top: 0; z-index: 5; }
/* 3.0.18-beta.110 (the owner: «максимально похожем на оригинальный вариант»): the original window's places - the
   character in the middle (no numbers), the cards' dots after a thing's name, the ammo in the strip under */
.eql .stage { display: flex; flex-direction: column; justify-content: flex-end; align-items: center; position: relative;
              height: 21.6667rem; padding-bottom: 2.8333rem; box-sizing: border-box; }
.eql .stage img { display: block; }
.eql .turn { position: absolute; left: 0.5rem; right: 0.5rem; bottom: 0.1667rem; display: flex;
    justify-content: space-between; }
.eql .tb { display: block; width: 2.3333rem; height: 2.3333rem; line-height: 2.3333rem; text-align: center;
    border-radius: 0.6667rem;
           border: 0.0833rem #ffffff22; background-color: #ffffff0d; font-size: 1rem; }
.eql .tb:hover, .eql .tb.hot { background-color: #ffffff24; border-color: #e6cb8a88; color: #e6cb8a; }
.eql .nm .dot { display: inline-block; width: 0.5833rem; height: 0.5833rem; margin: 0 0 0.0833rem 0.3333rem;
    vertical-align: middle; }
.eql .right .nm .dot { margin: 0 0.3333rem 0.0833rem 0; }
.eql .ammorow { display: block; margin: 0.5rem 1.3333rem 0 1.3333rem; width: 19rem; }
.eql .ammorow .slot.ammo { position: static; height: 3.8333rem; margin: 0; padding: 0 0.5rem; }
.eql .left .slot { flex-direction: row; }
.eql .right .slot { flex-direction: row-reverse; }
.eql .left .txt { text-align: left; }
.eql .right .txt { text-align: right; }
/* 3.0.18-beta.113 (the owner: «чтобы имена не обрезались и не добавлялись точки»): a long name goes on a second line,
   its slot grows; the columns and the middle as tall as they need */
.eql .cols { height: auto; }
.eql .slot { height: auto; min-height: 3.8333rem; padding-top: 0.25rem; padding-bottom: 0.25rem; }
.eql .txt { overflow: visible; }
.eql .nm { height: auto; line-height: 1.4167rem; white-space: normal; }
.eql .centre { height: auto; min-height: 21.6667rem; }
.eql .stage { height: 100%; min-height: 21.6667rem; }
/* 3.0.18-beta.114 (the owner: «нету кнопки для анманта пеко итд»), .116 (the owner chose «2 Значок між стрілками»):
   a round sign between ◀ and ▶, its words in a tip over it */
.eql .turn { align-items: center; }
.eql .mi { display: flex; justify-content: center; align-items: center; width: 2.3333rem; height: 2.3333rem;
    border-radius: 1.1667rem; border: 0.0833rem #e6cb8a88; background-color: #00000066; }
.eql .mi.hot, .eql .mi:hover { background-color: #ffffff24; border-color: #e6cb8a; }
.eql .mi img { display: block; }
.eql .mtip { position: absolute; left: 0; right: 0; bottom: 2.9167rem; display: flex; justify-content: center;
    z-index: 3; }
.eql .tp { display: block; height: 1.8333rem; line-height: 1.8333rem; padding: 0 0.6667rem; font-size: 0.9167rem;
    border-radius: 0.5rem; border: 0.0833rem #ffffff22; background-color: #000000cc; color: #e9e4ea;
    white-space: nowrap; }
/* 3.0.18-beta.117 (the owner: «при анмаунте надо сделать подтверждение да нет»): asked over the sign */
.eql .ask { display: block; padding: 0.5rem 0.6667rem; border-radius: 0.5833rem; border: 0.0833rem #e6cb8a66;
    background-color: #000000e0; text-align: center; }
.eql .aq { display: block; font-size: 0.9167rem; color: #e9e4ea; white-space: nowrap; margin-bottom: 0.4167rem; }
.eql .ab { display: flex; justify-content: center; }
.eql .ay, .eql .an { display: block; width: 3.6667rem; height: 1.8333rem; line-height: 1.8333rem; margin: 0 0.25rem;
    border-radius: 0.5rem; text-align: center; font-size: 0.9167rem; border: 0.0833rem #ffffff22; }
.eql .ay { color: #e6cb8a; border-color: #e6cb8a88; }
.eql .an { color: #c9cfdb; }
.eql .ay.hot, .eql .an.hot { background-color: #ffffff24; }
/* 3.0.18-beta.132 (the pad analysis, variant A): the skill wheel - eight cells round a dark ring, the pointed one
   gold, its name in the middle */
.swh { position: absolute; font-family: Plex; }
/* 3.0.18-beta.133: the pad's cross on the big map */
.bmx { position: absolute; }
.bmx .r { position: absolute; left: 0; top: 0; border-width: 2px; border-color: #e6cb8a; background-color: #e6cb8a26; }
.bmx .d { position: absolute; left: 50%; top: 50%; width: 4px; height: 4px; margin: -2px 0 0 -2px;
    border-radius: 2px; background-color: #e6cb8a; }
/* 3.0.18-beta.151: a window's size grip - its bottom right corner under the pointer */
.wgr { position: absolute; }
.wgr .c { position: absolute; left: 0; top: 0; border-right-width: 3px; border-bottom-width: 3px;
    border-color: #e6cb8a; border-bottom-right-radius: 6px; }
/* 3.0.18-beta.135: the pad's ring on our panels' buttons */
.ppr { position: absolute; }
.ppr .r { position: absolute; left: 0; top: 0; border-width: 2px; border-color: #e6cb8a; border-radius: 6px;
    background-color: #e6cb8a1f; }
.swh .ring { position: absolute; left: 0; top: 0; background-color: #10141cb8; border-width: 1px;
    border-color: #ffffff1f; }
.swh .cell { position: absolute; display: flex; align-items: center; justify-content: center;
    background-color: #ffffff12; border-width: 1px; border-color: #ffffff22; }
.swh .cell.lit { background-color: #e6cb8a3a; border-color: #e6cb8a; }
.swh .cell .fk { position: absolute; left: 3px; top: 1px; font-size: 10px; color: #e6cb8a; }
.swh .mid { position: absolute; display: flex; flex-direction: column; align-items: center; justify-content: center;
    background-color: #00000066; }
.swh .mid .name { display: block; font-size: 13px; font-weight: bold; color: #eef2f8; text-align: center; }
.swh .mid .title { display: block; font-size: 11px; color: #9aa3b5; }
/* 3.0.18-beta.239 (docs/superpowers/plans/2026-11-13-reference-rewrite.md): «Довідник» a live window (uikit
   reference_live) - its own classes under «rfw»; gold only for what is pressed and what is chosen, numbers white */
.rfw { position: absolute; display: flex; flex-direction: column; font-family: Plex; font-size: 13.5dp; color: #eceff5;
       border-radius: 14dp; border: 1dp #ffffff17; box-sizing: border-box; }
.rfw.marked { border-color: #e6cb8ad9; }
.glassy.rfw { decorator: ua-glass(1); }
.motion.rfw .rr, .motion.rfw .rres, .motion.rfw .rri, .motion.rfw .rtile, .motion.rfw .rchp {
    transition: background-color border-color color 0.12s cubic-out; }
.rfw .rhead { display: flex; flex-direction: row; align-items: center; flex: none; height: 56dp; padding: 0 14dp;
    border-bottom: 1dp #ffffff17; }
.rfw .rbk { display: block; flex: none; height: 30dp; line-height: 30dp; padding: 0 11dp; margin-right: 8dp;
    border-radius: 8dp; background-color: #ffffff0f; color: #c9d1de; font-size: 13dp; white-space: nowrap; }
.rfw .rhm { display: block; flex: none; width: 30dp; height: 30dp; margin-right: 10dp; border-radius: 8dp;
    background-color: #ffffff0f; }
.rfw .rhm img { display: block; width: 18dp; height: 18dp; margin: 6dp; }
.rfw .rbk:hover, .rfw .rhm:hover, .rfw .rx:hover, .rfw .rst:hover { background-color: #ffffff21; color: #ffffff; }
.rfw .rtt { display: block; flex: none; font-size: 18dp; font-weight: bold; margin-right: 12dp; white-space: nowrap; }
.rfw .rcr { display: block; flex: 1; overflow: hidden; white-space: nowrap; font-size: 12.5dp; color: #9aa3b5; }
.rfw .rcb { margin-right: 4dp; }
.rfw .rcb .sep { margin-right: 4dp; color: #7d8597; }
.rfw .rsrch { display: block; position: relative; flex: none; width: 320dp; height: 32dp; margin-left: 10dp;
    border-radius: 9dp; border: 1dp #ffffff1a; background-color: #00000059; }
.rfw .rsrch input { display: block; width: 100%; height: 32dp; padding: 0 64dp 0 12dp; line-height: 32dp;
    font-size: 13dp; color: #eceff5; box-sizing: border-box; }
.rfw .rsrch .ph { position: absolute; left: 12dp; top: 0; line-height: 32dp; font-size: 13dp; color: #9aa3b5; }
.rfw .rsrch .kbd { position: absolute; right: 8dp; top: 7dp; height: 16dp; line-height: 16dp; padding: 0 5dp;
    font-size: 10.5dp; color: #9aa3b5; border: 1dp #ffffff29; border-radius: 4dp; }
.rfw .rst, .rfw .rx { display: block; flex: none; width: 30dp; height: 30dp; line-height: 30dp; margin-left: 8dp;
    text-align: center; border-radius: 8dp; background-color: #ffffff0f; color: #9aa3b5; }
.rfw .rst { font-size: 16dp; }
.rfw .rst.on { color: #e6cb8a; }
.rfw .rmid { display: flex; flex-direction: row; flex: 1; min-height: 0; }
.rfw .rrail { display: block; flex: none; width: 196dp; padding: 10dp 8dp; overflow-y: auto;
    border-right: 1dp #ffffff17; }
.rfw .rri { display: flex; flex-direction: row; align-items: center; height: 34dp; padding: 0 10dp; margin-bottom: 2dp;
    border-radius: 8dp; color: #c9d1de; }
.rfw .rri:hover { background-color: #ffffff10; }
.rfw .rri.on { background-color: #e6cb8a21; color: #e6cb8a; font-weight: bold; }
.rfw .rri img { display: block; flex: none; width: 18dp; height: 18dp; margin-right: 10dp; }
.rfw .rrn { display: block; flex: 1; white-space: nowrap; overflow: hidden; }
.rfw .rrc { display: block; flex: none; margin-left: 6dp; font-size: 11dp; color: #9aa3b5; }
.rfw.narrow .rrail { width: 52dp; padding: 10dp 6dp; }
.rfw.narrow .rrn, .rfw.narrow .rrc, .rfw.narrow .rcr { display: none; }
.rfw.narrow .rsrch { width: 200dp; }
.rfw .rscroll { display: block; flex: 1; overflow-y: auto; padding: 14dp 18dp 16dp 18dp; }
.rfw .rfix { display: block; margin-bottom: 12dp; padding: 9dp 12dp; border-radius: 9dp; border: 1dp #f3c65352;
    background-color: #f3c65317; }
.rfw .rbody { display: block; }
.rfw .rfoot { display: flex; flex-direction: row; align-items: center; flex: none; height: 36dp; padding: 0 14dp;
    border-top: 1dp #ffffff17; font-size: 12dp; color: #9aa3b5; }
.rfw .rchip { display: block; flex: none; height: 22dp; line-height: 22dp; padding: 0 9dp; margin-right: 8dp;
    border-radius: 11dp; background-color: #ffffff0f; white-space: nowrap; }
.rfw .rchip:hover { background-color: #ffffff21; color: #eceff5; }
.rfw .rchip.fresh { color: #9be7a9; }
.rfw .rchip.stale { color: #f0b46a; }
.rfw .rkeys { display: block; flex: 1; text-align: right; white-space: nowrap; overflow: hidden; }
.rfw .ring { position: absolute; display: none; border: 2dp #e6cb8a; border-radius: 8dp; background-color: #e6cb8a14; }
/* 3.0.18-beta.241 (the concept's pad): the cross on a map the pad took - two gold bars edged dark */
.rfw .rcross { position: absolute; display: none; width: 26dp; height: 26dp; }
.rfw .rch { position: absolute; left: 0; top: 11dp; width: 24dp; height: 2dp; background-color: #e6cb8a;
  border: 1dp #000000b3; }
.rfw .rcv { position: absolute; left: 11dp; top: 0; width: 2dp; height: 24dp; background-color: #e6cb8a;
  border: 1dp #000000b3; }
.rfw .rtitle { display: block; font-size: 24dp; font-weight: bold; margin: 2dp 0 6dp 0; }
.rfw .rtitle .rrt { margin-left: 10dp; font-size: 14dp; font-weight: normal; color: #9aa3b5; }
.rfw .rsubl { display: block; margin-bottom: 8dp; color: #9aa3b5; }
.rfw .rnote { display: block; padding: 4dp 6dp; font-size: 12.5dp; color: #9aa3b5; }
.rfw .rtext { display: block; padding: 4dp 6dp; line-height: 1.45; }
.rfw .rhint { display: block; margin: 6dp 0 10dp 0; font-size: 12dp; color: #9aa3b5; }
.rfw .rcap { display: block; margin: 2dp 0 6dp 0; font-size: 15dp; font-weight: bold; }
.rfw .rwarn { display: block; margin: 6dp 0; padding: 8dp 10dp; border-radius: 8dp; border: 1dp #f0b46a55;
    background-color: #f0b46a1c; color: #f6cf9a; }
.rfw .rmore { display: block; padding: 5dp 6dp; font-size: 13dp; color: #e6cb8a; }
.rfw .rmore:hover { color: #f3d998; }
.rfw .rempty { display: block; padding: 20dp 6dp; }
.rfw .rempty .rel { display: block; font-size: 15dp; }
.rfw .rempty .rsb { display: block; margin: 4dp 0 0 0; color: #9aa3b5; }
.rfw .rport { display: flex; flex-direction: row; align-items: center; margin: 0 0 14dp 0; }
.rfw .rpic { display: flex; flex: none; align-items: center; justify-content: center; width: 158dp; height: 158dp;
    margin-right: 16dp; border-radius: 12dp; border: 1dp #ffffff17; background-color: #ffffff08; }
.rfw .rpimg { display: block; flex: none; width: 150dp; height: 150dp; }
.rfw .rport.hasfacts { align-items: flex-start; }
.rfw .rpinfo { display: block; flex: 1; }
.rfw .rpl { display: block; font-size: 14dp; }
.rfw .rpr { display: block; margin-top: 3dp; color: #9aa3b5; }
/* 3.0.18-beta.242 (the concept's cards): an item's words from the game under its name; a monster's level by yours -
   fitting green, too strong red, too weak faint */
.rfw .rpd { display: block; margin-top: 6dp; font-size: 12.5dp; line-height: 17dp; color: #c9cfdb; }
.rfw .rlv.t1 { color: #7fd49a; }
.rfw .rlv.t2 { color: #ff8a7a; }
.rfw .rlv.t3 { color: #7d8597; }
.rfw .rtags { display: flex; flex-direction: row; flex-wrap: wrap; margin-top: 8dp; }
.rfw .rtag { display: flex; flex-direction: row; align-items: center; height: 24dp; padding: 0 10dp;
    margin: 0 6dp 6dp 0; border-radius: 12dp; background-color: #ffffff0f; font-size: 12.5dp; color: #c9d1de; }
.rfw .rtag.gold { background-color: #c0392b33; color: #ffb3a8; }
.rfw .rdot { display: block; flex: none; width: 9dp; height: 9dp; margin-right: 7dp; border-radius: 5dp; }
.rfw .rtiles { display: flex; flex-direction: row; flex-wrap: wrap; margin: 4dp 0 14dp 0; }
.rfw .rtile { display: block; width: 19%; margin: 0 1% 10dp 0; padding: 12dp; border-radius: 12dp;
    border: 1dp #ffffff17; background-color: #ffffff05; box-sizing: border-box; }
.rfw .rtile:hover { background-color: #ffffff12; border-color: #ffffff29; }
.rfw .rtile .ri { width: 22dp; height: 22dp; }
.rfw .rtn { display: block; margin-top: 6dp; font-size: 14dp; font-weight: bold; }
.rfw .rtc { display: block; font-size: 12dp; color: #9aa3b5; }
.rfw .rts { display: block; margin-top: 3dp; font-size: 11.5dp; color: #7d8597; }
.rfw.narrow .rtile { width: 32%; }
.rfw .rcols { display: flex; flex-direction: row; align-items: flex-start; }
.rfw .rcol { display: block; flex: 1 1 0; min-width: 0; }
.rfw .rcol:first-child { margin-right: 8dp; }
.rfw .rcol:last-child { margin-left: 8dp; }
.rfw .rsec { display: block; margin-bottom: 14dp; padding: 10dp 12dp; border-radius: 12dp; border: 1dp #ffffff14;
    background-color: #ffffff05; }
.rfw .rsh { display: flex; flex-direction: row; align-items: center; height: 26dp; margin-bottom: 4dp; font-size: 13dp;
    font-weight: bold; color: #9aa3b5; }
.rfw .rsh.ctl:hover { color: #eceff5; }
.rfw .rshn { display: block; flex: 1; white-space: nowrap; overflow: hidden; }
.rfw .rshr { display: block; flex: none; margin-left: 8dp; font-weight: normal; }
.rfw .rgroup { color: #c9d1de; }
.rfw .rsplit { display: flex; flex-direction: row; align-items: flex-start; }
.rfw .rspc { display: block; flex: 1 1 0; min-width: 0; margin-right: 14dp; }
.rfw .rspc:last-child { margin-right: 0; }
.rfw .rjump { display: flex; flex-direction: row; flex-wrap: wrap; margin-bottom: 12dp; }
.rfw .rjc { display: block; height: 24dp; line-height: 24dp; padding: 0 10dp; margin: 0 6dp 6dp 0; border-radius: 12dp;
    background-color: #ffffff0f; font-size: 12dp; color: #e6cb8a; }
.rfw .rjc:hover { background-color: #e6cb8a26; }
.rfw .rr, .rfw .rres { display: flex; flex-direction: row; align-items: center; min-height: 30dp; padding: 0 6dp;
    border-radius: 7dp; }
.rfw .rres { min-height: 38dp; }
.rfw .rr.ctl:hover, .rfw .rres.ctl:hover, .rfw .rstep.ctl:hover, .rfw .rchp.ctl:hover, .rfw .rgrid.ctl:hover,
.rfw .rleg.ctl:hover { background-color: #ffffff12; }
.rfw .rr.hl { background-color: #ffffff0a; }
.rfw .sel, .rfw .sel:hover { background-color: #e6cb8a26; }
.rfw .ri { display: block; flex: none; width: 24dp; height: 24dp; margin-right: 8dp; }
/* 3.0.18-beta.250 (the owner: «картинки сделай побольше в базе»): the lists' monsters and things bigger */
.rfw .rr .ri.rim, .rfw .rres .ri.rim { width: 44dp; height: 44dp; margin-right: 10dp; }
.rfw .rr .ri.rit, .rfw .rres .ri.rit { width: 32dp; height: 32dp; margin-right: 10dp; }
.rfw .rl { display: block; flex: 1; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.rfw .rsb { display: block; flex: none; margin-left: 10dp; font-size: 12dp; color: #9aa3b5; white-space: nowrap; }
/* 3.0.18-beta.255 (the owner: «что падает не показывает шанс»): .253's max-width 60% with overflow hidden left it no
   width in RmlUi's flex layout - the note whole again (the long ones are entries now, cut there) */
.rfw .rrt { display: block; flex: none; margin-left: 10dp; font-size: 12.5dp; color: #eceff5; white-space: nowrap; }
/* 3.0.18-beta.253 (the owner's review of the reference: R-N7GPT5 «список сделан совсем ни о чём», R-U5HAJ6, R-9CD8YA
   - a craft's things over the next column): a list's entry - its picture, its name and a line under it (cut with «…»),
   its figures at the right in columns, a value over its label */
.rfw .rent { display: flex; flex-direction: row; align-items: center; min-height: 48dp; padding: 3dp 8dp;
    border-radius: 8dp; }
.rfw .rent.ctl:hover { background-color: #ffffff12; }
.rfw .rent .ri { margin-right: 10dp; }
.rfw .rent .ri.rim { width: 44dp; height: 44dp; }
.rfw .rent .ri.rit { width: 32dp; height: 32dp; margin-left: 6dp; margin-right: 16dp; }
.rfw .ren { display: block; flex: 1 1 0; min-width: 0; overflow: hidden; }
.rfw .renn { display: block; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-size: 14dp; }
.rfw .rent.hl .renn { color: #e6cb8a; }
.rfw .rens { display: block; margin-top: 2dp; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
    font-size: 12dp; color: #9aa3b5; }
.rfw .rec { display: block; flex: none; width: 58dp; margin-left: 6dp; text-align: right; }
.rfw .rec.w { width: 104dp; }
.rfw .recv { display: block; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-size: 13.5dp; }
.rfw .recl { display: block; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-size: 10.5dp;
    color: #7d8597; }
.rfw .recv.t1, .rfw .rfv.t1 { color: #7fd49a; }
.rfw .recv.t2, .rfw .rfv.t2 { color: #ff8a7a; }
.rfw .recv.t3, .rfw .rfv.t3 { color: #7d8597; }
.rfw .recv.t4, .rfw .rfv.t4 { color: #e6cb8a; }
/* 3.0.18-beta.253 (R-JSWF77 «дай полную инфу, статы», R-WP985Q): a card's facts - a grid of small boxes, a label over
   its value, four to a line (a wide one the whole line) */
.rfw .rfacts { display: flex; flex-direction: row; flex-wrap: wrap; margin-top: 10dp; }
.rfw .rfact { display: block; width: 23.8%; margin: 0 1% 6dp 0; padding: 5dp 9dp; border-radius: 8dp;
    background-color: #ffffff0b; box-sizing: border-box; }
.rfw .rfact.wide { width: 98.2%; }
.rfw .rfl { display: block; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-size: 10.5dp;
    color: #8a93a3; }
.rfw .rfv { display: block; margin-top: 1dp; font-size: 13.5dp; }
.rfw .rfrow { display: block; }
.rfw .rres .rrs { display: block; flex: 1; overflow: hidden; }
.rfw .rres .rl, .rfw .rres .rsb { display: block; margin-left: 0; }
.rfw .rres .rrt { color: #9aa3b5; }
.rfw .mk { color: #e6cb8a; font-weight: bold; }
.rfw .rchips { display: flex; flex-direction: row; flex-wrap: wrap; margin: 2dp 0 4dp 0; }
.rfw .rchp { display: flex; flex-direction: row; align-items: center; height: 30dp; padding: 0 10dp 0 6dp;
    margin: 0 6dp 6dp 0; border-radius: 15dp; background-color: #ffffff0f; }
.rfw .rchp .ri { width: 22dp; height: 22dp; margin-right: 6dp; }
.rfw .rcn { display: block; white-space: nowrap; }
.rfw .rcs { display: block; margin-left: 6dp; font-size: 11.5dp; color: #9aa3b5; white-space: nowrap; }
.rfw .rgrid { display: flex; flex-direction: row; align-items: center; height: 26dp; padding: 0 6dp; }
.rfw .rgrid.hd { font-size: 11.5dp; color: #9aa3b5; }
.rfw .rgl { display: block; flex: none; width: 90dp; white-space: nowrap; }
.rfw .rgc { display: block; flex: 1 1 0; text-align: center; font-size: 12.5dp; }
.rfw .rgc.up { color: #9be7a9; }
.rfw .rgc.dn { color: #8a93a3; }
.rfw .rgc.heal { color: #ff9b9b; }
.rfw .rwbar { display: flex; flex-direction: row; flex-wrap: wrap; align-items: center; margin-bottom: 10dp; }
.rfw .rwtab { display: flex; flex-direction: row; align-items: center; height: 30dp; padding: 0 12dp; margin-right: 6dp;
    border-radius: 8dp; background-color: #ffffff0f; color: #c9d1de; }
.rfw .rwtab.on { background-color: #e6cb8a26; color: #e6cb8a; font-weight: bold; }
.rfw .rwtab .ri { width: 18dp; height: 18dp; margin-right: 6dp; }
.rfw .rwtool { display: block; height: 30dp; margin-left: 6dp; border-radius: 8dp; background-color: #ffffff0f; }
.rfw .rwtool .ri { width: 18dp; height: 18dp; margin: 6dp; }
.rfw .rwtab.ctl:hover, .rfw .rwtool:hover { background-color: #ffffff21; }
.rfw .rwrow { display: flex; flex-direction: row; align-items: flex-start; }
.rfw .rwleft { display: block; flex: none; }
.rfw .rwmap { display: block; position: relative; overflow: hidden; border-radius: 10dp; border: 1dp #ffffff17; }
.rfw .rwmap img { display: block; }
.rfw .ww { position: absolute; white-space: nowrap; font-effect: outline(2px #000000b3); }
.rfw .ww.b { font-weight: bold; }
.rfw .ww.wrap { white-space: normal; }
.rfw .rwpanel { display: block; flex: 1; margin-left: 14dp; overflow-y: auto; }
.rfw .rleg { display: flex; flex-direction: row; align-items: center; height: 24dp; font-size: 12.5dp; color: #c9d1de; }
.rfw .lg { display: block; flex: none; width: 14dp; height: 14dp; margin-right: 8dp; border-radius: 7dp; }
.rfw .lg-here { background-color: #f3c653; }
.rfw .lg-players { background-color: #3fb950; border-radius: 4dp; }
.rfw .lg-route { height: 3dp; border-radius: 2dp; background-color: #e6cb8a; }
.rfw .lg-jump { height: 3dp; border-radius: 2dp; background-color: #e0447c; }
.rfw .rstep { display: flex; flex-direction: row; align-items: flex-start; padding: 6dp; border-radius: 8dp; }
.rfw .rsn { display: block; flex: none; width: 22dp; height: 22dp; line-height: 22dp; margin-right: 10dp;
    text-align: center; border-radius: 11dp; background-color: #ffffff17; font-size: 12dp; }
.rfw .rstep.paid .rsn { background-color: #e0447c40; }
.rfw .rss { display: block; flex: 1; }
.rfw .rss .rl, .rfw .rss .rsb { display: block; margin-left: 0; white-space: normal; }
.rfw .rbtn { display: block; height: 32dp; line-height: 32dp; margin: 8dp 0 0 0; padding: 0 14dp; border-radius: 9dp;
    text-align: center; background-color: #ffffff12; }
.rfw .rbtn:hover { background-color: #ffffff21; }
.rfw .rbtn.main { background-color: #f3c653; color: #1a1406; font-weight: bold; }
.rfw .rbtn.main:hover { background-color: #f6d27a; }
.rfw .rmrow { display: flex; flex-direction: row; align-items: flex-start; margin-bottom: 14dp; }
.rfw .rmleft { display: block; flex: none; }
.rfw .rmbar { display: flex; flex-direction: row; flex-wrap: wrap; align-items: center; margin-bottom: 8dp; }
.rfw .rlay { display: block; height: 26dp; line-height: 26dp; padding: 0 10dp; margin-right: 6dp; border-radius: 13dp;
    background-color: #ffffff0f; font-size: 12dp; color: #9aa3b5; }
.rfw .rlay.on { background-color: #e6cb8a26; color: #e6cb8a; }
.rfw .rlay:hover { background-color: #ffffff21; }
.rfw .rmview { display: block; position: relative; overflow: hidden; border-radius: 10dp; border: 1dp #ffffff17; }
.rfw .rmview img { display: block; }
.rfw .rmside { display: block; flex: 1; margin-left: 14dp; }
.rfw.narrow .rmrow { flex-direction: column; }
.rfw.narrow .rmside { margin: 12dp 0 0 0; }
)";

}  // namespace

const char* builtInCss() { return kTheme; }

}  // namespace uagfx::uitheme

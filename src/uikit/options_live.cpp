#include "uikit/options_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>

namespace uagfx::uikit::options {
namespace {

// 3.0.21-beta.8: an element a list makes (data-for) carries no view of its own - .srow, .slw and .chw are plain
// wrappers, the views a level down (see fit() in the header); a slider's part and the choices exist only in the rows
// that have them
const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw ow sw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span><span class="hk">Alt+O</span></div>
<span class="x ctl" data-event-click="opt_close()">&#x2715;</span>
<div class="srch"><input class="ctl" type="text" data-value="query"/><span class="ph"
  data-if="query == ''">{{search_hint}}</span></div>
<div class="sbody">
 <div class="snav"><div class="sn ctl" data-for="n, i : nav" data-class-on="i == sec && query == ''"
   data-event-click="sec_go(i)"><span class="nn">{{n.name}}</span><span class="cnt"
   data-if="n.count > 0">{{n.count}}</span></div></div>
 <div id="spane" class="spane"><div class="srow" data-for="r, i : srows"><div class="sgrp"
   data-if="r.group != ''">{{r.group}}</div><div class="smore ctl" data-if="r.more"
   data-event-click="set_more(i)">{{r.label}}</div><div class="sro" data-if="!r.more"
   data-class-sub="r.sub"><div class="st"><span
   class="sl">{{r.label}}</span><span class="shn" data-if="r.hint != ''">{{r.hint}}</span><span class="swn"
   data-if="r.warn != ''">{{r.warn}}</span></div><div
   class="sctl"><span class="tg ctl" data-if="r.toggle" data-class-on="r.on"
   data-event-click="set_toggle(i)"><span class="kn"></span></span><div class="slw"
   data-for="s : r.sliders"><input class="ctl" type="range" data-attr-min="r.min" data-attr-max="r.max"
   data-attr-step="r.step" data-attr-value="r.value" data-event-change="set_slider(i)"/><span class="ch ctl"
   data-event-click="set_step(i, -1)">&#x2212;</span><span class="sv">{{r.vtext}}</span><span
   class="ch ctl" data-event-click="set_step(i, 1)">+</span></div><div class="chs"><span class="chw"
   data-for="c, j : r.choices"><span class="ch ctl" data-class-on="c.on"
   data-event-click="set_choice(i, j)">{{c.label}}</span></span></div></div></div></div>
   <div class="snone" data-if="shown_rows == 0">{{none}}</div></div>
</div>
<div class="sfoot"><span class="sfh">{{hint}}</span><div class="sfb"><span class="btn ctl"
  data-event-click="open_layout()">{{layout_btn}}</span><span class="btn ctl"
  data-event-click="open_console()">{{console_btn}}</span></div></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";

}  // namespace

const char* rml() { return kRml; }

void fit(std::vector<RowView>& rows) {
    for (RowView& r : rows) r.sliders.assign(r.slider && !r.more ? 1 : 0, 0);
}

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto v = c.RegisterStruct<ChoiceView>()) {
        v.RegisterMember("label", &ChoiceView::label);
        v.RegisterMember("on", &ChoiceView::on);
    }
    c.RegisterArray<std::vector<ChoiceView>>();
    c.RegisterArray<std::vector<int>>();
    if (auto v = c.RegisterStruct<RowView>()) {
        v.RegisterMember("group", &RowView::group);
        v.RegisterMember("label", &RowView::label);
        v.RegisterMember("hint", &RowView::hint);
        v.RegisterMember("warn", &RowView::warn);
        v.RegisterMember("vtext", &RowView::vtext);
        v.RegisterMember("toggle", &RowView::toggle);
        v.RegisterMember("on", &RowView::on);
        v.RegisterMember("slider", &RowView::slider);
        v.RegisterMember("value", &RowView::value);
        v.RegisterMember("min", &RowView::min);
        v.RegisterMember("max", &RowView::max);
        v.RegisterMember("step", &RowView::step);
        v.RegisterMember("choices", &RowView::choices);
        v.RegisterMember("sliders", &RowView::sliders);
        v.RegisterMember("key", &RowView::key);
        v.RegisterMember("more", &RowView::more);
        v.RegisterMember("sub", &RowView::sub);
    }
    c.RegisterArray<std::vector<RowView>>();
    if (auto v = c.RegisterStruct<NavView>()) {
        v.RegisterMember("name", &NavView::name);
        v.RegisterMember("count", &NavView::count);
    }
    c.RegisterArray<std::vector<NavView>>();
    c.Bind("srows", &m.rows);
    c.Bind("shown_rows", &m.shownRows);
    c.Bind("nav", &m.nav);
    c.Bind("sec", &m.sec);
    c.Bind("query", &m.query);
    c.Bind("title", &m.title);
    c.Bind("hint", &m.hint);
    c.Bind("search_hint", &m.searchHint);
    c.Bind("none", &m.none);
    c.Bind("console_btn", &m.consoleBtn);
    c.Bind("layout_btn", &m.layoutBtn);
    c.Bind("ground", &m.ground);
}

}  // namespace uagfx::uikit::options

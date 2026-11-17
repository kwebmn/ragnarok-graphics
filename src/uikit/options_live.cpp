#include "uikit/options_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>

namespace uagfx::uikit::options {
namespace {

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
 <div id="spane" class="spane"><div data-for="r, i : srows" data-if="r.label != ''"><div class="sgrp"
   data-if="r.group != ''">{{r.group}}</div><div class="smore ctl" data-if="r.more"
   data-event-click="set_more(i)">{{r.label}}</div><div class="sro" data-if="!r.more"
   data-class-sub="r.sub"><div class="st"><span
   class="sl">{{r.label}}</span><span class="shn" data-if="r.hint != ''">{{r.hint}}</span><span class="swn"
   data-if="r.warn != ''">{{r.warn}}</span></div><div
   class="sctl"><span class="tg ctl" data-if="r.toggle" data-class-on="r.on"
   data-event-click="set_toggle(i)"><span class="kn"></span></span><input class="ctl" data-if="r.slider"
   type="range" data-attr-min="r.min" data-attr-max="r.max" data-attr-step="r.step" data-attr-value="r.value"
   data-event-change="set_slider(i)"/><span class="ch ctl" data-if="r.slider"
   data-event-click="set_step(i, -1)">&#x2212;</span><span class="sv" data-if="r.slider">{{r.vtext}}</span><span
   class="ch ctl" data-if="r.slider" data-event-click="set_step(i, 1)">+</span><div class="chs"
   data-if="r.choices.size > 0 && r.choices[0].label != ''"><span class="ch ctl"
   data-for="c, j : r.choices" data-if="c.label != ''" data-class-on="c.on"
   data-event-click="set_choice(i, j)">{{c.label}}</span></div></div></div></div>
   <div class="snone" data-if="shown_rows == 0">{{none}}</div></div>
</div>
<div class="sfoot"><span class="sfh">{{hint}}</span><span class="btn ctl"
  data-event-click="open_console()">{{console_btn}}</span></div>
<div id="ring" class="ring"></div>
</body></rml>)RML";

}  // namespace

const char* rml() { return kRml; }

void pad(std::vector<RowView>& rows, size_t shownBefore) {
    while (rows.size() < std::max(shownBefore, kLeastRows)) rows.push_back(RowView{});
    for (RowView& r : rows)
        while (r.choices.size() < kMostChoices) r.choices.push_back(ChoiceView{});
}

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto v = c.RegisterStruct<ChoiceView>()) {
        v.RegisterMember("label", &ChoiceView::label);
        v.RegisterMember("on", &ChoiceView::on);
    }
    c.RegisterArray<std::vector<ChoiceView>>();
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
    c.Bind("ground", &m.ground);
}

}  // namespace uagfx::uikit::options

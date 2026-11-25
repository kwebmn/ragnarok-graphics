#include "uikit/options_live.hpp"

#include "uikit/applying.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>

namespace uagfx::uikit::options {
namespace {

// 3.0.21-beta.8: an element a list makes (data-for) carries no view of its own - .srow, .slw and .chw are plain
// wrappers, the views a level down (see fit() in the header); a slider's part and the choices exist only in the rows
// that have them
const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw ow sw" data-style-background-color="ground">
<div class="head"><span class="title">{{title}}</span><span class="hk">Alt+O</span><span class="sseg"><span
  class="ctl" data-class-on="simple" data-event-click="set_mode(0)">{{mode_simple}}</span><span class="ctl"
  data-class-on="!simple" data-event-click="set_mode(1)">{{mode_full}}</span></span></div>
<span class="x ctl" data-event-click="opt_close()">&#x2715;</span>
<div class="srch"><input class="ctl" type="text" data-value="query"/><span class="ph"
  data-if="query == ''">{{search_hint}}</span></div>
<div class="sbody">
 <div class="snav" data-if="!simple"><div class="sn ctl" data-for="n, i : nav" data-class-on="i == sec && query == ''"
   data-event-click="sec_go(i)"><span class="nn">{{n.name}}</span><span class="cnt"
   data-if="n.count > 0">{{n.count}}</span></div></div>
 <div id="spane" class="spane"><div class="snote" data-if="note != ''">{{note}}</div><div class="srow"
   data-for="r, i : srows"><div class="sgrp" data-if="r.group != ''">{{r.group}}<span class="srst ctl"
   data-if="r.reset != ''" data-event-click="set_reset(i)">{{r.reset}}</span></div><div class="ssub"
   data-if="r.subhead">{{r.label}}</div><div class="scw" data-for="c, j : r.cards"><div class="scard"
   data-class-off="!c.on"><div class="scb"><span class="scn">{{c.label}}</span><span
   class="scd">{{c.desc}}</span><span class="scm ctl" data-if="c.more != ''"
   data-event-click="card_more(i, j)">{{c.more}}</span><div class="sca" data-if="c.check"><span class="btn ok ctl"
   data-event-click="check_ok(i, j)">{{c.ok}}</span><span class="btn bad ctl"
   data-event-click="check_bad(i, j)">{{c.bad}}</span></div></div><span class="tg ctl" data-if="c.toggle"
   data-class-on="c.on" data-event-click="card_toggle(i, j)"><span class="kn"></span></span></div></div><div class="sro"
   data-if="!r.subhead && r.ncards == 0" data-class-sub="r.sub"
   data-class-det="r.det"><div class="st"><span
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
    for (RowView& r : rows) r.sliders.assign(r.slider && !r.subhead ? 1 : 0, 0);
}

int cull(Rml::ElementDocument* doc, bool all) {
    Rml::Element* pane = doc ? doc->GetElementById("spane") : nullptr;
    if (!pane) return 0;
    const float top = pane->GetAbsoluteTop(), view = pane->GetClientHeight();
    const float from = top - view, to = top + 2 * view;  // the view and a view's height over and under it
    int hidden = 0;
    for (int i = 0; i < pane->GetNumChildren(); ++i) {
        Rml::Element* row = pane->GetChild(i);
        if (!row->IsClassSet("srow")) continue;
        // the row's own lines (its wrapper stands in the line: no box of its own); none laid out yet - shown
        float first = 1e9f, last = -1e9f;
        for (int k = 0; k < row->GetNumChildren(); ++k) {
            Rml::Element* line = row->GetChild(k);
            const float height = line->GetOffsetHeight();
            if (height <= 0) continue;
            const float y = line->GetAbsoluteTop();
            first = std::min(first, y);
            last = std::max(last, y + height);
        }
        const bool off = !all && first <= last && (last < from || first > to);
        // The row's own property, not a class: a class set on one row has RmlUi match the theme's rules again for
        // every row of the pane and all in them (Element::DirtyDefinition, SelfAndSiblings) - measured 1.4 ms a
        // frame while scrolling 183 rows; the property costs nothing (as RmlUi's own data-visible does it)
        const bool was = row->GetLocalStyleProperties().count(Rml::PropertyId::Visibility) != 0;
        if (off && !was) row->SetProperty(Rml::PropertyId::Visibility, Rml::Property(Rml::Style::Visibility::Hidden));
        if (!off && was) row->RemoveProperty(Rml::PropertyId::Visibility);
        hidden += off ? 1 : 0;
    }
    return hidden;
}

bool sliderMoved(const Rml::Event& e, const Model& m, int row, int& value) {
    if (applying()) return false;  // the model's own values reaching the element: not a person's move
    if (row < 0 || size_t(row) >= m.rows.size() || !m.rows[size_t(row)].slider) return false;
    const RowView& r = m.rows[size_t(row)];
    value = std::clamp(int(std::lround(e.GetParameter<float>("value", 0.0f))), r.min, r.max);
    return value != r.value;
}

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto v = c.RegisterStruct<ChoiceView>()) {
        v.RegisterMember("label", &ChoiceView::label);
        v.RegisterMember("on", &ChoiceView::on);
    }
    c.RegisterArray<std::vector<ChoiceView>>();
    c.RegisterArray<std::vector<int>>();
    if (auto v = c.RegisterStruct<CardView>()) {
        v.RegisterMember("label", &CardView::label);
        v.RegisterMember("desc", &CardView::desc);
        v.RegisterMember("more", &CardView::more);
        v.RegisterMember("on", &CardView::on);
        v.RegisterMember("toggle", &CardView::toggle);
        v.RegisterMember("check", &CardView::check);
        v.RegisterMember("ok", &CardView::ok);
        v.RegisterMember("bad", &CardView::bad);
    }
    c.RegisterArray<std::vector<CardView>>();
    if (auto v = c.RegisterStruct<RowView>()) {
        v.RegisterMember("group", &RowView::group);
        v.RegisterMember("label", &RowView::label);
        v.RegisterMember("hint", &RowView::hint);
        v.RegisterMember("warn", &RowView::warn);
        v.RegisterMember("reset", &RowView::reset);
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
        v.RegisterMember("subhead", &RowView::subhead);
        v.RegisterMember("det", &RowView::det);
        v.RegisterMember("sub", &RowView::sub);
        v.RegisterMember("cards", &RowView::cards);
        v.RegisterMember("ncards", &RowView::ncards);
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
    c.Bind("note", &m.note);
    c.Bind("ground", &m.ground);
    c.Bind("simple", &m.simple);
    c.Bind("mode_simple", &m.modeSimple);
    c.Bind("mode_full", &m.modeFull);
}

}  // namespace uagfx::uikit::options

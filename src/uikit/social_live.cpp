#include "uikit/social_live.hpp"

#include <RmlUi/Core.h>

namespace uagfx::uikit::social {

const char* rml() {
    return R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw socw" data-style-background-color="ground">
<div class="tabs"><span class="tab ctl" data-for="t, i : tabs" data-class-on="i == tab"
  data-event-click="so_tab(i)">{{t}}</span></div>
<span class="x ctl" data-event-click="so_close()">&#x2715;</span>
<div class="shead"><span class="lbl">{{header}}</span><span class="cnt" data-if="count != ''">{{count}}</span>
<span class="sexp" data-if="exp != ''">{{exp}}</span><span class="sset ctl" data-if="settings != ''"
  data-event-click="so_settings()">{{settings}}</span></div>
<div class="list"><div class="prow" data-for="r, i : rows">
<div class="row ctl" data-class-on="r.idx == selected" data-class-off="!r.online" data-class-open="i == menu_row"
  data-class-pad="i == pad_row" data-event-click="so_choose(i)" data-event-dblclick="so_whisper(i)"
  data-event-mousedown="so_down(i)"><div class="face"><img data-if="r.face != ''" data-attr-src="r.face"/><span
  class="lt" data-if="r.face == ''">{{r.letter}}</span><span class="dot" data-class-on="r.online"></span></div>
<div class="who"><div class="l1"><span class="name">{{r.name}}</span><span class="crown"
  data-if="r.leader"> &#x2605;</span><span class="job">{{r.job}}</span></div>
<div class="hpb" data-if="r.hp >= 0"><div class="hpf" data-class-bad="30 > r.hp"
  data-style-width="r.hp + '%'"></div></div>
<div class="l2" data-class-bad="r.dead">{{r.where}}</div></div>
<div class="qa"><span class="qb ctl" data-if="r.q0 != ''" data-event-click="so_quick(i, 0)">{{r.q0}}</span><span
  class="qb ctl" data-if="r.q1 != ''" data-event-click="so_quick(i, 1)">{{r.q1}}</span></div></div>
<div class="pmenu" data-if="i == menu_row"><span class="mi ctl" data-for="m, j : menu" data-class-on="j == menu_focus"
  data-event-click="so_item(j)">{{m}}</span></div></div>
<div class="hint" data-if="rows.size == 0">{{empty}}</div></div>
<div class="addf" data-if="tab == 0"><div class="fld"><input class="ctl" type="text" data-value="add_name"/><span
  class="ph" data-if="add_name == ''">{{name_hint}}</span></div><div class="btn ctl"
  data-event-click="so_add()">{{add}}</div></div>
<div class="sfoot">{{hint}}</div>
<div id="ring" class="ring"></div>
</body></rml>)RML";
}

bool RowV::operator==(const RowV& o) const {
    return name == o.name && job == o.job && where == o.where && face == o.face && letter == o.letter && q0 == o.q0 &&
           q1 == o.q1 && idx == o.idx && hp == o.hp && online == o.online && leader == o.leader && dead == o.dead;
}

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto v = c.RegisterStruct<RowV>()) {
        v.RegisterMember("name", &RowV::name);
        v.RegisterMember("job", &RowV::job);
        v.RegisterMember("where", &RowV::where);
        v.RegisterMember("face", &RowV::face);
        v.RegisterMember("letter", &RowV::letter);
        v.RegisterMember("q0", &RowV::q0);
        v.RegisterMember("q1", &RowV::q1);
        v.RegisterMember("idx", &RowV::idx);
        v.RegisterMember("hp", &RowV::hp);
        v.RegisterMember("online", &RowV::online);
        v.RegisterMember("leader", &RowV::leader);
        v.RegisterMember("dead", &RowV::dead);
    }
    c.RegisterArray<std::vector<RowV>>();
    c.RegisterArray<std::vector<Rml::String>>();
    c.Bind("rows", &m.rows);
    c.Bind("tabs", &m.tabs);
    c.Bind("menu", &m.menu);
    c.Bind("header", &m.header);
    c.Bind("count", &m.count);
    c.Bind("exp", &m.exp);
    c.Bind("settings", &m.settings);
    c.Bind("empty", &m.empty);
    c.Bind("add_name", &m.addName);
    c.Bind("name_hint", &m.nameHint);
    c.Bind("add", &m.add);
    c.Bind("hint", &m.hint);
    c.Bind("tab", &m.tab);
    c.Bind("selected", &m.selected);
    c.Bind("menu_row", &m.menuRow);
    c.Bind("menu_focus", &m.menuFocus);
    c.Bind("pad_row", &m.padRow);
    c.Bind("ground", &m.ground);
}

}  // namespace uagfx::uikit::social

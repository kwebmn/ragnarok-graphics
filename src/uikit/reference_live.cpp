#include "uikit/reference_live.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <RmlUi/Core/Element.h>

namespace uagfx::uikit::reference {

bool RailV::operator==(const RailV& o) const {
    return name == o.name && icon == o.icon && count == o.count && on == o.on;
}

namespace {

// The window: the header, the rail at the left, the page's body scrolled, the bottom - the body's markup made by
// body() and put in by data-rml; the window's click listener on #body maps data-ln to the row's link
const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="rfw" data-class-narrow="narrow" data-style-background-color="ground" data-style-width="w"
  data-style-height="h">
<div class="rhead">
 <span class="rbk ctl" data-if="back != ''" data-event-click="ref_back()">&#x2039; {{back}}</span>
 <span class="rhm ctl" data-if="home" data-event-click="ref_home()"><img data-attr-src="home_icon"/></span>
 <span class="rtt">{{title}}</span>
 <span id="rcr" class="rcr"><span class="rcb" data-for="c, i : crumbs"><span class="sep"
   data-if="i > 0">&#x203A;</span>{{c}}</span></span>
 <div class="rsrch"><input id="rq" class="ctl" type="text" data-value="query"/><span class="ph"
   data-if="query == ''">{{find}}</span><span class="kbd" data-if="query == ''">Ctrl+F</span></div>
 <span class="rst ctl" data-if="star != ''" data-class-on="starred"
   data-event-click="ref_star()">{{star}}</span>
 <span class="rx ctl" data-event-click="ref_close()">&#x2715;</span>
</div>
<div class="rmid">
 <div class="rrail" data-if="rail.size > 0">
  <div class="rri ctl" data-for="r, i : rail" data-class-on="r.on" data-event-click="ref_rail(i)"><img
    data-if="r.icon != ''" data-attr-src="r.icon"/><span class="rrn">{{r.name}}</span><span class="rrc"
    data-if="r.count != ''">{{r.count}}</span></div>
 </div>
 <div id="scroll" class="rscroll"><div class="rfix" data-if="note != ''">{{note}}</div><div id="body" class="rbody"
   data-rml="body"></div></div>
</div>
<div class="rfoot"><span class="rchip ctl" data-if="pack != ''" data-event-click="ref_chip(0)">{{pack}}</span><span
  class="rchip ctl" data-if="live != ''" data-class-fresh="live_state == 1" data-class-off="live_state == 2"
  data-class-stale="live_state == 3" data-event-click="ref_chip(1)">{{live}}</span><span
  class="rkeys">{{keys}}</span></div>
<div id="ring" class="ring"></div>
<div id="rcross" class="rcross"><div class="rch"></div><div class="rcv"></div></div>
</body></rml>)RML";

using refpages::Row;
using refpages::Style;

std::string hex(uint32_t rgb) {
    char b[8];
    std::snprintf(b, sizeof b, "#%06x", unsigned(rgb & 0xFFFFFF));
    return b;
}

// Words with their marked parts (byte ranges [from, to), the query found in them) as gold spans
std::string marked(const std::string& s, const refpages::Marks& marks) {
    if (marks.empty()) return escape(s);
    std::vector<std::pair<uint16_t, uint16_t>> m = marks;
    std::sort(m.begin(), m.end());
    std::string out;
    size_t at = 0;
    for (const auto& [from, to] : m) {
        const size_t f = std::min<size_t>(std::max<size_t>(from, at), s.size()), t = std::min<size_t>(to, s.size());
        if (t <= f) continue;
        out += escape(s.substr(at, f - at)) + "<span class=\"mk\">" + escape(s.substr(f, t - f)) + "</span>";
        at = t;
    }
    return out + escape(s.substr(std::min(at, s.size())));
}

struct Maker {
    const refpages::Page& p;
    const Look& look;
    std::string o;

    // The row's element's own attributes: its id, its classes (and «ctl» when it leads somewhere), data-ln
    std::string open(const char* tag, int i, const std::string& cls) const {
        const Row& r = p.rows[size_t(i)];
        std::string c = cls;
        const bool leads = r.link.go != refpages::Go::None;
        if (leads) c += " ctl";
        if (i == look.selected && leads) c += " sel";
        std::string s = std::string("<") + tag + " id=\"r" + std::to_string(i) + "\" class=\"" + c + "\"";
        if (leads) s += " data-ln=\"" + std::to_string(i) + "\"";
        return s + ">";
    }
    // 3.0.18-beta.242: a monster's «Lv N» in its words coloured by the character's level (refpages::Row::lvTone)
    static std::string toned(const Row& r, const std::string& words) {
        std::string s = escape(words);
        if (r.lvTone < 1 || r.lvTone > 3) return s;
        const size_t at = s.find("Lv ");
        if (at == std::string::npos) return s;
        size_t end = at + 3;
        while (end < s.size() && s[end] >= '0' && s[end] <= '9') ++end;
        if (end == at + 3) return s;
        return s.substr(0, at) + "<span class=\"rlv t" + std::to_string(r.lvTone) + "\">" + s.substr(at, end - at) +
               "</span>" + s.substr(end);
    }
    // A portrait's words from the game, line by line
    static std::string lines(const std::string& words) {
        std::string out;
        size_t from = 0;
        while (from <= words.size()) {
            const size_t nl = words.find('\n', from);
            const std::string one = words.substr(from, nl == std::string::npos ? std::string::npos : nl - from);
            out += (out.empty() && from == 0 ? "" : "<br/>") + escape(one);
            if (nl == std::string::npos) break;
            from = nl + 1;
        }
        return out;
    }
    std::string pic(const std::string& key, const char* cls) const {
        const std::string src = look.picture && !key.empty() ? look.picture(key) : std::string();
        // 3.0.18-beta.247 (R-ZW43KT): not made yet - a placeholder naming its picture; the window puts the picture
        // on it in place (a decorator: no layout, no new markup)
        if (src.empty())
            return std::string("<span class=\"") + cls + " none\"" +
                   (key.empty() ? "" : " data-pic=\"" + escape(key) + "\"") + "></span>";
        return std::string("<img class=\"") + cls + "\" src=\"" + escape(src) + "\"/>";
    }
    std::string icon(const Row& r) const {
        if (r.icon.empty()) return {};
        if (r.icon.rfind("mark:", 0) == 0) return "<span class=\"lg lg-" + escape(r.icon.substr(5)) + "\"></span>";
        return pic(r.icon.rfind("ui:", 0) == 0 ? r.icon : "icon:" + r.icon, "ri");
    }
    std::string dot(const Row& r) const {
        return r.dot ? "<span class=\"rdot\" style=\"background-color: " + hex(r.dot) + ";\"></span>" : std::string();
    }
    std::string tags(const Row& r) const {
        if (r.tags.empty()) return {};
        std::string s = "<div class=\"rtags\">";
        for (const std::string& t : r.tags) {
            if (t.rfind("el", 0) == 0 && t.find(':') != std::string::npos && t.find(':') > 2) {  // «elN:» an element's
                const int e = std::atoi(t.c_str() + 2);
                s += "<span class=\"rtag\"><span class=\"rdot\" style=\"background-color: " +
                     hex(refpages::elementColour(e)) + ";\"></span>" + escape(t.substr(t.find(':') + 1)) + "</span>";
            } else if (t.rfind("gold:", 0) == 0) {
                s += "<span class=\"rtag gold\">" + escape(t.substr(5)) + "</span>";
            } else {
                s += "<span class=\"rtag\">" + escape(t) + "</span>";
            }
        }
        return s + "</div>";
    }
    // A cell of the elements' table by its per cent: more than 100 good, below 100 weak, below 0 heals
    static std::string cell(const std::string& t) {
        const int v = std::atoi(t.c_str());
        const char* tone = v < 0 ? " heal" : v > 100 ? " up" : v < 100 ? " dn" : "";
        return std::string("<span class=\"rgc") + tone + "\">" + escape(t) + "</span>";
    }

    void line(int i) {  // one row by its style
        const Row& r = p.rows[size_t(i)];
        switch (r.style) {
            case Style::Title:
                o += open("div", i, "rtitle") + escape(r.left) +
                     (r.right.empty() ? "" : "<span class=\"rrt\">" + escape(r.right) + "</span>") + "</div>";
                break;
            case Style::Sub: o += open("div", i, "rsubl") + escape(r.left) + "</div>"; break;
            case Style::Note: o += open("div", i, "rnote") + escape(r.left) + "</div>"; break;
            case Style::Text: o += open("div", i, "rtext") + escape(r.left) + "</div>"; break;
            case Style::Hint: o += open("div", i, "rhint") + escape(r.left) + "</div>"; break;
            case Style::Caption: o += open("div", i, "rcap") + escape(r.left) + "</div>"; break;
            case Style::Warn: o += open("div", i, "rwarn") + escape(r.left) + "</div>"; break;
            case Style::More: o += open("div", i, "rmore") + escape(r.left) + "</div>"; break;
            case Style::Search: break;  // the header's field is the one (the owner: «одно поле»)
            case Style::Empty:
                o += open("div", i, "rempty") + "<div class=\"rel\">" + escape(r.left) + "</div><div class=\"rsb\">" +
                     escape(r.sub) + "</div></div>";
                break;
            case Style::Heading:
            case Style::Block:
            case Style::Group: {
                std::string arrow;
                if (r.fold >= 0) arrow = r.closed ? "&#x25B8; " : "&#x25BE; ";
                o += open("div", i, r.style == Style::Group ? "rsh rgroup" : "rsh") + "<span class=\"rshn\">" +
                     arrow + escape(r.left) + "</span><span class=\"rshr\">" + escape(r.right) + "</span></div>";
                break;
            }
            case Style::Portrait: {  // 3.0.18-beta.240: no picture (a location's: its map below), or one that
                // cannot be made (an NPC's sprite, a monster without one: portraits are made at once) - no box
                const std::string src = look.picture && !r.picture.empty() ? look.picture(r.picture) : std::string();
                const std::string box = "<div class=\"rpic\"><img class=\"rpimg\" src=\"" + escape(src) + "\"/></div>";
                o += open("div", i, "rport") + (src.empty() ? "" : box) +
                     "<div class=\"rpinfo\"><div class=\"rpl\">" + escape(r.left) + "</div>" +
                     (r.sub.empty() ? "" : "<div class=\"rpd\">" + lines(r.sub) + "</div>") +
                     (r.right.empty() ? "" : "<div class=\"rpr\">" + escape(r.right) + "</div>") + tags(r) +
                     "</div></div>";
                break;
            }
            case Style::Tile:
                o += open("div", i, "rtile") + icon(r) + "<div class=\"rtn\">" + escape(r.left) + "</div>" +
                     (r.right.empty() ? "" : "<div class=\"rtc\">" + escape(r.right) + "</div>") +
                     (r.sub.empty() ? "" : "<div class=\"rts\">" + escape(r.sub) + "</div>") + "</div>";
                break;
            case Style::Chip: {
                std::string picture;
                if (!r.picture.empty()) picture = pic(r.picture, "ri");
                else picture = icon(r);
                o += open("div", i, "rchp") + picture + dot(r) + "<span class=\"rcn\">" + escape(r.left) + "</span>" +
                     (r.sub.empty() ? "" : "<span class=\"rcs\">" + toned(r, r.sub) + "</span>") +
                     (r.right.empty() ? "" : "<span class=\"rcs\">" + toned(r, r.right) + "</span>") + "</div>";
                break;
            }
            case Style::Result:
                o += open("div", i, "rres") + icon(r) + dot(r) + "<div class=\"rrs\"><span class=\"rl\">" +
                     marked(r.left, r.marks) + "</span>" +
                     (r.sub.empty() ? "" : "<span class=\"rsb\">" + marked(r.sub, r.subMarks) + "</span>") +
                     "</div><span class=\"rrt\">" + toned(r, r.right) + "</span></div>";
                break;
            case Style::Grid: {
                o += open("div", i, r.gold ? "rgrid hd" : "rgrid") + "<span class=\"rgl\">" + escape(r.left) +
                     "</span>";
                for (const std::string& t : r.tags)
                    o += r.gold ? "<span class=\"rgc\">" + escape(t) + "</span>" : cell(t);
                o += "</div>";
                break;
            }
            case Style::Tab:
                o += open("span", i, r.gold ? "rwtab on" : "rwtab") + icon(r) + dot(r) + escape(r.left) +
                     (r.right.empty() ? "" : " <span class=\"rrt\">" + escape(r.right) + "</span>") + "</span>";
                break;
            case Style::Tool: o += open("span", i, "rwtool") + icon(r) + escape(r.left) + "</span>"; break;
            case Style::Legend:
                o += open("div", i, "rleg") + icon(r) + "<span class=\"rl\">" + escape(r.left) + "</span></div>";
                break;
            case Style::Step:
                o += open("div", i, r.tone > 0 ? "rstep paid" : "rstep") + "<span class=\"rsn\">" + escape(r.right) +
                     "</span><div class=\"rss\"><div class=\"rl\">" + escape(r.left) + "</div>" +
                     (r.sub.empty() ? "" : "<div class=\"rsb\">" + escape(r.sub) + "</div>") + "</div></div>";
                break;
            case Style::Button: o += open("div", i, r.gold ? "rbtn main" : "rbtn") + escape(r.left) + "</div>"; break;
            case Style::World: {
                const std::string src = look.picture ? look.picture("world") : std::string();
                char size[64];
                std::snprintf(size, sizeof size, "width: %dpx; height: %dpx;", std::max(1, look.worldW),
                              std::max(1, look.worldH));
                o += std::string("<div id=\"wmap\" class=\"rwmap ctl\" style=\"") + size + "\">" +
                     (src.empty() ? "" : "<img id=\"wimg\" src=\"" + escape(src) + "\" style=\"" + size + "\"/>") +
                     "</div>";
                break;
            }
            case Style::Row:
            default:
                o += open("div", i, r.gold ? "rr hl" : "rr") + icon(r) + dot(r) + "<span class=\"rl\">" +
                     escape(r.left) + "</span>" +
                     (r.sub.empty() ? "" : "<span class=\"rsb\">" + toned(r, r.sub) + "</span>") +
                     (r.right.empty() ? "" : "<span class=\"rrt\">" + toned(r, r.right) + "</span>") + "</div>";
                break;
        }
    }

    // Rows from..to-1 in order, a run of tiles a grid, a run of chips one wrapped line
    void run(const std::vector<int>& rows) {
        for (size_t k = 0; k < rows.size();) {
            const Style st = p.rows[size_t(rows[k])].style;
            if (st == Style::Tile || st == Style::Chip || st == Style::Tab || st == Style::Tool) {
                const char* box = st == Style::Tile ? "rtiles" : st == Style::Chip ? "rchips" : "rwbar";
                o += std::string("<div class=\"") + box + "\">";
                while (k < rows.size()) {
                    const Style s2 = p.rows[size_t(rows[k])].style;
                    const bool same = st == Style::Tab || st == Style::Tool ? (s2 == Style::Tab || s2 == Style::Tool)
                                                                            : s2 == st;
                    if (!same) break;
                    line(rows[k++]);
                }
                o += "</div>";
                continue;
            }
            line(rows[k++]);
        }
    }
};

bool head(Style s) { return s == Style::Heading || s == Style::Block || s == Style::Group; }

struct Section {
    int head = -1;
    std::vector<int> rows;
};

// What a section takes in the column, roughly in rows (its heading, its lines, its chips four to a line)
int weight(const refpages::Page& p, const Section& s) {
    int lines = s.head >= 0 ? 2 : 0, chips = 0;
    for (const int i : s.rows) {
        const Style st = p.rows[size_t(i)].style;
        if (st == Style::Chip) ++chips;
        else if (st == Style::Text) lines += 1 + int(p.rows[size_t(i)].left.size() / 90);
        else ++lines;
    }
    return lines + (chips + 3) / 4;
}

size_t lineRows(const refpages::Page& p, const Section& s) {
    size_t n = 0;
    for (const int i : s.rows)
        if (p.rows[size_t(i)].style != Style::Chip) ++n;
    return n;
}

}  // namespace

const char* rml() { return kRml; }

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto v = c.RegisterStruct<RailV>()) {
        v.RegisterMember("name", &RailV::name);
        v.RegisterMember("icon", &RailV::icon);
        v.RegisterMember("count", &RailV::count);
        v.RegisterMember("on", &RailV::on);
    }
    c.RegisterArray<std::vector<RailV>>();
    c.RegisterArray<std::vector<Rml::String>>();
    c.Bind("title", &m.title);
    c.Bind("back", &m.back);
    c.Bind("query", &m.query);
    c.Bind("find", &m.find);
    c.Bind("keys", &m.keys);
    c.Bind("pack", &m.pack);
    c.Bind("live", &m.live);
    c.Bind("star", &m.star);
    c.Bind("home_icon", &m.home_icon);
    c.Bind("body", &m.body);
    c.Bind("note", &m.note);
    c.Bind("ground", &m.ground);
    c.Bind("w", &m.w);
    c.Bind("h", &m.h);
    c.Bind("live_state", &m.live_state);
    c.Bind("home", &m.home);
    c.Bind("starred", &m.starred);
    c.Bind("narrow", &m.narrow);
    c.Bind("rail", &m.rail);
    c.Bind("crumbs", &m.crumbs);
}

std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (const char ch : s) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += ch;
        }
    }
    return out;
}

// 3.0.18-beta.240: a location's map - its bar (the layers' chips, the zoom), its box (the window's picture and words)
static void mapView(Maker& m, const Look& look) {
    m.o += "<div class=\"rmbar\">";
    for (size_t i = 0; i < look.layers.size(); ++i)
        m.o += std::string("<span class=\"rlay ctl") + (look.layers[i].second ? " on" : "") + "\" data-lay=\"" +
               std::to_string(i) + "\">" + escape(look.layers[i].first) + "</span>";
    const std::pair<int, const char*> tools[] = {{-1, "ui:minus"}, {0, "ui:expand"}, {1, "ui:plus"}};
    for (const auto& [by, icon] : tools)
        m.o += "<span class=\"rwtool ctl\" data-mz=\"" + std::to_string(by) + "\">" + m.pic(icon, "ri") + "</span>";
    const std::string src = look.picture ? look.picture("mapview") : std::string();
    char size[64];
    std::snprintf(size, sizeof size, "width: %dpx; height: %dpx;", std::max(1, look.mapW), std::max(1, look.mapH));
    m.o += std::string("</div><div id=\"mview\" class=\"rmview ctl\" style=\"") + size + "\">" +
           (src.empty() ? "" : "<img id=\"mimg\" src=\"" + escape(src) + "\" style=\"" + size + "\"/>") +
           look.mapWords + "</div>";
}

std::string body(const refpages::Page& p, const Look& look) {
    Maker m{p, look, {}};
    const int n = int(p.rows.size());
    // The world map's page: its bar over the map, the map at the left, its panel (the rows after it) at the right
    int world = -1;
    for (int i = 0; i < n; ++i)
        if (p.rows[size_t(i)].style == Style::World) world = i;
    if (world >= 0) {
        std::vector<int> bar, panel;
        for (int i = 0; i < world; ++i) bar.push_back(i);
        for (int i = world + 1; i < n; ++i) panel.push_back(i);
        m.run(bar);
        m.o += "<div class=\"rwrow\"><div class=\"rwleft\">";
        m.line(world);
        m.o += "</div><div class=\"rwpanel\">";
        m.run(panel);
        m.o += "</div></div>";
        return m.o;
    }
    // 3.0.18-beta.240: a location's card - its map at the left, its NPCs beside it (the rows marked side), the rest
    // below as any card's
    int view = -1;
    for (int i = 0; i < n; ++i)
        if (p.rows[size_t(i)].style == Style::MapView) view = i;
    std::vector<int> side;
    if (view >= 0) {
        std::vector<int> before;
        for (int i = 0; i < view; ++i) before.push_back(i);
        m.run(before);
        std::vector<int> heads;
        for (int i = view + 1; i < n; ++i)
            if (p.rows[size_t(i)].side) {
                side.push_back(i);
                if (p.rows[size_t(i)].style == Style::Group) heads.push_back(i);
            }
        m.o += "<div class=\"rmrow\"><div class=\"rmleft\">";
        mapView(m, look);
        m.o += "</div><div class=\"rmside\">";
        if (heads.size() >= 2) {
            m.o += "<div class=\"rjump\">";
            for (const int g : heads)
                m.o += "<span class=\"rjc ctl\" data-jump=\"r" + std::to_string(g) + "\">" +
                       escape(p.rows[size_t(g)].left) + "</span>";
            m.o += "</div>";
        }
        m.run(side);
        m.o += "</div></div>";
    }
    // The hero (the rows before the first heading), then the sections
    std::vector<int> hero;
    std::vector<Section> sections;
    for (int i = view >= 0 ? view + 1 : 0; i < n; ++i) {
        if (p.rows[size_t(i)].side) continue;
        if (head(p.rows[size_t(i)].style)) {
            sections.push_back(Section{i, {}});
            continue;
        }
        if (sections.empty()) hero.push_back(i);
        else sections.back().rows.push_back(i);
    }
    m.run(hero);
    // 3.0.18-beta.239 (the owner 13.11, question 2): two groups or more - their names on top, a jump to each
    std::vector<int> groups;
    for (const Section& s : sections)
        if (p.rows[size_t(s.head)].style == Style::Group) groups.push_back(s.head);
    if (groups.size() >= 2) {
        m.o += "<div class=\"rjump\">";
        for (const int g : groups)
            m.o += "<span class=\"rjc ctl\" data-jump=\"r" + std::to_string(g) + "\">" +
                   escape(p.rows[size_t(g)].left) + "</span>";
        m.o += "</div>";
    }
    auto section = [&](const Section& s, int columns) {
        m.o += "<div class=\"rsec\">";
        if (s.head >= 0) m.line(s.head);
        if (columns <= 1) {
            m.run(s.rows);
        } else {  // across: its rows in columns, top to bottom then the next
            const size_t per = (s.rows.size() + size_t(columns) - 1) / size_t(columns);
            m.o += "<div class=\"rsplit\">";
            for (int c = 0; c < columns; ++c) {
                const size_t from = size_t(c) * per, to = std::min(s.rows.size(), from + per);
                if (from >= to) break;
                m.o += "<div class=\"rspc\">";
                m.run(std::vector<int>(s.rows.begin() + long(from), s.rows.begin() + long(to)));
                m.o += "</div>";
            }
            m.o += "</div>";
        }
        m.o += "</div>";
    };
    if (look.narrow) {
        for (const Section& s : sections) section(s, 1);
        return m.o;
    }
    // Two columns, the next block into the shorter one; a long list across (its own columns) between them
    std::vector<const Section*> cols[2];
    int height[2] = {0, 0};
    auto flush = [&]() {
        if (cols[0].empty() && cols[1].empty()) return;
        m.o += "<div class=\"rcols\">";
        for (int c = 0; c < 2; ++c) {
            m.o += "<div class=\"rcol\">";
            for (const Section* s : cols[c]) section(*s, 1);
            m.o += "</div>";
        }
        m.o += "</div>";
        cols[0].clear(), cols[1].clear();
        height[0] = height[1] = 0;
    };
    for (const Section& s : sections) {
        const size_t lines = lineRows(p, s);
        if (lines > kLong) {
            flush();
            section(s, lines > kLonger && look.wide ? 3 : 2);  // 3.0.18-beta.248: three only when wide
            continue;
        }
        const int c = height[1] < height[0] ? 1 : 0;
        cols[c].push_back(&s);
        height[c] += weight(p, s);
    }
    flush();
    return m.o;
}

std::vector<Rml::Element*> padTargets(Rml::Element* body) {
    std::vector<Rml::Element*> out;
    if (!body) return out;
    Rml::ElementList found;
    body->QuerySelectorAll(found, ".ctl");
    for (Rml::Element* e : found)
        if (!e->HasAttribute("data-for") && e->IsVisible(true)) out.push_back(e);
    return out;
}

}  // namespace uagfx::uikit::reference

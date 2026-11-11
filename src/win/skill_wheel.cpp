#include "win/skill_wheel.hpp"

#include <cmath>
#include <string>
#include <vector>

#include "core/hotbar_panel.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "win/hotbar.hpp"
#include "win/real_ddraw.hpp"
#include "win/uaui.hpp"

namespace uagfx::win::skillwheelwin {
namespace {

constexpr int kSectors = 8;

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="swh" data-model="skillwheel" data-style-width="w + 'px'" data-style-height="w + 'px'">
<div class="ring" data-style-width="w + 'px'" data-style-height="w + 'px'"
  data-style-border-radius="w / 2 + 'px'"></div>
<div data-for="c : cells" data-attr-class="c.lit ? 'cell lit' : 'cell'" data-style-left="c.x + 'px'"
  data-style-top="c.y + 'px'" data-style-width="c.side + 'px'" data-style-height="c.side + 'px'"
  data-style-border-radius="c.side / 4 + 'px'"><img data-if="c.icon != ''" data-attr-src="c.icon"
  data-style-width="c.side * 0.7 + 'px'" data-style-height="c.side * 0.7 + 'px'"/><span
  class="fk">{{c.key}}</span></div>
<div class="mid" data-style-left="w / 2 - mid / 2 + 'px'" data-style-top="w / 2 - mid / 2 + 'px'"
  data-style-width="mid + 'px'" data-style-height="mid + 'px'" data-style-border-radius="mid / 2 + 'px'"><span
  class="name">{{name}}</span><span class="title">{{title}}</span></div>
</body></rml>)RML";

struct CellV {
    float x = 0, y = 0, side = 0;
    Rml::String icon, key;
    bool lit = false;
};

struct Ui {
    uaui::Doc doc;
    float w = 0, mid = 0;
    std::vector<CellV> cells;
    Rml::String name, title;
    std::string key;
    std::vector<std::string> sources = std::vector<std::string>(kSectors);
};

Ui& ui() {
    static Ui u;
    return u;
}

}  // namespace

void frame(int index, int sector, double k, int targetWidth, int targetHeight) {
    Ui& u = ui();
    if (index < 0 || k <= 0 || !uaui::ready()) {
        u.doc.show(false);
        return;
    }
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("skill wheel", kDoc, [&](Rml::DataModelConstructor& m) {
                if (auto c = m.RegisterStruct<CellV>()) {
                    c.RegisterMember("x", &CellV::x);
                    c.RegisterMember("y", &CellV::y);
                    c.RegisterMember("side", &CellV::side);
                    c.RegisterMember("icon", &CellV::icon);
                    c.RegisterMember("key", &CellV::key);
                    c.RegisterMember("lit", &CellV::lit);
                }
                m.RegisterArray<std::vector<CellV>>();
                m.Bind("w", &u.w);
                m.Bind("mid", &u.mid);
                m.Bind("cells", &u.cells);
                m.Bind("name", &u.name);
                m.Bind("title", &u.title);
            }))
            return;
    }
    const float s = uistyle::partScale(k);
    const float w = std::round(300 * s), side = std::round(54 * s), r = (w - side) / 2 - 6 * s;
    std::vector<CellV> cells;
    std::string key = std::to_string(index) + ':' + std::to_string(sector) + ':' + std::to_string(int(w));
    std::string name;
    for (int i = 0; i < kSectors; ++i) {
        const int slot = (index + 1) * 9 + i;
        const hotbar::Cell c = hotbarwin::slotCell(slot);
        const double a = double(i) * 3.14159265358979 / 4.0;  // the first at the top, clockwise
        CellV v;
        v.side = side;
        v.x = w / 2 + float(std::sin(a)) * r - side / 2;
        v.y = w / 2 - float(std::cos(a)) * r - side / 2;
        v.icon = c.icon ? uaui::picture(c.icon, uaui::stamp(c.icon), u.sources[size_t(i)]) : "";
        v.key = "F" + std::to_string(i + 1);
        v.lit = i == sector;
        if (v.lit) name = c.name;
        key += '|' + c.name + (c.icon ? "i" : "");
        cells.push_back(std::move(v));
    }
    if (key != u.key) {
        u.key = key;
        u.w = w, u.mid = std::round(120 * s);
        u.cells = std::move(cells);
        u.name = name;
        u.title = settings::tr(language(), settings::Text::PadWheelTitle) + std::string(" ") +
                  std::to_string(index + 1);
        u.doc.dirtyAll();
    }
    u.doc.place((targetWidth - int(w)) / 2, (targetHeight - int(w)) / 2);
    u.doc.show(true);
}

}  // namespace uagfx::win::skillwheelwin

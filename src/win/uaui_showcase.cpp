#include "win/uaui_showcase.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "core/char_status.hpp"
#include "win/game_camera.hpp"
#include "win/item_window.hpp"
#include "win/uaui.hpp"

namespace uagfx::win::uaui_showcase {
namespace {

struct ItemView {
    Rml::String icon;
    int count = 0;
};

const char* kDemo = R"(<rml><head><link type="text/template" href="uaui:window.rml"/><title>uaUI · компоненти</title></head>
<body template="ua-window" style="left: 120dp; top: 200dp;">
<tabset>
<tab>Речі</tab><panel>
<div class="ua-grid" data-if="items.size > 0"><div class="ua-slot" data-for="it : items"><img data-attr-src="it.icon"/><span class="ua-count" data-if="it.count > 1">{{it.count | num}}</span></div></div>
<span class="ua-label" data-if="items.size == 0">Відкрийте «Речі» (Alt+E) — тут з'являться предмети.</span>
</panel>
<tab>Список</tab><panel><div class="ua-list"><p data-for="line : lines">{{line}}</p></div></panel>
<tab>Кнопки</tab><panel>
<span class="ua-label">Кнопки</span><span class="ua-button">Звичайна</span><span class="ua-button">Ще одна</span>
<span class="ua-label">Перемикачі</span>
<div><input type="checkbox" checked="checked"/>Увімкнено</div>
<div><input type="checkbox"/>Вимкнено</div>
<span class="ua-label">Смуга (HP персонажа)</span><div class="ua-bar"><div class="ua-fill" data-style-width="hp_pct + '%'"></div></div>
</panel>
<tab>Форма</tab><panel>
<span class="ua-label">Поле (пишіть - рядок нижче змінюється сам)</span>
<div class="ua-row2"><input type="text" data-value="name" maxlength="24"/></div>
<span class="ua-label">Повзунок</span>
<div class="ua-row2"><input type="range" min="0" max="100" step="1" data-value="volume"/></div>
<span class="ua-label">Список</span>
<div class="ua-row2"><select data-value="pick"><option value="standard">Стандарт</option>
<option value="classic">Класика</option><option value="contrast">Контраст</option></select></div>
<span class="ua-label">Вибір одного</span>
<div><input type="radio" name="size" value="small" data-checked="size"/>Малий</div>
<div><input type="radio" name="size" value="big" data-checked="size"/>Великий</div>
<span class="ua-label">Те, що вибрано (прив'язка даних в обидва боки)</span>
<span class="ua-row2">{{name == '' ? '(порожньо)' : name}} · {{volume}} % · {{pick}} · {{size}}</span>
</panel>
</tabset>
</body></rml>)";


struct State {
    uaui::Doc doc;
    std::vector<ItemView> items;
    std::vector<Rml::String> lines;
    float hp = 0;
    Rml::String name, pick = "standard", size = "small";  // 3.0.18-beta.59: «Форма»
    int volume = 60;
    bool closed = false, was = false;
    ULONGLONG itemsAt = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

void frame(bool on) {
    State& s = state();
    if (on != s.was) s.closed = false, s.was = on;  // turned again: back after its ×
    if (!on || !uaui::ready()) {
        s.doc.show(false);
        return;
    }
    if (!s.doc.made()) {
        for (int i = 1; i <= 60; ++i)
            s.lines.push_back("Рядок " + std::to_string(i) + " — коліщатко миші гортає список, смуга праворуч тягнеться");
        const bool made = s.doc.make("showcase", kDemo, [&](Rml::DataModelConstructor& m) {
            if (auto item = m.RegisterStruct<ItemView>()) {
                item.RegisterMember("icon", &ItemView::icon);
                item.RegisterMember("count", &ItemView::count);
            }
            m.RegisterArray<std::vector<ItemView>>();
            m.RegisterArray<std::vector<Rml::String>>();
            m.Bind("items", &s.items);
            m.Bind("lines", &s.lines);
            m.Bind("hp_pct", &s.hp);
            m.Bind("name", &s.name);
            m.Bind("volume", &s.volume);
            m.Bind("pick", &s.pick);
            m.Bind("size", &s.size);
        });
        if (!made) return;
        s.doc.onClose = [] { state().closed = true; };
        if (Rml::Element* h = s.doc.byId("ua-hint"))
            h->SetInnerRML("Заголовок тягне вікно · × ховає · своя розмітка — uaro-graphics\\ui\\showcase.rml");
    }
    s.doc.show(!s.closed);
    if (s.closed) return;
    const ULONGLONG now = GetTickCount64();
    if (now - s.itemsAt >= 500) {  // the bag as «Речі» last read it, twice a second
        s.itemsAt = now;
        std::vector<ItemView> items;
        for (const auto& it : itemswin::lastBag()) {
            if (items.size() >= 60) break;
            char src[32];
            std::snprintf(src, sizeof src, "icon:%llx", (unsigned long long)uintptr_t(it.icon));
            items.push_back({it.icon ? src : "", it.count});
        }
        bool same = items.size() == s.items.size();
        for (size_t i = 0; same && i < items.size(); ++i)
            same = items[i].icon == s.items[i].icon && items[i].count == s.items[i].count;
        if (!same) s.items = std::move(items), s.doc.dirty("items");
    }
    charstatus::Status st;
    if (readOwnStatus(st)) {
        const float hp = st.hpMost > 0 ? 100.0f * float(st.hp) / float(st.hpMost) : 0.0f;
        if (hp != s.hp) s.hp = hp, s.doc.dirty("hp_pct");
    }
}

}  // namespace uagfx::win::uaui_showcase

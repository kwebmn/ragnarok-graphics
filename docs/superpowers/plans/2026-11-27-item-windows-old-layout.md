# Окна вещей в старой раскладке (часть 2 из 4) — план работ

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** все окна вещей — по раскладке и поведению старого клиента (kRO 2007), в нашем виде и двух темах, с новыми
возможностями на виду (подсказка клавиш внизу, кнопки), и у каждого действия один хозяин — наш пакет.

**Architecture:** окна — uaUI (RmlUi): разметка RML руками в `src/uikit/*_live.cpp` и `src/win/*`, стиль — общие
классы в `src/core/ui_theme_css.cpp` (тема их перекрашивает, часть 1). Общие детали (заголовок со счётом и клавишей,
вертикальные вкладки, строка-подсказка, нижняя полоса вес/места/зени, кнопки, строки списка) — один набор классов и
кусков разметки для всех окон. Действия — пакетами нашего слоя (`core/item_packets` → `core/item_state` →
`core/item_actions` → `win/item_net`, беты 3.0.24-beta.4–.18); новые пакеты — по `packet_db.txt` сервера для
PACKETVER 7 (только чтение). Каждое окно — своя галка для тестеров; окна вещей у игроков выключены с 3.0.24-beta.1.

**Tech Stack:** C++17, RmlUi (uaUI), тесты `build-host/uagfx_tests` (RmlUi на хосте — `tests/rml_host`).

**Spec:** `docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md` (разделы «Общие детали», «Окна вещей»,
«Решено владельцем 27.11», «Обязательно для всех окон», «Порядок» п. 3–4); эскизы:
https://uaro.kiev.ua/docs/item-windows/ (`bag_equip_*.jpg`, `storage_*.jpg`, `cart_dark.jpg`, `shop_buy_*.jpg`,
`shop_sell_dark.jpg`, `vend_buy_dark.jpg`, `vend_setup_dark.jpg`, `myshop_dark.jpg`, `trade_*.jpg`, `count_dark.jpg`,
`card_dark.jpg`).

**Части работы:** 1 — темы (выпущено 3.0.24-beta.19); 2 — окна вещей (этот план); 3 — остальной интерфейс; 4 — цены
с рынка сайта (строки «ринок сайту» в картке и «Відкрити лавку» появятся там).

## Global Constraints

- Сервер игры не меняем и не предлагаем менять; `packet_db.txt`, `clif.c` и базу только читаем.
- Пакеты — PACKETVER 7 (kRO Sakexe 2007-05-07): `dropitem 0x00a2` 6 {index@2 u16, amount@4 u16}; `useitem 0x00a7`
  17 {index@6 u16, account@13 u32}; `equipitem 0x00a9` 6 {index@2, location@4}; `unequipitem 0x00ab` 4 {index@2};
  ответы сервера: 0x00a8 7, 0x00aa 7, 0x00ac 7, 0x00af 6. Лавка: `openvending 0x01b2` −1 {len@2, имя 80 @4,
  flag@84, {index u16, amount u16, price u32}@85…}, `closevending 0x012e` 2; ответы 0x0136 −1, 0x0137 6. Обмен:
  `tradeadditem 0x00e8` 8 {index@2, amount@4}, `tradeok 0x00eb` 2, `tradecancel 0x00ed` 2, `tradecommit 0x00ef` 2;
  ответы 0x00e9 19, 0x00ea 5, 0x00ec 3, 0x00ee 2, 0x00f0 3. Расклад полей ответа сверяется с `clif.c` перед кодом.
- Окна — RML руками, проверка XML-парсером в тестах; без `SetProperty` каждый кадр; ширины в % — border-box.
- Скрытый `data-if` элемент живёт в документе: в списках новые части — внутри уже существующих частей строки (урок
  части 1, Alt+O: бюджет элементов на строку).
- Знаки — только те, что есть в шрифте; стрелки, звёзды, треугольники — рисованные значки.
- Тексты — три языка в `src/core/settings/strings.cpp`; слова окон — на языке игры.
- Строки ≤ 120; сборки `nice -n 19 ionice -c3`, не больше `-j4`; `build-host`, `build-win`, `build-win64`.
- Каждое окно — своя карточка в «Перевіряємо зараз» (`src/core/checks_now.cpp`); беты `gship.sh 3.0.24-beta.N`
  пачками (несколько окон в одной бете); выпуск — только по слову владельца.
- Продажа NPC (решено): «обране» в продажу не попадает; вещь с картой — всегда спрашиваем; заточка +5 и выше —
  спрашиваем. Обмін: красная рамка — что другой положил или изменил (и зени) за 5 с до своего «Готово». Моя лавка:
  последний покупатель и тихий звук продажи; налог сервера 2 % (`vending_tax 200`). Скільки: 1 / 10 / половина / усе.

## Review Focus

1. Ответ сервера не пришёл (пакет потерян, сервер перезапущен в 03:01) — действие не висит: через 2 с окно снова
   принимает нажатия, вещь показана как говорит сервер. Тест — в задачах с новыми пакетами (`Outcome::Timeout`).
2. Окно игры и наше над одной вещью одновременно — наше действие не дублирует игровое (галка пакетов: окно игры
   закрыто и спрятано). Тест: при включённой галке действие уходит одним пакетом, без нажатий в окно игры.
3. Пачка на 30 000 штук, вес почти полный, место кончилось — «скільки» не предлагает больше, чем влезет. Тест в
   задаче «Скільки».
4. Светлая тема: каждое новое окно читается (нет светлого на светлом) — цвета только через тему группы. Тест: новые
   цвета в C++ идут через `uaui::themed`/`uitheme::paint` (grep-тест в задаче общих деталей).
5. Пад и клавиатура доходят до каждой новой кнопки (`.ctl`/`data-hit`). Тест: в каждом окне новые кнопки — `ctl` или
   с `data-hit`.

---

### Task 1: Общие детали окон вещей

**Files:**
- Create: `src/core/item_ui.{hpp,cpp}` (чистое ядро)
- Modify: `src/core/ui_theme_css.cpp` (классы `.uw …`), `src/core/settings/strings.{hpp,cpp}` (строки-подсказки)
- Test: `tests/test_item_ui.cpp`, `tests/test_uikit_item_frame.cpp`

**Interfaces:**
- Produces:
  - `enum class itemui::Window : uint8_t { Items, Equip, Storage, Cart, ShopBuy, ShopSell, Vend, VendSetup, MyShop,
    Trade, Count, Card, Pick };`
  - `settings::Text itemui::hintOf(Window w)` — строка-подсказка клавиш окна (uk: Речі «ПКМ — опис · Alt+ПКМ —
    перенести цілим»; Склад «Ctrl, Shift — вибрати кілька · Alt+ПКМ — уся пачка»; Крамниця «клік — 1 · Shift — 10 ·
    ПКМ — картка»; …)
  - `struct itemui::Fill { int percent; const char* tone; };` `Fill itemui::fillOf(int64_t used, int64_t most)` —
    tone `"ok"` < 70 %, `"warn"` 70–89 %, `"full"` ≥ 90 % (красная)
  - `std::string itemui::countTitle(const std::string& name, int used, int most)` — «Речі 16/100»
  - `bool itemui::fontHas(const std::string& utf8)` — только буквы из `itemui::kLetters` (набор, сверенный с
    таблицей символов шрифта интерфейса)
  - классы RCSS: `.uw` (окно вещей), `.uw .uhd` (заголовок: `.ut` название со счётом, `.uk` клавиша, `.ux` ×),
    `.uw .vt` / `.vt .vb` / `.vb.on` (вертикальные вкладки), `.uw .uhint` (подсказка), `.uw .uft` (полоса: `.uwt`,
    `.ubar .ufill.ok|warn|full`, `.uz`), `.uw .ub` / `.ub.main` / `.ub.danger` (кнопки), `.uw .ur` (строка списка:
    `.ui` значок, `.un` название, `.uc` подпись, `.up` цена, `.uq` «− N +»)

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(item_ui_fill_tones_and_titles) {
    CHECK(std::string(itemui::fillOf(914, 5810).tone) == "ok" && itemui::fillOf(914, 5810).percent == 15);
    CHECK(std::string(itemui::fillOf(4100, 5810).tone) == "warn");
    CHECK(std::string(itemui::fillOf(5300, 5810).tone) == "full");
    CHECK(itemui::fillOf(1, 0).percent == 0 && itemui::fillOf(9, 5).percent == 100);
    CHECK(itemui::countTitle("Речі", 16, 100) == "Речі 16/100");
}
TEST(item_ui_every_window_has_its_hint_in_each_language) {
    // for every itemui::Window: tr(lang, hintOf(w)) is longer than 10 in en, uk, ru
}
TEST(item_ui_words_use_the_fonts_letters_only) {
    // R-WP985Q «непонятные символы в карточке»: every word of the item windows (their Text entries, each language)
    // holds only letters the interface's font has - Latin, Cyrillic, digits, the punctuation of itemui::kLetters
    // (checked against the font file once, by its cmap); arrows, stars, triangles are drawn icons, never letters
}
TEST(uikit_item_frame_tabs_stand_left_hint_and_foot_below) {
    // a sample document of .uw parts (head, .vt with 6 .vb, a grid box, .uhint, .uft) laid out by RmlUi:
    // the tabs one under another left of the grid; the hint under the grid; the foot at the bottom
}
```
- [ ] **Step 2:** run — не собирается (нет `core/item_ui`).
- [ ] **Step 3:** implement (`fillOf` — целые проценты, предел 0–100; классы RCSS по эскизу `bag_equip_dark.jpg`).
- [ ] **Step 4:** run — PASS; `build-win`.
- [ ] **Step 5:** commit `feat: the item windows' common parts - the head with its count, tabs on the left, the keys'
  hint, the weight and zeny bar, buttons and rows (core/item_ui)`

### Task 2: «Речі» — старая раскладка и действия пакетами

**Files:**
- Modify: `src/uikit/item_grid_live.{hpp,cpp}` (разметка: заголовок «Речі 16/100 Alt+E», вкладки слева, поиск,
  сетка, подсказка, полоса), `src/core/item_window.{hpp,cpp}` (раскладка: вкладки слева), `src/win/item_window.cpp`
  (тексты, действия), `src/core/item_packets.{hpp,cpp}`, `src/core/item_state.{hpp,cpp}`,
  `src/core/item_actions.{hpp,cpp}`, `src/win/item_net.{hpp,cpp}`, config / schema / catalogue / strings / live_config
  (`[interface] items_packets`, проба), `src/core/checks_now.cpp`
- Test: `tests/test_uikit_item_grid.cpp`, `tests/test_item_packets.cpp`, `tests/test_item_state.cpp`,
  `tests/test_item_actions.cpp`, `tests/test_catalogue.cpp` (порядок проб)

**Interfaces:**
- Consumes: Task 1 (классы `.uw`, `hintOf(Items)`, `fillOf`, `countTitle`).
- Produces: `itempackets::useItem(uint16_t index, uint32_t account)`, `equip(uint16_t index, uint16_t location)`,
  `unequip(uint16_t index)`, `drop(uint16_t index, uint16_t amount)`; `serverLength` 0x00a8/0x00aa/0x00ac → 7;
  `Change` от 0x00a8 (index, amount, result), 0x00aa / 0x00ac (index, location, result);
  `Queue::askUse/askEquip/askUnequip/askDrop(…, now, out)` → `Outcome` (Done / Refused / Timeout за 2 с);
  `itemnet::use/equip/unequip/drop`.

- [ ] **Step 1: Write the failing tests** — байты пакетов (длина, номер, поля по Global Constraints); разбор ответов;
  очередь: одно нажатие — один пакет, повтор до ответа не уходит, Timeout через 2 с; разметка: шесть вкладок одна под
  другой слева от сетки, подсказка и полоса под сеткой, заголовок со счётом.
- [ ] **Step 2:** run — FAIL.
- [ ] **Step 3:** implement. С галкой `items_packets`: двойной клик — использовать / надеть, перетаскивание на землю —
  выбросить (с «скільки»), снять — из «Спорядження»; без галки — как сейчас.
- [ ] **Step 4:** run; `build-win`, `build-win64`.
- [ ] **Step 5:** commit `feat: «Речі» in the old layout - tabs on the left, the count in the head, the keys' hint;
  use, equip, unequip and drop by our packets (trial)`

### Task 3: «Спорядження» — старая раскладка

**Files:** `src/uikit/equip_live.cpp`, `src/win/equip_window.cpp`, `src/core/ui_theme_css.cpp`
- Test: `tests/test_uikit_equip.cpp`

- [ ] **Step 1:** test — слоты двумя колонками вокруг фигурки по местам старого окна (верх / середина / низ голови,
  тіло, права / ліва рука, накидка, взуття, прикраса × 2), под ними «Стріли N» и «Комплекти 1 2 3»; заголовок
  «Спорядження Alt+Q»; подсказка внизу; окно стоит вплотную слева от «Речей».
- [ ] **Step 2–4:** FAIL → implement → PASS, сборки.
- [ ] **Step 5:** commit; бета: Task 1–3 + карточки «Речі: стара розкладка», «Речі: дії пакетами».

### Task 4: «Склад» и «Візок»

**Files:** `src/win/storage_window.cpp`, `src/win/cart_window.cpp`, `src/uikit/item_grid_live.cpp`,
`src/core/item_select.{hpp,cpp}` (новое: выбор нескольких)
- Test: `tests/test_item_select.cpp`, `tests/test_uikit_item_grid.cpp`

**Interfaces:**
- Produces: `itemselect::Set` — `click(index, ctrl, shift)`, `clear()`, `picked() -> std::vector<int>`;
  кнопка «Забрати вибране (N)»; места «921 / 2 338» в полосе; у «Візка» — полоса веса (красная ≥ 90 %) и кнопка
  «Відкрити лавку».

- [ ] **Step 1:** tests — Ctrl добавляет / убирает, Shift — от последнего до этого, клик без клавиш — один; кнопка
  забирает выбранные очередью (одно нажатие — пакет на вещь); полоса «Візка» красная при 90 %.
- [ ] **Step 2–4:** FAIL → implement → PASS, сборки.
- [ ] **Step 5:** commit; карточки «Склад: вибір кількох», «Візок: стара розкладка».

### Task 5: «Крамниця NPC» — купить и продать

**Files:** `src/uikit/shop_live.cpp`, `src/win/shop_window.cpp`, `src/core/shop_guard.{hpp,cpp}` (новое)
- Test: `tests/test_shop_guard.cpp`, `tests/test_uikit_shop.cpp`

**Interfaces:**
- Produces: `enum class shopguard::Ask { None, Card, Refine };` `Ask shopguard::askBeforeSelling(int refine,
  int cardsInside)` (карта — всегда, +5 и выше); `bool shopguard::sellable(bool favourite)` («обране» — нет);
  `shopguard::After after(int64_t zeny, int weight, int mostWeight, const Cart&)` — «залишиться» зени и вес.

- [ ] **Step 1:** tests — правила продажи; остаток после покупки; разметка: список и кошик рядом, поиск, «Купити» /
  «Скасувати»; продажа: «Різне» одной кнопкой, двойной клик — пачка.
- [ ] **Step 2–4:** FAIL → implement → PASS, сборки.
- [ ] **Step 5:** commit; карточки «Крамниця: купити», «Крамниця: продати».

### Task 6: «Лавка торговця»

**Files:** `src/win/vend_shop.cpp`, `src/core/ui_theme_css.cpp`
- Test: `tests/test_vend_shop.cpp` (вес после покупки), `tests/test_uikit_*` (разметка)

- [ ] **Step 1:** tests — список и кошик, вес после покупки, ПКМ — картка (с картами и заточкой).
- [ ] **Step 2–4:** FAIL → implement → PASS, сборки.
- [ ] **Step 5:** commit; бета: Task 4–6.

### Task 7: «Скільки штук»

**Files:** `src/win/ask_card.cpp`, `src/core/count_limit.{hpp,cpp}` (новое)
- Test: `tests/test_count_limit.cpp`

**Interfaces:**
- Produces: `int countlimit::most(int stack, int unitWeight, int weightFree, int placesFree, bool stacks)` — сколько
  влезет; текст «у склад влізе 87»; кнопки 1 / 10 / половина / усе.

- [ ] **Step 1:** tests — пачка 30 000 при 87 свободного веса единиц по 1 — 87; без места — 0; «половина» от 7 — 3.
- [ ] **Step 2–4:** FAIL → implement → PASS, сборки.
- [ ] **Step 5:** commit; карточка «Скільки штук: кнопки й межа».

### Task 8: «Картка речі»

**Files:** `src/win/info_card.cpp`, `src/core/info_card.{hpp,cpp}`
- Test: `tests/test_info_card.cpp`

- [ ] **Step 1:** tests — строки «що дає карта» (из описания карты клиента: первые строки до пустой), «вільних
  гнізд: N», цена у NPC (половина цены продажи NPC — из таблиц справочника, если есть).
- [ ] **Step 2–4:** FAIL → implement → PASS, сборки.
- [ ] **Step 5:** commit; бета: Task 7–8.

### Task 9: «Відкрити лавку» и «Моя лавка» — новые пакеты

**Files:** `src/core/item_packets`, `src/core/item_state`, `src/core/item_actions`, `src/win/item_net`,
`src/win/vend_setup.cpp`, `src/win/my_shop.cpp`, `src/core/vend_ledger.{hpp,cpp}` (новое), config…
(`[interface] vend_open_packets`, проба)
- Test: `tests/test_item_packets.cpp`, `tests/test_vend_ledger.cpp`

**Interfaces:**
- Produces: `itempackets::openVending(const std::string& name, const std::vector<VendRow>& rows)` (0x01b2),
  `closeVending()` (0x012e); `vendledger::Ledger` — продано N из M, заработано с налогом 2 %, последний покупатель
  (строка сервера «%s has bought your item(s).»), цены по номеру вещи — запоминаются (`uaro-graphics\vend_prices.txt`).

- [ ] **Step 1:** tests — байты 0x01b2 (имя 80 байт с нулём, flag 1, строки), разбор 0x0136 / 0x0137, бухгалтерия
  (налог 2 % с суммы, «усе» когда распродано), имя покупателя из строки сервера, цены сохраняются и читаются.
- [ ] **Step 2–4:** FAIL → implement → PASS, сборки.
- [ ] **Step 5:** commit; карточки «Відкрити лавку: пакети», «Моя лавка: продано й заробіток».

### Task 10: «Обмін» — новые пакеты

**Files:** `src/core/item_packets`, `src/core/item_state`, `src/core/trade_state.{hpp,cpp}` (новое),
`src/win/item_net`, `src/uikit/trade_live.cpp`, `src/win/trade_window.cpp`, config… (`[interface] trade_packets`)
- Test: `tests/test_trade_state.cpp`, `tests/test_item_packets.cpp`

**Interfaces:**
- Produces: `tradestate::State` — свои / чужие вещи и зени, «Готово» каждой стороны, `late(now)` — что другой
  положил или изменил за 5 с до своего «Готово» (красная рамка); пакеты `tradeAdd(index, amount)` (index 0 — зени),
  `tradeOk()`, `tradeCancel()`, `tradeCommit()`.

- [ ] **Step 1:** tests — чужая вещь (0x00e9) за 3 с до «Готово» — в `late`, за 7 с — нет; зени так же; отмена
  (0x00ee) очищает; завершение (0x00f0) — итог.
- [ ] **Step 2–4:** FAIL → implement → PASS, сборки.
- [ ] **Step 5:** commit; карточка «Обмін: пакети й червона рамка».

### Task 11: «Вибір речі»

**Files:** `src/win/pick_card.cpp`, `src/core/pick_card.{hpp,cpp}`
- Test: `tests/test_pick_card.cpp`

- [ ] **Step 1:** tests — значки и поиск по названию, ПКМ — картка; ответ — тем же путём, что и сейчас (пакеты
  выбора — после сверки с `packet_db.txt`, отдельной пробой, если ответ меняется).
- [ ] **Step 2–4:** FAIL → implement → PASS, сборки.
- [ ] **Step 5:** commit; бета: Task 9–11; память и заметки.

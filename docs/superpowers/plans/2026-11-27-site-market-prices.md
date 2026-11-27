# Цены с рынка сайта — план (часть 4 из 4)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** к строке «Ринок: від X z · лотів N» добавить среднюю цену сделок сайта (когда сделок три и больше) и
показать строку рынка в «Картці речі».

**Architecture:** данные уже идут: сайт `/api/public/game/live.json` (`GameLiveController`, market =
`[nameid, cheapest, lots, pieces]`), программа настроек качает его каждые 2 минуты (`src/app/reference_pack.cpp`)
в `uaro-graphics\reference\live.json`, игра читает его (`core/reference_live`, `win/stash_watch::prices` — подсказки
вещей и «Відкрити лавку»). Строка рынка получает два поля в конце: `[..., average, deals]` — старые клиенты читают
поля 0–3 и их не замечают.

**Tech Stack:** C++17 (клиент), Laravel / PHP (сайт на kozak), MySQL `c2uaronew` (`market_transactions`).

**Spec:** `docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md` — «Цены с рынка сайта».

## Global Constraints

- Сайт: правки кода только ночью по Киеву, перед правкой — копия в `private/backup-…`; конфиги сервера не трогаем
  (только через ISPConfig). Сервер игры не трогаем.
- Спецификация: «показывать «на ринку сайту від X z (N пропозицій)» по текущим лотам и среднюю — только при трёх
  сделках и больше»; «Нет файла или вещи — строки «ринок сайту» нет».
- Старые клиенты и старый файл: новый клиент читает и строки из 4 полей; новые поля — в конце строки.
- Строки ≤ 120; тексты на трёх языках; «» только вокруг названий интерфейса; беты `gship.sh 3.0.24-beta.N`.

## Review Focus

1. Новый клиент со старым `live.json` (сайт ещё не обновлён) — строка как раньше, без средней. Тест: Task 1
   `reference_live_reads_the_deals_average` (строка из 4 полей).
2. Две сделки — средней нет, одна строка лотов. Тест: Task 1 `market_line_words`.
3. Вещь со сделками, но без активных лотов — только «середня …». Тест: Task 1 `market_line_words`.
4. Сделка с нулём штук (amount 0) не делит на ноль на сайте. Проверка: Task 3, `sum(amount) > 0` в запросе.
5. Файл старше свежести (`fresh`) — строки нет (как и раньше). Тест: уже есть в тестах `reference_live`.

---

### Task 1: Средняя цена сделок в `live.json` и слова строки рынка

**Files:** `src/core/reference_live.{hpp,cpp}`, `src/core/settings/strings.{hpp,cpp}`; Test:
`tests/test_market_prices.cpp` (новый).

**Interfaces:**
- Produces: `reflive::Market { int item; long long cheapest; int lots, pieces; long long average = 0; int deals = 0; }`
  — `average`, `deals` из полей 4 и 5 строки; строка сохраняется, если `(cheapest > 0 && lots > 0) || deals > 0`;
  `write()` пишет 6 полей. `constexpr int kAverageFrom = 3;`
  `std::string reflive::marketLine(const Market& m, const std::string& lotsForm, const std::string& averageForm)` —
  лоты (`{1}` цена, `{2}` лотов) и через « · » средняя (`{1}` цена, `{2}` сделок) при `deals >= kAverageFrom`; цены
  `itemwin::grouped`; ничего — "".
- Texts: `ItemsMarketAverage` — «середня {1} z · угод {2}» / "average {1} z · {2} deals" / «средняя {1} z · сделок {2}».

- [ ] **Step 1:** tests `reference_live_reads_the_deals_average` (строка из 6 полей → average/deals; из 4 — deals 0;
  только сделки — сохранена; write → read сохраняет оба), `market_line_words` (лоты: «Ринок: від 95 000 z · лотів 2»;
  лоты и 10 сделок: «… · середня 100 000 z · угод 10»; 2 сделки — без средней; только 3 сделки — «середня …»;
  ничего — "").
- [ ] **Step 2–4:** FAIL → PASS.
- [ ] **Step 5:** commit.

### Task 2: Строка рынка в подсказках, «Відкрити лавку» и «Картці речі»

**Files:** `src/win/stash_watch.cpp`, `src/win/info_card.cpp`, `src/core/checks_now.cpp`.

**Interfaces:**
- Consumes: `reflive::marketLine`, `Text::ItemsMarket`, `Text::ItemsMarketAverage`.

- [ ] **Step 1:** `stashwatch::prices` пишет `it.market = reflive::marketLine(...)` (подсказки вещей и «Відкрити
  лавку» берут его оттуда же); «Картка речі» добавляет строку рынка после «NPC заплатить», когда она не пустая.
- [ ] **Step 2:** сборки host/win/win64, тесты.
- [ ] **Step 3:** commit; карточка «Ринок сайту: середня ціна, у картці речі»; бета.

### Task 3: Сайт — средняя цена и число сделок в `live.json` (ночью)

**Files (kozak):** `/var/www/uaro.kiev.ua/private/laravel/app/Http/Controllers/Api/Public/GameLiveController.php`.

- [ ] **Step 1:** ночью: копия файла в `private/backup-site-market-avg-<дата>/`.
- [ ] **Step 2:** в `build()` — по `market_transactions` за 180 дней: `nameid`, `count(*)`, `sum(price)`,
  `sum(amount)` (группа по `nameid`, `sum(amount) > 0`); `average = round(sum(price) / sum(amount))`. В строку вещи
  с лотами — поля 4 и 5; вещь только со сделками — `[id, 0, 0, 0, average, deals]`. Кэш — как был (60 с).
- [ ] **Step 3:** `php -l`, `artisan` кэш маршрутов не трогаем; проверка: `curl` к `live.json` — строки из 6 полей,
  у 2522 — `deals` 10; старый разбор (поля 0–3) не меняется.
- [ ] **Step 4:** запись в журнал и в память (что изменено на сайте, где копия).

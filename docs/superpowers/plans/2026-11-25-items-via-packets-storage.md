# Окна вещей через пакеты — этап 1: «Склад». План реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** «Склад» кладёт, забирает и закрывается собственными пакетами драйвера и показывает только то, что сказал
сервер; включается тестерам отдельной галкой.

**Architecture:** чистое ядро — таблица пакетов клиента 2007-05-07 (`core/item_packets`), состояние сумки и склада по
пакетам сервера (`core/item_state`), действия с ожиданием ответа (`core/item_actions`). Windows-слой: перехват
пакетов кормит состояние (`win/packet_tap`), отправка идёт через сессию пада (`uapad::runtime::sendPacket`), окно
«Склад» (`win/storage_window`) читает только состояние и шлёт только действия, если включено
`[interface] storage_packets`.

**Tech Stack:** C++20, mingw i686 (ddraw.dll), тесты `uagfx_tests` (build-host), RmlUi окна как есть.

**Spec:** `docs/superpowers/specs/2026-11-25-items-via-packets-design.md`

## Global Constraints

- Клиент kRO Sakexe 2007-05-07, PACKETVER 7; номера пакетов — из `/root/uAthena/db/packet_db.txt` (действующие для
  этой даты строки): movetokafra `0x0094` длина 14, индекс @7 (W), количество @10 (L); movefromkafra `0x00f7`
  длина 22, индекс @14 (W), количество @18 (L); closekafra `0x0193` длина 2.
- Индекс в пакетах склада и сумки — серверный индекс (сумка: позиция + 2, склад: позиция + 1, как в clif.c).
- Раскладки пакетов сервера сверять с `/root/uAthena/src/map/clif.c`: `clif_storagelist`, `clif_updatestorageamount`
  (0x00f2), `clif_storageitemadded` (0x01c4), `clif_storageitemremoved` (0x00f6), `clif_storageclose` (0x00f8),
  `clif_inventorylist`, `clif_additem` (0x00a0), `clif_delitem` (0x00af), `clif_useitemack` (0x01c8) при PACKETVER 7.
- Сервер uAthena не трогать.
- Отправлять только когда сессия считает поток понятым (`healthy()`), иначе действие отклоняется с записью в журнал.
- Тайм-аут ответа — 2000 мс.
- Новая галка по умолчанию выключена; всем включается только по слову владельца.
- Строки ≤ 120 символов, сборка `nice -n 19 ionice -c3 … -j4`, беты `3.0.24-beta.N` через `gship.sh`, заметки на
  трёх языках, подписи интерфейса из колонки uk strings.cpp.

## Review Focus

1. Список склада длиннее 2048 байт (сотни вещей): перехват читает пакет по его полю длины, а не по `kReadable`, —
   иначе склад покажется обрезанным.
2. Повторный клик или двойной клик по ячейке, пока ждём ответа: второй пакет не уходит.
3. Смена карты, разрыв или закрытие склада сервером, пока действие ждёт: ожидание снимается, окно не зависает.
4. Сервер молча не принял вещь (склад полон, нельзя класть такую вещь): по тайм-ауту окно говорит, что склад не
   принял, и ячейка снова доступна.
5. Количество больше, чем есть в пачке (окно «Скільки штук» и Alt+ПКМ): в пакет уходит не больше, чем есть.

---

### Task 1: Таблица пакетов клиента — `core/item_packets`

**Files:**
- Create: `src/core/item_packets.hpp`, `src/core/item_packets.cpp`
- Test: `tests/test_item_packets.cpp`

**Interfaces:**
- Produces: `namespace uagfx::itempackets { using Bytes = std::vector<uint8_t>;
  Bytes toStorage(uint16_t bagIndex, uint32_t amount); Bytes fromStorage(uint16_t storageIndex, uint32_t amount);
  Bytes closeStorage(); int serverLength(uint16_t id); }` — `serverLength`: длина пакета сервера по таблице
  (−1 переменная, 0 неизвестный) для id из Global Constraints и пакетов задачи 2.

- [ ] **Step 1: Write the failing test** `item_packets_storage_requests_as_the_server_reads_them`:
  `toStorage(5, 3)` → 14 байт, `[0..1] == 0x94,0x00`, `rd16(@7) == 5`, `rd32(@10) == 3`;
  `fromStorage(12, 40)` → 22 байта, `0xf7,0x00`, `rd16(@14) == 12`, `rd32(@18) == 40`;
  `closeStorage()` → `{0x93, 0x01}`; `serverLength(0x01f0) == -1`, `serverLength(0x00f6) == 8`,
  `serverLength(0x01c4) == 22`, `serverLength(0x1234) == 0`.
- [ ] **Step 2:** собрать `build-host`, запустить `./build-host/uagfx_tests` — ожидается ошибка сборки (нет файла).
- [ ] **Step 3:** реализовать; байты-заполнители между полями — нули.
- [ ] **Step 4:** тест PASS, весь набор зелёный.
- [ ] **Step 5:** коммит `feat: the client's item packets (core/item_packets) - storage put, take, close`.

### Task 2: Состояние сумки и склада — `core/item_state`

**Files:**
- Create: `src/core/item_state.hpp`, `src/core/item_state.cpp`
- Test: `tests/test_item_state.cpp`

**Interfaces:**
- Produces: `namespace uagfx::itemstate { struct Item { uint16_t index; uint16_t id; uint32_t amount;
  bool identified; uint8_t refine, attribute; uint16_t cards[4]; uint16_t equipMask; };
  enum class Where : uint8_t { Bag, Storage };
  struct Change { Where where; uint16_t index; int64_t delta; bool closed = false; };
  class State { public: std::optional<Change> feed(const uint8_t* p, size_t n);
  const std::vector<Item>& bag() const; const std::vector<Item>& storage() const; bool storageOpen() const;
  int storageMost() const; void reset(); }; }`
  `feed` принимает целый пакет сервера; возвращает, что изменилось (для `item_actions`), или пусто.

- [ ] **Step 1: Write the failing tests** (пакеты собраны в тесте по раскладкам из clif.c):
  `item_state_storage_list_and_count` — 0x01f0 с двумя пачками + 0x00a6 с одной вещью + 0x00f2 (2/600):
  `storage().size() == 3`, `storageOpen()`, `storageMost() == 600`;
  `item_state_storage_add_remove_close` — 0x01c4 (index 4, amount 5, id 501) добавляет, повторный 0x01c4 той же
  ячейки складывает количество, 0x00f6 (index 4, amount 5) убирает до нуля и удаляет, 0x00f8 → `!storageOpen()`
  и `Change{closed=true}`;
  `item_state_bag_add_and_delete` — 0x00a0 (результат 0) добавляет, 0x00a0 с результатом ≠ 0 ничего не меняет,
  0x00af уменьшает, 0x01c8 ставит остаток;
  `item_state_long_list_not_cut` — 0x01f0 на 300 пачек (5404 байта) даёт 300 вещей.
- [ ] **Step 2:** запустить — FAIL (нет `itemstate`).
- [ ] **Step 3:** реализовать по раскладкам clif.c (Global Constraints); короткий или битый пакет игнорируется.
- [ ] **Step 4:** PASS, набор зелёный.
- [ ] **Step 5:** коммит `feat: the bag's and the storage's things as the server tells them (core/item_state)`.

### Task 3: Действия с ожиданием — `core/item_actions`

**Files:**
- Create: `src/core/item_actions.hpp`, `src/core/item_actions.cpp`
- Test: `tests/test_item_actions.cpp`

**Interfaces:**
- Consumes: `itempackets::toStorage/fromStorage/closeStorage`, `itemstate::Change`, `itemstate::Where`.
- Produces: `namespace uagfx::itemactions { enum class Kind : uint8_t { ToStorage, FromStorage, CloseStorage };
  enum class Result : uint8_t { Sent, Busy, NotConnected, NothingToMove };
  enum class Outcome : uint8_t { None, Done, TimedOut, Dropped };
  constexpr uint32_t kTimeoutMs = 2000;
  class Queue { public: Result ask(Kind k, uint16_t index, uint32_t amount, uint32_t have, bool connected,
  uint32_t nowMs, itempackets::Bytes& out); Outcome onChange(const itemstate::Change& c);
  Outcome tick(uint32_t nowMs); bool waiting(itemstate::Where w, uint16_t index) const; void dropAll(); }; }`
  `ask` кладёт в `out` пакет, если вернул `Sent`; `amount` урезается до `have`; `have == 0` → `NothingToMove`.
  `ToStorage` закрывается изменением сумки по этому индексу (минус); `FromStorage` — изменением склада по этому
  индексу (минус); `CloseStorage` — `Change{closed}`.

- [ ] **Step 1: Write the failing tests:**
  `item_actions_one_press_one_packet` — `ask(ToStorage, 5, 3, 10, true, 0, out) == Sent`, повтор той же ячейки
  `== Busy`, другая ячейка `== Sent`; `waiting(Bag, 5)`;
  `item_actions_done_by_the_servers_answer` — `onChange(Change{Bag, 5, -3}) == Done`, `!waiting(Bag, 5)`;
  `item_actions_timeout_frees_the_cell` — без ответа `tick(1999) == None`, `tick(2000) == TimedOut`, ячейка свободна;
  `item_actions_never_more_than_there_is` — `ask(..., amount 50, have 7)` → в пакете количество 7;
  `ask(..., have 0) == NothingToMove`, `connected=false` → `NotConnected` и `out` пуст;
  `item_actions_closed_storage_drops_the_waiting` — ожидание `FromStorage`, затем `onChange(Change{closed=true})`
  `== Dropped` и `!waiting(Storage, …)`.
- [ ] **Step 2:** FAIL.
- [ ] **Step 3:** реализовать.
- [ ] **Step 4:** PASS, набор зелёный.
- [ ] **Step 5:** коммит `feat: item actions - one press one packet, answered or timed out (core/item_actions)`.

### Task 4: Проводка в драйвере — `win/item_net`

**Files:**
- Create: `src/win/item_net.hpp`, `src/win/item_net.cpp`
- Modify: `src/win/packet_tap.cpp` (вызов `itemnet::packet(id, packet)` в `take()`), `src/pad/win/runtime.hpp`,
  `src/pad/win/runtime.cpp` (новая `bool sendPacket(std::vector<uint8_t> p)` рядом с `walkTo`, тот же
  `healthy() && session` и `session->request`), место смены карты/разрыва (где сейчас сбрасываются окна —
  `itemnet::reset()`).

**Interfaces:**
- Consumes: задачи 1–3; `uapad::runtime::sendPacket`.
- Produces: `namespace uagfx::win::itemnet { void packet(uint16_t id, const uint8_t* p);
  const itemstate::State& state(); itemactions::Result ask(itemactions::Kind, uint16_t index, uint32_t amount);
  bool waiting(itemstate::Where, uint16_t index); void frame();  // tick тайм-аутов
  void reset(); const char* lastWords(); }`
  `lastWords()` — текст для окна после `TimedOut`/`NotConnected` (из strings, ключи задачи 5).

- [ ] **Step 1:** в `packet()` длину брать из `serverLength(id)`; для переменных — поле длины @2, не больше
  размера буфера цикла игры (0x54B4 байт); неизвестные id не трогать.
- [ ] **Step 2:** журнал: на каждое действие одна строка «items: <kind> index … amount … sent» и «… answered in N
  ms» / «… no answer in 2 s»; первые 20 собственных отправок игры с id 0x0094, 0x00f7, 0x0193 — строкой с id и
  длиной (сверка таблицы; наблюдатель клиентских данных сессии).
- [ ] **Step 3:** проверить в журнале игры, что при `[pad] enabled = 0` сессия создана (`runtime.cpp:1831`) и
  `sendPacket` проходит; если нет — создавать сессию без управления падом (Ruling в журнале плана).
- [ ] **Step 4:** собрать `build-win`, `build-win64`, `build-host`; набор зелёный.
- [ ] **Step 5:** коммит `feat: the item packets wired - the tap feeds the state, the pad's session sends`.

### Task 5: «Склад» через пакеты по галке тестера

**Files:**
- Modify: `src/core/config.hpp/.cpp` (`bool storagePackets = false;` ← `[interface] storage_packets`),
  `src/core/live_config.cpp`, `src/core/settings/schema.cpp` (Check, по умолчанию 0, Page::Interface),
  `src/core/settings/strings.cpp` (подпись и подсказка; uk «Склад: нова передача речей (бета)»), 
  `src/win/storage_window.cpp` (ветка действий и чтения данных).
- Test: `tests/test_config.cpp` (`config_interface_storage_packets`), `tests/test_item_actions.cpp` не меняется.

**Interfaces:**
- Consumes: `itemnet::state/ask/waiting/lastWords`.

- [ ] **Step 1: Write the failing test** `config_interface_storage_packets`: по умолчанию `!storagePackets`;
  `storage_packets = 1` → `storagePackets`; ключ есть в `settings::findSetting("interface.storage_packets")`.
- [ ] **Step 2:** FAIL, затем реализовать конфиг и схему; PASS.
- [ ] **Step 3:** в `storage_window.cpp` при `storagePackets` (и включённом окне склада): ячейки берутся из
  `itemnet::state().storage()` / `.bag()`; «положить», «забрать», двойной клик, Alt+ПКМ и «Готово» окна «Скільки
  штук» вызывают `itemnet::ask(...)` вместо прежних путей через окна игры; ✕ — `CloseStorage`; ячейка с
  `waiting()` рисуется приглушённой и не принимает нажатий; `lastWords()` показывается в подвале окна 5 с.
  Без галки — всё как сейчас.
- [ ] **Step 4:** собрать все три сборки; набор зелёный; строки ≤ 120.
- [ ] **Step 5:** коммит `feat: «Склад» by its own packets behind [interface] storage_packets (the testers' box)`,
  заметки uk/en/ru (где галка: программа настроек → «Інтерфейс» → «Склад: нова передача речей (бета)», нужен
  включённый «Склад Кафри»), `gship.sh 3.0.24-beta.N`.

# uaRO Graphics 0.5.0 — чёткий текст, этап 6 (план)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** текст, рамки и заливки окон игры — в настоящих точках окна при `k > 1`: драйвер ведёт двойник каждой
картинки окна и рисует плитки окон из него; выпуск 0.5.0 через `update.bat`.

**Architecture:** ядро без Win32 (сетка двойника, картинка-двойник с копией пикселей игры, буквы на местах раскладки
игры, кеш букв, проверка exe) тестируется на Linux. Модуль `win/crisp` подменяет 8 методов vtable картинки окна и три
импорта GDI exe, повторяет каждую запись игры в двойнике, загружает текстуры-двойники плиток, когда игра загружает
свои, и отмечает их у текстур плиток; `Device7::record` передаёт двойник плитки в рисование, рендер рисует такую
плитку новым режимом шейдера `uiMode = 3` — тексель на точку цели.

**Tech Stack:** C++17, MinGW-w64 i686 GCC 12, Direct3D 12, GDI, тесты на Linux.

**Spec:** `docs/superpowers/specs/2026-10-13-stage6-crisp-text-design.md` (уточнение этапа 6; основание —
`2026-10-13-uaro-graphics-design.md`, `2026-10-13-stage3-resolution-design.md`, исследование `docs/research/text.md`).

## Global Constraints

- Всё из прежних планов в силе: PE32 i386; импорты только `KERNEL32`, `USER32`, `GDI32`, `msvcrt`; пролог горячего
  патча у `DirectDrawCreateEx`; откат на dgVoodoo; сборка без предупреждений.
- Версия — `0.5.0`.
- Чёткий текст — только при `k > 1`, по умолчанию `[display] crisp_text = 1`; Pause (клавиша сравнения) выключает его
  вместе с остальными улучшениями (спец. §1).
- Другая версия exe или любое несовпадение при проверке — `crisp text: off (<причина>)`, ничего не подменяется (спец.
  §5).
- Каждая подмена метода игры снимает со стека ровно столько байт, сколько оригинал (`ret N` в `uaRO.exe`):
  слоты 0 и 2 — 4, слот 4 — 0x18, слоты 5, 6, 9 — 0x14, слот 7 — 0x28, слот 8 — 8; импорты GDI — `WINAPI`.
- Мелкая картинка игры всегда ровно такая, как без драйвера: подмена сначала вызывает оригинал.
- Стиль и коммиты — как раньше (KWEB Team, русские сообщения, трейлер `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`).

## Review Focus

1. **Края окон и стыки плиток при дробном `k` и любом месте окна, в том числе отрицательном**: каждая точка игры
   видна, плитки без шва, точка цели берёт одну точку двойника — эталонный растеризатор в Task 1.
2. **Картинка на том же адресе** (облачка пересоздаются при каждой смене текста): старый двойник не показывается —
   проверка точек и размера в `picture()`, сброс в `CreateDIBSection` и деструкторе (Task 6), забывание поверхности
   плитки (Task 8); вычитка.
3. **Текст поверх текста** (обводка — 5 вызовов, чат перерисовывается каждый кадр): копия поглощает мелкие буквы,
   сверка не тянет их в двойник — тесты `absorb`/`sync` (Task 2), буквы «поверх» (Task 3).
4. **Pause, `K ≠ k`, нет двойника** — плитка рисуется ровно как в 0.4: `twinMatches` (Task 1), путь рендера (Task 5,
   вычитка).
5. **Соглашение вызова подмен**: каждая снимает со стека столько же, сколько метод игры — `tools/check_hooks.py` по
   объектным файлам (Task 6, 7, 8).

## Как записаны правки

Как в планах 0.4.x: «`путь`:» — файл целиком; «В `путь` заменить функцию `НАЧАЛО` целиком:» — многострочная функция
до первой строки `}`; «В `путь` после функции `НАЧАЛО` добавить:»; «В `путь` заменить: … на: …» — фрагмент встречается
ровно один раз; «В `путь` добавить в конец:». `.bat` и `.ini` — CRLF на диске, в блоках — LF.

Проверка задачи — скрипт `verify3.sh` (scratch): сборка DLL без предупреждений, `check_dll`, сборка и тесты ядра.

---

### Task 1: Сетка двойника

Спецификация §3: точка игры `g` занимает в двойнике `[floor(g·K), floor((g+1)·K))`; окно в точке игры `x0` начинается
в точке цели `ceil(x0·k)`; плитке нужен отрезок двойника `[floor(tx·K), min(ceil((tx+tw)·K), W'))`; точка цели `p`
берёт тексель `p − (ceil(x0·k) + floor(tx·K))`. Плитки игры — 256×256 по строкам.

**Files:**
- Create: `src/core/twin_grid.hpp`, `src/core/twin_grid.cpp`
- Test: `tests/test_twin_grid.cpp`

**Interfaces:**
- Produces: `int twinEdge(int g, double K)`; `int ceilEdge(int x, double k)`; `struct TwinSpan {int first, end;}`;
  `TwinSpan tileSpan(int t, int n, int twinSize, double K)`; `int tileOrigin(int x0, int t, double k, double K)`;
  `bool twinMatches(double K, double k)`; `struct TilePlace {int x, y, width, height;}`;
  `TilePlace tilePlace(int index, int width, int height)`.

- [ ] **Step 1: Падающие тесты**

`tests/test_twin_grid.cpp`:
```cpp
#include <algorithm>
#include <vector>

#include "core/scale.hpp"
#include "core/twin_grid.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
// Windows scaling 125-262 %, a 2560 x 1440 monitor for a 1920 x 1080 game, odd ratios.
const double kScales[] = {1.25, 1.5, 1.75, 2.0, 2.625, 4.0 / 3.0, 1.37, 2560.0 / 1920.0 * 1.3};
}  // namespace

TEST(twin_grid_blocks_at_1_75) {
    // Game pixels 0..3 at K = 1.75: blocks [0,1) [1,3) [3,5) [5,7) - one, two, two, two pixels.
    CHECK_EQ(twinEdge(0, 1.75), 0);
    CHECK_EQ(twinEdge(1, 1.75), 1);
    CHECK_EQ(twinEdge(2, 1.75), 3);
    CHECK_EQ(twinEdge(3, 1.75), 5);
    CHECK_EQ(twinEdge(4, 1.75), 7);
    CHECK_EQ(twinEdge(-3, 1.75), -6);  // floor(-5.25): lists scroll by drawing at negative x
}

TEST(twin_grid_every_block_has_a_pixel) {
    for (const double K : kScales)
        for (int g = -50; g < 700; ++g) CHECK(twinEdge(g + 1, K) - twinEdge(g, K) >= 1);
}

TEST(twin_grid_ceil_edge_is_the_stage_3_coverage_rule) {
    for (const double k : kScales)
        for (int x = -40; x < 2000; x += 7) CHECK_EQ(ceilEdge(x, k), scaleRect(Rect{x, 0, x + 1, 1}, k).left);
}

TEST(twin_grid_tile_places_follow_the_game) {
    const TilePlace a = tilePlace(0, 300, 120);
    const TilePlace b = tilePlace(1, 300, 120);
    CHECK(a.x == 0 && a.y == 0 && a.width == 256 && a.height == 120);
    CHECK(b.x == 256 && b.y == 0 && b.width == 44 && b.height == 120);
    const TilePlace c = tilePlace(4, 600, 600);
    const TilePlace d = tilePlace(8, 600, 600);
    CHECK(c.x == 256 && c.y == 256 && c.width == 256 && c.height == 256);
    CHECK(d.x == 512 && d.y == 512 && d.width == 88 && d.height == 88);
    CHECK_EQ(tilePlace(1, 256, 256).width, 0);  // there is no second tile
    CHECK_EQ(tilePlace(2, 300, 120).width, 0);
    CHECK_EQ(tilePlace(9, 600, 600).height, 0);
}

TEST(twin_grid_a_twin_matches_only_its_own_scale_above_1) {
    CHECK(twinMatches(1.75, 1.75));
    CHECK(!twinMatches(1.75, 1.0));  // Pause: the picture at the game's size
    CHECK(!twinMatches(1.0, 1.0));
    CHECK(!twinMatches(0.0, 1.0));
}

// Spec §3 with a reference rasterizer on one axis: a window w wide at game x0, cut into the game's tiles, drawn at
// k. Every target pixel of every tile quad takes one twin pixel from its tile's texture, and it is the one the whole
// window shows there: no seams, nothing past a texture but the clamp of the last tile. Every twin pixel - every game
// pixel's block - is shown, and each block lands within a target pixel of its game pixel.
TEST(twin_grid_every_target_pixel_shows_one_twin_pixel) {
    for (const double k : kScales)
        for (const int w : {1, 2, 3, 7, 255, 256, 257, 300, 513, 600})
            for (int x0 = -40; x0 < 40; ++x0) {
                const int twin = twinEdge(w, k);
                const int origin = ceilEdge(x0, k);
                std::vector<bool> shown(size_t(twin), false);
                for (int i = 0;; ++i) {
                    const TilePlace t = tilePlace(i, w, 1);
                    if (t.width == 0) break;
                    const TwinSpan span = tileSpan(t.x, t.width, twin, k);
                    const int size = span.end - span.first;
                    const bool last = t.x + t.width == w;
                    for (int p = ceilEdge(x0 + t.x, k); p < ceilEdge(x0 + t.x + t.width, k); ++p) {
                        const int texel = p - tileOrigin(x0, t.x, k, k);
                        CHECK(texel >= 0);
                        if (!last) CHECK(texel < size);  // only the last tile repeats its last column
                        const int column = span.first + std::clamp(texel, 0, size - 1);
                        CHECK_EQ(column, std::min(p - origin, twin - 1));  // the window's own mapping: no seam
                        shown[size_t(column)] = true;
                    }
                }
                CHECK(std::all_of(shown.begin(), shown.end(), [](bool b) { return b; }));
                for (int g = 0; g < w; ++g) {
                    const int shift = origin + twinEdge(g, k) - ceilEdge(x0 + g, k);
                    CHECK(shift >= -1 && shift <= 0);
                }
            }
}
```

- [ ] **Step 2: Тесты падают**

Run: `cmake --build build-host -j 2>&1 | tail -3`
Expected: ошибка сборки — `core/twin_grid.hpp` не найден.

- [ ] **Step 3: Сетка**

`src/core/twin_grid.hpp`:
```cpp
#pragma once

namespace uagfx {

// Stage 6, crisp text (docs/superpowers/specs/2026-10-13-stage6-crisp-text-design.md §3): the twin of a game window
// picture at scale K. Game pixel g covers twin pixels [twinEdge(g, K), twinEdge(g + 1, K)): side by side, each block
// at least one pixel when K >= 1. A picture of w x h has a twin of twinEdge(w, K) x twinEdge(h, K).
int twinEdge(int g, double K);
// ceil(x k): the first target pixel a quad from game coordinate x covers at k (stage 3, scaleRect).
int ceilEdge(int x, double k);

// The twin pixels [first, end) the texture of tile [t, t + n) holds on one axis: every twin pixel its quad shows
// (spec §3), cut at the twin's size.
struct TwinSpan {
    int first = 0;
    int end = 0;
};
TwinSpan tileSpan(int t, int n, int twinSize, double K);

// A window at game x0 whose tile starts at t: target pixel p shows texel p - tileOrigin() of the tile's texture.
int tileOrigin(int x0, int t, double k, double K);

// A twin made at K is shown only while the game is drawn at that scale, and only above 1.
bool twinMatches(double K, double k);

// Tile `index` of a window picture of width x height: the game cuts its pictures into tiles of 256 x 256 at most,
// row by row (docs/research/text.md §4.1). Past the last tile: an empty place.
struct TilePlace {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};
TilePlace tilePlace(int index, int width, int height);

}  // namespace uagfx
```

`src/core/twin_grid.cpp`:
```cpp
#include "core/twin_grid.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx {

// 1e-6: K and k are ratios of sizes, so g K can come out a hair off the whole number it equals (as in scaleRect).
int twinEdge(int g, double K) { return int(std::floor(g * K + 1e-6)); }

int ceilEdge(int x, double k) { return int(std::ceil(x * k - 1e-6)); }

TwinSpan tileSpan(int t, int n, int twinSize, double K) {
    TwinSpan s;
    s.first = std::min(twinEdge(t, K), twinSize);
    s.end = std::max(s.first, std::min(ceilEdge(t + n, K), twinSize));
    return s;
}

int tileOrigin(int x0, int t, double k, double K) { return ceilEdge(x0, k) + twinEdge(t, K); }

bool twinMatches(double K, double k) { return K > 1.0 && std::fabs(K - k) < 1e-9; }

TilePlace tilePlace(int index, int width, int height) {
    TilePlace p;
    if (index < 0 || width <= 0 || height <= 0) return p;
    const int columns = (width + 255) / 256;
    const int rows = (height + 255) / 256;
    if (index >= columns * rows) return p;  // past the last tile
    p.x = index % columns * 256;
    p.y = index / columns * 256;
    p.width = std::min(256, width - p.x);
    p.height = std::min(256, height - p.y);
    return p;
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests twin_grid | tail -2`
Expected: предупреждений нет; `6 tests, 0 failed checks`.

- [ ] **Step 5: Коммит**

```bash
git add src/core/twin_grid.hpp src/core/twin_grid.cpp tests/test_twin_grid.cpp
git commit -m "Сетка двойника: блоки по нижней границе, отрезки плиток, тексель на точку цели (этап 6, §3)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Картинка-двойник

Спецификация §3–§4: двойник (premultiplied RGBA8) и копия пикселей игры; правило прозрачности загрузки игры
(пятибитный пурпурный); заливка блоков (Clear, FillRgn), сверка по цвету, поглощение мелких букв, `CopyFrom`
дочерней «поверх» с её последним столбцом и строкой; обрезка `CopyFrom` по правилу игры. Двойник помнит, какие его
точки изменились с прошлой загрузки: окно чата обновляется каждый кадр, а грузить нужно только строку ввода. Заливка
идёт строками: блок первой строки, затем копии строки — окна перерисовываются каждый кадр.

**Files:**
- Create: `src/core/twin.hpp`, `src/core/twin.cpp`
- Test: `tests/test_twin.cpp`

**Interfaces:**
- Consumes: `twinEdge`, `tileSpan`, `TilePlace` (Task 1); `Rect`, `clipRect` (`core/image.hpp`).
- Produces: `bool transparentAtUpload(uint32_t)`; `uint32_t twinValue(uint32_t)`; `uint32_t over(uint32_t src,
  uint32_t dst)`; `bool copyFromRegion(int x, int y, int w, int h, int width, int height, Rect& out)`;
  `class TwinPicture` — `TwinPicture(const uint32_t* dib, int width, int height, double K)`, `width()`, `height()`,
  `scale()`, `twinWidth()`, `twinHeight()`, `twin()`, `bytes()`, `static bytesFor(int, int, double)`,
  `overwrite(const uint32_t*, Rect)`, `int sync(const uint32_t*, Rect)`, `absorb(const uint32_t*, Rect)`,
  `copyFrom(const uint32_t* dib, const TwinPicture& child, int x, int y, int w, int h)`,
  `texels(const Rect& r, std::vector<uint32_t>& out) const` (r — в точках двойника, внутри него),
  `Rect takeChanged()`, `void touch(Rect twinPixels)`.

- [ ] **Step 1: Падающие тесты**

`tests/test_twin.cpp`:
```cpp
#include <vector>

#include "core/twin.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
const uint32_t kMagenta = 0xFFFF00FF;  // the game's transparent clear
uint32_t at(const TwinPicture& t, int x, int y) { return t.twin()[size_t(y) * size_t(t.twinWidth()) + size_t(x)]; }
}  // namespace

TEST(twin_transparency_is_the_game_upload_rule) {
    CHECK(transparentAtUpload(0xFFFF00FF));
    CHECK(transparentAtUpload(0x00FF00FF));   // GDI writes alpha 0; the game forces 0xFF before the upload
    CHECK(transparentAtUpload(0xFFF807F8));   // magenta in 5 bits
    CHECK(!transparentAtUpload(0xFFF808F8));  // green 0x08 is 1 in 5 bits
    CHECK(!transparentAtUpload(0xFFF0F0F0));
    CHECK_EQ(twinValue(0x00123456), 0xFF123456u);
    CHECK_EQ(twinValue(kMagenta), 0u);
}

TEST(twin_over_is_premultiplied) {
    CHECK_EQ(over(0xFF102030, 0xFF405060), 0xFF102030u);
    CHECK_EQ(over(0, 0xFF405060), 0xFF405060u);
    CHECK_EQ(over(0x80400000, 0xFF0000FF), 0xFF40007Fu);  // half red over blue
    CHECK_EQ(over(0x80400000, 0), 0x80400000u);
}

TEST(twin_picture_enlarges_the_game_pixels) {
    // 3 x 2 at 1.75: columns [0,1) [1,3) [3,5), rows [0,1) [1,3); the magenta pixel is transparent.
    const std::vector<uint32_t> dib = {0xFF0000AA, 0xFF0000BB, kMagenta, 0xFF0000DD, 0xFF0000EE, 0xFF0000FF};
    const TwinPicture t(dib.data(), 3, 2, 1.75);
    CHECK_EQ(t.twinWidth(), 5);
    CHECK_EQ(t.twinHeight(), 3);
    CHECK_EQ(at(t, 0, 0), 0xFF0000AAu);
    CHECK_EQ(at(t, 1, 0), 0xFF0000BBu);
    CHECK_EQ(at(t, 2, 0), 0xFF0000BBu);
    CHECK_EQ(at(t, 3, 0), 0u);
    CHECK_EQ(at(t, 0, 1), 0xFF0000DDu);
    CHECK_EQ(at(t, 0, 2), 0xFF0000DDu);
    CHECK_EQ(at(t, 4, 2), 0xFF0000FFu);
}

TEST(twin_picture_overwrite_erases_what_sync_keeps) {
    const std::vector<uint32_t> dib(16, 0xFF808080);
    TwinPicture t(dib.data(), 4, 4, 2.0);
    t.twin()[size_t(2 * t.twinWidth() + 2)] = 0xFF000000;  // a letter's pixel in game pixel (1, 1)
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 4, 4}), 0);      // the game's pixels did not change: sync keeps it
    CHECK_EQ(at(t, 2, 2), 0xFF000000u);
    t.overwrite(dib.data(), Rect{1, 1, 2, 2});              // a Clear of that pixel erases it
    CHECK_EQ(at(t, 2, 2), 0xFF808080u);
    t.overwrite(dib.data(), Rect{-5, -5, 50, 50});          // cut to the picture
    CHECK_EQ(at(t, 7, 7), 0xFF808080u);
}

TEST(twin_picture_sync_takes_changed_colours_only) {
    std::vector<uint32_t> dib(4, 0xFF808080);
    TwinPicture t(dib.data(), 2, 2, 2.0);
    dib[1] = 0xFF00FF00;  // a new colour
    dib[2] = 0x00808080;  // the alpha byte only: the game's own business before an upload
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 2, 2}), 1);
    CHECK_EQ(at(t, 2, 0), 0xFF00FF00u);
    CHECK_EQ(at(t, 3, 1), 0xFF00FF00u);
    CHECK_EQ(at(t, 0, 2), 0xFF808080u);
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 2, 2}), 0);  // the copy followed
}

TEST(twin_picture_absorb_keeps_the_twin) {
    std::vector<uint32_t> dib(4, 0xFF808080);
    TwinPicture t(dib.data(), 2, 2, 2.0);
    dib[0] = 0x00000000;  // the game's small letter
    t.absorb(dib.data(), Rect{0, 0, 2, 2});
    CHECK_EQ(at(t, 0, 0), 0xFF808080u);  // the twin has its own letters
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 2, 2}), 0);
}

TEST(twin_picture_copy_from_lays_the_child_over) {
    // A 3 x 2 child at (1, 1) of an 8 x 4 parent, K = 1.75: its magenta pixel keeps the parent, its red ones cover it,
    // and a half-covered letter edge on its transparent part blends.
    std::vector<uint32_t> parent(32, 0xFF0000FF);
    TwinPicture p(parent.data(), 8, 4, 1.75);
    const std::vector<uint32_t> kid = {kMagenta, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000};
    TwinPicture c(kid.data(), 3, 2, 1.75);
    c.twin()[0] = 0x80400000;  // twin pixel (0, 0): the block of the magenta game pixel
    for (int y = 1; y < 3; ++y)  // what the game's copy does to the parent's pixels
        for (int x = 1; x < 4; ++x) {
            const uint32_t v = kid[size_t(y - 1) * 3 + size_t(x - 1)];
            if (v != kMagenta) parent[size_t(y) * 8 + size_t(x)] = v;
        }
    p.copyFrom(parent.data(), c, 1, 1, 3, 2);
    // The child starts at the parent's block of game pixel 1 - twin (1, 1) - and ends before the block of 4, twin 7;
    // the parent's blocks need six columns there, the child has five: its last one repeats.
    CHECK_EQ(at(p, 1, 1), over(0x80400000, 0xFF0000FF));
    CHECK_EQ(at(p, 2, 1), 0xFFFF0000u);
    CHECK_EQ(at(p, 6, 2), 0xFFFF0000u);
    CHECK_EQ(at(p, 7, 1), 0xFF0000FFu);  // game pixel 4: past the copy
    CHECK_EQ(at(p, 0, 1), 0xFF0000FFu);
    CHECK_EQ(p.sync(parent.data(), Rect{0, 0, 8, 4}), 0);  // the copy followed the game
}

TEST(twin_picture_copy_from_a_child_at_another_scale_enlarges) {
    std::vector<uint32_t> parent(16, 0xFF0000FF);
    TwinPicture p(parent.data(), 4, 4, 1.75);
    const std::vector<uint32_t> kid(4, 0xFFFF0000);
    const TwinPicture c(kid.data(), 2, 2, 2.0);  // made before the scale changed
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x) parent[size_t(y) * 4 + size_t(x)] = 0xFFFF0000;
    p.copyFrom(parent.data(), c, 0, 0, 2, 2);
    CHECK_EQ(at(p, 0, 0), 0xFFFF0000u);
    CHECK_EQ(at(p, 2, 2), 0xFFFF0000u);  // game pixel (1, 1): [1, 3) x [1, 3)
    CHECK_EQ(at(p, 3, 3), 0xFF0000FFu);
}

TEST(twin_picture_copy_from_region_is_the_game_clipping) {
    Rect r;
    CHECK(!copyFromRegion(-1, 0, 5, 5, 8, 4, r));  // negative: the game compares unsigned and copies nothing
    CHECK(!copyFromRegion(0, -5, 5, 5, 8, 4, r));
    CHECK(!copyFromRegion(8, 0, 5, 5, 8, 4, r));
    CHECK(!copyFromRegion(0, 0, 0, 3, 8, 4, r));
    CHECK(copyFromRegion(5, 2, 10, 10, 8, 4, r));
    CHECK(r == (Rect{5, 2, 8, 4}));
}

TEST(twin_picture_texels_cut_a_rect) {
    std::vector<uint32_t> dib(300 * 2);
    for (size_t i = 0; i < dib.size(); ++i) dib[i] = 0xFF000000u | uint32_t(i % 300);  // a colour per column
    const TwinPicture t(dib.data(), 300, 2, 1.75);
    const TwinSpan second = tileSpan(256, 44, t.twinWidth(), 1.75);  // the second tile: [448, 525)
    std::vector<uint32_t> texels;
    t.texels(Rect{second.first, 0, second.end, t.twinHeight()}, texels);
    CHECK_EQ(texels.size(), size_t(77 * 3));
    CHECK_EQ(texels[0], 0xFF000000u | 256u);  // twin column 448 starts game column 256
    CHECK_EQ(texels[76], 0xFF000000u | 299u);
    CHECK_EQ(texels[77 * 2], 0xFF000000u | 256u);
}

TEST(twin_picture_reports_the_twin_pixels_it_changed) {
    std::vector<uint32_t> dib(16, 0xFF808080);
    TwinPicture t(dib.data(), 4, 4, 1.75);  // a 7 x 7 twin: edges 0 1 3 5 7
    CHECK(t.takeChanged() == (Rect{0, 0, 7, 7}));  // a new twin is all new
    CHECK(t.takeChanged().empty());
    CHECK_EQ(t.sync(dib.data(), Rect{0, 0, 4, 4}), 0);
    CHECK(t.takeChanged().empty());  // nothing changed, nothing to load
    dib[1 * 4 + 2] = 0xFF000000;     // game pixel (2, 1)
    t.sync(dib.data(), Rect{0, 0, 4, 4});
    CHECK(t.takeChanged() == (Rect{3, 1, 5, 3}));
    t.overwrite(dib.data(), Rect{0, 3, 1, 4});  // game pixel (0, 3)
    t.absorb(dib.data(), Rect{0, 0, 4, 4});     // the copy only
    CHECK(t.takeChanged() == (Rect{0, 5, 1, 7}));
    t.touch(Rect{-3, -3, 2, 2});  // cut to the twin
    CHECK(t.takeChanged() == (Rect{0, 0, 2, 2}));
}

TEST(twin_picture_bytes_for_matches_a_made_one) {
    const std::vector<uint32_t> dib(30 * 20, 0xFF000000);
    const TwinPicture t(dib.data(), 30, 20, 2.625);
    CHECK_EQ(TwinPicture::bytesFor(30, 20, 2.625), t.bytes());
}
```

- [ ] **Step 2: Тесты падают**

Run: `cmake --build build-host -j 2>&1 | tail -3`
Expected: ошибка сборки — `core/twin.hpp` не найден.

- [ ] **Step 3: Двойник**

`src/core/twin.hpp`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/image.hpp"
#include "core/twin_grid.hpp"

namespace uagfx {

// Stage 6 (spec §2.5, §3): a pixel of a game window picture in its twin. The game uploads its pictures with alpha
// forced to 0xFF and makes every pixel whose 5-bit colour is magenta a transparent texel; the twin keeps the 8-bit
// colour, opaque, or transparent - premultiplied 0xAARRGGBB, as every twin pixel.
bool transparentAtUpload(uint32_t pixel);
uint32_t twinValue(uint32_t pixel);
// Premultiplied src over dst.
uint32_t over(uint32_t src, uint32_t dst);

// The game's CopyFrom clipping (docs/research/text.md §1.1, slot 5): x or y at or past the picture - negative ones
// too, the game compares them unsigned - copies nothing; otherwise the copy is cut at the right and bottom edges.
// false: nothing is copied.
bool copyFromRegion(int x, int y, int w, int h, int width, int height, Rect& out);

// A game window picture (CDCBitmap) as the driver keeps it (spec §4): its twin at K and a copy of the game's pixels
// the twin shows. dib: the game's pixels, width x height, row by row.
class TwinPicture {
public:
    TwinPicture(const uint32_t* dib, int width, int height, double K);  // the twin enlarges dib, the copy is dib

    int width() const { return w_; }
    int height() const { return h_; }
    double scale() const { return k_; }
    int twinWidth() const { return ex_.back(); }
    int twinHeight() const { return ey_.back(); }
    std::vector<uint32_t>& twin() { return twin_; }
    const std::vector<uint32_t>& twin() const { return twin_; }
    size_t bytes() const { return (twin_.size() + copy_.size()) * sizeof(uint32_t); }
    // What a picture of width x height takes at K, before it is made.
    static size_t bytesFor(int width, int height, double K);

    // The game's Clear or FillRgn wrote r: every pixel's block takes its new value, whatever the twin had there.
    void overwrite(const uint32_t* dib, Rect r);
    // The pixels of r whose colour differs from the copy go to the twin as blocks and the copy follows; the alpha
    // byte is ignored (the game sets it before every upload). Returns how many.
    int sync(const uint32_t* dib, Rect r);
    // The game drew text into r, which the twin got at its own size: the copy takes the new pixels, the twin stays.
    void absorb(const uint32_t* dib, Rect r);
    // CopyFrom (spec §3, §4): child's twin over this twin on the game pixels [x, x + w) x [y, y + h), x and y not
    // negative; then the copy takes the new pixels there. A child made at another scale goes in through sync.
    void copyFrom(const uint32_t* dib, const TwinPicture& child, int x, int y, int w, int h);
    // The twin pixels of r - twin coordinates, inside the twin - row by row: what a tile's texture loads.
    void texels(const Rect& r, std::vector<uint32_t>& out) const;
    // The twin pixels changed since the last call (a new twin: all of it); empty when none.
    Rect takeChanged();
    // drawGlyph and other direct writers of twin() say which twin pixels they changed.
    void touch(Rect twinPixels);

private:
    void fillRow(const uint32_t* dib, int y, int x0, int x1);
    bool clip(Rect& r) const { return clipRect(Rect{0, 0, w_, h_}, r); }

    int w_;
    int h_;
    double k_;
    std::vector<int> ex_;         // twinEdge(g, K) for g = 0..width
    std::vector<int> ey_;         // for g = 0..height
    std::vector<uint32_t> twin_;  // twinWidth x twinHeight
    std::vector<uint32_t> copy_;  // width x height
    Rect changed_;                // twin pixels
};

}  // namespace uagfx
```

`src/core/twin.cpp`:
```cpp
#include "core/twin.hpp"

#include <algorithm>

namespace uagfx {

bool transparentAtUpload(uint32_t p) { return (p >> 19 & 31) == 31 && (p >> 11 & 31) == 0 && (p >> 3 & 31) == 31; }

uint32_t twinValue(uint32_t p) { return transparentAtUpload(p) ? 0u : 0xFF000000u | (p & 0xFFFFFFu); }

uint32_t over(uint32_t src, uint32_t dst) {
    const uint32_t keep = 255 - (src >> 24);
    if (keep == 0) return src;
    if (src == 0) return dst;
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8) {
        const uint32_t c = (src >> s & 255) + ((dst >> s & 255) * keep + 127) / 255;
        out |= std::min(c, 255u) << s;
    }
    return out;
}

bool copyFromRegion(int x, int y, int w, int h, int width, int height, Rect& out) {
    if (width <= 0 || height <= 0 || uint32_t(x) >= uint32_t(width) || uint32_t(y) >= uint32_t(height)) return false;
    w = std::min(w, width - x);
    h = std::min(h, height - y);
    if (w <= 0 || h <= 0) return false;
    out = Rect{x, y, x + w, y + h};
    return true;
}

TwinPicture::TwinPicture(const uint32_t* dib, int width, int height, double K)
    : w_(std::max(width, 0)), h_(std::max(height, 0)), k_(K), ex_(size_t(w_) + 1), ey_(size_t(h_) + 1) {
    for (int g = 0; g <= w_; ++g) ex_[size_t(g)] = twinEdge(g, K);
    for (int g = 0; g <= h_; ++g) ey_[size_t(g)] = twinEdge(g, K);
    twin_.assign(size_t(twinWidth()) * size_t(twinHeight()), 0u);
    copy_.assign(size_t(w_) * size_t(h_), 0u);
    for (int y = 0; y < h_; ++y) fillRow(dib, y, 0, w_);
}

size_t TwinPicture::bytesFor(int width, int height, double K) {
    const size_t twin = size_t(std::max(twinEdge(width, K), 0)) * size_t(std::max(twinEdge(height, K), 0));
    return (twin + size_t(std::max(width, 0)) * size_t(std::max(height, 0))) * sizeof(uint32_t);
}

// The blocks of game pixels [x0, x1) of row y take dib's values, and so does the copy: the first twin row of the
// blocks pixel by pixel, the other rows as copies of it.
void TwinPicture::fillRow(const uint32_t* dib, int y, int x0, int x1) {
    const size_t tw = size_t(twinWidth());
    const uint32_t* src = dib + size_t(y) * size_t(w_);
    uint32_t* copy = copy_.data() + size_t(y) * size_t(w_);
    uint32_t* first = twin_.data() + size_t(ey_[size_t(y)]) * tw;
    for (int x = x0; x < x1; ++x) {
        copy[x] = src[x];
        std::fill(first + ex_[size_t(x)], first + ex_[size_t(x) + 1], twinValue(src[x]));
    }
    const int left = ex_[size_t(x0)];
    const int right = ex_[size_t(x1)];
    for (int ty = ey_[size_t(y)] + 1; ty < ey_[size_t(y) + 1]; ++ty)
        std::copy(first + left, first + right, twin_.data() + size_t(ty) * tw + size_t(left));
    touch(Rect{left, ey_[size_t(y)], right, ey_[size_t(y) + 1]});
}

void TwinPicture::overwrite(const uint32_t* dib, Rect r) {
    if (!clip(r)) return;
    for (int y = r.top; y < r.bottom; ++y) fillRow(dib, y, r.left, r.right);
}

int TwinPicture::sync(const uint32_t* dib, Rect r) {
    if (!clip(r)) return 0;
    int n = 0;
    for (int y = r.top; y < r.bottom; ++y) {
        const uint32_t* src = dib + size_t(y) * size_t(w_);
        const uint32_t* copy = copy_.data() + size_t(y) * size_t(w_);
        for (int x = r.left; x < r.right;) {
            if (((src[x] ^ copy[x]) & 0xFFFFFFu) == 0) {
                ++x;
                continue;
            }
            const int run = x;  // changed pixels side by side: one fillRow; the unchanged keep their twin
            while (x < r.right && ((src[x] ^ copy[x]) & 0xFFFFFFu) != 0) ++x;
            fillRow(dib, y, run, x);
            n += x - run;
        }
    }
    return n;
}

void TwinPicture::absorb(const uint32_t* dib, Rect r) {
    if (!clip(r)) return;
    for (int y = r.top; y < r.bottom; ++y) {
        const size_t i = size_t(y) * size_t(w_) + size_t(r.left);
        std::copy_n(dib + i, size_t(r.width()), copy_.data() + i);
    }
}

void TwinPicture::copyFrom(const uint32_t* dib, const TwinPicture& child, int x, int y, int w, int h) {
    Rect r{x, y, x + w, y + h};
    if (x < 0 || y < 0 || !clip(r)) return;
    if (child.k_ != k_) {
        sync(dib, r);
        return;
    }
    // The child's twin from its corner; where this picture's blocks need one more column or row than the child
    // has, its last one repeats (spec §3).
    const int cw = child.ex_[size_t(std::min(r.width(), child.w_))];
    const int ch = child.ey_[size_t(std::min(r.height(), child.h_))];
    if (cw > 0 && ch > 0) {
        const int x0 = ex_[size_t(r.left)];
        const int x1 = ex_[size_t(r.right)];
        const int y0 = ey_[size_t(r.top)];
        const int y1 = ey_[size_t(r.bottom)];
        const size_t tw = size_t(twinWidth());
        const size_t ctw = size_t(child.twinWidth());
        for (int py = y0; py < y1; ++py) {
            const uint32_t* src = child.twin_.data() + size_t(std::min(py - y0, ch - 1)) * ctw;
            uint32_t* dst = twin_.data() + size_t(py) * tw;
            for (int px = x0; px < x1; ++px) dst[px] = over(src[std::min(px - x0, cw - 1)], dst[px]);
        }
        touch(Rect{x0, y0, x1, y1});
    }
    absorb(dib, r);
}

void TwinPicture::texels(const Rect& r, std::vector<uint32_t>& out) const {
    out.resize(size_t(std::max(r.width(), 0)) * size_t(std::max(r.height(), 0)));
    for (int y = r.top; y < r.bottom; ++y)
        std::copy_n(twin_.data() + size_t(y) * size_t(twinWidth()) + size_t(r.left), size_t(r.width()),
                    out.data() + size_t(y - r.top) * size_t(r.width()));
}

Rect TwinPicture::takeChanged() {
    const Rect r = changed_;
    changed_ = Rect{};
    return r;
}

void TwinPicture::touch(Rect r) {
    if (!clipRect(Rect{0, 0, twinWidth(), twinHeight()}, r)) return;
    if (changed_.empty()) {
        changed_ = r;
        return;
    }
    changed_.left = std::min(changed_.left, r.left);
    changed_.top = std::min(changed_.top, r.top);
    changed_.right = std::max(changed_.right, r.right);
    changed_.bottom = std::max(changed_.bottom, r.bottom);
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests twin_ | tail -2`
Expected: предупреждений нет; `18 tests, 0 failed checks` (6 сетки и 12 двойника).

- [ ] **Step 5: Коммит**

```bash
git add src/core/twin.hpp src/core/twin.cpp tests/test_twin.cpp
git commit -m "Картинка-двойник: прозрачность как у загрузки игры, заливка, сверка, поглощение букв, CopyFrom поверх (этап 6, §3–§4)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 3: Буквы в двойнике

Спецификация §4: буква `i` строки `TextOutW` в `(x, y)` ставится в начало блока точки игры `x + a_i`, где `a_i` —
ширина букв перед ней в шрифте игры; базовая линия — верх строки игры `y + ascent`; покрытие буквы
(`GGO_GRAY8_BITMAP`, 0–64) — альфа, буква кладётся «поверх» цветом текста, обрезка — по краям двойника.

**Files:**
- Create: `src/core/twin_text.hpp`, `src/core/twin_text.cpp`
- Test: `tests/test_twin_text.cpp`

**Interfaces:**
- Consumes: `twinEdge` (Task 1); `TwinPicture`, `over` (Task 2).
- Produces: `struct GlyphBitmap {int width, height, originX, originY; std::vector<uint8_t> coverage;}`;
  `int letterPen(int x, int advance, double K)`; `int baselineRow(int y, int ascent, double K)`;
  `void drawGlyph(TwinPicture& t, const GlyphBitmap& g, int penX, int baseline, uint32_t rgb)`;
  `Rect textRect(int x, int y, int extent, int cellHeight, int margin)`; `uint32_t textColor(uint32_t colorref)`.

- [ ] **Step 1: Падающие тесты**

`tests/test_twin_text.cpp`:
```cpp
#include <utility>
#include <vector>

#include "core/twin.hpp"
#include "core/twin_text.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
uint32_t at(const TwinPicture& t, int x, int y) { return t.twin()[size_t(y) * size_t(t.twinWidth()) + size_t(x)]; }
GlyphBitmap glyph(int w, int h, int ox, int oy, std::vector<uint8_t> coverage) {
    GlyphBitmap g;
    g.width = w;
    g.height = h;
    g.originX = ox;
    g.originY = oy;
    g.coverage = std::move(coverage);
    return g;
}
}  // namespace

TEST(twin_text_letters_start_at_their_game_pixel_blocks) {
    CHECK_EQ(letterPen(10, 0, 1.75), 17);    // floor(17.5)
    CHECK_EQ(letterPen(10, 7, 1.75), 29);    // floor(29.75)
    CHECK_EQ(letterPen(-3, 0, 1.75), -6);    // a list scrolled left
    CHECK_EQ(baselineRow(2, 12, 1.75), 24);  // floor(24.5)
    CHECK_EQ(letterPen(4, 0, 2.0), 8);
}

TEST(twin_text_coverage_becomes_premultiplied_alpha) {
    const std::vector<uint32_t> dib(16, 0xFFFF00FF);  // a transparent picture: a name over the world
    TwinPicture t(dib.data(), 4, 4, 2.0);
    drawGlyph(t, glyph(2, 1, 0, 1, {64, 32}), 1, 3, 0xFFFFFF);  // its box at (1, 2)
    CHECK_EQ(at(t, 1, 2), 0xFFFFFFFFu);
    CHECK_EQ(at(t, 2, 2), 0x80808080u);  // alpha 128, white premultiplied
    CHECK_EQ(at(t, 0, 2), 0u);
    CHECK_EQ(at(t, 1, 1), 0u);
}

TEST(twin_text_letter_over_an_opaque_window) {
    const std::vector<uint32_t> dib(4, 0xFFFFFFFF);
    TwinPicture t(dib.data(), 2, 2, 2.0);
    drawGlyph(t, glyph(1, 1, 0, 0, {32}), 0, 0, 0x000000);  // half-covered black
    CHECK_EQ(at(t, 0, 0), 0xFF7F7F7Fu);
}

TEST(twin_text_letters_are_clipped_to_the_twin) {
    const std::vector<uint32_t> dib(4, 0xFFFFFFFF);
    TwinPicture t(dib.data(), 2, 2, 2.0);  // a 4 x 4 twin
    const GlyphBitmap g = glyph(3, 3, 0, 0, std::vector<uint8_t>(9, 64));
    drawGlyph(t, g, -2, 0, 0x000000);  // its box at (-2, 0): one column inside
    drawGlyph(t, g, 3, 2, 0x000000);   // at (3, 2): one column and two rows inside
    CHECK_EQ(at(t, 0, 0), 0xFF000000u);
    CHECK_EQ(at(t, 1, 0), 0xFFFFFFFFu);
    CHECK_EQ(at(t, 3, 2), 0xFF000000u);
    CHECK_EQ(at(t, 3, 3), 0xFF000000u);
    CHECK_EQ(at(t, 2, 3), 0xFFFFFFFFu);
    drawGlyph(t, glyph(2, 2, 0, 0, {64}), 0, 0, 0x000000);  // a broken bitmap draws nothing
    CHECK_EQ(at(t, 1, 1), 0xFFFFFFFFu);
}

TEST(twin_text_a_letter_reports_its_box) {
    const std::vector<uint32_t> dib(16, 0xFFFFFFFF);
    TwinPicture t(dib.data(), 4, 4, 2.0);  // an 8 x 8 twin
    t.takeChanged();
    drawGlyph(t, glyph(3, 2, 1, 2, std::vector<uint8_t>(6, 64)), 2, 4, 0x000000);  // its box at (3, 2)
    CHECK(t.takeChanged() == (Rect{3, 2, 6, 4}));
    drawGlyph(t, glyph(3, 2, 0, 0, std::vector<uint8_t>(6, 64)), 6, 7, 0x000000);  // at (6, 7): cut
    CHECK(t.takeChanged() == (Rect{6, 7, 8, 8}));
}

TEST(twin_text_rect_has_its_margin) { CHECK(textRect(5, 6, 30, 15, 15) == (Rect{-10, -9, 50, 36})); }

TEST(twin_text_colour_from_gdi) {
    CHECK_EQ(textColor(0x00332211), 0x112233u);  // a COLORREF is 0x00BBGGRR
    CHECK_EQ(textColor(0x02FFFFFF), 0xFFFFFFu);  // its top byte is no colour
}
```

- [ ] **Step 2: Тесты падают**

Run: `cmake --build build-host -j 2>&1 | tail -3`
Expected: ошибка сборки — `core/twin_text.hpp` не найден.

- [ ] **Step 3: Буквы**

`src/core/twin_text.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <vector>

#include "core/image.hpp"

namespace uagfx {

class TwinPicture;

// A letter at the twin's size as GetGlyphOutline(GGO_GRAY8_BITMAP) gives it: coverage 0..64 per pixel.
struct GlyphBitmap {
    int width = 0;    // the black box
    int height = 0;
    int originX = 0;  // from the pen to the box's left edge
    int originY = 0;  // from the baseline up to the box's top edge
    std::vector<uint8_t> coverage;  // width x height, row by row
};

// Stage 6 (spec §4): letter i of a TextOutW at game (x, y) goes where the game drew its small one - to the start of
// the block of game pixel x + advance, advance being the width of the letters before it in the game's font.
int letterPen(int x, int advance, double K);
// The baseline: the top of game pixel row y + ascent (the game's font) in the twin.
int baselineRow(int y, int ascent, double K);
// Lays a letter over the twin in colour rgb (0xRRGGBB) with its coverage as alpha, cut to the twin, and reports
// its box as changed.
void drawGlyph(TwinPicture& t, const GlyphBitmap& g, int penX, int baseline, uint32_t rgb);
// The game pixels a TextOutW may touch: its cell box and a margin on every side.
Rect textRect(int x, int y, int extent, int cellHeight, int margin);
// GetTextColor's COLORREF (0x00BBGGRR) as the 0xRRGGBB drawGlyph takes.
uint32_t textColor(uint32_t colorref);

}  // namespace uagfx
```

`src/core/twin_text.cpp`:
```cpp
#include "core/twin_text.hpp"

#include <algorithm>

#include "core/twin.hpp"
#include "core/twin_grid.hpp"

namespace uagfx {

int letterPen(int x, int advance, double K) { return twinEdge(x + advance, K); }

int baselineRow(int y, int ascent, double K) { return twinEdge(y + ascent, K); }

void drawGlyph(TwinPicture& t, const GlyphBitmap& g, int penX, int baseline, uint32_t rgb) {
    if (g.width <= 0 || g.height <= 0 || g.coverage.size() < size_t(g.width) * size_t(g.height)) return;
    const int left = penX + g.originX;
    const int top = baseline - g.originY;
    const int tw = t.twinWidth();
    const int th = t.twinHeight();
    std::vector<uint32_t>& px = t.twin();
    const uint32_t r = rgb >> 16 & 255, gr = rgb >> 8 & 255, b = rgb & 255;
    for (int y = std::max(0, -top); y < g.height && top + y < th; ++y)
        for (int x = std::max(0, -left); x < g.width && left + x < tw; ++x) {
            const uint32_t c = std::min<uint32_t>(g.coverage[size_t(y) * size_t(g.width) + size_t(x)], 64);
            if (c == 0) continue;
            const uint32_t a = (c * 255 + 32) / 64;
            const uint32_t src =
                a << 24 | (r * a + 127) / 255 << 16 | (gr * a + 127) / 255 << 8 | (b * a + 127) / 255;
            uint32_t& d = px[size_t(top + y) * size_t(tw) + size_t(left + x)];
            d = over(src, d);
        }
    t.touch(Rect{left, top, left + g.width, top + g.height});
}

Rect textRect(int x, int y, int extent, int cellHeight, int margin) {
    return Rect{x - margin, y - margin, x + extent + margin, y + cellHeight + margin};
}

uint32_t textColor(uint32_t c) { return (c & 0xFF) << 16 | (c & 0xFF00) | (c >> 16 & 0xFF); }

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests twin_text | tail -2`
Expected: предупреждений нет; `7 tests, 0 failed checks`.

- [ ] **Step 5: Коммит**

```bash
git add src/core/twin_text.hpp src/core/twin_text.cpp tests/test_twin_text.cpp
git commit -m "Буквы в двойнике: на местах раскладки игры, покрытие как альфа поверх, обрезка по краям (этап 6, §4)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Кеш букв

Спецификация §4: покрытие и метрики буквы по ключу (шрифт, буква), LRU с пределом в байтах — окно чата
перерисовывается каждый кадр, а буквы растрируются один раз. Шрифт в ключе — номер, который драйвер дал шрифту игры
на размере двойника (Task 7).

**Files:**
- Create: `src/core/glyph_cache.hpp`, `src/core/glyph_cache.cpp`
- Test: `tests/test_glyph_cache.cpp`

**Interfaces:**
- Consumes: `GlyphBitmap` (Task 3).
- Produces: `class GlyphCache` — `explicit GlyphCache(size_t budgetBytes)`, `const GlyphBitmap* find(uint32_t font,
  uint32_t ch)`, `const GlyphBitmap& put(uint32_t font, uint32_t ch, GlyphBitmap g)`, `void clear()`, `size()`,
  `bytes()`, `static size_t cost(const GlyphBitmap&)`. Указатель и ссылка живут до следующего `put` или `clear`.

- [ ] **Step 1: Падающие тесты**

`tests/test_glyph_cache.cpp`:
```cpp
#include "core/glyph_cache.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
GlyphBitmap letter(int bytes) {
    GlyphBitmap g;
    g.width = bytes;
    g.height = 1;
    g.coverage.assign(size_t(bytes), 64);
    return g;
}
}  // namespace

TEST(glyph_cache_finds_what_was_put) {
    GlyphCache c(1 << 20);
    CHECK(c.find(1, L'a') == nullptr);
    c.put(1, L'a', letter(10));
    const GlyphBitmap* g = c.find(1, L'a');
    CHECK(g != nullptr && g->width == 10);
    CHECK(c.find(2, L'a') == nullptr);  // another font
    CHECK(c.find(1, L'b') == nullptr);
    CHECK_EQ(c.size(), size_t(1));
}

TEST(glyph_cache_drops_the_least_recent_past_its_budget) {
    const size_t one = GlyphCache::cost(letter(100));
    GlyphCache c(one * 2);
    c.put(1, 'a', letter(100));
    c.put(1, 'b', letter(100));
    CHECK(c.find(1, 'a') != nullptr);  // 'a' is now the most recent
    c.put(1, 'c', letter(100));
    CHECK(c.find(1, 'a') != nullptr);
    CHECK(c.find(1, 'b') == nullptr);
    CHECK(c.find(1, 'c') != nullptr);
    CHECK(c.bytes() <= one * 2);
}

TEST(glyph_cache_keeps_a_new_letter_bigger_than_its_budget) {
    GlyphCache c(10);
    const GlyphBitmap& g = c.put(1, 'a', letter(1000));
    CHECK_EQ(g.width, 1000);
    CHECK(c.find(1, 'a') != nullptr);
}

TEST(glyph_cache_put_again_replaces) {
    GlyphCache c(1 << 20);
    c.put(1, 'a', letter(10));
    c.put(1, 'a', letter(20));
    CHECK_EQ(c.size(), size_t(1));
    CHECK_EQ(c.bytes(), GlyphCache::cost(letter(20)));
    CHECK_EQ(c.find(1, 'a')->width, 20);
    c.clear();
    CHECK_EQ(c.size(), size_t(0));
    CHECK_EQ(c.bytes(), size_t(0));
    CHECK(c.find(1, 'a') == nullptr);
}
```

- [ ] **Step 2: Тесты падают**

Run: `cmake --build build-host -j 2>&1 | tail -3`
Expected: ошибка сборки — `core/glyph_cache.hpp` не найден.

- [ ] **Step 3: Кеш**

`src/core/glyph_cache.hpp`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>

#include "core/twin_text.hpp"

namespace uagfx {

// Stage 6 (spec §4): letters at the twin's size, the least recently used ones dropped when they no longer fit the
// budget. font: the driver's number for a game font at the twin's size; ch: the letter. A pointer or reference it
// returns stays valid until the next put or clear.
class GlyphCache {
public:
    explicit GlyphCache(size_t budgetBytes) : budget_(budgetBytes) {}
    const GlyphBitmap* find(uint32_t font, uint32_t ch);              // makes it the most recent
    const GlyphBitmap& put(uint32_t font, uint32_t ch, GlyphBitmap g);  // the new letter itself is never dropped
    void clear();
    size_t size() const { return map_.size(); }
    size_t bytes() const { return bytes_; }
    static size_t cost(const GlyphBitmap& g);

private:
    struct Entry {
        uint64_t key;
        GlyphBitmap glyph;
        size_t bytes;
    };
    size_t budget_;
    size_t bytes_ = 0;
    std::list<Entry> order_;  // the most recent first
    std::unordered_map<uint64_t, std::list<Entry>::iterator> map_;
};

}  // namespace uagfx
```

`src/core/glyph_cache.cpp`:
```cpp
#include "core/glyph_cache.hpp"

#include <utility>

namespace uagfx {

size_t GlyphCache::cost(const GlyphBitmap& g) { return g.coverage.size() + sizeof(Entry) + 32; }

const GlyphBitmap* GlyphCache::find(uint32_t font, uint32_t ch) {
    const auto it = map_.find(uint64_t(font) << 32 | ch);
    if (it == map_.end()) return nullptr;
    order_.splice(order_.begin(), order_, it->second);
    return &it->second->glyph;
}

const GlyphBitmap& GlyphCache::put(uint32_t font, uint32_t ch, GlyphBitmap g) {
    const uint64_t key = uint64_t(font) << 32 | ch;
    const auto old = map_.find(key);
    if (old != map_.end()) {
        bytes_ -= old->second->bytes;
        order_.erase(old->second);
        map_.erase(old);
    }
    const size_t c = cost(g);
    order_.push_front(Entry{key, std::move(g), c});
    map_[key] = order_.begin();
    bytes_ += c;
    while (bytes_ > budget_ && order_.size() > 1) {
        bytes_ -= order_.back().bytes;
        map_.erase(order_.back().key);
        order_.pop_back();
    }
    return order_.front().glyph;
}

void GlyphCache::clear() {
    order_.clear();
    map_.clear();
    bytes_ = 0;
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests glyph_cache | tail -2`
Expected: предупреждений нет; `4 tests, 0 failed checks`.

- [ ] **Step 5: Коммит**

```bash
git add src/core/glyph_cache.hpp src/core/glyph_cache.cpp tests/test_glyph_cache.cpp
git commit -m "Кеш букв на размере двойника: LRU с пределом в байтах (этап 6, §4)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Рендер рисует плитку из двойника

Спецификация §6: рисование интерфейса с двойником плитки (`DrawCmd::twin`), сделанным при `K = k`, рисует новым
режимом `uiMode = 3`: тексель `clamp(floor(SV_Position.xy) − O, 0, размер − 1)` читается `Load`, premultiplied →
обычный цвет, дальше — как текстура стадии 0. `O = ceil(x0·k) + floor(tx·K)`, `x0` — левый край квада в точках игры
минус `tx` (по `y` так же). Иначе — как в 0.4. Константы: `twin[4]` — 29–32.

**Files:**
- Modify: `src/core/frame.hpp`, `src/win/gpu/impl.hpp`, `src/win/gpu/shaders.cpp`, `src/win/gpu/draw.cpp`
- Test: `tests/test_frame.cpp`

**Interfaces:**
- Consumes: `tileOrigin`, `twinMatches` (Task 1).
- Produces: `DrawCmd::twin` (`uint32_t`, текстура рендера; 0 — нет), `DrawCmd::twinTile[2]` (`int32_t`, место плитки в
  картинке, точки игры), `DrawCmd::twinScale` (`double`, `K`); `Constants::twin[4]`, `kConstantCount == 33`.

- [ ] **Step 1: Падающий тест**

В `tests/test_frame.cpp` добавить в конец:
```cpp

TEST(frame_draw_carries_a_window_twin) {
    CHECK_EQ(DrawCmd{}.twin, 0u);  // none unless the driver attaches one (stage 6)
    FrameRecord f;
    DrawCmd cmd;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.twin = 7;
    cmd.twinTile[0] = 256;
    cmd.twinTile[1] = 0;
    cmd.twinScale = 1.75;
    const uint8_t vertices[32 * 3] = {};
    f.draw(cmd, vertices, 3, nullptr, 0);
    CHECK_EQ(f.draws()[0].twin, 7u);
    CHECK_EQ(f.draws()[0].twinTile[0], 256);
    CHECK(f.draws()[0].twinScale == 1.75);
}
```

- [ ] **Step 2: Тест падает**

Run: `cmake --build build-host -j 2>&1 | grep -m3 error`
Expected: ошибка сборки — у `DrawCmd` нет поля `twin`.

- [ ] **Step 3: Поля рисования, константы, шейдер, путь рисования**

В `src/core/frame.hpp` заменить:
```cpp
    uint32_t firstIndex = 0;       // into indices()
    uint32_t indexCount = 0;       // 0 = not indexed
};
```
на:
```cpp
    uint32_t firstIndex = 0;       // into indices()
    uint32_t indexCount = 0;       // 0 = not indexed
    // Stage 6: a tile of a game window picture drawn from the picture's twin (win/crisp) - the twin texture, the
    // tile's place in the picture in game pixels and the scale K the twin was made at; twin 0 = none.
    uint32_t twin = 0;
    int32_t twinTile[2] = {0, 0};
    double twinScale = 0.0;
};
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    uint32_t uiMode;       // 13: uiFilterMode() - 0 none, 1 sharp, 2 smooth (interface draws at k > 1)
```
на:
```cpp
    uint32_t uiMode;       // 13: uiFilterMode() - 0 none, 1 sharp, 2 smooth (interface draws at k > 1); 3 a twin
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    float sharpen;         // 28: blit only - CAS sharpness 0..1 of the picture on screen, below 0 = none
};
constexpr UINT kConstantCount = sizeof(Constants) / 4;
static_assert(kConstantCount == 29, "Constants must stay 29 DWORDs");
```
на:
```cpp
    float sharpen;         // 28: blit only - CAS sharpness 0..1 of the picture on screen, below 0 = none
    float twin[4];         // 29-32: uiMode 3 only (stage 6) - the twin texel origin O (x, y), the twin texture's size
};
constexpr UINT kConstantCount = sizeof(Constants) / 4;
static_assert(kConstantCount == 33, "Constants must stay 33 DWORDs");
```

Четыре числа в HLSL — отдельными `float`: `float4` с 29-го слова перешёл бы на границу регистра (упаковка cbuffer).

В `src/win/gpu/shaders.cpp` заменить:
```cpp
    float4 sourceClamp;
    float sharpen;
};
```
на:
```cpp
    float4 sourceClamp;
    float sharpen;
    float twinX;  // uiMode 3 (stage 6): the twin texel origin O and the twin texture's size
    float twinY;
    float twinW;
    float twinH;
};
```

В `src/win/gpu/shaders.cpp` заменить:
```cpp
float4 draw_ps(PsIn i) : SV_Target {
```
на:
```cpp
// Stage 6 (crisp text spec §3, §6): a window tile drawn from its twin. Target pixel p shows texel p - O of the
// tile's twin texture - one texel per pixel; past the texture the last column and row repeat. The twin is
// premultiplied; the game's blending wants the plain colour.
float4 twinTexel(float2 pos) {
    float2 t = clamp(floor(pos) - float2(twinX, twinY), float2(0.0, 0.0), float2(twinW, twinH) - 1.0);
    float4 c = tex0.Load(int3(int2(t), 0));
    return c.a > 0.0 ? float4(c.rgb / c.a, c.a) : float4(0.0, 0.0, 0.0, 0.0);
}

float4 draw_ps(PsIn i) : SV_Target {
```

В `src/win/gpu/shaders.cpp` заменить:
```cpp
    float4 t0 = tex0.Sample(samp0, uv0);
    if (uiMode != 0) t0 = uiSample(uv0, du, dv, i.pos.xy);
```
на:
```cpp
    float4 t0 = tex0.Sample(samp0, uv0);
    if (uiMode == 3)
        t0 = twinTexel(i.pos.xy);
    else if (uiMode != 0)
        t0 = uiSample(uv0, du, dv, i.pos.xy);
```

В `src/win/gpu/draw.cpp` заменить:
```cpp
#include "core/scale.hpp"
#include "win/gpu/impl.hpp"
```
на:
```cpp
#include "core/scale.hpp"
#include "core/twin_grid.hpp"
#include "win/gpu/impl.hpp"
```

В `src/win/gpu/draw.cpp` заменить:
```cpp
    Texture* t[2] = {nullptr, nullptr};
    for (int i = 0; i < 2; ++i) {
        auto it = textures.find(d.texture[i] ? d.texture[i] : white);
```
на:
```cpp
    // Stage 6: a window tile whose twin was made at this scale is drawn from the twin, a texel per target pixel;
    // otherwise (Pause, another scale, no twin) exactly as before.
    const bool twin = d.twin && s.ui && twinMatches(d.twinScale, scale) && textures.count(d.twin);
    Texture* t[2] = {nullptr, nullptr};
    for (int i = 0; i < 2; ++i) {
        const uint32_t id = i == 0 && twin ? d.twin : d.texture[i];
        auto it = textures.find(id ? id : white);
```

В `src/win/gpu/draw.cpp` заменить:
```cpp
    c.uiMode = uint32_t(uiFilterMode(s.ui, scale, uiSharp ? UiFilter::Sharp : UiFilter::Smooth));
    if (c.uiMode != 0) uiPixels(rec, d, c.uiPixels);
```
на:
```cpp
    c.uiMode = uint32_t(uiFilterMode(s.ui, scale, uiSharp ? UiFilter::Sharp : UiFilter::Smooth));
    if (twin) {  // the tile's first game pixel is the window's place plus the tile's (spec §6)
        float quad[4];
        uiPixels(rec, d, quad);
        c.uiMode = 3;
        c.twin[0] = float(tileOrigin(int(quad[0]) - d.twinTile[0], d.twinTile[0], scale, d.twinScale));
        c.twin[1] = float(tileOrigin(int(quad[1]) - d.twinTile[1], d.twinTile[1], scale, d.twinScale));
        c.twin[2] = float(t[0]->width);
        c.twin[3] = float(t[0]->height);
    } else if (c.uiMode != 0) {
        uiPixels(rec, d, c.uiPixels);
    }
```

- [ ] **Step 4: Тест, сборка, шейдер**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests frame_draw_carries | tail -1`
Expected: предупреждений нет; `1 tests, 0 failed checks`.

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/hlsl_check.sh`
Expected: `hlsl ok: draw_vs draw_ps present_vs present_ps`.

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify3.sh`
Expected: `OK: build-win/ddraw.dll: …`, затем `176 tests, 0 failed checks`.

- [ ] **Step 5: Коммит**

```bash
git add src/core/frame.hpp src/win/gpu/impl.hpp src/win/gpu/shaders.cpp src/win/gpu/draw.cpp tests/test_frame.cpp
git commit -m "Рендер рисует плитку окна из двойника: режим шейдера 3, тексель на точку цели (этап 6, §6)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 6: Перехваты методов картинки окна

Спецификация §5: проверка exe (база `0x400000`, 10 записей vtable `0x6956EC`, три импорта ведут в `gdi32`) — иначе
`crisp text: off (<что>)`; подмена 7 записей vtable (слоты 0, 2, 4, 5, 6, 7, 8) и импорта `CreateDIBSection`
(`VirtualProtect`), возврат при закрытии рендера. Спецификация §4: Clear — заливка блоков, CopyFrom — сверка и
дочерняя «поверх», CopyPixels, спрайт и копия из текстуры — сверка; двойник создаётся при первой записи, ищется по
адресу, сбрасывается деструктором и `CreateDIBSection` (`this = ppvBits − 0x18`). `K` — `k` с включёнными
улучшениями; новая `K` удаляет двойники и ставит всем окнам признак «изменено». Ключ `[display] crisp_text`. Показа
ещё нет (Task 8).

**Files:**
- Create: `src/core/crisp_exe.hpp`, `src/core/crisp_exe.cpp`, `src/win/crisp/crisp.hpp`, `src/win/crisp/state.hpp`,
  `src/win/crisp/crisp.cpp`, `tools/check_hooks.py`
- Modify: `src/core/config.hpp`, `src/core/config.cpp`, `src/win/context.cpp`
- Test: `tests/test_crisp_exe.cpp`, `tests/test_config.cpp`

**Interfaces:**
- Consumes: `TwinPicture`, `copyFromRegion` (Task 2); `GlyphCache` (Task 4).
- Produces:
  - `namespace crisp_exe` — константы exe (`kImageBase`, `kBitmapVtable`, `kBitmapSlots`, `kBitmapMethods`, слоты
    `kDestroy … kDraw`, поля `kWidth`, `kHeight`, `kDc`, `kBits`, `kDirty`, `kTiles`, `kTileCount`,
    `kTextureSurface`, импорты `kImportTextOutW`, `kImportFillRgn`, `kImportCreateDibSection`, `kCreateDibReturn`,
    окна `kWindowList`, `kWindowDirty`, `kWindowChildren`, узел `kNodeNext`, `kNodeValue`) и
    `std::string checkBitmapMethods(const uint32_t entries[kBitmapSlots])`;
  - `win::crisp`: `void install(bool enabled)`, `void uninstall()`, `void setScale(double K)`, `std::string stats()`;
  - внутреннее (`win/crisp/state.hpp`): `UAGFX_THISCALL`, `field<T>(object, offset)`, `struct Picture` (с `tiles`),
    `struct TileRef`, `struct State` (`original[10]`, `textOutW`, `fillRgn`, `createDibSection`, `pictures`, `dcs`,
    `tiles`, `bytes`, `loggedSizes`, `glyphs`), `State& state()`, `bool active()`, `Picture* picture(uintptr_t)`,
    `void drop(uintptr_t)`;
  - `Config::crispText` (`bool`, по умолчанию `true`), ключ `display.crisp_text`;
  - `tools/check_hooks.py OBJECT NAME=BYTES...` — сколько байт снимает со стека каждая подмена.

- [ ] **Step 1: Падающие тесты**

`tests/test_crisp_exe.cpp`:
```cpp
#include <string>

#include "core/crisp_exe.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(crisp_exe_the_known_methods_pass) {
    uint32_t e[crisp_exe::kBitmapSlots];
    for (int i = 0; i < crisp_exe::kBitmapSlots; ++i) e[i] = crisp_exe::kBitmapMethods[i];
    CHECK(crisp_exe::checkBitmapMethods(e).empty());
    // As read from the .rdata of uaRO.exe (docs/research/text.md §1.1).
    CHECK_EQ(crisp_exe::kBitmapMethods[crisp_exe::kClear], 0x403020u);
    CHECK_EQ(crisp_exe::kBitmapMethods[crisp_exe::kDraw], 0x403880u);
}

TEST(crisp_exe_another_method_turns_crisp_text_off) {
    uint32_t e[crisp_exe::kBitmapSlots];
    for (int i = 0; i < crisp_exe::kBitmapSlots; ++i) e[i] = crisp_exe::kBitmapMethods[i];
    e[7] = 0x10001234;  // another exe, or something patched the vtable before us
    const std::string why = crisp_exe::checkBitmapMethods(e);
    CHECK(why.find("method 7") != std::string::npos);
    CHECK(why.find("0x10001234") != std::string::npos);
    CHECK(why.find("0x00403460") != std::string::npos);
}
```

В `tests/test_config.cpp` заменить:
```cpp
    CHECK(!c.allowTearing);
```
на:
```cpp
    CHECK(!c.allowTearing);
    CHECK(c.crispText);
```

В `tests/test_config.cpp` заменить:
```cpp
        "high_resolution = 0\nui_filter = smooth\n"
```
на:
```cpp
        "high_resolution = 0\nui_filter = smooth\ncrisp_text = 0\n"
```

В `tests/test_config.cpp` заменить:
```cpp
    CHECK(c.uiFilter == UiFilter::Smooth);
```
на:
```cpp
    CHECK(c.uiFilter == UiFilter::Smooth);
    CHECK(!c.crispText);
```

В `tests/test_config.cpp` заменить:
```cpp
        "high_resolution = maybe\nui_filter = blurry\n[quality]\nrender_scale = 300\nsharpen = 101\n"
```
на:
```cpp
        "high_resolution = maybe\nui_filter = blurry\ncrisp_text = maybe\n"
        "[quality]\nrender_scale = 300\nsharpen = 101\n"
```

В `tests/test_config.cpp` заменить:
```cpp
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(c.logLevel == LogLevel::Info);
```
на:
```cpp
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(c.crispText);
    CHECK(c.logLevel == LogLevel::Info);
```

- [ ] **Step 2: Тесты падают**

Run: `cmake --build build-host -j 2>&1 | grep -m3 error`
Expected: ошибки сборки — нет `core/crisp_exe.hpp`, у `Config` нет `crispText`.

- [ ] **Step 3: Проверка exe и ключ**

`src/core/crisp_exe.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <string>

namespace uagfx::crisp_exe {

// The uaRO.exe stage 6 hooks (docs/research/text.md; crisp text spec §2, §5). Anything else turns crisp text off.
constexpr uint32_t kImageBase = 0x400000;
// CDCBitmap, the game's window picture: its vtable, the ten methods in it, its fields.
constexpr uint32_t kBitmapVtable = 0x6956EC;
constexpr int kBitmapSlots = 10;
constexpr uint32_t kBitmapMethods[kBitmapSlots] = {0x402EB0, 0x403780, 0x402FF0, 0x403010, 0x4037C0,
                                                    0x4030E0, 0x4031B0, 0x403460, 0x403020, 0x403880};
enum Slot { kDestroy = 0, kGetDc = 2, kCopyPixels = 4, kCopyFrom = 5, kSprite = 6, kTextureCopy = 7, kClear = 8,
            kDraw = 9 };
constexpr uint32_t kWidth = 0x04;
constexpr uint32_t kHeight = 0x08;
constexpr uint32_t kDc = 0x0C;
constexpr uint32_t kBits = 0x18;
constexpr uint32_t kDirty = 0x1C;           // a byte: set by every write, cleared by the upload
constexpr uint32_t kTiles = 0x24;           // std::list<CTexture*>: the pointer to its head node
constexpr uint32_t kTileCount = 0x28;
constexpr uint32_t kTextureSurface = 0x0C;  // CTexture: its IDirectDrawSurface7
// The GDI imports of uaRO.exe.
constexpr uint32_t kImportTextOutW = 0x695084;
constexpr uint32_t kImportFillRgn = 0x69507C;
constexpr uint32_t kImportCreateDibSection = 0x6950A0;
constexpr uint32_t kCreateDibReturn = 0x403A16;  // after the only call (FUN_00403960): its ppvBits is &bitmap->bits
// UIWindowMgr (0x727ED8): the list of top-level windows; UIWindow: the flag Invalidate sets, its children.
constexpr uint32_t kWindowList = 0x727ED8 + 0x38;  // the pointer to the list's head node
constexpr uint32_t kWindowDirty = 0x24;            // a dword
constexpr uint32_t kWindowChildren = 0x0C;         // the pointer to the head node of its children
// An MSVC std::list node: next, previous, value.
constexpr uint32_t kNodeNext = 0x00;
constexpr uint32_t kNodeValue = 0x08;

// "" when the ten vtable entries are the known methods, else what differs.
std::string checkBitmapMethods(const uint32_t entries[kBitmapSlots]);

}  // namespace uagfx::crisp_exe
```

`src/core/crisp_exe.cpp`:
```cpp
#include "core/crisp_exe.hpp"

#include <cstdio>

namespace uagfx::crisp_exe {

std::string checkBitmapMethods(const uint32_t entries[kBitmapSlots]) {
    for (int i = 0; i < kBitmapSlots; ++i)
        if (entries[i] != kBitmapMethods[i]) {
            char s[96];
            std::snprintf(s, sizeof s, "window picture method %d is 0x%08x, expected 0x%08x", i,
                          static_cast<unsigned>(entries[i]), static_cast<unsigned>(kBitmapMethods[i]));
            return s;
        }
    return "";
}

}  // namespace uagfx::crisp_exe
```

В `src/core/config.hpp` заменить:
```cpp
    UiFilter uiFilter = UiFilter::Sharp;
```
на:
```cpp
    UiFilter uiFilter = UiFilter::Sharp;
    bool crispText = true;       // window text and lines at the window's real pixels when k > 1 (stage 6)
```

В `src/core/config.cpp` заменить:
```cpp
            if (val == "smooth") c.uiFilter = UiFilter::Smooth;
```
на:
```cpp
            if (val == "smooth") c.uiFilter = UiFilter::Smooth;
        } else if (key == "display.crisp_text") {
            setBool(val, c.crispText);
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests crisp_exe | tail -1; ./build-host/uagfx_tests config_ | tail -1`
Expected: предупреждений нет; `2 tests, 0 failed checks`; `8 tests, 0 failed checks`.

- [ ] **Step 5: Модуль `win/crisp`: состояние, проверка, подмены, масштаб**

`src/win/crisp/crisp.hpp`:
```cpp
#pragma once
#include <string>

namespace uagfx::win::crisp {

// Stage 6, crisp text (docs/superpowers/specs/2026-10-13-stage6-crisp-text-design.md): the driver keeps a twin of
// every game window picture at the window's real pixels and draws the windows' tiles from it. Called under the
// driver's lock; the hooks take it themselves.

// At the uaRO renderer's start: checks the exe and puts the hooks in place; logs "crisp text: on" or why it is off.
void install(bool enabled);
// When the renderer closes: the game's methods and imports back, every twin and its textures gone.
void uninstall();
// At every present, between the game's frames: the scale twins are made at - k with every improvement on (Pause
// does not change it). A new scale drops the twins and has the game redraw every window.
void setScale(double K);
// For the 30-second stats line: "" while crisp text is off.
std::string stats();

}  // namespace uagfx::win::crisp
```

`src/win/crisp/state.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "core/crisp_exe.hpp"
#include "core/glyph_cache.hpp"
#include "core/twin.hpp"

// The game's methods are MSVC __thiscall: this in ECX, the callee pops its stack arguments.
#define UAGFX_THISCALL __attribute__((thiscall))

namespace uagfx::win::crisp {

// A field of a game object.
template <typename T>
T& field(uintptr_t object, uintptr_t offset) {
    return *reinterpret_cast<T*>(object + offset);
}

// One game window picture (CDCBitmap) and its twin (spec §4).
struct Picture {
    uintptr_t self = 0;  // the CDCBitmap
    HDC dc = nullptr;
    const uint32_t* bits = nullptr;
    std::unique_ptr<TwinPicture> twin;
    struct Tile {  // the twin texture of one of the game's tiles (spec §6)
        uint32_t texture = 0;  // in the renderer; 0 = none yet
        int width = 0;
        int height = 0;
    };
    std::vector<Tile> tiles;  // by the game's tile index
};

// A tile's surface: whose tile it is.
struct TileRef {
    uintptr_t picture = 0;
    int index = 0;
};

struct State {
    bool installed = false;
    double K = 0.0;  // the twins' scale; they are kept only above 1
    uintptr_t original[crisp_exe::kBitmapSlots] = {};  // the game's methods in the vtable entries the hooks took
    uintptr_t textOutW = 0;  // the game's GDI imports before the hooks
    uintptr_t fillRgn = 0;
    uintptr_t createDibSection = 0;
    std::unordered_map<uintptr_t, std::unique_ptr<Picture>> pictures;  // by the CDCBitmap's address
    std::unordered_map<HDC, uintptr_t> dcs;                             // a picture's DC -> its CDCBitmap
    std::unordered_map<const void*, TileRef> tiles;                     // a tile's surface -> its picture
    size_t bytes = 0;  // every twin and copy together
    bool budgetLogged = false;
    std::set<uint32_t> loggedSizes;  // the safety net writes one line per window size
    GlyphCache glyphs{size_t(16) << 20};
};

State& state();
bool active();  // the hooks are in place and K > 1
// The picture of a CDCBitmap: made from its pixels at first sight, made again when the address holds another
// picture now; null while crisp text is off or past the memory budget.
Picture* picture(uintptr_t bitmap);
void drop(uintptr_t bitmap);  // its twin, copy, tile textures and tile surfaces

}  // namespace uagfx::win::crisp
```

`src/win/crisp/crisp.cpp`:
```cpp
#include "win/crisp/crisp.hpp"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <iterator>
#include <new>
#include <vector>

#include "win/context.hpp"
#include "win/crisp/state.hpp"
#include "win/log.hpp"

namespace uagfx::win::crisp {

namespace ce = crisp_exe;

State& state() {
    static State s;
    return s;
}

bool active() { return state().installed && state().K > 1.0; }

namespace {

constexpr size_t kBudget = size_t(256) << 20;  // every twin and copy together: a 32-bit game has 2 GB

uintptr_t self(const void* p) { return reinterpret_cast<uintptr_t>(p); }

template <typename Fn>
Fn original(int slot) {
    return reinterpret_cast<Fn>(state().original[slot]);
}

Rect whole(const Picture& p) { return Rect{0, 0, p.twin->width(), p.twin->height()}; }

// [at, at + n) lies in committed, readable memory: the game's window lists are walked on its word.
bool readable(uintptr_t at, size_t n) {
    MEMORY_BASIC_INFORMATION m{};
    if (!at || !VirtualQuery(reinterpret_cast<const void*>(at), &m, sizeof m) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return false;
    return at + n <= reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
}

// Writes a pointer into the exe's read-only data: a vtable entry or an import.
bool write(uintptr_t at, uintptr_t value) {
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(at), sizeof value, PAGE_EXECUTE_READWRITE, &old)) return false;
    *reinterpret_cast<uintptr_t*>(at) = value;
    DWORD unused = 0;
    VirtualProtect(reinterpret_cast<void*>(at), sizeof value, old, &unused);
    return true;
}

void forgetDc(uintptr_t bitmap) {
    State& s = state();
    const auto it = s.dcs.find(field<HDC>(bitmap, ce::kDc));
    if (it != s.dcs.end() && it->second == bitmap) s.dcs.erase(it);
}

void dropAll() {
    State& s = state();
    while (!s.pictures.empty()) drop(s.pictures.begin()->first);
    s.tiles.clear();
}

// The game's windows from a list's head node down through their children: each gets the flag Invalidate sets, so
// the game redraws it at its next frame (spec §5). Returns how many.
int markWindows(uintptr_t head, int depth) {
    if (depth > 32 || !readable(head, 12)) return 0;
    int n = 0;
    int guard = 0;
    for (uintptr_t node = field<uintptr_t>(head, ce::kNodeNext); node != head && ++guard < 100000;
         node = field<uintptr_t>(node, ce::kNodeNext)) {
        if (!readable(node, 12)) break;
        const uintptr_t window = field<uintptr_t>(node, ce::kNodeValue);
        if (!readable(window, ce::kWindowDirty + 4)) continue;
        field<uint32_t>(window, ce::kWindowDirty) = 1;
        n += 1 + markWindows(field<uintptr_t>(window, ce::kWindowChildren), depth + 1);
    }
    return n;
}

// --- The window picture's methods (docs/research/text.md §1.1): the game's first, then the twin (spec §4). ---

using DestroyFn = void*(UAGFX_THISCALL*)(void*, unsigned);
using GetDcFn = void(UAGFX_THISCALL*)(void*, HDC*);
using CopyPixelsFn = void(UAGFX_THISCALL*)(void*, int, int, int, int, const uint32_t*, int);
using CopyFromFn = void(UAGFX_THISCALL*)(void*, int, int, int, int, void*);
using SpriteFn = void(UAGFX_THISCALL*)(void*, int, int, int, int, int);
using TextureCopyFn = void(UAGFX_THISCALL*)(void*, int, int, int, int, int, int, int, int, int, int);
using ClearFn = void(UAGFX_THISCALL*)(void*, const RECT*, uint32_t);
using CreateDibFn = HBITMAP(WINAPI*)(HDC, const BITMAPINFO*, UINT, void**, HANDLE, DWORD);

void* UAGFX_THISCALL onDestroy(void* p, unsigned flags) {
    Guard guard;
    drop(self(p));
    forgetDc(self(p));
    return original<DestroyFn>(ce::kDestroy)(p, flags);
}

void UAGFX_THISCALL onGetDc(void* p, HDC* out) {
    Guard guard;
    original<GetDcFn>(ce::kGetDc)(p, out);
    if (active() && out && *out) state().dcs[*out] = self(p);  // TextOutW and FillRgn find the picture by its DC
}

void UAGFX_THISCALL onCopyPixels(void* p, int x, int y, int w, int h, const uint32_t* src, int key) {
    Guard guard;
    original<CopyPixelsFn>(ce::kCopyPixels)(p, x, y, w, h, src, key);
    if (Picture* pic = picture(self(p))) pic->twin->sync(pic->bits, Rect{x, y, x + w, y + h});
}

void UAGFX_THISCALL onCopyFrom(void* p, int x, int y, int w, int h, void* child) {
    Guard guard;
    Picture* pic = picture(self(p));
    Picture* kid = pic && child ? picture(self(child)) : nullptr;
    Rect r;
    const bool copies = pic && copyFromRegion(x, y, w, h, pic->twin->width(), pic->twin->height(), r);
    if (copies) {  // what both twins missed so far, before the copy mixes them
        pic->twin->sync(pic->bits, r);
        if (kid) kid->twin->sync(kid->bits, whole(*kid));
    }
    original<CopyFromFn>(ce::kCopyFrom)(p, x, y, w, h, child);
    if (!copies) return;
    if (kid)
        pic->twin->copyFrom(pic->bits, *kid->twin, r.left, r.top, r.width(), r.height());
    else
        pic->twin->sync(pic->bits, r);
}

void UAGFX_THISCALL onSprite(void* p, int a, int b, int c, int d, int e) {
    Guard guard;
    original<SpriteFn>(ce::kSprite)(p, a, b, c, d, e);
    if (Picture* pic = picture(self(p))) pic->twin->sync(pic->bits, whole(*pic));
}

void UAGFX_THISCALL onTextureCopy(void* p, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    Guard guard;
    original<TextureCopyFn>(ce::kTextureCopy)(p, a, b, c, d, e, f, g, h, i, j);
    if (Picture* pic = picture(self(p))) pic->twin->sync(pic->bits, whole(*pic));
}

void UAGFX_THISCALL onClear(void* p, const RECT* r, uint32_t color) {
    Guard guard;
    original<ClearFn>(ce::kClear)(p, r, color);
    if (Picture* pic = picture(self(p))) {
        const Rect all = whole(*pic);
        pic->twin->overwrite(pic->bits, r ? Rect{int(r->left), int(r->top), int(r->right), int(r->bottom)} : all);
    }
}

HBITMAP WINAPI onCreateDibSection(HDC dc, const BITMAPINFO* info, UINT usage, void** bits, HANDLE section,
                                  DWORD offset) {
    Guard guard;
    const auto real = reinterpret_cast<CreateDibFn>(state().createDibSection);
    const HBITMAP made = real(dc, info, usage, bits, section, offset);
    // The game's only call (FUN_00403960) passes &bitmap->bits: a new DIB for that window picture, so whatever twin
    // the address had is gone (spec §5).
    if (bits && reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == ce::kCreateDibReturn)
        drop(self(bits) - ce::kBits);
    return made;
}

// Where a hook goes and where the game's own function is kept.
struct Hook {
    uintptr_t at;
    uintptr_t hook;
    uintptr_t* original;
};

std::vector<Hook> hooks() {
    State& s = state();
    const auto slot = [&s](int i, uintptr_t fn) {
        return Hook{ce::kBitmapVtable + 4u * uint32_t(i), fn, &s.original[i]};
    };
    return {
        slot(ce::kDestroy, reinterpret_cast<uintptr_t>(&onDestroy)),
        slot(ce::kGetDc, reinterpret_cast<uintptr_t>(&onGetDc)),
        slot(ce::kCopyPixels, reinterpret_cast<uintptr_t>(&onCopyPixels)),
        slot(ce::kCopyFrom, reinterpret_cast<uintptr_t>(&onCopyFrom)),
        slot(ce::kSprite, reinterpret_cast<uintptr_t>(&onSprite)),
        slot(ce::kTextureCopy, reinterpret_cast<uintptr_t>(&onTextureCopy)),
        slot(ce::kClear, reinterpret_cast<uintptr_t>(&onClear)),
        Hook{ce::kImportCreateDibSection, reinterpret_cast<uintptr_t>(&onCreateDibSection), &s.createDibSection},
    };
}

// "" when this is the uaRO.exe the hooks know (spec §5).
std::string check() {
    if (reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) != ce::kImageBase) return "the game is not at 0x400000";
    if (!readable(ce::kBitmapVtable, 4 * ce::kBitmapSlots)) return "no window picture methods at 0x6956ec";
    uint32_t entries[ce::kBitmapSlots];
    std::memcpy(entries, reinterpret_cast<const void*>(uintptr_t(ce::kBitmapVtable)), sizeof entries);
    const std::string methods = ce::checkBitmapMethods(entries);
    if (!methods.empty()) return methods;
    const HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    const struct {
        uint32_t at;
        const char* name;
    } imports[] = {{ce::kImportTextOutW, "TextOutW"},
                   {ce::kImportFillRgn, "FillRgn"},
                   {ce::kImportCreateDibSection, "CreateDIBSection"}};
    for (const auto& i : imports)
        if (!gdi || !readable(i.at, 4) ||
            field<uintptr_t>(i.at, 0) != reinterpret_cast<uintptr_t>(GetProcAddress(gdi, i.name)))
            return std::string("the game's ") + i.name + " does not lead to gdi32";
    return "";
}

}  // namespace

Picture* picture(uintptr_t bitmap) {
    State& s = state();
    if (!active() || !bitmap) return nullptr;
    GdiFlush();  // GDI may still hold drawing for these pixels
    const uint32_t* bits = field<const uint32_t*>(bitmap, ce::kBits);
    const int w = field<int>(bitmap, ce::kWidth);
    const int h = field<int>(bitmap, ce::kHeight);
    const auto it = s.pictures.find(bitmap);
    if (it != s.pictures.end()) {
        const Picture& p = *it->second;
        if (p.bits == bits && p.twin->width() == w && p.twin->height() == h) return it->second.get();
        drop(bitmap);  // another picture at that address: its twin starts again
    }
    if (!bits || w <= 0 || h <= 0 || w > 8192 || h > 8192) return nullptr;
    const size_t bytes = TwinPicture::bytesFor(w, h, s.K);
    if (s.bytes + bytes > kBudget) {
        if (!s.budgetLogged)
            LOG_INFO("crisp text: twins reached %u MB, more windows stay as in 0.4", unsigned(kBudget >> 20));
        s.budgetLogged = true;
        return nullptr;
    }
    std::unique_ptr<Picture> p;
    try {  // an exception must not reach the game's frames
        p = std::make_unique<Picture>();
        p->twin = std::make_unique<TwinPicture>(bits, w, h, s.K);
    } catch (const std::bad_alloc&) {
        return nullptr;  // no memory for this twin: the window stays as in 0.4
    }
    p->self = bitmap;
    p->dc = field<HDC>(bitmap, ce::kDc);
    p->bits = bits;
    Picture* made = p.get();
    if (p->dc) s.dcs[p->dc] = bitmap;
    s.bytes += bytes;
    s.pictures[bitmap] = std::move(p);
    return made;
}

void drop(uintptr_t bitmap) {
    State& s = state();
    const auto it = s.pictures.find(bitmap);
    if (it == s.pictures.end()) return;
    gpu::Renderer* r = Context::get().renderer;
    for (const Picture::Tile& t : it->second->tiles)
        if (t.texture && r) r->releaseTexture(t.texture);
    for (auto t = s.tiles.begin(); t != s.tiles.end();)
        t = t->second.picture == bitmap ? s.tiles.erase(t) : std::next(t);
    s.bytes -= it->second->twin->bytes();
    s.pictures.erase(it);
}

void install(bool enabled) {
    State& s = state();
    if (s.installed) return;
    if (!enabled) {
        LOG_INFO("crisp text: off (crisp_text = 0)");
        return;
    }
    const std::string why = check();
    if (!why.empty()) {
        LOG_INFO("crisp text: off (%s)", why.c_str());
        return;
    }
    const std::vector<Hook> list = hooks();
    size_t done = 0;
    for (; done < list.size(); ++done) {
        *list[done].original = field<uintptr_t>(list[done].at, 0);
        if (!write(list[done].at, list[done].hook)) break;
    }
    if (done < list.size()) {
        const unsigned long error = GetLastError();
        while (done > 0) {
            --done;
            write(list[done].at, *list[done].original);
        }
        LOG_INFO("crisp text: off (the game's methods cannot be changed: error %lu)", error);
        return;
    }
    s.installed = true;
    s.K = 0.0;
    LOG_INFO("crisp text: on");
}

void uninstall() {
    State& s = state();
    if (!s.installed) return;
    for (const Hook& h : hooks()) write(h.at, *h.original);
    dropAll();
    s.dcs.clear();
    s.loggedSizes.clear();
    s.budgetLogged = false;
    s.K = 0.0;
    s.installed = false;
}

void setScale(double K) {
    State& s = state();
    if (!s.installed || K == s.K) return;
    dropAll();
    s.K = K;
    if (K <= 1.0) {
        LOG_INFO("crisp text: k %.3f, waiting for the game to be enlarged", K);
        return;
    }
    const uintptr_t head = readable(ce::kWindowList, 4) ? field<uintptr_t>(ce::kWindowList, 0) : 0;
    LOG_INFO("crisp text: k %.3f, %d windows redrawn", K, markWindows(head, 0));
}

std::string stats() {
    const State& s = state();
    if (!active()) return "";
    char line[128];
    std::snprintf(line, sizeof line, "; crisp text: %u windows, %.1f MB, %u letters",
                  static_cast<unsigned>(s.pictures.size()), double(s.bytes) / double(1 << 20),
                  static_cast<unsigned>(s.glyphs.size()));
    return line;
}

}  // namespace uagfx::win::crisp
```

`tools/check_hooks.py`:
```python
#!/usr/bin/env python3
"""Checks that every crisp text hook pops the stack like the game function it replaces.

The game's window picture methods are MSVC __thiscall and its GDI imports __stdcall: the callee pops its stack
arguments, and a hook that pops a different number of bytes breaks the game's stack at once. The byte counts are the
"ret N" of the game's methods in uaRO.exe (docs/research/text.md §1.1) and the arguments of the GDI functions.
Usage: check_hooks.py OBJECT NAME=BYTES...
"""
import re
import subprocess
import sys


def main(path, wanted):
    try:
        out = subprocess.run(["i686-w64-mingw32-objdump", "-d", path], capture_output=True, text=True,
                             check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        print("FAIL: cannot disassemble " + path)
        sys.exit(1)
    rets = {}
    current = None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            current = m.group(1)
            rets.setdefault(current, [])
        elif current and re.search(r"\sret\b", line):
            n = re.search(r"\$0x([0-9a-f]+)", line)
            rets[current].append(int(n.group(1), 16) if n else 0)
    failed = False
    for item in wanted:
        name, count = item.split("=")
        # its body and cold part; not the file's static initializer, which GCC names after the first function
        found = [f for f in rets if not f.startswith("__GLOBAL__") and re.search(r"\d%s(E|@|$)" % re.escape(name), f)]
        pops = sorted({n for f in found for n in rets[f]})
        if pops != [int(count, 0)]:
            print("FAIL: %s pops %s, the game's function pops %s" % (name, pops or "nothing found", count))
            failed = True
        else:
            print("OK: %s pops %s" % (name, count))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
```

- [ ] **Step 6: Включение в драйвер**

В `src/win/context.cpp` заменить:
```cpp
#include "core/scale.hpp"
#include "win/dpi.hpp"
```
на:
```cpp
#include "core/scale.hpp"
#include "win/crisp/crisp.hpp"
#include "win/dpi.hpp"
```

В `src/win/context.cpp` заменить:
```cpp
    LOG_INFO("Direct3D 12 renderer on %s", renderer->adapterName().c_str());
```
на:
```cpp
    LOG_INFO("Direct3D 12 renderer on %s", renderer->adapterName().c_str());
    crisp::install(config.crispText);
```

В `src/win/context.cpp` заменить:
```cpp
    logStats(true);
    delete renderer;  // waits for the GPU
```
на:
```cpp
    logStats(true);
    crisp::uninstall();  // the game's methods back; the twins' textures go before the renderer
    delete renderer;     // waits for the GPU
```

В `src/win/context.cpp` заменить:
```cpp
    const ScalePlan plan = planScale(shadow.width(), shadow.height(), out.cx, out.cy, config.renderScale,
                                     config.highResolution && enhanced_);
```
на:
```cpp
    const ScalePlan plan = planScale(shadow.width(), shadow.height(), out.cx, out.cy, config.renderScale,
                                     config.highResolution && enhanced_);
    // Stage 6: twins follow the scale with every improvement on, so Pause leaves them as they are.
    crisp::setScale(
        planScale(shadow.width(), shadow.height(), out.cx, out.cy, config.renderScale, config.highResolution).k);
```

В `src/win/context.cpp` заменить:
```cpp
        LOG_INFO("%u frames in %.1f s (%.1f fps), %u reads of the frame from the GPU", stats_.frames, ms / 1000.0,
                 ms ? stats_.frames * 1000.0 / ms : 0.0, stats_.readBacks);
```
на:
```cpp
        LOG_INFO("%u frames in %.1f s (%.1f fps), %u reads of the frame from the GPU%s", stats_.frames, ms / 1000.0,
                 ms ? stats_.frames * 1000.0 / ms : 0.0, stats_.readBacks, crisp::stats().c_str());
```

- [ ] **Step 7: Сборка, проверки, подмены снимают со стека столько же, сколько методы игры**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify3.sh`
Expected: `OK: build-win/ddraw.dll: 22 exports, … imports gdi32.dll kernel32.dll msvcrt.dll user32.dll`, затем
`178 tests, 0 failed checks`.

Run: `python3 tools/check_hooks.py build-win/CMakeFiles/ddraw.dir/src/win/crisp/crisp.cpp.obj onDestroy=4 onGetDc=4 onCopyPixels=0x18 onCopyFrom=0x14 onSprite=0x14 onTextureCopy=0x28 onClear=8 onCreateDibSection=0x18`
Expected: восемь строк `OK: …`, код выхода 0.

- [ ] **Step 8: Коммит**

```bash
git add src/core/crisp_exe.hpp src/core/crisp_exe.cpp tests/test_crisp_exe.cpp src/core/config.hpp src/core/config.cpp tests/test_config.cpp src/win/crisp/crisp.hpp src/win/crisp/state.hpp src/win/crisp/crisp.cpp src/win/context.cpp tools/check_hooks.py
git commit -m "Перехваты методов картинки окна: проверка exe, двойник ведётся за каждой записью игры, смена масштаба перерисовывает окна (этап 6, §4–§5)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 7: Текст и треугольники через GDI

Спецификация §4: подмена импорта `TextOutW` exe на DC картинки окна — сверка прямоугольника строки с запасом на высоту
шрифта, вызов игры, буквы на размере двойника (шрифт из DC, высота ×`K`, `GetGlyphOutlineW(GGO_GRAY8_BITMAP)`, кеш
букв) на местах раскладки игры (`GetTextExtentExPointW`), цвет `GetTextColor`, затем копия поглощает мелкие буквы.
Другое выравнивание, наклонный шрифт, не TrueType — как «прочая» запись. `FillRgn` — заливка блоков всех точек
области (`GetRegionData`).

**Files:**
- Create: `src/win/crisp/text.cpp`
- Modify: `src/win/crisp/state.hpp`, `src/win/crisp/crisp.cpp`

**Interfaces:**
- Consumes: `letterPen`, `baselineRow`, `drawGlyph`, `textRect`, `textColor`, `GlyphBitmap` (Task 3); `GlyphCache`
  (Task 4); `State`, `picture`, `active`, `field` (Task 6).
- Produces: `BOOL WINAPI hookTextOutW(HDC, int, int, LPCWSTR, int)`, `BOOL WINAPI hookFillRgn(HDC, HRGN, HBRUSH)`,
  `void dropFonts()` (`win/crisp/state.hpp`).

Задача — только Windows: проверяется сборкой без предупреждений, `check_hooks.py` и вычиткой; раскладка букв и их
наложение проверены тестами ядра (Task 3).

- [ ] **Step 1: Буквы и треугольники**

`src/win/crisp/text.cpp`:
```cpp
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "core/twin_text.hpp"
#include "win/context.hpp"
#include "win/crisp/state.hpp"

namespace uagfx::win::crisp {
namespace {

// A game font, by its LOGFONT: the twin needs it K times larger.
struct FontKey {
    std::wstring face;
    LONG height;
    LONG width;
    LONG weight;
    BYTE italic;
    BYTE charset;
    bool operator<(const FontKey& o) const {
        return std::tie(face, height, width, weight, italic, charset) <
               std::tie(o.face, o.height, o.width, o.weight, o.italic, o.charset);
    }
};

struct BigFont {
    HFONT font = nullptr;
    uint32_t id = 0;  // the glyph cache's number for it
};

std::map<FontKey, BigFont> g_fonts;
HDC g_glyphDc = nullptr;  // GetGlyphOutlineW reads the big fonts here
uint32_t g_nextFont = 1;

// The game's font K times larger (spec §4); null when GDI cannot make it.
const BigFont* bigFont(const LOGFONTW& lf, double K) {
    const FontKey key{lf.lfFaceName, lf.lfHeight, lf.lfWidth, lf.lfWeight, lf.lfItalic, lf.lfCharSet};
    auto it = g_fonts.find(key);
    if (it == g_fonts.end()) {
        LOGFONTW big = lf;
        big.lfHeight = LONG(std::lround(lf.lfHeight * K));
        big.lfWidth = LONG(std::lround(lf.lfWidth * K));
        big.lfQuality = ANTIALIASED_QUALITY;
        it = g_fonts.emplace(key, BigFont{CreateFontIndirectW(&big), g_nextFont++}).first;
    }
    return it->second.font ? &it->second : nullptr;
}

// A letter K times larger: from the cache, else from GDI; null when GDI cannot draw it (not a TrueType font).
const GlyphBitmap* glyph(const BigFont& f, wchar_t ch) {
    State& s = state();
    if (const GlyphBitmap* g = s.glyphs.find(f.id, ch)) return g;
    if (!g_glyphDc) g_glyphDc = CreateCompatibleDC(nullptr);
    if (!g_glyphDc || !SelectObject(g_glyphDc, f.font)) return nullptr;
    const MAT2 identity{{0, 1}, {0, 0}, {0, 0}, {0, 1}};
    GLYPHMETRICS gm{};
    const DWORD size = GetGlyphOutlineW(g_glyphDc, ch, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &identity);
    if (size == GDI_ERROR) return nullptr;
    GlyphBitmap g;
    g.originX = gm.gmptGlyphOrigin.x;
    g.originY = gm.gmptGlyphOrigin.y;
    if (size > 0) {  // a space has no pixels
        std::vector<uint8_t> raw(size);
        if (GetGlyphOutlineW(g_glyphDc, ch, GGO_GRAY8_BITMAP, &gm, size, raw.data(), &identity) == GDI_ERROR)
            return nullptr;
        g.width = int(gm.gmBlackBoxX);
        g.height = int(gm.gmBlackBoxY);
        const size_t pitch = (size_t(g.width) + 3) & ~size_t(3);  // rows are DWORD-aligned
        if (pitch * size_t(g.height) > raw.size()) return nullptr;
        g.coverage.resize(size_t(g.width) * size_t(g.height));
        for (int y = 0; y < g.height; ++y)
            std::copy_n(raw.data() + size_t(y) * pitch, size_t(g.width),
                        g.coverage.data() + size_t(y) * size_t(g.width));
    }
    return &s.glyphs.put(f.id, ch, std::move(g));
}

// The picture whose DC this is, while crisp text is on.
Picture* pictureOf(HDC dc) {
    if (!active()) return nullptr;
    State& s = state();
    const auto it = s.dcs.find(dc);
    if (it == s.dcs.end() || field<HDC>(it->second, crisp_exe::kDc) != dc) return nullptr;
    return picture(it->second);
}

Rect all(const Picture& p) { return Rect{0, 0, p.twin->width(), p.twin->height()}; }

using TextOutWFn = BOOL(WINAPI*)(HDC, int, int, LPCWSTR, int);
using FillRgnFn = BOOL(WINAPI*)(HDC, HRGN, HBRUSH);

}  // namespace

// The game's TextOutW on a window picture's DC (spec §4): the same letters K times larger, each where the game put
// its small one.
BOOL WINAPI hookTextOutW(HDC dc, int x, int y, LPCWSTR text, int count) {
    Guard guard;
    State& s = state();
    const auto real = reinterpret_cast<TextOutWFn>(s.textOutW);
    Picture* pic = text && count > 0 ? pictureOf(dc) : nullptr;
    if (!pic) return real(dc, x, y, text, count);
    LOGFONTW lf{};
    TEXTMETRICW tm{};
    SIZE extent{};
    std::vector<int> ends(static_cast<size_t>(count));  // the width of the first i + 1 letters in the game's font
    const BigFont* font = nullptr;
    if (GetTextAlign(dc) == 0 &&  // TA_TOP | TA_LEFT | TA_NOUPDATECP: the game never changes it
        GetObjectW(GetCurrentObject(dc, OBJ_FONT), sizeof lf, &lf) == int(sizeof lf) && lf.lfEscapement == 0 &&
        GetTextMetricsW(dc, &tm) && GetTextExtentExPointW(dc, text, count, 0, nullptr, ends.data(), &extent))
        font = bigFont(lf, s.K);
    bool letters = font != nullptr;
    for (int i = 0; letters && i < count; ++i) letters = glyph(*font, text[i]) != nullptr;  // every one, or none
    if (!letters) {  // like any other write: the small letters enlarged
        const BOOL ok = real(dc, x, y, text, count);
        GdiFlush();
        pic->twin->sync(pic->bits, all(*pic));
        return ok;
    }
    const Rect r = textRect(x, y, extent.cx, tm.tmHeight, tm.tmHeight);
    pic->twin->sync(pic->bits, r);  // what the twin missed there so far
    const BOOL ok = real(dc, x, y, text, count);
    GdiFlush();
    const uint32_t rgb = textColor(GetTextColor(dc));
    const int baseline = baselineRow(y, tm.tmAscent, s.K);
    for (int i = 0; i < count; ++i)
        if (const GlyphBitmap* g = glyph(*font, text[i]))
            drawGlyph(*pic->twin, *g, letterPen(x, i > 0 ? ends[size_t(i) - 1] : 0, s.K), baseline, rgb);
    pic->twin->absorb(pic->bits, r);  // the small letters stay out of the twin
    return ok;
}

// The game's FillRgn (DrawTriangle) on a window picture's DC: every pixel of the region takes its new value in the
// twin (spec §4).
BOOL WINAPI hookFillRgn(HDC dc, HRGN region, HBRUSH brush) {
    Guard guard;
    const BOOL ok = reinterpret_cast<FillRgnFn>(state().fillRgn)(dc, region, brush);
    Picture* pic = pictureOf(dc);  // after its GdiFlush the fill is in the pixels
    if (!pic) return ok;
    const DWORD size = GetRegionData(region, 0, nullptr);
    std::vector<uint8_t> data(size);
    if (size < sizeof(RGNDATAHEADER) || GetRegionData(region, size, reinterpret_cast<RGNDATA*>(data.data())) != size) {
        pic->twin->sync(pic->bits, all(*pic));
        return ok;
    }
    const RGNDATA* d = reinterpret_cast<const RGNDATA*>(data.data());
    const RECT* rects = reinterpret_cast<const RECT*>(d->Buffer);
    const DWORD fit = DWORD((size - sizeof(RGNDATAHEADER)) / sizeof(RECT));
    for (DWORD i = 0; i < std::min(d->rdh.nCount, fit); ++i) {
        const RECT& b = rects[i];
        pic->twin->overwrite(pic->bits, Rect{int(b.left), int(b.top), int(b.right), int(b.bottom)});
    }
    return ok;
}

void dropFonts() {
    if (g_glyphDc) DeleteDC(g_glyphDc);  // first: a font selected into a DC is not deleted
    g_glyphDc = nullptr;
    for (const auto& f : g_fonts)
        if (f.second.font) DeleteObject(f.second.font);
    g_fonts.clear();
}

}  // namespace uagfx::win::crisp
```

В `src/win/crisp/state.hpp` заменить:
```cpp
void drop(uintptr_t bitmap);  // its twin, copy, tile textures and tile surfaces

}  // namespace uagfx::win::crisp
```
на:
```cpp
void drop(uintptr_t bitmap);  // its twin, copy, tile textures and tile surfaces

// text.cpp: the game's text and triangles (spec §4); the big fonts go when K changes.
BOOL WINAPI hookTextOutW(HDC dc, int x, int y, LPCWSTR text, int count);
BOOL WINAPI hookFillRgn(HDC dc, HRGN region, HBRUSH brush);
void dropFonts();

}  // namespace uagfx::win::crisp
```

В `src/win/crisp/crisp.cpp` заменить:
```cpp
        Hook{ce::kImportCreateDibSection, reinterpret_cast<uintptr_t>(&onCreateDibSection), &s.createDibSection},
    };
```
на:
```cpp
        Hook{ce::kImportCreateDibSection, reinterpret_cast<uintptr_t>(&onCreateDibSection), &s.createDibSection},
        Hook{ce::kImportTextOutW, reinterpret_cast<uintptr_t>(&hookTextOutW), &s.textOutW},
        Hook{ce::kImportFillRgn, reinterpret_cast<uintptr_t>(&hookFillRgn), &s.fillRgn},
    };
```

В `src/win/crisp/crisp.cpp` заменить функцию `void dropAll() {` целиком:
```cpp
void dropAll() {
    State& s = state();
    while (!s.pictures.empty()) drop(s.pictures.begin()->first);
    s.tiles.clear();
    dropFonts();  // the big fonts and their letters were for the old K
    s.glyphs.clear();
}
```

- [ ] **Step 2: Сборка, проверки, подмены импортов**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify3.sh`
Expected: `OK: build-win/ddraw.dll: …`, затем `178 tests, 0 failed checks`.

Run: `python3 tools/check_hooks.py build-win/CMakeFiles/ddraw.dir/src/win/crisp/text.cpp.obj hookTextOutW=0x14 hookFillRgn=0xc`
Expected: две строки `OK: …`, код выхода 0.

- [ ] **Step 3: Коммит**

```bash
git add src/win/crisp/text.cpp src/win/crisp/state.hpp src/win/crisp/crisp.cpp
git commit -m "Текст и треугольники игры в двойнике: буквы шрифта игры ×K на местах её раскладки, заливка области (этап 6, §4)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Плитки окна из двойника

Спецификация §4, §6: подмена `Draw` (слот 9) — страховка (сверка всей картинки, если игра пометила её изменённой;
строка в лог на размер окна), вызов игры (её загрузка и квады плиток в список интерфейса, который игра рисует позже),
затем текстуры-двойники плиток: созданы или пересозданы при смене размера; в них грузится то, что изменилось в
двойнике с прошлого раза (`takeChanged`), новая текстура — целиком; поверхность каждой плитки отмечена —
`Device7::record` по ней прикрепляет двойник к рисованию (`DrawCmd::twin`). Поверхность, которую освобождает игра,
забывается.

**Files:**
- Modify: `src/win/crisp/crisp.hpp`, `src/win/crisp/crisp.cpp`, `src/win/dx7/device7.cpp`, `src/win/dx7/surface7.cpp`

**Interfaces:**
- Consumes: `tilePlace`, `tileSpan` (Task 1); `TwinPicture::texels`, `takeChanged` (Task 2); `DrawCmd::twin`, `twinTile`,
  `twinScale` (Task 5); `State::tiles`, `Picture::tiles`, `picture`, `active` (Task 6).
- Produces: `void tileDraw(const void* surface, DrawCmd& cmd)`, `void forgetSurface(const void* surface)`
  (`win/crisp/crisp.hpp`).

Задача — только Windows: проверяется сборкой, `check_hooks.py` и вычиткой; путь рендера — Task 5, сетка — Task 1.

- [ ] **Step 1: Draw, текстуры плиток, двойник у рисования**

В `src/win/crisp/crisp.hpp` заменить:
```cpp
#pragma once
#include <string>

namespace uagfx::win::crisp {
```
на:
```cpp
#pragma once
#include <string>

#include "core/frame.hpp"

namespace uagfx::win::crisp {
```

В `src/win/crisp/crisp.hpp` заменить:
```cpp
// For the 30-second stats line: "" while crisp text is off.
std::string stats();
```
на:
```cpp
// Device7::record: a draw whose stage 0 texture is a tile of a window picture with a twin carries that twin.
void tileDraw(const void* surface, DrawCmd& cmd);
// Surface7's destructor: that surface is no tile any more.
void forgetSurface(const void* surface);
// For the 30-second stats line: "" while crisp text is off.
std::string stats();
```

В `src/win/crisp/crisp.cpp` после функции `void UAGFX_THISCALL onClear(` добавить:
```cpp
using DrawFn = void(UAGFX_THISCALL*)(void*, int, int, int, int, uint32_t);

// The twin textures of a picture's tiles (spec §6): made, or made again when their size changes, and loaded with
// what changed in the twin since the last time - for the chat window, which the game redraws every frame, its edit
// line. Every tile's surface is noted, so its quad - drawn later, from the game's interface list - finds the twin.
void uploadTiles(Picture& pic) {
    State& s = state();
    Context& c = Context::get();
    const int count = field<int>(pic.self, ce::kTileCount);
    const uintptr_t head = field<uintptr_t>(pic.self, ce::kTiles);
    if (!c.renderer || count <= 0 || count > 4096 || !head) return;
    for (size_t i = size_t(count); i < pic.tiles.size(); ++i)
        if (pic.tiles[i].texture) c.renderer->releaseTexture(pic.tiles[i].texture);
    pic.tiles.resize(size_t(count));
    const Rect changed = pic.twin->takeChanged();
    std::vector<uint32_t> texels;
    int i = 0;
    for (uintptr_t node = field<uintptr_t>(head, ce::kNodeNext); node != head && i < count;
         node = field<uintptr_t>(node, ce::kNodeNext), ++i) {
        const uintptr_t texture = field<uintptr_t>(node, ce::kNodeValue);
        if (!texture) continue;
        const void* surface = reinterpret_cast<const void*>(field<uintptr_t>(texture, ce::kTextureSurface));
        if (!surface) continue;
        s.tiles[surface] = TileRef{pic.self, i};
        const TilePlace place = tilePlace(i, pic.twin->width(), pic.twin->height());
        const TwinSpan sx = tileSpan(place.x, place.width, pic.twin->twinWidth(), s.K);
        const TwinSpan sy = tileSpan(place.y, place.height, pic.twin->twinHeight(), s.K);
        const Rect span{sx.first, sy.first, sx.end, sy.end};  // the tile's texture, in twin pixels
        Picture::Tile& t = pic.tiles[size_t(i)];
        const bool fresh = t.width != span.width() || t.height != span.height();
        if (fresh) {  // a texture that cannot be made is not tried again at this size
            if (t.texture) c.renderer->releaseTexture(t.texture);
            t.width = span.width();
            t.height = span.height();
            t.texture = t.width > 0 && t.height > 0 ? c.renderer->createTexture(t.width, t.height, 1) : 0;
        }
        Rect part = span;
        if (!t.texture || (!fresh && !clipRect(changed, part))) continue;  // none, or nothing of it changed
        pic.twin->texels(part, texels);
        const Rect at{part.left - span.left, part.top - span.top, part.right - span.left, part.bottom - span.top};
        c.record.upload(t.texture, 0, at, texels.data());
        c.flushIfFull();
    }
}

// The game shows a window picture (spec §4, §6): first the safety net - what reached its pixels outside the known
// calls goes into the twin enlarged - then the game's own upload and quads, then the twin textures.
void UAGFX_THISCALL onDraw(void* p, int x, int y, int w, int h, uint32_t argb) {
    Guard guard;
    State& s = state();
    Picture* pic = picture(self(p));
    const bool dirty = field<uint8_t>(self(p), ce::kDirty) != 0;
    if (pic && dirty && pic->twin->sync(pic->bits, whole(*pic)) > 0 &&
        s.loggedSizes.insert(uint32_t(pic->twin->width()) << 16 | uint32_t(pic->twin->height())).second)
        LOG_INFO("crisp text: a %dx%d window changed outside the known calls", pic->twin->width(),
                 pic->twin->height());
    original<DrawFn>(ce::kDraw)(p, x, y, w, h, argb);
    if (pic) uploadTiles(*pic);
}
```

В `src/win/crisp/crisp.cpp` заменить:
```cpp
        slot(ce::kClear, reinterpret_cast<uintptr_t>(&onClear)),
```
на:
```cpp
        slot(ce::kClear, reinterpret_cast<uintptr_t>(&onClear)),
        slot(ce::kDraw, reinterpret_cast<uintptr_t>(&onDraw)),
```

В `src/win/crisp/crisp.cpp` после функции `std::string stats() {` добавить:
```cpp
void tileDraw(const void* surface, DrawCmd& cmd) {
    const State& s = state();
    if (!active() || !surface) return;
    const auto t = s.tiles.find(surface);
    if (t == s.tiles.end()) return;
    const auto p = s.pictures.find(t->second.picture);
    if (p == s.pictures.end() || size_t(t->second.index) >= p->second->tiles.size()) return;
    const Picture& pic = *p->second;
    const Picture::Tile& tile = pic.tiles[size_t(t->second.index)];
    if (!tile.texture) return;
    const TilePlace place = tilePlace(t->second.index, pic.twin->width(), pic.twin->height());
    cmd.twin = tile.texture;
    cmd.twinTile[0] = place.x;
    cmd.twinTile[1] = place.y;
    cmd.twinScale = s.K;
}

void forgetSurface(const void* surface) { state().tiles.erase(surface); }
```

В `src/win/dx7/device7.cpp` заменить:
```cpp
#include "win/context.hpp"
#include "win/dx7/caps.hpp"
```
на:
```cpp
#include "win/context.hpp"
#include "win/crisp/crisp.hpp"
#include "win/dx7/caps.hpp"
```

В `src/win/dx7/device7.cpp` заменить:
```cpp
        cmd.keyed[i] = t->colorKeyed();
    }
    DrawSetup setup = resolveDrawSetup(states_, false, hasTexture);  // the target has no alpha
```
на:
```cpp
        cmd.keyed[i] = t->colorKeyed();
    }
    win::crisp::tileDraw(textures_[0], cmd);  // stage 6: a window tile carries its twin
    DrawSetup setup = resolveDrawSetup(states_, false, hasTexture);  // the target has no alpha
```

В `src/win/dx7/surface7.cpp` заменить:
```cpp
#include "win/context.hpp"
#include "win/dx7/formats.hpp"
```
на:
```cpp
#include "win/context.hpp"
#include "win/crisp/crisp.hpp"
#include "win/dx7/formats.hpp"
```

В `src/win/dx7/surface7.cpp` заменить:
```cpp
Surface7::~Surface7() {
    magic_ = 0;
```
на:
```cpp
Surface7::~Surface7() {
    magic_ = 0;
    win::crisp::forgetSurface(this);  // stage 6: a tile's surface is gone
```

- [ ] **Step 2: Сборка, проверки, подмена Draw**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify3.sh`
Expected: `OK: build-win/ddraw.dll: …`, затем `178 tests, 0 failed checks`.

Run: `python3 tools/check_hooks.py build-win/CMakeFiles/ddraw.dir/src/win/crisp/crisp.cpp.obj onDestroy=4 onGetDc=4 onCopyPixels=0x18 onCopyFrom=0x14 onSprite=0x14 onTextureCopy=0x28 onClear=8 onDraw=0x14 onCreateDibSection=0x18`
Expected: девять строк `OK: …`, код выхода 0.

- [ ] **Step 3: Коммит**

```bash
git add src/win/crisp/crisp.hpp src/win/crisp/crisp.cpp src/win/dx7/device7.cpp src/win/dx7/surface7.cpp
git commit -m "Плитки окна из двойника: страховка при Draw, текстуры-двойники плиток, двойник у рисования плитки (этап 6, §4, §6)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Выпуск 0.5.0

Версия 0.5.0, ключ `crisp_text` в настройках (установка и обновление дописывают его в старый файл, как `sharpen` в
0.4.0), чек-лист 0.5.0, пакет.

**Files:**
- Modify: `src/core/version.cpp`, `tests/test_version.cpp`, `dist/uaro-graphics.ini`, `dist/install.bat`,
  `dist/TEST.ru.md`

- [ ] **Step 1: Падающий тест версии**

В `tests/test_version.cpp` заменить:
```cpp
TEST(version_is_0_4_3) { CHECK(std::strcmp(uagfx::kVersion, "0.4.3") == 0); }
```
на:
```cpp
TEST(version_is_0_5_0) { CHECK(std::strcmp(uagfx::kVersion, "0.5.0") == 0); }
```

Run: `cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests version_is | tail -2`
Expected: `FAIL version_is_0_5_0`, `1 tests, 1 failed checks`.

- [ ] **Step 2: Версия, настройки, установка, чек-лист**

В `src/core/version.cpp` заменить:
```cpp
const char* const kVersion = "0.4.3";
```
на:
```cpp
const char* const kVersion = "0.5.0";
```

В `dist/uaro-graphics.ini` заменить:
```ini
; how enlarged interface pictures look: sharp = crisp pixels, smooth = soft
ui_filter = sharp
```
на:
```ini
; how enlarged interface pictures look: sharp = crisp pixels, smooth = soft
ui_filter = sharp
; 1 = the game's text, window frames and fills at the real pixels of the window when the game is enlarged
;     (Windows scaling, or render_scale above 100); 0 = as version 0.4
crisp_text = 1
```

В `dist/install.bat` заменить:
```bat
    >>"uaro-graphics.ini" echo ; in the game, Pause or Scroll Lock switches all improvements off and on, to compare
)
```
на:
```bat
    >>"uaro-graphics.ini" echo ; in the game, Pause or Scroll Lock switches all improvements off and on, to compare
)
rem A settings file older than 0.5 has no crisp_text key: it is added at the end the same way.
findstr /c:"crisp_text" "uaro-graphics.ini" >nul 2>&1
if errorlevel 1 (
    >>"uaro-graphics.ini" echo.
    >>"uaro-graphics.ini" echo [display]
    >>"uaro-graphics.ini" echo ; 1 = the game's text, window frames and fills at the real pixels of the window when the game is enlarged; 0 = as version 0.4
    >>"uaro-graphics.ini" echo crisp_text = 1
)
```

`dist/TEST.ru.md`:
```md
# uaRO Graphics 0.5.0 — проверка

Что нового: чёткий текст. Весь текст игры — чат, окна, кнопки, списки, подсказки, имена над головами — рисуется в
настоящих точках экрана, со сглаживанием, а не растянутым. Рамки и заливки окон — ровными блоками. Окна и раскладка
те же: каждая буква стоит там, где стояла.

Работает, когда игра увеличена: масштаб Windows больше 100 % или `render_scale` больше 100. Pause или Scroll Lock
выключает и включает все улучшения — так видно разницу.

## Установка

Закрой игру и дважды щёлкни `update.bat` в папке клиента.

## Проверка

1. `update.bat` в конце написал `uaRO Graphics 0.5.0 build … is installed`.
2. Запусти игру: в `uaro-graphics.log` есть строки `crisp text: on` и `crisp text: k 1.750, N windows redrawn`.
3. Чат: набери текст по-русски и по-английски, подвигай курсор стрелками, выдели слово, прокрути историю. Буквы
   чёткие, курсор и выделение на своих местах, цвета сообщений (группа, гильдия, шёпот) как раньше.
4. Окна: инвентарь, умения, статус, снаряжение, магазин, диалог NPC, квесты, книга. Текст чёткий, ничего не обрезано
   и не съехало, иконки и рамки на местах.
5. Подсказки к предметам и умениям, имена над головами (свои, чужие, монстров, NPC), облачка чата над головами.
6. Перетащи окно мышью: текст остаётся чётким во время и после перетаскивания.
7. Pause: всё растянуто, как без улучшений; ещё раз Pause — снова чётко, сразу.
8. В `uaro-graphics.ini` поставь `render_scale = 150`, запусти игру: текст чёткий. Верни `render_scale = 100`.
9. fps в строках `… frames in 30.0 s (… fps)` — не ниже, чем в 0.4.3. В конце этих строк — `crisp text: N windows,
   M MB, G letters`.
10. Если в логе есть `crisp text: a WxH window changed outside the known calls` или `crisp text: off (…)` — пришли
    лог.

## Что прислать

Файл `uaro-graphics.log`, пару снимков экрана с текстом (чат, окно, имена) и коротко: что стало лучше, что хуже.

## Если что-то не так

Сделай снимок экрана (Win+Shift+S) и запомни, что делал. Чёткий текст выключается отдельно: в `uaro-graphics.ini`
поставь `crisp_text = 0`. Всё выключить — `use-dgvoodoo.bat`.

## Удаление

Дважды щёлкни `uninstall.bat` — всё вернётся как было.
```

- [ ] **Step 3: Тесты, сборка, пакет**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify3.sh`
Expected: `OK: build-win/ddraw.dll: …`, затем `178 tests, 0 failed checks`.

Run: `tools/package.sh | tail -2 && unzip -p build-win/uaro-graphics.zip uaro-graphics/uaro-graphics.ini | grep -c crisp_text && unzip -p build-win/uaro-graphics.zip install.bat | grep -c crisp_text`
Expected: `OK: build-win/uaro-graphics.zip: …`, `packed build-win/uaro-graphics.zip`, затем `1` и `3`.

- [ ] **Step 4: Коммит**

```bash
git add src/core/version.cpp tests/test_version.cpp dist/uaro-graphics.ini dist/install.bat dist/TEST.ru.md
git commit -m "0.5.0: чёткий текст — ключ crisp_text, установка дописывает его в старые настройки, чек-лист

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Выпуск (`release/uaro-graphics.zip`) — после финального ревью и слияния, как в 0.4.x.

# uaRO Graphics — этап 1B: картинка на Direct3D 12 (план)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `ddraw.dll` 0.2.0 сам рисует игру через Direct3D 12 в масштабе 1 (`s = r = 1`): все вызовы DirectX 7, которые
делает `uaRO.exe`, обслуживает наш код; картинка совпадает с dgVoodoo; окно или полный экран без рамки; VSync;
калибровка клиента даёт (0.5, 0.5); чтение кадра назад — только когда клиент реально трогает память.

**Architecture:** три слоя. (1) Переносимое ядро `src/core`: модель состояний D3D7 и их сведение к «состоянию
рисования», запись кадра (список команд без указателей), роли поверхностей, эталонный растеризатор (тест калибровки).
(2) DX7-объекты `src/win/dx7`: IDirectDraw7, IDirectDrawSurface7, IDirectDrawClipper, IDirect3D7, IDirect3DDevice7 —
поверхности живут на CPU (`Image`), рисование пишется в запись кадра. (3) Рендер `src/win/gpu` на D3D12: исполняет
запись кадра, читает кадр назад, показывает кадр в окне. Ниже вызовов — общий контекст `uagfx::win::Context`
(глобальный замок, рендер, запись кадра, тень RT).

**Tech Stack:** C++17, MinGW-w64 i686 GCC 12 (`d3d12.h`, `dxgi1_6.h`, `d3dcompiler.h`, `ddraw.h`, `d3d.h`),
Direct3D 12 (feature level 11_0, шейдеры SM 5.0, компиляция `d3dcompiler_47.dll` во время работы), Python 3
(генератор заглушек COM), тесты на Linux.

**Spec:** `docs/superpowers/specs/2026-10-13-uaro-graphics-design.md` (§2, §3.2–3.3, §4.1, §4.3–4.5, §4.9, §7–§9).
Справки по клиенту (обязательны к чтению исполнителем): `docs/research/surfaces-present-caps.md`,
`docs/research/states.md`, `docs/research/calibration.md`. Основание — план 1A (ветка `feat/stage1a`).

## Global Constraints

- Всё из плана 1A остаётся в силе: PE32 i386, импорты только `KERNEL32`, `USER32`, `GDI32`, `msvcrt` (D3D12, DXGI,
  `d3dcompiler_47` — только динамически), пролог горячего патча у `DirectDrawCreateEx`, откат при любой ошибке.
- Версия этого плана — `0.2.0`; `kUaroBuilt = true`.
- Масштаб `s = r = 1`: логическое разрешение рисуется 1:1, вывод в окно — растягиванием с сохранением пропорций.
- Центры пикселей: позиция вершины DX7 `x` → `x + 0.5` в пространстве пикселей D3D12
  (`ndc = ((x + 0.5)·2/W − 1, 1 − (y + 0.5)·2/H)`). **Не −0.5:** в `states.md` (Hazard 3) ошибка, правильный знак
  доказан в `calibration.md` §3.
- Пустая текстура (NULL) на стадии 0 читается как белая (1,1,1,1); на стадии ≥1 стадия с NULL-текстурой, чья операция
  берёт TEXTURE, работает как выключенная (пропускает CURRENT).
- Задний буфер клиента без альфы: `DESTALPHA` → `ONE`, `INVDESTALPHA` → `ZERO`; `BOTHSRCALPHA`/`BOTHINVSRCALPHA`
  раскрываются в пару.
- Цветовой ключ: при `COLORKEYENABLE` тексели текстур с ключом (`DDSD_CKSRCBLT`) отбрасываются.
- Туман вершинный: `rgb = lerp(FOGCOLOR.rgb, rgb, specular.a)` при `FOGENABLE` и `FOGTABLEMODE = NONE`.
- Возвраты для клиента: `GetCaps` устройства — ровно `D3D_OK` с полями из `surfaces-present-caps.md` §5; никогда не
  возвращать `DDERR_SURFACELOST`; `Release` DD7 и устройства при выходе — 0 (наши объекты не держат ссылок друг на
  друга сверх клиентских); `Unlock` незаблокированной поверхности — `DDERR_NOTLOCKED` без падения.
- Задний буфер (RT) сохраняет содержимое после показа кадра.
- `DDSCL_FPUSETUP` (0x800) → x87 в одинарную точность, как настоящий DirectX 7; `DDSCL_MULTITHREADED` (0x400) →
  все методы под одним рекурсивным замком.
- Перевод RT → 16-битный формат клиента при чтении назад: из 8 бит `v5 = v8·31/255`, `v6 = v8·63/255` (целочисленное
  деление, «пол»), чтобы калибровка в 16 битах давала (0.5, 0.5).
- Имена в `src/core` не совпадают с макросами Win32 (`DrawState`, `GetObject`, `LoadImage`, `CreateWindow`, …): ядро
  собирается без `windows.h`, а `src/win` — с ним, и такое имя там молча становится `…A` (ошибка видна только при
  линковке `ddraw.dll`).
- Стиль и коммиты — как в 1A (KWEB Team, русские сообщения, трейлер `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`).

## Review Focus

1. Клиент блокирует RT каждый кадр, не трогая память, — чтения с GPU быть не должно (иначе падает FPS). Тесты:
   Task 6 (`shadow_untouched_lock_reads_nothing`); в логе — строка раз в 30 секунд с числом чтений (Task 9),
   её присылает пользователь (чек-лист 0.2.0, Task 13).
2. Калибровка: 65 536 мелких рисований и чтение назад в 32-битном режиме → (0.5, 0.5), в 16-битном — тоже.
   Тесты: Task 4 (`calibration_…`), Task 3 (`target_to_client_floors_into_16_bits`), Task 10
   (`surfops_read_target_…`, в том числе растягивание в поверхность, округлённую до ×4).
3. Выход из игры без MessageBox «still referenced»: последний `Release` DD7 и устройства возвращает 0. На Linux не
   проверить: правила счётчиков — Task 11 (только клиентские ссылки, цепочки делят счётчик верхней поверхности) и
   Task 12 (устройство держит цель и текстуры, его не держит никто); проверка — чек-лист 0.2.0, п. 8.
4. Alt+Tab, сворачивание, перемещение и смена размера окна — без потери картинки и без `SURFACELOST`. Task 7
   (`ensureSwapChain` → `ResizeBuffers`), Task 9 (свёрнутое окно не показывается), Task 11 (`IsLost`/`Restore` —
   `DD_OK`); проверка — чек-лист 0.2.0, п. 4 и 9.
5. Игра с геймпадом uaRO Pad и ROExt одновременно — курсор и окно ведут себя как с dgVoodoo. Проверка: чек-лист
   0.2.0, п. 7.

**Не входит в 1B** (следующие этапы по §9 спецификации): `UIBegin` в записи кадра и масштаб `s ≠ 1` (этап 3),
сохранение кадров `.uagf`, их сериализация и горячие клавиши (§4.9), кеш шейдеров на диске, MSAA, анизотропия,
HDR, чёткий текст.

---

### Task 1: Константы D3D7 и модель состояний устройства

**Files:**
- Create: `src/core/d3d7.hpp`, `src/core/states.hpp`, `src/core/states.cpp`
- Test: `tests/test_states.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace uagfx::d3d7 { /* RS_*, TSS_*, TOP_*, TA_*, BLEND_*, CMP_*, CULL_*, TADDRESS_*, TFG_*, TFN_*, TFP_*,
                             PT_*, CLEAR_*, FOG_*, FVF_TL1 = 0x1C4, FVF_TL2 = 0x2C4, kRenderStateCount = 256,
                             kStageStateCount = 32, kStages = 8 */ }
  namespace uagfx {
  class DeviceStates {
  public:
      DeviceStates();                                    // D3D7 defaults
      void setRender(uint32_t state, uint32_t value);    // out of range: ignored
      uint32_t render(uint32_t state) const;             // out of range: 0
      void setStage(uint32_t stage, uint32_t type, uint32_t value);  // TSS_ADDRESS sets U and V
      uint32_t stage(uint32_t stage, uint32_t type) const;
  };
  struct StageSetup { uint8_t colorOp, colorArg1, colorArg2, alphaOp, alphaArg1, alphaArg2, texCoord,
                      addressU, addressV, magFilter, minFilter, mipFilter; uint32_t borderColor; };
  struct DrawSetup { bool zEnable, zWrite; uint8_t zFunc, cull; bool blend; uint8_t srcBlend, dstBlend;
                     bool alphaTest; uint8_t alphaFunc, alphaRef; bool fog; uint32_t fogColor; bool colorKey;
                     StageSetup stage[2]; bool operator==(const DrawSetup&) const; };
  DrawSetup resolveDrawSetup(const DeviceStates& s, bool targetHasAlpha, const bool stageHasTexture[2]);
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_states.cpp`:
```cpp
#include "core/d3d7.hpp"
#include "core/states.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::d3d7;

namespace {
const bool kBoth[2] = {true, true};
const bool kNone[2] = {false, false};

// What the client sets once at start-up (states.md §1-2, FUN_00403f30).
DeviceStates clientInit() {
    DeviceStates s;
    s.setRender(RS_CULLMODE, CULL_NONE);
    s.setRender(RS_ZFUNC, CMP_LESSEQUAL);
    s.setRender(RS_ALPHAFUNC, CMP_GREATEREQUAL);
    s.setRender(RS_ALPHAREF, 0xF0);
    s.setRender(RS_COLORKEYENABLE, 1);
    s.setRender(RS_ZENABLE, 1);
    s.setRender(RS_ZWRITEENABLE, 1);
    s.setRender(RS_SRCBLEND, BLEND_SRCALPHA);
    s.setRender(RS_DESTBLEND, BLEND_INVSRCALPHA);
    s.setStage(0, TSS_COLOROP, TOP_MODULATE);
    s.setStage(0, TSS_COLORARG1, TA_TEXTURE);
    s.setStage(0, TSS_COLORARG2, TA_DIFFUSE);
    s.setStage(0, TSS_ALPHAOP, TOP_MODULATE);
    s.setStage(0, TSS_ALPHAARG1, TA_TEXTURE);
    s.setStage(0, TSS_ALPHAARG2, TA_DIFFUSE);
    s.setStage(0, TSS_ADDRESS, TADDRESS_CLAMP);
    s.setStage(0, TSS_MAGFILTER, TFG_LINEAR);
    s.setStage(0, TSS_MINFILTER, TFN_LINEAR);
    s.setStage(1, TSS_COLOROP, TOP_MODULATEALPHA_ADDCOLOR);
    s.setStage(1, TSS_COLORARG1, TA_TEXTURE);
    s.setStage(1, TSS_COLORARG2, TA_CURRENT);
    s.setStage(1, TSS_ALPHAOP, TOP_DISABLE);
    s.setStage(1, TSS_ADDRESS, TADDRESS_BORDER);
    s.setStage(1, TSS_BORDERCOLOR, 0xFF000000);
    return s;
}
}  // namespace

TEST(states_d3d7_defaults) {
    const DeviceStates s;
    CHECK_EQ(s.render(RS_ZENABLE), 0u);
    CHECK_EQ(s.render(RS_ZWRITEENABLE), 1u);
    CHECK_EQ(s.render(RS_SRCBLEND), uint32_t(BLEND_ONE));
    CHECK_EQ(s.render(RS_DESTBLEND), uint32_t(BLEND_ZERO));
    CHECK_EQ(s.render(RS_CULLMODE), uint32_t(CULL_CCW));
    CHECK_EQ(s.render(RS_ZFUNC), uint32_t(CMP_LESSEQUAL));
    CHECK_EQ(s.render(RS_ALPHAFUNC), uint32_t(CMP_ALWAYS));
    CHECK_EQ(s.render(RS_FOGENABLE), 0u);
    CHECK_EQ(s.stage(0, TSS_COLOROP), uint32_t(TOP_MODULATE));
    CHECK_EQ(s.stage(0, TSS_ALPHAOP), uint32_t(TOP_SELECTARG1));
    CHECK_EQ(s.stage(1, TSS_COLOROP), uint32_t(TOP_DISABLE));
    CHECK_EQ(s.stage(1, TSS_TEXCOORDINDEX), 1u);
    CHECK_EQ(s.stage(0, TSS_ADDRESSU), uint32_t(TADDRESS_WRAP));
    CHECK_EQ(s.stage(0, TSS_MAGFILTER), uint32_t(TFG_POINT));
    CHECK_EQ(s.stage(0, TSS_MIPFILTER), uint32_t(TFP_NONE));
}

TEST(states_address_sets_both_axes_and_out_of_range_is_ignored) {
    DeviceStates s;
    s.setStage(0, TSS_ADDRESS, TADDRESS_CLAMP);
    CHECK_EQ(s.stage(0, TSS_ADDRESSU), uint32_t(TADDRESS_CLAMP));
    CHECK_EQ(s.stage(0, TSS_ADDRESSV), uint32_t(TADDRESS_CLAMP));
    s.setRender(999, 5);
    CHECK_EQ(s.render(999), 0u);
    s.setStage(9, TSS_COLOROP, TOP_ADD);
    CHECK_EQ(s.stage(9, TSS_COLOROP), 0u);
}

TEST(states_ui_list) {
    DeviceStates s = clientInit();
    s.setRender(RS_ZENABLE, 0);
    s.setRender(RS_ALPHATESTENABLE, 1);
    s.setRender(RS_ALPHAREF, 1);
    s.setRender(RS_ALPHABLENDENABLE, 1);
    s.setStage(1, TSS_COLOROP, TOP_DISABLE);
    const DrawSetup d = resolveDrawSetup(s, false, kBoth);
    CHECK(!d.zEnable);
    CHECK(!d.zWrite);
    CHECK(d.blend);
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_SRCALPHA));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_INVSRCALPHA));
    CHECK(d.alphaTest);
    CHECK_EQ(d.alphaFunc, uint8_t(CMP_GREATEREQUAL));
    CHECK_EQ(d.alphaRef, uint8_t(1));
    CHECK(d.colorKey);
    CHECK_EQ(d.cull, uint8_t(CULL_NONE));
    CHECK_EQ(d.stage[0].colorOp, uint8_t(TOP_MODULATE));
    CHECK_EQ(d.stage[0].addressU, uint8_t(TADDRESS_CLAMP));
    CHECK_EQ(d.stage[1].colorOp, uint8_t(TOP_DISABLE));
}

TEST(states_both_blends_expand_and_dest_alpha_needs_target_alpha) {
    DeviceStates s = clientInit();
    s.setRender(RS_ALPHABLENDENABLE, 1);
    s.setRender(RS_SRCBLEND, BLEND_BOTHSRCALPHA);
    s.setRender(RS_DESTBLEND, BLEND_ZERO);
    DrawSetup d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_SRCALPHA));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_INVSRCALPHA));
    s.setRender(RS_SRCBLEND, BLEND_BOTHINVSRCALPHA);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_INVSRCALPHA));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_SRCALPHA));
    // The STR effects' favourite pair: on a target without alpha DESTALPHA reads 1 -> additive.
    s.setRender(RS_SRCBLEND, BLEND_SRCALPHA);
    s.setRender(RS_DESTBLEND, BLEND_DESTALPHA);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_ONE));
    s.setRender(RS_DESTBLEND, BLEND_INVDESTALPHA);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_ZERO));
    d = resolveDrawSetup(s, true, kBoth);
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_INVDESTALPHA));
}

TEST(states_disabled_parts_are_normalised) {
    DeviceStates a = clientInit();
    a.setRender(RS_ALPHABLENDENABLE, 0);
    a.setRender(RS_SRCBLEND, BLEND_SRCALPHA);
    a.setStage(1, TSS_COLOROP, TOP_DISABLE);
    DeviceStates b = a;
    b.setRender(RS_SRCBLEND, BLEND_ONE);              // ignored: blending is off
    b.setStage(1, TSS_COLORARG1, TA_DIFFUSE);          // ignored: stage 1 is off
    b.setRender(RS_ALPHAREF, 0x55);                    // ignored: alpha test is off
    CHECK(resolveDrawSetup(a, false, kBoth) == resolveDrawSetup(b, false, kBoth));
    DeviceStates c = a;
    c.setStage(0, TSS_COLOROP, TOP_DISABLE);           // stage 0 off turns every later stage off
    c.setStage(1, TSS_COLOROP, TOP_ADD);
    const DrawSetup d = resolveDrawSetup(c, false, kBoth);
    CHECK_EQ(d.stage[0].colorOp, uint8_t(TOP_DISABLE));
    CHECK_EQ(d.stage[1].colorOp, uint8_t(TOP_DISABLE));
}

TEST(states_stage1_without_texture_passes_through) {
    // Calibration leaves stage 1 on MODULATEALPHA_ADDCOLOR(TEXTURE, CURRENT) with no texture bound.
    const DeviceStates s = clientInit();
    const bool onlyStage0[2] = {true, false};
    const DrawSetup d = resolveDrawSetup(s, false, onlyStage0);
    CHECK_EQ(d.stage[1].colorOp, uint8_t(TOP_DISABLE));
    // Stage 0 without a texture keeps its operation: the renderer binds a white texture.
    const DrawSetup e = resolveDrawSetup(s, false, kNone);
    CHECK_EQ(e.stage[0].colorOp, uint8_t(TOP_MODULATE));
}

TEST(states_vertex_fog) {
    DeviceStates s = clientInit();
    s.setRender(RS_FOGENABLE, 1);
    s.setRender(RS_FOGCOLOR, 0xFF6699CC);
    s.setRender(RS_FOGTABLEMODE, FOG_NONE);
    DrawSetup d = resolveDrawSetup(s, false, kBoth);
    CHECK(d.fog);
    CHECK_EQ(d.fogColor, 0xFF6699CCu);
    s.setRender(RS_FOGENABLE, 0);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK(!d.fog);
    CHECK_EQ(d.fogColor, 0u);
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/d3d7.hpp: No such file or directory`.

- [ ] **Step 3: Константы D3D7**

`src/core/d3d7.hpp`:
```cpp
#pragma once
#include <cstdint>

// Direct3D 7 numbers (d3dtypes.h) the client uses. Kept here so the core stays free of Windows headers.
namespace uagfx::d3d7 {

constexpr uint32_t kRenderStateCount = 256;
constexpr uint32_t kStageStateCount = 32;
constexpr uint32_t kStages = 8;

enum : uint32_t {  // D3DRENDERSTATETYPE
    RS_TEXTUREPERSPECTIVE = 4,
    RS_ZENABLE = 7,
    RS_FILLMODE = 8,
    RS_SHADEMODE = 9,
    RS_ZWRITEENABLE = 14,
    RS_ALPHATESTENABLE = 15,
    RS_LASTPIXEL = 16,
    RS_SRCBLEND = 19,
    RS_DESTBLEND = 20,
    RS_CULLMODE = 22,
    RS_ZFUNC = 23,
    RS_ALPHAREF = 24,
    RS_ALPHAFUNC = 25,
    RS_DITHERENABLE = 26,
    RS_ALPHABLENDENABLE = 27,
    RS_FOGENABLE = 28,
    RS_SPECULARENABLE = 29,
    RS_FOGCOLOR = 34,
    RS_FOGTABLEMODE = 35,
    RS_FOGSTART = 36,
    RS_FOGEND = 37,
    RS_FOGDENSITY = 38,
    RS_COLORKEYENABLE = 41,
    RS_RANGEFOGENABLE = 48,
    RS_TEXTUREFACTOR = 60,
    RS_CLIPPING = 136,
    RS_LIGHTING = 137,
    RS_AMBIENT = 139,
    RS_FOGVERTEXMODE = 140,
    RS_COLORVERTEX = 141,
};

enum : uint32_t {  // D3DTEXTURESTAGESTATETYPE
    TSS_COLOROP = 1,
    TSS_COLORARG1 = 2,
    TSS_COLORARG2 = 3,
    TSS_ALPHAOP = 4,
    TSS_ALPHAARG1 = 5,
    TSS_ALPHAARG2 = 6,
    TSS_TEXCOORDINDEX = 11,
    TSS_ADDRESS = 12,
    TSS_ADDRESSU = 13,
    TSS_ADDRESSV = 14,
    TSS_BORDERCOLOR = 15,
    TSS_MAGFILTER = 16,
    TSS_MINFILTER = 17,
    TSS_MIPFILTER = 18,
    TSS_MIPMAPLODBIAS = 19,
    TSS_MAXMIPLEVEL = 20,
    TSS_MAXANISOTROPY = 21,
};

enum : uint32_t {  // D3DTEXTUREOP
    TOP_DISABLE = 1,
    TOP_SELECTARG1 = 2,
    TOP_SELECTARG2 = 3,
    TOP_MODULATE = 4,
    TOP_MODULATE2X = 5,
    TOP_MODULATE4X = 6,
    TOP_ADD = 7,
    TOP_ADDSIGNED = 8,
    TOP_ADDSIGNED2X = 9,
    TOP_SUBTRACT = 10,
    TOP_ADDSMOOTH = 11,
    TOP_BLENDDIFFUSEALPHA = 12,
    TOP_BLENDTEXTUREALPHA = 13,
    TOP_BLENDFACTORALPHA = 14,
    TOP_BLENDTEXTUREALPHAPM = 15,
    TOP_BLENDCURRENTALPHA = 16,
    TOP_PREMODULATE = 17,
    TOP_MODULATEALPHA_ADDCOLOR = 18,
    TOP_MODULATECOLOR_ADDALPHA = 19,
    TOP_MODULATEINVALPHA_ADDCOLOR = 20,
    TOP_MODULATEINVCOLOR_ADDALPHA = 21,
};

enum : uint32_t {  // D3DTA_*
    TA_DIFFUSE = 0,
    TA_CURRENT = 1,
    TA_TEXTURE = 2,
    TA_TFACTOR = 3,
    TA_SPECULAR = 4,
    TA_SELECTMASK = 0x0F,
    TA_COMPLEMENT = 0x10,
    TA_ALPHAREPLICATE = 0x20,
};

enum : uint32_t {  // D3DBLEND
    BLEND_ZERO = 1,
    BLEND_ONE = 2,
    BLEND_SRCCOLOR = 3,
    BLEND_INVSRCCOLOR = 4,
    BLEND_SRCALPHA = 5,
    BLEND_INVSRCALPHA = 6,
    BLEND_DESTALPHA = 7,
    BLEND_INVDESTALPHA = 8,
    BLEND_DESTCOLOR = 9,
    BLEND_INVDESTCOLOR = 10,
    BLEND_SRCALPHASAT = 11,
    BLEND_BOTHSRCALPHA = 12,
    BLEND_BOTHINVSRCALPHA = 13,
};

enum : uint32_t {  // D3DCMPFUNC
    CMP_NEVER = 1,
    CMP_LESS = 2,
    CMP_EQUAL = 3,
    CMP_LESSEQUAL = 4,
    CMP_GREATER = 5,
    CMP_NOTEQUAL = 6,
    CMP_GREATEREQUAL = 7,
    CMP_ALWAYS = 8,
};

enum : uint32_t { CULL_NONE = 1, CULL_CW = 2, CULL_CCW = 3 };
enum : uint32_t { TADDRESS_WRAP = 1, TADDRESS_MIRROR = 2, TADDRESS_CLAMP = 3, TADDRESS_BORDER = 4 };
enum : uint32_t { TFG_POINT = 1, TFG_LINEAR = 2, TFG_ANISOTROPIC = 5 };  // MAGFILTER
enum : uint32_t { TFN_POINT = 1, TFN_LINEAR = 2, TFN_ANISOTROPIC = 3 };  // MINFILTER
enum : uint32_t { TFP_NONE = 1, TFP_POINT = 2, TFP_LINEAR = 3 };         // MIPFILTER
enum : uint32_t { FOG_NONE = 0, FOG_EXP = 1, FOG_EXP2 = 2, FOG_LINEAR = 3 };
enum : uint32_t { PT_POINTLIST = 1, PT_LINELIST = 2, PT_LINESTRIP = 3, PT_TRIANGLELIST = 4, PT_TRIANGLESTRIP = 5,
                  PT_TRIANGLEFAN = 6 };
enum : uint32_t { CLEAR_TARGET = 1, CLEAR_ZBUFFER = 2, CLEAR_STENCIL = 4 };
enum : uint32_t { FVF_TL1 = 0x1C4, FVF_TL2 = 0x2C4 };  // XYZRHW|DIFFUSE|SPECULAR + one or two UV sets

}  // namespace uagfx::d3d7
```

- [ ] **Step 4: Модель состояний**

`src/core/states.hpp`:
```cpp
#pragma once
#include <array>
#include <cstdint>

#include "core/d3d7.hpp"

namespace uagfx {

// The device's render and texture stage states as Direct3D 7 numbers, starting from D3D7's defaults.
class DeviceStates {
public:
    DeviceStates();
    void setRender(uint32_t state, uint32_t value);
    uint32_t render(uint32_t state) const;
    void setStage(uint32_t stage, uint32_t type, uint32_t value);
    uint32_t stage(uint32_t stage, uint32_t type) const;

private:
    std::array<uint32_t, d3d7::kRenderStateCount> rs_{};
    std::array<std::array<uint32_t, d3d7::kStageStateCount>, d3d7::kStages> ts_{};
};

// One texture stage, resolved for the pixel shader and the sampler.
struct StageSetup {
    uint8_t colorOp = d3d7::TOP_DISABLE;
    uint8_t colorArg1 = 0;
    uint8_t colorArg2 = 0;
    uint8_t alphaOp = d3d7::TOP_DISABLE;
    uint8_t alphaArg1 = 0;
    uint8_t alphaArg2 = 0;
    uint8_t texCoord = 0;
    uint8_t addressU = 0;
    uint8_t addressV = 0;
    uint8_t magFilter = 0;
    uint8_t minFilter = 0;
    uint8_t mipFilter = 0;
    uint32_t borderColor = 0;
    bool operator==(const StageSetup& o) const;
};

// What one draw needs from the state: the GPU pipeline (depth, cull, blending) and the pixel shader's inputs.
// Parts that are switched off are normalised, so equal-looking draws compare equal. (Not "DrawState": windows.h
// turns that name into DrawStateA in the files that include it.)
struct DrawSetup {
    bool zEnable = false;
    bool zWrite = false;
    uint8_t zFunc = d3d7::CMP_LESSEQUAL;
    uint8_t cull = d3d7::CULL_NONE;
    bool blend = false;
    uint8_t srcBlend = d3d7::BLEND_ONE;
    uint8_t dstBlend = d3d7::BLEND_ZERO;
    bool alphaTest = false;
    uint8_t alphaFunc = d3d7::CMP_ALWAYS;
    uint8_t alphaRef = 0;
    bool fog = false;
    uint32_t fogColor = 0;
    bool colorKey = false;
    StageSetup stage[2];
    bool operator==(const DrawSetup& o) const;
};

// targetHasAlpha: the client's screen formats have no alpha, so DESTALPHA reads 1 there.
// stageHasTexture: which of stages 0 and 1 have a texture bound.
DrawSetup resolveDrawSetup(const DeviceStates& s, bool targetHasAlpha, const bool stageHasTexture[2]);

}  // namespace uagfx
```

`src/core/states.cpp`:
```cpp
#include "core/states.hpp"

namespace uagfx {

using namespace d3d7;

namespace {

uint8_t noDestAlpha(uint8_t factor) {
    if (factor == BLEND_DESTALPHA) return BLEND_ONE;
    if (factor == BLEND_INVDESTALPHA) return BLEND_ZERO;
    return factor;
}

bool usesTexture(uint32_t op, uint32_t arg1, uint32_t arg2) {
    if (op == TOP_DISABLE) return false;
    const bool a1 = (arg1 & TA_SELECTMASK) == TA_TEXTURE;
    const bool a2 = (arg2 & TA_SELECTMASK) == TA_TEXTURE;
    if (op == TOP_SELECTARG1) return a1;
    if (op == TOP_SELECTARG2) return a2;
    return a1 || a2 || op == TOP_BLENDTEXTUREALPHA || op == TOP_BLENDTEXTUREALPHAPM;
}

StageSetup readStage(const DeviceStates& s, uint32_t i) {
    StageSetup st;
    st.colorOp = uint8_t(s.stage(i, TSS_COLOROP));
    st.colorArg1 = uint8_t(s.stage(i, TSS_COLORARG1));
    st.colorArg2 = uint8_t(s.stage(i, TSS_COLORARG2));
    st.alphaOp = uint8_t(s.stage(i, TSS_ALPHAOP));
    st.alphaArg1 = uint8_t(s.stage(i, TSS_ALPHAARG1));
    st.alphaArg2 = uint8_t(s.stage(i, TSS_ALPHAARG2));
    st.texCoord = uint8_t(s.stage(i, TSS_TEXCOORDINDEX) & 7);
    st.addressU = uint8_t(s.stage(i, TSS_ADDRESSU));
    st.addressV = uint8_t(s.stage(i, TSS_ADDRESSV));
    st.magFilter = uint8_t(s.stage(i, TSS_MAGFILTER));
    st.minFilter = uint8_t(s.stage(i, TSS_MINFILTER));
    st.mipFilter = uint8_t(s.stage(i, TSS_MIPFILTER));
    st.borderColor = s.stage(i, TSS_BORDERCOLOR);
    return st;
}

}  // namespace

DeviceStates::DeviceStates() {
    rs_[RS_TEXTUREPERSPECTIVE] = 1;
    rs_[RS_ZENABLE] = 0;
    rs_[RS_FILLMODE] = 3;   // D3DFILL_SOLID
    rs_[RS_SHADEMODE] = 2;  // D3DSHADE_GOURAUD
    rs_[RS_ZWRITEENABLE] = 1;
    rs_[RS_LASTPIXEL] = 1;
    rs_[RS_SRCBLEND] = BLEND_ONE;
    rs_[RS_DESTBLEND] = BLEND_ZERO;
    rs_[RS_CULLMODE] = CULL_CCW;
    rs_[RS_ZFUNC] = CMP_LESSEQUAL;
    rs_[RS_ALPHAFUNC] = CMP_ALWAYS;
    rs_[RS_FOGEND] = 0x3F800000;      // 1.0f
    rs_[RS_FOGDENSITY] = 0x3F800000;  // 1.0f
    rs_[RS_TEXTUREFACTOR] = 0xFFFFFFFF;
    rs_[RS_CLIPPING] = 1;
    rs_[RS_LIGHTING] = 1;
    rs_[RS_COLORVERTEX] = 1;
    for (uint32_t i = 0; i < kStages; ++i) {
        auto& t = ts_[i];
        t[TSS_COLOROP] = i == 0 ? TOP_MODULATE : TOP_DISABLE;
        t[TSS_COLORARG1] = TA_TEXTURE;
        t[TSS_COLORARG2] = TA_CURRENT;
        t[TSS_ALPHAOP] = i == 0 ? TOP_SELECTARG1 : TOP_DISABLE;
        t[TSS_ALPHAARG1] = TA_TEXTURE;
        t[TSS_ALPHAARG2] = TA_CURRENT;
        t[TSS_TEXCOORDINDEX] = i;
        t[TSS_ADDRESS] = TADDRESS_WRAP;
        t[TSS_ADDRESSU] = TADDRESS_WRAP;
        t[TSS_ADDRESSV] = TADDRESS_WRAP;
        t[TSS_MAGFILTER] = TFG_POINT;
        t[TSS_MINFILTER] = TFN_POINT;
        t[TSS_MIPFILTER] = TFP_NONE;
        t[TSS_MAXANISOTROPY] = 1;
    }
}

void DeviceStates::setRender(uint32_t state, uint32_t value) {
    if (state < kRenderStateCount) rs_[state] = value;
}

uint32_t DeviceStates::render(uint32_t state) const { return state < kRenderStateCount ? rs_[state] : 0; }

void DeviceStates::setStage(uint32_t stage, uint32_t type, uint32_t value) {
    if (stage >= kStages || type >= kStageStateCount) return;
    ts_[stage][type] = value;
    if (type == TSS_ADDRESS) {
        ts_[stage][TSS_ADDRESSU] = value;
        ts_[stage][TSS_ADDRESSV] = value;
    }
}

uint32_t DeviceStates::stage(uint32_t stage, uint32_t type) const {
    return stage < kStages && type < kStageStateCount ? ts_[stage][type] : 0;
}

bool StageSetup::operator==(const StageSetup& o) const {
    return colorOp == o.colorOp && colorArg1 == o.colorArg1 && colorArg2 == o.colorArg2 && alphaOp == o.alphaOp &&
           alphaArg1 == o.alphaArg1 && alphaArg2 == o.alphaArg2 && texCoord == o.texCoord && addressU == o.addressU &&
           addressV == o.addressV && magFilter == o.magFilter && minFilter == o.minFilter && mipFilter == o.mipFilter &&
           borderColor == o.borderColor;
}

bool DrawSetup::operator==(const DrawSetup& o) const {
    return zEnable == o.zEnable && zWrite == o.zWrite && zFunc == o.zFunc && cull == o.cull && blend == o.blend &&
           srcBlend == o.srcBlend && dstBlend == o.dstBlend && alphaTest == o.alphaTest && alphaFunc == o.alphaFunc &&
           alphaRef == o.alphaRef && fog == o.fog && fogColor == o.fogColor && colorKey == o.colorKey &&
           stage[0] == o.stage[0] && stage[1] == o.stage[1];
}

DrawSetup resolveDrawSetup(const DeviceStates& s, bool targetHasAlpha, const bool stageHasTexture[2]) {
    DrawSetup d;
    d.zEnable = s.render(RS_ZENABLE) != 0;
    d.zWrite = d.zEnable && s.render(RS_ZWRITEENABLE) != 0;
    d.zFunc = d.zEnable ? uint8_t(s.render(RS_ZFUNC)) : uint8_t(CMP_ALWAYS);
    d.cull = uint8_t(s.render(RS_CULLMODE));

    d.blend = s.render(RS_ALPHABLENDENABLE) != 0;
    if (d.blend) {
        uint8_t src = uint8_t(s.render(RS_SRCBLEND));
        uint8_t dst = uint8_t(s.render(RS_DESTBLEND));
        if (src == BLEND_BOTHSRCALPHA) {
            src = BLEND_SRCALPHA;
            dst = BLEND_INVSRCALPHA;
        } else if (src == BLEND_BOTHINVSRCALPHA) {
            src = BLEND_INVSRCALPHA;
            dst = BLEND_SRCALPHA;
        }
        if (!targetHasAlpha) {
            src = noDestAlpha(src);
            dst = noDestAlpha(dst);
        }
        d.srcBlend = src;
        d.dstBlend = dst;
    }

    d.alphaTest = s.render(RS_ALPHATESTENABLE) != 0;
    if (d.alphaTest) {
        d.alphaFunc = uint8_t(s.render(RS_ALPHAFUNC));
        d.alphaRef = uint8_t(s.render(RS_ALPHAREF) & 0xFF);
    }

    d.fog = s.render(RS_FOGENABLE) != 0 && s.render(RS_FOGTABLEMODE) == FOG_NONE;
    if (d.fog) d.fogColor = s.render(RS_FOGCOLOR);
    d.colorKey = s.render(RS_COLORKEYENABLE) != 0;

    for (uint32_t i = 0; i < 2; ++i) {
        StageSetup st = readStage(s, i);
        bool off = st.colorOp == TOP_DISABLE || (i > 0 && d.stage[i - 1].colorOp == TOP_DISABLE);
        // A later stage with nothing bound that wants a texture passes CURRENT through (what Wine does too);
        // stage 0 gets a white texture from the renderer instead.
        if (!off && i > 0 && !stageHasTexture[i] &&
            (usesTexture(st.colorOp, st.colorArg1, st.colorArg2) || usesTexture(st.alphaOp, st.alphaArg1, st.alphaArg2)))
            off = true;
        d.stage[i] = off ? StageSetup{} : st;
    }
    return d;
}

}  // namespace uagfx
```

- [ ] **Step 5: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests states`
Expected: 7 тестов `PASS states_…`, `7 tests, 0 failed checks`.

- [ ] **Step 6: Commit**

```bash
git add src/core/d3d7.hpp src/core/states.hpp src/core/states.cpp tests/test_states.cpp
git commit -m "Модель состояний D3D7: умолчания, сведение к состоянию рисования, нормализация выключенного

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Запись кадра

**Files:**
- Create: `src/core/frame.hpp`, `src/core/frame.cpp`
- Test: `tests/test_frame.cpp`

**Interfaces:**
- Consumes: `DrawSetup` (Task 1), `Rect` (1A Task 8).
- Produces:
  ```cpp
  namespace uagfx {
  enum class Cmd : uint8_t { Clear, Viewport, Draw, Upload, WriteTarget };
  struct ClearCmd { uint32_t flags; uint32_t argb; float z; Rect rect; };
  struct ViewportCmd { Rect rect; float minZ, maxZ; };
  struct DrawCmd { uint32_t state; uint32_t texture[2]; bool keyed[2]; uint32_t primitive; uint32_t fvf;
                   uint32_t vertexOffset; uint32_t vertexCount; uint32_t firstIndex; uint32_t indexCount; };
  struct UploadCmd { uint32_t texture; uint32_t level; Rect rect; uint32_t pixels; };
  struct WriteTargetCmd { Rect rect; uint32_t pixels; };
  struct Command { Cmd type; uint32_t index; };
  class FrameRecord {
  public:
      static uint32_t stride(uint32_t fvf);  // 32 for FVF_TL1, 40 for FVF_TL2
      uint32_t addState(const DrawSetup& s);
      void clear(const ClearCmd& c);
      void viewport(const ViewportCmd& v);
      void draw(DrawCmd cmd, const void* vertices, uint32_t vertexCount, const uint16_t* indices, uint32_t indexCount);
      void upload(uint32_t texture, uint32_t level, const Rect& rect, const uint32_t* argb);
      void writeTarget(const Rect& rect, const uint32_t* argb);
      bool empty() const;
      void reset();
      const std::vector<Command>& commands() const; const std::vector<ClearCmd>& clears() const;
      const std::vector<ViewportCmd>& viewports() const; const std::vector<DrawCmd>& draws() const;
      const std::vector<UploadCmd>& uploads() const; const std::vector<WriteTargetCmd>& writes() const;
      const std::vector<DrawSetup>& states() const; const std::vector<uint8_t>& vertexBytes() const;
      const std::vector<uint16_t>& indices() const; const std::vector<uint32_t>& pixels() const;
  };
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_frame.cpp`:
```cpp
#include <cstring>

#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(frame_strides) {
    CHECK_EQ(FrameRecord::stride(d3d7::FVF_TL1), 32u);
    CHECK_EQ(FrameRecord::stride(d3d7::FVF_TL2), 40u);
}

TEST(frame_states_are_deduplicated_against_the_last_one) {
    FrameRecord f;
    DrawSetup a;
    DrawSetup b;
    b.blend = true;
    CHECK_EQ(f.addState(a), 0u);
    CHECK_EQ(f.addState(a), 0u);
    CHECK_EQ(f.addState(b), 1u);
    CHECK_EQ(f.addState(a), 2u);  // only the last one is compared: cheap and enough for runs of equal draws
    CHECK_EQ(f.states().size(), size_t(3));
}

TEST(frame_draw_copies_vertices_and_indices) {
    FrameRecord f;
    uint8_t v1[2 * 32];
    uint8_t v2[3 * 40];
    for (size_t i = 0; i < sizeof v1; ++i) v1[i] = uint8_t(i);
    for (size_t i = 0; i < sizeof v2; ++i) v2[i] = uint8_t(200 + i % 50);
    const uint16_t idx[3] = {0, 1, 2};
    DrawCmd d;
    d.state = f.addState(DrawSetup{});
    d.primitive = d3d7::PT_TRIANGLESTRIP;
    d.fvf = d3d7::FVF_TL1;
    f.draw(d, v1, 2, nullptr, 0);
    d.fvf = d3d7::FVF_TL2;
    d.primitive = d3d7::PT_TRIANGLELIST;
    f.draw(d, v2, 3, idx, 3);
    CHECK_EQ(f.draws().size(), size_t(2));
    CHECK_EQ(f.draws()[0].vertexOffset, 0u);
    CHECK_EQ(f.draws()[0].vertexCount, 2u);
    CHECK_EQ(f.draws()[0].indexCount, 0u);
    CHECK_EQ(f.draws()[1].vertexOffset, 64u);
    CHECK_EQ(f.draws()[1].firstIndex, 0u);
    CHECK_EQ(f.draws()[1].indexCount, 3u);
    CHECK_EQ(f.vertexBytes().size(), size_t(64 + 120));
    CHECK(std::memcmp(f.vertexBytes().data(), v1, 64) == 0);
    CHECK(std::memcmp(f.vertexBytes().data() + 64, v2, 120) == 0);
    CHECK_EQ(f.indices().size(), size_t(3));
}

TEST(frame_keeps_command_order) {
    FrameRecord f;
    f.clear(ClearCmd{d3d7::CLEAR_TARGET, 0xFF000000, 1.0f, Rect{0, 0, 4, 4}});
    const uint32_t px[4] = {1, 2, 3, 4};
    f.upload(7, 0, Rect{0, 0, 2, 2}, px);
    DrawCmd d;
    d.state = f.addState(DrawSetup{});
    d.fvf = d3d7::FVF_TL1;
    uint8_t v[32] = {};
    f.draw(d, v, 1, nullptr, 0);
    f.writeTarget(Rect{1, 1, 3, 3}, px);
    f.viewport(ViewportCmd{Rect{0, 0, 640, 480}, 0.0f, 1.0f});
    const auto& c = f.commands();
    CHECK_EQ(c.size(), size_t(5));
    CHECK(c[0].type == Cmd::Clear);
    CHECK(c[1].type == Cmd::Upload);
    CHECK(c[2].type == Cmd::Draw);
    CHECK(c[3].type == Cmd::WriteTarget);
    CHECK(c[4].type == Cmd::Viewport);
    CHECK_EQ(f.uploads()[0].texture, 7u);
    CHECK_EQ(f.pixels().size(), size_t(8));
    CHECK_EQ(f.writes()[0].pixels, 4u);
    CHECK_EQ(f.pixels()[f.writes()[0].pixels + 3], 4u);
}

TEST(frame_reset_empties_everything) {
    FrameRecord f;
    f.clear(ClearCmd{d3d7::CLEAR_TARGET, 0, 1.0f, Rect{0, 0, 1, 1}});
    CHECK(!f.empty());
    f.reset();
    CHECK(f.empty());
    CHECK(f.commands().empty());
    CHECK(f.states().empty());
    CHECK(f.pixels().empty());
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/frame.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/frame.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <vector>

#include "core/image.hpp"
#include "core/states.hpp"

namespace uagfx {

// Everything the client drew between two flushes, in order, as plain data: no pointers, so a record can be
// executed here, written to a capture file, or (later) sent to a 64-bit renderer process.
enum class Cmd : uint8_t { Clear, Viewport, Draw, Upload, WriteTarget };

struct ClearCmd {
    uint32_t flags = 0;  // d3d7::CLEAR_TARGET | CLEAR_ZBUFFER
    uint32_t argb = 0;
    float z = 1.0f;
    Rect rect;
};

struct ViewportCmd {
    Rect rect;
    float minZ = 0.0f;
    float maxZ = 1.0f;
};

struct DrawCmd {
    uint32_t state = 0;            // index into states()
    uint32_t texture[2] = {0, 0};  // renderer texture ids; 0 = nothing bound
    bool keyed[2] = {false, false};  // the texture has a colour key baked into alpha 0
    uint32_t primitive = 0;        // d3d7::PT_*
    uint32_t fvf = 0;              // d3d7::FVF_TL1 or FVF_TL2
    uint32_t vertexOffset = 0;     // bytes into vertexBytes()
    uint32_t vertexCount = 0;
    uint32_t firstIndex = 0;       // into indices()
    uint32_t indexCount = 0;       // 0 = not indexed
};

struct UploadCmd {  // texture pixels, ARGB8, rect.width() * rect.height() values at pixels()[pixels]
    uint32_t texture = 0;
    uint32_t level = 0;
    Rect rect;
    uint32_t pixels = 0;
};

struct WriteTargetCmd {  // CPU pixels written into the render target (Unlock after a write, Blt from memory)
    Rect rect;
    uint32_t pixels = 0;
};

struct Command {
    Cmd type;
    uint32_t index;  // into the vector of that type
};

class FrameRecord {
public:
    static uint32_t stride(uint32_t fvf);

    uint32_t addState(const DrawSetup& s);
    void clear(const ClearCmd& c);
    void viewport(const ViewportCmd& v);
    void draw(DrawCmd cmd, const void* vertices, uint32_t vertexCount, const uint16_t* indices, uint32_t indexCount);
    void upload(uint32_t texture, uint32_t level, const Rect& rect, const uint32_t* argb);
    void writeTarget(const Rect& rect, const uint32_t* argb);
    bool empty() const { return commands_.empty(); }
    void reset();

    const std::vector<Command>& commands() const { return commands_; }
    const std::vector<ClearCmd>& clears() const { return clears_; }
    const std::vector<ViewportCmd>& viewports() const { return viewports_; }
    const std::vector<DrawCmd>& draws() const { return draws_; }
    const std::vector<UploadCmd>& uploads() const { return uploads_; }
    const std::vector<WriteTargetCmd>& writes() const { return writes_; }
    const std::vector<DrawSetup>& states() const { return states_; }
    const std::vector<uint8_t>& vertexBytes() const { return vertices_; }
    const std::vector<uint16_t>& indices() const { return indices_; }
    const std::vector<uint32_t>& pixels() const { return pixels_; }

private:
    uint32_t addPixels(const uint32_t* argb, size_t count);

    std::vector<Command> commands_;
    std::vector<ClearCmd> clears_;
    std::vector<ViewportCmd> viewports_;
    std::vector<DrawCmd> draws_;
    std::vector<UploadCmd> uploads_;
    std::vector<WriteTargetCmd> writes_;
    std::vector<DrawSetup> states_;
    std::vector<uint8_t> vertices_;
    std::vector<uint16_t> indices_;
    std::vector<uint32_t> pixels_;
};

}  // namespace uagfx
```

`src/core/frame.cpp`:
```cpp
#include "core/frame.hpp"

#include <cstring>

#include "core/d3d7.hpp"

namespace uagfx {

uint32_t FrameRecord::stride(uint32_t fvf) { return fvf == d3d7::FVF_TL2 ? 40u : 32u; }

uint32_t FrameRecord::addState(const DrawSetup& s) {
    if (!states_.empty() && states_.back() == s) return uint32_t(states_.size() - 1);
    states_.push_back(s);
    return uint32_t(states_.size() - 1);
}

void FrameRecord::clear(const ClearCmd& c) {
    commands_.push_back({Cmd::Clear, uint32_t(clears_.size())});
    clears_.push_back(c);
}

void FrameRecord::viewport(const ViewportCmd& v) {
    commands_.push_back({Cmd::Viewport, uint32_t(viewports_.size())});
    viewports_.push_back(v);
}

void FrameRecord::draw(DrawCmd cmd, const void* vertices, uint32_t vertexCount, const uint16_t* indices,
                       uint32_t indexCount) {
    const uint32_t bytes = vertexCount * stride(cmd.fvf);
    cmd.vertexOffset = uint32_t(vertices_.size());
    cmd.vertexCount = vertexCount;
    vertices_.resize(vertices_.size() + bytes);
    std::memcpy(vertices_.data() + cmd.vertexOffset, vertices, bytes);
    cmd.firstIndex = uint32_t(indices_.size());
    cmd.indexCount = indices ? indexCount : 0;
    if (indices) indices_.insert(indices_.end(), indices, indices + indexCount);
    commands_.push_back({Cmd::Draw, uint32_t(draws_.size())});
    draws_.push_back(cmd);
}

uint32_t FrameRecord::addPixels(const uint32_t* argb, size_t count) {
    const uint32_t at = uint32_t(pixels_.size());
    pixels_.insert(pixels_.end(), argb, argb + count);
    return at;
}

void FrameRecord::upload(uint32_t texture, uint32_t level, const Rect& rect, const uint32_t* argb) {
    UploadCmd u;
    u.texture = texture;
    u.level = level;
    u.rect = rect;
    u.pixels = addPixels(argb, size_t(rect.width()) * size_t(rect.height()));
    commands_.push_back({Cmd::Upload, uint32_t(uploads_.size())});
    uploads_.push_back(u);
}

void FrameRecord::writeTarget(const Rect& rect, const uint32_t* argb) {
    WriteTargetCmd w;
    w.rect = rect;
    w.pixels = addPixels(argb, size_t(rect.width()) * size_t(rect.height()));
    commands_.push_back({Cmd::WriteTarget, uint32_t(writes_.size())});
    writes_.push_back(w);
}

void FrameRecord::reset() {
    commands_.clear();
    clears_.clear();
    viewports_.clear();
    draws_.clear();
    uploads_.clear();
    writes_.clear();
    states_.clear();
    vertices_.clear();
    indices_.clear();
    pixels_.clear();
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests frame`
Expected: 5 тестов `PASS frame_…`, `5 tests, 0 failed checks`.

- [ ] **Step 5: Commit**

```bash
git add src/core/frame.hpp src/core/frame.cpp tests/test_frame.cpp
git commit -m "Запись кадра: команды без указателей, вершины, индексы, загрузки текстур и записи в RT

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Роли поверхностей из `CreateSurface` и перевод RT в формат клиента

**Files:**
- Create: `src/core/surface_plan.hpp`, `src/core/surface_plan.cpp`
- Modify: `src/core/pixel.hpp`, `src/core/pixel.cpp` (функция `targetToClient`)
- Test: `tests/test_surface_plan.cpp`

**Interfaces:**
- Consumes: `PixelFormat`, `fromArgb8` (1A Task 7).
- Produces:
  ```cpp
  namespace uagfx {
  namespace ddcaps { constexpr uint32_t BACKBUFFER = 0x4, COMPLEX = 0x8, FLIP = 0x10, OFFSCREENPLAIN = 0x40,
      PRIMARYSURFACE = 0x200, SYSTEMMEMORY = 0x800, TEXTURE = 0x1000, D3DDEVICE = 0x2000, VIDEOMEMORY = 0x4000,
      ZBUFFER = 0x20000, MIPMAP = 0x400000; constexpr uint32_t CAPS2_TEXTUREMANAGE = 0x10; }
  namespace ddsd { constexpr uint32_t CAPS = 0x1, HEIGHT = 0x2, WIDTH = 0x4, PITCH = 0x8, BACKBUFFERCOUNT = 0x20,
      LPSURFACE = 0x800, PIXELFORMAT = 0x1000, CKSRCBLT = 0x10000, MIPMAPCOUNT = 0x20000, TEXTURESTAGE = 0x100000; }
  enum class SurfaceRole { Primary, RenderTarget, Offscreen, ZBuffer, Texture, Unsupported };
  struct SurfaceRequest { uint32_t flags = 0, caps = 0, caps2 = 0; int width = 0, height = 0;
      uint32_t backBuffers = 0, mipLevels = 0; PixelFormat format = PixelFormat::Unknown;
      uint32_t zBits = 0; uint32_t keyLow = 0, keyHigh = 0; };
  struct SurfacePlan { SurfaceRole role = SurfaceRole::Unsupported; int width = 0, height = 0;
      PixelFormat format = PixelFormat::Unknown; int mipLevels = 1; bool backBuffer = false; bool colorKey = false;
      uint32_t keyLow = 0, keyHigh = 0; std::string error; };
  SurfacePlan planSurface(const SurfaceRequest& r, int screenWidth, int screenHeight, PixelFormat screenFormat);
  uint32_t targetToClient(PixelFormat f, uint32_t argb);  // pixel.hpp
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_surface_plan.cpp`:
```cpp
#include "core/pixel.hpp"
#include "core/surface_plan.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
SurfaceRequest req(uint32_t flags, uint32_t caps, int w = 0, int h = 0) {
    SurfaceRequest r;
    r.flags = flags;
    r.caps = caps;
    r.width = w;
    r.height = h;
    return r;
}
constexpr PixelFormat kScreen = PixelFormat::X8R8G8B8;
}  // namespace

// The rows of surfaces-present-caps.md §1.
TEST(plan_windowed_primary) {
    const SurfacePlan p = planSurface(req(ddsd::CAPS, ddcaps::PRIMARYSURFACE), 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::Primary);
    CHECK_EQ(p.width, 1024);
    CHECK_EQ(p.height, 768);
    CHECK(p.format == kScreen);
    CHECK(!p.backBuffer);
}

TEST(plan_fullscreen_flip_chain) {
    SurfaceRequest r = req(ddsd::CAPS | ddsd::BACKBUFFERCOUNT,
                           ddcaps::PRIMARYSURFACE | ddcaps::D3DDEVICE | ddcaps::FLIP | ddcaps::COMPLEX);
    r.backBuffers = 1;
    const SurfacePlan p = planSurface(r, 800, 600, PixelFormat::R5G6B5);
    CHECK(p.role == SurfaceRole::Primary);
    CHECK(p.backBuffer);
    CHECK(p.format == PixelFormat::R5G6B5);
}

TEST(plan_windowed_back_buffer_is_the_render_target) {
    const SurfacePlan p = planSurface(req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT,
                                          ddcaps::D3DDEVICE | ddcaps::OFFSCREENPLAIN, 1600, 863),
                                      1920, 1080, kScreen);
    CHECK(p.role == SurfaceRole::RenderTarget);
    CHECK_EQ(p.width, 1600);
    CHECK_EQ(p.height, 863);
    CHECK(p.format == kScreen);
}

TEST(plan_system_memory_copy_and_keyed_offscreen) {
    const SurfacePlan sys = planSurface(req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT,
                                            ddcaps::OFFSCREENPLAIN | ddcaps::SYSTEMMEMORY, 1024, 768),
                                        1024, 768, kScreen);
    CHECK(sys.role == SurfaceRole::Offscreen);
    CHECK(sys.format == kScreen);
    SurfaceRequest keyed = req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT | ddsd::CKSRCBLT, ddcaps::OFFSCREENPLAIN, 1600, 864);
    keyed.keyLow = keyed.keyHigh = 0xF81F;
    const SurfacePlan k = planSurface(keyed, 1024, 768, PixelFormat::R5G6B5);
    CHECK(k.role == SurfaceRole::Offscreen);
    CHECK(k.colorKey);
    CHECK_EQ(k.keyLow, 0xF81Fu);
    CHECK(k.format == PixelFormat::R5G6B5);  // no DDSD_PIXELFORMAT: the screen's format
}

TEST(plan_z_buffer) {
    SurfaceRequest r = req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT | ddsd::PIXELFORMAT,
                           ddcaps::ZBUFFER | ddcaps::VIDEOMEMORY, 1024, 768);
    r.zBits = 16;
    const SurfacePlan p = planSurface(r, 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::ZBuffer);
    CHECK_EQ(p.width, 1024);
}

TEST(plan_textures_with_and_without_mipmaps) {
    SurfaceRequest r = req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT | ddsd::PIXELFORMAT | ddsd::TEXTURESTAGE,
                           ddcaps::TEXTURE, 256, 128);
    r.caps2 = ddcaps::CAPS2_TEXTUREMANAGE;
    r.format = PixelFormat::A1R5G5B5;
    SurfacePlan p = planSurface(r, 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::Texture);
    CHECK(p.format == PixelFormat::A1R5G5B5);
    CHECK_EQ(p.mipLevels, 1);
    r.flags |= ddsd::MIPMAPCOUNT;
    r.caps |= ddcaps::MIPMAP | ddcaps::COMPLEX;
    r.mipLevels = 7;
    p = planSurface(r, 1024, 768, kScreen);
    CHECK_EQ(p.mipLevels, 7);
    r.mipLevels = 20;  // more than the size allows: clamp to log2(min side) + 1
    p = planSurface(r, 1024, 768, kScreen);
    CHECK_EQ(p.mipLevels, 8);
}

TEST(plan_rejects_what_the_client_never_asks_for) {
    SurfacePlan p = planSurface(req(ddsd::CAPS | ddsd::WIDTH | ddsd::HEIGHT, ddcaps::TEXTURE, 64, 64), 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::Unsupported);  // a texture without a pixel format
    CHECK(!p.error.empty());
    p = planSurface(req(ddsd::CAPS, ddcaps::OFFSCREENPLAIN), 1024, 768, kScreen);
    CHECK(p.role == SurfaceRole::Unsupported);  // no size
}

TEST(target_to_client_floors_into_16_bits) {
    CHECK_EQ(targetToClient(PixelFormat::R5G6B5, 0xFFFFFFFF), 0xFFFFu);
    CHECK_EQ(targetToClient(PixelFormat::R5G6B5, 0xFF000000), 0x0000u);
    // 253 is a bilinear 0.9921875 rounded to 8 bits: floor(253 * 31 / 255) = 30, not 31 (calibration.md §5).
    CHECK_EQ(targetToClient(PixelFormat::R5G6B5, 0xFFFD0000), uint32_t(30u << 11));
    CHECK_EQ(targetToClient(PixelFormat::R5G6B5, 0xFF00FD00), uint32_t(62u << 5));
    CHECK_EQ(targetToClient(PixelFormat::X1R5G5B5, 0xFFFFFFFF), 0x7FFFu);
    CHECK_EQ(targetToClient(PixelFormat::X8R8G8B8, 0xFF123456), 0x123456u);
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/surface_plan.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

В `src/core/pixel.hpp` после `fromArgb8` добавить:
```cpp
// A render-target pixel (ARGB8 from the GPU) -> the client's screen format when the client reads the target
// back. 16-bit channels are floored from the 8-bit values (v5 = v8 * 31 / 255), so the client's start-up
// calibration settles on (0.5, 0.5) in 16-bit modes too (docs/research/calibration.md §5).
uint32_t targetToClient(PixelFormat f, uint32_t argb);
```

В `src/core/pixel.cpp` перед закрывающей `}  // namespace uagfx` добавить:
```cpp
uint32_t targetToClient(PixelFormat f, uint32_t argb) {
    const uint32_t r = (argb >> 16) & 255;
    const uint32_t g = (argb >> 8) & 255;
    const uint32_t b = argb & 255;
    switch (f) {
        case PixelFormat::R5G6B5: return (r * 31 / 255) << 11 | (g * 63 / 255) << 5 | (b * 31 / 255);
        case PixelFormat::X1R5G5B5: return (r * 31 / 255) << 10 | (g * 31 / 255) << 5 | (b * 31 / 255);
        default: return fromArgb8(f, argb);
    }
}
```

`src/core/surface_plan.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <string>

#include "core/pixel.hpp"

namespace uagfx {

// DDSCAPS / DDSD bits the client uses (ddraw.h values).
namespace ddcaps {
constexpr uint32_t BACKBUFFER = 0x4;
constexpr uint32_t COMPLEX = 0x8;
constexpr uint32_t FLIP = 0x10;
constexpr uint32_t OFFSCREENPLAIN = 0x40;
constexpr uint32_t PRIMARYSURFACE = 0x200;
constexpr uint32_t SYSTEMMEMORY = 0x800;
constexpr uint32_t TEXTURE = 0x1000;
constexpr uint32_t D3DDEVICE = 0x2000;
constexpr uint32_t VIDEOMEMORY = 0x4000;
constexpr uint32_t ZBUFFER = 0x20000;
constexpr uint32_t MIPMAP = 0x400000;
constexpr uint32_t CAPS2_TEXTUREMANAGE = 0x10;
}  // namespace ddcaps

namespace ddsd {
constexpr uint32_t CAPS = 0x1;
constexpr uint32_t HEIGHT = 0x2;
constexpr uint32_t WIDTH = 0x4;
constexpr uint32_t PITCH = 0x8;
constexpr uint32_t BACKBUFFERCOUNT = 0x20;
constexpr uint32_t LPSURFACE = 0x800;
constexpr uint32_t PIXELFORMAT = 0x1000;
constexpr uint32_t CKSRCBLT = 0x10000;
constexpr uint32_t MIPMAPCOUNT = 0x20000;
constexpr uint32_t TEXTURESTAGE = 0x100000;
}  // namespace ddsd

// What a CreateSurface call is for. Primary: what the window shows (Flip / Blt target), with the back buffer
// attached in full screen. RenderTarget: the client draws into it (lives on the GPU). Offscreen: CPU memory
// (the screen copy, the calibration readback, big pictures). ZBuffer: depth for the render target.
enum class SurfaceRole { Primary, RenderTarget, Offscreen, ZBuffer, Texture, Unsupported };

struct SurfaceRequest {
    uint32_t flags = 0;  // DDSURFACEDESC2::dwFlags
    uint32_t caps = 0;
    uint32_t caps2 = 0;
    int width = 0;
    int height = 0;
    uint32_t backBuffers = 0;
    uint32_t mipLevels = 0;
    PixelFormat format = PixelFormat::Unknown;  // from ddpfPixelFormat when DDSD_PIXELFORMAT is set
    uint32_t zBits = 0;
    uint32_t keyLow = 0;  // ddckCKSrcBlt when DDSD_CKSRCBLT is set
    uint32_t keyHigh = 0;
};

struct SurfacePlan {
    SurfaceRole role = SurfaceRole::Unsupported;
    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::Unknown;
    int mipLevels = 1;
    bool backBuffer = false;  // Primary: create the attached back buffer (render target)
    bool colorKey = false;
    uint32_t keyLow = 0;
    uint32_t keyHigh = 0;
    std::string error;  // why the request is Unsupported
};

SurfacePlan planSurface(const SurfaceRequest& r, int screenWidth, int screenHeight, PixelFormat screenFormat);

}  // namespace uagfx
```

`src/core/surface_plan.cpp`:
```cpp
#include "core/surface_plan.hpp"

#include <algorithm>

namespace uagfx {
namespace {

int maxLevels(int w, int h) {
    int levels = 1;
    for (int side = std::min(w, h); side > 1; side >>= 1) ++levels;
    return levels;
}

SurfacePlan unsupported(const std::string& why) {
    SurfacePlan p;
    p.error = why;
    return p;
}

}  // namespace

SurfacePlan planSurface(const SurfaceRequest& r, int screenWidth, int screenHeight, PixelFormat screenFormat) {
    SurfacePlan p;
    if (!(r.flags & ddsd::CAPS)) return unsupported("no DDSD_CAPS");
    if (r.flags & ddsd::CKSRCBLT) {
        p.colorKey = true;
        p.keyLow = r.keyLow;
        p.keyHigh = r.keyHigh;
    }
    if (r.caps & ddcaps::PRIMARYSURFACE) {
        p.role = SurfaceRole::Primary;
        p.width = screenWidth;
        p.height = screenHeight;
        p.format = screenFormat;
        p.backBuffer = (r.caps & ddcaps::FLIP) && (r.flags & ddsd::BACKBUFFERCOUNT) && r.backBuffers >= 1;
        return p;
    }
    if (!(r.flags & ddsd::WIDTH) || !(r.flags & ddsd::HEIGHT) || r.width <= 0 || r.height <= 0)
        return unsupported("no size");
    p.width = r.width;
    p.height = r.height;
    if (r.caps & ddcaps::ZBUFFER) {
        p.role = SurfaceRole::ZBuffer;
        return p;
    }
    if (r.caps & ddcaps::TEXTURE) {
        if (!(r.flags & ddsd::PIXELFORMAT) || r.format == PixelFormat::Unknown)
            return unsupported("texture without a known pixel format");
        p.role = SurfaceRole::Texture;
        p.format = r.format;
        if ((r.caps & ddcaps::MIPMAP) && (r.flags & ddsd::MIPMAPCOUNT) && r.mipLevels > 1)
            p.mipLevels = std::min(int(r.mipLevels), maxLevels(r.width, r.height));
        return p;
    }
    p.format = (r.flags & ddsd::PIXELFORMAT) && r.format != PixelFormat::Unknown ? r.format : screenFormat;
    if (r.caps & ddcaps::D3DDEVICE) {
        p.role = SurfaceRole::RenderTarget;
        return p;
    }
    if (r.caps & ddcaps::OFFSCREENPLAIN) {
        p.role = SurfaceRole::Offscreen;
        return p;
    }
    return unsupported("caps the client never uses");
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests plan && ./build-host/uagfx_tests target_to_client`
Expected: 7 тестов `PASS plan_…` и `PASS target_to_client_floors_into_16_bits`, `0 failed checks`.

- [ ] **Step 5: Commit**

```bash
git add src/core/surface_plan.hpp src/core/surface_plan.cpp src/core/pixel.hpp src/core/pixel.cpp tests/test_surface_plan.cpp
git commit -m "Роли поверхностей CreateSurface и перевод RT в 16 бит с полом для калибровки

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 4: Центры пикселей, эталонный растеризатор и тест калибровки клиента

**Files:**
- Create: `src/core/raster.hpp`, `src/core/raster.cpp`
- Test: `tests/test_raster.cpp`, `tests/test_calibration.cpp`

**Interfaces:**
- Consumes: `targetToClient` (Task 3), `d3d7::TADDRESS_*` (Task 1).
- Produces:
  ```cpp
  namespace uagfx {
  struct TlVertex { float x, y, z, rhw; uint32_t diffuse, specular; float u0, v0, u1, v1; };
  void tlToNdc(float x, float y, int width, int height, float& ndcX, float& ndcY);
  enum class PixelCenters { D3D7, D3D10 };
  struct RefTexture { int width = 0; int height = 0; std::vector<uint32_t> argb; };
  struct Rgb { double r, g, b; };
  class RefRaster {
  public:
      RefRaster(int width, int height, PixelCenters centers);
      void clear(const Rgb& c);
      void strip(const TlVertex* v, int count, const RefTexture& tex, bool linear, uint32_t address);
      void triangle(const TlVertex& a, const TlVertex& b, const TlVertex& c, const RefTexture& tex, bool linear,
                    uint32_t address);
      Rgb at(int x, int y) const;
      int width() const; int height() const;
  };
  }
  ```

- [ ] **Step 1: Написать падающие тесты растеризатора**

`tests/test_raster.cpp`:
```cpp
#include <cmath>

#include "core/d3d7.hpp"
#include "core/raster.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
RefTexture checker() {  // calibration.md §1: 4x4, 2x2 blocks, black-white over white-black
    const uint32_t B = 0x00000000, W = 0xFFFFFFFF;
    return RefTexture{4, 4, {B, B, W, W, B, B, W, W, W, W, B, B, W, W, B, B}};
}
TlVertex tl(float x, float y, float u, float v) {
    return TlVertex{x, y, 1e-5f, 0.99999f, 0xFFFFFFFF, 0xFFFFFFFF, u, v, 0.0f, 0.0f};
}
void quad(RefRaster& r, float x0, float y0, float u0, float v0, float u1, float v1, uint32_t address) {
    const TlVertex v[4] = {tl(x0, y0, u0, v0), tl(x0, y0 + 4, u0, v1), tl(x0 + 4, y0, u1, v0), tl(x0 + 4, y0 + 4, u1, v1)};
    r.strip(v, 4, checker(), true, address);
}
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
}  // namespace

TEST(raster_ndc_puts_d3d7_pixel_centres_on_d3d12_centres) {
    float x = 0, y = 0;
    tlToNdc(0.0f, 0.0f, 4, 4, x, y);  // D3D7 pixel 0 -> D3D12 centre 0.5 -> -0.75
    CHECK(std::fabs(x - -0.75f) < 1e-6f);
    CHECK(std::fabs(y - 0.75f) < 1e-6f);
    tlToNdc(3.0f, 3.0f, 4, 4, x, y);
    CHECK(std::fabs(x - 0.75f) < 1e-6f);
    CHECK(std::fabs(y - -0.75f) < 1e-6f);
}

TEST(raster_quad_covers_exactly_its_16_pixels) {
    RefRaster r(8, 8, PixelCenters::D3D7);
    r.clear(Rgb{0.25, 0.25, 0.25});
    quad(r, 2, 2, 0.125f, 0.125f, 1.125f, 1.125f, d3d7::TADDRESS_CLAMP);  // offset 0.5 texel: exact texels
    int covered = 0;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (!near(r.at(x, y).r, 0.25)) ++covered;
    CHECK_EQ(covered, 16);
    CHECK(near(r.at(1, 2).r, 0.25));  // left of the quad
    CHECK(near(r.at(6, 2).r, 0.25));  // right edge x = 6 is not covered (top-left rule)
}

TEST(raster_half_texel_offset_samples_texel_centres) {
    RefRaster r(4, 4, PixelCenters::D3D7);
    quad(r, 0, 0, 0.125f, 0.125f, 1.125f, 1.125f, d3d7::TADDRESS_CLAMP);
    CHECK(near(r.at(0, 0).g, 0.0));
    CHECK(near(r.at(2, 0).g, 1.0));
    CHECK(near(r.at(0, 2).g, 1.0));
    CHECK(near(r.at(3, 3).g, 0.0));
}

TEST(raster_zero_offset_blurs_half_and_half) {
    RefRaster r(4, 4, PixelCenters::D3D7);
    quad(r, 0, 0, 0.0f, 0.0f, 1.0f, 1.0f, d3d7::TADDRESS_CLAMP);  // calibration.md §5, idx 0x0000
    CHECK(near(r.at(0, 0).g, 0.0));
    CHECK(near(r.at(1, 0).g, 0.0));
    CHECK(near(r.at(2, 0).g, 0.5));
    CHECK(near(r.at(3, 0).g, 1.0));
    CHECK(near(r.at(0, 2).g, 0.5));
    CHECK(near(r.at(2, 2).g, 0.5));
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/raster.hpp: No such file or directory`.

- [ ] **Step 3: Растеризатор**

`src/core/raster.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <vector>

namespace uagfx {

// A pre-transformed vertex as the client submits it (FVF_TL1 uses u0/v0, FVF_TL2 also u1/v1).
struct TlVertex {
    float x, y, z, rhw;
    uint32_t diffuse, specular;  // ARGB
    float u0, v0, u1, v1;
};

// Direct3D 7 puts pixel centres on integer coordinates, Direct3D 10+ on half-integers: a D3D7 x becomes
// x + 0.5 in D3D12 pixel space. Returns the normalized device coordinates on a width x height target.
void tlToNdc(float x, float y, int width, int height, float& ndcX, float& ndcY);

enum class PixelCenters { D3D7, D3D10 };  // D3D10: what an uncorrected translation does (negative tests)

struct RefTexture {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> argb;  // row-major
};

struct Rgb {
    double r, g, b;
};

// A small CPU rasterizer with Direct3D 7 rules, for tests: top-left fill, perspective-correct attributes via rhw,
// colour = stage-0 texture (bilinear or point, clamp or wrap) x diffuse. Double precision throughout.
class RefRaster {
public:
    RefRaster(int width, int height, PixelCenters centers);
    void clear(const Rgb& c);
    void strip(const TlVertex* v, int count, const RefTexture& tex, bool linear, uint32_t address);
    void triangle(const TlVertex& a, const TlVertex& b, const TlVertex& c, const RefTexture& tex, bool linear,
                  uint32_t address);
    Rgb at(int x, int y) const { return px_[size_t(y) * size_t(w_) + size_t(x)]; }
    int width() const { return w_; }
    int height() const { return h_; }

private:
    int w_;
    int h_;
    PixelCenters centers_;
    std::vector<Rgb> px_;
};

}  // namespace uagfx
```

`src/core/raster.cpp`:
```cpp
#include "core/raster.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/d3d7.hpp"

namespace uagfx {
namespace {

double edge(double ax, double ay, double bx, double by, double px, double py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

// With the triangle oriented so its area is positive (y down), top edges run right along a horizontal line and
// left edges go up; pixels exactly on them belong to this triangle.
bool topLeft(double ax, double ay, double bx, double by) {
    const double dx = bx - ax;
    const double dy = by - ay;
    return (dy == 0 && dx > 0) || dy < 0;
}

int wrapIndex(int i, int n, uint32_t address) {
    if (address == d3d7::TADDRESS_WRAP) return ((i % n) + n) % n;
    return std::clamp(i, 0, n - 1);  // CLAMP; BORDER and MIRROR are not needed by the tests
}

Rgb texel(const RefTexture& t, int x, int y, uint32_t address) {
    const uint32_t c = t.argb[size_t(wrapIndex(y, t.height, address)) * size_t(t.width) + size_t(wrapIndex(x, t.width, address))];
    return Rgb{double((c >> 16) & 255) / 255.0, double((c >> 8) & 255) / 255.0, double(c & 255) / 255.0};
}

Rgb sample(const RefTexture& t, double u, double v, bool linear, uint32_t address) {
    if (!linear) return texel(t, int(std::floor(u * t.width)), int(std::floor(v * t.height)), address);
    const double s = u * t.width - 0.5;
    const double q = v * t.height - 0.5;
    const int x0 = int(std::floor(s));
    const int y0 = int(std::floor(q));
    const double fx = s - x0;
    const double fy = q - y0;
    const Rgb a = texel(t, x0, y0, address), b = texel(t, x0 + 1, y0, address);
    const Rgb c = texel(t, x0, y0 + 1, address), d = texel(t, x0 + 1, y0 + 1, address);
    auto mix = [&](double pa, double pb, double pc, double pd) {
        return (pa * (1 - fx) + pb * fx) * (1 - fy) + (pc * (1 - fx) + pd * fx) * fy;
    };
    return Rgb{mix(a.r, b.r, c.r, d.r), mix(a.g, b.g, c.g, d.g), mix(a.b, b.b, c.b, d.b)};
}

}  // namespace

void tlToNdc(float x, float y, int width, int height, float& ndcX, float& ndcY) {
    ndcX = (x + 0.5f) * 2.0f / float(width) - 1.0f;
    ndcY = 1.0f - (y + 0.5f) * 2.0f / float(height);
}

RefRaster::RefRaster(int width, int height, PixelCenters centers)
    : w_(width), h_(height), centers_(centers), px_(size_t(width) * size_t(height), Rgb{0, 0, 0}) {}

void RefRaster::clear(const Rgb& c) { std::fill(px_.begin(), px_.end(), c); }

void RefRaster::strip(const TlVertex* v, int count, const RefTexture& tex, bool linear, uint32_t address) {
    for (int i = 0; i + 2 < count; ++i) {
        if (i % 2 == 0)
            triangle(v[i], v[i + 1], v[i + 2], tex, linear, address);
        else
            triangle(v[i + 1], v[i], v[i + 2], tex, linear, address);
    }
}

void RefRaster::triangle(const TlVertex& a, const TlVertex& b, const TlVertex& c, const RefTexture& tex, bool linear,
                         uint32_t address) {
    const TlVertex* p0 = &a;
    const TlVertex* p1 = &b;
    const TlVertex* p2 = &c;
    double area = edge(p0->x, p0->y, p1->x, p1->y, p2->x, p2->y);
    if (area == 0) return;
    if (area < 0) {
        std::swap(p1, p2);
        area = -area;
    }
    const double off = centers_ == PixelCenters::D3D7 ? 0.0 : 0.5;  // pixel (x, y) has its centre at (x+off, y+off)
    const double minX = std::min({double(p0->x), double(p1->x), double(p2->x)});
    const double maxX = std::max({double(p0->x), double(p1->x), double(p2->x)});
    const double minY = std::min({double(p0->y), double(p1->y), double(p2->y)});
    const double maxY = std::max({double(p0->y), double(p1->y), double(p2->y)});
    const int x0 = std::max(0, int(std::ceil(minX - off)));
    const int x1 = std::min(w_ - 1, int(std::floor(maxX - off)));
    const int y0 = std::max(0, int(std::ceil(minY - off)));
    const int y1 = std::min(h_ - 1, int(std::floor(maxY - off)));
    const bool tl12 = topLeft(p1->x, p1->y, p2->x, p2->y);
    const bool tl20 = topLeft(p2->x, p2->y, p0->x, p0->y);
    const bool tl01 = topLeft(p0->x, p0->y, p1->x, p1->y);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const double px = x + off;
            const double py = y + off;
            const double w0 = edge(p1->x, p1->y, p2->x, p2->y, px, py);
            const double w1 = edge(p2->x, p2->y, p0->x, p0->y, px, py);
            const double w2 = edge(p0->x, p0->y, p1->x, p1->y, px, py);
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            if ((w0 == 0 && !tl12) || (w1 == 0 && !tl20) || (w2 == 0 && !tl01)) continue;
            // Perspective-correct: interpolate attribute*rhw and rhw linearly, then divide.
            const double l0 = w0 / area, l1 = w1 / area, l2 = w2 / area;
            const double q = l0 * p0->rhw + l1 * p1->rhw + l2 * p2->rhw;
            const double u = (l0 * p0->u0 * p0->rhw + l1 * p1->u0 * p1->rhw + l2 * p2->u0 * p2->rhw) / q;
            const double v = (l0 * p0->v0 * p0->rhw + l1 * p1->v0 * p1->rhw + l2 * p2->v0 * p2->rhw) / q;
            const Rgb t = sample(tex, u, v, linear, address);
            const auto chan = [&](int shift) {
                const double d0 = double((p0->diffuse >> shift) & 255) / 255.0;
                const double d1 = double((p1->diffuse >> shift) & 255) / 255.0;
                const double d2 = double((p2->diffuse >> shift) & 255) / 255.0;
                return (l0 * d0 * p0->rhw + l1 * d1 * p1->rhw + l2 * d2 * p2->rhw) / q;
            };
            px_[size_t(y) * size_t(w_) + size_t(x)] = Rgb{t.r * chan(16), t.g * chan(8), t.b * chan(0)};
        }
    }
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты растеризатора проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests raster`
Expected: 4 теста `PASS raster_…`, `4 tests, 0 failed checks`.

- [ ] **Step 5: Написать тест калибровки клиента**

`tests/test_calibration.cpp` — повтор `FUN_004051f0` (`docs/research/calibration.md` §1, §5):
```cpp
#include <cmath>
#include <cstdint>

#include "core/d3d7.hpp"
#include "core/pixel.hpp"
#include "core/raster.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// How a read-back pixel reaches the client's bright/dark sums.
enum class Readback {
    Round565,  // reference hardware model: float -> 565, round to nearest
    Floor565,  // float -> 565, floor
    Ours565,   // our driver: GPU float -> 8 bits (round), then targetToClient() floors to 565
    Xrgb8888,  // a 32-bit screen: the client masks every pixel with 0xFFFF
};

const bool kWhite[16] = {false, false, true, true, false, false, true, true,
                         true,  true,  false, false, true, true,  false, false};

int channelSum(const Rgb& p, Readback mode) {
    auto r8 = [](double v) { return uint32_t(std::lround(v * 255.0)); };
    switch (mode) {
        case Readback::Round565: {
            const int r = int(std::lround(p.r * 31)), g = int(std::lround(p.g * 63)), b = int(std::lround(p.b * 31));
            return (r << 3) + (g << 2) + (b << 3);
        }
        case Readback::Floor565: {
            const int r = int(std::floor(p.r * 31 + 1e-9)), g = int(std::floor(p.g * 63 + 1e-9)),
                      b = int(std::floor(p.b * 31 + 1e-9));
            return (r << 3) + (g << 2) + (b << 3);
        }
        case Readback::Ours565: {
            const uint32_t v = targetToClient(PixelFormat::R5G6B5, 0xFF000000u | r8(p.r) << 16 | r8(p.g) << 8 | r8(p.b));
            return int(((v >> 11) & 31) << 3) + int(((v >> 5) & 63) << 2) + int((v & 31) << 3);
        }
        case Readback::Xrgb8888: {
            const uint32_t v = (r8(p.r) << 16 | r8(p.g) << 8 | r8(p.b)) & 0xFFFF;  // red is lost
            return int((v >> 8) & 255) + int(v & 255);
        }
    }
    return 0;
}

struct Calibration {
    uint32_t best = 0x8080;  // the client's starting index: (0.5, 0.5)
    int accepted = 0;
};

Calibration calibrate(int w, int h, PixelCenters centers, Readback mode, uint32_t address) {
    const uint32_t B = 0x00000000, W = 0xFFFFFFFF;
    const RefTexture tex{4, 4, {B, B, W, W, B, B, W, W, W, W, B, B, W, W, B, B}};
    const uint32_t cols = uint32_t(w) >> 2, rows = uint32_t(h) >> 2, perPass = cols * rows;
    Calibration out;
    int best = -1024;
    for (uint32_t base = 0; base < 0x10000; base += perPass) {
        RefRaster r(w, h, centers);
        for (uint32_t i = 0; i < perPass && base + i <= 0xFFFF; ++i) {
            const uint32_t idx = base + i, lo = idx & 0xFF, hi = idx >> 8;
            const float x0 = float(4 * (i % cols)), y0 = float(4 * (i / cols));
            const float du = float(lo) * (1 / 256.f), dv = float(hi) * (1 / 256.f);
            const float u0 = du / 4, u1 = (du + 4.0f) / 4, v0 = dv / 4, v1 = (dv + 4.0f) / 4;
            const TlVertex v[4] = {{x0, y0, 1e-5f, 0.99999f, ~0u, ~0u, u0, v0, 0, 0},
                                   {x0, y0 + 4, 1e-5f, 0.99999f, ~0u, ~0u, u0, v1, 0, 0},
                                   {x0 + 4, y0, 1e-5f, 0.99999f, ~0u, ~0u, u1, v0, 0, 0},
                                   {x0 + 4, y0 + 4, 1e-5f, 0.99999f, ~0u, ~0u, u1, v1, 0, 0}};
            r.strip(v, 4, tex, true, address);
        }
        for (uint32_t i = 0; i < perPass && base + i <= 0xFFFF; ++i) {
            int bright = 0, dark = 0;
            for (int k = 0; k < 16; ++k) {
                const int x = int(4 * (i % cols)) + (k & 3), y = int(4 * (i / cols)) + (k >> 2);
                (kWhite[k] ? bright : dark) += channelSum(r.at(x, y), mode);
            }
            if (bright >= 0x1680 && dark <= 0x180) {
                ++out.accepted;
                if (bright - dark > best) {
                    best = bright - dark;
                    out.best = base + i;
                }
            }
        }
    }
    return out;
}

}  // namespace

TEST(calibration_32bit_screen_never_accepts_and_keeps_half) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Xrgb8888, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.accepted, 0);
    CHECK_EQ(c.best, 0x8080u);
}

TEST(calibration_our_16bit_readback_settles_on_half) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Ours565, d3d7::TADDRESS_CLAMP);
    CHECK(c.accepted > 0);
    CHECK_EQ(c.best, 0x8080u);
}

TEST(calibration_floor_model_settles_on_half) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Floor565, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.best, 0x8080u);
}

TEST(calibration_round_model_matches_the_research) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Round565, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.best, 0x7E80u);  // (0.5, 0.4921875)
}

TEST(calibration_uncorrected_d3d10_centres_pick_zero) {
    const Calibration c = calibrate(1024, 1024, PixelCenters::D3D10, Readback::Round565, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.best, 0x0000u);  // what a translation without the +0.5 shift would make the client choose
}

TEST(calibration_address_mode_changes_the_count) {
    const Calibration clamp = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Round565, d3d7::TADDRESS_CLAMP);
    const Calibration wrap = calibrate(1024, 1024, PixelCenters::D3D7, Readback::Round565, d3d7::TADDRESS_WRAP);
    CHECK(clamp.accepted != wrap.accepted);
}

TEST(calibration_runs_in_passes_on_a_small_screen) {
    const Calibration c = calibrate(800, 600, PixelCenters::D3D7, Readback::Ours565, d3d7::TADDRESS_CLAMP);
    CHECK_EQ(c.best, 0x8080u);
}
```

- [ ] **Step 6: Тесты калибровки**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests calibration`
Expected: 7 тестов `PASS calibration_…`. Числа из `calibration.md` получены другим симулятором; если не сходится
`calibration_round_model_matches_the_research` или `…uncorrected_d3d10…`, это повод для systematic-debugging
(сначала проверить растеризатор на `raster_…`), а не правка ожиданий. `calibration_our_16bit_readback_settles_on_half`
и `calibration_32bit_…` — требования к драйверу: если они падают, меняется перевод `targetToClient`, не тест.

- [ ] **Step 7: Commit**

```bash
git add src/core/raster.hpp src/core/raster.cpp tests/test_raster.cpp tests/test_calibration.cpp
git commit -m "Центры пикселей DX7→DX12, эталонный растеризатор и повтор калибровки клиента

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Генератор базовых классов COM

**Files:**
- Create: `tools/gen_com_bases.py`, `src/win/dx7/com.hpp`, `src/win/dx7/com.cpp`
- Create (generated, committed): `src/win/dx7/com_bases.hpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace uagfx::dx7 {
  HRESULT notImplemented(const char* method);  // logs once per name, returns DDERR_UNSUPPORTED
  class DirectDraw7Base : public IDirectDraw7 { /* every IDirectDraw7 method except IUnknown's */ };
  class Surface7Base : public IDirectDrawSurface7 { ... };
  class ClipperBase : public IDirectDrawClipper { ... };
  class Direct3D7Base : public IDirect3D7 { ... };
  class Device7Base : public IDirect3DDevice7 { ... };
  }
  ```
  Наши классы наследуют эти базы и переопределяют только то, что вызывает клиент; IUnknown (QueryInterface,
  AddRef, Release) реализует каждый класс сам.

- [ ] **Step 1: Генератор**

`tools/gen_com_bases.py`:
```python
#!/usr/bin/env python3
"""Generates src/win/dx7/com_bases.hpp from the MinGW-w64 headers.

One C++ base class per DirectDraw / Direct3D 7 interface the driver implements. Every method except IUnknown's
answers DDERR_UNSUPPORTED through notImplemented(), which logs the name once. Our classes override only what
uaRO.exe calls (docs/research/*.md).

Usage: tools/gen_com_bases.py          write the header
       tools/gen_com_bases.py --check  exit 1 if the committed header is out of date
"""
import re
import sys
from pathlib import Path

INCLUDE = Path("/usr/share/mingw-w64/include")
OUT = Path(__file__).resolve().parent.parent / "src/win/dx7/com_bases.hpp"
INTERFACES = [
    ("ddraw.h", "IDirectDraw7", "DirectDraw7Base"),
    ("ddraw.h", "IDirectDrawSurface7", "Surface7Base"),
    ("ddraw.h", "IDirectDrawClipper", "ClipperBase"),
    ("d3d.h", "IDirect3D7", "Direct3D7Base"),
    ("d3d.h", "IDirect3DDevice7", "Device7Base"),
]
SKIP = {"QueryInterface", "AddRef", "Release"}


def methods(header, iface):
    text = (INCLUDE / header).read_text(errors="replace")
    m = re.search(r"DECLARE_INTERFACE_\(\s*%s\s*,\s*\w+\s*\)\s*\{(.*?)\n\};" % re.escape(iface), text, re.S)
    if not m:
        sys.exit("interface %s not found in %s" % (iface, header))
    body = re.sub(r"/\*.*?\*/", " ", m.group(1), flags=re.S)
    out = []
    for head, params in re.findall(r"STDMETHOD_?\(([^)]*)\)\s*\((.*?)\)\s*PURE\s*;", body, re.S):
        parts = [p.strip() for p in head.split(",")]
        name = parts[-1]
        ret = parts[0] if len(parts) == 2 else "HRESULT"
        if name in SKIP:
            continue
        if ret != "HRESULT":
            sys.exit("%s::%s returns %s; teach the generator" % (iface, name, ret))
        params = " ".join(params.split())
        params = "" if params == "THIS" else re.sub(r"^THIS_\s*", "", params)
        out.append((name, params))
    return out


def generate():
    lines = [
        "// Generated by tools/gen_com_bases.py from the MinGW-w64 headers - do not edit.",
        "#pragma once",
        "#include <ddraw.h>",
        "#include <d3d.h>",
        "",
        '#include "win/dx7/com.hpp"',
        "",
        "#pragma GCC diagnostic push",
        '#pragma GCC diagnostic ignored "-Wunused-parameter"',
        "",
        "namespace uagfx::dx7 {",
    ]
    for header, iface, cls in INTERFACES:
        lines += ["", "class %s : public %s {" % (cls, iface), "public:", "    virtual ~%s() = default;" % cls]
        for name, params in methods(header, iface):
            lines.append('    HRESULT STDMETHODCALLTYPE %s(%s) override { return notImplemented("%s::%s"); }'
                         % (name, params, iface, name))
        lines.append("};")
    lines += ["", "}  // namespace uagfx::dx7", "", "#pragma GCC diagnostic pop", ""]
    return "\n".join(lines)


def main():
    text = generate()
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_text() != text:
            print("FAIL: %s is out of date; run tools/gen_com_bases.py" % OUT)
            sys.exit(1)
        print("OK: %s is up to date" % OUT)
        return
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text)
    print("wrote %s" % OUT)


if __name__ == "__main__":
    main()
```

Run: `python3 tools/gen_com_bases.py --check`
Expected: `FAIL: …/src/win/dx7/com_bases.hpp is out of date; run tools/gen_com_bases.py` (файла ещё нет).

- [ ] **Step 2: `notImplemented` и сгенерированный заголовок**

`src/win/dx7/com.hpp`:
```cpp
#pragma once
#include <windows.h>

namespace uagfx::dx7 {

// A method uaRO.exe is not known to call (docs/research/*.md): logged once per name, answered DDERR_UNSUPPORTED.
HRESULT notImplemented(const char* method);

}  // namespace uagfx::dx7
```

`src/win/dx7/com.cpp`:
```cpp
#include "win/dx7/com.hpp"

#include <ddraw.h>

#include <set>
#include <string>

#include "win/log.hpp"

namespace uagfx::dx7 {
namespace {

struct Seen {
    CRITICAL_SECTION lock;
    std::set<std::string> names;
    Seen() { InitializeCriticalSection(&lock); }
};

Seen& seen() {
    static Seen s;
    return s;
}

}  // namespace

HRESULT notImplemented(const char* method) {
    Seen& s = seen();
    EnterCriticalSection(&s.lock);
    const bool first = s.names.insert(method).second;
    LeaveCriticalSection(&s.lock);
    if (first) LOG_INFO("not implemented: %s (answered DDERR_UNSUPPORTED)", method);
    return DDERR_UNSUPPORTED;
}

}  // namespace uagfx::dx7
```

Run: `python3 tools/gen_com_bases.py && python3 tools/gen_com_bases.py --check && grep -c "override" src/win/dx7/com_bases.hpp`
Expected: `wrote …`, `OK: … is up to date`, число методов > 140 (у IDirectDrawSurface7 и IDirect3DDevice7 по ~45).

- [ ] **Step 3: Заголовок компилируется**

Временный файл не нужен: `com_bases.hpp` подключается из `com.cpp` проверочной строкой. Добавить в конец
`src/win/dx7/com.cpp`:
```cpp
#include "win/dx7/com_bases.hpp"  // compiled here so a header the MinGW headers disagree with fails the build early
```

Run: `cmake -S . -B build-win --no-warn-unused-cli -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build-win -j && python3 tools/check_dll.py`
Expected: сборка без предупреждений, `OK: build-win/ddraw.dll: 22 exports, …`.

- [ ] **Step 4: Commit**

```bash
git add tools/gen_com_bases.py src/win/dx7
git commit -m "Базовые классы COM из заголовков MinGW: всё, что клиент не вызывает, — DDERR_UNSUPPORTED с записью в лог

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 6: Маршрут Blt и учёт касаний копии RT (ядро)

**Files:**
- Create: `src/core/blit.hpp`, `src/core/blit.cpp`, `src/core/shadow_state.hpp`, `src/core/shadow_state.cpp`
- Test: `tests/test_blit.cpp`, `tests/test_shadow_state.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace uagfx {
  namespace ddblt { constexpr uint32_t ASYNC = 0x200, COLORFILL = 0x400, DDFX = 0x800, KEYSRC = 0x8000, WAIT = 0x1000000; }
  namespace ddbltfast { constexpr uint32_t SRCCOLORKEY = 0x1, DESTCOLORKEY = 0x2, WAIT = 0x10; }
  enum class Place { Cpu, Target, Front };
  enum class BlitAction { Fill, FillTarget, CopyCpu, ReadTarget, WriteTarget, Present, Unsupported };
  struct BlitRequest { Place dst = Place::Cpu; Place src = Place::Cpu; bool hasSource = false; uint32_t flags = 0; };
  BlitAction routeBlit(const BlitRequest& r);
  class ShadowState {
  public:
      struct Touch { bool readBack; bool writable; };
      void gpuChanged(); void lock(); Touch touch(bool write); bool unlock();
      bool locked() const; bool stale() const; bool dirty() const;
  };
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_blit.cpp` (строки `surfaces-present-caps.md` §3):
```cpp
#include "core/blit.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
BlitAction route(Place dst, Place src, uint32_t flags, bool hasSource = true) {
    BlitRequest r;
    r.dst = dst;
    r.src = src;
    r.hasSource = hasSource;
    r.flags = flags;
    return routeBlit(r);
}
}  // namespace

TEST(blit_present_paths) {
    CHECK(route(Place::Front, Place::Target, ddblt::WAIT) == BlitAction::Present);  // windowed present, Bink
}

TEST(blit_colour_fill) {
    CHECK(route(Place::Cpu, Place::Cpu, ddblt::COLORFILL | ddblt::WAIT, false) == BlitAction::Fill);
    CHECK(route(Place::Target, Place::Cpu, ddblt::COLORFILL | ddblt::WAIT, false) == BlitAction::FillTarget);
    CHECK(route(Place::Front, Place::Cpu, ddblt::COLORFILL, false) == BlitAction::Unsupported);
}

TEST(blit_between_memory_and_the_target) {
    CHECK(route(Place::Cpu, Place::Target, ddblt::WAIT) == BlitAction::ReadTarget);  // calibration, fade save
    CHECK(route(Place::Target, Place::Cpu, ddblt::WAIT) == BlitAction::WriteTarget);  // fade restore, big pictures
    CHECK(route(Place::Cpu, Place::Cpu, ddblt::ASYNC) == BlitAction::CopyCpu);         // mip levels, glyphs
}

TEST(blit_what_the_client_never_does) {
    CHECK(route(Place::Target, Place::Target, ddblt::WAIT) == BlitAction::Unsupported);
    CHECK(route(Place::Cpu, Place::Front, ddblt::WAIT) == BlitAction::Unsupported);
    CHECK(route(Place::Cpu, Place::Cpu, ddblt::WAIT, false) == BlitAction::Unsupported);  // no source, no fill
}
```

`tests/test_shadow_state.cpp`:
```cpp
#include "core/shadow_state.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(shadow_untouched_lock_reads_nothing) {
    ShadowState s;  // the client locks the target every frame and never touches the memory
    s.lock();
    CHECK(s.locked());
    CHECK(!s.unlock());
    CHECK(!s.dirty());
    CHECK(s.stale());  // nothing was read, so the next touch still reads back
}

TEST(shadow_first_read_reads_back_once) {
    ShadowState s;
    s.lock();
    ShadowState::Touch t = s.touch(false);
    CHECK(t.readBack);
    CHECK(!t.writable);
    CHECK(!s.stale());
    t = s.touch(false);
    CHECK(!t.readBack);
    CHECK(!s.unlock());
}

TEST(shadow_write_marks_dirty_and_unlock_asks_for_an_upload) {
    ShadowState s;
    s.lock();
    ShadowState::Touch t = s.touch(true);  // the wave effect, Bink
    CHECK(t.readBack);
    CHECK(t.writable);
    CHECK(s.dirty());
    CHECK(s.unlock());
    CHECK(!s.locked());
    CHECK(!s.stale());  // our own upload: the copy and the GPU target agree
}

TEST(shadow_gpu_change_makes_the_next_lock_read_again) {
    ShadowState s;
    s.lock();
    s.touch(false);
    s.unlock();
    s.lock();
    CHECK(!s.touch(false).readBack);  // nothing drawn in between
    s.unlock();
    s.gpuChanged();
    s.lock();
    CHECK(s.touch(false).readBack);
}

TEST(shadow_read_then_write_needs_one_readback) {
    ShadowState s;
    s.lock();
    CHECK(s.touch(false).readBack);
    const ShadowState::Touch t = s.touch(true);
    CHECK(!t.readBack);
    CHECK(t.writable);
    CHECK(s.unlock());
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/blit.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/blit.hpp`:
```cpp
#pragma once
#include <cstdint>

namespace uagfx {

// DDBLT / DDBLTFAST flags the client uses (ddraw.h values).
namespace ddblt {
constexpr uint32_t ASYNC = 0x200;
constexpr uint32_t COLORFILL = 0x400;
constexpr uint32_t DDFX = 0x800;
constexpr uint32_t KEYSRC = 0x8000;
constexpr uint32_t WAIT = 0x1000000;
}  // namespace ddblt
namespace ddbltfast {
constexpr uint32_t SRCCOLORKEY = 0x1;
constexpr uint32_t DESTCOLORKEY = 0x2;
constexpr uint32_t WAIT = 0x10;
}  // namespace ddbltfast

// Where a surface's pixels live: CPU memory (offscreen surfaces, textures), the GPU render target (the back
// buffer the client draws into), or the window (the primary surface).
enum class Place { Cpu, Target, Front };

enum class BlitAction {
    Fill,         // DDBLT_COLORFILL on a CPU surface
    FillTarget,   // DDBLT_COLORFILL on the render target
    CopyCpu,      // CPU surface -> CPU surface (mip levels, glyphs, big pictures)
    ReadTarget,   // render target -> CPU surface (calibration readback, fade save)
    WriteTarget,  // CPU surface -> render target (fade restore, pictures bigger than a texture)
    Present,      // render target -> primary: show a frame (windowed present, loading bar, Bink)
    Unsupported,
};

struct BlitRequest {
    Place dst = Place::Cpu;
    Place src = Place::Cpu;
    bool hasSource = false;
    uint32_t flags = 0;
};

BlitAction routeBlit(const BlitRequest& r);

}  // namespace uagfx
```

`src/core/blit.cpp`:
```cpp
#include "core/blit.hpp"

namespace uagfx {

BlitAction routeBlit(const BlitRequest& r) {
    if (r.flags & ddblt::COLORFILL) {
        if (r.dst == Place::Cpu) return BlitAction::Fill;
        if (r.dst == Place::Target) return BlitAction::FillTarget;
        return BlitAction::Unsupported;
    }
    if (!r.hasSource) return BlitAction::Unsupported;
    if (r.dst == Place::Front && r.src == Place::Target) return BlitAction::Present;
    if (r.dst == Place::Cpu && r.src == Place::Cpu) return BlitAction::CopyCpu;
    if (r.dst == Place::Cpu && r.src == Place::Target) return BlitAction::ReadTarget;
    if (r.dst == Place::Target && r.src == Place::Cpu) return BlitAction::WriteTarget;
    return BlitAction::Unsupported;
}

}  // namespace uagfx
```

`src/core/shadow_state.hpp`:
```cpp
#pragma once

namespace uagfx {

// Bookkeeping for the CPU copy of the render target that Lock and GetDC hand out. Reading the GPU target back
// stalls, so it happens only when the client really touches the memory: the client locks the target every frame
// without touching it (surfaces-present-caps.md §2). The Windows side protects the pages and calls touch() from
// its exception handler on the first read or write.
class ShadowState {
public:
    struct Touch {
        bool readBack;  // copy the GPU target into the memory first
        bool writable;  // open the pages for writing (otherwise read-only, so a later write is noticed)
    };

    void gpuChanged() { stale_ = true; }  // something was drawn or written into the target by other means
    void lock() {
        locked_ = true;
        dirty_ = false;
    }
    Touch touch(bool write);
    bool unlock();  // true when the client wrote: upload the memory into the target
    bool locked() const { return locked_; }
    bool stale() const { return stale_; }
    bool dirty() const { return dirty_; }

private:
    bool locked_ = false;
    bool stale_ = true;
    bool dirty_ = false;
};

}  // namespace uagfx
```

`src/core/shadow_state.cpp`:
```cpp
#include "core/shadow_state.hpp"

namespace uagfx {

ShadowState::Touch ShadowState::touch(bool write) {
    Touch t{stale_, write};
    stale_ = false;
    if (write) dirty_ = true;
    return t;
}

bool ShadowState::unlock() {
    locked_ = false;
    return dirty_;  // after the upload the copy and the target agree: stale_ stays false
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests blit && ./build-host/uagfx_tests shadow`
Expected: 4 теста `PASS blit_…` и 5 `PASS shadow_…`, `0 failed checks`.

- [ ] **Step 5: Commit**

```bash
git add src/core/blit.hpp src/core/blit.cpp src/core/shadow_state.hpp src/core/shadow_state.cpp tests/test_blit.cpp tests/test_shadow_state.cpp
git commit -m "Маршрут Blt и учёт касаний копии RT: чтение кадра с GPU только при реальном обращении

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 7: Рендер D3D12 — устройство, RT клиента, текстуры, чтение кадра, показ в окне

Рисование (`Draw`) в этой задаче — заглушка; его делает Task 8.

**Files:**
- Modify: `CMakeLists.txt` (определение `WIDL_EXPLICIT_AGGREGATE_RETURNS`)
- Create: `src/win/gpu/renderer.hpp`, `src/win/gpu/impl.hpp`, `src/win/gpu/device.cpp`, `src/win/gpu/shaders.hpp`,
  `src/win/gpu/shaders.cpp`, `src/win/gpu/pipeline.cpp`, `src/win/gpu/draw.cpp`

**Interfaces:**
- Consumes: `FrameRecord`, `Cmd`, `ClearCmd`, `ViewportCmd`, `UploadCmd`, `WriteTargetCmd`, `DrawCmd` (Task 2),
  `StageSetup`/`DrawSetup` (Task 1), `Rect` (1A).
- Produces:
  ```cpp
  namespace uagfx::gpu {
  struct PresentOptions { bool vsync = true; bool allowTearing = false; int fpsLimit = 0; };
  class Renderer {
  public:
      bool create(std::string& error);
      std::string adapterName() const;
      bool setTarget(int width, int height);
      int targetWidth() const; int targetHeight() const;
      uint32_t createTexture(int width, int height, int levels);
      void releaseTexture(uint32_t id);
      void execute(const FrameRecord& record);
      bool readTarget(std::vector<uint32_t>& argb);
      void present(HWND window, const Rect& source, const PresentOptions& options);
  };
  }
  ```
  Внутри (`impl.hpp`): `struct Renderer::Impl` с методами `begin()`, `submit()`, `waitIdle()`, `allocUpload()`,
  `transition()`, `samplerTable()`, `textureTable()`, `draw()` — Task 8 дополняет `pipeline.cpp`/`draw.cpp`.

- [ ] **Step 1: Сборка с правильным ABI для D3D12**

MinGW по умолчанию объявляет методы D3D12, возвращающие структуру (`GetCPUDescriptorHandleForHeapStart`, `GetDesc`),
не так, как их вызывает драйвер (MSVC ABI); макрос `WIDL_EXPLICIT_AGGREGATE_RETURNS` включает правильные объявления.
В `CMakeLists.txt` строку
```cmake
    target_compile_definitions(ddraw PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
```
заменить на
```cmake
    target_compile_definitions(ddraw PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX WIDL_EXPLICIT_AGGREGATE_RETURNS)
```

- [ ] **Step 2: Интерфейс рендера**

`src/win/gpu/renderer.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"

namespace uagfx::gpu {

struct PresentOptions {
    bool vsync = true;
    bool allowTearing = false;  // used only with vsync off, when Windows supports it
    int fpsLimit = 0;           // 0 = no limit
};

// Direct3D 12 behind the DirectDraw surfaces: plays frame records onto the render target the client draws into,
// reads that target back for the client and shows it in the window. Called under the driver's global lock.
class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool create(std::string& error);  // device, queue, heaps, shaders; false when Direct3D 12 is not usable
    std::string adapterName() const;
    bool setTarget(int width, int height);  // the client's target (BGRA8) and depth (D32), cleared to black
    int targetWidth() const;
    int targetHeight() const;

    uint32_t createTexture(int width, int height, int levels);  // an ARGB8 texture; 0 on failure
    void releaseTexture(uint32_t id);                           // freed once the GPU no longer uses it

    void execute(const FrameRecord& record);        // submits, does not wait
    bool readTarget(std::vector<uint32_t>& argb);   // waits; width * height values, row-major
    void present(HWND window, const Rect& source, const PresentOptions& options);

    struct Impl;

private:
    Impl* impl_;
};

}  // namespace uagfx::gpu
```

- [ ] **Step 3: Внутренности**

`src/win/gpu/impl.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/frame.hpp"
#include "win/gpu/renderer.hpp"

namespace uagfx::gpu {

template <typename T>
void releaseCom(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

constexpr UINT kSlots = 2;              // submissions in flight
constexpr UINT kCpuSrvCount = 65536;    // one SRV per texture (the client keeps thousands), the white one, the target
constexpr UINT kNoSrv = UINT(-1);       // allocSrv: the heap is full
constexpr UINT kGpuSrvCount = 65536;    // ring of texture pairs bound for drawing
constexpr UINT kSamplerCount = 1024;    // cache of sampler pairs, two descriptors each
constexpr UINT64 kUploadSize = 32ull << 20;  // per slot; grows when one submission needs more

constexpr UINT kRootConstants = 0;
constexpr UINT kRootTextures = 1;
constexpr UINT kRootSamplers = 2;

// Root constants; must match cbuffer Params in shaders.cpp.
struct Constants {
    float targetSize[2];   // 0-1
    uint32_t stage0[2];    // 2-3: colorOp | colorArg1 << 8 | colorArg2 << 16 | alphaOp << 24, alphaArg1 | alphaArg2 << 8 | texCoord << 16 | keyed << 24
    uint32_t stage1[2];    // 4-5
    uint32_t alphaTest;    // 6: func | ref << 8 | enabled << 16
    uint32_t fogEnable;    // 7
    float fogColor[4];     // 8-11
    uint32_t colorKey;     // 12
    uint32_t pad[3];       // 13-15
    float sourceRect[4];   // 16-19: present only, normalised u0 v0 u1 v1
};
constexpr UINT kConstantCount = sizeof(Constants) / 4;
static_assert(kConstantCount == 20, "Constants must stay 20 DWORDs");

struct Texture {
    ID3D12Resource* resource = nullptr;
    int width = 0;
    int height = 0;
    int levels = 1;
    UINT srv = 0;  // index in the CPU SRV heap
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
};

struct Slot {
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12Resource* upload = nullptr;
    uint8_t* mapped = nullptr;
    UINT64 size = 0;
    UINT64 used = 0;
    UINT64 fence = 0;                        // signalled when this slot's last submission finished
    std::vector<IUnknown*> garbage;          // released once that fence has passed
};

struct Upload {
    uint8_t* cpu;
    D3D12_GPU_VIRTUAL_ADDRESS gpu;
    UINT64 offset;  // in the slot's upload buffer
};

struct Renderer::Impl {
    HMODULE d3d12 = nullptr;
    HMODULE dxgi = nullptr;
    HMODULE compiler = nullptr;
    IDXGIFactory4* factory = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValue = 0;
    Slot slots[kSlots];
    UINT slot = 0;
    bool open = false;
    std::string adapter;

    ID3D12DescriptorHeap* cpuSrv = nullptr;
    std::vector<UINT> freeSrv;
    UINT nextSrv = 0;
    ID3D12DescriptorHeap* gpuSrv = nullptr;
    UINT ringSrv = 0;
    ID3D12DescriptorHeap* samplerHeap = nullptr;
    std::unordered_map<uint64_t, UINT> samplerPairs;
    UINT nextSampler = 0;
    ID3D12DescriptorHeap* rtvHeap = nullptr;  // 0: target, 1-2: swap chain buffers
    ID3D12DescriptorHeap* dsvHeap = nullptr;
    UINT srvStep = 0;
    UINT samplerStep = 0;
    UINT rtvStep = 0;

    ID3D12Resource* target = nullptr;
    ID3D12Resource* depth = nullptr;
    D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    UINT targetSrv = 0;
    int width = 0;
    int height = 0;
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissor{};
    ID3D12Resource* readback = nullptr;
    UINT64 readbackSize = 0;

    std::unordered_map<uint32_t, Texture> textures;
    uint32_t nextTexture = 1;
    uint32_t white = 0;

    ID3D12RootSignature* root = nullptr;
    ID3DBlob* drawVs = nullptr;
    ID3DBlob* drawPs = nullptr;
    ID3DBlob* presentVs = nullptr;
    ID3DBlob* presentPs = nullptr;
    ID3D12PipelineState* presentPipeline = nullptr;
    std::unordered_map<uint64_t, ID3D12PipelineState*> pipelines;

    HWND window = nullptr;
    IDXGISwapChain3* swapChain = nullptr;
    ID3D12Resource* backBuffers[2] = {nullptr, nullptr};
    D3D12_RESOURCE_STATES backStates[2] = {D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT};
    UINT swapWidth = 0;
    UINT swapHeight = 0;
    bool tearingSupported = false;
    LARGE_INTEGER lastPresent{};

    // Geometry of the record being executed, copied into the upload buffer once per execute().
    D3D12_GPU_VIRTUAL_ADDRESS vertexBase = 0;
    D3D12_GPU_VIRTUAL_ADDRESS indexBase = 0;
    UINT indexBytes = 0;
    // Bindings already set on the open command list (reset by begin()).
    ID3D12PipelineState* boundPipeline = nullptr;
    uint64_t boundTextures = ~0ull;
    uint64_t boundSamplers = ~0ull;
    bool targetBound = false;

    // device.cpp
    bool begin();
    void submit();
    void waitSlot(Slot& s);
    void waitIdle();
    bool growUpload(Slot& s, UINT64 size);
    Upload allocUpload(UINT64 size, UINT64 alignment);
    void transition(ID3D12Resource* r, D3D12_RESOURCE_STATES& state, D3D12_RESOURCE_STATES to);
    UINT allocSrv();
    D3D12_CPU_DESCRIPTOR_HANDLE cpuSrvHandle(UINT i) const;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(UINT i) const;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle() const;
    bool ensureSwapChain(HWND hwnd, bool wantTearing);
    void releaseSwapChain();
    void clear(const ClearCmd& c);
    void setViewport(const ViewportCmd& v);
    void upload(const FrameRecord& rec, const UploadCmd& u);
    void writeTarget(const FrameRecord& rec, const WriteTargetCmd& w);
    void bindTarget();
    void destroy();
    // pipeline.cpp
    bool createPipelines(std::string& error);
    D3D12_GPU_DESCRIPTOR_HANDLE samplerTable(const StageSetup& s0, const StageSetup& s1);
    D3D12_GPU_DESCRIPTOR_HANDLE textureTable(UINT srv0, UINT srv1);
    ID3D12PipelineState* pipelineFor(const DrawSetup& s, uint32_t fvf);
    // draw.cpp
    void prepareGeometry(const FrameRecord& rec);
    void draw(const FrameRecord& rec, const DrawCmd& d);
};

}  // namespace uagfx::gpu
```

- [ ] **Step 4: Шейдеры (HLSL, общий источник для рисования и показа)**

`src/win/gpu/shaders.hpp`:
```cpp
#pragma once

namespace uagfx::gpu {

// HLSL for shader model 5.0: draw_vs/draw_ps emulate Direct3D 7's fixed pipeline for pre-transformed vertices,
// present_vs/present_ps scale the client's target into the window.
const char* shaderSource();

}  // namespace uagfx::gpu
```

`src/win/gpu/shaders.cpp`:
```cpp
#include "win/gpu/shaders.hpp"

namespace uagfx::gpu {

const char* shaderSource() {
    return R"HLSL(
cbuffer Params : register(b0) {
    float2 targetSize;
    uint2 stage0;
    uint2 stage1;
    uint alphaTest;
    uint fogEnable;
    float4 fogColor;
    uint colorKey;
    uint3 pad;
    float4 sourceRect;
};
Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
SamplerState samp0 : register(s0);
SamplerState samp1 : register(s1);

struct VsIn {
    float4 pos : POSITION;       // x, y in D3D7 pixels, z in [0, 1], rhw
    float4 diffuse : COLOR0;
    float4 specular : COLOR1;    // alpha = vertex fog factor
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};
struct PsIn {
    float4 pos : SV_Position;
    float4 diffuse : COLOR0;
    float4 specular : COLOR1;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

// D3D7 puts pixel centres on integer coordinates, D3D12 on half-integers: x + 0.5 (calibration.md §3).
PsIn draw_vs(VsIn i) {
    PsIn o;
    float w = i.pos.w > 0.0 ? 1.0 / i.pos.w : 1.0;
    float x = (i.pos.x + 0.5) * 2.0 / targetSize.x - 1.0;
    float y = 1.0 - (i.pos.y + 0.5) * 2.0 / targetSize.y;
    o.pos = float4(x * w, y * w, i.pos.z * w, w);
    o.diffuse = i.diffuse;
    o.specular = i.specular;
    o.uv0 = i.uv0;
    o.uv1 = i.uv1;
    return o;
}

float4 arg(uint a, float4 diffuse, float4 current, float4 tex, float4 specular) {
    uint sel = a & 15;
    float4 v = float4(1, 1, 1, 1);  // TFACTOR: the client never sets it, D3D7's default is white
    if (sel == 0) v = diffuse;
    else if (sel == 1) v = current;
    else if (sel == 2) v = tex;
    else if (sel == 4) v = specular;
    if (a & 16) v = 1.0 - v;   // COMPLEMENT
    if (a & 32) v = v.aaaa;    // ALPHAREPLICATE
    return v;
}

float4 combine(uint op, float4 a1, float4 a2, float4 diffuse, float4 tex, float4 current) {
    switch (op) {
        case 2: return a1;                                  // SELECTARG1
        case 3: return a2;                                  // SELECTARG2
        case 4: return a1 * a2;                             // MODULATE
        case 5: return saturate(a1 * a2 * 2);               // MODULATE2X
        case 6: return saturate(a1 * a2 * 4);               // MODULATE4X
        case 7: return saturate(a1 + a2);                   // ADD
        case 8: return saturate(a1 + a2 - 0.5);             // ADDSIGNED
        case 9: return saturate((a1 + a2 - 0.5) * 2);       // ADDSIGNED2X
        case 10: return saturate(a1 - a2);                  // SUBTRACT
        case 11: return saturate(a1 + a2 - a1 * a2);        // ADDSMOOTH
        case 12: return lerp(a2, a1, diffuse.a);            // BLENDDIFFUSEALPHA
        case 13: return lerp(a2, a1, tex.a);                // BLENDTEXTUREALPHA
        case 14: return a1;                                 // BLENDFACTORALPHA, factor alpha 1
        case 15: return saturate(a1 + a2 * (1 - tex.a));    // BLENDTEXTUREALPHAPM
        case 16: return lerp(a2, a1, current.a);            // BLENDCURRENTALPHA
        case 18: return saturate(a1 + a1.aaaa * a2);        // MODULATEALPHA_ADDCOLOR
        case 19: return saturate(a1 * a2 + a1.aaaa);        // MODULATECOLOR_ADDALPHA
        case 20: return saturate((1 - a1.aaaa) * a2 + a1);  // MODULATEINVALPHA_ADDCOLOR
        case 21: return saturate((1 - a1) * a2 + a1.aaaa);  // MODULATEINVCOLOR_ADDALPHA
    }
    return current;
}

float4 stage(uint2 s, float4 diffuse, float4 specular, float4 current, float4 tex) {
    uint colorOp = s.x & 255, colorArg1 = (s.x >> 8) & 255, colorArg2 = (s.x >> 16) & 255, alphaOp = s.x >> 24;
    uint alphaArg1 = s.y & 255, alphaArg2 = (s.y >> 8) & 255;
    float3 rgb = combine(colorOp, arg(colorArg1, diffuse, current, tex, specular),
                         arg(colorArg2, diffuse, current, tex, specular), diffuse, tex, current).rgb;
    float a = current.a;  // ALPHAOP DISABLE with COLOROP on: the alpha so far passes through
    if (alphaOp != 1)
        a = combine(alphaOp, arg(alphaArg1, diffuse, current, tex, specular),
                    arg(alphaArg2, diffuse, current, tex, specular), diffuse, tex, current).a;
    return float4(rgb, a);
}

bool passes(uint func, float a, float ref) {
    switch (func) {
        case 1: return false;
        case 2: return a < ref;
        case 3: return a == ref;
        case 4: return a <= ref;
        case 5: return a > ref;
        case 6: return a != ref;
        case 7: return a >= ref;
    }
    return true;
}

float4 draw_ps(PsIn i) : SV_Target {
    float4 t0 = tex0.Sample(samp0, ((stage0.y >> 16) & 7) == 1 ? i.uv1 : i.uv0);
    float4 t1 = tex1.Sample(samp1, ((stage1.y >> 16) & 7) == 0 ? i.uv0 : i.uv1);
    if (colorKey != 0) {  // colour-keyed textures carry the key as alpha 0 (TextureLevels, Task 10)
        if (((stage0.y >> 24) & 1) != 0 && t0.a == 0) discard;
        if (((stage1.y >> 24) & 1) != 0 && t1.a == 0) discard;
    }
    float4 c = i.diffuse;  // stage 0 off: diffuse
    if ((stage0.x & 255) != 1) {
        c = stage(stage0, i.diffuse, i.specular, i.diffuse, t0);
        if ((stage1.x & 255) != 1) c = stage(stage1, i.diffuse, i.specular, c, t1);
    }
    if (fogEnable != 0) c.rgb = lerp(fogColor.rgb, c.rgb, i.specular.a);
    if (((alphaTest >> 16) & 1) != 0 && !passes(alphaTest & 255, round(c.a * 255.0), (alphaTest >> 8) & 255)) discard;
    return c;
}

struct PresentOut {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

PresentOut present_vs(uint id : SV_VertexID) {
    PresentOut o;
    float2 t = float2((id << 1) & 2, id & 2);  // a triangle covering the viewport
    o.pos = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = lerp(sourceRect.xy, sourceRect.zw, t);
    return o;
}

float4 present_ps(PresentOut i) : SV_Target {
    return float4(tex0.Sample(samp0, i.uv).rgb, 1);
}
)HLSL";
}

}  // namespace uagfx::gpu
```

- [ ] **Step 5: Корневая сигнатура, компиляция шейдеров, кеш самплеров, конвейер показа**

`src/win/gpu/pipeline.cpp`:
```cpp
#include <windows.h>

#include <d3d12.h>
#include <d3dcompiler.h>

#include <cstring>

#include "core/d3d7.hpp"
#include "win/gpu/impl.hpp"
#include "win/gpu/shaders.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

using SerializeRootSignatureFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION,
                                                  ID3DBlob**, ID3DBlob**);

bool compile(pD3DCompile fn, const char* entry, const char* profile, ID3DBlob*& out, std::string& error) {
    const char* src = shaderSource();
    ID3DBlob* errors = nullptr;
    const HRESULT hr = fn(src, std::strlen(src), "uaro-graphics.hlsl", nullptr, nullptr, entry, profile,
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &out, &errors);
    if (FAILED(hr)) {
        error = std::string("shader ") + entry + ": " +
                (errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
                        : std::string("compile failed"));
    }
    releaseCom(errors);
    return SUCCEEDED(hr);
}

// One D3D12 sampler from a D3D7 stage. D3D7 address modes and D3D12's share their numbers (WRAP 1 ... BORDER 4).
D3D12_SAMPLER_DESC samplerFor(const StageSetup& s) {
    D3D12_SAMPLER_DESC d{};
    const bool linearMin = s.minFilter == d3d7::TFN_LINEAR || s.minFilter == d3d7::TFN_ANISOTROPIC;
    const bool linearMag = s.magFilter == d3d7::TFG_LINEAR || s.magFilter == d3d7::TFG_ANISOTROPIC;
    const bool mips = s.mipFilter == d3d7::TFP_POINT || s.mipFilter == d3d7::TFP_LINEAR;
    const bool linearMip = s.mipFilter == d3d7::TFP_LINEAR;
    d.Filter = D3D12_FILTER((linearMin ? 0x10 : 0) | (linearMag ? 0x4 : 0) | (linearMip ? 0x1 : 0));
    auto address = [](uint8_t a) {
        return a >= d3d7::TADDRESS_WRAP && a <= d3d7::TADDRESS_BORDER ? D3D12_TEXTURE_ADDRESS_MODE(a)
                                                                       : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    };
    d.AddressU = address(s.addressU);
    d.AddressV = address(s.addressV);
    d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    d.MaxAnisotropy = 1;
    d.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    d.BorderColor[0] = float((s.borderColor >> 16) & 255) / 255.0f;
    d.BorderColor[1] = float((s.borderColor >> 8) & 255) / 255.0f;
    d.BorderColor[2] = float(s.borderColor & 255) / 255.0f;
    d.BorderColor[3] = float(s.borderColor >> 24) / 255.0f;
    d.MinLOD = 0.0f;
    d.MaxLOD = mips ? D3D12_FLOAT32_MAX : 0.0f;
    return d;
}

uint64_t samplerKey(const StageSetup& s) {
    return uint64_t(s.addressU) | uint64_t(s.addressV) << 4 | uint64_t(s.magFilter) << 8 |
           uint64_t(s.minFilter) << 12 | uint64_t(s.mipFilter) << 16 | uint64_t(s.borderColor) << 20;
}

}  // namespace

bool Renderer::Impl::createPipelines(std::string& error) {
    compiler = LoadLibraryA("d3dcompiler_47.dll");
    const auto compileFn = compiler ? reinterpret_cast<pD3DCompile>(reinterpret_cast<void*>(GetProcAddress(compiler, "D3DCompile")))
                                    : nullptr;
    if (!compileFn) {
        error = "d3dcompiler_47.dll is missing";
        return false;
    }
    if (!compile(compileFn, "draw_vs", "vs_5_0", drawVs, error) || !compile(compileFn, "draw_ps", "ps_5_0", drawPs, error) ||
        !compile(compileFn, "present_vs", "vs_5_0", presentVs, error) ||
        !compile(compileFn, "present_ps", "ps_5_0", presentPs, error))
        return false;

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 2;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_DESCRIPTOR_RANGE samplerRange{};
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.NumDescriptors = 2;
    samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER params[3] = {};
    params[kRootConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootConstants].Constants.ShaderRegister = 0;
    params[kRootConstants].Constants.Num32BitValues = kConstantCount;
    params[kRootConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[kRootTextures].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootTextures].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootTextures].DescriptorTable.pDescriptorRanges = &srvRange;
    params[kRootTextures].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[kRootSamplers].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootSamplers].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootSamplers].DescriptorTable.pDescriptorRanges = &samplerRange;
    params[kRootSamplers].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 3;
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    const auto serialize = reinterpret_cast<SerializeRootSignatureFn>(
        reinterpret_cast<void*>(GetProcAddress(d3d12, "D3D12SerializeRootSignature")));
    ID3DBlob* blob = nullptr;
    ID3DBlob* errors = nullptr;
    if (!serialize || FAILED(serialize(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors)) ||
        FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                           __uuidof(ID3D12RootSignature), reinterpret_cast<void**>(&root)))) {
        error = "root signature";
        releaseCom(blob);
        releaseCom(errors);
        return false;
    }
    releaseCom(blob);
    releaseCom(errors);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {presentVs->GetBufferPointer(), presentVs->GetBufferSize()};
    p.PS = {presentPs->GetBufferPointer(), presentPs->GetBufferSize()};
    p.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.SampleDesc.Count = 1;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState),
                                                   reinterpret_cast<void**>(&presentPipeline)))) {
        error = "present pipeline";
        return false;
    }
    return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::samplerTable(const StageSetup& s0, const StageSetup& s1) {
    const uint64_t key = samplerKey(s0) * 1000003ull ^ samplerKey(s1);
    auto it = samplerPairs.find(key);
    if (it == samplerPairs.end()) {
        if (nextSampler + 2 > kSamplerCount) {  // full: start over once the GPU is done with the old pairs
            waitIdle();
            samplerPairs.clear();
            nextSampler = 0;
            begin();
        }
        const UINT index = nextSampler;
        nextSampler += 2;
        D3D12_CPU_DESCRIPTOR_HANDLE h = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += SIZE_T(index) * samplerStep;
        const D3D12_SAMPLER_DESC d0 = samplerFor(s0);
        const D3D12_SAMPLER_DESC d1 = samplerFor(s1);
        device->CreateSampler(&d0, h);
        h.ptr += samplerStep;
        device->CreateSampler(&d1, h);
        it = samplerPairs.emplace(key, index).first;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE g = samplerHeap->GetGPUDescriptorHandleForHeapStart();
    g.ptr += UINT64(it->second) * samplerStep;
    return g;
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::textureTable(UINT srv0, UINT srv1) {
    if (ringSrv + 2 > kGpuSrvCount) ringSrv = 0;  // the ring is far larger than one submission ever needs
    D3D12_CPU_DESCRIPTOR_HANDLE dst = gpuSrv->GetCPUDescriptorHandleForHeapStart();
    dst.ptr += SIZE_T(ringSrv) * srvStep;
    device->CopyDescriptorsSimple(1, dst, cpuSrvHandle(srv0), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    dst.ptr += srvStep;
    device->CopyDescriptorsSimple(1, dst, cpuSrvHandle(srv1), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_GPU_DESCRIPTOR_HANDLE g = gpuSrv->GetGPUDescriptorHandleForHeapStart();
    g.ptr += UINT64(ringSrv) * srvStep;
    ringSrv += 2;
    return g;
}

ID3D12PipelineState* Renderer::Impl::pipelineFor(const DrawSetup&, uint32_t) { return nullptr; }  // Task 8

}  // namespace uagfx::gpu
```

`src/win/gpu/draw.cpp` (заглушка до Task 8):
```cpp
#include "win/gpu/impl.hpp"

namespace uagfx::gpu {

void Renderer::Impl::prepareGeometry(const FrameRecord&) {}

void Renderer::Impl::draw(const FrameRecord&, const DrawCmd&) {}

}  // namespace uagfx::gpu
```

- [ ] **Step 6: Устройство, цели, текстуры, чтение, показ**

`src/win/gpu/device.cpp`:
```cpp
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <cstring>

#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

UINT64 alignUp(UINT64 v, UINT64 a) { return (v + a - 1) & ~(a - 1); }

std::string narrow(const wchar_t* w) {
    char out[512];
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, sizeof out, nullptr, nullptr);
    return n > 0 ? std::string(out) : std::string();
}

ID3D12Resource* createBuffer(ID3D12Device* device, D3D12_HEAP_TYPE type, UINT64 size, D3D12_RESOURCE_STATES state) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = type;
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* r = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, state, nullptr,
                                               __uuidof(ID3D12Resource), reinterpret_cast<void**>(&r))))
        return nullptr;
    return r;
}

ID3D12Resource* createTexture2D(ID3D12Device* device, int w, int h, int levels, DXGI_FORMAT format,
                                D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE* clear) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = UINT64(w);
    d.Height = UINT(h);
    d.DepthOrArraySize = 1;
    d.MipLevels = UINT16(levels);
    d.Format = format;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d.Flags = flags;
    ID3D12Resource* r = nullptr;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, state, clear,
                                               __uuidof(ID3D12Resource), reinterpret_cast<void**>(&r))))
        return nullptr;
    return r;
}

}  // namespace

// ---- Impl: submissions -------------------------------------------------------------------------------------

bool Renderer::Impl::begin() {
    if (open) return true;
    Slot& s = slots[slot];
    waitSlot(s);
    for (IUnknown* g : s.garbage) g->Release();
    s.garbage.clear();
    s.used = 0;
    if (FAILED(s.allocator->Reset()) || FAILED(list->Reset(s.allocator, nullptr))) {
        LOG_ERROR("command list reset failed");
        return false;
    }
    ID3D12DescriptorHeap* heaps[2] = {gpuSrv, samplerHeap};
    list->SetDescriptorHeaps(2, heaps);
    boundPipeline = nullptr;
    boundTextures = ~0ull;
    boundSamplers = ~0ull;
    targetBound = false;
    open = true;
    return true;
}

void Renderer::Impl::submit() {
    if (!open) return;
    open = false;
    if (FAILED(list->Close())) {
        LOG_ERROR("command list close failed (device removed: 0x%08lx)",
                  static_cast<unsigned long>(device->GetDeviceRemovedReason()));
        return;
    }
    ID3D12CommandList* lists[1] = {list};
    queue->ExecuteCommandLists(1, lists);
    queue->Signal(fence, ++fenceValue);
    slots[slot].fence = fenceValue;
    slot = (slot + 1) % kSlots;
}

void Renderer::Impl::waitSlot(Slot& s) {
    if (s.fence && fence->GetCompletedValue() < s.fence) {
        fence->SetEventOnCompletion(s.fence, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

void Renderer::Impl::waitIdle() {
    submit();
    queue->Signal(fence, ++fenceValue);
    if (fence->GetCompletedValue() < fenceValue) {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}

bool Renderer::Impl::growUpload(Slot& s, UINT64 size) {
    if (s.upload) s.garbage.push_back(s.upload);  // still used by what this slot recorded so far
    s.upload = createBuffer(device, D3D12_HEAP_TYPE_UPLOAD, size, D3D12_RESOURCE_STATE_GENERIC_READ);
    s.mapped = nullptr;
    s.size = s.upload ? size : 0;
    s.used = 0;
    if (!s.upload) return false;
    D3D12_RANGE none{0, 0};
    return SUCCEEDED(s.upload->Map(0, &none, reinterpret_cast<void**>(&s.mapped)));
}

Upload Renderer::Impl::allocUpload(UINT64 size, UINT64 alignment) {
    Slot& s = slots[slot];
    UINT64 at = alignUp(s.used, alignment);
    if (at + size > s.size) {
        growUpload(s, std::max(s.size * 2, alignUp(size, 1 << 20) + (1 << 20)));
        at = 0;
    }
    s.used = at + size;
    return Upload{s.mapped + at, s.upload->GetGPUVirtualAddress() + at, at};
}

void Renderer::Impl::transition(ID3D12Resource* r, D3D12_RESOURCE_STATES& state, D3D12_RESOURCE_STATES to) {
    if (state == to) return;
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = state;
    b.Transition.StateAfter = to;
    list->ResourceBarrier(1, &b);
    state = to;
}

UINT Renderer::Impl::allocSrv() {
    if (!freeSrv.empty()) {
        const UINT i = freeSrv.back();
        freeSrv.pop_back();
        return i;
    }
    return nextSrv < kCpuSrvCount ? nextSrv++ : kNoSrv;
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::Impl::cpuSrvHandle(UINT i) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = cpuSrv->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(i) * srvStep;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::Impl::rtvHandle(UINT i) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(i) * rtvStep;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::Impl::dsvHandle() const { return dsvHeap->GetCPUDescriptorHandleForHeapStart(); }

void Renderer::Impl::bindTarget() {
    transition(target, targetState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (targetBound) return;
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandle(0);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHandle();
    list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &scissor);
    targetBound = true;
}

// ---- Impl: frame record commands ---------------------------------------------------------------------------

void Renderer::Impl::clear(const ClearCmd& c) {
    D3D12_RECT r{c.rect.left, c.rect.top, c.rect.right, c.rect.bottom};
    r.left = std::max<LONG>(r.left, 0);
    r.top = std::max<LONG>(r.top, 0);
    r.right = std::min<LONG>(r.right, width);
    r.bottom = std::min<LONG>(r.bottom, height);
    if (r.right <= r.left || r.bottom <= r.top) return;
    bindTarget();
    if (c.flags & 1) {  // D3DCLEAR_TARGET
        const float color[4] = {float((c.argb >> 16) & 255) / 255.0f, float((c.argb >> 8) & 255) / 255.0f,
                                float(c.argb & 255) / 255.0f, 1.0f};
        list->ClearRenderTargetView(rtvHandle(0), color, 1, &r);
    }
    if (c.flags & 2) list->ClearDepthStencilView(dsvHandle(), D3D12_CLEAR_FLAG_DEPTH, c.z, 0, 1, &r);  // ZBUFFER
}

void Renderer::Impl::setViewport(const ViewportCmd& v) {
    viewport = D3D12_VIEWPORT{float(v.rect.left), float(v.rect.top), float(v.rect.width()), float(v.rect.height()),
                              v.minZ, v.maxZ};
    scissor = D3D12_RECT{v.rect.left, v.rect.top, v.rect.right, v.rect.bottom};
    if (targetBound) {
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
    }
}

namespace {

// Copies width x height ARGB values into a placed footprint (rows aligned to 256 bytes) in the upload buffer.
D3D12_TEXTURE_COPY_LOCATION stage(Renderer::Impl& m, const uint32_t* argb, UINT w, UINT h) {
    const UINT pitch = UINT(alignUp(UINT64(w) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
    const Upload u = m.allocUpload(UINT64(pitch) * h, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    for (UINT y = 0; y < h; ++y) std::memcpy(u.cpu + UINT64(y) * pitch, argb + UINT64(y) * w, size_t(w) * 4);
    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = m.slots[m.slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = u.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    src.PlacedFootprint.Footprint.Width = w;
    src.PlacedFootprint.Footprint.Height = h;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = pitch;
    return src;
}

}  // namespace

void Renderer::Impl::upload(const FrameRecord& rec, const UploadCmd& u) {
    auto it = textures.find(u.texture);
    if (it == textures.end() || u.rect.empty()) return;
    Texture& t = it->second;
    const D3D12_TEXTURE_COPY_LOCATION src = stage(*this, rec.pixels().data() + u.pixels, UINT(u.rect.width()),
                                                  UINT(u.rect.height()));
    transition(t.resource, t.state, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = t.resource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = u.level;
    list->CopyTextureRegion(&dst, UINT(u.rect.left), UINT(u.rect.top), 0, &src, nullptr);
}

void Renderer::Impl::writeTarget(const FrameRecord& rec, const WriteTargetCmd& w) {
    if (w.rect.empty()) return;
    const D3D12_TEXTURE_COPY_LOCATION src = stage(*this, rec.pixels().data() + w.pixels, UINT(w.rect.width()),
                                                  UINT(w.rect.height()));
    transition(target, targetState, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = target;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    list->CopyTextureRegion(&dst, UINT(w.rect.left), UINT(w.rect.top), 0, &src, nullptr);
}

// ---- Impl: swap chain --------------------------------------------------------------------------------------

void Renderer::Impl::releaseSwapChain() {
    for (ID3D12Resource*& b : backBuffers) releaseCom(b);
    releaseCom(swapChain);
    window = nullptr;
    swapWidth = swapHeight = 0;
}

bool Renderer::Impl::ensureSwapChain(HWND hwnd, bool wantTearing) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const UINT w = UINT(std::max<LONG>(1, rc.right - rc.left));
    const UINT h = UINT(std::max<LONG>(1, rc.bottom - rc.top));
    if (swapChain && hwnd == window && w == swapWidth && h == swapHeight) return true;
    waitIdle();
    const UINT flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    if (swapChain && hwnd == window) {  // the window changed size
        for (ID3D12Resource*& b : backBuffers) releaseCom(b);
        if (FAILED(swapChain->ResizeBuffers(2, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, flags))) {
            LOG_ERROR("swap chain resize to %ux%u failed", w, h);
            releaseSwapChain();
            return false;
        }
    } else {
        releaseSwapChain();
        IDXGIFactory5* f5 = nullptr;
        BOOL allow = FALSE;
        if (wantTearing && SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory5), reinterpret_cast<void**>(&f5)))) {
            if (FAILED(f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof allow))) allow = FALSE;
            f5->Release();
        }
        tearingSupported = allow == TRUE;
        DXGI_SWAP_CHAIN_DESC1 d{};
        d.Width = w;
        d.Height = h;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d.BufferCount = 2;
        d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        d.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        d.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        IDXGISwapChain1* sc1 = nullptr;
        if (FAILED(factory->CreateSwapChainForHwnd(queue, hwnd, &d, nullptr, nullptr, &sc1)) ||
            FAILED(sc1->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain)))) {
            LOG_ERROR("cannot create a swap chain for the game window");
            releaseCom(sc1);
            return false;
        }
        sc1->Release();
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        window = hwnd;
        LOG_INFO("swap chain %ux%u%s", w, h, tearingSupported ? ", tearing allowed" : "");
    }
    for (UINT i = 0; i < 2; ++i) {
        if (FAILED(swapChain->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&backBuffers[i])))) {
            releaseSwapChain();
            return false;
        }
        device->CreateRenderTargetView(backBuffers[i], nullptr, rtvHandle(1 + i));
        backStates[i] = D3D12_RESOURCE_STATE_PRESENT;
    }
    swapWidth = w;
    swapHeight = h;
    return true;
}

void Renderer::Impl::destroy() {
    if (device && fence) waitIdle();
    releaseSwapChain();
    for (auto& t : textures) releaseCom(t.second.resource);
    textures.clear();
    for (auto& p : pipelines) releaseCom(p.second);
    pipelines.clear();
    releaseCom(presentPipeline);
    releaseCom(root);
    releaseCom(drawVs);
    releaseCom(drawPs);
    releaseCom(presentVs);
    releaseCom(presentPs);
    releaseCom(readback);
    releaseCom(target);
    releaseCom(depth);
    for (Slot& s : slots) {
        for (IUnknown* g : s.garbage) g->Release();
        s.garbage.clear();
        releaseCom(s.upload);
        releaseCom(s.allocator);
    }
    releaseCom(list);
    releaseCom(cpuSrv);
    releaseCom(gpuSrv);
    releaseCom(samplerHeap);
    releaseCom(rtvHeap);
    releaseCom(dsvHeap);
    releaseCom(fence);
    if (fenceEvent) CloseHandle(fenceEvent);
    fenceEvent = nullptr;
    releaseCom(queue);
    releaseCom(device);
    releaseCom(factory);
}

// ---- Renderer -------------------------------------------------------------------------------------------------

Renderer::Renderer() : impl_(new Impl) {}

Renderer::~Renderer() {
    impl_->destroy();
    delete impl_;
}

std::string Renderer::adapterName() const { return impl_->adapter; }
int Renderer::targetWidth() const { return impl_->width; }
int Renderer::targetHeight() const { return impl_->height; }

bool Renderer::create(std::string& error) {
    Impl& m = *impl_;
    m.d3d12 = LoadLibraryA("d3d12.dll");
    m.dxgi = LoadLibraryA("dxgi.dll");
    if (!m.d3d12 || !m.dxgi) {
        error = "d3d12.dll or dxgi.dll is missing";
        return false;
    }
    using CreateFactory2Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);
    const auto createFactory = reinterpret_cast<CreateFactory2Fn>(
        reinterpret_cast<void*>(GetProcAddress(m.dxgi, "CreateDXGIFactory2")));
    const auto createDevice = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
        reinterpret_cast<void*>(GetProcAddress(m.d3d12, "D3D12CreateDevice")));
    if (!createFactory || !createDevice ||
        FAILED(createFactory(0, __uuidof(IDXGIFactory4), reinterpret_cast<void**>(&m.factory)))) {
        error = "no DXGI 1.4 factory";
        return false;
    }
    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; !m.device && m.factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 d{};
        adapter->GetDesc1(&d);
        if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&  // WARP / Basic Render: not worth it
            SUCCEEDED(createDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                                   reinterpret_cast<void**>(&m.device))))
            m.adapter = narrow(d.Description);
        adapter->Release();
    }
    if (!m.device) {
        error = "no hardware adapter with Direct3D 12";
        return false;
    }
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(m.device->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&m.queue)))) {
        error = "command queue";
        return false;
    }
    for (Slot& s : m.slots) {
        if (FAILED(m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                    reinterpret_cast<void**>(&s.allocator))) ||
            !m.growUpload(s, kUploadSize)) {
            error = "command allocators / upload buffers";
            return false;
        }
    }
    if (FAILED(m.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m.slots[0].allocator, nullptr,
                                           __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&m.list)))) {
        error = "command list";
        return false;
    }
    m.list->Close();
    if (FAILED(m.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&m.fence))) ||
        !(m.fenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr))) {
        error = "fence";
        return false;
    }
    auto heap = [&](D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool visible, ID3D12DescriptorHeap*& out) {
        D3D12_DESCRIPTOR_HEAP_DESC d{};
        d.Type = type;
        d.NumDescriptors = count;
        d.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        return SUCCEEDED(m.device->CreateDescriptorHeap(&d, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&out)));
    };
    if (!heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kCpuSrvCount, false, m.cpuSrv) ||
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kGpuSrvCount, true, m.gpuSrv) ||
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, kSamplerCount, true, m.samplerHeap) ||
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3, false, m.rtvHeap) ||
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false, m.dsvHeap)) {
        error = "descriptor heaps";
        return false;
    }
    m.srvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m.samplerStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    m.rtvStep = m.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m.targetSrv = m.allocSrv();
    if (!m.createPipelines(error)) return false;

    // The texture bound wherever the client binds none: D3D7 reads it as white (states.md §5).
    m.white = createTexture(1, 1, 1);
    const uint32_t whitePixel = 0xFFFFFFFF;
    FrameRecord rec;
    rec.upload(m.white, 0, Rect{0, 0, 1, 1}, &whitePixel);
    if (!m.white || !m.begin()) {
        error = "white texture";
        return false;
    }
    m.upload(rec, rec.uploads()[0]);
    m.waitIdle();
    return true;
}

bool Renderer::setTarget(int width, int height) {
    Impl& m = *impl_;
    if (width <= 0 || height <= 0) return false;
    if (width == m.width && height == m.height && m.target) return true;
    m.waitIdle();
    releaseCom(m.target);
    releaseCom(m.depth);
    D3D12_CLEAR_VALUE black{};
    black.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    black.Color[3] = 1.0f;
    m.target = createTexture2D(m.device, width, height, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                               D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, &black);
    D3D12_CLEAR_VALUE farthest{};  // not "far": windef.h defines far as an empty macro
    farthest.Format = DXGI_FORMAT_D32_FLOAT;
    farthest.DepthStencil.Depth = 1.0f;
    m.depth = createTexture2D(m.device, width, height, 1, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                              D3D12_RESOURCE_STATE_DEPTH_WRITE, &farthest);
    if (!m.target || !m.depth) {
        LOG_ERROR("cannot create a %dx%d render target", width, height);
        return false;
    }
    m.targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m.device->CreateRenderTargetView(m.target, nullptr, m.rtvHandle(0));
    m.device->CreateDepthStencilView(m.depth, nullptr, m.dsvHandle());
    m.device->CreateShaderResourceView(m.target, nullptr, m.cpuSrvHandle(m.targetSrv));
    m.width = width;
    m.height = height;
    m.viewport = D3D12_VIEWPORT{0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f};
    m.scissor = D3D12_RECT{0, 0, width, height};
    if (!m.begin()) return false;
    const float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m.list->ClearRenderTargetView(m.rtvHandle(0), color, 0, nullptr);
    m.list->ClearDepthStencilView(m.dsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m.submit();
    LOG_INFO("render target %dx%d", width, height);
    return true;
}

uint32_t Renderer::createTexture(int width, int height, int levels) {
    Impl& m = *impl_;
    Texture t;
    t.width = width;
    t.height = height;
    t.levels = std::max(1, levels);
    t.srv = m.allocSrv();
    if (t.srv == kNoSrv) {  // index 0 is the target's: never hand out a slot that is in use
        LOG_ERROR("no texture descriptor left for a %dx%d texture (%u textures alive)", width, height,
                  static_cast<unsigned>(m.textures.size()));
        return 0;
    }
    t.resource = createTexture2D(m.device, width, height, t.levels, DXGI_FORMAT_B8G8R8A8_UNORM,
                                 D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
    if (!t.resource) {
        m.freeSrv.push_back(t.srv);
        return 0;
    }
    m.device->CreateShaderResourceView(t.resource, nullptr, m.cpuSrvHandle(t.srv));
    const uint32_t id = m.nextTexture++;
    m.textures.emplace(id, t);
    return id;
}

void Renderer::releaseTexture(uint32_t id) {
    Impl& m = *impl_;
    auto it = m.textures.find(id);
    if (it == m.textures.end()) return;
    m.slots[m.slot].garbage.push_back(it->second.resource);  // the GPU may still read it
    m.freeSrv.push_back(it->second.srv);
    m.textures.erase(it);
}

void Renderer::execute(const FrameRecord& record) {
    Impl& m = *impl_;
    if (!m.target || record.empty() || !m.begin()) return;
    m.prepareGeometry(record);
    for (const Command& c : record.commands()) {
        switch (c.type) {
            case Cmd::Clear: m.clear(record.clears()[c.index]); break;
            case Cmd::Viewport: m.setViewport(record.viewports()[c.index]); break;
            case Cmd::Upload: m.upload(record, record.uploads()[c.index]); break;
            case Cmd::WriteTarget: m.writeTarget(record, record.writes()[c.index]); break;
            case Cmd::Draw: m.draw(record, record.draws()[c.index]); break;
        }
    }
    m.submit();
}

bool Renderer::readTarget(std::vector<uint32_t>& argb) {
    Impl& m = *impl_;
    if (!m.target || !m.begin()) return false;
    const UINT pitch = UINT(alignUp(UINT64(m.width) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
    const UINT64 size = UINT64(pitch) * UINT64(m.height);
    if (m.readbackSize < size) {
        releaseCom(m.readback);
        m.readback = createBuffer(m.device, D3D12_HEAP_TYPE_READBACK, size, D3D12_RESOURCE_STATE_COPY_DEST);
        m.readbackSize = m.readback ? size : 0;
        if (!m.readback) return false;
    }
    m.transition(m.target, m.targetState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = m.readback;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Offset = 0;
    dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    dst.PlacedFootprint.Footprint.Width = UINT(m.width);
    dst.PlacedFootprint.Footprint.Height = UINT(m.height);
    dst.PlacedFootprint.Footprint.Depth = 1;
    dst.PlacedFootprint.Footprint.RowPitch = pitch;
    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = m.target;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;
    m.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    m.waitIdle();
    uint8_t* data = nullptr;
    D3D12_RANGE all{0, SIZE_T(size)};
    if (FAILED(m.readback->Map(0, &all, reinterpret_cast<void**>(&data)))) return false;
    argb.resize(size_t(m.width) * size_t(m.height));
    for (int y = 0; y < m.height; ++y)
        std::memcpy(argb.data() + size_t(y) * size_t(m.width), data + UINT64(y) * pitch, size_t(m.width) * 4);
    D3D12_RANGE none{0, 0};
    m.readback->Unmap(0, &none);
    return true;
}

void Renderer::present(HWND window, const Rect& source, const PresentOptions& options) {
    Impl& m = *impl_;
    if (!m.target || !window || source.empty()) return;
    if (!m.ensureSwapChain(window, options.allowTearing) || !m.begin()) return;
    const UINT index = m.swapChain->GetCurrentBackBufferIndex();
    m.transition(m.backBuffers[index], m.backStates[index], D3D12_RESOURCE_STATE_RENDER_TARGET);
    m.transition(m.target, m.targetState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m.rtvHandle(1 + index);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m.list->ClearRenderTargetView(rtv, black, 0, nullptr);
    // Fit the source into the window, keeping its shape: bars where the shapes differ.
    const float sw = float(source.width()), sh = float(source.height());
    const float scale = std::min(float(m.swapWidth) / sw, float(m.swapHeight) / sh);
    D3D12_VIEWPORT vp{(float(m.swapWidth) - sw * scale) / 2, (float(m.swapHeight) - sh * scale) / 2, sw * scale,
                      sh * scale, 0.0f, 1.0f};
    D3D12_RECT all{0, 0, LONG(m.swapWidth), LONG(m.swapHeight)};
    m.list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    m.list->RSSetViewports(1, &vp);
    m.list->RSSetScissorRects(1, &all);
    m.list->SetGraphicsRootSignature(m.root);
    m.list->SetPipelineState(m.presentPipeline);
    Constants c{};
    c.sourceRect[0] = float(source.left) / float(m.width);
    c.sourceRect[1] = float(source.top) / float(m.height);
    c.sourceRect[2] = float(source.right) / float(m.width);
    c.sourceRect[3] = float(source.bottom) / float(m.height);
    m.list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    m.list->SetGraphicsRootDescriptorTable(kRootTextures, m.textureTable(m.targetSrv, m.targetSrv));
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = 3;  // CLAMP
    linearClamp.magFilter = linearClamp.minFilter = 2;  // LINEAR
    linearClamp.mipFilter = 1;                          // NONE
    m.list->SetGraphicsRootDescriptorTable(kRootSamplers, m.samplerTable(linearClamp, linearClamp));
    m.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m.list->DrawInstanced(3, 1, 0, 0);
    m.transition(m.backBuffers[index], m.backStates[index], D3D12_RESOURCE_STATE_PRESENT);
    m.boundPipeline = nullptr;  // the next draws rebind everything
    m.boundTextures = m.boundSamplers = ~0ull;
    m.targetBound = false;
    m.submit();

    if (options.fpsLimit > 0) {  // simple pacing: wait until 1/limit seconds after the previous present
        LARGE_INTEGER now, freq;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&now);
        const LONGLONG period = freq.QuadPart / options.fpsLimit;
        if (m.lastPresent.QuadPart) {
            const LONGLONG wait = m.lastPresent.QuadPart + period - now.QuadPart;
            if (wait > 0) Sleep(DWORD(wait * 1000 / freq.QuadPart));
        }
        QueryPerformanceCounter(&m.lastPresent);
    }
    const UINT sync = options.vsync ? 1 : 0;
    const UINT flags = !options.vsync && options.allowTearing && m.tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
    const HRESULT hr = m.swapChain->Present(sync, flags);
    if (FAILED(hr)) LOG_ERROR("Present failed: 0x%08lx", static_cast<unsigned long>(hr));
}

}  // namespace uagfx::gpu
```

- [ ] **Step 7: Собирается**

Run: `cmake -S . -B build-win --no-warn-unused-cli -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build-win -j && python3 tools/check_dll.py`
Expected: сборка без предупреждений; `OK: build-win/ddraw.dll: 22 exports, …` — импорты по-прежнему только системные
(D3D12, DXGI, `d3dcompiler_47` — через `LoadLibrary`). Рендер пока никто не вызывает.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt src/win/gpu
git commit -m "Рендер D3D12: устройство, RT клиента, текстуры, загрузки, чтение кадра, показ в окне с полями

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 8: Рендер D3D12 — рисование: конвейеры по состоянию D3D7 и вызовы Draw

**Files:**
- Modify: `src/win/gpu/pipeline.cpp` (функция `pipelineFor`)
- Modify: `src/win/gpu/draw.cpp` (целиком)

**Interfaces:**
- Consumes: `Renderer::Impl` (Task 7), `DrawSetup`, `StageSetup` (Task 1), `FrameRecord::stride` (Task 2).
- Produces: рабочие `Renderer::Impl::pipelineFor`, `prepareGeometry`, `draw` — `Renderer::execute` рисует.

- [ ] **Step 1: Кеш конвейеров**

В `src/win/gpu/pipeline.cpp` заменить строку
```cpp
ID3D12PipelineState* Renderer::Impl::pipelineFor(const DrawSetup&, uint32_t) { return nullptr; }  // Task 8
```
на:
```cpp
// D3D7 and D3D12 share the numbers of blend factors (ZERO 1 ... SRCALPHASAT 11), comparison functions
// (NEVER 1 ... ALWAYS 8) and cull modes (NONE 1, CW = FRONT 2, CCW = BACK 3 with clockwise front faces), and both
// count winding in window space (y down), so the states map one to one.
ID3D12PipelineState* Renderer::Impl::pipelineFor(const DrawSetup& s, uint32_t fvf) {
    const bool two = fvf == d3d7::FVF_TL2;
    const uint64_t key = uint64_t(s.zEnable) | uint64_t(s.zWrite) << 1 | uint64_t(s.zFunc & 15) << 2 |
                         uint64_t(s.cull & 3) << 6 | uint64_t(s.blend) << 8 | uint64_t(s.srcBlend & 15) << 9 |
                         uint64_t(s.dstBlend & 15) << 13 | uint64_t(two) << 17;
    auto it = pipelines.find(key);
    if (it != pipelines.end()) return it->second;

    // FVF 0x1C4 has one UV set: TEXCOORD1 reads the same bytes as TEXCOORD0.
    const D3D12_INPUT_ELEMENT_DESC layout[5] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, two ? 32u : 24u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};
    p.pRootSignature = root;
    p.VS = {drawVs->GetBufferPointer(), drawVs->GetBufferSize()};
    p.PS = {drawPs->GetBufferPointer(), drawPs->GetBufferSize()};
    D3D12_RENDER_TARGET_BLEND_DESC& b = p.BlendState.RenderTarget[0];
    b.BlendEnable = s.blend ? TRUE : FALSE;
    b.SrcBlend = D3D12_BLEND(s.srcBlend ? uint32_t(s.srcBlend) : uint32_t(d3d7::BLEND_ONE));
    b.DestBlend = D3D12_BLEND(s.dstBlend ? uint32_t(s.dstBlend) : uint32_t(d3d7::BLEND_ZERO));
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ONE;  // the target's alpha is never read (DESTALPHA reads 1): keep it simple
    b.DestBlendAlpha = D3D12_BLEND_ZERO;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
    p.SampleMask = UINT_MAX;
    p.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    p.RasterizerState.CullMode = s.cull >= d3d7::CULL_NONE && s.cull <= d3d7::CULL_CCW ? D3D12_CULL_MODE(s.cull)
                                                                                        : D3D12_CULL_MODE_NONE;
    p.RasterizerState.FrontCounterClockwise = FALSE;
    p.RasterizerState.DepthClipEnable = TRUE;
    p.DepthStencilState.DepthEnable = s.zEnable ? TRUE : FALSE;
    p.DepthStencilState.DepthWriteMask = s.zWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    p.DepthStencilState.DepthFunc = s.zFunc >= d3d7::CMP_NEVER && s.zFunc <= d3d7::CMP_ALWAYS
                                        ? D3D12_COMPARISON_FUNC(s.zFunc)
                                        : D3D12_COMPARISON_FUNC_ALWAYS;
    p.InputLayout = {layout, 5};
    p.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    p.NumRenderTargets = 1;
    p.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    p.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    p.SampleDesc.Count = 1;
    ID3D12PipelineState* pso = nullptr;
    if (FAILED(device->CreateGraphicsPipelineState(&p, __uuidof(ID3D12PipelineState), reinterpret_cast<void**>(&pso)))) {
        LOG_ERROR("pipeline for key 0x%llx failed", static_cast<unsigned long long>(key));
        pso = nullptr;
    }
    pipelines.emplace(key, pso);  // a failure is remembered too: no retry every draw
    return pso;
}
```

- [ ] **Step 2: Геометрия и рисование**

`src/win/gpu/draw.cpp` заменить целиком:
```cpp
#include <cstring>

#include "core/d3d7.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

void packStage(const StageSetup& st, bool keyed, uint32_t out[2]) {
    out[0] = uint32_t(st.colorOp) | uint32_t(st.colorArg1) << 8 | uint32_t(st.colorArg2) << 16 | uint32_t(st.alphaOp) << 24;
    out[1] = uint32_t(st.alphaArg1) | uint32_t(st.alphaArg2) << 8 | uint32_t(st.texCoord) << 16 | uint32_t(keyed ? 1 : 0) << 24;
}

}  // namespace

void Renderer::Impl::prepareGeometry(const FrameRecord& rec) {
    vertexBase = 0;
    indexBase = 0;
    indexBytes = 0;
    const std::vector<uint8_t>& v = rec.vertexBytes();
    if (!v.empty()) {
        const Upload u = allocUpload(v.size(), 16);
        std::memcpy(u.cpu, v.data(), v.size());
        vertexBase = u.gpu;
    }
    const std::vector<uint16_t>& i = rec.indices();
    if (!i.empty()) {
        indexBytes = UINT(i.size() * sizeof(uint16_t));
        const Upload u = allocUpload(indexBytes, 4);
        std::memcpy(u.cpu, i.data(), indexBytes);
        indexBase = u.gpu;
    }
}

void Renderer::Impl::draw(const FrameRecord& rec, const DrawCmd& d) {
    if (d.vertexCount == 0 || !vertexBase) return;
    if (d.primitive != d3d7::PT_TRIANGLELIST && d.primitive != d3d7::PT_TRIANGLESTRIP) {
        static bool logged = false;  // the client only draws lists and strips (states.md §4)
        if (!logged) LOG_INFO("primitive type %u not drawn", static_cast<unsigned>(d.primitive));
        logged = true;
        return;
    }
    const DrawSetup& s = rec.states()[d.state];
    ID3D12PipelineState* pso = pipelineFor(s, d.fvf);
    if (!pso) return;
    Texture* t[2] = {nullptr, nullptr};
    for (int i = 0; i < 2; ++i) {
        auto it = textures.find(d.texture[i] ? d.texture[i] : white);
        if (it == textures.end()) it = textures.find(white);
        if (it == textures.end()) return;
        t[i] = &it->second;
        transition(t[i]->resource, t[i]->state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(s.stage[0], s.stage[1]);  // may restart the list
    bindTarget();
    if (!boundPipeline) {  // a fresh list or after present(): the root signature and its tables are gone
        list->SetGraphicsRootSignature(root);
        boundTextures = ~0ull;
        boundSamplers = ~0ull;
    }
    if (pso != boundPipeline) {
        list->SetPipelineState(pso);
        boundPipeline = pso;
    }
    const uint64_t textureKey = uint64_t(t[0]->srv) << 32 | t[1]->srv;
    if (textureKey != boundTextures) {
        list->SetGraphicsRootDescriptorTable(kRootTextures, textureTable(t[0]->srv, t[1]->srv));
        boundTextures = textureKey;
    }
    if (samplers.ptr != boundSamplers) {
        list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
        boundSamplers = samplers.ptr;
    }
    Constants c{};
    c.targetSize[0] = float(width);
    c.targetSize[1] = float(height);
    packStage(s.stage[0], d.keyed[0], c.stage0);
    packStage(s.stage[1], d.keyed[1], c.stage1);
    c.alphaTest = s.alphaTest ? uint32_t(s.alphaFunc) | uint32_t(s.alphaRef) << 8 | 1u << 16 : 0u;
    c.fogEnable = s.fog ? 1u : 0u;
    c.fogColor[0] = float((s.fogColor >> 16) & 255) / 255.0f;
    c.fogColor[1] = float((s.fogColor >> 8) & 255) / 255.0f;
    c.fogColor[2] = float(s.fogColor & 255) / 255.0f;
    c.fogColor[3] = 1.0f;
    c.colorKey = s.colorKey ? 1u : 0u;
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->IASetPrimitiveTopology(d.primitive == d3d7::PT_TRIANGLESTRIP ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                                                                       : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride = FrameRecord::stride(d.fvf);
    D3D12_VERTEX_BUFFER_VIEW vb{vertexBase + d.vertexOffset, d.vertexCount * stride, stride};
    list->IASetVertexBuffers(0, 1, &vb);
    if (d.indexCount) {
        D3D12_INDEX_BUFFER_VIEW ib{indexBase, indexBytes, DXGI_FORMAT_R16_UINT};
        list->IASetIndexBuffer(&ib);
        list->DrawIndexedInstanced(d.indexCount, 1, d.firstIndex, 0, 0);
    } else {
        list->DrawInstanced(d.vertexCount, 1, 0, 0);
    }
}

}  // namespace uagfx::gpu
```

- [ ] **Step 3: Собирается**

Run: `cmake --build build-win -j && python3 tools/check_dll.py`
Expected: без предупреждений; `OK: build-win/ddraw.dll: 22 exports, …`.

- [ ] **Step 4: Commit**

```bash
git add src/win/gpu/pipeline.cpp src/win/gpu/draw.cpp
git commit -m "Рендер D3D12: конвейеры по состоянию D3D7, текстуры и самплеры по стадиям, рисование вершин XYZRHW

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 9: Контекст драйвера, копия RT на защищённых страницах, режим окна, форматы DirectDraw

**Files:**
- Create: `src/win/context.hpp`, `src/win/context.cpp`, `src/win/shadow.hpp`, `src/win/shadow.cpp`,
  `src/win/window_mode.hpp`, `src/win/window_mode.cpp`, `src/win/dx7/guids.cpp`, `src/win/dx7/formats.hpp`,
  `src/win/dx7/formats.cpp`

**Interfaces:**
- Consumes: `gpu::Renderer` (Tasks 7–8), `FrameRecord` (Task 2), `ShadowState` (Task 6), `Config` (1A),
  `PixelFormat`, `toArgb8`, `targetToClient`, `readPixel`, `writePixel`, `masksOf`, `formatFromMasks`.
- Produces:
  ```cpp
  namespace uagfx::win {
  class Shadow {
  public:
      bool resize(int width, int height, PixelFormat format);
      int width() const; int height() const; int pitch() const; PixelFormat format() const; uint8_t* data();
      uint8_t* lock(); bool unlock(); bool locked() const; void gpuChanged();
      void fill(const std::vector<uint32_t>& argb); void toArgb(std::vector<uint32_t>& argb) const;
      bool contains(const void* p) const; void fault(bool write);
  };
  class Context {
  public:
      static Context& get(); void lock(); void unlock();
      Config config; gpu::Renderer* renderer; FrameRecord record;
      int screenWidth, screenHeight; PixelFormat screenFormat; HWND window; bool windowApplied; bool vblankWaited;
      Shadow shadow;
      bool open(std::string& error); void close();
      bool setTarget(int width, int height); void flush(); bool readTarget(std::vector<uint32_t>& argb);
      void writeTarget(const Rect& rect, const uint32_t* argb); void present(const Rect& source, bool waitForVblank);
  };
  class Guard;  // RAII: Context::get().lock() / unlock()
  void applyWindowMode(HWND hwnd, const Config& config, int gameWidth, int gameHeight);
  }
  namespace uagfx::dx7 {
  void fillPixelFormat(DDPIXELFORMAT& pf, PixelFormat f);
  PixelFormat readPixelFormat(const DDPIXELFORMAT& pf);
  void fillSurfaceDesc(DDSURFACEDESC2& d, int width, int height, int pitch, PixelFormat f, DWORD caps, void* bits);
  }
  ```

- [ ] **Step 1: Идентификаторы интерфейсов**

`src/win/dx7/guids.cpp` — единственное место, где определяются GUID из `ddraw.h`/`d3d.h` (`IID_IDirectDraw7`,
`IID_IDirectDrawSurface7`, `IID_IDirect3D7`, `IID_IDirect3DHALDevice`, …):
```cpp
// Defines the GUIDs that ddraw.h and d3d.h declare with DEFINE_GUID; every other file only refers to them.
#include <windows.h>

#include <initguid.h>

#include <ddraw.h>

#include <d3d.h>
```

- [ ] **Step 2: Форматы DirectDraw**

`src/win/dx7/formats.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <ddraw.h>

#include "core/pixel.hpp"

namespace uagfx::dx7 {

void fillPixelFormat(DDPIXELFORMAT& pf, PixelFormat f);
PixelFormat readPixelFormat(const DDPIXELFORMAT& pf);  // Unknown when it is not one of ours
// What Lock and GetSurfaceDesc report (surfaces-present-caps.md §2: the full size and pitch, the format).
void fillSurfaceDesc(DDSURFACEDESC2& d, int width, int height, int pitch, PixelFormat f, DWORD caps, void* bits);

}  // namespace uagfx::dx7
```

`src/win/dx7/formats.cpp`:
```cpp
#include "win/dx7/formats.hpp"

#include <cstring>

namespace uagfx::dx7 {

void fillPixelFormat(DDPIXELFORMAT& pf, PixelFormat f) {
    std::memset(&pf, 0, sizeof pf);
    pf.dwSize = sizeof pf;
    const Masks m = masksOf(f);
    pf.dwFlags = DDPF_RGB | (m.a ? DDPF_ALPHAPIXELS : 0);
    pf.dwRGBBitCount = m.bits;
    pf.dwRBitMask = m.r;
    pf.dwGBitMask = m.g;
    pf.dwBBitMask = m.b;
    pf.dwRGBAlphaBitMask = m.a;
}

PixelFormat readPixelFormat(const DDPIXELFORMAT& pf) {
    if (!(pf.dwFlags & DDPF_RGB)) return PixelFormat::Unknown;
    return formatFromMasks(pf.dwRGBBitCount, pf.dwRBitMask, pf.dwGBitMask, pf.dwBBitMask,
                           (pf.dwFlags & DDPF_ALPHAPIXELS) ? pf.dwRGBAlphaBitMask : 0);
}

void fillSurfaceDesc(DDSURFACEDESC2& d, int width, int height, int pitch, PixelFormat f, DWORD caps, void* bits) {
    const DWORD size = d.dwSize ? d.dwSize : sizeof d;
    std::memset(&d, 0, sizeof d);
    d.dwSize = size;
    d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | (bits ? DDSD_LPSURFACE : 0);
    d.dwWidth = DWORD(width);
    d.dwHeight = DWORD(height);
    d.lPitch = pitch;
    d.lpSurface = bits;
    fillPixelFormat(d.ddpfPixelFormat, f);
    d.ddsCaps.dwCaps = caps;
}

}  // namespace uagfx::dx7
```

- [ ] **Step 3: Копия RT для Lock**

`src/win/shadow.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <cstdint>
#include <vector>

#include "core/pixel.hpp"
#include "core/shadow_state.hpp"

namespace uagfx::win {

// The memory Lock and GetDC hand out for the render target, in the client's screen format. Its pages are made
// inaccessible on Lock; the first read or write traps into a vectored exception handler that reads the GPU target
// back first (ShadowState decides). The client locks the target every frame without touching it, so most locks
// cost nothing.
class Shadow {
public:
    ~Shadow();
    bool resize(int width, int height, PixelFormat format);
    int width() const { return w_; }
    int height() const { return h_; }
    int pitch() const { return pitch_; }
    PixelFormat format() const { return f_; }
    uint8_t* data() { return mem_; }

    uint8_t* lock();
    bool unlock();  // true when the client wrote: upload the memory into the target
    bool locked() const { return state_.locked(); }
    void gpuChanged() { state_.gpuChanged(); }

    void fill(const std::vector<uint32_t>& argb);    // GPU target -> memory
    void toArgb(std::vector<uint32_t>& argb) const;  // memory -> GPU target
    bool contains(const void* p) const;
    void fault(bool write);  // called by the exception handler, under the global lock

private:
    void protect(DWORD mode);

    uint8_t* mem_ = nullptr;
    size_t size_ = 0;
    int w_ = 0;
    int h_ = 0;
    int pitch_ = 0;
    PixelFormat f_ = PixelFormat::Unknown;
    ShadowState state_;
};

void installShadowHandler();  // once per process

}  // namespace uagfx::win
```

`src/win/shadow.cpp`:
```cpp
#include "win/shadow.hpp"

#include "win/context.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

LONG CALLBACK onFault(EXCEPTION_POINTERS* e) {
    const EXCEPTION_RECORD* r = e->ExceptionRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || r->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const void* address = reinterpret_cast<const void*>(r->ExceptionInformation[1]);
    Context& c = Context::get();
    // Other modules' access violations (handled by their own SEH) must not wait for our lock: check the address
    // first, then take the lock and check again.
    if (!c.shadow.contains(address)) return EXCEPTION_CONTINUE_SEARCH;
    Guard guard;
    if (!c.shadow.contains(address) || !c.shadow.locked()) return EXCEPTION_CONTINUE_SEARCH;
    c.shadow.fault(r->ExceptionInformation[0] == 1);
    return EXCEPTION_CONTINUE_EXECUTION;
}

}  // namespace

void installShadowHandler() {
    static bool installed = false;
    if (installed) return;
    installed = AddVectoredExceptionHandler(1, onFault) != nullptr;
    if (!installed) LOG_ERROR("cannot install the exception handler: every Lock of the target will read it back");
}

Shadow::~Shadow() {
    if (mem_) VirtualFree(mem_, 0, MEM_RELEASE);
}

bool Shadow::resize(int width, int height, PixelFormat format) {
    if (mem_) VirtualFree(mem_, 0, MEM_RELEASE);
    mem_ = nullptr;
    w_ = width;
    h_ = height;
    f_ = format;
    pitch_ = (width * bytesPerPixel(format) + 3) & ~3;
    size_ = (size_t(pitch_) * size_t(height) + 4095) & ~size_t(4095);
    state_ = ShadowState{};
    mem_ = static_cast<uint8_t*>(VirtualAlloc(nullptr, size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    return mem_ != nullptr;
}

void Shadow::protect(DWORD mode) {
    DWORD old = 0;
    if (mem_) VirtualProtect(mem_, size_, mode, &old);
}

uint8_t* Shadow::lock() {
    state_.lock();
    protect(PAGE_NOACCESS);
    return mem_;
}

bool Shadow::unlock() {
    const bool written = state_.unlock();
    protect(PAGE_READWRITE);
    return written;
}

bool Shadow::contains(const void* p) const {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    return mem_ && b >= mem_ && b < mem_ + size_;
}

void Shadow::fault(bool write) {
    Context& c = Context::get();
    c.flush();  // what was recorded before the touch is part of what the client reads
    const ShadowState::Touch t = state_.touch(write);
    if (t.readBack) {
        protect(PAGE_READWRITE);
        std::vector<uint32_t> argb;
        if (c.readTarget(argb)) fill(argb);  // the record is already empty: no second flush
    }
    protect(t.writable ? PAGE_READWRITE : PAGE_READONLY);
}

void Shadow::fill(const std::vector<uint32_t>& argb) {
    if (argb.size() < size_t(w_) * size_t(h_)) return;
    const int bpp = bytesPerPixel(f_);
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x)
            writePixel(f_, mem_ + size_t(y) * size_t(pitch_) + size_t(x) * size_t(bpp),
                       targetToClient(f_, argb[size_t(y) * size_t(w_) + size_t(x)]));
}

void Shadow::toArgb(std::vector<uint32_t>& argb) const {
    argb.resize(size_t(w_) * size_t(h_));
    const int bpp = bytesPerPixel(f_);
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x)
            argb[size_t(y) * size_t(w_) + size_t(x)] =
                toArgb8(f_, readPixel(f_, mem_ + size_t(y) * size_t(pitch_) + size_t(x) * size_t(bpp)));
}

}  // namespace uagfx::win
```

- [ ] **Step 4: Контекст**

`src/win/context.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/pixel.hpp"
#include "win/gpu/renderer.hpp"
#include "win/shadow.hpp"

namespace uagfx::win {

// What the DirectDraw objects share: the global lock (the client asks for DDSCL_MULTITHREADED; Bink may call from
// its own thread), the settings, the renderer, the frame being recorded, the display mode the client sees, the
// window and the CPU copy of the render target.
class Context {
public:
    static Context& get();
    void lock() { EnterCriticalSection(&cs_); }
    void unlock() { LeaveCriticalSection(&cs_); }

    Config config;
    gpu::Renderer* renderer = nullptr;
    FrameRecord record;
    int screenWidth = 0;
    int screenHeight = 0;
    PixelFormat screenFormat = PixelFormat::X8R8G8B8;
    HWND window = nullptr;
    bool windowApplied = false;  // [display] mode is applied at the first present, once the client's size is fixed
    bool vblankWaited = false;   // WaitForVerticalBlank came before the next Blt onto the primary
    Shadow shadow;

    bool open(std::string& error);          // the renderer, at DirectDrawCreateEx; false: use another DirectDraw
    void close();                           // the client released DirectDraw: the renderer goes with it
    bool setTarget(int width, int height);  // the renderer's target and the shadow, in the screen format
    void flush();                           // plays the record on the GPU
    bool readTarget(std::vector<uint32_t>& argb);
    void writeTarget(const Rect& rect, const uint32_t* argb);
    // Flip and WaitForVerticalBlank + Blt wait for vblank (when vsync is on); the loading bar's Blt shows at once
    // (spec §4.1), and so does Bink's.
    void present(const Rect& source, bool waitForVblank);

private:
    Context();
    // One log line every 30 seconds and one at close: frames shown and reads of the frame from the GPU. Reads
    // should stay near 0 while playing: the per-frame Lock must not cause any (Review Focus 1).
    void logStats(bool final);
    struct Stats {
        unsigned frames = 0;
        unsigned readBacks = 0;
        DWORD since = 0;
    };

    CRITICAL_SECTION cs_;
    Stats stats_;
};

// Holds the global lock for a scope; every DirectDraw / Direct3D method starts with one.
class Guard {
public:
    Guard() { Context::get().lock(); }
    ~Guard() { Context::get().unlock(); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
};

}  // namespace uagfx::win
```

`src/win/context.cpp`:
```cpp
#include "win/context.hpp"

#include "win/log.hpp"
#include "win/window_mode.hpp"

namespace uagfx::win {

Context& Context::get() {
    static Context c;
    return c;
}

Context::Context() {
    InitializeCriticalSection(&cs_);
    screenWidth = GetSystemMetrics(SM_CXSCREEN);  // what a windowed client sees until SetDisplayMode
    screenHeight = GetSystemMetrics(SM_CYSCREEN);
}

bool Context::open(std::string& error) {
    if (renderer) return true;
    gpu::Renderer* r = new gpu::Renderer;
    if (!r->create(error)) {
        delete r;
        return false;
    }
    renderer = r;
    LOG_INFO("Direct3D 12 renderer on %s", renderer->adapterName().c_str());
    return true;
}

void Context::close() {
    flush();
    logStats(true);
    delete renderer;  // waits for the GPU
    renderer = nullptr;
    record.reset();
    window = nullptr;
    windowApplied = false;
}

bool Context::setTarget(int width, int height) {
    if (!renderer || !renderer->setTarget(width, height)) return false;
    record.reset();
    installShadowHandler();
    if (!shadow.resize(width, height, screenFormat)) {
        LOG_ERROR("cannot allocate the %dx%d CPU copy of the target", width, height);
        return false;
    }
    return true;
}

void Context::flush() {
    if (!renderer || record.empty()) return;
    renderer->execute(record);
    record.reset();
    shadow.gpuChanged();
}

bool Context::readTarget(std::vector<uint32_t>& argb) {
    flush();
    ++stats_.readBacks;
    return renderer && renderer->readTarget(argb);
}

void Context::writeTarget(const Rect& rect, const uint32_t* argb) { record.writeTarget(rect, argb); }

void Context::present(const Rect& source, bool waitForVblank) {
    flush();
    if (!renderer) return;
    if (window && IsIconic(window)) {  // minimised: nothing to show, and no vsync to hold the game's loop back
        Sleep(15);
        return;
    }
    if (!windowApplied && window) {
        // Not earlier: a windowed client sizes its back buffer from the window's client area (L3039), and that
        // logical size must stay the one from its settings, whatever the window becomes. The flag goes first:
        // SetWindowPos sends WM_MOVE, and the client may present again from its handler (L457929).
        windowApplied = true;
        applyWindowMode(window, config, shadow.width(), shadow.height());
    }
    gpu::PresentOptions o;
    o.vsync = config.vsync && waitForVblank;
    o.allowTearing = config.allowTearing;
    o.fpsLimit = config.fpsLimit;
    renderer->present(window, source, o);
    ++stats_.frames;
    logStats(false);
}

void Context::logStats(bool final) {
    const DWORD now = GetTickCount();
    if (!stats_.since) stats_.since = now;
    const DWORD ms = now - stats_.since;
    if (!final && ms < 30000) return;
    if (stats_.frames || stats_.readBacks)
        LOG_INFO("%u frames in %.1f s (%.1f fps), %u reads of the frame from the GPU", stats_.frames, ms / 1000.0,
                 ms ? stats_.frames * 1000.0 / ms : 0.0, stats_.readBacks);
    stats_ = Stats{};
    stats_.since = now;
}

}  // namespace uagfx::win
```

- [ ] **Step 5: Режим окна**

`src/win/window_mode.hpp`:
```cpp
#pragma once
#include <windows.h>

#include "core/config.hpp"

namespace uagfx::win {

// [display] mode: borderless over the chosen monitor (the monitor's mode never changes), or a normal window of
// window_width x window_height (0 = the game's size), centred on that monitor.
void applyWindowMode(HWND hwnd, const Config& config, int gameWidth, int gameHeight);

}  // namespace uagfx::win
```

`src/win/window_mode.cpp`:
```cpp
#include "win/window_mode.hpp"

#include <vector>

#include "win/log.hpp"

namespace uagfx::win {
namespace {

BOOL CALLBACK addMonitor(HMONITOR m, HDC, LPRECT, LPARAM data) {
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (GetMonitorInfoA(m, &mi)) reinterpret_cast<std::vector<RECT>*>(data)->push_back(mi.rcMonitor);
    return TRUE;
}

RECT monitorRect(int index) {
    if (index > 0) {
        std::vector<RECT> all;
        EnumDisplayMonitors(nullptr, nullptr, addMonitor, reinterpret_cast<LPARAM>(&all));
        if (size_t(index) < all.size()) return all[size_t(index)];
    }
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    GetMonitorInfoA(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    return mi.rcMonitor;
}

}  // namespace

void applyWindowMode(HWND hwnd, const Config& config, int gameWidth, int gameHeight) {
    if (!hwnd) return;
    const RECT mon = monitorRect(config.monitor);
    const LONG style = GetWindowLongA(hwnd, GWL_STYLE);
    const LONG ex = GetWindowLongA(hwnd, GWL_EXSTYLE);
    const LONG visible = style & WS_VISIBLE;
    if (config.mode == DisplayMode::Borderless) {
        SetWindowLongA(hwnd, GWL_STYLE, LONG(WS_POPUP) | visible);
        SetWindowLongA(hwnd, GWL_EXSTYLE,
                       ex & ~LONG(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE | WS_EX_TOPMOST));
        SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        const int w = config.windowWidth ? config.windowWidth : gameWidth;
        const int h = config.windowHeight ? config.windowHeight : gameHeight;
        const LONG ws = LONG(WS_OVERLAPPEDWINDOW) | visible;
        SetWindowLongA(hwnd, GWL_STYLE, ws);
        RECT r{0, 0, w, h};
        AdjustWindowRectEx(&r, DWORD(ws), FALSE, DWORD(ex));
        const int ow = r.right - r.left;
        const int oh = r.bottom - r.top;
        SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left + (mon.right - mon.left - ow) / 2,
                     mon.top + (mon.bottom - mon.top - oh) / 2, ow, oh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    RECT now{};
    GetWindowRect(hwnd, &now);
    LOG_INFO("window: %s, %ldx%ld at %ld,%ld", displayModeToken(config.mode), now.right - now.left,
             now.bottom - now.top, now.left, now.top);
}

}  // namespace uagfx::win
```

- [ ] **Step 6: Собирается**

Run: `cmake --build build-win -j && python3 tools/check_dll.py`
Expected: без предупреждений; `OK: … 22 exports, … imports kernel32.dll msvcrt.dll user32.dll` (`user32.dll` — окно,
мониторы; он в `ALLOWED_IMPORTS`).

- [ ] **Step 7: Commit**

```bash
git add src/win/context.hpp src/win/context.cpp src/win/shadow.hpp src/win/shadow.cpp src/win/window_mode.hpp src/win/window_mode.cpp src/win/dx7/guids.cpp src/win/dx7/formats.hpp src/win/dx7/formats.cpp
git commit -m "Контекст драйвера, копия RT на защищённых страницах (чтение с GPU по первому касанию), режим окна

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 10: Поверхности на CPU (ядро): блиты с переводом формата, чтение и запись RT, уровни текстур

Всё, что поверхности DirectDraw делают с пикселями, — здесь, в переносимом ядре с тестами на Linux; COM-объекты
задачи 11 только вызывают эти функции. Wine на сервере нет, поэтому то, что можно проверить без Windows, проверяется
здесь.

**Files:**
- Create: `src/core/surface_ops.hpp`, `src/core/surface_ops.cpp`, `src/core/texture_levels.hpp`,
  `src/core/texture_levels.cpp`
- Test: `tests/test_surface_ops.cpp`, `tests/test_texture_levels.cpp`

**Interfaces:**
- Consumes: `Image`, `Rect`, `clipRect`, `fill`, `copy`, `copyKeyed`, `stretch` (1A), `toArgb8`, `fromArgb8`,
  `targetToClient` (Task 3), `FrameRecord::upload` (Task 2).
- Produces:
  ```cpp
  namespace uagfx {
  struct ColorKey { bool enabled = false; uint32_t low = 0, high = 0; bool matches(uint32_t raw) const; };
  uint8_t* pixelAddress(Image& img, int x, int y);
  void toArgbRect(const Image& img, const Rect& r, const ColorKey& key, std::vector<uint32_t>& out);
  bool blitCpu(Image& dst, const Rect& dr, const Image& src, const Rect& sr, const ColorKey& key);
  bool readTargetInto(Image& dst, const Rect& dr, const std::vector<uint32_t>& target, int width, int height,
                      const Rect& sr);
  struct TargetWrite { Rect rect; std::vector<uint32_t> argb; };
  bool targetWrite(const Image& src, const Rect& sr, const Rect& dr, int width, int height, const ColorKey& key,
                   const std::vector<uint32_t>& current, TargetWrite& out);
  class TextureLevels {
  public:
      TextureLevels(int width, int height, PixelFormat format, int levels);
      int levels() const; Image& level(int i); const Image& level(int i) const;
      const ColorKey& key() const; void setKey(const ColorKey& key);
      void changed(int level, Rect r); bool pending() const;
      void sync(FrameRecord& record, uint32_t texture);
  };
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_surface_ops.cpp`:
```cpp
#include <vector>

#include "core/surface_ops.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
const ColorKey kMagenta{true, 0xF81F, 0xF81F};  // the client's key in R5G6B5 (surfaces-present-caps.md §1)
}  // namespace

TEST(surfops_pixel_address_is_the_rect_origin) {
    Image img(10, 4, PixelFormat::R5G6B5);
    CHECK(pixelAddress(img, 3, 2) == img.data() + 2 * img.pitch() + 3 * 2);
}

TEST(surfops_argb_rect_with_and_without_key) {
    Image img(3, 1, PixelFormat::R5G6B5);
    img.setPixel(0, 0, 0xF81F);
    img.setPixel(1, 0, 0x07E0);
    img.setPixel(2, 0, 0xFFFF);
    std::vector<uint32_t> out;
    toArgbRect(img, Rect{0, 0, 3, 1}, ColorKey{}, out);
    CHECK_EQ(out.size(), size_t(3));
    CHECK_EQ(out[0], 0xFFFF00FFu);
    toArgbRect(img, Rect{0, 0, 3, 1}, kMagenta, out);
    CHECK_EQ(out[0], 0u);  // alpha 0: the shader discards it while COLORKEYENABLE is on
    CHECK_EQ(out[1], 0xFF00FF00u);
    CHECK_EQ(out[2], 0xFFFFFFFFu);
}

TEST(surfops_blit_same_size_copies_and_skips_the_key) {
    Image src(2, 1, PixelFormat::R5G6B5);
    src.setPixel(0, 0, 0xF81F);
    src.setPixel(1, 0, 0x001F);
    Image dst(2, 1, PixelFormat::R5G6B5);
    dst.setPixel(0, 0, 0x1234);
    CHECK(blitCpu(dst, dst.bounds(), src, src.bounds(), kMagenta));
    CHECK_EQ(dst.pixel(0, 0), 0x1234u);
    CHECK_EQ(dst.pixel(1, 0), 0x001Fu);
}

TEST(surfops_blit_converts_formats) {
    Image src(1, 1, PixelFormat::A4R4G4B4);
    src.setPixel(0, 0, 0xF0F0);  // opaque green
    Image dst(1, 1, PixelFormat::A1R5G5B5);
    CHECK(blitCpu(dst, dst.bounds(), src, src.bounds(), ColorKey{}));
    CHECK_EQ(dst.pixel(0, 0), 0x83E0u);
}

TEST(surfops_blit_stretches_a_mip_level) {
    Image level0(4, 4, PixelFormat::A1R5G5B5);
    fill(level0, level0.bounds(), 0x801F);
    Image level1(2, 2, PixelFormat::A1R5G5B5);
    CHECK(blitCpu(level1, level1.bounds(), level0, level0.bounds(), ColorKey{}));
    CHECK_EQ(level1.pixel(0, 0), 0x801Fu);
    CHECK_EQ(level1.pixel(1, 1), 0x801Fu);
}

TEST(surfops_blit_rejects_a_source_outside_its_surface) {
    Image a(4, 4, PixelFormat::R5G6B5);
    Image b(4, 4, PixelFormat::R5G6B5);
    CHECK(!blitCpu(a, a.bounds(), b, Rect{2, 2, 6, 6}, ColorKey{}));
}

TEST(surfops_read_target_floors_into_16_bits) {
    const std::vector<uint32_t> target = {0xFFFDFDFD, 0xFFFFFFFF};
    Image dst(2, 1, PixelFormat::R5G6B5);
    CHECK(readTargetInto(dst, dst.bounds(), target, 2, 1, Rect{0, 0, 2, 1}));
    CHECK_EQ(dst.pixel(0, 0), uint32_t(30u << 11 | 62u << 5 | 30u));
    CHECK_EQ(dst.pixel(1, 0), 0xFFFFu);
}

TEST(surfops_read_target_stretches_into_a_surface_rounded_up_to_4) {
    // ROExt's 1600x863 window: the calibration readback surface is 1600x864, Blt(NULL, target, NULL) stretches.
    const std::vector<uint32_t> target = {0xFF000000, 0xFF0000FF, 0xFFFF0000};  // 1x3: black, blue, red
    Image dst(1, 4, PixelFormat::X8R8G8B8);
    CHECK(readTargetInto(dst, dst.bounds(), target, 1, 3, Rect{0, 0, 1, 3}));
    CHECK_EQ(dst.pixel(0, 0), 0x000000u);
    CHECK_EQ(dst.pixel(0, 1), 0x0000FFu);
    CHECK_EQ(dst.pixel(0, 2), 0x0000FFu);  // nearest: rows 0, 1, 1, 2
    CHECK_EQ(dst.pixel(0, 3), 0xFF0000u);
}

TEST(surfops_target_write_is_clipped_to_the_target) {
    Image src(4, 1, PixelFormat::R5G6B5);
    fill(src, src.bounds(), 0xFFFF);
    TargetWrite w;
    CHECK(targetWrite(src, src.bounds(), Rect{2, 0, 6, 1}, 4, 2, ColorKey{}, {}, w));
    CHECK(w.rect == (Rect{2, 0, 4, 1}));
    CHECK_EQ(w.argb.size(), size_t(2));
    CHECK_EQ(w.argb[0], 0xFFFFFFFFu);
}

TEST(surfops_target_write_with_key_keeps_the_target) {
    Image src(2, 1, PixelFormat::R5G6B5);
    src.setPixel(0, 0, 0xF81F);
    src.setPixel(1, 0, 0x001F);
    const std::vector<uint32_t> current = {0xFF112233, 0xFF445566};
    TargetWrite w;
    CHECK(targetWrite(src, src.bounds(), Rect{0, 0, 2, 1}, 2, 1, kMagenta, current, w));
    CHECK_EQ(w.argb[0], 0xFF112233u);
    CHECK_EQ(w.argb[1], 0xFF0000FFu);
    CHECK(!targetWrite(src, src.bounds(), Rect{0, 0, 2, 1}, 2, 1, kMagenta, {}, w));  // a key needs the target
}
```

`tests/test_texture_levels.cpp`:
```cpp
#include "core/texture_levels.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(texlevels_new_levels_are_uploaded_whole_once) {
    TextureLevels t(8, 4, PixelFormat::A1R5G5B5, 3);
    CHECK_EQ(t.levels(), 3);
    CHECK_EQ(t.level(2).width(), 2);
    CHECK_EQ(t.level(2).height(), 1);
    CHECK(t.pending());
    FrameRecord r;
    t.sync(r, 7);
    CHECK_EQ(r.uploads().size(), size_t(3));
    CHECK(r.uploads()[0].rect == (Rect{0, 0, 8, 4}));
    CHECK_EQ(r.uploads()[1].level, 1u);
    CHECK_EQ(r.uploads()[2].texture, 7u);
    CHECK_EQ(r.pixels()[0], 0u);  // zeroed memory: transparent black (the glyph atlas relies on it)
    CHECK(!t.pending());
    t.sync(r, 7);
    CHECK_EQ(r.uploads().size(), size_t(3));  // nothing changed since
}

TEST(texlevels_changes_merge_into_one_upload_per_level) {
    TextureLevels t(8, 8, PixelFormat::A1R5G5B5, 1);
    FrameRecord r;
    t.sync(r, 1);
    r.reset();
    t.level(0).setPixel(1, 1, 0xFFFF);
    t.changed(0, Rect{1, 1, 2, 2});
    t.changed(0, Rect{4, 2, 6, 3});
    t.sync(r, 1);
    CHECK_EQ(r.uploads().size(), size_t(1));
    CHECK(r.uploads()[0].rect == (Rect{1, 1, 6, 3}));
    CHECK_EQ(r.pixels()[r.uploads()[0].pixels], 0xFFFFFFFFu);
}

TEST(texlevels_change_outside_the_level_is_clipped) {
    TextureLevels t(4, 4, PixelFormat::A4R4G4B4, 2);
    FrameRecord r;
    t.sync(r, 1);
    r.reset();
    t.changed(1, Rect{1, 1, 9, 9});  // level 1 is 2x2
    t.sync(r, 1);
    CHECK_EQ(r.uploads().size(), size_t(1));
    CHECK(r.uploads()[0].rect == (Rect{1, 1, 2, 2}));
}

TEST(texlevels_colour_key_sends_keyed_texels_as_alpha_zero) {
    TextureLevels t(2, 1, PixelFormat::R5G6B5, 1);
    t.level(0).setPixel(0, 0, 0xF81F);
    t.level(0).setPixel(1, 0, 0x07E0);
    FrameRecord r;
    t.sync(r, 1);
    CHECK_EQ(r.pixels()[0], 0xFFFF00FFu);
    r.reset();
    t.setKey(ColorKey{true, 0xF81F, 0xF81F});
    CHECK(t.pending());  // the alpha of the keyed texels changed: everything goes again
    t.sync(r, 1);
    CHECK_EQ(r.pixels()[0], 0u);
    CHECK_EQ(r.pixels()[1], 0xFF00FF00u);
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/surface_ops.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/surface_ops.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <vector>

#include "core/image.hpp"

namespace uagfx {

// A DirectDraw source colour key (DDSD_CKSRCBLT, SetColorKey): raw values in [low, high] are transparent.
struct ColorKey {
    bool enabled = false;
    uint32_t low = 0;
    uint32_t high = 0;
    bool matches(uint32_t raw) const { return enabled && raw >= low && raw <= high; }
};

// What Lock hands out for a rectangle: the address of its top-left pixel; the pitch stays the image's.
uint8_t* pixelAddress(Image& img, int x, int y);

// r of img as 0xAARRGGBB, row by row; texels matching the key become 0 (alpha 0: the shader discards them).
void toArgbRect(const Image& img, const Rect& r, const ColorKey& key, std::vector<uint32_t>& out);

// Blt / BltFast between CPU surfaces. dr and sr are full rectangles (NULL already replaced by the bounds); sr must
// lie inside src. Equal sizes copy (with the key, matching source texels are skipped); different sizes stretch,
// linearly, or with the nearest texel when keyed so the key colour is not blended into its neighbours. Different
// pixel formats are converted through ARGB8 (the key too: exact for the single-colour keys the client uses).
bool blitCpu(Image& dst, const Rect& dr, const Image& src, const Rect& sr, const ColorKey& key);

// Blt from the render target into a CPU surface. target holds width x height ARGB8 values (Renderer::readTarget).
// Pixels become the surface's format with targetToClient (16-bit floor, calibration.md §5); different sizes take
// the nearest pixel (the calibration readback surface is the target rounded up to a multiple of 4).
bool readTargetInto(Image& dst, const Rect& dr, const std::vector<uint32_t>& target, int width, int height,
                    const Rect& sr);

// Blt from a CPU surface into the render target (width x height): the ARGB8 pixels for dr clipped to the target.
// With a key, matching texels keep the target's pixel from current, the target read back (required then).
struct TargetWrite {
    Rect rect;  // empty when dr lies outside the target: nothing to write
    std::vector<uint32_t> argb;
};
bool targetWrite(const Image& src, const Rect& sr, const Rect& dr, int width, int height, const ColorKey& key,
                 const std::vector<uint32_t>& current, TargetWrite& out);

}  // namespace uagfx
```

`src/core/surface_ops.cpp`:
```cpp
#include "core/surface_ops.hpp"

namespace uagfx {
namespace {

bool inside(const Rect& r, int width, int height) {
    return !r.empty() && r.left >= 0 && r.top >= 0 && r.right <= width && r.bottom <= height;
}

// The sr part of src as its own image in format f.
Image region(const Image& src, const Rect& sr, PixelFormat f) {
    Image out(sr.width(), sr.height(), f);
    for (int y = 0; y < sr.height(); ++y)
        for (int x = 0; x < sr.width(); ++x)
            out.setPixel(x, y, fromArgb8(f, toArgb8(src.format(), src.pixel(sr.left + x, sr.top + y))));
    return out;
}

}  // namespace

uint8_t* pixelAddress(Image& img, int x, int y) {
    return img.row(y) + size_t(x) * size_t(bytesPerPixel(img.format()));
}

void toArgbRect(const Image& img, const Rect& r, const ColorKey& key, std::vector<uint32_t>& out) {
    out.resize(size_t(r.width()) * size_t(r.height()));
    size_t i = 0;
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x) {
            const uint32_t raw = img.pixel(x, y);
            out[i++] = key.matches(raw) ? 0u : toArgb8(img.format(), raw);
        }
}

bool blitCpu(Image& dst, const Rect& dr, const Image& src, const Rect& sr, const ColorKey& key) {
    if (dst.format() == PixelFormat::Unknown || src.format() == PixelFormat::Unknown) return false;
    if (dr.empty() || !inside(sr, src.width(), src.height())) return false;
    if (src.format() != dst.format()) {
        const Image part = region(src, sr, dst.format());
        ColorKey k = key;
        k.low = fromArgb8(dst.format(), toArgb8(src.format(), key.low));
        k.high = fromArgb8(dst.format(), toArgb8(src.format(), key.high));
        return blitCpu(dst, dr, part, part.bounds(), k);
    }
    if (dr.width() == sr.width() && dr.height() == sr.height())
        return key.enabled ? copyKeyed(dst, dr.left, dr.top, src, sr, key.low, key.high)
                           : copy(dst, dr.left, dr.top, src, sr);
    if (!key.enabled) return stretch(dst, dr, src, sr, StretchFilter::Linear);
    Image scaled(dr.width(), dr.height(), src.format());
    stretch(scaled, scaled.bounds(), src, sr, StretchFilter::Point);
    return copyKeyed(dst, dr.left, dr.top, scaled, scaled.bounds(), key.low, key.high);
}

bool readTargetInto(Image& dst, const Rect& dr, const std::vector<uint32_t>& target, int width, int height,
                    const Rect& sr) {
    if (dst.format() == PixelFormat::Unknown || dr.empty() || !inside(sr, width, height)) return false;
    if (target.size() < size_t(width) * size_t(height)) return false;
    Image part(sr.width(), sr.height(), dst.format());
    for (int y = 0; y < sr.height(); ++y)
        for (int x = 0; x < sr.width(); ++x)
            part.setPixel(x, y, targetToClient(dst.format(),
                                               target[size_t(sr.top + y) * size_t(width) + size_t(sr.left + x)]));
    if (dr.width() == part.width() && dr.height() == part.height())
        return copy(dst, dr.left, dr.top, part, part.bounds());
    return stretch(dst, dr, part, part.bounds(), StretchFilter::Point);
}

bool targetWrite(const Image& src, const Rect& sr, const Rect& dr, int width, int height, const ColorKey& key,
                 const std::vector<uint32_t>& current, TargetWrite& out) {
    if (src.format() == PixelFormat::Unknown || dr.empty() || !inside(sr, src.width(), src.height())) return false;
    if (key.enabled && current.size() < size_t(width) * size_t(height)) return false;
    out.rect = dr;
    out.argb.clear();
    if (!clipRect(Rect{0, 0, width, height}, out.rect)) {
        out.rect = Rect{};
        return true;
    }
    const bool same = dr.width() == sr.width() && dr.height() == sr.height();
    Image scaled;
    if (!same) {
        scaled = Image(dr.width(), dr.height(), src.format());
        stretch(scaled, scaled.bounds(), src, sr, key.enabled ? StretchFilter::Point : StretchFilter::Linear);
    }
    out.argb.reserve(size_t(out.rect.width()) * size_t(out.rect.height()));
    for (int y = out.rect.top; y < out.rect.bottom; ++y)
        for (int x = out.rect.left; x < out.rect.right; ++x) {
            const uint32_t raw = same ? src.pixel(sr.left + x - dr.left, sr.top + y - dr.top)
                                      : scaled.pixel(x - dr.left, y - dr.top);
            out.argb.push_back(key.matches(raw) ? current[size_t(y) * size_t(width) + size_t(x)]
                                                : toArgb8(src.format(), raw));
        }
    return true;
}

}  // namespace uagfx
```

`src/core/texture_levels.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/surface_ops.hpp"

namespace uagfx {

// The CPU side of a texture and its mip levels: the client writes them with Lock and Blt and reads them back for
// the GDI path (surfaces-present-caps.md §2, L2155). The GPU texture gets the changed rectangles just before a
// draw samples it, so many small writes between two draws cost one upload. A new texture counts as changed
// everywhere: its memory is zeroed and the GPU texture starts undefined.
class TextureLevels {
public:
    TextureLevels(int width, int height, PixelFormat format, int levels);
    int levels() const { return int(images_.size()); }
    Image& level(int i) { return images_[size_t(i)]; }
    const Image& level(int i) const { return images_[size_t(i)]; }
    const ColorKey& key() const { return key_; }
    void setKey(const ColorKey& key);  // the alpha of keyed texels changes: every level is sent again
    void changed(int level, Rect r);   // r is clipped to the level
    bool pending() const;
    void sync(FrameRecord& record, uint32_t texture);  // records the changed rectangles as uploads

private:
    std::vector<Image> images_;
    std::vector<Rect> dirty_;
    ColorKey key_;
};

}  // namespace uagfx
```

`src/core/texture_levels.cpp`:
```cpp
#include "core/texture_levels.hpp"

#include <algorithm>

namespace uagfx {
namespace {

Rect unite(const Rect& a, const Rect& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return Rect{std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
                std::max(a.bottom, b.bottom)};
}

}  // namespace

TextureLevels::TextureLevels(int width, int height, PixelFormat format, int levels) {
    for (int i = 0; i < std::max(1, levels); ++i) {
        images_.emplace_back(std::max(1, width >> i), std::max(1, height >> i), format);
        dirty_.push_back(images_.back().bounds());
    }
}

void TextureLevels::setKey(const ColorKey& key) {
    key_ = key;
    for (size_t i = 0; i < images_.size(); ++i) dirty_[i] = images_[i].bounds();
}

void TextureLevels::changed(int level, Rect r) {
    if (level < 0 || level >= levels() || !clipRect(images_[size_t(level)].bounds(), r)) return;
    dirty_[size_t(level)] = unite(dirty_[size_t(level)], r);
}

bool TextureLevels::pending() const {
    for (const Rect& r : dirty_)
        if (!r.empty()) return true;
    return false;
}

void TextureLevels::sync(FrameRecord& record, uint32_t texture) {
    std::vector<uint32_t> argb;
    for (size_t i = 0; i < images_.size(); ++i) {
        if (dirty_[i].empty()) continue;
        toArgbRect(images_[i], dirty_[i], key_, argb);
        record.upload(texture, uint32_t(i), dirty_[i], argb.data());
        dirty_[i] = Rect{};
    }
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests surfops && ./build-host/uagfx_tests texlevels && ./build-host/uagfx_tests`
Expected: 10 тестов `PASS surfops_…`, 4 теста `PASS texlevels_…`; полный прогон — все `PASS`, `0 failed checks`.

- [ ] **Step 5: Commit**

```bash
git add src/core/surface_ops.hpp src/core/surface_ops.cpp src/core/texture_levels.hpp src/core/texture_levels.cpp tests/test_surface_ops.cpp tests/test_texture_levels.cpp
git commit -m "Поверхности на CPU: блиты с переводом формата, чтение и запись RT, загрузка изменённых частей текстур

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 11: Объекты DirectDraw: клиппер, поверхности, IDirectDraw7

Тонкая обёртка COM над ядром задачи 10 и контекстом задачи 9. На Linux её не запустить (Wine на сервере нет):
проверка этой задачи — сборка без предупреждений и `check_dll.py`; поведение проверяет пользователь по чек-листу
0.2.0 (Task 13). Поэтому вся логика пикселей уже в ядре, а здесь — только разбор аргументов DirectDraw, счётчики
ссылок и вызовы ядра.

Правила счётчиков ссылок (`surfaces-present-caps.md` §7): счётчик DirectDraw — только клиентский; поверхности не
держат ссылок на DirectDraw. Поверхности, созданные вместе с верхней (задний буфер цепочки переключения, уровни
mipmap), живут и умирают с ней и делят её счётчик, как в DirectDraw: указатель из `GetAttachedSurface` держит всю
цепочку. Прикреплённый Z-буфер и клиппер держатся обычной ссылкой.

**Files:**
- Modify: `src/win/dx7/com.hpp`, `src/win/dx7/com.cpp` (`unknownInterface`)
- Create: `src/win/dx7/clipper.hpp`, `src/win/dx7/clipper.cpp`, `src/win/dx7/surface7.hpp`,
  `src/win/dx7/surface7.cpp`, `src/win/dx7/ddraw7.hpp`, `src/win/dx7/ddraw7.cpp`

**Interfaces:**
- Consumes: `win::Context`, `win::Guard`, `win::Shadow` (Task 9; `Context::open/close` — тоже Task 9),
  `fillPixelFormat`, `readPixelFormat`, `fillSurfaceDesc` (Task 9), `planSurface`, `SurfacePlan`, `ddcaps`
  (Task 3), `routeBlit`, `BlitRequest`, `ddblt`, `ddbltfast` (Task 6), `ColorKey`, `pixelAddress`, `blitCpu`,
  `readTargetInto`, `targetWrite`, `TextureLevels` (Task 10), `ClearCmd`, `FrameRecord` (Task 2), `d3d7::CLEAR_TARGET`
  (Task 1), базы `ClipperBase`, `Surface7Base`, `DirectDraw7Base` и `notImplemented` (Task 5).
- Produces:
  ```cpp
  namespace uagfx::dx7 {
  void unknownInterface(const char* object, REFIID riid);  // com.hpp: logs once, for E_NOINTERFACE
  class Clipper final : public ClipperBase { /* GetHWnd, SetHWnd */ };
  class Surface7 final : public Surface7Base {
  public:
      static Surface7* create(const SurfacePlan& plan, DWORD caps, std::string& error);
      static Surface7* from(IDirectDrawSurface7* s);  // ours or nullptr
      SurfaceRole role() const; Surface7* zBuffer() const;
      uint32_t gpuTexture() const; bool colorKeyed() const; void sync(FrameRecord& record);
  };
  class DirectDraw7 final : public DirectDraw7Base { ... };
  HRESULT createDirectDraw(REFIID iid, void** out);  // after win::Context::open succeeded
  }
  ```

- [ ] **Step 1: Запись в лог о незнакомых интерфейсах**

В `src/win/dx7/com.hpp` после `notImplemented` добавить:
```cpp
// QueryInterface for an interface we do not offer: logged once per object and interface, so the log names it if
// Bink or ROExt asks a surface for an older interface.
void unknownInterface(const char* object, REFIID riid);
```

В `src/win/dx7/com.cpp` добавить `#include <cstdio>` к остальным `#include` и после `notImplemented`:
```cpp
void unknownInterface(const char* object, REFIID riid) {
    char name[112];
    std::snprintf(name, sizeof name, "%s::QueryInterface {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                  object, static_cast<unsigned long>(riid.Data1), riid.Data2, riid.Data3, riid.Data4[0],
                  riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6],
                  riid.Data4[7]);
    Seen& s = seen();
    EnterCriticalSection(&s.lock);
    const bool first = s.names.insert(name).second;
    LeaveCriticalSection(&s.lock);
    if (first) LOG_INFO("%s: E_NOINTERFACE", name);
}
```

- [ ] **Step 2: Клиппер**

`src/win/dx7/clipper.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <ddraw.h>

#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

// IDirectDrawClipper. The client attaches one to the primary in windowed mode only to name its window
// (surfaces-present-caps.md §4); frames always go to the window given to SetCooperativeLevel.
class Clipper final : public ClipperBase {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE GetHWnd(HWND* out) override;
    HRESULT STDMETHODCALLTYPE SetHWnd(DWORD flags, HWND hwnd) override;

private:
    LONG refs_ = 1;
    HWND hwnd_ = nullptr;
};

}  // namespace uagfx::dx7
```

`src/win/dx7/clipper.cpp`:
```cpp
#include "win/dx7/clipper.hpp"

#include "win/context.hpp"

namespace uagfx::dx7 {

HRESULT STDMETHODCALLTYPE Clipper::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirectDrawClipper) {
        *out = static_cast<IDirectDrawClipper*>(this);
        AddRef();
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirectDrawClipper", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Clipper::AddRef() {
    win::Guard guard;
    return ULONG(++refs_);
}

ULONG STDMETHODCALLTYPE Clipper::Release() {
    win::Guard guard;
    if (refs_ <= 0) return 0;
    const LONG n = --refs_;
    if (n == 0) delete this;
    return ULONG(n);
}

HRESULT STDMETHODCALLTYPE Clipper::GetHWnd(HWND* out) {
    win::Guard guard;
    if (!out) return DDERR_INVALIDPARAMS;
    *out = hwnd_;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Clipper::SetHWnd(DWORD, HWND hwnd) {
    win::Guard guard;
    hwnd_ = hwnd;
    return DD_OK;
}

}  // namespace uagfx::dx7
```

- [ ] **Step 3: Поверхность**

`src/win/dx7/surface7.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <ddraw.h>

#include <memory>
#include <string>

#include "core/blit.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/surface_ops.hpp"
#include "core/surface_plan.hpp"
#include "core/texture_levels.hpp"
#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

// IDirectDrawSurface7. Where the pixels live depends on the role (core/surface_plan.hpp):
// - Offscreen: an Image in CPU memory (the screen copy, the calibration readback, big pictures);
// - Texture: one level of TextureLevels, owned by the top level of the mip chain and sent to its GPU texture
//   before a draw samples it;
// - RenderTarget: the GPU target; Lock and GetDC go through the context's CPU copy (win::Shadow);
// - Primary: the window (Flip and Blt onto it show a frame); ZBuffer: the renderer's depth, no pixels.
// Surfaces created together with a top surface (the back buffer of a flip chain, mip levels) live and die with it
// and share its reference count, as in DirectDraw.
class Surface7 final : public Surface7Base {
public:
    static constexpr uint32_t kMagic = 0x37534655;  // "UFS7": tells our surfaces from anything else passed in

    // A new surface with one reference, or nullptr with the reason in error.
    static Surface7* create(const SurfacePlan& plan, DWORD caps, std::string& error);
    // A surface the client hands back to us; nullptr when it is not one of ours.
    static Surface7* from(IDirectDrawSurface7* s);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE7 surface) override;
    HRESULT STDMETHODCALLTYPE Blt(LPRECT dstRect, LPDIRECTDRAWSURFACE7 srcSurface, LPRECT srcRect, DWORD flags,
                                  LPDDBLTFX fx) override;
    HRESULT STDMETHODCALLTYPE BltFast(DWORD x, DWORD y, LPDIRECTDRAWSURFACE7 srcSurface, LPRECT srcRect,
                                      DWORD flags) override;
    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD flags, LPDIRECTDRAWSURFACE7 surface) override;
    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE7 target, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS2 caps, LPDIRECTDRAWSURFACE7* out) override;
    HRESULT STDMETHODCALLTYPE GetCaps(LPDDSCAPS2 caps) override;
    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD flags, LPDDCOLORKEY key) override;
    HRESULT STDMETHODCALLTYPE GetDC(HDC* out) override;
    HRESULT STDMETHODCALLTYPE GetPixelFormat(LPDDPIXELFORMAT format) override;
    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(LPDDSURFACEDESC2 desc) override;
    HRESULT STDMETHODCALLTYPE IsLost() override;
    HRESULT STDMETHODCALLTYPE Lock(LPRECT rect, LPDDSURFACEDESC2 desc, DWORD flags, HANDLE event) override;
    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC dc) override;
    HRESULT STDMETHODCALLTYPE Restore() override;
    HRESULT STDMETHODCALLTYPE SetClipper(LPDIRECTDRAWCLIPPER clipper) override;
    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD flags, LPDDCOLORKEY key) override;
    HRESULT STDMETHODCALLTYPE Unlock(LPRECT rect) override;

    SurfaceRole role() const { return role_; }
    Surface7* zBuffer() const { return zBuffer_; }
    // Textures: the GPU texture of the chain, whether it has a colour key, and the uploads a draw needs first.
    uint32_t gpuTexture() const { return top()->gpuTexture_; }
    bool colorKeyed() const { return colorKey().enabled; }
    void sync(FrameRecord& record);

private:
    Surface7(SurfaceRole role, DWORD caps, int width, int height, PixelFormat format);
    ~Surface7() override;
    Surface7* top() { return root_ ? root_ : this; }
    const Surface7* top() const { return root_ ? root_ : this; }
    Rect bounds() const { return Rect{0, 0, width_, height_}; }
    Place place() const;
    Image& pixels();  // Offscreen and Texture only
    ColorKey colorKey() const;
    int pitch() const;
    void changed(const Rect& r);
    void describe(DDSURFACEDESC2& d, void* bits) const;
    HRESULT blit(BlitAction action, const Rect& dr, Surface7* src, const Rect& sr, const DDBLTFX* fx,
                 const ColorKey& key);
    void freeDc();

    uint32_t magic_ = kMagic;
    LONG refs_ = 1;
    SurfaceRole role_;
    DWORD caps_;
    int width_;
    int height_;
    PixelFormat format_;
    Image image_;                            // Offscreen
    ColorKey key_;                           // Offscreen; a texture keeps its key in TextureLevels
    std::unique_ptr<TextureLevels> levels_;  // Texture, top level only
    int level_ = 0;
    uint32_t gpuTexture_ = 0;                // Texture, top level only
    Surface7* root_ = nullptr;               // the top surface this one was created with
    Surface7* next_ = nullptr;               // owned: the back buffer of a primary, the next mip level
    Surface7* zBuffer_ = nullptr;            // counted: AddAttachedSurface
    IDirectDrawClipper* clipper_ = nullptr;  // counted
    bool locked_ = false;
    Rect lockRect_;
    HDC dc_ = nullptr;
    HBITMAP dib_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    uint32_t* dibBits_ = nullptr;
};

}  // namespace uagfx::dx7
```

`src/win/dx7/surface7.cpp`:
```cpp
#include "win/dx7/surface7.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include "core/d3d7.hpp"
#include "win/context.hpp"
#include "win/dx7/formats.hpp"
#include "win/log.hpp"

namespace uagfx::dx7 {
namespace {

Rect toRect(const RECT& r) { return Rect{int(r.left), int(r.top), int(r.right), int(r.bottom)}; }

bool within(const Rect& r, const Rect& bounds) {
    return !r.empty() && r.left >= bounds.left && r.top >= bounds.top && r.right <= bounds.right &&
           r.bottom <= bounds.bottom;
}

// DDSURFACEDESC2, or the older DDSURFACEDESC a library like Bink may pass: it is the first 108 bytes of the new one.
bool descSizeOk(DWORD size) { return size == sizeof(DDSURFACEDESC2) || size == sizeof(DDSURFACEDESC); }

void copyDesc(LPDDSURFACEDESC2 out, const DDSURFACEDESC2& d) {
    const DWORD size = out->dwSize;
    std::memcpy(out, &d, size);
    out->dwSize = size;
}

void zPixelFormat(DDPIXELFORMAT& pf) {
    std::memset(&pf, 0, sizeof pf);
    pf.dwSize = sizeof pf;
    pf.dwFlags = DDPF_ZBUFFER;
    pf.dwZBufferBitDepth = 16;
    pf.dwZBitMask = 0xFFFF;
}

}  // namespace

Surface7::Surface7(SurfaceRole role, DWORD caps, int width, int height, PixelFormat format)
    : role_(role), caps_(caps), width_(width), height_(height), format_(format) {}

Surface7::~Surface7() {
    magic_ = 0;
    win::Context& c = win::Context::get();
    if (locked_ && role_ == SurfaceRole::RenderTarget) c.shadow.unlock();
    freeDc();
    delete next_;
    if (zBuffer_) zBuffer_->Release();
    if (clipper_) clipper_->Release();
    if (gpuTexture_ && c.renderer) c.renderer->releaseTexture(gpuTexture_);
}

Surface7* Surface7::create(const SurfacePlan& plan, DWORD caps, std::string& error) {
    if (plan.role == SurfaceRole::Unsupported) {
        error = plan.error;
        return nullptr;
    }
    win::Context& c = win::Context::get();
    Surface7* s = new Surface7(plan.role, caps, plan.width, plan.height, plan.format);
    // A key range written the wrong way round means the single colour in low.
    const ColorKey key{plan.colorKey, plan.keyLow, std::max(plan.keyLow, plan.keyHigh)};
    switch (plan.role) {
        case SurfaceRole::Offscreen:
            s->image_ = Image(plan.width, plan.height, plan.format);
            s->key_ = key;
            break;
        case SurfaceRole::Texture: {
            s->levels_.reset(new TextureLevels(plan.width, plan.height, plan.format, plan.mipLevels));
            if (key.enabled) s->levels_->setKey(key);
            s->gpuTexture_ = c.renderer ? c.renderer->createTexture(plan.width, plan.height, s->levels_->levels()) : 0;
            if (!s->gpuTexture_) {
                error = "the GPU texture could not be created";
                delete s;
                return nullptr;
            }
            Surface7* last = s;
            for (int i = 1; i < s->levels_->levels(); ++i) {
                const Image& img = s->levels_->level(i);
                Surface7* m = new Surface7(SurfaceRole::Texture, caps & ~DWORD(ddcaps::COMPLEX), img.width(),
                                           img.height(), plan.format);
                m->root_ = s;
                m->level_ = i;
                last->next_ = m;
                last = m;
            }
            break;
        }
        case SurfaceRole::RenderTarget:
            if (!c.setTarget(plan.width, plan.height)) {
                error = "the render target could not be created";
                delete s;
                return nullptr;
            }
            break;
        case SurfaceRole::Primary:
            if (plan.backBuffer) {
                if (!c.setTarget(plan.width, plan.height)) {
                    error = "the render target could not be created";
                    delete s;
                    return nullptr;
                }
                Surface7* back = new Surface7(SurfaceRole::RenderTarget,
                                              ddcaps::BACKBUFFER | ddcaps::D3DDEVICE | ddcaps::FLIP | ddcaps::COMPLEX |
                                                  ddcaps::VIDEOMEMORY,
                                              plan.width, plan.height, plan.format);
                back->root_ = s;
                s->next_ = back;
            }
            break;
        case SurfaceRole::ZBuffer:
        case SurfaceRole::Unsupported:
            break;
    }
    return s;
}

Surface7* Surface7::from(IDirectDrawSurface7* s) {
    Surface7* p = static_cast<Surface7*>(s);
    return p && p->magic_ == kMagic ? p : nullptr;
}

HRESULT STDMETHODCALLTYPE Surface7::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirectDrawSurface7) {
        *out = static_cast<IDirectDrawSurface7*>(this);
        AddRef();
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirectDrawSurface7", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Surface7::AddRef() {
    win::Guard guard;
    return ULONG(++top()->refs_);
}

ULONG STDMETHODCALLTYPE Surface7::Release() {
    win::Guard guard;
    Surface7* t = top();
    if (t->refs_ <= 0) return 0;  // more Releases than references: ignored rather than freed twice
    const LONG n = --t->refs_;
    if (n == 0) delete t;  // a chain goes as a whole
    return ULONG(n);
}

Place Surface7::place() const {
    if (role_ == SurfaceRole::RenderTarget) return Place::Target;
    if (role_ == SurfaceRole::Primary) return Place::Front;
    return Place::Cpu;
}

Image& Surface7::pixels() { return role_ == SurfaceRole::Texture ? top()->levels_->level(level_) : image_; }

ColorKey Surface7::colorKey() const { return role_ == SurfaceRole::Texture ? top()->levels_->key() : key_; }

int Surface7::pitch() const {
    switch (role_) {
        case SurfaceRole::Offscreen: return image_.pitch();
        case SurfaceRole::Texture: return top()->levels_->level(level_).pitch();
        case SurfaceRole::RenderTarget: return win::Context::get().shadow.pitch();
        case SurfaceRole::ZBuffer: return width_ * 2;
        default: return width_ * bytesPerPixel(format_);
    }
}

void Surface7::changed(const Rect& r) {
    if (role_ == SurfaceRole::Texture) top()->levels_->changed(level_, r);
}

void Surface7::sync(FrameRecord& record) {
    if (role_ == SurfaceRole::Texture) top()->levels_->sync(record, top()->gpuTexture_);
}

void Surface7::describe(DDSURFACEDESC2& d, void* bits) const {
    d.dwSize = sizeof d;
    fillSurfaceDesc(d, width_, height_, pitch(), format_, caps_, bits);
    if (role_ == SurfaceRole::ZBuffer) zPixelFormat(d.ddpfPixelFormat);
    const ColorKey k = colorKey();
    if (k.enabled) {
        d.dwFlags |= DDSD_CKSRCBLT;
        d.ddckCKSrcBlt.dwColorSpaceLowValue = k.low;
        d.ddckCKSrcBlt.dwColorSpaceHighValue = k.high;
    }
    if (role_ == SurfaceRole::Texture && top()->levels_->levels() > 1) {
        d.dwFlags |= DDSD_MIPMAPCOUNT;
        d.dwMipMapCount = DWORD(top()->levels_->levels() - level_);
    }
}

HRESULT STDMETHODCALLTYPE Surface7::AddAttachedSurface(LPDIRECTDRAWSURFACE7 surface) {
    win::Guard guard;
    Surface7* z = from(surface);
    if (!z || z->role_ != SurfaceRole::ZBuffer || role_ != SurfaceRole::RenderTarget) return DDERR_CANNOTATTACHSURFACE;
    if (z == zBuffer_) return DDERR_SURFACEALREADYATTACHED;
    z->AddRef();
    if (zBuffer_) zBuffer_->Release();
    zBuffer_ = z;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::DeleteAttachedSurface(DWORD, LPDIRECTDRAWSURFACE7 surface) {
    win::Guard guard;
    if (!zBuffer_ || (surface && from(surface) != zBuffer_)) return DDERR_SURFACENOTATTACHED;
    Surface7* z = zBuffer_;
    zBuffer_ = nullptr;
    z->Release();
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::Blt(LPRECT dstRect, LPDIRECTDRAWSURFACE7 srcSurface, LPRECT srcRect, DWORD flags,
                                       LPDDBLTFX fx) {
    win::Guard guard;
    Surface7* src = from(srcSurface);
    if (srcSurface && !src) return DDERR_INVALIDPARAMS;
    if (role_ == SurfaceRole::ZBuffer || (src && src->role_ == SurfaceRole::ZBuffer))
        return notImplemented("IDirectDrawSurface7::Blt (Z-buffer)");
    BlitRequest q;
    q.dst = place();
    q.src = src ? src->place() : Place::Cpu;
    q.hasSource = src != nullptr;
    q.flags = flags;
    const Rect dr = dstRect ? toRect(*dstRect) : bounds();
    const Rect sr = src ? (srcRect ? toRect(*srcRect) : src->bounds()) : Rect{};
    const ColorKey key = src && (flags & ddblt::KEYSRC) ? src->colorKey() : ColorKey{};
    return blit(routeBlit(q), dr, src, sr, fx, key);
}

HRESULT STDMETHODCALLTYPE Surface7::BltFast(DWORD x, DWORD y, LPDIRECTDRAWSURFACE7 srcSurface, LPRECT srcRect,
                                           DWORD flags) {
    win::Guard guard;
    Surface7* src = from(srcSurface);
    if (!src) return DDERR_INVALIDPARAMS;
    if (role_ == SurfaceRole::ZBuffer || src->role_ == SurfaceRole::ZBuffer)
        return notImplemented("IDirectDrawSurface7::BltFast (Z-buffer)");
    BlitRequest q;
    q.dst = place();
    q.src = src->place();
    q.hasSource = true;
    q.flags = ddblt::WAIT;
    const Rect sr = srcRect ? toRect(*srcRect) : src->bounds();
    const Rect dr{int(x), int(y), int(x) + sr.width(), int(y) + sr.height()};
    const ColorKey key = (flags & ddbltfast::SRCCOLORKEY) ? src->colorKey() : ColorKey{};
    return blit(routeBlit(q), dr, src, sr, nullptr, key);
}

HRESULT Surface7::blit(BlitAction action, const Rect& dr, Surface7* src, const Rect& sr, const DDBLTFX* fx,
                       const ColorKey& key) {
    win::Context& c = win::Context::get();
    const Rect target{0, 0, c.shadow.width(), c.shadow.height()};
    switch (action) {
        case BlitAction::Fill: {  // fx->dwFillColor is already in this surface's format
            if (!fx) return DDERR_INVALIDPARAMS;
            Rect r = dr;
            if (clipRect(bounds(), r)) {
                fill(pixels(), r, fx->dwFillColor);
                changed(r);
            }
            return DD_OK;
        }
        case BlitAction::FillTarget: {
            if (!fx) return DDERR_INVALIDPARAMS;
            ClearCmd clear;
            clear.flags = d3d7::CLEAR_TARGET;
            clear.argb = toArgb8(format_, fx->dwFillColor);
            clear.rect = dr;
            if (clipRect(target, clear.rect)) c.record.clear(clear);
            return DD_OK;
        }
        case BlitAction::CopyCpu:
            if (!blitCpu(pixels(), dr, src->pixels(), sr, key)) return DDERR_INVALIDRECT;
            changed(dr);
            return DD_OK;
        case BlitAction::ReadTarget: {  // calibration readback, the fade's saved frame
            std::vector<uint32_t> argb;
            if (!c.readTarget(argb)) return DDERR_GENERIC;
            if (!readTargetInto(pixels(), dr, argb, target.width(), target.height(), sr)) return DDERR_INVALIDRECT;
            changed(dr);
            return DD_OK;
        }
        case BlitAction::WriteTarget: {  // the fade's restored frame, pictures bigger than a texture
            std::vector<uint32_t> current;
            if (key.enabled && !c.readTarget(current)) return DDERR_GENERIC;
            TargetWrite w;
            if (!targetWrite(src->pixels(), sr, dr, target.width(), target.height(), key, current, w))
                return DDERR_INVALIDRECT;
            if (!w.rect.empty()) c.writeTarget(w.rect, w.argb.data());
            return DD_OK;
        }
        case BlitAction::Present: {  // windowed present, the loading bar, Bink's 512x347 frame
            Rect s = sr;
            const bool vblank = c.vblankWaited;  // only the windowed game frame waits for it first
            c.vblankWaited = false;
            if (clipRect(target, s)) c.present(s, vblank);
            return DD_OK;
        }
        case BlitAction::Unsupported:
            break;
    }
    return notImplemented("IDirectDrawSurface7::Blt (this pair of surfaces)");
}

HRESULT STDMETHODCALLTYPE Surface7::Flip(LPDIRECTDRAWSURFACE7, DWORD) {
    win::Guard guard;
    if (role_ != SurfaceRole::Primary || !next_) return DDERR_NOTFLIPPABLE;
    win::Context& c = win::Context::get();
    c.present(Rect{0, 0, c.shadow.width(), c.shadow.height()}, true);  // a copy: the back buffer keeps its pixels
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetAttachedSurface(LPDDSCAPS2 caps, LPDIRECTDRAWSURFACE7* out) {
    win::Guard guard;
    if (!caps || !out) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    Surface7* found = nullptr;
    if (caps->dwCaps & ddcaps::ZBUFFER)
        found = zBuffer_;
    else if (role_ == SurfaceRole::Primary && (caps->dwCaps & (ddcaps::BACKBUFFER | ddcaps::FLIP)))
        found = next_;
    else if (role_ == SurfaceRole::Texture && (caps->dwCaps & (ddcaps::MIPMAP | ddcaps::TEXTURE)))
        found = next_;
    if (!found) return DDERR_NOTFOUND;
    found->AddRef();
    *out = found;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetCaps(LPDDSCAPS2 caps) {
    if (!caps) return DDERR_INVALIDPARAMS;
    std::memset(caps, 0, sizeof *caps);
    caps->dwCaps = caps_;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetColorKey(DWORD flags, LPDDCOLORKEY key) {
    win::Guard guard;
    if (!key || !(flags & DDCKEY_SRCBLT)) return DDERR_INVALIDPARAMS;
    const ColorKey k = colorKey();
    if (!k.enabled) return DDERR_NOCOLORKEY;
    key->dwColorSpaceLowValue = k.low;
    key->dwColorSpaceHighValue = k.high;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::SetColorKey(DWORD flags, LPDDCOLORKEY key) {
    win::Guard guard;
    if (!(flags & DDCKEY_SRCBLT)) return notImplemented("IDirectDrawSurface7::SetColorKey (not a source key)");
    ColorKey k;
    if (key) {
        k.enabled = true;
        k.low = key->dwColorSpaceLowValue;
        k.high = (flags & DDCKEY_COLORSPACE) ? std::max(key->dwColorSpaceLowValue, key->dwColorSpaceHighValue)
                                             : key->dwColorSpaceLowValue;
    }
    if (role_ == SurfaceRole::Texture)
        top()->levels_->setKey(k);
    else
        key_ = k;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetDC(HDC* out) {
    win::Guard guard;
    if (!out) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    if (role_ != SurfaceRole::RenderTarget) return notImplemented("IDirectDrawSurface7::GetDC (not the render target)");
    if (dc_) return DDERR_DCALREADYCREATED;
    // The screenshot caption is drawn over the current frame (surfaces-present-caps.md §8).
    win::Context& c = win::Context::get();
    std::vector<uint32_t> argb;
    if (!c.readTarget(argb)) return DDERR_GENERIC;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = width_;
    bi.bmiHeader.biHeight = -height_;  // top-down, like the target
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    dc_ = CreateCompatibleDC(nullptr);
    if (dc_) dib_ = CreateDIBSection(dc_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib_ || !bits) {
        freeDc();
        return DDERR_GENERIC;
    }
    dibBits_ = static_cast<uint32_t*>(bits);
    std::memcpy(dibBits_, argb.data(), std::min(argb.size(), size_t(width_) * size_t(height_)) * sizeof(uint32_t));
    oldBitmap_ = SelectObject(dc_, dib_);  // ARGB8 in memory is the DIB's B, G, R, X
    *out = dc_;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::ReleaseDC(HDC dc) {
    win::Guard guard;
    if (!dc_ || dc != dc_) return DDERR_NODC;
    GdiFlush();
    std::vector<uint32_t> argb(dibBits_, dibBits_ + size_t(width_) * size_t(height_));
    for (uint32_t& p : argb) p |= 0xFF000000u;  // GDI leaves the X byte 0
    win::Context::get().writeTarget(bounds(), argb.data());
    freeDc();
    return DD_OK;
}

void Surface7::freeDc() {
    if (dc_ && oldBitmap_) SelectObject(dc_, oldBitmap_);
    if (dib_) DeleteObject(dib_);
    if (dc_) DeleteDC(dc_);
    dc_ = nullptr;
    dib_ = nullptr;
    oldBitmap_ = nullptr;
    dibBits_ = nullptr;
}

HRESULT STDMETHODCALLTYPE Surface7::GetPixelFormat(LPDDPIXELFORMAT format) {
    win::Guard guard;
    if (!format) return DDERR_INVALIDPARAMS;
    if (role_ == SurfaceRole::ZBuffer)
        zPixelFormat(*format);
    else
        fillPixelFormat(*format, format_);
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::GetSurfaceDesc(LPDDSURFACEDESC2 desc) {
    win::Guard guard;
    if (!desc || !descSizeOk(desc->dwSize)) return DDERR_INVALIDPARAMS;
    DDSURFACEDESC2 d{};
    describe(d, nullptr);
    copyDesc(desc, d);
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::IsLost() { return DD_OK; }  // never lost: everything lives in our memory

HRESULT STDMETHODCALLTYPE Surface7::Restore() { return DD_OK; }

HRESULT STDMETHODCALLTYPE Surface7::Lock(LPRECT rect, LPDDSURFACEDESC2 desc, DWORD, HANDLE) {
    win::Guard guard;
    if (!desc || !descSizeOk(desc->dwSize)) return DDERR_INVALIDPARAMS;
    if (locked_) return DDERR_SURFACEBUSY;
    const Rect r = rect ? toRect(*rect) : bounds();
    if (!within(r, bounds())) return DDERR_INVALIDRECT;
    uint8_t* bits = nullptr;
    switch (role_) {
        case SurfaceRole::Offscreen:
        case SurfaceRole::Texture:
            bits = pixelAddress(pixels(), r.left, r.top);
            break;
        case SurfaceRole::RenderTarget: {  // every frame, mostly without touching the memory (Review Focus 1)
            win::Shadow& sh = win::Context::get().shadow;
            uint8_t* mem = sh.lock();
            if (!mem) {
                sh.unlock();
                return DDERR_GENERIC;
            }
            bits = mem + size_t(r.top) * size_t(sh.pitch()) + size_t(r.left) * size_t(bytesPerPixel(sh.format()));
            break;
        }
        default:
            return notImplemented("IDirectDrawSurface7::Lock (primary or Z-buffer)");
    }
    locked_ = true;
    lockRect_ = r;
    DDSURFACEDESC2 d{};
    describe(d, bits);
    copyDesc(desc, d);
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::Unlock(LPRECT) {
    win::Guard guard;
    if (!locked_) return DDERR_NOTLOCKED;  // the client unlocks everything before an error box
    locked_ = false;
    if (role_ == SurfaceRole::Texture) changed(lockRect_);
    if (role_ == SurfaceRole::RenderTarget) {
        win::Context& c = win::Context::get();
        if (c.shadow.unlock()) {  // written (the wave effect, Bink): the GPU target gets the memory
            std::vector<uint32_t> argb;
            c.shadow.toArgb(argb);
            c.writeTarget(Rect{0, 0, c.shadow.width(), c.shadow.height()}, argb.data());
        }
    }
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE Surface7::SetClipper(LPDIRECTDRAWCLIPPER clipper) {
    win::Guard guard;
    if (clipper) clipper->AddRef();
    if (clipper_) clipper_->Release();
    clipper_ = clipper;
    return DD_OK;
}

}  // namespace uagfx::dx7
```

- [ ] **Step 4: IDirectDraw7**

`src/win/dx7/ddraw7.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <ddraw.h>

#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

// IDirectDraw7 of the uaRO renderer: creates our surfaces and clippers, keeps the display mode the client asked
// for (the monitor's own mode never changes) and its window. The reference count is the client's alone: unless the
// last Release returns 0 the client shows "DDraw object is still referenced" (surfaces-present-caps.md §7).
class DirectDraw7 final : public DirectDraw7Base {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE CreateClipper(DWORD flags, LPDIRECTDRAWCLIPPER* out, IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE CreateSurface(LPDDSURFACEDESC2 desc, LPDIRECTDRAWSURFACE7* out, IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE FlipToGDISurface() override;
    HRESULT STDMETHODCALLTYPE GetCaps(LPDDCAPS driverCaps, LPDDCAPS helCaps) override;
    HRESULT STDMETHODCALLTYPE GetDisplayMode(LPDDSURFACEDESC2 desc) override;
    HRESULT STDMETHODCALLTYPE RestoreAllSurfaces() override;
    HRESULT STDMETHODCALLTYPE RestoreDisplayMode() override;
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hwnd, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE SetDisplayMode(DWORD width, DWORD height, DWORD bpp, DWORD refreshRate,
                                             DWORD flags) override;
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override;
    HRESULT STDMETHODCALLTYPE WaitForVerticalBlank(DWORD flags, HANDLE event) override;

private:
    ~DirectDraw7() override;
    LONG refs_ = 1;
};

// DirectDrawCreateEx of the uaRO renderer, once win::Context::open has succeeded.
HRESULT createDirectDraw(REFIID iid, void** out);

}  // namespace uagfx::dx7
```

`src/win/dx7/ddraw7.cpp`:
```cpp
#include "win/dx7/ddraw7.hpp"

#include <float.h>

#include <cstring>
#include <initializer_list>
#include <string>

#include "core/surface_plan.hpp"
#include "win/context.hpp"
#include "win/dx7/clipper.hpp"
#include "win/dx7/formats.hpp"
#include "win/dx7/surface7.hpp"
#include "win/log.hpp"

namespace uagfx::dx7 {
namespace {

SurfaceRequest requestOf(const DDSURFACEDESC2& d) {
    SurfaceRequest r;
    r.flags = d.dwFlags;
    r.caps = d.ddsCaps.dwCaps;
    r.caps2 = d.ddsCaps.dwCaps2;
    r.width = int(d.dwWidth);
    r.height = int(d.dwHeight);
    r.backBuffers = d.dwBackBufferCount;
    r.mipLevels = d.dwMipMapCount;
    if (d.dwFlags & DDSD_PIXELFORMAT) {
        if (d.ddpfPixelFormat.dwFlags & DDPF_ZBUFFER)
            r.zBits = d.ddpfPixelFormat.dwZBufferBitDepth;
        else
            r.format = readPixelFormat(d.ddpfPixelFormat);
    }
    r.keyLow = d.ddckCKSrcBlt.dwColorSpaceLowValue;
    r.keyHigh = d.ddckCKSrcBlt.dwColorSpaceHighValue;
    return r;
}

const char* roleName(SurfaceRole r) {
    switch (r) {
        case SurfaceRole::Primary: return "primary";
        case SurfaceRole::RenderTarget: return "render target";
        case SurfaceRole::Offscreen: return "offscreen";
        case SurfaceRole::ZBuffer: return "Z-buffer";
        case SurfaceRole::Texture: return "texture";
        case SurfaceRole::Unsupported: break;
    }
    return "unsupported";
}

}  // namespace

HRESULT STDMETHODCALLTYPE DirectDraw7::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirectDraw7) {
        *out = static_cast<IDirectDraw7*>(this);
        AddRef();
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirectDraw7", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DirectDraw7::AddRef() {
    win::Guard guard;
    return ULONG(++refs_);
}

ULONG STDMETHODCALLTYPE DirectDraw7::Release() {
    win::Guard guard;
    if (refs_ <= 0) return 0;
    const LONG n = --refs_;
    if (n == 0) delete this;
    return ULONG(n);
}

DirectDraw7::~DirectDraw7() {
    win::Context::get().close();
    LOG_INFO("DirectDraw released, renderer closed");
}

HRESULT STDMETHODCALLTYPE DirectDraw7::CreateClipper(DWORD, LPDIRECTDRAWCLIPPER* out, IUnknown* outer) {
    if (!out) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    if (outer) return CLASS_E_NOAGGREGATION;
    *out = new Clipper;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::CreateSurface(LPDDSURFACEDESC2 desc, LPDIRECTDRAWSURFACE7* out,
                                                     IUnknown* outer) {
    win::Guard guard;
    if (!desc || !out || desc->dwSize != sizeof(DDSURFACEDESC2)) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    if (outer) return CLASS_E_NOAGGREGATION;
    win::Context& c = win::Context::get();
    const SurfacePlan plan = planSurface(requestOf(*desc), c.screenWidth, c.screenHeight, c.screenFormat);
    std::string error;
    Surface7* s = Surface7::create(plan, desc->ddsCaps.dwCaps, error);
    if (!s) {
        LOG_ERROR("CreateSurface (flags 0x%lx, caps 0x%lx, %lux%lu): %s", static_cast<unsigned long>(desc->dwFlags),
                  static_cast<unsigned long>(desc->ddsCaps.dwCaps), static_cast<unsigned long>(desc->dwWidth),
                  static_cast<unsigned long>(desc->dwHeight), error.c_str());
        return plan.role == SurfaceRole::Unsupported ? DDERR_INVALIDCAPS : DDERR_OUTOFVIDEOMEMORY;
    }
    if (plan.role == SurfaceRole::Texture)  // textures come by the thousand
        LOG_DEBUG("texture %dx%d, %d levels", plan.width, plan.height, plan.mipLevels);
    else
        LOG_INFO("surface: %s %dx%d, %d bits", roleName(plan.role), plan.width, plan.height,
                 bytesPerPixel(plan.format) * 8);
    *out = s;
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::FlipToGDISurface() { return DD_OK; }  // before the client's error boxes

HRESULT STDMETHODCALLTYPE DirectDraw7::GetCaps(LPDDCAPS driverCaps, LPDDCAPS helCaps) {
    for (LPDDCAPS caps : {driverCaps, helCaps}) {  // the client reads only DDCAPS_BLTSTRETCH, and never uses it
        if (!caps) continue;
        const DWORD size = caps->dwSize;
        if (size < 2 * sizeof(DWORD) || size > sizeof(DDCAPS)) return DDERR_INVALIDPARAMS;
        std::memset(caps, 0, size);
        caps->dwSize = size;
        caps->dwCaps = DDCAPS_3D | DDCAPS_BLT | DDCAPS_BLTSTRETCH | DDCAPS_BLTCOLORFILL | DDCAPS_COLORKEY;
    }
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::GetDisplayMode(LPDDSURFACEDESC2 desc) {
    win::Guard guard;
    if (!desc || desc->dwSize != sizeof(DDSURFACEDESC2)) return DDERR_INVALIDPARAMS;
    const win::Context& c = win::Context::get();
    fillSurfaceDesc(*desc, c.screenWidth, c.screenHeight, c.screenWidth * bytesPerPixel(c.screenFormat),
                    c.screenFormat, 0, nullptr);
    desc->dwFlags &= ~DWORD(DDSD_CAPS);
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::RestoreAllSurfaces() { return DD_OK; }  // nothing is ever lost

HRESULT STDMETHODCALLTYPE DirectDraw7::RestoreDisplayMode() { return DD_OK; }

HRESULT STDMETHODCALLTYPE DirectDraw7::SetCooperativeLevel(HWND hwnd, DWORD flags) {
    win::Guard guard;
    win::Context& c = win::Context::get();
    if (hwnd) c.window = hwnd;
    if (flags & DDSCL_FPUSETUP) _control87(_PC_24, _MCW_PC);  // the x87 state DirectX 7 leaves behind
    LOG_INFO("SetCooperativeLevel(0x%lx)%s", static_cast<unsigned long>(flags),
             (flags & DDSCL_FULLSCREEN) ? ": full screen becomes a borderless window" : "");
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::SetDisplayMode(DWORD width, DWORD height, DWORD bpp, DWORD, DWORD) {
    win::Guard guard;
    if (width == 0 || height == 0 || (bpp != 16 && bpp != 32)) return DDERR_INVALIDMODE;
    win::Context& c = win::Context::get();
    c.screenWidth = int(width);
    c.screenHeight = int(height);
    c.screenFormat = bpp == 16 ? PixelFormat::R5G6B5 : PixelFormat::X8R8G8B8;
    LOG_INFO("display mode %lux%lu, %lu bits: the game's size; the monitor keeps its own mode",
             static_cast<unsigned long>(width), static_cast<unsigned long>(height), static_cast<unsigned long>(bpp));
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE DirectDraw7::TestCooperativeLevel() { return DD_OK; }

HRESULT STDMETHODCALLTYPE DirectDraw7::WaitForVerticalBlank(DWORD, HANDLE) {
    win::Guard guard;
    win::Context::get().vblankWaited = true;  // the Blt that follows presents with vsync instead of waiting here
    return DD_OK;
}

HRESULT createDirectDraw(REFIID iid, void** out) {
    if (!out) return DDERR_INVALIDPARAMS;
    *out = nullptr;
    if (iid != IID_IDirectDraw7) return DDERR_INVALIDPARAMS;  // what DirectDrawCreateEx itself accepts
    *out = static_cast<IDirectDraw7*>(new DirectDraw7);
    return DD_OK;
}

}  // namespace uagfx::dx7
```

- [ ] **Step 5: Собирается, тесты ядра зелёные**

Run: `cmake --build build-win -j 2>&1 | grep -E "warning|error" ; python3 tools/check_dll.py && cmake --build build-host -j && ./build-host/uagfx_tests | tail -1`
Expected: ни одной строки `warning`/`error`; `OK: build-win/ddraw.dll: 22 exports, … imports gdi32.dll kernel32.dll msvcrt.dll user32.dll`
(`gdi32.dll` появился: DIB для `GetDC`); последняя строка тестов — `… tests, 0 failed checks`.

- [ ] **Step 6: Commit**

```bash
git add src/win/dx7/com.hpp src/win/dx7/com.cpp src/win/dx7/clipper.hpp src/win/dx7/clipper.cpp src/win/dx7/surface7.hpp src/win/dx7/surface7.cpp src/win/dx7/ddraw7.hpp src/win/dx7/ddraw7.cpp
git commit -m "Объекты DirectDraw: поверхности (Lock, Blt, Flip, GetDC), клиппер, IDirectDraw7 со своим счётчиком ссылок

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 12: Устройство Direct3D 7: возможности, форматы, состояния, рисование в запись кадра

**Files:**
- Create: `src/core/device_caps.hpp`, `src/core/device_caps.cpp`, `src/win/dx7/caps.hpp`, `src/win/dx7/caps.cpp`,
  `src/win/dx7/d3d7.hpp`, `src/win/dx7/d3d7.cpp`, `src/win/dx7/device7.hpp`, `src/win/dx7/device7.cpp`
- Modify: `src/win/dx7/ddraw7.cpp` (`QueryInterface(IID_IDirect3D7)`)
- Test: `tests/test_device_caps.cpp`

**Interfaces:**
- Consumes: `DeviceStates`, `resolveDrawSetup`, `d3d7::*` (Task 1), `FrameRecord`, `DrawCmd`, `ClearCmd`,
  `ViewportCmd` (Task 2), `Surface7` (`from`, `role`, `gpuTexture`, `colorKeyed`, `sync`; Task 11), `win::Context`,
  `win::Guard`, `fillPixelFormat` (Task 9), `notImplemented`, `unknownInterface`, `Direct3D7Base`, `Device7Base`
  (Tasks 5, 11).
- Produces:
  ```cpp
  namespace uagfx {
  namespace d3dcaps { /* DEV_*, MISC_*, RASTER_*, CMP_ALL, BLEND_ALL, SHADE_*, TEXTURE_*, FILTER_*, ADDRESS_*,
                         TEXOP_*, BITDEPTH_* */ }
  struct DeviceCaps { /* the D3DDEVICEDESC7 values */ };
  DeviceCaps deviceCaps();
  const std::vector<PixelFormat>& textureFormats();
  }
  namespace uagfx::dx7 {
  void fillDeviceDesc(D3DDEVICEDESC7& d);
  class Direct3D7 final : public Direct3D7Base { /* CreateDevice, EnumZBufferFormats, EvictManagedTextures */ };
  class Device7 final : public Device7Base { /* states, textures, Clear, SetViewport, Draw*, GetCaps, ... */ };
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_device_caps.cpp`:
```cpp
#include <vector>

#include "core/d3d7.hpp"
#include "core/device_caps.hpp"
#include "test.hpp"

using namespace uagfx;

// surfaces-present-caps.md §5: the fields the client reads and what it does with them.
TEST(devcaps_what_the_client_checks) {
    const DeviceCaps c = deviceCaps();
    CHECK((c.rasterCaps & d3dcaps::RASTER_FOGVERTEX) != 0);        // otherwise "does not support fog effect"
    CHECK((c.rasterCaps & d3dcaps::RASTER_ZBUFFERLESSHSR) == 0);   // otherwise the client creates no Z-buffer
    CHECK((c.filterCaps & d3dcaps::FILTER_LINEARMIPLINEAR) != 0);  // mipmaps with TRILINEARFILTER
    CHECK(c.minTextureSize <= 4);                                  // the 4x4 calibration texture stays 4x4
    CHECK_EQ(c.maxTextureSize, 4096u);
    CHECK_EQ(c.maxAspectRatio, 0u);                                // 0: the client does not clamp the aspect
    CHECK((c.textureOpCaps & d3dcaps::TEXOP_MODULATEALPHA_ADDCOLOR) != 0);  // ground + lightmap in one pass
    CHECK(c.maxBlendStages >= 2);
}

TEST(devcaps_texture_ops_are_the_shaders) {
    // combine() in the pixel shader (Task 7) does operations 1-16 and 18-21; report exactly those.
    const uint32_t ops = deviceCaps().textureOpCaps;
    for (uint32_t op = d3d7::TOP_DISABLE; op <= d3d7::TOP_MODULATEINVCOLOR_ADDALPHA; ++op)
        CHECK(((ops >> (op - 1)) & 1u) == (op == d3d7::TOP_PREMODULATE ? 0u : 1u));
    CHECK((ops >> d3d7::TOP_MODULATEINVCOLOR_ADDALPHA) == 0u);  // no bump mapping, no dot product
    CHECK_EQ(deviceCaps().maxBlendStages, uint16_t(2));         // the shader has two stages
}

TEST(devcaps_texture_formats_start_with_the_clients) {
    const std::vector<PixelFormat>& f = textureFormats();
    CHECK(f.size() >= 2);
    CHECK(f[0] == PixelFormat::A1R5G5B5);  // format codes 0 and 1 of the client (surfaces-present-caps.md §6)
    CHECK(f[1] == PixelFormat::A4R4G4B4);
    for (PixelFormat p : f) CHECK_EQ(bytesPerPixel(p), 2);  // 16-bit only (spec §4.4)
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/device_caps.hpp: No such file or directory`.

- [ ] **Step 3: Возможности устройства (ядро)**

`src/core/device_caps.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <vector>

#include "core/pixel.hpp"

namespace uagfx {

// D3DDEVICEDESC7 bits (d3dcaps.h / ddraw.h values); src/win/dx7/caps.cpp checks them against the real headers.
namespace d3dcaps {
constexpr uint32_t DEV_FLOATTLVERTEX = 0x1;
constexpr uint32_t DEV_TEXTUREVIDEOMEMORY = 0x200;
constexpr uint32_t DEV_DRAWPRIMTLVERTEX = 0x400;
constexpr uint32_t DEV_CANRENDERAFTERFLIP = 0x800;
constexpr uint32_t DEV_DRAWPRIMITIVES2 = 0x2000;
constexpr uint32_t DEV_DRAWPRIMITIVES2EX = 0x8000;
constexpr uint32_t DEV_HWRASTERIZATION = 0x80000;
constexpr uint32_t MISC_CULLNONE = 0x10;
constexpr uint32_t MISC_CULLCW = 0x20;
constexpr uint32_t MISC_CULLCCW = 0x40;
constexpr uint32_t RASTER_DITHER = 0x1;
constexpr uint32_t RASTER_ZTEST = 0x10;
constexpr uint32_t RASTER_SUBPIXEL = 0x20;
constexpr uint32_t RASTER_FOGVERTEX = 0x80;
constexpr uint32_t RASTER_ZBUFFERLESSHSR = 0x8000;
constexpr uint32_t CMP_ALL = 0xFF;      // NEVER ... ALWAYS
constexpr uint32_t BLEND_ALL = 0x1FFF;  // ZERO ... BOTHINVSRCALPHA
constexpr uint32_t SHADE_COLORGOURAUDRGB = 0x8;
constexpr uint32_t SHADE_SPECULARGOURAUDRGB = 0x200;
constexpr uint32_t SHADE_ALPHAGOURAUDBLEND = 0x4000;
constexpr uint32_t SHADE_FOGGOURAUD = 0x80000;
constexpr uint32_t TEXTURE_PERSPECTIVE = 0x1;
constexpr uint32_t TEXTURE_ALPHA = 0x4;
constexpr uint32_t TEXTURE_TRANSPARENCY = 0x8;
constexpr uint32_t TEXTURE_BORDER = 0x10;
constexpr uint32_t FILTER_NEAREST = 0x1;
constexpr uint32_t FILTER_LINEAR = 0x2;
constexpr uint32_t FILTER_MIPNEAREST = 0x4;
constexpr uint32_t FILTER_MIPLINEAR = 0x8;
constexpr uint32_t FILTER_LINEARMIPNEAREST = 0x10;
constexpr uint32_t FILTER_LINEARMIPLINEAR = 0x20;
constexpr uint32_t FILTER_MINFPOINT = 0x100;
constexpr uint32_t FILTER_MINFLINEAR = 0x200;
constexpr uint32_t FILTER_MIPFPOINT = 0x10000;
constexpr uint32_t FILTER_MIPFLINEAR = 0x20000;
constexpr uint32_t FILTER_MAGFPOINT = 0x1000000;
constexpr uint32_t FILTER_MAGFLINEAR = 0x2000000;
constexpr uint32_t ADDRESS_WRAP = 0x1;
constexpr uint32_t ADDRESS_MIRROR = 0x2;
constexpr uint32_t ADDRESS_CLAMP = 0x4;
constexpr uint32_t ADDRESS_BORDER = 0x8;
constexpr uint32_t ADDRESS_INDEPENDENTUV = 0x10;
constexpr uint32_t TEXOP_PREMODULATE = 0x10000;
constexpr uint32_t TEXOP_MODULATEALPHA_ADDCOLOR = 0x20000;
constexpr uint32_t TEXOP_BUMPENVMAP = 0x200000;
constexpr uint32_t BITDEPTH_16 = 0x400;  // DDBD_16
constexpr uint32_t BITDEPTH_32 = 0x100;  // DDBD_32
}  // namespace d3dcaps

struct DeviceCaps {
    uint32_t devCaps = 0;
    uint32_t miscCaps = 0;
    uint32_t rasterCaps = 0;
    uint32_t zCmpCaps = 0;
    uint32_t srcBlendCaps = 0;
    uint32_t destBlendCaps = 0;
    uint32_t alphaCmpCaps = 0;
    uint32_t shadeCaps = 0;
    uint32_t textureCaps = 0;
    uint32_t filterCaps = 0;
    uint32_t addressCaps = 0;
    uint32_t renderBitDepth = 0;
    uint32_t zBufferBitDepth = 0;
    uint32_t minTextureSize = 0;
    uint32_t maxTextureSize = 0;
    uint32_t maxTextureRepeat = 0;
    uint32_t maxAspectRatio = 0;
    uint32_t maxAnisotropy = 0;
    uint32_t fvfCaps = 0;
    uint32_t textureOpCaps = 0;
    uint16_t maxBlendStages = 0;
    uint16_t maxSimultaneousTextures = 0;
};

// What IDirect3DDevice7::GetCaps reports: what the client checks (surfaces-present-caps.md §5) and what the
// renderer really does (states.md §8): two texture stages, the shader's stage operations, every blend factor.
DeviceCaps deviceCaps();

// EnumTextureFormats: only 16-bit formats, like the cards of its time, so the client picks what it always did
// (spec §4.4); the client's A1R5G5B5 and A4R4G4B4 first. The GPU keeps every texture as RGBA8 anyway.
const std::vector<PixelFormat>& textureFormats();

}  // namespace uagfx
```

`src/core/device_caps.cpp`:
```cpp
#include "core/device_caps.hpp"

#include "core/d3d7.hpp"

namespace uagfx {
namespace {

// The stage operations combine() in the pixel shader implements (Task 7): all of 1-21 except PREMODULATE.
uint32_t shaderTextureOps() {
    uint32_t ops = 0;
    for (uint32_t op = d3d7::TOP_DISABLE; op <= d3d7::TOP_MODULATEINVCOLOR_ADDALPHA; ++op)
        if (op != d3d7::TOP_PREMODULATE) ops |= 1u << (op - 1);
    return ops;
}

}  // namespace

DeviceCaps deviceCaps() {
    using namespace d3dcaps;
    DeviceCaps c;
    c.devCaps = DEV_FLOATTLVERTEX | DEV_TEXTUREVIDEOMEMORY | DEV_DRAWPRIMTLVERTEX | DEV_CANRENDERAFTERFLIP |
                DEV_DRAWPRIMITIVES2 | DEV_DRAWPRIMITIVES2EX | DEV_HWRASTERIZATION;
    c.miscCaps = MISC_CULLNONE | MISC_CULLCW | MISC_CULLCCW;
    c.rasterCaps = RASTER_DITHER | RASTER_ZTEST | RASTER_SUBPIXEL | RASTER_FOGVERTEX;
    c.zCmpCaps = CMP_ALL;
    c.srcBlendCaps = BLEND_ALL;
    c.destBlendCaps = BLEND_ALL;
    c.alphaCmpCaps = CMP_ALL;
    c.shadeCaps = SHADE_COLORGOURAUDRGB | SHADE_SPECULARGOURAUDRGB | SHADE_ALPHAGOURAUDBLEND | SHADE_FOGGOURAUD;
    c.textureCaps = TEXTURE_PERSPECTIVE | TEXTURE_ALPHA | TEXTURE_TRANSPARENCY | TEXTURE_BORDER;
    c.filterCaps = FILTER_NEAREST | FILTER_LINEAR | FILTER_MIPNEAREST | FILTER_MIPLINEAR | FILTER_LINEARMIPNEAREST |
                   FILTER_LINEARMIPLINEAR | FILTER_MINFPOINT | FILTER_MINFLINEAR | FILTER_MIPFPOINT |
                   FILTER_MIPFLINEAR | FILTER_MAGFPOINT | FILTER_MAGFLINEAR;
    c.addressCaps = ADDRESS_WRAP | ADDRESS_MIRROR | ADDRESS_CLAMP | ADDRESS_BORDER | ADDRESS_INDEPENDENTUV;
    c.renderBitDepth = BITDEPTH_16 | BITDEPTH_32;
    c.zBufferBitDepth = BITDEPTH_16;
    c.minTextureSize = 1;
    c.maxTextureSize = 4096;
    c.maxTextureRepeat = 8192;
    c.maxAspectRatio = 0;
    c.maxAnisotropy = 1;  // anisotropic filtering arrives with stage 4 of the spec
    c.fvfCaps = 2;        // two texture coordinate sets (FVF 0x2C4)
    c.textureOpCaps = shaderTextureOps();
    c.maxBlendStages = 2;
    c.maxSimultaneousTextures = 2;
    return c;
}

const std::vector<PixelFormat>& textureFormats() {
    static const std::vector<PixelFormat> formats = {PixelFormat::A1R5G5B5, PixelFormat::A4R4G4B4,
                                                     PixelFormat::R5G6B5, PixelFormat::X1R5G5B5};
    return formats;
}

}  // namespace uagfx
```

Run: `cmake --build build-host -j && ./build-host/uagfx_tests devcaps`
Expected: `PASS devcaps_what_the_client_checks`, `PASS devcaps_texture_ops_are_the_shaders`,
`PASS devcaps_texture_formats_start_with_the_clients`, `3 tests, 0 failed checks`.

- [ ] **Step 4: D3DDEVICEDESC7 из ядра**

`src/win/dx7/caps.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <d3d.h>

namespace uagfx::dx7 {

void fillDeviceDesc(D3DDEVICEDESC7& d);  // IDirect3DDevice7::GetCaps

}  // namespace uagfx::dx7
```

`src/win/dx7/caps.cpp`:
```cpp
#include "win/dx7/caps.hpp"

#include <cstddef>
#include <cstring>

#include "core/device_caps.hpp"

namespace uagfx::dx7 {

// The client reads these fields by offset (surfaces-present-caps.md §5): the header has to agree.
static_assert(offsetof(D3DDEVICEDESC7, dpcTriCaps) + offsetof(D3DPRIMCAPS, dwRasterCaps) == 0x44, "RasterCaps");
static_assert(offsetof(D3DDEVICEDESC7, dpcTriCaps) + offsetof(D3DPRIMCAPS, dwTextureFilterCaps) == 0x60, "FilterCaps");
static_assert(offsetof(D3DDEVICEDESC7, dwMinTextureWidth) == 0x7C, "dwMinTextureWidth");
static_assert(offsetof(D3DDEVICEDESC7, dwMaxTextureWidth) == 0x84, "dwMaxTextureWidth");
static_assert(offsetof(D3DDEVICEDESC7, dwMaxTextureAspectRatio) == 0x90, "dwMaxTextureAspectRatio");
static_assert(offsetof(D3DDEVICEDESC7, dwTextureOpCaps) == 0xB4, "dwTextureOpCaps");
static_assert(offsetof(D3DDEVICEDESC7, wMaxTextureBlendStages) == 0xB8, "wMaxTextureBlendStages");
// The core's numbers are the headers'.
static_assert(d3dcaps::DEV_HWRASTERIZATION == D3DDEVCAPS_HWRASTERIZATION, "");
static_assert(d3dcaps::DEV_DRAWPRIMITIVES2EX == D3DDEVCAPS_DRAWPRIMITIVES2EX, "");
static_assert(d3dcaps::MISC_CULLCCW == D3DPMISCCAPS_CULLCCW, "");
static_assert(d3dcaps::RASTER_FOGVERTEX == D3DPRASTERCAPS_FOGVERTEX, "");
static_assert(d3dcaps::RASTER_ZBUFFERLESSHSR == D3DPRASTERCAPS_ZBUFFERLESSHSR, "");
static_assert(d3dcaps::BLEND_ALL == (D3DPBLENDCAPS_BOTHINVSRCALPHA << 1) - 1, "");
static_assert(d3dcaps::CMP_ALL == (D3DPCMPCAPS_ALWAYS << 1) - 1, "");
static_assert(d3dcaps::SHADE_FOGGOURAUD == D3DPSHADECAPS_FOGGOURAUD, "");
static_assert(d3dcaps::TEXTURE_BORDER == D3DPTEXTURECAPS_BORDER, "");
static_assert(d3dcaps::FILTER_LINEARMIPLINEAR == D3DPTFILTERCAPS_LINEARMIPLINEAR, "");
static_assert(d3dcaps::FILTER_MAGFLINEAR == D3DPTFILTERCAPS_MAGFLINEAR, "");
static_assert(d3dcaps::ADDRESS_INDEPENDENTUV == D3DPTADDRESSCAPS_INDEPENDENTUV, "");
static_assert(d3dcaps::TEXOP_PREMODULATE == D3DTEXOPCAPS_PREMODULATE, "");
static_assert(d3dcaps::TEXOP_MODULATEALPHA_ADDCOLOR == D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR, "");
static_assert(d3dcaps::TEXOP_BUMPENVMAP == D3DTEXOPCAPS_BUMPENVMAP, "");
static_assert(d3dcaps::BITDEPTH_16 == DDBD_16 && d3dcaps::BITDEPTH_32 == DDBD_32, "");

void fillDeviceDesc(D3DDEVICEDESC7& d) {
    const DeviceCaps c = deviceCaps();
    std::memset(&d, 0, sizeof d);
    d.dwDevCaps = c.devCaps;
    D3DPRIMCAPS p;
    std::memset(&p, 0, sizeof p);
    p.dwSize = sizeof p;
    p.dwMiscCaps = c.miscCaps;
    p.dwRasterCaps = c.rasterCaps;
    p.dwZCmpCaps = c.zCmpCaps;
    p.dwSrcBlendCaps = c.srcBlendCaps;
    p.dwDestBlendCaps = c.destBlendCaps;
    p.dwAlphaCmpCaps = c.alphaCmpCaps;
    p.dwShadeCaps = c.shadeCaps;
    p.dwTextureCaps = c.textureCaps;
    p.dwTextureFilterCaps = c.filterCaps;
    p.dwTextureAddressCaps = c.addressCaps;
    d.dpcLineCaps = p;
    d.dpcTriCaps = p;
    d.dwDeviceRenderBitDepth = c.renderBitDepth;
    d.dwDeviceZBufferBitDepth = c.zBufferBitDepth;
    d.dwMinTextureWidth = c.minTextureSize;
    d.dwMinTextureHeight = c.minTextureSize;
    d.dwMaxTextureWidth = c.maxTextureSize;
    d.dwMaxTextureHeight = c.maxTextureSize;
    d.dwMaxTextureRepeat = c.maxTextureRepeat;
    d.dwMaxTextureAspectRatio = c.maxAspectRatio;
    d.dwMaxAnisotropy = c.maxAnisotropy;
    d.dwFVFCaps = c.fvfCaps;
    d.dwTextureOpCaps = c.textureOpCaps;
    d.wMaxTextureBlendStages = c.maxBlendStages;
    d.wMaxSimultaneousTextures = c.maxSimultaneousTextures;
    d.dvMaxVertexW = 1e10f;
    d.deviceGUID = IID_IDirect3DHALDevice;
}

}  // namespace uagfx::dx7
```

- [ ] **Step 5: IDirect3D7 и IDirect3DDevice7**

`src/win/dx7/d3d7.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <d3d.h>

#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

// IDirect3D7, from IDirectDraw7::QueryInterface. Its own reference count; the device does not hold it.
class Direct3D7 final : public Direct3D7Base {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE CreateDevice(REFCLSID rclsid, IDirectDrawSurface7* surface,
                                           IDirect3DDevice7** device) override;
    HRESULT STDMETHODCALLTYPE EnumZBufferFormats(REFCLSID deviceIid, LPD3DENUMPIXELFORMATSCALLBACK cb,
                                                 void* ctx) override;
    HRESULT STDMETHODCALLTYPE EvictManagedTextures() override;

private:
    LONG refs_ = 1;
};

}  // namespace uagfx::dx7
```

`src/win/dx7/device7.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <d3d.h>

#include "core/d3d7.hpp"
#include "core/states.hpp"
#include "win/dx7/com_bases.hpp"

namespace uagfx::dx7 {

class Surface7;

// IDirect3DDevice7: keeps the D3D7 states and bound textures, and turns Clear, SetViewport and the draws into the
// frame record (Task 2) that the renderer plays on the GPU. XYZRHW vertices come transformed and lit, so
// transforms, lights and materials are accepted and ignored (states.md §4, §6). It holds references to its target
// and to the bound textures; nothing holds it, so the client's last Release returns 0 (surfaces-present-caps.md §7).
class Device7 final : public Device7Base {
public:
    explicit Device7(Surface7* target);
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC7* desc) override;
    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void* ctx) override;
    HRESULT STDMETHODCALLTYPE BeginScene() override;
    HRESULT STDMETHODCALLTYPE EndScene() override;
    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirectDrawSurface7* surface, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirectDrawSurface7** surface) override;
    HRESULT STDMETHODCALLTYPE Clear(DWORD count, D3DRECT* rects, DWORD flags, D3DCOLOR color, D3DVALUE z,
                                    DWORD stencil) override;
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX* matrix) override;
    HRESULT STDMETHODCALLTYPE SetViewport(D3DVIEWPORT7* data) override;
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT7* data) override;
    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL7* data) override;
    HRESULT STDMETHODCALLTYPE SetLight(DWORD idx, D3DLIGHT7* data) override;
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override;
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE state, LPDWORD value) override;
    HRESULT STDMETHODCALLTYPE PreLoad(IDirectDrawSurface7* surface) override;
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, void* vertices, DWORD vertexCount,
                                            DWORD flags) override;
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, void* vertices,
                                                   DWORD vertexCount, WORD* indices, DWORD indexCount,
                                                   DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD stage, IDirectDrawSurface7** surface) override;
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirectDrawSurface7* surface) override;
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, LPDWORD value) override;
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) override;
    HRESULT STDMETHODCALLTYPE ValidateDevice(LPDWORD passes) override;
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD idx, WINBOOL enable) override;

private:
    ~Device7() override;
    HRESULT record(D3DPRIMITIVETYPE type, DWORD fvf, const void* vertices, DWORD vertexCount, const WORD* indices,
                   DWORD indexCount);

    LONG refs_ = 1;
    Surface7* target_;
    Surface7* textures_[d3d7::kStages] = {};
    DeviceStates states_;
    D3DVIEWPORT7 viewport_{};
};

}  // namespace uagfx::dx7
```

`src/win/dx7/d3d7.cpp`:
```cpp
#include "win/dx7/d3d7.hpp"

#include <cstring>

#include "win/context.hpp"
#include "win/dx7/device7.hpp"
#include "win/dx7/surface7.hpp"
#include "win/log.hpp"

namespace uagfx::dx7 {

HRESULT STDMETHODCALLTYPE Direct3D7::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirect3D7) {
        *out = static_cast<IDirect3D7*>(this);
        AddRef();
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirect3D7", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Direct3D7::AddRef() {
    win::Guard guard;
    return ULONG(++refs_);
}

ULONG STDMETHODCALLTYPE Direct3D7::Release() {
    win::Guard guard;
    if (refs_ <= 0) return 0;
    const LONG n = --refs_;
    if (n == 0) delete this;
    return ULONG(n);
}

HRESULT STDMETHODCALLTYPE Direct3D7::CreateDevice(REFCLSID, IDirectDrawSurface7* surface, IDirect3DDevice7** device) {
    win::Guard guard;
    if (!device) return DDERR_INVALIDPARAMS;
    *device = nullptr;
    Surface7* target = Surface7::from(surface);
    if (!target || target->role() != SurfaceRole::RenderTarget) return DDERR_INVALIDPARAMS;
    *device = new Device7(target);  // any device GUID: HAL, TnLHAL and RGB all get the same device
    const win::Context& c = win::Context::get();
    LOG_INFO("Direct3D device on the %dx%d render target", c.shadow.width(), c.shadow.height());
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Direct3D7::EnumZBufferFormats(REFCLSID, LPD3DENUMPIXELFORMATSCALLBACK cb, void* ctx) {
    if (!cb) return DDERR_INVALIDPARAMS;
    DDPIXELFORMAT pf;  // D16, which the client takes (surfaces-present-caps.md §6); the GPU depth is D32 anyway
    std::memset(&pf, 0, sizeof pf);
    pf.dwSize = sizeof pf;
    pf.dwFlags = DDPF_ZBUFFER;
    pf.dwZBufferBitDepth = 16;
    pf.dwZBitMask = 0xFFFF;
    cb(&pf, ctx);
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Direct3D7::EvictManagedTextures() { return D3D_OK; }

}  // namespace uagfx::dx7
```

`src/win/dx7/device7.cpp`:
```cpp
#include "win/dx7/device7.hpp"

#include "core/device_caps.hpp"
#include "core/frame.hpp"
#include "win/context.hpp"
#include "win/dx7/caps.hpp"
#include "win/dx7/formats.hpp"
#include "win/dx7/surface7.hpp"
#include "win/log.hpp"

namespace uagfx::dx7 {

Device7::Device7(Surface7* target) : target_(target) {
    target_->AddRef();
    const win::Context& c = win::Context::get();  // D3D7's default viewport: the whole target
    viewport_.dwWidth = DWORD(c.shadow.width());
    viewport_.dwHeight = DWORD(c.shadow.height());
    viewport_.dvMaxZ = 1.0f;
}

Device7::~Device7() {
    for (Surface7*& t : textures_)
        if (t) t->Release();
    target_->Release();
}

HRESULT STDMETHODCALLTYPE Device7::QueryInterface(REFIID riid, void** out) {
    if (!out) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == IID_IDirect3DDevice7) {
        *out = static_cast<IDirect3DDevice7*>(this);
        AddRef();
        return S_OK;
    }
    *out = nullptr;
    unknownInterface("IDirect3DDevice7", riid);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Device7::AddRef() {
    win::Guard guard;
    return ULONG(++refs_);
}

ULONG STDMETHODCALLTYPE Device7::Release() {
    win::Guard guard;
    if (refs_ <= 0) return 0;
    const LONG n = --refs_;
    if (n == 0) delete this;
    return ULONG(n);
}

HRESULT STDMETHODCALLTYPE Device7::GetCaps(D3DDEVICEDESC7* desc) {
    if (!desc) return DDERR_INVALIDPARAMS;
    fillDeviceDesc(*desc);
    return D3D_OK;  // exactly 0: otherwise the client keeps a max texture size of 0 (surfaces-present-caps.md §5)
}

HRESULT STDMETHODCALLTYPE Device7::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void* ctx) {
    if (!cb) return DDERR_INVALIDPARAMS;
    for (PixelFormat f : textureFormats()) {
        DDPIXELFORMAT pf;
        fillPixelFormat(pf, f);
        if (cb(&pf, ctx) == D3DENUMRET_CANCEL) break;  // the client stops at its match
    }
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::BeginScene() { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::EndScene() {
    win::Guard guard;
    win::Context::get().flush();  // the GPU starts on the frame while the client gets ready to present it
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetRenderTarget(IDirectDrawSurface7* surface, DWORD) {
    win::Guard guard;
    Surface7* t = Surface7::from(surface);
    if (!t || t->role() != SurfaceRole::RenderTarget) return DDERR_INVALIDPARAMS;
    t->AddRef();  // there is one GPU target; its depth stands for the attached Z-buffer
    target_->Release();
    target_ = t;
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetRenderTarget(IDirectDrawSurface7** surface) {
    win::Guard guard;
    if (!surface) return DDERR_INVALIDPARAMS;
    target_->AddRef();
    *surface = target_;
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::Clear(DWORD count, D3DRECT* rects, DWORD flags, D3DCOLOR color, D3DVALUE z, DWORD) {
    win::Guard guard;
    win::Context& c = win::Context::get();
    ClearCmd cmd;
    cmd.flags = flags & (d3d7::CLEAR_TARGET | d3d7::CLEAR_ZBUFFER);  // no stencil
    cmd.argb = color;
    cmd.z = z;
    if (!cmd.flags) return D3D_OK;
    const Rect vp{int(viewport_.dwX), int(viewport_.dwY), int(viewport_.dwX + viewport_.dwWidth),
                  int(viewport_.dwY + viewport_.dwHeight)};
    if (count == 0 || !rects) {  // the whole viewport (the calibration clear)
        cmd.rect = vp;
        if (clipRect(Rect{0, 0, c.shadow.width(), c.shadow.height()}, cmd.rect)) c.record.clear(cmd);
        return D3D_OK;
    }
    for (DWORD i = 0; i < count; ++i) {
        cmd.rect = Rect{int(rects[i].x1), int(rects[i].y1), int(rects[i].x2), int(rects[i].y2)};
        if (clipRect(vp, cmd.rect)) c.record.clear(cmd);
    }
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetTransform(D3DTRANSFORMSTATETYPE, D3DMATRIX*) { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::SetViewport(D3DVIEWPORT7* data) {
    win::Guard guard;
    if (!data) return DDERR_INVALIDPARAMS;
    viewport_ = *data;
    ViewportCmd v;
    v.rect = Rect{int(data->dwX), int(data->dwY), int(data->dwX + data->dwWidth), int(data->dwY + data->dwHeight)};
    v.minZ = data->dvMinZ;
    v.maxZ = data->dvMaxZ;
    win::Context::get().record.viewport(v);
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetViewport(D3DVIEWPORT7* data) {
    win::Guard guard;
    if (!data) return DDERR_INVALIDPARAMS;
    *data = viewport_;
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetMaterial(D3DMATERIAL7*) { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::SetLight(DWORD, D3DLIGHT7*) { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::LightEnable(DWORD, WINBOOL) { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::SetRenderState(D3DRENDERSTATETYPE state, DWORD value) {
    win::Guard guard;
    states_.setRender(uint32_t(state), value);
    static bool keyLogged = false;
    if (uint32_t(state) == d3d7::RS_COLORKEYENABLE && value && !keyLogged) {  // spec §4.5
        LOG_INFO("COLORKEYENABLE on: only textures created with a colour key drop their keyed texels");
        keyLogged = true;
    }
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetRenderState(D3DRENDERSTATETYPE state, LPDWORD value) {
    win::Guard guard;
    if (!value) return DDERR_INVALIDPARAMS;
    *value = states_.render(uint32_t(state));
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::PreLoad(IDirectDrawSurface7*) { return D3D_OK; }

HRESULT STDMETHODCALLTYPE Device7::DrawPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, void* vertices, DWORD vertexCount,
                                                DWORD) {
    win::Guard guard;
    return record(type, fvf, vertices, vertexCount, nullptr, 0);
}

HRESULT STDMETHODCALLTYPE Device7::DrawIndexedPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, void* vertices,
                                                       DWORD vertexCount, WORD* indices, DWORD indexCount, DWORD) {
    win::Guard guard;
    if (!indices || indexCount == 0) return DDERR_INVALIDPARAMS;
    return record(type, fvf, vertices, vertexCount, indices, indexCount);
}

HRESULT Device7::record(D3DPRIMITIVETYPE type, DWORD fvf, const void* vertices, DWORD vertexCount,
                        const WORD* indices, DWORD indexCount) {
    if (fvf != d3d7::FVF_TL1 && fvf != d3d7::FVF_TL2)
        return notImplemented("IDirect3DDevice7::DrawPrimitive (a vertex format other than 0x1C4 and 0x2C4)");
    if (!vertices || vertexCount == 0) return DDERR_INVALIDPARAMS;
    win::Context& c = win::Context::get();
    DrawCmd cmd;
    bool hasTexture[2] = {false, false};
    for (int i = 0; i < 2; ++i) {
        Surface7* t = textures_[i];
        if (!t) continue;
        t->sync(c.record);  // the texture's changes go before the draw that samples it
        hasTexture[i] = true;
        cmd.texture[i] = t->gpuTexture();
        cmd.keyed[i] = t->colorKeyed();
    }
    cmd.state = c.record.addState(resolveDrawSetup(states_, false, hasTexture));  // the target has no alpha
    cmd.primitive = uint32_t(type);
    cmd.fvf = fvf;
    c.record.draw(cmd, vertices, vertexCount, indices, indices ? indexCount : 0);
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetTexture(DWORD stage, IDirectDrawSurface7** surface) {
    win::Guard guard;
    if (!surface || stage >= d3d7::kStages) return DDERR_INVALIDPARAMS;
    *surface = textures_[stage];
    if (textures_[stage]) textures_[stage]->AddRef();
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetTexture(DWORD stage, IDirectDrawSurface7* surface) {
    win::Guard guard;
    if (stage >= d3d7::kStages) return DDERR_INVALIDPARAMS;
    Surface7* t = Surface7::from(surface);
    if (surface && (!t || t->role() != SurfaceRole::Texture)) return DDERR_INVALIDPARAMS;
    if (t) t->AddRef();
    if (textures_[stage]) textures_[stage]->Release();
    textures_[stage] = t;  // NULL on stage 0 samples white (states.md §5)
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, LPDWORD value) {
    win::Guard guard;
    if (!value || stage >= d3d7::kStages) return DDERR_INVALIDPARAMS;
    *value = states_.stage(stage, uint32_t(type));
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) {
    win::Guard guard;
    if (stage >= d3d7::kStages) return DDERR_INVALIDPARAMS;
    states_.setStage(stage, uint32_t(type), value);
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE Device7::ValidateDevice(LPDWORD passes) {
    if (passes) *passes = 1;
    return D3D_OK;
}

}  // namespace uagfx::dx7
```

- [ ] **Step 6: DirectDraw отдаёт IDirect3D7**

В `src/win/dx7/ddraw7.cpp` добавить `#include "win/dx7/d3d7.hpp"` к остальным `#include "win/dx7/…"` и в
`DirectDraw7::QueryInterface` перед строкой `*out = nullptr;` (той, что перед `unknownInterface`):
```cpp
    if (riid == IID_IDirect3D7) {  // right after the surfaces (surfaces-present-caps.md §7, L2951)
        *out = static_cast<IDirect3D7*>(new Direct3D7);
        return S_OK;
    }
```

- [ ] **Step 7: Собирается, тесты ядра зелёные**

Run: `cmake --build build-win -j 2>&1 | grep -E "warning|error" ; python3 tools/check_dll.py && cmake --build build-host -j && ./build-host/uagfx_tests | tail -1`
Expected: ни одной строки `warning`/`error` (в том числе ни один `static_assert` из `caps.cpp` не сработал);
`OK: build-win/ddraw.dll: 22 exports, …`; последняя строка тестов — `… tests, 0 failed checks`.

- [ ] **Step 8: Commit**

```bash
git add src/core/device_caps.hpp src/core/device_caps.cpp tests/test_device_caps.cpp src/win/dx7/caps.hpp src/win/dx7/caps.cpp src/win/dx7/d3d7.hpp src/win/dx7/d3d7.cpp src/win/dx7/device7.hpp src/win/dx7/device7.cpp src/win/dx7/ddraw7.cpp
git commit -m "Устройство Direct3D 7: возможности для клиента, форматы текстур и Z, состояния, рисование в запись кадра

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 13: Включение рендера uaRO: DirectDrawCreateEx, откат, версия 0.2.0, пакет и чек-лист

**Files:**
- Modify: `src/core/version.cpp`, `tests/test_version.cpp`, `src/win/real_ddraw.hpp`, `src/win/real_ddraw.cpp`,
  `src/win/exports.cpp`, `dist/uaro-graphics.ini`, `dist/install.bat`, `dist/TEST.ru.md`, `tools/package.sh`,
  `tools/check_package.py`
- Create: `dist/use-uaro.bat`, `dist/use-dgvoodoo.bat`, `release/uaro-graphics-0.2.0.zip`
- Delete: `release/uaro-graphics-0.1.1.zip`

**Interfaces:**
- Consumes: `chooseRenderer`, `Renderer`, `RendererChoice` (1A), `win::Context::open/config`, `win::Guard` (Task 9),
  `dx7::createDirectDraw` (Task 11).
- Produces:
  ```cpp
  namespace uagfx::win {
  struct Startup { Config config; RendererChoice choice; };
  const Startup& startup();  // the settings, the log and the renderer, decided once on the first export call
  HMODULE realModule();      // dgVoodoo or the system DirectDraw: for backend = dgvoodoo and as the fallback
  }
  ```

- [ ] **Step 1: Падающий тест версии**

`tests/test_version.cpp` целиком:
```cpp
#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_2_0) { CHECK(std::strcmp(uagfx::kVersion, "0.2.0") == 0); }
```

Run: `cmake --build build-host -j && ./build-host/uagfx_tests version`
Expected: `FAIL version_is_0_2_0`, `1 tests, 1 failed checks`.

- [ ] **Step 2: Версия 0.2.0**

В `src/core/version.cpp` строку `const char* const kVersion = "0.1.1";` заменить на
`const char* const kVersion = "0.2.0";`.

Run: `cmake --build build-host -j && ./build-host/uagfx_tests version`
Expected: `PASS version_is_0_2_0`, `1 tests, 0 failed checks`.

- [ ] **Step 3: Решение при запуске отдельно от загрузки чужого DirectDraw**

`src/win/real_ddraw.hpp` целиком:
```cpp
#pragma once
#include <windows.h>

#include "core/config.hpp"
#include "core/renderer_choice.hpp"

namespace uagfx::win {

// What the first call of any export decides, once: the settings, the log and the renderer.
struct Startup {
    Config config;
    RendererChoice choice;
};
const Startup& startup();

// The DirectDraw calls go to when the uaRO renderer is not used: dgVoodoo or the system one. With the uaRO
// renderer chosen it is the fallback (dgVoodoo when it is there). Loaded on the first call that needs it, never in
// DllMain (no LoadLibrary under the loader lock).
HMODULE realModule();
FARPROC realProc(const char* name);  // nullptr when the module or the function is missing
// The same, but never starts loading: for the private exports that d3dim700.dll / dgVoodoo's D3DImm.dll call
// while the real DirectDraw is being set up. nullptr until something has loaded it.
FARPROC loadedProc(const char* name);
// Called from DllMain on DLL_PROCESS_DETACH: which DirectDraw modules the game ended up with.
void logModulesAtExit();

}  // namespace uagfx::win
```

`src/win/real_ddraw.cpp` целиком:
```cpp
#include "win/real_ddraw.hpp"

#include <string>

#include "core/version.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/system_probe.hpp"

namespace uagfx::win {
namespace {

HMODULE g_loaded = nullptr;  // set once the real DirectDraw is loaded

// 0.2.0 has the uaRO renderer. It is opt-in (backend = uaro); dgVoodoo stays the default.
constexpr bool kUaroBuilt = true;

void logModule(const char* name) {
    if (HMODULE m = GetModuleHandleA(name)) LOG_INFO("%s: %s", name, modulePath(m).c_str());
}

// dgVoodoo's DDraw.dll in the dgvoodoo folder; its D3DImm.dll sits next to it, so that folder is added to the
// DLL search path for the rest of the process.
HMODULE loadDgVoodoo(const std::string& dir) {
    const std::string folder = dir + "\\dgvoodoo";
    if (!SetDllDirectoryA(folder.c_str())) LOG_ERROR("SetDllDirectory(%s) failed (error %lu)", folder.c_str(), GetLastError());
    const std::string path = folder + "\\DDraw.dll";
    HMODULE m = LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m) LOG_ERROR("cannot load %s (error %lu)", path.c_str(), GetLastError());
    return m;
}

HMODULE loadSystem() {
    const std::string path = systemDir() + "\\ddraw.dll";
    HMODULE m = LoadLibraryA(path.c_str());
    if (!m) LOG_ERROR("cannot load %s (error %lu)", path.c_str(), GetLastError());
    return m;
}

Startup decide() {
    Startup s;
    const std::string dir = gameDir();
    std::string ini;
    const bool haveIni = readTextFile(dir + "\\uaro-graphics.ini", ini);
    s.config = parseConfig(ini);
    wlog::open(dir + "\\uaro-graphics.log", s.config.logLevel);
    LOG_INFO("uaRO Graphics %s in %s", kVersion, modulePath(GetModuleHandleA(nullptr)).c_str());
    LOG_INFO("settings: %s; backend = %s, mode = %s", haveIni ? "uaro-graphics.ini" : "no uaro-graphics.ini, defaults",
             backendToken(s.config.backend), displayModeToken(s.config.mode));
    const WindowsVersion v = windowsVersion();
    LOG_INFO("Windows %lu.%lu build %lu", v.major, v.minor, v.build);
    logModule("dinput.dll");   // ROExt or uaRO Pad
    logModule("dinput8.dll");

    SystemFacts facts;
    facts.windows10 = v.major >= 10;
    facts.dgvoodoo = fileExists(dir + "\\dgvoodoo\\DDraw.dll");
    if (facts.windows10) {  // always: backend = uaro needs it, and the log names the adapter either way
        const D3D12Probe p = probeD3D12();
        facts.d3d12 = p.ok;
        LOG_INFO("Direct3D 12: %s, adapter %s (vendor 0x%04x, device 0x%04x, %llu MB), hr 0x%08lx",
                 p.ok ? "OK" : "FAILED", p.adapter.c_str(), p.vendor, p.device, p.vramMb,
                 static_cast<unsigned long>(p.hr));
    }
    s.choice = chooseRenderer(s.config, facts, kUaroBuilt);
    LOG_INFO("renderer: %s (%s)", rendererName(s.choice.renderer), s.choice.reason.c_str());
    return s;
}

HMODULE loadReal() {
    const Startup& s = startup();
    const std::string dir = gameDir();
    HMODULE m = nullptr;
    // dgVoodoo when it was chosen, and as the uaRO renderer's fallback when it is there.
    if (s.choice.renderer != Renderer::System && fileExists(dir + "\\dgvoodoo\\DDraw.dll")) m = loadDgVoodoo(dir);
    if (!m) m = loadSystem();
    if (m) LOG_INFO("passing DirectDraw to %s", modulePath(m).c_str());
    g_loaded = m;
    return m;
}

}  // namespace

const Startup& startup() {
    static const Startup s = decide();
    return s;
}

HMODULE realModule() {
    static HMODULE m = loadReal();
    return m;
}

FARPROC realProc(const char* name) {
    HMODULE m = realModule();
    return m ? GetProcAddress(m, name) : nullptr;
}

FARPROC loadedProc(const char* name) { return g_loaded ? GetProcAddress(g_loaded, name) : nullptr; }

void logModulesAtExit() {
    logModule("D3DImm.dll");  // which D3DImm.dll dgVoodoo's DirectDraw picked up
    logModule("ddraw.dll");
}

}  // namespace uagfx::win
```

- [ ] **Step 4: DirectDrawCreateEx создаёт наш DirectDraw**

В `src/win/exports.cpp`:

1. Список `#include` в начале файла заменить на:
```cpp
#include <windows.h>

#include <ddraw.h>

#include <string>

#include "core/renderer_choice.hpp"
#include "win/context.hpp"
#include "win/dx7/ddraw7.hpp"
#include "win/log.hpp"
#include "win/real_ddraw.hpp"
```

2. В первый `namespace { … }` (где `real<Fn>`) после функции `real` добавить:
```cpp
bool g_uaroFailed = false;  // the uaRO renderer did not start: every later call goes to the other DirectDraw

// DirectDraw of the uaRO renderer. S_FALSE: Direct3D 12 would not start, the caller falls back.
HRESULT createUaro(REFIID iid, LPVOID* out, IUnknown* outer) {
    if (outer) return CLASS_E_NOAGGREGATION;
    uagfx::win::Guard guard;
    uagfx::win::Context& c = uagfx::win::Context::get();
    c.config = uagfx::win::startup().config;
    std::string error;
    if (!c.open(error)) {
        LOG_ERROR("the uaRO renderer could not start (%s): falling back to another DirectDraw", error.c_str());
        return S_FALSE;
    }
    const HRESULT hr = uagfx::dx7::createDirectDraw(iid, out);
    LOG_INFO("DirectDrawCreateEx = 0x%08lx (uaRO renderer)", static_cast<unsigned long>(hr));
    return hr;
}
```

3. Тело `Proxy_DirectDrawCreateEx` заменить на:
```cpp
    if (uagfx::win::startup().choice.renderer == uagfx::Renderer::Uaro && !g_uaroFailed) {
        const HRESULT hr = createUaro(iid, out, outer);
        if (hr != S_FALSE) return hr;
        g_uaroFailed = true;
    }
    using Fn = HRESULT(WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
    const Fn fn = real<Fn>("DirectDrawCreateEx");
    if (!fn) return E_FAIL;
    const HRESULT hr = fn(guid, out, iid, outer);
    LOG_INFO("DirectDrawCreateEx = 0x%08lx", static_cast<unsigned long>(hr));
    return hr;
```

Run: `cmake --build build-win -j 2>&1 | grep -E "warning|error" ; python3 tools/check_dll.py`
Expected: ни одной строки `warning`/`error`; `OK: build-win/ddraw.dll: 22 exports, …` — пролог горячего патча у
`DirectDrawCreateEx` на месте.

- [ ] **Step 5: Падающая проверка пакета**

В `tools/check_package.py`:
- `EXPECTED` заменить на
```python
EXPECTED = {"install.bat", "uninstall.bat", "use-uaro.bat", "use-dgvoodoo.bat", "TEST.ru.md",
            "uaro-graphics/ddraw.dll", "uaro-graphics/uaro-graphics.ini"}
```
- строку `for bat in ("install.bat", "uninstall.bat"):` заменить на
  `for bat in sorted(n for n in names if n.endswith(".bat")):`;
- путь по умолчанию в `main(...)` в конце файла: `"build-win/uaro-graphics-0.2.0.zip"`.

Run: `tools/package.sh`
Expected: `FAIL: layout [...], expected [...]` — в архиве нет `use-uaro.bat` и `use-dgvoodoo.bat`.

- [ ] **Step 6: Переключатели рендера и пакет**

`dist/use-uaro.bat`:
```bat
@echo off
rem uaRO Graphics: the game draws with the uaRO Graphics renderer (Direct3D 12). use-dgvoodoo.bat switches back.
setlocal
cd /d "%~dp0"
if not exist "uaro-graphics.ini" (
    echo uaro-graphics.ini is not in this folder: run install.bat first.
    goto :fail
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='uaro-graphics.ini'; $t=@(Get-Content -LiteralPath $f); if ($t -match '^\s*backend\s*=') { $t = $t -replace '^\s*backend\s*=.*$','backend = uaro' } else { $t += '[renderer]'; $t += 'backend = uaro' }; Set-Content -LiteralPath $f -Value $t -Encoding ASCII"
if errorlevel 1 goto :noaccess
echo.
echo The game now draws with uaRO Graphics (Direct3D 12). Start it as usual.
echo If something looks wrong: send uaro-graphics.log and run use-dgvoodoo.bat to go back.
echo.
pause
exit /b 0

:noaccess
echo.
echo Cannot change uaro-graphics.ini. Right-click use-uaro.bat and choose "Run as administrator".
:fail
echo.
pause
exit /b 1
```

`dist/use-dgvoodoo.bat`:
```bat
@echo off
rem uaRO Graphics: the game draws with dgVoodoo again. use-uaro.bat switches to the uaRO Graphics renderer.
setlocal
cd /d "%~dp0"
if not exist "uaro-graphics.ini" (
    echo uaro-graphics.ini is not in this folder: run install.bat first.
    goto :fail
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='uaro-graphics.ini'; $t=@(Get-Content -LiteralPath $f); if ($t -match '^\s*backend\s*=') { $t = $t -replace '^\s*backend\s*=.*$','backend = dgvoodoo' } else { $t += '[renderer]'; $t += 'backend = dgvoodoo' }; Set-Content -LiteralPath $f -Value $t -Encoding ASCII"
if errorlevel 1 goto :noaccess
echo.
echo The game draws with dgVoodoo again. Start it as usual.
echo.
pause
exit /b 0

:noaccess
echo.
echo Cannot change uaro-graphics.ini. Right-click use-dgvoodoo.bat and choose "Run as administrator".
:fail
echo.
pause
exit /b 1
```

Строки `.bat` — CRLF (cmd.exe): `sed -i 's/\r*$/\r/' dist/use-uaro.bat dist/use-dgvoodoo.bat` (`.gitattributes` их так и
хранит, а пакет собирается из рабочих файлов).

В `dist/install.bat` после строки `echo uaRO Graphics is installed. Start the game as usual.` добавить строку
(тоже с CRLF):
```bat
echo To draw with the uaRO Graphics renderer (Direct3D 12), run use-uaro.bat; use-dgvoodoo.bat switches back.
```

В `dist/uaro-graphics.ini` две строки про `uaro` в разделе `[renderer]` заменить на (CRLF сохранить):
```ini
; uaro     = the uaRO Graphics Direct3D 12 renderer (Windows 10 or newer; if it cannot start, the game
;            uses dgVoodoo and the log says why). use-uaro.bat and use-dgvoodoo.bat switch this line.
```

В `tools/package.sh` строку `cp dist/install.bat dist/uninstall.bat dist/TEST.ru.md "$stage/"` заменить на
`cp dist/install.bat dist/uninstall.bat dist/use-uaro.bat dist/use-dgvoodoo.bat dist/TEST.ru.md "$stage/"`.

Run: `tools/package.sh && file dist/*.bat`
Expected: `OK: build-win/uaro-graphics-0.2.0.zip: TEST.ru.md, install.bat, uaro-graphics/ddraw.dll,
uaro-graphics/uaro-graphics.ini, uninstall.bat, use-dgvoodoo.bat, use-uaro.bat`, `packed …`; у всех четырёх `.bat` —
`with CRLF line terminators`.

- [ ] **Step 7: Чек-лист 0.2.0**

`dist/TEST.ru.md` целиком:
```markdown
# uaRO Graphics 0.2.0 — проверка

Эта версия впервые рисует игру сама — через Direct3D 12, без dgVoodoo. По умолчанию по-прежнему работает dgVoodoo;
наш рендер включается одним щелчком и так же выключается.

## Установка

1. Закрой игру. Распакуй архив в папку клиента — туда, где лежит `uaRO.exe` (с папками, как в архиве).
2. Дважды щёлкни `install.bat`. Если стояла 0.1.1 — он просто заменит наш `ddraw.dll`, настройки не тронет.
3. Дважды щёлкни `use-uaro.bat` — игра переключится на наш рендер. Вернуть dgVoodoo: `use-dgvoodoo.bat`.
   Если клиент в `Program Files` и скрипт пишет «Cannot …», запусти его правой кнопкой → «Запуск от имени
   администратора».

## Проверка

Запусти игру как обычно. Полный экран у нас — окно без рамки на весь монитор (режим монитора не меняется).

1. Заставка-видео при запуске (если она есть), вход и выбор персонажа: картинка как с dgVoodoo, без чёрного
   экрана и мусора.
2. В игре походи по городу и по полю: земля и тени на ней, здания и деревья, персонажи и монстры, надписи, окна
   интерфейса, эффекты умений, туман на картах, где он есть.
3. Смена карты: полоса загрузки видна.
4. Alt+Tab туда и обратно, свернуть и развернуть окно игры — картинка на месте.
5. Скриншот клавишей игры: в файле есть подпись с датой и временем.
6. Плавность: игра не тормозит, рывков нет.
7. ROExt и геймпад: мышь, курсор и окно ведут себя как с dgVoodoo.
8. Выход из игры: без окон с ошибками.
9. По желанию — обычное окно: в `uaro-graphics.ini` поставь `mode = window` (раздел `[display]`), запусти игру,
   подвигай окно, потяни за край. Вернуть: `mode = borderless`.

## Если что-то не так

Сделай снимок экрана (Win+Shift+S), запомни, что делал, и включи dgVoodoo обратно: `use-dgvoodoo.bat`.

## Что прислать

Файл `uaro-graphics.log` из папки клиента и коротко по пунктам 1–9: что так, что не так. В логе раз в 30 секунд
есть строка вида `3600 frames in 60.0 s (60.0 fps), 0 reads of the frame from the GPU` — число чтений во время
обычной игры должно быть около нуля. Если клиент в `Program Files`, Windows кладёт лог в
`%LOCALAPPDATA%\VirtualStore\Program Files (x86)\…` (вставь этот путь в адресную строку Проводника).

## Удаление

Дважды щёлкни `uninstall.bat` — всё вернётся как было.
```

- [ ] **Step 8: Полный прогон и пакет в репозиторий**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests | tail -1 && tools/package.sh && cp build-win/uaro-graphics-0.2.0.zip release/ && git rm -q release/uaro-graphics-0.1.1.zip && ls release`
Expected: `… tests, 0 failed checks`; `OK: build-win/uaro-graphics-0.2.0.zip: …`; в `release/` — только
`uaro-graphics-0.2.0.zip` (копия делается до `git rm`, поэтому папка не исчезает).

- [ ] **Step 9: Commit**

```bash
git add src/core/version.cpp tests/test_version.cpp src/win/real_ddraw.hpp src/win/real_ddraw.cpp src/win/exports.cpp dist/uaro-graphics.ini dist/install.bat dist/use-uaro.bat dist/use-dgvoodoo.bat dist/TEST.ru.md tools/package.sh tools/check_package.py release/uaro-graphics-0.2.0.zip
git commit -m "0.2.0: рендер uaRO (Direct3D 12) по backend = uaro с откатом на dgVoodoo, переключатели use-uaro/use-dgvoodoo, чек-лист

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Пуш и проверку на ПК пользователя делает финальный шаг исполнения (superpowers:finishing-a-development-branch).

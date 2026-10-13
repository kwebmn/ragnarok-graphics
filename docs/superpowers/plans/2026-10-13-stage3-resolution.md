# uaRO Graphics — этап 3: высокое разрешение и масштаб интерфейса (план)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `ddraw.dll` 0.3.0 рисует игру в настоящих точках окна: на мониторе с масштабом Windows (у пользователя 4K и
175 %) мир чётче, окно и интерфейс того же размера, мышь, ROExt и геймпад не меняются; плюс две отложенные мелочи
ревью 1B (M3 — лог сбоев видеодрайвера, M7 — DESTBLEND 12/13).

**Architecture:** игра по-прежнему рисует в своём размере `L`; драйвер рисует в цель в `k` раз больше
(`x' = x · k + 0.5`) и показывает её в swap chain размером с окно в настоящих точках (размер узнаётся через
`SetThreadDpiAwarenessContext` на время одного `GetClientRect`). Вся математика — в ядре с тестами на Linux
(`core/scale`, эталонный растеризатор, признак интерфейса, фильтр `sharp`); рендер D3D12 получает цель `×k`, общий
метод копирования `blit` (показ, чтение кадра назад в размере игры, запись пикселей клиента в большую цель).

**Tech Stack:** C++17, MinGW-w64 i686 GCC 12, Direct3D 12 (SM 5.0, `d3dcompiler_47` во время работы), тесты на Linux.

**Spec:** `docs/superpowers/specs/2026-10-13-stage3-resolution-design.md` (уточнение этапа 3) и
`docs/superpowers/specs/2026-10-13-uaro-graphics-design.md` (§4.2, §4.3, §6, §9). Справки:
`docs/research/{surfaces-present-caps,states,calibration}.md`. Основание — этап 1 (0.2.0), принят пользователем.

## Global Constraints

- Всё из планов 1A/1B в силе: PE32 i386; импорты только `KERNEL32`, `USER32`, `GDI32`, `msvcrt` (D3D12, DXGI,
  `d3dcompiler_47`, `SetThreadDpiAwarenessContext` — только через `LoadLibrary`/`GetProcAddress`); пролог горячего
  патча у `DirectDrawCreateEx`; откат на dgVoodoo при любой ошибке запуска рендера; сборка без предупреждений.
- Версия — `0.3.0`.
- Вершины: `x' = x · k + 0.5`, `y' = y · k + 0.5` в точках цели. Не `(x + 0.5) · k` (уточнение §4).
- До первого показанного кадра `k = 1` (стартовая калибровка клиента видит картинку DX7).
- Процесс игры не переводится в режим DPI: координаты окна, мыши, ROExt и геймпада не меняются. Настоящий размер
  окна — только `GetClientRect` внутри `SetThreadDpiAwarenessContext(PER_MONITOR_AWARE_V2)` на одном потоке.
- `high_resolution = 0` — ровно поведение 0.2.0 (`k = 1`, swap chain в размере окна для Windows).
- Клиент читает и пишет кадр в своём размере `L`: чтение уменьшает цель, запись растягивает (уточнение §5).
- HLSL здесь не компилируется (нет `d3dcompiler` под Linux): в шейдерах — только конструкции SM 5.0; ошибка компиляции
  на Windows видна в логе (`the uaRO renderer could not start (shader …)`) и даёт откат на dgVoodoo.
- Имена в `src/core` не совпадают с макросами Win32 (`DrawState`, `GetObject`, …).
- Стиль и коммиты — как раньше (KWEB Team, русские сообщения, трейлер `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`).

## Review Focus

1. **Калибровка при `k = 1`.** Если игра покажет кадр раньше стартовой калибровки, калибровка пройдёт при `k ≠ 1`, и
   UV интерфейса сдвинутся (мыло навсегда). По справкам калибровка — до первого показа; проверка: строка `scale:` в логе
   после строк калибровки, чек-лист п. 3.
2. **Чтение и запись кадра при `k ≠ 1`** (скриншот с подписью, затемнение, «волна», Bink): размер и содержимое в `L`.
   Код — Task 9; проверка — чек-лист п. 6–7.
3. **Края, швы и точка в точку.** Полоса по левому/верхнему краю, швы между квадами интерфейса, мягкий вывод при
   окне с нецелым `L · k`. Тесты: Task 5 (`raster_full_screen_quad_at_1_75_covers_every_pixel`), Task 4
   (`scale_rect_edges_follow_the_vertices`, `scale_target_is_the_picture_at_100_percent`); показ переводит источник по
   осям отдельно (Task 8); чек-лист п. 2–4.
4. **Мышь, ROExt, геймпад** — координаты те же, что в 0.2.0. Гарантия — Global Constraints (процесс без DPI-режима);
   проверка — чек-лист п. 5.
5. **Смена окна** (размер, переезд на монитор с другим масштабом, сворачивание): `setScale` и swap chain при каждом
   изменении. Код — Task 8; проверка — чек-лист п. 8.

## Как записаны правки

Код в задачах — точный, применяется дословно. Формы (за каждой — блок кода, у замены — два):
- `` `путь`: `` — новый файл (или файл целиком);
- «В `путь` заменить функцию `НАЧАЛО` целиком:» — многострочная функция, первая строка которой начинается с
  `НАЧАЛО`, до первой строки, равной `}`, заменяется блоком (однострочные функции меняются заменой фрагмента);
- «В `путь` после функции `НАЧАЛО` добавить:» — пустая строка и блок после этой функции;
- «В `путь` заменить: … на: …» — фрагмент встречается в файле ровно один раз;
- «В `путь` добавить в конец:».
В `.bat` и `.ini` строки — CRLF (блоки в плане — с обычными переводами строк).

---

### Task 1: M7 — DESTBLEND 12/13 читается как его «исходная» половина

Отложенная мелочь ревью 1B: значения `BOTHSRCALPHA` (12) и `BOTHINVSRCALPHA` (13) определены только для `SRCBLEND`.
В эффектах STR (данные без проверки, `states.md` §3) они бывают и в `DESTBLEND`; сейчас такое значение уходит в D3D12
как есть, конвейер не создаётся, и рисование пропадает. Как в Wine: 12 → `SRCALPHA`, 13 → `INVSRCALPHA`.

**Files:**
- Modify: `src/core/states.cpp`
- Test: `tests/test_states.cpp`

**Interfaces:**
- Consumes: `DeviceStates`, `resolveDrawSetup`, `d3d7::BLEND_*` (1B Task 1).
- Produces: в `DrawSetup::dstBlend` больше не бывает 12 и 13.

- [ ] **Step 1: Падающий тест**

В `tests/test_states.cpp` добавить в конец:
```cpp

TEST(states_both_blends_as_dest_read_as_their_source_half) {
    DeviceStates s = clientInit();
    s.setRender(RS_ALPHABLENDENABLE, 1);
    s.setRender(RS_SRCBLEND, BLEND_ONE);
    s.setRender(RS_DESTBLEND, BLEND_BOTHSRCALPHA);  // STR effect data is not validated (states.md §3)
    DrawSetup d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.srcBlend, uint8_t(BLEND_ONE));
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_SRCALPHA));
    s.setRender(RS_DESTBLEND, BLEND_BOTHINVSRCALPHA);
    d = resolveDrawSetup(s, false, kBoth);
    CHECK_EQ(d.dstBlend, uint8_t(BLEND_INVSRCALPHA));
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests states_both_blends_as_dest`
Expected: `FAIL states_both_blends_as_dest_read_as_their_source_half` (две проверки `dstBlend`), `1 tests, 2 failed checks`.

- [ ] **Step 3: Реализация**

В `src/core/states.cpp` заменить:
```cpp
        } else if (src == BLEND_BOTHINVSRCALPHA) {
            src = BLEND_INVSRCALPHA;
            dst = BLEND_SRCALPHA;
        }
```
на:
```cpp
        } else if (src == BLEND_BOTHINVSRCALPHA) {
            src = BLEND_INVSRCALPHA;
            dst = BLEND_SRCALPHA;
        }
        // BOTH* is defined for SRCBLEND only. As a DESTBLEND (unvalidated STR effect data, states.md §3) it reads as
        // its source half, as in Wine; passed through, D3D12 would refuse the pipeline and the draw would vanish.
        if (dst == BLEND_BOTHSRCALPHA) dst = BLEND_SRCALPHA;
        if (dst == BLEND_BOTHINVSRCALPHA) dst = BLEND_INVSRCALPHA;
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests states && ./build-host/uagfx_tests | tail -1`
Expected: все `PASS states_…`; последняя строка — `… tests, 0 failed checks`.

- [ ] **Step 5: Commit**

```bash
git add src/core/states.cpp tests/test_states.cpp
git commit -m "M7: DESTBLEND BOTHSRCALPHA/BOTHINVSRCALPHA читается как SRCALPHA/INVSRCALPHA, а не ломает конвейер

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 2: M3 — сбои видеодрайвера: в лог один раз и с причиной, без работы GPU после потери устройства

Отложенная мелочь ревью 1B: при сбое показа или потере устройства (сбой драйвера, TDR) строка ошибки писалась каждый
кадр, причина (`GetDeviceRemovedReason`) — нет, а рендер продолжал ждать GPU. Теперь: каждое место и код ошибки — одна
строка; потеря устройства — одна строка с причиной, дальше рендер ничего не отправляет на GPU (картинка замирает,
игра не виснет). `DXGI_STATUS_OCCLUDED` (окно закрыто другим) — короткая пауза вместо холостого цикла.

**Files:**
- Create: `src/core/log_once.hpp`
- Modify: `src/win/gpu/impl.hpp`, `src/win/gpu/device.cpp`
- Test: `tests/test_log_once.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace uagfx { class LogOnce { public: bool first(uint32_t place, uint32_t code); }; }
  // Renderer::Impl: LogOnce failures; bool lost; UINT failedWidth, failedHeight;
  //                 void failed(uint32_t place, const char* what, HRESULT hr);
  ```

- [ ] **Step 1: Падающий тест**

`tests/test_log_once.cpp`:
```cpp
#include "core/log_once.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(logonce_reports_each_failure_once) {
    LogOnce l;
    CHECK(l.first(5, 0x887A0005u));   // Present: DXGI_ERROR_DEVICE_REMOVED
    CHECK(!l.first(5, 0x887A0005u));  // the same every frame after that: once is enough
    CHECK(l.first(5, 0x887A0001u));   // another code at the same place
    CHECK(l.first(3, 0x887A0005u));   // the same code at another place
}
```

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/log_once.hpp: No such file or directory`.

- [ ] **Step 2: `LogOnce`**

`src/core/log_once.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <set>

namespace uagfx {

// Remembers which (place, code) pairs were reported, so a failure that repeats every frame is logged once.
class LogOnce {
public:
    bool first(uint32_t place, uint32_t code) { return seen_.insert(uint64_t(place) << 32 | code).second; }

private:
    std::set<uint64_t> seen_;
};

}  // namespace uagfx
```

Run: `cmake --build build-host -j && ./build-host/uagfx_tests logonce`
Expected: `PASS logonce_reports_each_failure_once`, `1 tests, 0 failed checks`.

- [ ] **Step 3: Рендер — поля**

В `src/win/gpu/impl.hpp` заменить:
```cpp
#include "core/frame.hpp"
#include "win/gpu/renderer.hpp"
```
на:
```cpp
#include "core/frame.hpp"
#include "core/log_once.hpp"
#include "win/gpu/renderer.hpp"
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    bool open = false;
    std::string adapter;
```
на:
```cpp
    bool open = false;
    std::string adapter;
    LogOnce failures;
    bool lost = false;  // the device was removed (driver crash, TDR): no more GPU work, the reason is in the log
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    UINT swapWidth = 0;
    UINT swapHeight = 0;
```
на:
```cpp
    UINT swapWidth = 0;
    UINT swapHeight = 0;
    UINT failedWidth = 0;  // a swap chain of this size could not be made: not retried every frame
    UINT failedHeight = 0;
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    void retireDying();  // after execute() has submitted: the dying textures go with that submission
```
на:
```cpp
    void retireDying();  // after execute() has submitted: the dying textures go with that submission
    void failed(uint32_t place, const char* what, HRESULT hr);
```

- [ ] **Step 4: Рендер — места сбоев**

В `src/win/gpu/device.cpp` заменить функцию `bool Renderer::Impl::begin()` целиком:
```cpp
bool Renderer::Impl::begin() {
    if (open) return true;
    if (lost) return false;
    Slot& s = slots[slot];
    waitSlot(s);
    for (IUnknown* g : s.garbage) g->Release();
    s.garbage.clear();
    s.used = 0;
    HRESULT hr = s.allocator->Reset();
    if (SUCCEEDED(hr)) hr = list->Reset(s.allocator, nullptr);
    if (FAILED(hr)) {
        failed(1, "command list reset", hr);
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
```

В `src/win/gpu/device.cpp` заменить функцию `void Renderer::Impl::submit()` целиком:
```cpp
void Renderer::Impl::submit() {
    if (!open) return;
    open = false;
    const HRESULT hr = list->Close();
    if (FAILED(hr)) {
        failed(2, "command list close", hr);
        return;
    }
    ID3D12CommandList* lists[1] = {list};
    queue->ExecuteCommandLists(1, lists);
    queue->Signal(fence, ++fenceValue);
    slots[slot].fence = fenceValue;
    slot = (slot + 1) % kSlots;
}
```

В `src/win/gpu/device.cpp` заменить функцию `void Renderer::Impl::waitIdle()` целиком:
```cpp
void Renderer::Impl::waitIdle() {
    submit();
    if (lost) return;  // a removed device's fence never moves on
    queue->Signal(fence, ++fenceValue);
    if (fence->GetCompletedValue() < fenceValue) {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
}
```

В `src/win/gpu/device.cpp` после функции `void Renderer::Impl::waitIdle()` добавить:
```cpp
// A failed call is logged once per place and code. A removed device (driver crash, TDR) stops all further GPU work
// with its reason in the log, instead of failing - and waiting - again every frame.
void Renderer::Impl::failed(uint32_t place, const char* what, HRESULT hr) {
    const HRESULT reason = device ? device->GetDeviceRemovedReason() : S_OK;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || FAILED(reason)) {
        if (!lost)
            LOG_ERROR("%s: the GPU device was removed (0x%08lx, reason 0x%08lx); the picture stops here, restart the game",
                      what, static_cast<unsigned long>(hr), static_cast<unsigned long>(reason));
        lost = true;
        return;
    }
    if (failures.first(place, uint32_t(hr))) LOG_ERROR("%s failed: 0x%08lx", what, static_cast<unsigned long>(hr));
}
```

В `src/win/gpu/device.cpp` заменить:
```cpp
    if (swapChain && hwnd == window && w == swapWidth && h == swapHeight) return true;
    waitIdle();
```
на:
```cpp
    if (swapChain && hwnd == window && w == swapWidth && h == swapHeight) return true;
    if (!swapChain && w == failedWidth && h == failedHeight) return false;  // tried: wait for another size
    waitIdle();
```

В `src/win/gpu/device.cpp` заменить:
```cpp
        if (FAILED(swapChain->ResizeBuffers(2, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, flags))) {
            LOG_ERROR("swap chain resize to %ux%u failed", w, h);
            releaseSwapChain();
            return false;
        }
```
на:
```cpp
        const HRESULT hr = swapChain->ResizeBuffers(2, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, flags);
        if (FAILED(hr)) {
            failed(3, "swap chain resize", hr);
            releaseSwapChain();
            failedWidth = w;
            failedHeight = h;
            return false;
        }
```

В `src/win/gpu/device.cpp` заменить:
```cpp
        IDXGISwapChain1* sc1 = nullptr;
        if (FAILED(factory->CreateSwapChainForHwnd(queue, hwnd, &d, nullptr, nullptr, &sc1)) ||
            FAILED(sc1->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain)))) {
            LOG_ERROR("cannot create a swap chain for the game window");
            releaseCom(sc1);
            return false;
        }
```
на:
```cpp
        IDXGISwapChain1* sc1 = nullptr;
        HRESULT hr = factory->CreateSwapChainForHwnd(queue, hwnd, &d, nullptr, nullptr, &sc1);
        if (SUCCEEDED(hr)) hr = sc1->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain));
        if (FAILED(hr)) {
            failed(4, "swap chain creation", hr);
            releaseCom(sc1);
            failedWidth = w;
            failedHeight = h;
            return false;
        }
```

В `src/win/gpu/device.cpp` заменить:
```cpp
    swapWidth = w;
    swapHeight = h;
    return true;
}
```
на:
```cpp
    swapWidth = w;
    swapHeight = h;
    failedWidth = failedHeight = 0;
    return true;
}
```

В `src/win/gpu/device.cpp` заменить:
```cpp
    if (!m.target || record.empty() || !m.begin()) return;
```
на:
```cpp
    if (m.lost || !m.target || record.empty() || !m.begin()) return;
```

В `src/win/gpu/device.cpp` заменить:
```cpp
    if (!m.target || !m.begin()) return false;
    const UINT pitch = UINT(alignUp(UINT64(m.width) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
```
на:
```cpp
    if (m.lost || !m.target || !m.begin()) return false;
    const UINT pitch = UINT(alignUp(UINT64(m.width) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
```

В `src/win/gpu/device.cpp` заменить:
```cpp
    if (FAILED(m.readback->Map(0, &all, reinterpret_cast<void**>(&data)))) return false;
```
на:
```cpp
    const HRESULT hr = m.readback->Map(0, &all, reinterpret_cast<void**>(&data));
    if (FAILED(hr)) {
        m.failed(6, "reading the frame back", hr);
        return false;
    }
```

В `src/win/gpu/device.cpp` заменить:
```cpp
    if (!m.target || !window || source.empty()) return;
```
на:
```cpp
    if (m.lost || !m.target || !window || source.empty()) return;
```

В `src/win/gpu/device.cpp` заменить:
```cpp
    const HRESULT hr = m.swapChain->Present(sync, flags);
    if (FAILED(hr)) LOG_ERROR("Present failed: 0x%08lx", static_cast<unsigned long>(hr));
}
```
на:
```cpp
    const HRESULT hr = m.swapChain->Present(sync, flags);
    if (FAILED(hr))
        m.failed(5, "Present", hr);
    else if (hr == DXGI_STATUS_OCCLUDED)
        Sleep(15);  // the window is covered: Present does not wait for vblank, keep the game's loop from spinning
}
```

- [ ] **Step 5: Собирается, тесты ядра зелёные**

Run: `cmake --build build-win -j 2>&1 | grep -E "warning|error" ; python3 tools/check_dll.py && cmake --build build-host -j && ./build-host/uagfx_tests | tail -1`
Expected: ни одной строки `warning`/`error`; `OK: build-win/ddraw.dll: 22 exports, …`; `… tests, 0 failed checks`.

- [ ] **Step 6: Commit**

```bash
git add src/core/log_once.hpp tests/test_log_once.cpp src/win/gpu/impl.hpp src/win/gpu/device.cpp
git commit -m "M3: сбои показа и видеодрайвера — одна строка в логе с причиной; после потери устройства GPU не трогаем

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 3: Настройки `high_resolution`, `ui_filter`, `render_scale`

**Files:**
- Modify: `src/core/config.hpp`, `src/core/config.cpp`, `dist/uaro-graphics.ini`, `src/win/real_ddraw.cpp`
- Test: `tests/test_config.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace uagfx {
  enum class UiFilter { Sharp, Smooth };
  struct Config { /* ... */ bool highResolution = true; UiFilter uiFilter = UiFilter::Sharp; int renderScale = 100; };
  const char* uiFilterToken(UiFilter f);
  }
  ```

- [ ] **Step 1: Падающие тесты**

В `tests/test_config.cpp` заменить:
```cpp
    CHECK(c.mode == DisplayMode::Auto);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(!c.allowTearing);
```
на:
```cpp
    CHECK(c.mode == DisplayMode::Auto);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.highResolution);
    CHECK(c.uiFilter == UiFilter::Sharp);
    CHECK_EQ(c.renderScale, 100);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(!c.allowTearing);
```

В `tests/test_config.cpp` заменить:
```cpp
        "[display]\nmode = window\nmonitor = 1\nwindow_width = 1600\nwindow_height = 900\n"
        "[frames]\nvsync = 0\nfps_limit = 144\nallow_tearing = 1\n"
```
на:
```cpp
        "[display]\nmode = window\nmonitor = 1\nwindow_width = 1600\nwindow_height = 900\n"
        "high_resolution = 0\nui_filter = smooth\n"
        "[quality]\nrender_scale = 150\n"
        "[frames]\nvsync = 0\nfps_limit = 144\nallow_tearing = 1\n"
```

В `tests/test_config.cpp` заменить:
```cpp
    CHECK_EQ(c.windowHeight, 900);
    CHECK(!c.vsync);
```
на:
```cpp
    CHECK_EQ(c.windowHeight, 900);
    CHECK(!c.highResolution);
    CHECK(c.uiFilter == UiFilter::Smooth);
    CHECK_EQ(c.renderScale, 150);
    CHECK(!c.vsync);
```

В `tests/test_config.cpp` заменить:
```cpp
        "[display]\nmode = fullscreen\nmonitor = -1\nwindow_width = 100\nwindow_height = abc\n"
```
на:
```cpp
        "[display]\nmode = fullscreen\nmonitor = -1\nwindow_width = 100\nwindow_height = abc\n"
        "high_resolution = maybe\nui_filter = blurry\n[quality]\nrender_scale = 300\n"
```

В `tests/test_config.cpp` заменить:
```cpp
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(c.logLevel == LogLevel::Info);
```
на:
```cpp
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.highResolution);
    CHECK(c.uiFilter == UiFilter::Sharp);
    CHECK_EQ(c.renderScale, 100);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(c.logLevel == LogLevel::Info);
```

В `tests/test_config.cpp` заменить:
```cpp
    CHECK(std::strcmp(displayModeToken(DisplayMode::Auto), "auto") == 0);
```
на:
```cpp
    CHECK(std::strcmp(displayModeToken(DisplayMode::Auto), "auto") == 0);
    CHECK(std::strcmp(uiFilterToken(UiFilter::Sharp), "sharp") == 0);
    CHECK(std::strcmp(uiFilterToken(UiFilter::Smooth), "smooth") == 0);
```

Run: `cmake --build build-host -j`
Expected: ошибка компиляции: `'struct uagfx::Config' has no member named 'highResolution'` (и `UiFilter` не объявлен).

- [ ] **Step 2: Реализация**

В `src/core/config.hpp` заменить:
```cpp
enum class DisplayMode { Auto, Window, Borderless };
```
на:
```cpp
enum class DisplayMode { Auto, Window, Borderless };
enum class UiFilter { Sharp, Smooth };  // [display] ui_filter: how magnified interface pictures are sampled
```

В `src/core/config.hpp` заменить:
```cpp
    int windowHeight = 0;
    bool vsync = true;
```
на:
```cpp
    int windowHeight = 0;
    bool highResolution = true;  // draw at the window's real pixels under Windows scaling (stage 3); false = 0.2
    UiFilter uiFilter = UiFilter::Sharp;
    int renderScale = 100;       // %, 100-200: the frame is drawn this much larger and shrunk when shown
    bool vsync = true;
```

В `src/core/config.hpp` заменить:
```cpp
const char* displayModeToken(DisplayMode m);
```
на:
```cpp
const char* displayModeToken(DisplayMode m);
const char* uiFilterToken(UiFilter f);
```

В `src/core/config.cpp` заменить:
```cpp
        } else if (key == "display.monitor") {
```
на:
```cpp
        } else if (key == "display.high_resolution") {
            setBool(val, c.highResolution);
        } else if (key == "display.ui_filter") {
            if (val == "sharp") c.uiFilter = UiFilter::Sharp;
            if (val == "smooth") c.uiFilter = UiFilter::Smooth;
        } else if (key == "quality.render_scale") {
            toInt(val, 100, 200, c.renderScale);
        } else if (key == "display.monitor") {
```

В `src/core/config.cpp` заменить:
```cpp
    return "auto";
}
```
на:
```cpp
    return "auto";
}

const char* uiFilterToken(UiFilter f) { return f == UiFilter::Smooth ? "smooth" : "sharp"; }
```

Run: `cmake --build build-host -j && ./build-host/uagfx_tests config`
Expected: 7 тестов `PASS config_…`, `7 tests, 0 failed checks`.

- [ ] **Step 3: Файл настроек и лог**

В `dist/uaro-graphics.ini` заменить:
```ini
; window     = always a normal window
mode = auto
```
на:
```ini
; window     = always a normal window
mode = auto
; 1 = draw the game at the real pixels of its window: with Windows scaling (e.g. 175 % on a 4K monitor)
;     the world is sharper and the window and the interface keep their size; 0 = as version 0.2
high_resolution = 1
; how enlarged interface pictures look: sharp = crisp pixels, smooth = soft
ui_filter = sharp
```

В `dist/uaro-graphics.ini` заменить:
```ini
[frames]
```
на:
```ini
[quality]
; the frame is drawn this many % of the window's pixels, 100-200, and shrunk when shown:
; above 100 edges get smoother, the graphics card does more work
render_scale = 100

[frames]
```

В `src/win/real_ddraw.cpp` заменить:
```cpp
    const WindowsVersion v = windowsVersion();
```
на:
```cpp
    LOG_INFO("display: high_resolution = %d, ui_filter = %s, render_scale = %d%%", s.config.highResolution ? 1 : 0,
             uiFilterToken(s.config.uiFilter), s.config.renderScale);
    const WindowsVersion v = windowsVersion();
```

Run: `cmake --build build-win -j 2>&1 | grep -E "warning|error" ; python3 tools/check_dll.py && grep -c $'\r$' dist/uaro-graphics.ini && wc -l < dist/uaro-graphics.ini`
Expected: ни одной строки `warning`/`error`; `OK: …`; два одинаковых числа (`42`) — все строки ini с CRLF.

- [ ] **Step 4: Commit**

```bash
git add src/core/config.hpp src/core/config.cpp tests/test_config.cpp dist/uaro-graphics.ini src/win/real_ddraw.cpp
git commit -m "Настройки этапа 3: high_resolution, ui_filter, render_scale (ini с пояснениями, строка в логе)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 4: Математика масштаба: план `s`/`k`/цель, перевод прямоугольников, фильтр `sharp`

Внутренний кадр — `round(L · k)` в целых числах, с тем же округлением, что у `fitPicture`: при `render_scale = 100`
он ровно картинка `P`, и показ идёт точка в точку (уточнение §3). С плавающей точкой `L · k = x,5` округлялось бы
иначе: игра 640×480 в окне 646×485 дала бы кадр 646×484 при картинке 646×485 — вывод растянут на строку, мягкий.

**Files:**
- Create: `src/core/scale.hpp`, `src/core/scale.cpp`
- Test: `tests/test_scale.cpp`

**Interfaces:**
- Consumes: `fitPicture`, `Rect` (1B).
- Produces:
  ```cpp
  namespace uagfx {
  struct ScalePlan { double s, r, k; int targetWidth, targetHeight; Rect picture; };
  constexpr int kMaxTarget = 16384;
  ScalePlan planScale(int gameWidth, int gameHeight, int outputWidth, int outputHeight, int renderScale,
                      bool highResolution);
  Rect scaleRect(const Rect& r, double k);
  inline double targetCoord(double x, double k);  // x * k + 0.5
  double sharpCoord(double texel, double magnification);
  }
  ```

- [ ] **Step 1: Падающие тесты**

`tests/test_scale.cpp`:
```cpp
#include <cmath>

#include "core/scale.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
}  // namespace

// The user's setup: a 1920x1080 window that Windows (175 %) shows as 3360x1890 real pixels.
TEST(scale_users_window_at_175) {
    const ScalePlan p = planScale(1920, 1080, 3360, 1890, 100, true);
    CHECK(near(p.s, 1.75));
    CHECK(near(p.k, 1.75));
    CHECK_EQ(p.targetWidth, 3360);
    CHECK_EQ(p.targetHeight, 1890);
    CHECK(p.picture == (Rect{0, 0, 3360, 1890}));  // r = 1: the target is shown pixel for pixel
}

TEST(scale_full_screen_on_4k) {
    const ScalePlan p = planScale(1920, 1080, 3840, 2160, 100, true);
    CHECK(near(p.k, 2.0));
    CHECK_EQ(p.targetWidth, 3840);
    CHECK_EQ(p.targetHeight, 2160);
}

// Windows at 100 % (or no SetThreadDpiAwarenessContext): the window's real pixels are the game's - as in 0.2.0.
TEST(scale_windows_at_100_percent_draws_at_the_game_size) {
    const ScalePlan p = planScale(1920, 1080, 1920, 1080, 100, true);
    CHECK(near(p.k, 1.0));
    CHECK_EQ(p.targetWidth, 1920);
    CHECK_EQ(p.targetHeight, 1080);
    CHECK(p.picture == (Rect{0, 0, 1920, 1080}));
}

TEST(scale_render_scale_supersamples) {
    const ScalePlan p = planScale(1920, 1080, 3360, 1890, 150, true);
    CHECK(near(p.r, 1.5));
    CHECK(near(p.k, 2.625));
    CHECK_EQ(p.targetWidth, 5040);
    CHECK_EQ(p.targetHeight, 2835);
    CHECK(p.picture == (Rect{0, 0, 3360, 1890}));  // shrunk into the window when shown
}

TEST(scale_off_draws_at_the_game_size) {
    const ScalePlan p = planScale(1920, 1080, 3360, 1890, 150, false);
    CHECK(near(p.k, 1.0));
    CHECK_EQ(p.targetWidth, 1920);
    CHECK_EQ(p.targetHeight, 1080);
}

TEST(scale_never_below_the_game_size) {
    const ScalePlan p = planScale(1920, 1080, 960, 540, 100, true);  // a window smaller than the game
    CHECK(near(p.s, 0.5));
    CHECK(near(p.k, 1.0));
    CHECK_EQ(p.targetWidth, 1920);
    CHECK(p.picture == (Rect{0, 0, 960, 540}));
}

TEST(scale_bars_for_another_shape) {
    const ScalePlan p = planScale(1024, 768, 1920, 1080, 100, true);
    CHECK(p.picture == (Rect{240, 0, 1680, 1080}));
    CHECK(near(p.s, 1.40625));
    CHECK_EQ(p.targetWidth, 1440);
    CHECK_EQ(p.targetHeight, 1080);
}

TEST(scale_target_within_d3d12_limits) {
    const ScalePlan p = planScale(4096, 2160, 16384, 8640, 200, true);  // s 4 and r 2 would make k 8
    CHECK(near(p.k, 4.0));
    CHECK_EQ(p.targetWidth, kMaxTarget);
}

// At 100 % the target is exactly the picture, so it is shown pixel for pixel (spec §3) - also where size * k is a
// half and floating point would round it the other way than fitPicture: 640x480 in 646x485 is 484.5 rows.
TEST(scale_target_is_the_picture_at_100_percent) {
    const ScalePlan p = planScale(640, 480, 646, 485, 100, true);
    CHECK_EQ(p.targetHeight, 485);
    CHECK(p.picture == (Rect{0, 0, 646, 485}));
    const int games[][2] = {{640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}};
    int differ = 0;
    for (const auto& g : games)
        for (int w = g[0] + 1; w <= g[0] * 5 / 2; ++w)
            for (int h = g[1] * w / g[0] - 9; h <= g[1] * w / g[0] + 9; ++h) {
                const ScalePlan q = planScale(g[0], g[1], w, h, 100, true);
                if (q.k > 1.0 && (q.targetWidth != q.picture.width() || q.targetHeight != q.picture.height()))
                    ++differ;
            }
    CHECK_EQ(differ, 0);
}

// At any render scale the target is the game's size times k, rounded: the full-screen quad, whose far edges are at
// size * k + 0.5, covers its last row and column (spec §4). round(picture * r) can miss them by a pixel at r > 1.
TEST(scale_target_is_the_game_times_k_rounded) {
    int off = 0;
    for (int percent = 100; percent <= 200; percent += 25)
        for (int w = 1921; w <= 4800; ++w) {
            const ScalePlan q = planScale(1920, 1080, w, 1080 * w / 1920 + 3, percent, true);
            if (std::fabs(q.targetWidth - 1920 * q.k) > 0.5 + 1e-9 ||
                std::fabs(q.targetHeight - 1080 * q.k) > 0.5 + 1e-9)
                ++off;
        }
    CHECK_EQ(off, 0);
}

TEST(scale_rect_edges_follow_the_vertices) {
    CHECK(scaleRect(Rect{0, 0, 1920, 1080}, 1.75) == (Rect{0, 0, 3360, 1890}));
    CHECK(scaleRect(Rect{1, 1, 2, 2}, 1.75) == (Rect{2, 2, 4, 4}));
    CHECK(scaleRect(Rect{3, 3, 4, 4}, 2.0) == (Rect{6, 6, 8, 8}));
    CHECK(scaleRect(Rect{5, 7, 9, 11}, 1.0) == (Rect{5, 7, 9, 11}));
    CHECK(scaleRect(Rect{0, 0, 1920, 1080}, 3840.0 / 1920.0) == (Rect{0, 0, 3840, 2160}));
}

TEST(scale_target_coord) {
    CHECK(near(targetCoord(0.0, 1.75), 0.5));        // the left edge of the screen: first pixel centre
    CHECK(near(targetCoord(1920.0, 1.75), 3360.5));  // the right edge: just past the last one
    CHECK(near(targetCoord(7.0, 1.0), 7.5));         // k = 1: calibration.md §3
}

TEST(sharp_coord_keeps_texels_flat_and_steps_in_one_pixel) {
    CHECK(near(sharpCoord(2.5, 1.75), 2.5));  // a texel centre stays
    CHECK(near(sharpCoord(2.7, 1.75), 2.5));  // inside the texel: snapped to it
    CHECK(near(sharpCoord(3.0, 1.75), 3.0));  // the middle of the step between two texels
    const double half = 0.5 / 1.75;           // the step is one screen pixel = 1 / 1.75 texel wide
    CHECK(near(sharpCoord(3.0 - half, 1.75), 2.5));
    CHECK(near(sharpCoord(3.0 + half, 1.75), 3.5));
    CHECK(near(sharpCoord(2.8, 1.0), 2.8));  // not magnified: unchanged
    CHECK(near(sharpCoord(2.8, 0.5), 2.8));
}
```

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/scale.hpp: No such file or directory`.

- [ ] **Step 2: Реализация**

`src/core/scale.hpp`:
```cpp
#pragma once
#include "core/image.hpp"

namespace uagfx {

// Stage 3 (docs/superpowers/specs/2026-10-13-stage3-resolution-design.md): the game keeps drawing at its own size L;
// the driver draws k times larger and shows the result in the window's real pixels.
struct ScalePlan {
    double s = 1.0;       // output / game, along the axis that limits the picture
    double r = 1.0;       // render scale (supersampling)
    double k = 1.0;       // game pixels -> target pixels
    int targetWidth = 0;  // the target and its depth buffer: the game's size times k, rounded
    int targetHeight = 0;
    Rect picture;         // where the whole frame goes in the output
};

constexpr int kMaxTarget = 16384;  // the largest Direct3D 12 texture side

// game: the client's resolution; output: the window's client area (real pixels when highResolution is on);
// renderScale: percent, 100-200. Never below the game's size: a smaller window shrinks a full-size frame. With
// highResolution off the frame is drawn at the game's size (k = 1), as in version 0.2. The target is size * k rounded
// in whole numbers, as fitPicture rounds: at 100 % it is exactly the picture, shown pixel for pixel.
ScalePlan planScale(int gameWidth, int gameHeight, int outputWidth, int outputHeight, int renderScale,
                    bool highResolution);

// Game pixels [x0, x1) in the target: [ceil(x0 k), ceil(x1 k)) - the pixels a quad with those edges covers once
// its vertices sit at x k + 0.5 (D3D12 pixel centres are on half-integers).
Rect scaleRect(const Rect& r, double k);

// Where a vertex at game coordinate x lands in the target. Not (x + 0.5) k: that shifts the picture by half a game
// pixel and leaves the first column and row uncovered (stage 3 spec §4). At k = 1 both are calibration.md §3.
inline double targetCoord(double x, double k) { return x * k + 0.5; }

// "Sharp bilinear" for interface pictures: a texel coordinate (uv * size) remapped so every texel is a flat square
// with a linear step one screen pixel wide between neighbours. magnification: screen pixels per texel; at 1 or less
// the coordinate is unchanged. The pixel shader's sharpUv() does the same.
double sharpCoord(double texel, double magnification);

}  // namespace uagfx
```

`src/core/scale.cpp`:
```cpp
#include "core/scale.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/window_plan.hpp"

namespace uagfx {

ScalePlan planScale(int gameWidth, int gameHeight, int outputWidth, int outputHeight, int renderScale,
                    bool highResolution) {
    ScalePlan p;
    p.picture = fitPicture(gameWidth, gameHeight, outputWidth, outputHeight);
    p.targetWidth = gameWidth;
    p.targetHeight = gameHeight;
    if (!highResolution || p.picture.empty()) return p;
    const bool byWidth = p.picture.width() == outputWidth;  // the axis that limits the picture
    const int64_t game = byWidth ? gameWidth : gameHeight;
    const int64_t out = byWidth ? outputWidth : outputHeight;
    const int percent = std::clamp(renderScale, 100, 200);
    p.s = double(out) / double(game);
    p.r = percent / 100.0;
    p.k = p.s * p.r;
    if (p.k <= 1.0) {  // never below the game's size
        p.k = 1.0;
        return p;
    }
    const double most = double(kMaxTarget) / std::max(gameWidth, gameHeight);
    if (p.k > most) {
        p.k = most;
        p.targetWidth = int(std::lround(gameWidth * p.k));
        p.targetHeight = int(std::lround(gameHeight * p.k));
        return p;
    }
    // size * out / game * percent / 100, rounded half up in whole numbers - as fitPicture rounds the picture.
    const auto times = [&](int64_t size) { return int((size * out * percent + game * 50) / (game * 100)); };
    p.targetWidth = times(gameWidth);
    p.targetHeight = times(gameHeight);
    return p;
}

Rect scaleRect(const Rect& r, double k) {
    const auto edge = [k](int v) { return int(std::ceil(v * k - 1e-6)); };  // 1e-6: k = O / L is not exact
    return Rect{edge(r.left), edge(r.top), edge(r.right), edge(r.bottom)};
}

double sharpCoord(double texel, double magnification) {
    const double base = std::floor(texel - 0.5) + 0.5;  // the centre of the left texel of the bilinear pair
    const double f = texel - base;
    return base + std::clamp((f - 0.5) * std::max(magnification, 1.0) + 0.5, 0.0, 1.0);
}

}  // namespace uagfx
```

- [ ] **Step 3: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests scale_ && ./build-host/uagfx_tests sharp_ && ./build-host/uagfx_tests | tail -1`
Expected: 12 тестов `PASS scale_…`, `PASS sharp_coord_keeps_texels_flat_and_steps_in_one_pixel`; полный прогон — `0 failed checks`.

- [ ] **Step 4: Commit**

```bash
git add src/core/scale.hpp src/core/scale.cpp tests/test_scale.cpp
git commit -m "Математика этапа 3: план масштаба (s, r, k, цель), перевод прямоугольников, фильтр sharp

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 5: Эталонный растеризатор с переводом драйвера: края без полос при `k ≠ 1`

**Files:**
- Modify: `src/core/raster.hpp`, `src/core/raster.cpp`
- Test: `tests/test_raster.cpp`

**Interfaces:**
- Consumes: `targetCoord` (Task 4).
- Produces: `enum class PixelCenters { D3D7, D3D10, Ours }`; `RefRaster(int width, int height, PixelCenters centers,
  double scale = 1.0)`.

- [ ] **Step 1: Падающие тесты**

В `tests/test_raster.cpp` заменить:
```cpp
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
}  // namespace
```
на:
```cpp
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
// A solid white rectangle of game pixels [x0, x1) x [y0, y1) drawn into r on a grey background.
void rect(RefRaster& r, float x0, float y0, float x1, float y1) {
    const RefTexture white{1, 1, {0xFFFFFFFF}};
    const TlVertex v[4] = {tl(x0, y0, 0, 0), tl(x0, y1, 0, 1), tl(x1, y0, 1, 0), tl(x1, y1, 1, 1)};
    r.clear(Rgb{0.25, 0.25, 0.25});
    r.strip(v, 4, white, false, d3d7::TADDRESS_CLAMP);
}
int covered(const RefRaster& r) {
    int n = 0;
    for (int y = 0; y < r.height(); ++y)
        for (int x = 0; x < r.width(); ++x)
            if (!near(r.at(x, y).r, 0.25)) ++n;
    return n;
}
}  // namespace
```

В `tests/test_raster.cpp` добавить в конец:
```cpp

TEST(raster_ours_at_scale_1_matches_d3d7) {
    RefRaster a(8, 8, PixelCenters::D3D7);
    RefRaster b(8, 8, PixelCenters::Ours, 1.0);
    quad(a, 2, 2, 0.0f, 0.0f, 1.0f, 1.0f, d3d7::TADDRESS_CLAMP);
    quad(b, 2, 2, 0.0f, 0.0f, 1.0f, 1.0f, d3d7::TADDRESS_CLAMP);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) CHECK(near(a.at(x, y).g, b.at(x, y).g));
}

TEST(raster_full_screen_quad_at_1_75_covers_every_pixel) {
    // An 8x8 game drawn 1.75 times larger: 14x14. The client's full-screen quad has its vertices on the screen's
    // edges (0 and 8); every target pixel must be covered ((x + 0.5) k would leave column 0 and row 0 empty).
    RefRaster r(14, 14, PixelCenters::Ours, 1.75);
    rect(r, 0, 0, 8, 8);
    CHECK_EQ(covered(r), 14 * 14);
}

TEST(raster_one_game_pixel_becomes_k_by_k_pixels) {
    RefRaster r(16, 16, PixelCenters::Ours, 2.0);
    rect(r, 3, 3, 4, 4);
    CHECK_EQ(covered(r), 4);
    CHECK(!near(r.at(6, 6).r, 0.25));
    CHECK(!near(r.at(7, 7).r, 0.25));
    CHECK(near(r.at(8, 8).r, 0.25));
}
```

Run: `cmake --build build-host -j`
Expected: ошибка компиляции: `'Ours' is not a member of 'uagfx::PixelCenters'`.

- [ ] **Step 2: Реализация**

В `src/core/raster.hpp` заменить:
```cpp
enum class PixelCenters { D3D7, D3D10 };  // D3D10: what an uncorrected translation does (negative tests)
```
на:
```cpp
// D3D10: what an uncorrected translation does (negative tests). Ours: what the driver does - a vertex at x goes to
// x * scale + 0.5 in a target `scale` times the game's size whose pixel centres are on half-integers (stage 3).
enum class PixelCenters { D3D7, D3D10, Ours };
```

В `src/core/raster.hpp` заменить:
```cpp
    RefRaster(int width, int height, PixelCenters centers);
```
на:
```cpp
    RefRaster(int width, int height, PixelCenters centers, double scale = 1.0);
```

В `src/core/raster.hpp` заменить:
```cpp
    PixelCenters centers_;
    std::vector<Rgb> px_;
```
на:
```cpp
    PixelCenters centers_;
    double scale_;
    std::vector<Rgb> px_;
```

В `src/core/raster.cpp` заменить:
```cpp
#include "core/d3d7.hpp"
```
на:
```cpp
#include "core/d3d7.hpp"
#include "core/scale.hpp"
```

В `src/core/raster.cpp` заменить:
```cpp
RefRaster::RefRaster(int width, int height, PixelCenters centers)
    : w_(width), h_(height), centers_(centers), px_(size_t(width) * size_t(height), Rgb{0, 0, 0}) {}
```
на:
```cpp
RefRaster::RefRaster(int width, int height, PixelCenters centers, double scale)
    : w_(width), h_(height), centers_(centers), scale_(scale), px_(size_t(width) * size_t(height), Rgb{0, 0, 0}) {}
```

В `src/core/raster.cpp` заменить:
```cpp
    const TlVertex* p0 = &a;
    const TlVertex* p1 = &b;
    const TlVertex* p2 = &c;
```
на:
```cpp
    TlVertex t[3] = {a, b, c};
    if (centers_ == PixelCenters::Ours)
        for (TlVertex& v : t) {
            v.x = float(targetCoord(v.x, scale_));
            v.y = float(targetCoord(v.y, scale_));
        }
    const TlVertex* p0 = &t[0];
    const TlVertex* p1 = &t[1];
    const TlVertex* p2 = &t[2];
```

- [ ] **Step 3: Тесты проходят (и тест ловит старую формулу)**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests raster && ./build-host/uagfx_tests calibration | tail -1`
Expected: 7 тестов `PASS raster_…`; калибровка по-прежнему `7 tests, 0 failed checks`.

Проверка самого теста (не коммитится): в `src/core/scale.hpp` временно заменить `return x * k + 0.5;` на
`return (x + 0.5) * k;`, запустить `cmake --build build-host -j && ./build-host/uagfx_tests raster_full` —
Expected: `FAIL raster_full_screen_quad_at_1_75_covers_every_pixel`; вернуть `return x * k + 0.5;` (`git diff
src/core/scale.hpp` — пусто).

- [ ] **Step 4: Commit**

```bash
git add src/core/raster.hpp src/core/raster.cpp tests/test_raster.cpp
git commit -m "Эталонный растеризатор с переводом драйвера x·k + 0.5: полноэкранный квад покрывает цель без полос

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 6: Признак интерфейса: всё после `ZENABLE = 0` в кадре

**Files:**
- Create: `src/core/scene.hpp`, `src/core/scene.cpp`
- Modify: `src/core/states.hpp`, `src/core/states.cpp`, `src/win/dx7/device7.hpp`, `src/win/dx7/device7.cpp`
- Test: `tests/test_scene.cpp`

**Interfaces:**
- Consumes: `d3d7::RS_ZENABLE` (1B Task 1), `DrawSetup` (1B).
- Produces:
  ```cpp
  namespace uagfx {
  class SceneTracker { public: void beginScene(); void renderState(uint32_t state, uint32_t value); bool ui() const; };
  struct DrawSetup { /* ... */ bool ui = false; };  // compared by operator==
  }
  ```

- [ ] **Step 1: Падающие тесты**

`tests/test_scene.cpp`:
```cpp
#include "core/d3d7.hpp"
#include "core/scene.hpp"
#include "core/states.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::d3d7;

TEST(scene_interface_starts_at_zenable_off) {
    SceneTracker t;
    t.beginScene();
    CHECK(!t.ui());
    t.renderState(RS_ZWRITEENABLE, 0);  // world lists G and I turn off Z writes only
    CHECK(!t.ui());
    t.renderState(RS_ZENABLE, 0);  // L11851: the interface lists U1 and U2
    CHECK(t.ui());
    t.renderState(RS_ZENABLE, 1);
    CHECK(t.ui());  // until the next frame
    t.beginScene();
    CHECK(!t.ui());
}

TEST(scene_draw_setups_differ_by_the_interface_flag) {
    DrawSetup a;
    DrawSetup b;
    b.ui = true;
    CHECK(!(a == b));
}
```

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/scene.hpp: No such file or directory`.

- [ ] **Step 2: Реализация (ядро)**

`src/core/scene.hpp`:
```cpp
#pragma once
#include <cstdint>

namespace uagfx {

// The client draws its interface last in every frame, after SetRenderState(ZENABLE, 0) (spec §2, L11851): from
// that call until the next BeginScene every draw is interface (stage 3: sharp filtering of magnified pictures).
class SceneTracker {
public:
    void beginScene() { ui_ = false; }
    void renderState(uint32_t state, uint32_t value);
    bool ui() const { return ui_; }

private:
    bool ui_ = false;
};

}  // namespace uagfx
```

`src/core/scene.cpp`:
```cpp
#include "core/scene.hpp"

#include "core/d3d7.hpp"

namespace uagfx {

void SceneTracker::renderState(uint32_t state, uint32_t value) {
    if (state == d3d7::RS_ZENABLE && value == 0) ui_ = true;
}

}  // namespace uagfx
```

В `src/core/states.hpp` заменить:
```cpp
    bool colorKey = false;
    StageSetup stage[2];
    bool operator==(const DrawSetup& o) const;
```
на:
```cpp
    bool colorKey = false;
    StageSetup stage[2];
    bool ui = false;  // an interface draw (SceneTracker): magnified pictures get the [display] ui_filter
    bool operator==(const DrawSetup& o) const;
```

В `src/core/states.cpp` заменить:
```cpp
           stage[0] == o.stage[0] && stage[1] == o.stage[1];
```
на:
```cpp
           stage[0] == o.stage[0] && stage[1] == o.stage[1] && ui == o.ui;
```

Run: `cmake --build build-host -j && ./build-host/uagfx_tests scene && ./build-host/uagfx_tests | tail -1`
Expected: 2 теста `PASS scene_…`; полный прогон — `0 failed checks`.

- [ ] **Step 3: Устройство отмечает рисования интерфейса**

В `src/win/dx7/device7.hpp` заменить:
```cpp
#include "core/d3d7.hpp"
#include "core/states.hpp"
```
на:
```cpp
#include "core/d3d7.hpp"
#include "core/scene.hpp"
#include "core/states.hpp"
```

В `src/win/dx7/device7.hpp` заменить:
```cpp
    DeviceStates states_;
    D3DVIEWPORT7 viewport_{};
```
на:
```cpp
    DeviceStates states_;
    SceneTracker scene_;  // which draws are interface (stage 3)
    D3DVIEWPORT7 viewport_{};
```

В `src/win/dx7/device7.cpp` заменить:
```cpp
HRESULT STDMETHODCALLTYPE Device7::BeginScene() { return D3D_OK; }
```
на:
```cpp
HRESULT STDMETHODCALLTYPE Device7::BeginScene() {
    win::Guard guard;
    scene_.beginScene();
    return D3D_OK;
}
```

В `src/win/dx7/device7.cpp` заменить:
```cpp
    states_.setRender(uint32_t(state), value);
    static bool keyLogged = false;
```
на:
```cpp
    states_.setRender(uint32_t(state), value);
    scene_.renderState(uint32_t(state), value);
    static bool keyLogged = false;
```

В `src/win/dx7/device7.cpp` заменить:
```cpp
    cmd.state = c.record.addState(resolveDrawSetup(states_, false, hasTexture));  // the target has no alpha
```
на:
```cpp
    DrawSetup setup = resolveDrawSetup(states_, false, hasTexture);  // the target has no alpha
    setup.ui = scene_.ui();
    cmd.state = c.record.addState(setup);
```

Run: `cmake --build build-win -j 2>&1 | grep -E "warning|error" ; python3 tools/check_dll.py`
Expected: ни одной строки `warning`/`error`; `OK: …`.

- [ ] **Step 4: Commit**

```bash
git add src/core/scene.hpp src/core/scene.cpp tests/test_scene.cpp src/core/states.hpp src/core/states.cpp src/win/dx7/device7.hpp src/win/dx7/device7.cpp
git commit -m "Признак интерфейса: рисования после ZENABLE = 0 в кадре отмечаются в состоянии рисования

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 7: Шейдеры и константы: вершины `x · k + 0.5`, фильтр `sharp` для интерфейса, усреднение при уменьшении

Пока `k = 1` (переход на масштаб — Task 8), картинка не меняется: вершины те же, `sharp` при увеличении 1 оставляет
UV как есть, усреднение при уменьшении не включается (`footprint` = 0).

**Files:**
- Modify: `src/win/gpu/impl.hpp`, `src/win/gpu/shaders.cpp`, `src/win/gpu/draw.cpp`, `src/win/gpu/renderer.hpp`,
  `src/win/gpu/device.cpp`, `src/win/context.cpp`

**Interfaces:**
- Consumes: `DrawSetup::ui` (Task 6), `Config::uiFilter` (Task 3).
- Produces: `Constants { …, uint32_t uiSharp; float scale; float footprint; float sourceRect[4]; }` (20 DWORD, как
  раньше); `Renderer::Impl::scale` (`double`, k), `Renderer::Impl::uiSharp`; `void Renderer::setUiSharp(bool sharp)`.

- [ ] **Step 1: Константы**

В `src/win/gpu/impl.hpp` заменить:
```cpp
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
```
на:
```cpp
// Root constants; must match cbuffer Params in shaders.cpp.
struct Constants {
    float targetSize[2];   // 0-1: the target's size in pixels
    uint32_t stage0[2];    // 2-3: colorOp | colorArg1 << 8 | colorArg2 << 16 | alphaOp << 24, alphaArg1 | alphaArg2 << 8 | texCoord << 16 | keyed << 24
    uint32_t stage1[2];    // 4-5
    uint32_t alphaTest;    // 6: func | ref << 8 | enabled << 16
    uint32_t fogEnable;    // 7
    float fogColor[4];     // 8-11
    uint32_t colorKey;     // 12
    uint32_t uiSharp;      // 13: an interface draw with [display] ui_filter = sharp
    float scale;           // 14: k, game pixels -> target pixels (stage 3)
    float footprint;       // 15: blit only - source texels per destination pixel
    float sourceRect[4];   // 16-19: blit only, normalised u0 v0 u1 v1
};
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    int width = 0;
    int height = 0;
    D3D12_VIEWPORT viewport{};
```
на:
```cpp
    int width = 0;
    int height = 0;
    double scale = 1.0;  // k: game pixels -> target pixels (stage 3)
    bool uiSharp = true;  // [display] ui_filter = sharp
    D3D12_VIEWPORT viewport{};
```

- [ ] **Step 2: Шейдеры**

В `src/win/gpu/shaders.cpp` заменить:
```cpp
    uint colorKey;
    uint3 pad;
    float4 sourceRect;
```
на:
```cpp
    uint colorKey;
    uint uiSharp;
    float scale;
    float footprint;
    float4 sourceRect;
```

В `src/win/gpu/shaders.cpp` заменить:
```cpp
// D3D7 puts pixel centres on integer coordinates, D3D12 on half-integers: x + 0.5 (calibration.md §3).
PsIn draw_vs(VsIn i) {
    PsIn o;
    float w = i.pos.w > 0.0 ? 1.0 / i.pos.w : 1.0;
    float x = (i.pos.x + 0.5) * 2.0 / targetSize.x - 1.0;
    float y = 1.0 - (i.pos.y + 0.5) * 2.0 / targetSize.y;
```
на:
```cpp
// D3D7 puts pixel centres on integer coordinates, D3D12 on half-integers, and the target is `scale` times the
// game's size: a game x lands at x * scale + 0.5 (stage 3 spec §4; at scale 1 that is calibration.md §3).
PsIn draw_vs(VsIn i) {
    PsIn o;
    float w = i.pos.w > 0.0 ? 1.0 / i.pos.w : 1.0;
    float x = (i.pos.x * scale + 0.5) * 2.0 / targetSize.x - 1.0;
    float y = 1.0 - (i.pos.y * scale + 0.5) * 2.0 / targetSize.y;
```

В `src/win/gpu/shaders.cpp` заменить:
```cpp
float4 draw_ps(PsIn i) : SV_Target {
    float4 t0 = tex0.Sample(samp0, ((stage0.y >> 16) & 7) == 1 ? i.uv1 : i.uv0);
```
на:
```cpp
// "Sharp bilinear" (stage 3 spec §6; core sharpCoord does the same): every texel a flat square, a one-pixel
// linear step between neighbours; unchanged where the picture is not magnified.
float2 sharpUv(Texture2D t, float2 uv) {
    float2 size;
    t.GetDimensions(size.x, size.y);
    float2 texel = uv * size;
    float2 magnification = max(1.0 / max(fwidth(texel), 1e-6), 1.0);
    float2 base = floor(texel - 0.5) + 0.5;
    return (base + saturate((texel - base - 0.5) * magnification + 0.5)) / size;
}

float4 draw_ps(PsIn i) : SV_Target {
    float2 uv0 = ((stage0.y >> 16) & 7) == 1 ? i.uv1 : i.uv0;
    if (uiSharp != 0) uv0 = sharpUv(tex0, uv0);
    float4 t0 = tex0.Sample(samp0, uv0);
```

В `src/win/gpu/shaders.cpp` заменить:
```cpp
float4 present_ps(PresentOut i) : SV_Target {
    return float4(tex0.Sample(samp0, i.uv).rgb, 1);
}
```
на:
```cpp
// Copies part of a texture: one bilinear sample when it is not shrunk, else the average of n x n samples over the
// destination pixel's footprint (supersampling; the target read back at the game's size).
float4 present_ps(PresentOut i) : SV_Target {
    if (footprint <= 1.0001) return float4(tex0.Sample(samp0, i.uv).rgb, 1);
    int n = min((int)ceil(footprint), 4);
    float2 dx = ddx(i.uv);
    float2 dy = ddy(i.uv);
    float3 sum = 0;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            float2 o = (float2(x, y) + 0.5) / n - 0.5;
            sum += tex0.SampleGrad(samp0, i.uv + o.x * dx + o.y * dy, dx, dy).rgb;
        }
    return float4(sum / (n * n), 1);
}
```

- [ ] **Step 3: Константы рисования и выбор фильтра**

В `src/win/gpu/draw.cpp` заменить:
```cpp
    c.colorKey = s.colorKey ? 1u : 0u;
```
на:
```cpp
    c.colorKey = s.colorKey ? 1u : 0u;
    c.uiSharp = s.ui && uiSharp ? 1u : 0u;
    c.scale = float(scale);
```

В `src/win/gpu/renderer.hpp` заменить:
```cpp
    std::string adapterName() const;
```
на:
```cpp
    std::string adapterName() const;
    void setUiSharp(bool sharp);  // [display] ui_filter for interface draws
```

В `src/win/gpu/device.cpp` заменить:
```cpp
std::string Renderer::adapterName() const { return impl_->adapter; }
int Renderer::targetWidth() const { return impl_->width; }
int Renderer::targetHeight() const { return impl_->height; }
```
на:
```cpp
std::string Renderer::adapterName() const { return impl_->adapter; }
int Renderer::targetWidth() const { return impl_->width; }
int Renderer::targetHeight() const { return impl_->height; }

void Renderer::setUiSharp(bool sharp) { impl_->uiSharp = sharp; }
```

В `src/win/context.cpp` заменить:
```cpp
    renderer = r;
    LOG_INFO("Direct3D 12 renderer on %s", renderer->adapterName().c_str());
```
на:
```cpp
    renderer = r;
    renderer->setUiSharp(config.uiFilter == UiFilter::Sharp);
    LOG_INFO("Direct3D 12 renderer on %s", renderer->adapterName().c_str());
```

- [ ] **Step 4: Собирается**

Run: `cmake --build build-win -j 2>&1 | grep -E "warning|error" ; python3 tools/check_dll.py && cmake --build build-host -j && ./build-host/uagfx_tests | tail -1`
Expected: ни одной строки `warning`/`error`; `OK: …`; `… tests, 0 failed checks`. HLSL здесь не компилируется:
сверить блоки шейдера с SM 5.0 глазами (`GetDimensions(float, float)`, `fwidth`, `SampleGrad`, цикл с `int`).

- [ ] **Step 5: Commit**

```bash
git add src/win/gpu/impl.hpp src/win/gpu/shaders.cpp src/win/gpu/draw.cpp src/win/gpu/renderer.hpp src/win/gpu/device.cpp src/win/context.cpp
git commit -m "Шейдеры этапа 3: вершины x·k + 0.5, фильтр sharp для интерфейса, усреднение при уменьшении кадра

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 8: Рендер в масштабе: цель `×k`, копирование `blit`, показ в настоящих точках окна

После этой задачи игра рисуется в масштабе. Чтение и запись кадра клиентом в масштабе — Task 9 (до неё скриншоты при
`k ≠ 1` неверны; выпуск — только после Task 10).

**Files:**
- Create: `src/win/dpi.hpp`, `src/win/dpi.cpp`
- Modify: `src/win/gpu/impl.hpp`, `src/win/gpu/renderer.hpp`, `src/win/gpu/device.cpp`, `src/win/context.hpp`,
  `src/win/context.cpp`

**Interfaces:**
- Consumes: `planScale`, `ScalePlan`, `scaleRect` (Task 4), `fitPicture` (1B), `Constants` (Task 7), `failed` (Task 2).
- Produces:
  ```cpp
  namespace uagfx::gpu {
  // Renderer: bool setScale(double k, int targetWidth, int targetHeight);
  //           void present(HWND window, int outputWidth, int outputHeight, const Rect& source, const PresentOptions&);
  //           (targetWidth()/targetHeight() removed: nobody uses them)
  // Impl: kRtvTarget 0, kRtvSwap 1-2, kRtvGame 3, kRtvNew 4; int gameWidth, gameHeight; ViewportCmd gameViewport;
  //       bool createTarget(int, int, ID3D12Resource*&, ID3D12Resource*&); void applyViewport();
  //       void blit(ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState, UINT srcSrv, int srcW, int srcH,
  //                 const float srcRect[4], D3D12_CPU_DESCRIPTOR_HANDLE dst, const Rect& dstRect, int dstW, int dstH);
  //       bool ensureSwapChain(HWND hwnd, UINT w, UINT h, bool wantTearing);
  }
  namespace uagfx::win { SIZE clientPixels(HWND hwnd, bool physical); }
  ```

- [ ] **Step 1: Настоящий размер окна**

`src/win/dpi.hpp`:
```cpp
#pragma once
#include <windows.h>

namespace uagfx::win {

// The window's client area in the screen's real pixels (physical), or the size Windows tells this game. uaRO.exe is
// not DPI-aware: under Windows scaling it is told a smaller size and Windows stretches its window, unless the frame
// is shown in real pixels (stage 3 spec §2: measured pixel for pixel on the game's own window).
SIZE clientPixels(HWND hwnd, bool physical);

}  // namespace uagfx::win
```

`src/win/dpi.cpp`:
```cpp
#include "win/dpi.hpp"

#include <algorithm>

namespace uagfx::win {
namespace {

using SetContextFn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);

// Windows 10 1607 and newer; without it the game's size is all there is.
SetContextFn setContext() {
    static const SetContextFn fn = reinterpret_cast<SetContextFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("user32.dll"), "SetThreadDpiAwarenessContext")));
    return fn;
}

}  // namespace

SIZE clientPixels(HWND hwnd, bool physical) {
    const SetContextFn set = physical ? setContext() : nullptr;
    const DPI_AWARENESS_CONTEXT old = set ? set(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) : nullptr;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (old) set(old);
    return SIZE{std::max<LONG>(1, rc.right - rc.left), std::max<LONG>(1, rc.bottom - rc.top)};
}

}  // namespace uagfx::win
```

- [ ] **Step 2: Рендер — объявления**

В `src/win/gpu/impl.hpp` заменить:
```cpp
constexpr UINT kRootConstants = 0;
```
на:
```cpp
// Render target views: the target, the two swap chain buffers, the game-sized helper, a new target being filled.
constexpr UINT kRtvTarget = 0;
constexpr UINT kRtvSwap = 1;
constexpr UINT kRtvGame = 3;
constexpr UINT kRtvNew = 4;
constexpr UINT kRtvCount = 5;

constexpr UINT kRootConstants = 0;
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    ID3D12DescriptorHeap* rtvHeap = nullptr;  // 0: target, 1-2: swap chain buffers
```
на:
```cpp
    ID3D12DescriptorHeap* rtvHeap = nullptr;  // kRtv*
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    int width = 0;
    int height = 0;
    double scale = 1.0;  // k: game pixels -> target pixels (stage 3)
    bool uiSharp = true;  // [display] ui_filter = sharp
```
на:
```cpp
    int width = 0;  // the target: the game's size times scale
    int height = 0;
    int gameWidth = 0;  // the client's size
    int gameHeight = 0;
    double scale = 1.0;  // k: game pixels -> target pixels (stage 3)
    bool uiSharp = true;  // [display] ui_filter = sharp
    ViewportCmd gameViewport;  // the client's viewport, in game pixels
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    void failed(uint32_t place, const char* what, HRESULT hr);
```
на:
```cpp
    void failed(uint32_t place, const char* what, HRESULT hr);
    bool createTarget(int w, int h, ID3D12Resource*& rt, ID3D12Resource*& ds);
    void applyViewport();
    void blit(ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState, UINT srcSrv, int srcW, int srcH,
              const float srcRect[4], D3D12_CPU_DESCRIPTOR_HANDLE dst, const Rect& dstRect, int dstW, int dstH);
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    bool ensureSwapChain(HWND hwnd, bool wantTearing);
```
на:
```cpp
    bool ensureSwapChain(HWND hwnd, UINT w, UINT h, bool wantTearing);
```

В `src/win/gpu/renderer.hpp` заменить:
```cpp
    bool setTarget(int width, int height);  // the client's target (BGRA8) and depth (D32), cleared to black
    int targetWidth() const;
    int targetHeight() const;
```
на:
```cpp
    bool setTarget(int width, int height);  // the client's size: target (BGRA8) and depth (D32) at k = 1, black
    // Stage 3: from now on draw into a target k times the game's size (targetWidth x targetHeight); the current
    // picture is stretched into it, as the back buffer keeps its contents across a present.
    bool setScale(double k, int targetWidth, int targetHeight);
```

В `src/win/gpu/renderer.hpp` заменить:
```cpp
    void present(HWND window, const Rect& source, const PresentOptions& options);
```
на:
```cpp
    // source: in game pixels; shown fitted into a swap chain of outputWidth x outputHeight (real pixels, stage 3).
    void present(HWND window, int outputWidth, int outputHeight, const Rect& source, const PresentOptions& options);
```

- [ ] **Step 3: Рендер — масштаб в командах, копирование, цель**

В `src/win/gpu/device.cpp` заменить:
```cpp
#include "core/window_plan.hpp"
#include "win/gpu/impl.hpp"
```
на:
```cpp
#include "core/d3d7.hpp"
#include "core/scale.hpp"
#include "core/window_plan.hpp"
#include "win/gpu/impl.hpp"
```

В `src/win/gpu/device.cpp` заменить:
```cpp
void Renderer::Impl::clear(const ClearCmd& c) {
    D3D12_RECT r{c.rect.left, c.rect.top, c.rect.right, c.rect.bottom};
```
на:
```cpp
void Renderer::Impl::clear(const ClearCmd& c) {
    const Rect t = scaleRect(c.rect, scale);  // game pixels -> target pixels
    D3D12_RECT r{t.left, t.top, t.right, t.bottom};
```

В `src/win/gpu/device.cpp` заменить функцию `void Renderer::Impl::setViewport(` целиком:
```cpp
void Renderer::Impl::setViewport(const ViewportCmd& v) {
    gameViewport = v;
    applyViewport();
}
```

В `src/win/gpu/device.cpp` после функции `void Renderer::Impl::setViewport(` добавить:
```cpp
// Pre-transformed vertices carry their screen position: the D3D7 viewport only clips them and gives the depth
// range. So the D3D12 viewport is the whole target and the client's viewport becomes the scissor rectangle.
void Renderer::Impl::applyViewport() {
    viewport = D3D12_VIEWPORT{0.0f, 0.0f, float(width), float(height), gameViewport.minZ, gameViewport.maxZ};
    const Rect r = scaleRect(gameViewport.rect, scale);
    const LONG left = std::max(r.left, 0), top = std::max(r.top, 0);
    scissor = D3D12_RECT{left, top, std::max<LONG>(left, std::min(r.right, width)),
                         std::max<LONG>(top, std::min(r.bottom, height))};
    if (open && targetBound) {  // setTarget and setScale call this between command lists
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
    }
}
```

В `src/win/gpu/device.cpp` после функции `void Renderer::Impl::writeTarget(` добавить:
```cpp
// Draws srcRect (in the source's pixels) of a texture into dstRect of a render target with the present pipeline:
// one bilinear sample per pixel when enlarging, an average over each pixel's footprint when shrinking.
void Renderer::Impl::blit(ID3D12Resource* src, D3D12_RESOURCE_STATES& srcState, UINT srcSrv, int srcW, int srcH,
                          const float srcRect[4], D3D12_CPU_DESCRIPTOR_HANDLE dst, const Rect& dstRect, int dstW,
                          int dstH) {
    if (dstRect.empty() || srcW <= 0 || srcH <= 0) return;
    StageSetup linearClamp;
    linearClamp.addressU = linearClamp.addressV = d3d7::TADDRESS_CLAMP;
    linearClamp.magFilter = d3d7::TFG_LINEAR;
    linearClamp.minFilter = d3d7::TFN_LINEAR;
    linearClamp.mipFilter = d3d7::TFP_NONE;
    const D3D12_GPU_DESCRIPTOR_HANDLE samplers = samplerTable(linearClamp, linearClamp);  // first: may restart the list
    const D3D12_GPU_DESCRIPTOR_HANDLE textures = textureTable(srcSrv, srcSrv);
    transition(src, srcState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const D3D12_VIEWPORT vp{float(dstRect.left), float(dstRect.top), float(dstRect.width()), float(dstRect.height()),
                            0.0f, 1.0f};
    const D3D12_RECT clip{std::max<LONG>(dstRect.left, 0), std::max<LONG>(dstRect.top, 0),
                          std::min<LONG>(dstRect.right, dstW), std::min<LONG>(dstRect.bottom, dstH)};
    list->OMSetRenderTargets(1, &dst, FALSE, nullptr);
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &clip);
    list->SetGraphicsRootSignature(root);
    list->SetPipelineState(presentPipeline);
    Constants c{};
    c.sourceRect[0] = srcRect[0] / float(srcW);
    c.sourceRect[1] = srcRect[1] / float(srcH);
    c.sourceRect[2] = srcRect[2] / float(srcW);
    c.sourceRect[3] = srcRect[3] / float(srcH);
    c.footprint = std::max((srcRect[2] - srcRect[0]) / float(dstRect.width()),
                           (srcRect[3] - srcRect[1]) / float(dstRect.height()));
    list->SetGraphicsRoot32BitConstants(kRootConstants, kConstantCount, &c, 0);
    list->SetGraphicsRootDescriptorTable(kRootTextures, textures);
    list->SetGraphicsRootDescriptorTable(kRootSamplers, samplers);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    boundPipeline = nullptr;  // the next draws bind everything again
    boundTextures = boundSamplers = ~0ull;
    targetBound = false;
}

bool Renderer::Impl::createTarget(int w, int h, ID3D12Resource*& rt, ID3D12Resource*& ds) {
    D3D12_CLEAR_VALUE black{};
    black.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    black.Color[3] = 1.0f;
    rt = createTexture2D(device, w, h, 1, DXGI_FORMAT_B8G8R8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                         D3D12_RESOURCE_STATE_RENDER_TARGET, &black);
    D3D12_CLEAR_VALUE farthest{};  // not "far": windef.h defines far as an empty macro
    farthest.Format = DXGI_FORMAT_D32_FLOAT;
    farthest.DepthStencil.Depth = 1.0f;
    ds = createTexture2D(device, w, h, 1, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                         D3D12_RESOURCE_STATE_DEPTH_WRITE, &farthest);
    if (rt && ds) return true;
    releaseCom(rt);
    releaseCom(ds);
    LOG_ERROR("cannot create a %dx%d render target", w, h);
    return false;
}
```

В `src/win/gpu/device.cpp` заменить:
```cpp
bool Renderer::Impl::ensureSwapChain(HWND hwnd, bool wantTearing) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const UINT w = UINT(std::max<LONG>(1, rc.right - rc.left));
    const UINT h = UINT(std::max<LONG>(1, rc.bottom - rc.top));
    if (swapChain && hwnd == window && w == swapWidth && h == swapHeight) return true;
```
на:
```cpp
bool Renderer::Impl::ensureSwapChain(HWND hwnd, UINT w, UINT h, bool wantTearing) {
    if (swapChain && hwnd == window && w == swapWidth && h == swapHeight) return true;
```

В `src/win/gpu/device.cpp` заменить:
```cpp
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3, false, m.rtvHeap) ||
```
на:
```cpp
        !heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kRtvCount, false, m.rtvHeap) ||
```

В `src/win/gpu/device.cpp` заменить:
```cpp
std::string Renderer::adapterName() const { return impl_->adapter; }
int Renderer::targetWidth() const { return impl_->width; }
int Renderer::targetHeight() const { return impl_->height; }
```
на:
```cpp
std::string Renderer::adapterName() const { return impl_->adapter; }
```

В `src/win/gpu/device.cpp` заменить функцию `bool Renderer::setTarget(` целиком:
```cpp
bool Renderer::setTarget(int width, int height) {
    Impl& m = *impl_;
    if (width <= 0 || height <= 0) return false;
    if (width == m.gameWidth && height == m.gameHeight && m.target) return true;
    m.waitIdle();
    releaseCom(m.target);
    releaseCom(m.depth);
    if (!m.createTarget(width, height, m.target, m.depth)) return false;
    m.targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m.device->CreateRenderTargetView(m.target, nullptr, m.rtvHandle(kRtvTarget));
    m.device->CreateDepthStencilView(m.depth, nullptr, m.dsvHandle());
    m.device->CreateShaderResourceView(m.target, nullptr, m.cpuSrvHandle(m.targetSrv));
    m.width = m.gameWidth = width;
    m.height = m.gameHeight = height;
    m.scale = 1.0;  // until the first present (stage 3 spec §3): the start-up calibration sees DX7's picture
    m.gameViewport = ViewportCmd{Rect{0, 0, width, height}, 0.0f, 1.0f};
    m.applyViewport();
    if (!m.begin()) return false;
    const float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m.list->ClearRenderTargetView(m.rtvHandle(kRtvTarget), color, 0, nullptr);
    m.list->ClearDepthStencilView(m.dsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m.submit();
    LOG_INFO("render target %dx%d", width, height);
    return true;
}
```

В `src/win/gpu/device.cpp` после функции `bool Renderer::setTarget(` добавить:
```cpp
bool Renderer::setScale(double k, int targetWidth, int targetHeight) {
    Impl& m = *impl_;
    if (m.lost || !m.target || targetWidth <= 0 || targetHeight <= 0) return false;
    if (k == m.scale && targetWidth == m.width && targetHeight == m.height) return true;
    ID3D12Resource* target = nullptr;
    ID3D12Resource* depth = nullptr;
    if (!m.createTarget(targetWidth, targetHeight, target, depth)) return false;
    // The back buffer keeps its picture across a present: the old target is stretched into the new one.
    if (!m.begin()) {
        releaseCom(target);
        releaseCom(depth);
        return false;
    }
    m.device->CreateRenderTargetView(target, nullptr, m.rtvHandle(kRtvNew));
    const float all[4] = {0.0f, 0.0f, float(m.width), float(m.height)};
    m.blit(m.target, m.targetState, m.targetSrv, m.width, m.height, all, m.rtvHandle(kRtvNew),
           Rect{0, 0, targetWidth, targetHeight}, targetWidth, targetHeight);
    m.waitIdle();
    releaseCom(m.target);
    releaseCom(m.depth);
    m.target = target;
    m.depth = depth;
    m.targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m.device->CreateRenderTargetView(m.target, nullptr, m.rtvHandle(kRtvTarget));
    m.device->CreateDepthStencilView(m.depth, nullptr, m.dsvHandle());
    m.device->CreateShaderResourceView(m.target, nullptr, m.cpuSrvHandle(m.targetSrv));
    m.width = targetWidth;
    m.height = targetHeight;
    m.scale = k;
    m.applyViewport();
    if (!m.begin()) return false;
    m.list->ClearDepthStencilView(m.dsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m.submit();
    LOG_INFO("render target %dx%d: %.3f times the game's %dx%d", targetWidth, targetHeight, k, m.gameWidth,
             m.gameHeight);
    return true;
}
```

В `src/win/gpu/device.cpp` заменить функцию `void Renderer::present(` целиком:
```cpp
void Renderer::present(HWND window, int outputWidth, int outputHeight, const Rect& source,
                       const PresentOptions& options) {
    Impl& m = *impl_;
    if (m.lost || !m.target || !window || source.empty()) return;
    if (!m.ensureSwapChain(window, UINT(std::max(1, outputWidth)), UINT(std::max(1, outputHeight)),
                           options.allowTearing) ||
        !m.begin())
        return;
    const UINT index = m.swapChain->GetCurrentBackBufferIndex();
    m.transition(m.backBuffers[index], m.backStates[index], D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m.rtvHandle(kRtvSwap + index);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m.list->ClearRenderTargetView(rtv, black, 0, nullptr);
    // Fit the source into the window, keeping its shape, on whole pixels: bars where the shapes differ.
    const Rect fit = fitPicture(source.width(), source.height(), int(m.swapWidth), int(m.swapHeight));
    if (!(fit == m.picture)) {
        m.picture = fit;
        LOG_INFO("picture %dx%d of the game shown as %dx%d at %d,%d in the %ux%u window", source.width(),
                 source.height(), fit.width(), fit.height(), fit.left, fit.top, m.swapWidth, m.swapHeight);
    }
    // The source is in game pixels: into target pixels by each axis' own ratio, so the whole frame is exactly the
    // whole target and at render_scale 100, where the target is the picture, it is copied pixel for pixel.
    const auto tx = [&m](int x) { return float(double(x) * m.width / m.gameWidth); };
    const auto ty = [&m](int y) { return float(double(y) * m.height / m.gameHeight); };
    const float from[4] = {tx(source.left), ty(source.top), tx(source.right), ty(source.bottom)};
    m.blit(m.target, m.targetState, m.targetSrv, m.width, m.height, from, rtv, fit, int(m.swapWidth),
           int(m.swapHeight));
    m.transition(m.backBuffers[index], m.backStates[index], D3D12_RESOURCE_STATE_PRESENT);
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
    if (FAILED(hr))
        m.failed(5, "Present", hr);
    else if (hr == DXGI_STATUS_OCCLUDED)
        Sleep(15);  // the window is covered: Present does not wait for vblank, keep the game's loop from spinning
}
```

- [ ] **Step 4: Контекст — план масштаба на каждом показе**

В `src/win/context.hpp` заменить:
```cpp
    CRITICAL_SECTION cs_;
    Stats stats_;
```
на:
```cpp
    CRITICAL_SECTION cs_;
    Stats stats_;
    int scaledWidth_ = 0;  // the target size the last scale plan asked for (stage 3)
    int scaledHeight_ = 0;
```

В `src/win/context.cpp` заменить:
```cpp
#include "win/log.hpp"
#include "win/window_mode.hpp"
```
на:
```cpp
#include "core/scale.hpp"
#include "win/dpi.hpp"
#include "win/log.hpp"
#include "win/window_mode.hpp"
```

В `src/win/context.cpp` заменить:
```cpp
    window = nullptr;
    windowApplied = false;
}
```
на:
```cpp
    window = nullptr;
    windowApplied = false;
    scaledWidth_ = scaledHeight_ = 0;
}
```

В `src/win/context.cpp` заменить:
```cpp
    if (!renderer || !renderer->setTarget(width, height)) return false;
    record.reset();
```
на:
```cpp
    if (!renderer || !renderer->setTarget(width, height)) return false;
    scaledWidth_ = width;  // the renderer is back at the game's size until the next present
    scaledHeight_ = height;
    record.reset();
```

В `src/win/context.cpp` заменить:
```cpp
    gpu::PresentOptions o;
    o.vsync = config.vsync && waitForVblank;
    o.allowTearing = config.allowTearing;
    o.fpsLimit = config.fpsLimit;
    renderer->present(window, source, o);
```
на:
```cpp
    // Stage 3: the window's real pixels and the scale for them. The first present switches from the game's size:
    // the start-up calibration has run by then (spec §4.3).
    const SIZE out = window ? clientPixels(window, config.highResolution) : SIZE{shadow.width(), shadow.height()};
    const ScalePlan plan =
        planScale(shadow.width(), shadow.height(), out.cx, out.cy, config.renderScale, config.highResolution);
    if (plan.targetWidth != scaledWidth_ || plan.targetHeight != scaledHeight_) {
        scaledWidth_ = plan.targetWidth;  // also when it fails: no new attempt every frame
        scaledHeight_ = plan.targetHeight;
        if (renderer->setScale(plan.k, plan.targetWidth, plan.targetHeight))
            LOG_INFO("scale: game %dx%d, output %ldx%ld, s %.3f, r %.2f, k %.3f, target %dx%d, picture %dx%d at %d,%d",
                     shadow.width(), shadow.height(), out.cx, out.cy, plan.s, plan.r, plan.k, plan.targetWidth,
                     plan.targetHeight, plan.picture.width(), plan.picture.height(), plan.picture.left,
                     plan.picture.top);
    }
    gpu::PresentOptions o;
    o.vsync = config.vsync && waitForVblank;
    o.allowTearing = config.allowTearing;
    o.fpsLimit = config.fpsLimit;
    renderer->present(window, out.cx, out.cy, source, o);
```

- [ ] **Step 5: Собирается**

Run: `cmake --build build-win -j 2>&1 | grep -E "warning|error" ; python3 tools/check_dll.py && cmake --build build-host -j && ./build-host/uagfx_tests | tail -1`
Expected: ни одной строки `warning`/`error`; `OK: … imports gdi32.dll kernel32.dll msvcrt.dll user32.dll` (новых
DLL нет: `SetThreadDpiAwarenessContext` — через `GetProcAddress`); `… tests, 0 failed checks`.

- [ ] **Step 6: Commit**

```bash
git add src/win/dpi.hpp src/win/dpi.cpp src/win/gpu/impl.hpp src/win/gpu/renderer.hpp src/win/gpu/device.cpp src/win/context.hpp src/win/context.cpp
git commit -m "Рендер в масштабе: цель k раз больше игры, копирование blit, показ в настоящих точках окна

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 9: Чтение и запись кадра клиентом в масштабе

Клиент видит кадр своего размера `L`. Чтение (Blt RT → память, Lock с касанием, GetDC): цель уменьшается на GPU
усреднением во вспомогательную текстуру размера игры, она читается назад. Запись (Unlock после записи, Blt память →
RT, ReleaseDC): точки клиента загружаются в ту же текстуру и растягиваются в цель (уточнение §5).

**Files:**
- Modify: `src/win/gpu/impl.hpp`, `src/win/gpu/device.cpp`

**Interfaces:**
- Consumes: `blit`, `createTarget`, `kRtvGame`, `scaleRect` (Tasks 4, 8).
- Produces: `Renderer::readTarget` всегда отдаёт `gameWidth × gameHeight` точек; `Impl::gameSized`, `gameState`,
  `gameSrv`; `bool Impl::scaled() const`.

- [ ] **Step 1: Текстура размера игры**

В `src/win/gpu/impl.hpp` заменить:
```cpp
    D3D12_RECT scissor{};
```
на:
```cpp
    D3D12_RECT scissor{};
    // The game-sized helper: the target shrunk for reading back, the client's pixels before they are stretched in.
    ID3D12Resource* gameSized = nullptr;
    D3D12_RESOURCE_STATES gameState = D3D12_RESOURCE_STATE_COPY_DEST;
    UINT gameSrv = 0;
```

В `src/win/gpu/impl.hpp` заменить:
```cpp
    bool createTarget(int w, int h, ID3D12Resource*& rt, ID3D12Resource*& ds);
```
на:
```cpp
    bool createTarget(int w, int h, ID3D12Resource*& rt, ID3D12Resource*& ds);
    bool scaled() const { return width != gameWidth || height != gameHeight; }
```

В `src/win/gpu/device.cpp` заменить:
```cpp
    m.targetSrv = m.allocSrv();
    if (!m.createPipelines(error)) return false;
```
на:
```cpp
    m.targetSrv = m.allocSrv();
    m.gameSrv = m.allocSrv();
    if (!m.createPipelines(error)) return false;
```

В `src/win/gpu/device.cpp` заменить:
```cpp
    releaseCom(target);
    releaseCom(depth);
```
на:
```cpp
    releaseCom(target);
    releaseCom(depth);
    releaseCom(gameSized);
```

В `src/win/gpu/device.cpp` заменить функцию `bool Renderer::setTarget(` целиком:
```cpp
bool Renderer::setTarget(int width, int height) {
    Impl& m = *impl_;
    if (width <= 0 || height <= 0) return false;
    if (width == m.gameWidth && height == m.gameHeight && m.target) return true;
    m.waitIdle();
    releaseCom(m.target);
    releaseCom(m.depth);
    releaseCom(m.gameSized);
    if (!m.createTarget(width, height, m.target, m.depth)) return false;
    m.gameSized = createTexture2D(m.device, width, height, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                                  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST, nullptr);
    if (!m.gameSized) {
        LOG_ERROR("cannot create the %dx%d helper of the game's size", width, height);
        return false;
    }
    m.targetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m.gameState = D3D12_RESOURCE_STATE_COPY_DEST;
    m.device->CreateRenderTargetView(m.target, nullptr, m.rtvHandle(kRtvTarget));
    m.device->CreateDepthStencilView(m.depth, nullptr, m.dsvHandle());
    m.device->CreateShaderResourceView(m.target, nullptr, m.cpuSrvHandle(m.targetSrv));
    m.device->CreateRenderTargetView(m.gameSized, nullptr, m.rtvHandle(kRtvGame));
    m.device->CreateShaderResourceView(m.gameSized, nullptr, m.cpuSrvHandle(m.gameSrv));
    m.width = m.gameWidth = width;
    m.height = m.gameHeight = height;
    m.scale = 1.0;  // until the first present (stage 3 spec §3): the start-up calibration sees DX7's picture
    m.gameViewport = ViewportCmd{Rect{0, 0, width, height}, 0.0f, 1.0f};
    m.applyViewport();
    if (!m.begin()) return false;
    const float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m.list->ClearRenderTargetView(m.rtvHandle(kRtvTarget), color, 0, nullptr);
    m.list->ClearDepthStencilView(m.dsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m.submit();
    LOG_INFO("render target %dx%d", width, height);
    return true;
}
```

- [ ] **Step 2: Запись и чтение в масштабе**

В `src/win/gpu/device.cpp` заменить функцию `void Renderer::Impl::writeTarget(` целиком:
```cpp
void Renderer::Impl::writeTarget(const FrameRecord& rec, const WriteTargetCmd& w) {
    if (w.rect.empty()) return;
    const D3D12_TEXTURE_COPY_LOCATION src = stage(*this, rec.pixels().data() + w.pixels, UINT(w.rect.width()),
                                                  UINT(w.rect.height()));
    if (!src.pResource) return;
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    if (!scaled()) {  // the target has the game's size: straight in
        transition(target, targetState, D3D12_RESOURCE_STATE_COPY_DEST);
        dst.pResource = target;
        list->CopyTextureRegion(&dst, UINT(w.rect.left), UINT(w.rect.top), 0, &src, nullptr);
        return;
    }
    // A larger target (stage 3 spec §5): the client's pixels go into the game-sized helper and are stretched in.
    transition(gameSized, gameState, D3D12_RESOURCE_STATE_COPY_DEST);
    dst.pResource = gameSized;
    list->CopyTextureRegion(&dst, UINT(w.rect.left), UINT(w.rect.top), 0, &src, nullptr);
    transition(target, targetState, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float from[4] = {float(w.rect.left), float(w.rect.top), float(w.rect.right), float(w.rect.bottom)};
    blit(gameSized, gameState, gameSrv, gameWidth, gameHeight, from, rtvHandle(kRtvTarget), scaleRect(w.rect, scale),
         width, height);
}
```

В `src/win/gpu/device.cpp` заменить функцию `bool Renderer::readTarget(` целиком:
```cpp
bool Renderer::readTarget(std::vector<uint32_t>& argb) {
    Impl& m = *impl_;
    if (m.lost || !m.target || !m.begin()) return false;
    ID3D12Resource* source = m.target;
    D3D12_RESOURCE_STATES* sourceState = &m.targetState;
    if (m.scaled()) {  // the client reads its own size: shrink the target into the helper first (stage 3 spec §5)
        m.transition(m.gameSized, m.gameState, D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float all[4] = {0.0f, 0.0f, float(m.width), float(m.height)};
        m.blit(m.target, m.targetState, m.targetSrv, m.width, m.height, all, m.rtvHandle(kRtvGame),
               Rect{0, 0, m.gameWidth, m.gameHeight}, m.gameWidth, m.gameHeight);
        source = m.gameSized;
        sourceState = &m.gameState;
    }
    const int w = m.gameWidth;
    const int h = m.gameHeight;
    const UINT pitch = UINT(alignUp(UINT64(w) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
    const UINT64 size = UINT64(pitch) * UINT64(h);
    if (m.readbackSize < size) {
        releaseCom(m.readback);
        m.readback = createBuffer(m.device, D3D12_HEAP_TYPE_READBACK, size, D3D12_RESOURCE_STATE_COPY_DEST);
        m.readbackSize = m.readback ? size : 0;
        if (!m.readback) return false;
    }
    m.transition(source, *sourceState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = m.readback;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Offset = 0;
    dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    dst.PlacedFootprint.Footprint.Width = UINT(w);
    dst.PlacedFootprint.Footprint.Height = UINT(h);
    dst.PlacedFootprint.Footprint.Depth = 1;
    dst.PlacedFootprint.Footprint.RowPitch = pitch;
    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = source;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;
    m.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    m.waitIdle();
    uint8_t* data = nullptr;
    D3D12_RANGE all{0, SIZE_T(size)};
    const HRESULT hr = m.readback->Map(0, &all, reinterpret_cast<void**>(&data));
    if (FAILED(hr)) {
        m.failed(6, "reading the frame back", hr);
        return false;
    }
    argb.resize(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        std::memcpy(argb.data() + size_t(y) * size_t(w), data + UINT64(y) * pitch, size_t(w) * 4);
    D3D12_RANGE none{0, 0};
    m.readback->Unmap(0, &none);
    return true;
}
```

В `src/win/gpu/renderer.hpp` заменить:
```cpp
    bool readTarget(std::vector<uint32_t>& argb);   // waits; width * height values, row-major
```
на:
```cpp
    bool readTarget(std::vector<uint32_t>& argb);   // waits; the game's width * height values, row-major
```

- [ ] **Step 3: Собирается**

Run: `cmake --build build-win -j 2>&1 | grep -E "warning|error" ; python3 tools/check_dll.py && cmake --build build-host -j && ./build-host/uagfx_tests | tail -1`
Expected: ни одной строки `warning`/`error`; `OK: …`; `… tests, 0 failed checks`.

- [ ] **Step 4: Commit**

```bash
git add src/win/gpu/impl.hpp src/win/gpu/device.cpp src/win/gpu/renderer.hpp
git commit -m "Чтение и запись кадра клиентом при масштабе: уменьшение и растяжение через текстуру размера игры

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 10: Версия 0.3.0, установка, чек-лист, пакет

**Files:**
- Modify: `src/core/version.cpp`, `tests/test_version.cpp`, `dist/install.bat`, `dist/TEST.ru.md`,
  `tools/check_package.py`
- Create: `release/uaro-graphics-0.3.0.zip`
- Delete: `release/uaro-graphics-0.2.0.zip`, `release/uaro-dpi-test.zip` (эксперимент DPI сделал своё)

**Interfaces:** —

- [ ] **Step 1: Падающий тест версии**

В `tests/test_version.cpp` заменить:
```cpp
TEST(version_is_0_2_0) { CHECK(std::strcmp(uagfx::kVersion, "0.2.0") == 0); }
```
на:
```cpp
TEST(version_is_0_3_0) { CHECK(std::strcmp(uagfx::kVersion, "0.3.0") == 0); }
```

Run: `cmake --build build-host -j && ./build-host/uagfx_tests version`
Expected: `FAIL version_is_0_3_0`, `1 tests, 1 failed checks`.

- [ ] **Step 2: Версия**

В `src/core/version.cpp` заменить:
```cpp
const char* const kVersion = "0.2.0";
```
на:
```cpp
const char* const kVersion = "0.3.0";
```

Run: `cmake --build build-host -j && ./build-host/uagfx_tests version`
Expected: `PASS version_is_0_3_0`, `1 tests, 0 failed checks`.

- [ ] **Step 3: Установка сохраняет выбор рендера и даёт новые настройки**

Файл настроек 0.1/0.2 не знает `high_resolution` и остальных новых ключей. `install.bat` заменяет такой файл новым
(с пояснениями ко всем настройкам), сохраняя `backend = uaro`, если он был.

В `dist/install.bat` заменить:
```bat
rem The settings file of version 0.1 holds only defaults; 0.2 changes some (mode = auto), so it is replaced.
if exist "uaro-graphics.ini" (
    findstr /c:"not in version 0.1 yet" "uaro-graphics.ini" >nul 2>&1
    if not errorlevel 1 copy /y "uaro-graphics\uaro-graphics.ini" "uaro-graphics.ini" >nul
)
```
на:
```bat
rem A settings file older than 0.3 holds the defaults and the renderer choice: it is replaced by the new one,
rem with every new setting and its notes, and keeps that choice.
if exist "uaro-graphics.ini" (
    findstr /c:"high_resolution" "uaro-graphics.ini" >nul 2>&1
    if errorlevel 1 (
        set "uaro="
        findstr /i /r /c:"^ *backend *= *uaro" "uaro-graphics.ini" >nul 2>&1 && set "uaro=1"
        copy /y "uaro-graphics\uaro-graphics.ini" "uaro-graphics.ini" >nul
        if defined uaro powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='uaro-graphics.ini'; (Get-Content -LiteralPath $f) -replace '^backend = dgvoodoo$','backend = uaro' | Set-Content -LiteralPath $f -Encoding ASCII"
    )
)
```

В `tools/check_package.py` заменить:
```python
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/uaro-graphics-0.2.0.zip")
```
на:
```python
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/uaro-graphics-0.3.0.zip")
```

- [ ] **Step 4: Чек-лист 0.3.0**

`dist/TEST.ru.md` целиком:
```markdown
# uaRO Graphics 0.3.0 — проверка

Что нового: на мониторе с масштабом Windows (например, 4K и 175 %) игра рисуется в настоящих точках экрана. Мир
(земля, дома, деревья, края моделей) чётче, окно и интерфейс — того же размера, мышь, ROExt и геймпад — как раньше.

## Установка

1. Закрой игру. Распакуй архив в папку клиента — туда, где лежит `uaRO.exe` (с папками, как в архиве).
2. Дважды щёлкни `install.bat`. Файл настроек от 0.2 он заменит новым (там появились новые настройки), выбор
   рендера сохранится. Если игра вдруг идёт через dgVoodoo — дважды щёлкни `use-uaro.bat`.

## Проверка

1. Запусти игру. В `uaro-graphics.log` есть строка `scale: game 1920x1080, output 3360x1890, …, k 1.750` (числа —
   под твоё окно и масштаб Windows).
2. Мир чётче, чем в 0.2.0: земля, дома, деревья, края моделей. Для сравнения в `uaro-graphics.ini` (раздел
   `[display]`) поставь `high_resolution = 0` — будет как 0.2.0; верни `1`.
3. Интерфейс, текст и курсор игры: резкие, без мыла; между частями окон нет швов и полосок. Попробуй
   `ui_filter = smooth` — мягче, как раньше. Напиши, что больше нравится.
4. По левому и верхнему краю окна нет чёрной полоски, картинка не сдвинута.
5. Курсор, клики, ROExt и геймпад попадают туда же, куда в 0.2.0.
6. Скриншот клавишей игры: файл того же размера, что раньше (1920×1080), с подписью даты и времени.
7. Смена карты, затемнение, эффекты умений — без мусора; заставка-видео (если есть) идёт.
8. Плавность (строка с fps в логе раз в 30 секунд), Alt+Tab, свернуть и развернуть окно.
9. По желанию: `render_scale = 150` (раздел `[quality]`) — края ещё мягче, видеокарта работает больше. Верни `100`.

## Если что-то не так

Сделай снимок экрана (Win+Shift+S), запомни, что делал, и включи dgVoodoo обратно: `use-dgvoodoo.bat`.

## Что прислать

Файл `uaro-graphics.log` и коротко по пунктам 1–9. Лучше всего — снимок экрана в игре с 0.3.0 и с
`high_resolution = 0` в одном и том же месте: так видна разница. Если клиент в `Program Files`, Windows кладёт лог в
`%LOCALAPPDATA%\VirtualStore\Program Files (x86)\…` (вставь этот путь в адресную строку Проводника).

## Удаление

Дважды щёлкни `uninstall.bat` — всё вернётся как было.
```

- [ ] **Step 5: Полный прогон и пакет**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests | tail -1 && cmake --build build-win -j 2>&1 | grep -E "warning|error" ; tools/package.sh && file dist/*.bat && grep -c $'\r$' dist/install.bat && wc -l < dist/install.bat`
Expected: `122 tests, 0 failed checks`; ни одной строки `warning`/`error`; `OK: build-win/uaro-graphics-0.3.0.zip: TEST.ru.md,
install.bat, uaro-graphics/ddraw.dll, uaro-graphics/uaro-graphics.ini, uninstall.bat, use-dgvoodoo.bat, use-uaro.bat`;
все `.bat` — `with CRLF line terminators`; два одинаковых числа у `install.bat`.

- [ ] **Step 6: Пакет в репозиторий и commit**

Run: `cp build-win/uaro-graphics-0.3.0.zip release/ && git rm -q release/uaro-graphics-0.2.0.zip release/uaro-dpi-test.zip && ls release`
Expected: в `release/` — только `uaro-graphics-0.3.0.zip` (копия до `git rm`, папка не исчезает).

```bash
git add src/core/version.cpp tests/test_version.cpp dist/install.bat dist/TEST.ru.md tools/check_package.py release/uaro-graphics-0.3.0.zip
git commit -m "0.3.0: игра в настоящих точках окна (масштаб Windows), установка сохраняет выбор рендера, чек-лист

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Пуш и проверку на ПК пользователя делает финальный шаг исполнения (superpowers:finishing-a-development-branch).

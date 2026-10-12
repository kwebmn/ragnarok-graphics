# uaRO Graphics — этап 1A: фундамент и сквозной ddraw.dll (план)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** собрать `ddraw.dll` 0.1.0, который игра грузит вместо dgVoodoo: он читает `uaro-graphics.ini`, пишет
понятный лог, выбирает рендер и пока передаёт все вызовы в dgVoodoo из подпапки `dgvoodoo\` (или в системный
DirectDraw), плюс переносимое ядро (форматы пикселей и операции поверхностей) с тестами на Linux.

**Architecture:** переносимое ядро `src/core` (без `windows.h`, тесты на Linux) и Windows-часть `src/win`
(экспорты ddraw, лог, проверки системы, загрузка настоящего ddraw). Решение «какой рендер» — чистая функция в ядре;
Windows-часть только собирает факты и исполняет решение. Рендер uaRO (D3D12) в 0.1.0 ещё не собран — выбор `uaro`
честно откатывается на dgVoodoo с причиной в логе.

**Tech Stack:** C++17, CMake 3.20+, MinGW-w64 i686 GCC 12 (кросс-сборка DLL), GCC/Clang хоста (тесты),
Python 3 (проверка DLL), zip.

**Spec:** `docs/superpowers/specs/2026-10-13-uaro-graphics-design.md` (§3.1, §3.2, §3.3 п. 2 и 6, §4.9 лог, §6, §7,
§9 этап 1). Этап 1 разбит на два плана: этот (1A) и 1B (DX7-объекты, запись кадра, D3D12) — 1B пишется после
справок `docs/research/*.md`.

## Global Constraints

- Клиент `uaRO.exe` — 32-битный: DLL собирается только как PE32 i386 (`cmake/mingw-i686.cmake`).
- `ddraw.dll` импортирует только системные DLL: `KERNEL32`, `USER32`, `GDI32`, `msvcrt`. `d3d12.dll`, `dxgi.dll`
  грузятся динамически — DLL должна загружаться и на Windows 7 (там — откат на dgVoodoo).
- Экспорт `DirectDrawCreateEx` начинается с пролога горячего патча `8B FF 55 8B EC` (ROExt ставит на него
  inline-перехват).
- Файлы в папке клиента: `ddraw.dll`, `uaro-graphics.ini`, `uaro-graphics.log`; dgVoodoo — в `dgvoodoo\DDraw.dll`,
  `dgvoodoo\D3DImm.dll`.
- Рендер по умолчанию — dgVoodoo (`[renderer] backend = dgvoodoo`).
- Ошибка драйвера не должна мешать запуску игры: всегда есть откат (dgVoodoo → системный ddraw).
- Версия этого плана — `0.1.0`.
- Стиль кода как в uaRO Pad: пространства имён `uagfx` (ядро) и `uagfx::win`, комментарии на английском,
  `-Wall -Wextra` без предупреждений, сообщения коммитов на русском с трейлером
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, автор коммитов — локальная настройка репозитория
  (KWEB Team).

## Review Focus

1. Нет `uaro-graphics.ini`, он пустой или сохранён Блокнотом (BOM, CRLF, заглавные буквы) — игра стартует с
   умолчаниями, в логе сказано, что ini не найден. Тесты: Task 2 (`config_*`).
2. Папки `dgvoodoo\` нет (игрок не перенёс файлы) — откат на системный ddraw, игра запускается, в логе причина.
   Тест: Task 3 (`choose_falls_back_to_system_without_dgvoodoo`).
3. ROExt перехватывает `DirectDrawCreateEx` — пролог горячего патча на месте. Тест: Task 4 (`tools/check_dll.py`).
4. Папка игры без прав записи (лог не создаётся) — DLL продолжает работу без лога, не падает. Проверка: Task 5,
   шаг «нет прав» в чек-листе `dist/TEST.ru.md`.
5. Windows 7 / нет D3D12 при `backend = uaro` — откат на dgVoodoo с причиной. Тесты: Task 3
   (`choose_uaro_needs_windows10`, `choose_uaro_needs_d3d12`), импорты без `d3d12`/`dxgi` — Task 4.

---

### Task 1: Каркас репозитория, сборка, раннер тестов, версия

**Files:**
- Create: `.gitignore`, `CMakeLists.txt`, `cmake/mingw-i686.cmake`
- Create: `src/core/version.hpp`, `src/core/version.cpp`
- Create: `tests/test.hpp`, `tests/test_main.cpp`, `tests/test_version.cpp`

**Interfaces:**
- Produces: библиотека `uagfx_core` (всё из `src/core/*.cpp`), тестовый бинарник `build-host/uagfx_tests`
  с макросами `TEST(name)`, `CHECK(cond)`, `CHECK_EQ(a, b)`; `extern const char* const uagfx::kVersion`.

- [ ] **Step 1: Каркас сборки и раннер тестов**

`.gitignore`:
```
build-host/
build-win/
```

`cmake/mingw-i686.cmake`:
```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20)
project(uaro_graphics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Portable core: formats, surfaces, config, decisions. No Windows headers; unit-tested on Linux.
file(GLOB_RECURSE UAGFX_CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
add_library(uagfx_core STATIC ${UAGFX_CORE_SOURCES})
target_include_directories(uagfx_core PUBLIC src)
target_compile_options(uagfx_core PRIVATE -Wall -Wextra)

if(NOT WIN32)
    enable_testing()
    file(GLOB UAGFX_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(uagfx_tests ${UAGFX_TEST_SOURCES})
    target_include_directories(uagfx_tests PRIVATE tests)
    target_link_libraries(uagfx_tests PRIVATE uagfx_core)
    add_test(NAME uagfx_tests COMMAND uagfx_tests)
endif()
```

`tests/test.hpp`:
```cpp
#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace t {
struct Case {
    const char* name;
    std::function<void()> fn;
};
std::vector<Case>& registry();
extern int failures;
struct Reg {
    Reg(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};
}  // namespace t

#define TEST(name)                         \
    static void name();                    \
    static t::Reg reg_##name(#name, name); \
    static void name()

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++t::failures;                                                \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        auto _va = (a);                                                          \
        auto _vb = (b);                                                          \
        if (!(_va == _vb)) {                                                     \
            std::printf("  FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); \
            ++t::failures;                                                       \
        }                                                                        \
    } while (0)
```

`tests/test_main.cpp`:
```cpp
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "test.hpp"

namespace {

const char* g_running = nullptr;

// A crashing test would otherwise take the whole report with it: name it and fail the run.
void onCrash(int sig) {
    std::printf("CRASH %s (signal %d)\n", g_running ? g_running : "?", sig);
    std::fflush(stdout);
    _exit(3);
}

}  // namespace

namespace t {
std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}
int failures = 0;
}  // namespace t

int main(int argc, char** argv) {
    std::signal(SIGSEGV, onCrash);
    std::signal(SIGABRT, onCrash);
    std::signal(SIGFPE, onCrash);
    const char* filter = argc > 1 ? argv[1] : "";
    int ran = 0;
    for (const t::Case& c : t::registry()) {
        if (*filter && !std::strstr(c.name, filter)) continue;
        const int before = t::failures;
        g_running = c.name;
        c.fn();
        ++ran;
        std::printf("%s %s\n", t::failures == before ? "PASS" : "FAIL", c.name);
        std::fflush(stdout);
    }
    std::printf("%d tests, %d failed checks\n", ran, t::failures);
    if (ran == 0) {
        std::printf("no tests match '%s'\n", filter);
        return 2;
    }
    return t::failures ? 1 : 0;
}
```

- [ ] **Step 2: Написать падающий тест версии**

`tests/test_version.cpp`:
```cpp
#include <cstring>

#include "core/version.hpp"
#include "test.hpp"

TEST(version_is_0_1_0) { CHECK(std::strcmp(uagfx::kVersion, "0.1.0") == 0); }
```

- [ ] **Step 3: Убедиться, что сборка падает**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host -j`
Expected: ошибка компиляции `core/version.hpp: No such file or directory`.

- [ ] **Step 4: Версия**

`src/core/version.hpp`:
```cpp
#pragma once

namespace uagfx {

extern const char* const kVersion;  // shown in the log and, later, in the settings app

}  // namespace uagfx
```

`src/core/version.cpp`:
```cpp
#include "core/version.hpp"

namespace uagfx {

const char* const kVersion = "0.1.0";

}  // namespace uagfx
```

- [ ] **Step 5: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests`
Expected: `PASS version_is_0_1_0`, `1 tests, 0 failed checks`.

- [ ] **Step 6: Commit**

```bash
git add .gitignore CMakeLists.txt cmake src tests
git commit -m "Каркас: CMake, кросс-сборка MinGW, раннер тестов, версия 0.1.0

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Настройки `uaro-graphics.ini`

**Files:**
- Create: `src/core/config.hpp`, `src/core/config.cpp`
- Create: `dist/uaro-graphics.ini`
- Test: `tests/test_config.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace uagfx {
  enum class LogLevel { Error = 0, Info = 1, Debug = 2 };
  enum class Backend { DgVoodoo, Uaro };
  enum class DisplayMode { Window, Borderless };
  struct Config { Backend backend; DisplayMode mode; int monitor; int windowWidth; int windowHeight;
                  bool vsync; int fpsLimit; bool allowTearing; LogLevel logLevel; };
  Config parseConfig(const std::string& text);
  const char* backendToken(Backend b);        // "dgvoodoo" | "uaro"
  const char* displayModeToken(DisplayMode m); // "window" | "borderless"
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_config.cpp`:
```cpp
#include <cstring>

#include "core/config.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(config_defaults_without_a_file) {
    const Config c = parseConfig("");
    CHECK(c.backend == Backend::DgVoodoo);
    CHECK(c.mode == DisplayMode::Borderless);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(!c.allowTearing);
    CHECK(c.logLevel == LogLevel::Info);
}

TEST(config_reads_every_key) {
    const Config c = parseConfig(
        "[renderer]\nbackend = uaro\n"
        "[display]\nmode = window\nmonitor = 1\nwindow_width = 1600\nwindow_height = 900\n"
        "[frames]\nvsync = 0\nfps_limit = 144\nallow_tearing = 1\n"
        "[debug]\nlog = verbose\n");
    CHECK(c.backend == Backend::Uaro);
    CHECK(c.mode == DisplayMode::Window);
    CHECK_EQ(c.monitor, 1);
    CHECK_EQ(c.windowWidth, 1600);
    CHECK_EQ(c.windowHeight, 900);
    CHECK(!c.vsync);
    CHECK_EQ(c.fpsLimit, 144);
    CHECK(c.allowTearing);
    CHECK(c.logLevel == LogLevel::Debug);
}

TEST(config_invalid_values_keep_defaults) {
    const Config c = parseConfig(
        "[renderer]\nbackend = vulkan\n"
        "[display]\nmode = fullscreen\nmonitor = -1\nwindow_width = 100\nwindow_height = abc\n"
        "[frames]\nvsync = maybe\nfps_limit = -5\n"
        "[debug]\nlog = loud\n");
    CHECK(c.backend == Backend::DgVoodoo);
    CHECK(c.mode == DisplayMode::Borderless);
    CHECK_EQ(c.monitor, 0);
    CHECK_EQ(c.windowWidth, 0);
    CHECK_EQ(c.windowHeight, 0);
    CHECK(c.vsync);
    CHECK_EQ(c.fpsLimit, 0);
    CHECK(c.logLevel == LogLevel::Info);
}

TEST(config_notepad_file_with_bom_crlf_comments_and_capitals) {
    const Config c = parseConfig("\xEF\xBB\xBF; uaRO Graphics\r\n[Renderer]\r\nBackend = UARO ; the new one\r\n");
    CHECK(c.backend == Backend::Uaro);
}

TEST(config_keys_outside_their_section_are_ignored) {
    const Config c = parseConfig("backend = uaro\n[display]\nbackend = uaro\n");
    CHECK(c.backend == Backend::DgVoodoo);
}

TEST(config_tokens) {
    CHECK(std::strcmp(backendToken(Backend::DgVoodoo), "dgvoodoo") == 0);
    CHECK(std::strcmp(backendToken(Backend::Uaro), "uaro") == 0);
    CHECK(std::strcmp(displayModeToken(DisplayMode::Window), "window") == 0);
    CHECK(std::strcmp(displayModeToken(DisplayMode::Borderless), "borderless") == 0);
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/config.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/config.hpp`:
```cpp
#pragma once
#include <string>

namespace uagfx {

enum class LogLevel { Error = 0, Info = 1, Debug = 2 };
enum class Backend { DgVoodoo, Uaro };          // [renderer] backend
enum class DisplayMode { Window, Borderless };  // [display] mode

// uaro-graphics.ini. Unknown keys and values that do not parse leave the default in place.
struct Config {
    Backend backend = Backend::DgVoodoo;
    DisplayMode mode = DisplayMode::Borderless;
    int monitor = 0;       // 0 = the main monitor
    int windowWidth = 0;   // window mode: inner size in pixels; 0 = the size the game asks for
    int windowHeight = 0;
    bool vsync = true;
    int fpsLimit = 0;      // 0 = no limit
    bool allowTearing = false;  // let G-SYNC / FreeSync monitors show frames as soon as they are ready
    LogLevel logLevel = LogLevel::Info;
};

Config parseConfig(const std::string& text);
const char* backendToken(Backend b);
const char* displayModeToken(DisplayMode m);

}  // namespace uagfx
```

`src/core/config.cpp`:
```cpp
#include "core/config.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace uagfx {
namespace {

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string lower(std::string s) {
    for (char& ch : s) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool toInt(const std::string& v, int lo, int hi, int& out) {
    char* end = nullptr;
    const long n = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str() || *end != '\0' || n < lo || n > hi) return false;
    out = int(n);
    return true;
}

void setBool(const std::string& v, bool& out) {
    if (v == "1" || v == "true" || v == "on") out = true;
    if (v == "0" || v == "false" || v == "off") out = false;
}

// A window side: 0 (the game's own size) or a real size.
void setSide(const std::string& v, int& out) {
    int n = 0;
    if (toInt(v, 0, 16384, n) && (n == 0 || n >= 320)) out = n;
}

}  // namespace

Config parseConfig(const std::string& text) {
    Config c;
    std::istringstream in(text);
    std::string line;
    std::string section;
    bool first = true;
    while (std::getline(in, line)) {
        if (first && line.rfind("\xEF\xBB\xBF", 0) == 0) line = line.substr(3);  // saved by Notepad with a BOM
        first = false;
        const size_t comment = line.find(';');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = lower(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = section + "." + lower(trim(line.substr(0, eq)));
        const std::string val = lower(trim(line.substr(eq + 1)));
        if (key == "renderer.backend") {
            if (val == "dgvoodoo") c.backend = Backend::DgVoodoo;
            if (val == "uaro") c.backend = Backend::Uaro;
        } else if (key == "display.mode") {
            if (val == "window") c.mode = DisplayMode::Window;
            if (val == "borderless") c.mode = DisplayMode::Borderless;
        } else if (key == "display.monitor") {
            toInt(val, 0, 16, c.monitor);
        } else if (key == "display.window_width") {
            setSide(val, c.windowWidth);
        } else if (key == "display.window_height") {
            setSide(val, c.windowHeight);
        } else if (key == "frames.vsync") {
            setBool(val, c.vsync);
        } else if (key == "frames.fps_limit") {
            toInt(val, 0, 1000, c.fpsLimit);
        } else if (key == "frames.allow_tearing") {
            setBool(val, c.allowTearing);
        } else if (key == "debug.log") {
            if (val == "normal") c.logLevel = LogLevel::Info;
            if (val == "verbose") c.logLevel = LogLevel::Debug;
        }
    }
    return c;
}

const char* backendToken(Backend b) { return b == Backend::Uaro ? "uaro" : "dgvoodoo"; }

const char* displayModeToken(DisplayMode m) { return m == DisplayMode::Window ? "window" : "borderless"; }

}  // namespace uagfx
```

`dist/uaro-graphics.ini`:
```ini
; uaRO Graphics - settings. Lines you do not change keep their defaults.
; The settings app (coming in a later version) edits this file and keeps these comments.

[renderer]
; dgvoodoo = keep using dgVoodoo (it lives in the dgvoodoo folder next to the game)
; uaro     = the uaRO Graphics Direct3D 12 renderer (Windows 10 or newer; not in version 0.1 yet -
;            the game then keeps using dgVoodoo and the log says why)
backend = dgvoodoo

[display]
; borderless = full screen without changing the monitor's mode; window = a normal window
mode = borderless
; 0 = the main monitor, 1 = the second one, ...
monitor = 0
; window mode only: the inner size of the window in pixels; 0 = the size the game asks for
window_width = 0
window_height = 0

[frames]
vsync = 1
; frames per second at most; 0 = no limit
fps_limit = 0
; 1 = let G-SYNC / FreeSync monitors show each frame as soon as it is ready
allow_tearing = 0

[debug]
; normal or verbose
log = normal
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests config`
Expected: 6 тестов `PASS config_…`, `6 tests, 0 failed checks`.

- [ ] **Step 5: Commit**

```bash
git add src/core/config.hpp src/core/config.cpp dist/uaro-graphics.ini tests/test_config.cpp
git commit -m "Настройки uaro-graphics.ini: рендер, экран, кадры, лог

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Выбор рендера (чистая логика)

**Files:**
- Create: `src/core/renderer_choice.hpp`, `src/core/renderer_choice.cpp`
- Test: `tests/test_renderer_choice.cpp`

**Interfaces:**
- Consumes: `uagfx::Config`, `uagfx::Backend` (Task 2).
- Produces:
  ```cpp
  namespace uagfx {
  struct SystemFacts { bool windows10 = false; bool d3d12 = false; bool dgvoodoo = false; };
  enum class Renderer { Uaro, DgVoodoo, System };
  struct RendererChoice { Renderer renderer; std::string reason; };
  RendererChoice chooseRenderer(const Config& config, const SystemFacts& facts, bool uaroBuilt);
  const char* rendererName(Renderer r);  // "uaRO Graphics" | "dgVoodoo" | "system DirectDraw"
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_renderer_choice.cpp`:
```cpp
#include <cstring>
#include <string>

#include "core/renderer_choice.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
Config wants(Backend b) {
    Config c;
    c.backend = b;
    return c;
}
SystemFacts everything() {
    SystemFacts f;
    f.windows10 = true;
    f.d3d12 = true;
    f.dgvoodoo = true;
    return f;
}
bool mentions(const std::string& reason, const char* what) { return reason.find(what) != std::string::npos; }
}  // namespace

TEST(choose_uaro_when_everything_is_there) {
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), everything(), true);
    CHECK(r.renderer == Renderer::Uaro);
}

TEST(choose_dgvoodoo_when_asked) {
    const RendererChoice r = chooseRenderer(wants(Backend::DgVoodoo), everything(), true);
    CHECK(r.renderer == Renderer::DgVoodoo);
    CHECK(mentions(r.reason, "backend = dgvoodoo"));
}

TEST(choose_uaro_not_built_falls_back_to_dgvoodoo) {
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), everything(), false);
    CHECK(r.renderer == Renderer::DgVoodoo);
    CHECK(mentions(r.reason, "not in this version"));
}

TEST(choose_uaro_needs_windows10) {
    SystemFacts f = everything();
    f.windows10 = false;
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), f, true);
    CHECK(r.renderer == Renderer::DgVoodoo);
    CHECK(mentions(r.reason, "Windows 10"));
}

TEST(choose_uaro_needs_d3d12) {
    SystemFacts f = everything();
    f.d3d12 = false;
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), f, true);
    CHECK(r.renderer == Renderer::DgVoodoo);
    CHECK(mentions(r.reason, "Direct3D 12"));
}

TEST(choose_falls_back_to_system_without_dgvoodoo) {
    SystemFacts f = everything();
    f.dgvoodoo = false;
    const RendererChoice r = chooseRenderer(wants(Backend::DgVoodoo), f, true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "dgvoodoo\\DDraw.dll not found"));
}

TEST(choose_uaro_failure_without_dgvoodoo_goes_to_system) {
    SystemFacts f;  // nothing at all: old Windows, no D3D12, no dgVoodoo
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), f, true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "Windows 10"));
}

TEST(renderer_names) {
    CHECK(std::strcmp(rendererName(Renderer::Uaro), "uaRO Graphics") == 0);
    CHECK(std::strcmp(rendererName(Renderer::DgVoodoo), "dgVoodoo") == 0);
    CHECK(std::strcmp(rendererName(Renderer::System), "system DirectDraw") == 0);
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/renderer_choice.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/renderer_choice.hpp`:
```cpp
#pragma once
#include <string>

#include "core/config.hpp"

namespace uagfx {

// What the running system offers; the Windows side finds out at start-up.
struct SystemFacts {
    bool windows10 = false;  // Windows 10 or newer (flip-model swap chains, D3D12)
    bool d3d12 = false;      // a Direct3D 12 device (feature level 11_0) could be created
    bool dgvoodoo = false;   // dgvoodoo\DDraw.dll exists next to the game
};

enum class Renderer { Uaro, DgVoodoo, System };

struct RendererChoice {
    Renderer renderer;
    std::string reason;  // one line for the log
};

// uaroBuilt: false while this version has no uaRO renderer yet (0.1.x passes the game through).
RendererChoice chooseRenderer(const Config& config, const SystemFacts& facts, bool uaroBuilt);
const char* rendererName(Renderer r);

}  // namespace uagfx
```

`src/core/renderer_choice.cpp`:
```cpp
#include "core/renderer_choice.hpp"

namespace uagfx {

RendererChoice chooseRenderer(const Config& config, const SystemFacts& facts, bool uaroBuilt) {
    std::string why;
    if (config.backend == Backend::Uaro) {
        if (!uaroBuilt) {
            why = "backend = uaro, but the uaRO renderer is not in this version";
        } else if (!facts.windows10) {
            why = "backend = uaro, but the uaRO renderer needs Windows 10 or newer";
        } else if (!facts.d3d12) {
            why = "backend = uaro, but no Direct3D 12 device could be created";
        } else {
            return {Renderer::Uaro, "backend = uaro"};
        }
    } else {
        why = "backend = dgvoodoo";
    }
    if (facts.dgvoodoo) return {Renderer::DgVoodoo, why};
    return {Renderer::System, why + "; dgvoodoo\\DDraw.dll not found"};
}

const char* rendererName(Renderer r) {
    switch (r) {
        case Renderer::Uaro: return "uaRO Graphics";
        case Renderer::DgVoodoo: return "dgVoodoo";
        case Renderer::System: break;
    }
    return "system DirectDraw";
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests` 
Expected: все тесты `PASS`, `15 tests, 0 failed checks`.

- [ ] **Step 5: Commit**

```bash
git add src/core/renderer_choice.hpp src/core/renderer_choice.cpp tests/test_renderer_choice.cpp
git commit -m "Выбор рендера: uaRO / dgVoodoo / системный DirectDraw с причиной для лога

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: `ddraw.dll`: экспорты, пролог горячего патча, передача в системный DirectDraw, лог

**Files:**
- Modify: `CMakeLists.txt` (ветка `WIN32`)
- Create: `src/win/ddraw.def`, `src/win/exports.cpp`, `src/win/real_ddraw.hpp`, `src/win/real_ddraw.cpp`
- Create: `src/win/log.hpp`, `src/win/log.cpp`, `src/win/paths.hpp`, `src/win/paths.cpp`
- Create: `tools/check_dll.py`

**Interfaces:**
- Consumes: `uagfx::LogLevel`, `uagfx::kVersion`.
- Produces:
  ```cpp
  namespace uagfx::wlog { void open(const std::string& path, LogLevel level); bool enabled(LogLevel);
                          void line(LogLevel level, const char* fmt, ...); }
  // LOG_ERROR(...), LOG_INFO(...), LOG_DEBUG(...)
  namespace uagfx::win {
  std::string gameDir();      // folder of the game's exe, no trailing backslash
  std::string systemDir();    // System32 (SysWOW64 for our 32-bit process on 64-bit Windows)
  std::string modulePath(HMODULE m);  // "" when m is null
  bool fileExists(const std::string& path);
  bool readTextFile(const std::string& path, std::string& out);
  HMODULE realModule();               // the DirectDraw everything is passed to (loaded on first use)
  FARPROC realProc(const char* name); // nullptr when missing
  }
  ```

- [ ] **Step 1: Скрипт проверки DLL (сначала падает)**

`tools/check_dll.py`:
```python
#!/usr/bin/env python3
"""Checks the cross-built ddraw.dll.

- It is a 32-bit DLL, like uaRO.exe.
- It exports what games and dgVoodoo users expect from ddraw.dll.
- DirectDrawCreateEx starts with the hot-patch prologue: ROExt inline-hooks it.
- It imports system DLLs only: d3d12.dll and dxgi.dll are loaded at run time, so the DLL loads on Windows 7.
"""
import struct
import sys

REQUIRED = ["DirectDrawCreate", "DirectDrawCreateClipper", "DirectDrawCreateEx", "DirectDrawEnumerateA",
            "DirectDrawEnumerateExA", "DirectDrawEnumerateExW", "DirectDrawEnumerateW",
            "DllCanUnloadNow", "DllGetClassObject"]
ALLOWED_IMPORTS = {"kernel32.dll", "user32.dll", "gdi32.dll", "msvcrt.dll"}
HOTPATCH = bytes([0x8B, 0xFF, 0x55, 0x8B, 0xEC])  # mov edi,edi / push ebp / mov ebp,esp


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def main(path):
    try:
        data = open(path, "rb").read()
    except OSError:
        fail(path + " not found")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        fail("not a PE file")
    machine, nsec = struct.unpack_from("<HH", data, pe + 4)
    if machine != 0x14C:
        fail("not a 32-bit (i386) DLL")
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        fail("not PE32")
    exp_rva = struct.unpack_from("<I", data, opt + 96)[0]
    imp_rva = struct.unpack_from("<I", data, opt + 104)[0]
    sections = [struct.unpack_from("<8sIIII", data, opt + optsize + i * 40) for i in range(nsec)]

    def off(rva):
        for _, vsize, va, rsize, raw in sections:
            if va <= rva < va + max(vsize, rsize):
                return rva - va + raw
        fail("rva %#x is outside the sections" % rva)

    def cstr(rva):
        o = off(rva)
        return data[o:data.index(b"\0", o)].decode()

    _, nnames, funcs, names, ords = struct.unpack_from("<IIIII", data, off(exp_rva) + 20)
    exports = {}
    for i in range(nnames):
        name = cstr(struct.unpack_from("<I", data, off(names) + 4 * i)[0])
        ordinal = struct.unpack_from("<H", data, off(ords) + 2 * i)[0]
        exports[name] = struct.unpack_from("<I", data, off(funcs) + 4 * ordinal)[0]
    missing = [n for n in REQUIRED if n not in exports]
    if missing:
        fail("missing exports: " + ", ".join(missing))
    start = off(exports["DirectDrawCreateEx"])
    if data[start:start + 5] != HOTPATCH:
        fail("DirectDrawCreateEx starts with %s, not the hot-patch prologue" % data[start:start + 5].hex())

    imports = []
    o = off(imp_rva)
    while struct.unpack_from("<I", data, o + 12)[0]:
        imports.append(cstr(struct.unpack_from("<I", data, o + 12)[0]).lower())
        o += 20
    bad = [d for d in imports if d not in ALLOWED_IMPORTS]
    if bad:
        fail("unexpected imports: " + ", ".join(bad))
    print("OK: %s: %d exports, hot-patchable DirectDrawCreateEx, imports %s" % (path, len(exports), " ".join(imports)))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/ddraw.dll")
```

Run: `python3 tools/check_dll.py`
Expected: `FAIL: build-win/ddraw.dll not found`.

- [ ] **Step 2: Ветка сборки DLL**

В `CMakeLists.txt` заменить блок `if(NOT WIN32) … endif()` на:
```cmake
if(WIN32)
    # ddraw.dll that uaRO.exe loads from its own folder instead of dgVoodoo's or the system one.
    file(GLOB_RECURSE UAGFX_WIN_SOURCES CONFIGURE_DEPENDS src/win/*.cpp)
    add_library(ddraw SHARED ${UAGFX_WIN_SOURCES} src/win/ddraw.def)
    set_target_properties(ddraw PROPERTIES PREFIX "" OUTPUT_NAME "ddraw")
    target_compile_definitions(ddraw PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(ddraw PRIVATE -Wall -Wextra)
    target_link_libraries(ddraw PRIVATE uagfx_core)
    target_link_options(ddraw PRIVATE -static -static-libgcc -static-libstdc++ -s)
else()
    enable_testing()
    file(GLOB UAGFX_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(uagfx_tests ${UAGFX_TEST_SOURCES})
    target_include_directories(uagfx_tests PRIVATE tests)
    target_link_libraries(uagfx_tests PRIVATE uagfx_core)
    add_test(NAME uagfx_tests COMMAND uagfx_tests)
endif()
```

- [ ] **Step 3: Лог и пути**

`src/win/log.hpp`:
```cpp
#pragma once
#include <string>

#include "core/config.hpp"

namespace uagfx::wlog {

// Opens the log once; when the file cannot be created the driver keeps working without a log.
void open(const std::string& path, LogLevel level);
bool enabled(LogLevel level);
void line(LogLevel level, const char* fmt, ...);

}  // namespace uagfx::wlog

#define LOG_ERROR(...) ::uagfx::wlog::line(::uagfx::LogLevel::Error, __VA_ARGS__)
#define LOG_INFO(...) ::uagfx::wlog::line(::uagfx::LogLevel::Info, __VA_ARGS__)
#define LOG_DEBUG(...) ::uagfx::wlog::line(::uagfx::LogLevel::Debug, __VA_ARGS__)
```

`src/win/log.cpp`:
```cpp
#include "win/log.hpp"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

namespace uagfx::wlog {
namespace {
FILE* g_file = nullptr;
LogLevel g_level = LogLevel::Info;
CRITICAL_SECTION g_lock;
}  // namespace

void open(const std::string& path, LogLevel level) {
    if (g_file) return;
    InitializeCriticalSection(&g_lock);
    g_level = level;
    g_file = std::fopen(path.c_str(), "w");
}

bool enabled(LogLevel level) { return g_file && int(level) <= int(g_level); }

void line(LogLevel level, const char* fmt, ...) {
    if (!enabled(level)) return;
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    SYSTEMTIME t;
    GetLocalTime(&t);
    static const char* const kNames[] = {"ERROR", "INFO", "DEBUG"};
    EnterCriticalSection(&g_lock);
    std::fprintf(g_file, "%02d:%02d:%02d.%03d %-5s %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
                 kNames[int(level)], msg);
    std::fflush(g_file);
    LeaveCriticalSection(&g_lock);
}

}  // namespace uagfx::wlog
```

`src/win/paths.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <string>

namespace uagfx::win {

std::string gameDir();               // folder of the game's exe, no trailing backslash
std::string systemDir();             // System32; SysWOW64 for our 32-bit process on 64-bit Windows
std::string modulePath(HMODULE m);   // "" when m is null
bool fileExists(const std::string& path);
bool readTextFile(const std::string& path, std::string& out);

}  // namespace uagfx::win
```

`src/win/paths.cpp`:
```cpp
#include "win/paths.hpp"

#include <fstream>
#include <sstream>

namespace uagfx::win {

std::string modulePath(HMODULE m) {
    if (!m) return "";
    char path[MAX_PATH];
    const DWORD n = GetModuleFileNameA(m, path, MAX_PATH);
    return n && n < MAX_PATH ? std::string(path, n) : "";
}

std::string gameDir() {
    const std::string exe = modulePath(GetModuleHandleA(nullptr));
    const size_t slash = exe.find_last_of("\\/");
    return slash == std::string::npos ? "." : exe.substr(0, slash);
}

std::string systemDir() {
    char dir[MAX_PATH];
    const UINT n = GetSystemDirectoryA(dir, MAX_PATH);
    return n && n < MAX_PATH ? std::string(dir, n) : "C:\\Windows\\System32";
}

bool fileExists(const std::string& path) {
    const DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool readTextFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream s;
    s << f.rdbuf();
    out = s.str();
    return true;
}

}  // namespace uagfx::win
```

- [ ] **Step 4: Настоящий DirectDraw (пока — системный) и экспорты**

`src/win/real_ddraw.hpp`:
```cpp
#pragma once
#include <windows.h>

namespace uagfx::win {

// The DirectDraw every call is passed to in this version. Loaded on the first call of any export,
// never in DllMain (no LoadLibrary under the loader lock).
HMODULE realModule();
FARPROC realProc(const char* name);  // nullptr when the module or the function is missing

}  // namespace uagfx::win
```

`src/win/real_ddraw.cpp`:
```cpp
#include "win/real_ddraw.hpp"

#include <string>

#include "core/version.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win {
namespace {

HMODULE loadReal() {
    const std::string dir = gameDir();
    wlog::open(dir + "\\uaro-graphics.log", LogLevel::Info);
    LOG_INFO("uaRO Graphics %s in %s", kVersion, modulePath(GetModuleHandleA(nullptr)).c_str());
    const std::string path = systemDir() + "\\ddraw.dll";
    HMODULE m = LoadLibraryA(path.c_str());
    if (m) {
        LOG_INFO("passing DirectDraw to %s", path.c_str());
    } else {
        LOG_ERROR("cannot load %s (error %lu)", path.c_str(), GetLastError());
    }
    return m;
}

}  // namespace

HMODULE realModule() {
    static HMODULE m = loadReal();
    return m;
}

FARPROC realProc(const char* name) {
    HMODULE m = realModule();
    return m ? GetProcAddress(m, name) : nullptr;
}

}  // namespace uagfx::win
```

`src/win/exports.cpp`:
```cpp
#include <windows.h>

#include <ddraw.h>

#include "win/log.hpp"
#include "win/real_ddraw.hpp"

namespace {

template <typename Fn>
Fn real(const char* name) {
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(uagfx::win::realProc(name)));
}

}  // namespace

// The only DDRAW.dll import of uaRO.exe. ROExt inline-hooks it as soon as the game starts, so it begins
// with the hot-patch prologue (mov edi,edi / push ebp / mov ebp,esp) that hooks expect.
extern "C" __attribute__((ms_hook_prologue)) HRESULT WINAPI Proxy_DirectDrawCreateEx(GUID* guid, LPVOID* out,
                                                                                    REFIID iid, IUnknown* outer) {
    using Fn = HRESULT(WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
    const Fn fn = real<Fn>("DirectDrawCreateEx");
    if (!fn) return E_FAIL;
    const HRESULT hr = fn(guid, out, iid, outer);
    LOG_INFO("DirectDrawCreateEx = 0x%08lx", static_cast<unsigned long>(hr));
    return hr;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawCreate(GUID* guid, LPDIRECTDRAW* out, IUnknown* outer) {
    using Fn = HRESULT(WINAPI*)(GUID*, LPDIRECTDRAW*, IUnknown*);
    const Fn fn = real<Fn>("DirectDrawCreate");
    return fn ? fn(guid, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawCreateClipper(DWORD flags, LPDIRECTDRAWCLIPPER* out, IUnknown* outer) {
    using Fn = HRESULT(WINAPI*)(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*);
    const Fn fn = real<Fn>("DirectDrawCreateClipper");
    return fn ? fn(flags, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawEnumerateA(LPDDENUMCALLBACKA cb, LPVOID ctx) {
    using Fn = HRESULT(WINAPI*)(LPDDENUMCALLBACKA, LPVOID);
    const Fn fn = real<Fn>("DirectDrawEnumerateA");
    return fn ? fn(cb, ctx) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawEnumerateW(LPDDENUMCALLBACKW cb, LPVOID ctx) {
    using Fn = HRESULT(WINAPI*)(LPDDENUMCALLBACKW, LPVOID);
    const Fn fn = real<Fn>("DirectDrawEnumerateW");
    return fn ? fn(cb, ctx) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawEnumerateExA(LPDDENUMCALLBACKEXA cb, LPVOID ctx, DWORD flags) {
    using Fn = HRESULT(WINAPI*)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);
    const Fn fn = real<Fn>("DirectDrawEnumerateExA");
    return fn ? fn(cb, ctx, flags) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DirectDrawEnumerateExW(LPDDENUMCALLBACKEXW cb, LPVOID ctx, DWORD flags) {
    using Fn = HRESULT(WINAPI*)(LPDDENUMCALLBACKEXW, LPVOID, DWORD);
    const Fn fn = real<Fn>("DirectDrawEnumerateExW");
    return fn ? fn(cb, ctx, flags) : E_FAIL;
}

extern "C" HRESULT WINAPI Proxy_DllCanUnloadNow() {
    using Fn = HRESULT(WINAPI*)();
    const Fn fn = real<Fn>("DllCanUnloadNow");
    return fn ? fn() : S_FALSE;
}

extern "C" HRESULT WINAPI Proxy_DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* out) {
    using Fn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
    const Fn fn = real<Fn>("DllGetClassObject");
    return fn ? fn(clsid, iid, out) : CLASS_E_CLASSNOTAVAILABLE;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(inst);
    return TRUE;
}
```

`src/win/ddraw.def`:
```
LIBRARY ddraw.dll
EXPORTS
    DirectDrawCreate = Proxy_DirectDrawCreate@12
    DirectDrawCreateClipper = Proxy_DirectDrawCreateClipper@12
    DirectDrawCreateEx = Proxy_DirectDrawCreateEx@16
    DirectDrawEnumerateA = Proxy_DirectDrawEnumerateA@8
    DirectDrawEnumerateExA = Proxy_DirectDrawEnumerateExA@12
    DirectDrawEnumerateExW = Proxy_DirectDrawEnumerateExW@12
    DirectDrawEnumerateW = Proxy_DirectDrawEnumerateW@8
    DllCanUnloadNow = Proxy_DllCanUnloadNow@0 PRIVATE
    DllGetClassObject = Proxy_DllGetClassObject@12 PRIVATE
```

- [ ] **Step 5: Собрать и проверить**

Run:
```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j
python3 tools/check_dll.py
```
Expected: сборка без предупреждений; `OK: build-win/ddraw.dll: 9 exports, hot-patchable DirectDrawCreateEx, imports kernel32.dll msvcrt.dll`
(порядок и набор импортов — только из разрешённых). Хост-тесты не сломаны: `cmake --build build-host -j && ./build-host/uagfx_tests` → `0 failed checks`.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/win tools/check_dll.py
git commit -m "ddraw.dll 0.1.0: экспорты, пролог горячего патча для ROExt, передача в системный DirectDraw, лог

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Проверка системы, выбор рендера, загрузка dgVoodoo из подпапки

**Files:**
- Create: `src/win/system_probe.hpp`, `src/win/system_probe.cpp`
- Modify: `src/win/real_ddraw.cpp` (функция `loadReal` целиком), `src/win/exports.cpp` (`DllMain`)

**Interfaces:**
- Consumes: `parseConfig`, `chooseRenderer`, `rendererName`, `backendToken`, `displayModeToken` (Tasks 2–3);
  `gameDir`, `systemDir`, `modulePath`, `fileExists`, `readTextFile`, `wlog::*` (Task 4).
- Produces:
  ```cpp
  namespace uagfx::win {
  struct WindowsVersion { unsigned long major = 0, minor = 0, build = 0; };
  WindowsVersion windowsVersion();
  struct D3D12Probe { bool ok = false; long hr = 0; std::string adapter; unsigned vendor = 0, device = 0;
                      unsigned long long vramMb = 0; };
  D3D12Probe probeD3D12();
  void logModulesAtExit();  // called from DllMain on DLL_PROCESS_DETACH
  }
  ```

- [ ] **Step 1: Проверки системы**

`src/win/system_probe.hpp`:
```cpp
#pragma once
#include <string>

namespace uagfx::win {

struct WindowsVersion {
    unsigned long major = 0;
    unsigned long minor = 0;
    unsigned long build = 0;
};
// The real version: GetVersionEx reports Windows 8 to programs without a manifest.
WindowsVersion windowsVersion();

struct D3D12Probe {
    bool ok = false;  // a feature level 11_0 device was created on the main adapter (and released)
    long hr = 0;      // HRESULT of the failing step
    std::string adapter;
    unsigned vendor = 0;
    unsigned device = 0;
    unsigned long long vramMb = 0;
};
// d3d12.dll and dxgi.dll are loaded at run time and stay loaded for the renderer.
D3D12Probe probeD3D12();

}  // namespace uagfx::win
```

`src/win/system_probe.cpp`:
```cpp
#include "win/system_probe.hpp"

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>

namespace uagfx::win {
namespace {

std::string utf8(const wchar_t* w) {
    char out[256];
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, sizeof out, nullptr, nullptr);
    return n > 0 ? std::string(out) : std::string();
}

}  // namespace

WindowsVersion windowsVersion() {
    WindowsVersion v;
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    const auto fn = ntdll ? reinterpret_cast<RtlGetVersionFn>(
                                reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")))
                          : nullptr;
    OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof info;
    if (fn && fn(&info) == 0) {
        v.major = info.dwMajorVersion;
        v.minor = info.dwMinorVersion;
        v.build = info.dwBuildNumber;
    }
    return v;
}

D3D12Probe probeD3D12() {
    D3D12Probe p;
    HMODULE d3d12 = LoadLibraryA("d3d12.dll");
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    if (!d3d12 || !dxgi) {
        p.hr = HRESULT_FROM_WIN32(GetLastError());
        return p;
    }
    const auto createDevice = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
        reinterpret_cast<void*>(GetProcAddress(d3d12, "D3D12CreateDevice")));
    using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    const auto createFactory =
        reinterpret_cast<CreateFactoryFn>(reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory1")));
    if (!createDevice || !createFactory) {
        p.hr = E_NOINTERFACE;
        return p;
    }
    IDXGIFactory1* factory = nullptr;
    p.hr = createFactory(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
    if (FAILED(p.hr)) return p;
    IDXGIAdapter1* adapter = nullptr;  // adapter 0 drives the main monitor
    p.hr = factory->EnumAdapters1(0, &adapter);
    if (SUCCEEDED(p.hr)) {
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            p.adapter = utf8(desc.Description);
            p.vendor = desc.VendorId;
            p.device = desc.DeviceId;
            p.vramMb = desc.DedicatedVideoMemory / (1024 * 1024);
        }
        ID3D12Device* device = nullptr;
        p.hr = createDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                            reinterpret_cast<void**>(&device));
        if (SUCCEEDED(p.hr)) {
            p.ok = true;
            device->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return p;
}

}  // namespace uagfx::win
```

- [ ] **Step 2: Выбор рендера и загрузка dgVoodoo**

В `src/win/real_ddraw.cpp` заменить всё содержимое на:
```cpp
#include "win/real_ddraw.hpp"

#include <string>

#include "core/config.hpp"
#include "core/renderer_choice.hpp"
#include "core/version.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/system_probe.hpp"

namespace uagfx::win {
namespace {

// 0.1.x has no uaRO renderer yet: everything goes to dgVoodoo or the system DirectDraw.
constexpr bool kUaroBuilt = false;

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

HMODULE loadReal() {
    const std::string dir = gameDir();
    std::string ini;
    const bool haveIni = readTextFile(dir + "\\uaro-graphics.ini", ini);
    const Config config = parseConfig(ini);
    wlog::open(dir + "\\uaro-graphics.log", config.logLevel);
    LOG_INFO("uaRO Graphics %s in %s", kVersion, modulePath(GetModuleHandleA(nullptr)).c_str());
    LOG_INFO("settings: %s; backend = %s, mode = %s", haveIni ? "uaro-graphics.ini" : "no uaro-graphics.ini, defaults",
             backendToken(config.backend), displayModeToken(config.mode));
    const WindowsVersion v = windowsVersion();
    LOG_INFO("Windows %lu.%lu build %lu", v.major, v.minor, v.build);
    logModule("dinput.dll");   // ROExt or uaRO Pad
    logModule("dinput8.dll");

    SystemFacts facts;
    facts.windows10 = v.major >= 10;
    facts.dgvoodoo = fileExists(dir + "\\dgvoodoo\\DDraw.dll");
    if (config.backend == Backend::Uaro && facts.windows10) {
        const D3D12Probe p = probeD3D12();
        facts.d3d12 = p.ok;
        LOG_INFO("Direct3D 12: %s, adapter %s (vendor 0x%04x, device 0x%04x, %llu MB), hr 0x%08lx",
                 p.ok ? "OK" : "FAILED", p.adapter.c_str(), p.vendor, p.device, p.vramMb,
                 static_cast<unsigned long>(p.hr));
    }
    const RendererChoice choice = chooseRenderer(config, facts, kUaroBuilt);
    LOG_INFO("renderer: %s (%s)", rendererName(choice.renderer), choice.reason.c_str());

    HMODULE m = nullptr;
    if (choice.renderer == Renderer::DgVoodoo) m = loadDgVoodoo(dir);
    if (!m) m = loadSystem();
    if (m) LOG_INFO("passing DirectDraw to %s", modulePath(m).c_str());
    return m;
}

}  // namespace

HMODULE realModule() {
    static HMODULE m = loadReal();
    return m;
}

FARPROC realProc(const char* name) {
    HMODULE m = realModule();
    return m ? GetProcAddress(m, name) : nullptr;
}

void logModulesAtExit() {
    logModule("D3DImm.dll");  // which D3DImm.dll dgVoodoo's DirectDraw picked up
    logModule("ddraw.dll");
}

}  // namespace uagfx::win
```

В `src/win/real_ddraw.hpp` добавить объявление после `realProc`:
```cpp
// Called from DllMain on DLL_PROCESS_DETACH: which DirectDraw modules the game ended up with.
void logModulesAtExit();
```

В `src/win/exports.cpp` заменить `DllMain` на:
```cpp
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(inst);
    if (reason == DLL_PROCESS_DETACH) uagfx::win::logModulesAtExit();
    return TRUE;
}
```

- [ ] **Step 3: Собрать и проверить**

Run:
```bash
cmake --build build-win -j && python3 tools/check_dll.py
cmake --build build-host -j && ./build-host/uagfx_tests
```
Expected: сборка без предупреждений; `OK: … imports kernel32.dll msvcrt.dll` (без `d3d12`/`dxgi`: они грузятся
динамически); хост-тесты `0 failed checks`.

- [ ] **Step 4: Commit**

```bash
git add src/win
git commit -m "Выбор рендера при запуске: проверка Windows и D3D12, dgVoodoo из подпапки, откат на системный DirectDraw

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Пакет 0.1.0 и чек-лист проверки для пользователя

**Files:**
- Create: `tools/package.sh`, `dist/TEST.ru.md`

**Interfaces:**
- Consumes: `build-win/ddraw.dll`, `tools/check_dll.py`, `dist/uaro-graphics.ini`.
- Produces: `build-win/uaro-graphics-0.1.0.zip` (`ddraw.dll`, `uaro-graphics.ini`, `TEST.ru.md`).

- [ ] **Step 1: Чек-лист для проверки**

`dist/TEST.ru.md`:
```markdown
# uaRO Graphics 0.1.0 — проверка

Эта версия ещё ничего не рисует сама: она встаёт на место dgVoodoo и передаёт ему всё как есть. Проверяем, что
игра запускается через наш ddraw.dll, ROExt и геймпад не ломаются, а dgVoodoo работает из подпапки.

## Установка (в копию папки клиента)

1. Сделай копию папки клиента, оригинал не трогай.
2. В копии создай папку `dgvoodoo` и **перенеси** в неё `DDraw.dll` и `D3DImm.dll`. Туда же **скопируй**
   `dgVoodoo.conf` (оригинал оставь на месте).
3. Положи рядом с `uaRO.exe` файлы `ddraw.dll` и `uaro-graphics.ini` из архива.

## Проверка 1: всё как раньше

Запусти игру, зайди персонажем, походи, открой окна, Alt+Tab туда и обратно, выйди из игры.

- Игра выглядит и работает как раньше (тот же dgVoodoo).
- ROExt работает как раньше (окно, мышь); с геймпадом — геймпад тоже.
- В папке появился `uaro-graphics.log`. В нём есть строки:
  - `uaRO Graphics 0.1.0 in …\uaRO.exe`
  - `settings: uaro-graphics.ini; backend = dgvoodoo, …`
  - `renderer: dgVoodoo (backend = dgvoodoo)`
  - `passing DirectDraw to …\dgvoodoo\DDraw.dll`
  - `DirectDrawCreateEx = 0x00000000`
  - в конце — `D3DImm.dll: …` (из какой папки dgVoodoo взял D3DImm.dll)

## Проверка 2: что видит наш драйвер

В `uaro-graphics.ini` поставь `backend = uaro`, запусти игру ещё раз. Игра снова должна пойти через dgVoodoo, а в
логе появятся строки:
- `Direct3D 12: OK, adapter NVIDIA GeForce RTX 5060 Ti …`
- `renderer: dgVoodoo (backend = uaro, but the uaRO renderer is not in this version)`

## Проверка 3: без dgVoodoo

Временно переименуй папку `dgvoodoo` и запусти игру: она должна пойти через системный DirectDraw (может выглядеть
хуже — это нормально), в логе `renderer: system DirectDraw (…; dgvoodoo\DDraw.dll not found)`. Верни имя папки.

## Проверка 4: без прав записи (по желанию)

Если копия лежит в `Program Files` и игра запускается без прав администратора, лог может не создаться — игра всё
равно должна запуститься.

## Что прислать

`uaro-graphics.log` после проверок 1 и 2 и коротко: всё ли работало как раньше.
```

- [ ] **Step 2: Скрипт упаковки**

`tools/package.sh`:
```bash
#!/usr/bin/env bash
# Builds the Windows DLL, checks it and packs build-win/uaro-graphics-<version>.zip.
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(sed -n 's/.*kVersion = "\(.*\)".*/\1/p' src/core/version.cpp)
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-win -j >/dev/null
python3 tools/check_dll.py build-win/ddraw.dll
out="build-win/uaro-graphics-$version.zip"
rm -f "$out"
zip -q -j "$out" build-win/ddraw.dll dist/uaro-graphics.ini dist/TEST.ru.md
echo "packed $out"
```

- [ ] **Step 3: Пакет собирается**

Run: `chmod +x tools/package.sh && tools/package.sh && unzip -l build-win/uaro-graphics-0.1.0.zip`
Expected: `OK: build-win/ddraw.dll: …`, `packed build-win/uaro-graphics-0.1.0.zip`; в архиве три файла:
`ddraw.dll`, `uaro-graphics.ini`, `TEST.ru.md`.

- [ ] **Step 4: Commit**

```bash
git add tools/package.sh dist/TEST.ru.md
git commit -m "Пакет 0.1.0 и чек-лист проверки: сквозной режим через dgVoodoo из подпапки

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

После этого шага архив уходит пользователю на проверку; задачи 7–8 продолжаются параллельно.

---

### Task 7: Форматы пикселей

**Files:**
- Create: `src/core/pixel.hpp`, `src/core/pixel.cpp`
- Test: `tests/test_pixel.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace uagfx {
  enum class PixelFormat : uint8_t { Unknown, R5G6B5, X1R5G5B5, A1R5G5B5, A4R4G4B4, X8R8G8B8, A8R8G8B8 };
  struct Masks { uint32_t bits, r, g, b, a; };
  int bytesPerPixel(PixelFormat f);
  Masks masksOf(PixelFormat f);
  PixelFormat formatFromMasks(uint32_t bits, uint32_t r, uint32_t g, uint32_t b, uint32_t a);
  uint32_t toArgb8(PixelFormat f, uint32_t raw);    // 0xAARRGGBB, alpha 0xFF for formats without alpha
  uint32_t fromArgb8(PixelFormat f, uint32_t argb);
  uint32_t readPixel(PixelFormat f, const uint8_t* p);   // little-endian raw value
  void writePixel(PixelFormat f, uint8_t* p, uint32_t raw);
  void rowToArgb8(PixelFormat f, const uint8_t* src, uint32_t* dst, int count);
  void rowFromArgb8(PixelFormat f, const uint32_t* src, uint8_t* dst, int count);
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_pixel.cpp`:
```cpp
#include <cstdint>

#include "core/pixel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
const PixelFormat kAll[] = {PixelFormat::R5G6B5, PixelFormat::X1R5G5B5, PixelFormat::A1R5G5B5,
                            PixelFormat::A4R4G4B4, PixelFormat::X8R8G8B8, PixelFormat::A8R8G8B8};
}

TEST(pixel_sizes) {
    CHECK_EQ(bytesPerPixel(PixelFormat::R5G6B5), 2);
    CHECK_EQ(bytesPerPixel(PixelFormat::A4R4G4B4), 2);
    CHECK_EQ(bytesPerPixel(PixelFormat::A8R8G8B8), 4);
    CHECK_EQ(bytesPerPixel(PixelFormat::Unknown), 0);
}

TEST(pixel_masks_round_trip) {
    for (PixelFormat f : kAll) {
        const Masks m = masksOf(f);
        CHECK(formatFromMasks(m.bits, m.r, m.g, m.b, m.a) == f);
    }
    CHECK(formatFromMasks(16, 0xF800, 0x07E0, 0x001F, 0x0001) == PixelFormat::Unknown);
    CHECK(formatFromMasks(24, 0xFF0000, 0xFF00, 0xFF, 0) == PixelFormat::Unknown);
}

TEST(pixel_expansion_reaches_full_range) {
    CHECK_EQ(toArgb8(PixelFormat::R5G6B5, 0xFFFF), 0xFFFFFFFFu);
    CHECK_EQ(toArgb8(PixelFormat::R5G6B5, 0x0000), 0xFF000000u);
    CHECK_EQ(toArgb8(PixelFormat::R5G6B5, 0xF800), 0xFFFF0000u);
    CHECK_EQ(toArgb8(PixelFormat::R5G6B5, 0x07E0), 0xFF00FF00u);
    CHECK_EQ(toArgb8(PixelFormat::X1R5G5B5, 0x7FFF), 0xFFFFFFFFu);
}

TEST(pixel_alpha_formats) {
    CHECK_EQ(toArgb8(PixelFormat::A1R5G5B5, 0x7C00), 0x00FF0000u);  // red, transparent (magenta turned into alpha)
    CHECK_EQ(toArgb8(PixelFormat::A1R5G5B5, 0xFC00), 0xFFFF0000u);
    CHECK_EQ(toArgb8(PixelFormat::A4R4G4B4, 0xF0F0), 0xFF00FF00u);
    CHECK_EQ(toArgb8(PixelFormat::A4R4G4B4, 0x8421), 0x88442211u);
    CHECK_EQ(toArgb8(PixelFormat::X8R8G8B8, 0x00123456), 0xFF123456u);
    CHECK_EQ(toArgb8(PixelFormat::A8R8G8B8, 0x80123456), 0x80123456u);
}

TEST(pixel_16bit_round_trip_is_lossless) {
    for (uint32_t v = 0; v < 0x10000; ++v) {
        CHECK_EQ(fromArgb8(PixelFormat::R5G6B5, toArgb8(PixelFormat::R5G6B5, v)), v);
        CHECK_EQ(fromArgb8(PixelFormat::A1R5G5B5, toArgb8(PixelFormat::A1R5G5B5, v)), v);
        CHECK_EQ(fromArgb8(PixelFormat::A4R4G4B4, toArgb8(PixelFormat::A4R4G4B4, v)), v);
        CHECK_EQ(fromArgb8(PixelFormat::X1R5G5B5, toArgb8(PixelFormat::X1R5G5B5, v)), v & 0x7FFF);
    }
}

TEST(pixel_rows_are_little_endian) {
    const uint8_t src[4] = {0x00, 0xF8, 0xE0, 0x07};  // R5G6B5: red, green
    uint32_t argb[2] = {};
    rowToArgb8(PixelFormat::R5G6B5, src, argb, 2);
    CHECK_EQ(argb[0], 0xFFFF0000u);
    CHECK_EQ(argb[1], 0xFF00FF00u);
    uint8_t back[4] = {};
    rowFromArgb8(PixelFormat::R5G6B5, argb, back, 2);
    CHECK(back[0] == 0x00 && back[1] == 0xF8 && back[2] == 0xE0 && back[3] == 0x07);
    uint8_t p[4] = {};
    writePixel(PixelFormat::A8R8G8B8, p, 0x11223344);
    CHECK(p[0] == 0x44 && p[3] == 0x11);
    CHECK_EQ(readPixel(PixelFormat::A8R8G8B8, p), 0x11223344u);
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/pixel.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/pixel.hpp`:
```cpp
#pragma once
#include <cstdint>

namespace uagfx {

// The pixel formats of DirectDraw surfaces the client can see. Textures are A1R5G5B5 or A4R4G4B4;
// the screen is R5G6B5, X1R5G5B5 or X8R8G8B8.
enum class PixelFormat : uint8_t { Unknown, R5G6B5, X1R5G5B5, A1R5G5B5, A4R4G4B4, X8R8G8B8, A8R8G8B8 };

struct Masks {
    uint32_t bits;
    uint32_t r, g, b, a;
};

int bytesPerPixel(PixelFormat f);
Masks masksOf(PixelFormat f);
PixelFormat formatFromMasks(uint32_t bits, uint32_t r, uint32_t g, uint32_t b, uint32_t a);

// One pixel: the raw value in format f <-> 0xAARRGGBB with 8-bit channels (alpha 0xFF when f has no alpha).
// Expansion repeats the high bits (5-bit 31 -> 255), so 16-bit values survive the round trip unchanged.
uint32_t toArgb8(PixelFormat f, uint32_t raw);
uint32_t fromArgb8(PixelFormat f, uint32_t argb);

uint32_t readPixel(PixelFormat f, const uint8_t* p);  // little-endian
void writePixel(PixelFormat f, uint8_t* p, uint32_t raw);
void rowToArgb8(PixelFormat f, const uint8_t* src, uint32_t* dst, int count);
void rowFromArgb8(PixelFormat f, const uint32_t* src, uint8_t* dst, int count);

}  // namespace uagfx
```

`src/core/pixel.cpp`:
```cpp
#include "core/pixel.hpp"

namespace uagfx {
namespace {

// v has `bits` bits -> 8 bits; 0 stays 0 and the maximum becomes 255.
uint32_t expand(uint32_t v, int bits) {
    switch (bits) {
        case 1: return v ? 255u : 0u;
        case 4: return v * 17u;
        case 5: return (v << 3) | (v >> 2);
        case 6: return (v << 2) | (v >> 4);
        default: return v;
    }
}

}  // namespace

int bytesPerPixel(PixelFormat f) {
    switch (f) {
        case PixelFormat::R5G6B5:
        case PixelFormat::X1R5G5B5:
        case PixelFormat::A1R5G5B5:
        case PixelFormat::A4R4G4B4: return 2;
        case PixelFormat::X8R8G8B8:
        case PixelFormat::A8R8G8B8: return 4;
        case PixelFormat::Unknown: break;
    }
    return 0;
}

Masks masksOf(PixelFormat f) {
    switch (f) {
        case PixelFormat::R5G6B5: return {16, 0xF800, 0x07E0, 0x001F, 0};
        case PixelFormat::X1R5G5B5: return {16, 0x7C00, 0x03E0, 0x001F, 0};
        case PixelFormat::A1R5G5B5: return {16, 0x7C00, 0x03E0, 0x001F, 0x8000};
        case PixelFormat::A4R4G4B4: return {16, 0x0F00, 0x00F0, 0x000F, 0xF000};
        case PixelFormat::X8R8G8B8: return {32, 0xFF0000, 0xFF00, 0xFF, 0};
        case PixelFormat::A8R8G8B8: return {32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000};
        case PixelFormat::Unknown: break;
    }
    return {0, 0, 0, 0, 0};
}

PixelFormat formatFromMasks(uint32_t bits, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    const PixelFormat all[] = {PixelFormat::R5G6B5,   PixelFormat::X1R5G5B5, PixelFormat::A1R5G5B5,
                               PixelFormat::A4R4G4B4, PixelFormat::X8R8G8B8, PixelFormat::A8R8G8B8};
    for (PixelFormat f : all) {
        const Masks m = masksOf(f);
        if (m.bits == bits && m.r == r && m.g == g && m.b == b && m.a == a) return f;
    }
    return PixelFormat::Unknown;
}

uint32_t toArgb8(PixelFormat f, uint32_t raw) {
    switch (f) {
        case PixelFormat::R5G6B5:
            return 0xFF000000u | expand((raw >> 11) & 31, 5) << 16 | expand((raw >> 5) & 63, 6) << 8 |
                   expand(raw & 31, 5);
        case PixelFormat::X1R5G5B5:
            return 0xFF000000u | expand((raw >> 10) & 31, 5) << 16 | expand((raw >> 5) & 31, 5) << 8 |
                   expand(raw & 31, 5);
        case PixelFormat::A1R5G5B5:
            return expand((raw >> 15) & 1, 1) << 24 | expand((raw >> 10) & 31, 5) << 16 |
                   expand((raw >> 5) & 31, 5) << 8 | expand(raw & 31, 5);
        case PixelFormat::A4R4G4B4:
            return expand((raw >> 12) & 15, 4) << 24 | expand((raw >> 8) & 15, 4) << 16 |
                   expand((raw >> 4) & 15, 4) << 8 | expand(raw & 15, 4);
        case PixelFormat::X8R8G8B8: return 0xFF000000u | (raw & 0xFFFFFFu);
        case PixelFormat::A8R8G8B8: return raw;
        case PixelFormat::Unknown: break;
    }
    return 0;
}

uint32_t fromArgb8(PixelFormat f, uint32_t argb) {
    const uint32_t a = argb >> 24;
    const uint32_t r = (argb >> 16) & 255;
    const uint32_t g = (argb >> 8) & 255;
    const uint32_t b = argb & 255;
    switch (f) {
        case PixelFormat::R5G6B5: return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
        case PixelFormat::X1R5G5B5: return (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);
        case PixelFormat::A1R5G5B5: return (a >= 128 ? 0x8000u : 0u) | (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);
        case PixelFormat::A4R4G4B4: return (a >> 4) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4);
        case PixelFormat::X8R8G8B8: return argb & 0xFFFFFFu;
        case PixelFormat::A8R8G8B8: return argb;
        case PixelFormat::Unknown: break;
    }
    return 0;
}

uint32_t readPixel(PixelFormat f, const uint8_t* p) {
    uint32_t v = 0;
    for (int i = bytesPerPixel(f) - 1; i >= 0; --i) v = v << 8 | p[i];
    return v;
}

void writePixel(PixelFormat f, uint8_t* p, uint32_t raw) {
    const int n = bytesPerPixel(f);
    for (int i = 0; i < n; ++i) p[i] = uint8_t(raw >> (8 * i));
}

void rowToArgb8(PixelFormat f, const uint8_t* src, uint32_t* dst, int count) {
    const int n = bytesPerPixel(f);
    for (int i = 0; i < count; ++i) dst[i] = toArgb8(f, readPixel(f, src + i * n));
}

void rowFromArgb8(PixelFormat f, const uint32_t* src, uint8_t* dst, int count) {
    const int n = bytesPerPixel(f);
    for (int i = 0; i < count; ++i) writePixel(f, dst + i * n, fromArgb8(f, src[i]));
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests pixel`
Expected: 6 тестов `PASS pixel_…`, `6 tests, 0 failed checks`.

- [ ] **Step 5: Commit**

```bash
git add src/core/pixel.hpp src/core/pixel.cpp tests/test_pixel.cpp
git commit -m "Форматы пикселей клиента: 16 и 32 бита, перевод в ARGB8 без потерь

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Поверхности на CPU: заливка, копирование, цветовой ключ, растягивание

**Files:**
- Create: `src/core/image.hpp`, `src/core/image.cpp`
- Test: `tests/test_image.cpp`

**Interfaces:**
- Consumes: `PixelFormat`, `bytesPerPixel`, `readPixel`, `writePixel`, `toArgb8`, `fromArgb8` (Task 7).
- Produces:
  ```cpp
  namespace uagfx {
  struct Rect { int left = 0, top = 0, right = 0, bottom = 0; int width() const; int height() const;
                bool empty() const; bool operator==(const Rect&) const; };
  bool clipRect(const Rect& bounds, Rect& r);  // r := r ∩ bounds; false when empty
  class Image {
  public:
      Image() = default;
      Image(int width, int height, PixelFormat format);
      int width() const; int height() const; PixelFormat format() const; int pitch() const;
      uint8_t* data(); const uint8_t* data() const; size_t sizeBytes() const;
      uint8_t* row(int y); const uint8_t* row(int y) const;
      uint32_t pixel(int x, int y) const; void setPixel(int x, int y, uint32_t raw);
      Rect bounds() const;
  };
  void fill(Image& dst, Rect r, uint32_t raw);                                          // DDBLT_COLORFILL
  bool copy(Image& dst, int dx, int dy, const Image& src, Rect sr);                     // BltFast / same-size Blt
  bool copyKeyed(Image& dst, int dx, int dy, const Image& src, Rect sr, uint32_t keyLow, uint32_t keyHigh);
  enum class StretchFilter { Point, Linear };
  bool stretch(Image& dst, Rect dr, const Image& src, Rect sr, StretchFilter filter);  // stretching Blt
  }
  ```

- [ ] **Step 1: Написать падающие тесты**

`tests/test_image.cpp`:
```cpp
#include "core/image.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
Image filled(int w, int h, PixelFormat f, uint32_t raw) {
    Image img(w, h, f);
    fill(img, img.bounds(), raw);
    return img;
}
}  // namespace

TEST(image_rows_are_dword_aligned) {
    const Image img(3, 2, PixelFormat::R5G6B5);
    CHECK_EQ(img.pitch(), 8);
    CHECK_EQ(img.sizeBytes(), size_t(16));
    const Image wide(5, 1, PixelFormat::A8R8G8B8);
    CHECK_EQ(wide.pitch(), 20);
}

TEST(image_clip_rect) {
    Rect r{-2, 1, 10, 3};
    CHECK(clipRect(Rect{0, 0, 4, 4}, r));
    CHECK(r == (Rect{0, 1, 4, 3}));
    Rect outside{5, 5, 9, 9};
    CHECK(!clipRect(Rect{0, 0, 4, 4}, outside));
}

TEST(image_fill_is_clipped) {
    Image img(4, 4, PixelFormat::A8R8G8B8);
    fill(img, Rect{2, 2, 10, 10}, 0x11223344);
    CHECK_EQ(img.pixel(2, 2), 0x11223344u);
    CHECK_EQ(img.pixel(3, 3), 0x11223344u);
    CHECK_EQ(img.pixel(1, 2), 0u);
    CHECK_EQ(img.pixel(2, 1), 0u);
}

TEST(image_fill_keeps_the_low_bytes_of_the_value) {
    Image img(1, 1, PixelFormat::A1R5G5B5);
    fill(img, img.bounds(), 0x12345678);  // the client may pass a screen-format colour to a 16-bit texture
    CHECK_EQ(img.pixel(0, 0), 0x5678u);
}

TEST(image_copy_clips_at_the_destination_edge) {
    Image src(2, 2, PixelFormat::R5G6B5);
    src.setPixel(0, 0, 1);
    src.setPixel(1, 0, 2);
    src.setPixel(0, 1, 3);
    src.setPixel(1, 1, 4);
    Image dst(4, 4, PixelFormat::R5G6B5);
    CHECK(copy(dst, -1, 0, src, src.bounds()));
    CHECK_EQ(dst.pixel(0, 0), 2u);
    CHECK_EQ(dst.pixel(0, 1), 4u);
    CHECK_EQ(dst.pixel(1, 0), 0u);
}

TEST(image_copy_needs_the_same_format) {
    const Image src = filled(2, 2, PixelFormat::R5G6B5, 7);
    Image dst(2, 2, PixelFormat::A1R5G5B5);
    CHECK(!copy(dst, 0, 0, src, src.bounds()));
    CHECK_EQ(dst.pixel(0, 0), 0u);
}

TEST(image_copy_within_the_same_image_overlapping) {
    Image img(4, 1, PixelFormat::A8R8G8B8);
    for (int x = 0; x < 4; ++x) img.setPixel(x, 0, uint32_t(x + 1));
    CHECK(copy(img, 1, 0, img, Rect{0, 0, 3, 1}));
    CHECK_EQ(img.pixel(0, 0), 1u);
    CHECK_EQ(img.pixel(1, 0), 1u);
    CHECK_EQ(img.pixel(2, 0), 2u);
    CHECK_EQ(img.pixel(3, 0), 3u);
}

TEST(image_keyed_copy_skips_the_key_colour) {
    Image src(2, 1, PixelFormat::R5G6B5);
    src.setPixel(0, 0, 0xF81F);  // magenta: the client's transparent colour
    src.setPixel(1, 0, 0x001F);
    Image dst = filled(2, 1, PixelFormat::R5G6B5, 0x07E0);
    CHECK(copyKeyed(dst, 0, 0, src, src.bounds(), 0xF81F, 0xF81F));
    CHECK_EQ(dst.pixel(0, 0), 0x07E0u);
    CHECK_EQ(dst.pixel(1, 0), 0x001Fu);
}

TEST(image_stretch_point_doubles_pixels) {
    Image src(2, 1, PixelFormat::A8R8G8B8);
    src.setPixel(0, 0, 0xFF0000FF);
    src.setPixel(1, 0, 0xFFFF0000);
    Image dst(4, 1, PixelFormat::A8R8G8B8);
    CHECK(stretch(dst, dst.bounds(), src, src.bounds(), StretchFilter::Point));
    CHECK_EQ(dst.pixel(0, 0), 0xFF0000FFu);
    CHECK_EQ(dst.pixel(1, 0), 0xFF0000FFu);
    CHECK_EQ(dst.pixel(2, 0), 0xFFFF0000u);
    CHECK_EQ(dst.pixel(3, 0), 0xFFFF0000u);
}

TEST(image_stretch_linear_halving_averages_four_pixels) {
    Image src(2, 2, PixelFormat::A8R8G8B8);
    src.setPixel(0, 0, 0xFF000000);
    src.setPixel(1, 0, 0xFF000064);  // blue 100
    src.setPixel(0, 1, 0xFF0000C8);  // blue 200
    src.setPixel(1, 1, 0xFF000064);
    Image dst(1, 1, PixelFormat::A8R8G8B8);
    CHECK(stretch(dst, dst.bounds(), src, src.bounds(), StretchFilter::Linear));
    CHECK_EQ(dst.pixel(0, 0), 0xFF000064u);  // (0 + 100 + 200 + 100) / 4
}

TEST(image_stretch_linear_one_to_one_is_a_copy) {
    Image src(3, 1, PixelFormat::A4R4G4B4);
    src.setPixel(0, 0, 0xF00F);
    src.setPixel(1, 0, 0x8421);
    src.setPixel(2, 0, 0x0FF0);
    Image dst(3, 1, PixelFormat::A4R4G4B4);
    CHECK(stretch(dst, dst.bounds(), src, src.bounds(), StretchFilter::Linear));
    for (int x = 0; x < 3; ++x) CHECK_EQ(dst.pixel(x, 0), src.pixel(x, 0));
}
```

- [ ] **Step 2: Убедиться, что падает**

Run: `cmake --build build-host -j`
Expected: ошибка компиляции `core/image.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/image.hpp`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/pixel.hpp"

namespace uagfx {

struct Rect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int width() const { return right - left; }
    int height() const { return bottom - top; }
    bool empty() const { return right <= left || bottom <= top; }
    bool operator==(const Rect& o) const {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
};

// r := r intersected with bounds; false when nothing is left.
bool clipRect(const Rect& bounds, Rect& r);

// The CPU copy of a DirectDraw surface: pixels in the client's format, rows aligned to 4 bytes.
class Image {
public:
    Image() = default;
    Image(int width, int height, PixelFormat format);
    int width() const { return w_; }
    int height() const { return h_; }
    PixelFormat format() const { return f_; }
    int pitch() const { return pitch_; }
    uint8_t* data() { return px_.data(); }
    const uint8_t* data() const { return px_.data(); }
    size_t sizeBytes() const { return px_.size(); }
    uint8_t* row(int y) { return px_.data() + size_t(y) * size_t(pitch_); }
    const uint8_t* row(int y) const { return px_.data() + size_t(y) * size_t(pitch_); }
    uint32_t pixel(int x, int y) const { return readPixel(f_, row(y) + x * bytesPerPixel(f_)); }
    void setPixel(int x, int y, uint32_t raw) { writePixel(f_, row(y) + x * bytesPerPixel(f_), raw); }
    Rect bounds() const { return Rect{0, 0, w_, h_}; }

private:
    int w_ = 0;
    int h_ = 0;
    PixelFormat f_ = PixelFormat::Unknown;
    int pitch_ = 0;
    std::vector<uint8_t> px_;
};

// DDBLT_COLORFILL: raw is a value in dst's format; only its low bytes are written.
void fill(Image& dst, Rect r, uint32_t raw);
// BltFast / Blt of the same size. Both images must have the same format; src and dst may be the same image.
bool copy(Image& dst, int dx, int dy, const Image& src, Rect sr);
// The same, skipping source pixels whose raw value lies in [keyLow, keyHigh] (source colour key).
bool copyKeyed(Image& dst, int dx, int dy, const Image& src, Rect sr, uint32_t keyLow, uint32_t keyHigh);

enum class StretchFilter { Point, Linear };
// A stretching Blt between images of the same format (mip-level generation, the Bink video).
bool stretch(Image& dst, Rect dr, const Image& src, Rect sr, StretchFilter filter);

}  // namespace uagfx
```

`src/core/image.cpp`:
```cpp
#include "core/image.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace uagfx {
namespace {

// Moves (dx, dy, sr) so that both the source and the destination rectangles lie inside their images.
bool clipCopy(const Image& dst, int& dx, int& dy, const Image& src, Rect& sr) {
    if (!clipRect(src.bounds(), sr)) return false;
    Rect dr{dx, dy, dx + sr.width(), dy + sr.height()};
    if (!clipRect(dst.bounds(), dr)) return false;
    sr.left += dr.left - dx;
    sr.top += dr.top - dy;
    sr.right = sr.left + dr.width();
    sr.bottom = sr.top + dr.height();
    dx = dr.left;
    dy = dr.top;
    return true;
}

uint32_t lerpArgb(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const float ca = float((a >> shift) & 255);
        const float cb = float((b >> shift) & 255);
        out |= uint32_t(std::lround(ca + (cb - ca) * t)) << shift;
    }
    return out;
}

}  // namespace

bool clipRect(const Rect& bounds, Rect& r) {
    r.left = std::max(r.left, bounds.left);
    r.top = std::max(r.top, bounds.top);
    r.right = std::min(r.right, bounds.right);
    r.bottom = std::min(r.bottom, bounds.bottom);
    return !r.empty();
}

Image::Image(int width, int height, PixelFormat format)
    : w_(width), h_(height), f_(format), pitch_((width * bytesPerPixel(format) + 3) & ~3),
      px_(size_t(pitch_) * size_t(height)) {}

void fill(Image& dst, Rect r, uint32_t raw) {
    if (!clipRect(dst.bounds(), r)) return;
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x) dst.setPixel(x, y, raw);
}

bool copy(Image& dst, int dx, int dy, const Image& src, Rect sr) {
    if (dst.format() != src.format() || dst.format() == PixelFormat::Unknown) return false;
    if (!clipCopy(dst, dx, dy, src, sr)) return true;  // nothing visible: like DirectDraw, not an error
    const int bpp = bytesPerPixel(src.format());
    const size_t bytes = size_t(sr.width()) * size_t(bpp);
    const bool bottomUp = &dst == &src && dy > sr.top;  // overlapping rows: copy from the far end
    for (int i = 0; i < sr.height(); ++i) {
        const int y = bottomUp ? sr.height() - 1 - i : i;
        std::memmove(dst.row(dy + y) + dx * bpp, src.row(sr.top + y) + sr.left * bpp, bytes);
    }
    return true;
}

bool copyKeyed(Image& dst, int dx, int dy, const Image& src, Rect sr, uint32_t keyLow, uint32_t keyHigh) {
    if (dst.format() != src.format() || dst.format() == PixelFormat::Unknown) return false;
    if (!clipCopy(dst, dx, dy, src, sr)) return true;
    for (int y = 0; y < sr.height(); ++y)
        for (int x = 0; x < sr.width(); ++x) {
            const uint32_t v = src.pixel(sr.left + x, sr.top + y);
            if (v < keyLow || v > keyHigh) dst.setPixel(dx + x, dy + y, v);
        }
    return true;
}

bool stretch(Image& dst, Rect dr, const Image& src, Rect sr, StretchFilter filter) {
    if (dst.format() != src.format() || dst.format() == PixelFormat::Unknown) return false;
    if (dr.empty() || sr.empty()) return false;
    const Rect full = dr;  // the mapping is defined by the unclipped rectangles
    if (!clipRect(dst.bounds(), dr)) return true;
    const PixelFormat f = src.format();
    const float sx = float(sr.width()) / float(full.width());
    const float sy = float(sr.height()) / float(full.height());
    for (int y = dr.top; y < dr.bottom; ++y) {
        for (int x = dr.left; x < dr.right; ++x) {
            const float u = (float(x - full.left) + 0.5f) * sx;  // source position of the pixel centre
            const float v = (float(y - full.top) + 0.5f) * sy;
            if (filter == StretchFilter::Point) {
                const int px = std::clamp(sr.left + int(u), sr.left, sr.right - 1);
                const int py = std::clamp(sr.top + int(v), sr.top, sr.bottom - 1);
                dst.setPixel(x, y, src.pixel(px, py));
                continue;
            }
            const float fu = u - 0.5f;
            const float fv = v - 0.5f;
            const int x0 = int(std::floor(fu));
            const int y0 = int(std::floor(fv));
            const float tx = fu - float(x0);
            const float ty = fv - float(y0);
            auto at = [&](int ix, int iy) {
                ix = std::clamp(sr.left + ix, sr.left, sr.right - 1);
                iy = std::clamp(sr.top + iy, sr.top, sr.bottom - 1);
                return toArgb8(f, src.pixel(ix, iy));
            };
            const uint32_t top = lerpArgb(at(x0, y0), at(x0 + 1, y0), tx);
            const uint32_t bottom = lerpArgb(at(x0, y0 + 1), at(x0 + 1, y0 + 1), tx);
            dst.setPixel(x, y, fromArgb8(f, lerpArgb(top, bottom, ty)));
        }
    }
    return true;
}

}  // namespace uagfx
```

- [ ] **Step 4: Тесты проходят**

Run: `cmake --build build-host -j && ./build-host/uagfx_tests`
Expected: все тесты `PASS` (в т. ч. 11 `image_…`), `0 failed checks`. Windows-сборка не сломана:
`cmake --build build-win -j && python3 tools/check_dll.py` → `OK: …`.

- [ ] **Step 5: Commit**

```bash
git add src/core/image.hpp src/core/image.cpp tests/test_image.cpp
git commit -m "Поверхности на CPU: заливка, копирование с обрезкой, цветовой ключ, растягивание

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Что дальше

План 1B (DX7-объекты поверх `Image`, запись кадра, D3D12-рендер в масштабе 1, ленивое чтение кадра, калибровка
на эталонном растеризаторе, сохранение кадров) пишется по справкам `docs/research/surfaces-present-caps.md`,
`docs/research/states.md`, `docs/research/calibration.md` и по результату проверки 0.1.0 у пользователя.

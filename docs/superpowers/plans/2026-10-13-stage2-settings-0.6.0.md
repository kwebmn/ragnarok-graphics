# uaRO Graphics 0.6.0 — утилита «uaRO Graphics Settings», первая часть (план)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `uaro-graphics.exe` — утилита настроек с разделами Рендер, Экран, Качество, Кадры, Диагностика и Сервис, на
трёх языках; разрешение игры без `setup.exe` и без прав администратора; драйвер применяет настройки на лету — выпуск
0.6.0 через `update.bat`.

**Architecture:** вся логика — в переносимом ядре `src/core/settings/` с тестами на Linux: правка ini без потери
комментариев, две копии реестра игры (личная поверх общей), расчёт экрана, распознавание модуля ввода, флажки
совместимости, пресеты, выдержка из лога, отчёт, тексты EN/UA/RU, схема ключей ini. `src/core/live_config` решает, что
драйвер применяет в запущенной игре. Слой Windows `src/app/` (библиотека `uagfx_app`) читает и пишет файлы, реестр,
узнаёт о системе; окно — чистый Win32 со списком разделов слева. Драйвер раз в секунду смотрит на ini.

**Tech Stack:** C++17, MinGW-w64 i686 GCC 12 (`windres` для ресурсов), Win32 + Common Controls 6, DXGI, тесты на Linux.

**Spec:** `docs/superpowers/specs/2026-10-13-stage2-settings-utility-design.md` (выпуск 0.6.0 — §11); разбор реестра
клиента `docs/research/registry.md`; спецификация v1 `2026-10-13-uaro-graphics-design.md`.

## Global Constraints

- Утилита 32-битная, статически (`libgcc`, `libstdc++`), импортирует только системные DLL Windows 7: `KERNEL32`,
  `USER32`, `GDI32`, `COMCTL32`, `SHELL32`, `OLE32`, `ADVAPI32`, `msvcrt`. Всё, чего нет в Windows 7 (`GetDpiForWindow`,
  `GetDpiForMonitor`, `AdjustWindowRectExForDpi`, DXGI, D3D12…), — только через `GetProcAddress` (`tools/check_exe.py`).
- Манифест: `asInvoker`, `PerMonitorV2`, Common Controls 6, `supportedOS` Windows 7–10.
- Утилита пишет реестр игры только в личную копию (`HKCU\Software\Classes\VirtualStore\…\Gravity Soft\Ragnarok`),
  общую (HKLM) — никогда; числа — только `REG_DWORD` (spec §2.1).
- Значения, которые игра пишет сама, и ini ROExt — только при закрытой игре (spec §4.1); ini драйвера — всегда.
- `uaro-graphics.ini` правится без потери комментариев и переводов строк (spec §2.2).
- Перед первой записью за запуск — резервная копия всего, что утилита может изменить (spec §4.6).
- Каждый текст окна — на EN, UA и RU с одинаковыми подстановками (spec §6).
- DLL: всё из прежних планов в силе — импорты только `KERNEL32`, `USER32`, `GDI32`, `msvcrt`; пролог горячего патча.
- Версия — `0.6.0`. Сборка без предупреждений. Стиль и коммиты — как раньше (KWEB Team, русские сообщения, трейлер
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`).

## Review Focus

1. **Полный экран клиента с нашим рендером** (`ISFULLSCREENMODE = 1`) ни разу не запускался на ПК пользователя: окно без
   рамки на весь монитор, мышь попадает, Alt+Tab сворачивает и возвращает игру — пункт 5 чек-листа (Task 12); здесь
   проверить нечем.
2. **Первый запуск без `setup.exe`** (ключа игры нет ни в одной копии): «Применить» создаёт всё нужное в личной копии —
   тест `registry_first_run_has_all_the_game_needs` (Task 3); видит ли игра ключ только из VirtualStore — проверка на
   чистой системе (spec §12).
3. **ini меняется, пока драйвер его читает** (ползунок в запущенной игре): утилита подменяет файл целиком
   (`writeFileAtomic`), драйвер при неудачном чтении берёт его на следующей проверке — вычитка Task 2 и Task 10.
4. **ROExt задаёт окну размер** (`WindowLock` или свой размер): «Сейчас» показывает размер ROExt, «Применить» пишет в
   ini ROExt тот же размер, при запущенной игре — отказ — тесты `screen_roext_gets_the_same_size_in_a_window`,
   `screen_now_reads_what_the_game_will_get`, `input_module_reads_roexts_window` (Tasks 5, 6).
5. **ini после `install.bat`** (повторённые разделы `[quality]`, `[display]`): правка меняет каждую копию ключа, новый
   ключ — в последнюю копию раздела — тест `ini_edit_handles_sections_repeated_by_install_bat` (Task 1).

## Как записаны правки

Как в прежних планах: «`путь`:» — файл целиком; «В `путь` заменить: … на: …» — фрагмент встречается ровно один раз.
`.bat` и `.ini` — CRLF на диске, в блоках — LF.

Команды проверки:
- ядро: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`;
- Windows: `cmake -S . -B build-win --no-warn-unused-cli -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build-win -j 2>&1 | grep -E "warning|error"; python3 tools/check_dll.py build-win/ddraw.dll`;
- задача целиком — скрипт `verify4.sh` (scratch): сборка Windows без предупреждений, `check_dll`, `check_exe` (когда
  он есть), сборка и тесты ядра.

---

### Task 1: Правка ini без потери комментариев — в ядре (spec §2.2)

Приём `applyIniEdits` из uaRO Pad переезжает в ядро: разделы и ключи сравниваются без учёта регистра (ini ROExt Windows
читает так же), повторённые `install.bat` разделы правятся во всех копиях, новый ключ идёт в последнюю копию раздела.
`readIni` даёт значения так, как их видит `parseConfig`. Тестам нужен путь к исходникам — `UAGFX_SOURCE_DIR`.

**Files:**
- Create: `src/core/settings/ini_edit.hpp`, `src/core/settings/ini_edit.cpp`
- Modify: `CMakeLists.txt` (цель тестов)
- Test: `tests/test_ini_edit.cpp`

**Interfaces:**
- Produces: `struct IniEdit { std::string section, key, value; }`; `std::string applyIniEdits(const std::string& text,
  const std::vector<IniEdit>& edits)`; `std::map<std::string, std::string> readIni(const std::string& text)` — ключи
  `"section.key"` в нижнем регистре (namespace `uagfx::settings`).

- [ ] **Step 1: Тесты и путь к исходникам для них**

`tests/test_ini_edit.cpp`:
```cpp
#include <fstream>
#include <sstream>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

TEST(ini_edit_replaces_only_the_value) {
    const std::string text =
        "; uaRO Graphics\r\n[quality]\r\nsharpen = 50   ; 0-100\r\nrender_scale = 100\r\n";
    CHECK(applyIniEdits(text, {{"quality", "sharpen", "80"}}) ==
          "; uaRO Graphics\r\n[quality]\r\nsharpen = 80   ; 0-100\r\nrender_scale = 100\r\n");
    CHECK(applyIniEdits("[ROExt]\nWindowWidth=1600\n", {{"ROExt", "WindowWidth", "1920"}}) ==
          "[ROExt]\nWindowWidth=1920\n");
    CHECK(applyIniEdits("[frames]\nvsync = \n", {{"frames", "vsync", "1"}}) == "[frames]\nvsync = 1\n");
}

TEST(ini_edit_matches_sections_and_keys_without_case) {
    // ROExt's ini is read by Windows, which ignores case; the line keeps its own spelling.
    CHECK(applyIniEdits("[ROExt]\nwindowwidth=1600\n", {{"roext", "WindowWidth", "1920"}}) ==
          "[ROExt]\nwindowwidth=1920\n");
    CHECK(applyIniEdits("[ROExt]\nWindowLock=1\n", {{"roext", "WindowWidth", "1920"}}) ==
          "[ROExt]\nWindowLock=1\nWindowWidth = 1920\n");
}

TEST(ini_edit_adds_a_missing_key_to_its_section) {
    const std::string text = "[display]\nmode = auto\n\n[quality]\nsharpen = 50\n";
    CHECK(applyIniEdits(text, {{"display", "monitor", "1"}, {"display", "crisp_text", "0"}}) ==
          "[display]\nmode = auto\nmonitor = 1\ncrisp_text = 0\n\n[quality]\nsharpen = 50\n");
    CHECK(applyIniEdits("[display]\n\n[quality]\n", {{"display", "monitor", "1"}}) ==
          "[display]\nmonitor = 1\n\n[quality]\n");
}

TEST(ini_edit_adds_a_missing_section_at_the_end) {
    CHECK(applyIniEdits("[display]\nmode = auto\n", {{"utility", "language", "ru"}}) ==
          "[display]\nmode = auto\n\n[utility]\nlanguage = ru\n");
    CHECK(applyIniEdits("[display]\nmode = auto", {{"display", "monitor", "1"}}) ==
          "[display]\nmode = auto\nmonitor = 1\n");
    CHECK(applyIniEdits("", {{"utility", "language", "uk"}}) == "[utility]\r\nlanguage = uk\r\n");
}

TEST(ini_edit_handles_sections_repeated_by_install_bat) {
    // install.bat appends "[quality] sharpen" and "[display] crisp_text" to older files: every copy of a key changes,
    // and a new key goes into the last copy of its section.
    const std::string text =
        "[display]\r\nmode = auto\r\n[quality]\r\nrender_scale = 100\r\n\r\n[quality]\r\nsharpen = 50\r\n"
        "\r\n[display]\r\ncrisp_text = 1\r\n";
    const std::string out = applyIniEdits(text, {{"quality", "sharpen", "70"}, {"display", "monitor", "2"}});
    CHECK(out ==
          "[display]\r\nmode = auto\r\n[quality]\r\nrender_scale = 100\r\n\r\n[quality]\r\nsharpen = 70\r\n"
          "\r\n[display]\r\ncrisp_text = 1\r\nmonitor = 2\r\n");
    const Config c = parseConfig(out);
    CHECK_EQ(c.sharpen, 70);
    CHECK_EQ(c.monitor, 2);
}

TEST(ini_edit_keeps_bom_cyrillic_and_crlf) {
    const std::string text = "\xEF\xBB\xBF; Настройки\r\n[utility]\r\nlanguage = en ; язык\r\n";
    CHECK(applyIniEdits(text, {{"utility", "language", "ru"}}) ==
          "\xEF\xBB\xBF; Настройки\r\n[utility]\r\nlanguage = ru ; язык\r\n");
}

TEST(ini_edit_reads_lines_the_way_the_parser_does) {
    // The same key in another section, a commented-out key, a section header with spaces and a comment.
    const std::string text = "[ quality ] ; q\nsharpen = 10\n; sharpen = 99\n[frames]\nsharpen = 0\n";
    CHECK(applyIniEdits(text, {{"quality", "sharpen", "20"}}) ==
          "[ quality ] ; q\nsharpen = 20\n; sharpen = 99\n[frames]\nsharpen = 0\n");
}

TEST(ini_read_gives_the_values_parse_config_uses) {
    const auto v = readIni(
        "\xEF\xBB\xBF[Display]\r\nMode = Window ; note\r\n[quality]\r\nsharpen = 10\r\n[quality]\r\nsharpen = 30\r\n"
        "; render_scale = 200\r\nbroken line\r\n[ROExt]\r\nWindowWidth=1600\r\n");
    CHECK(v.at("display.mode") == "Window");
    CHECK(v.at("quality.sharpen") == "30");  // the last copy, as parseConfig
    CHECK(v.count("quality.render_scale") == 0);
    CHECK(v.at("roext.windowwidth") == "1600");
}

namespace {

std::string shippedIni() {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/uaro-graphics.ini", std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

TEST(ini_edit_changes_the_shipped_file_in_place) {
    const std::string text = shippedIni();
    CHECK(!text.empty());
    const std::string out = applyIniEdits(text, {{"quality", "sharpen", "90"}, {"display", "crisp_text", "0"}});
    const Config c = parseConfig(out);
    CHECK_EQ(c.sharpen, 90);
    CHECK(!c.crispText);
    CHECK_EQ(out.size(), text.size());  // "50" -> "90", "1" -> "0": the same length, nothing added
}
```

В `CMakeLists.txt` заменить:
```cmake
    target_link_libraries(uagfx_tests PRIVATE uagfx_core)
```
на:
```cmake
    target_link_libraries(uagfx_tests PRIVATE uagfx_core)
    target_compile_definitions(uagfx_tests PRIVATE UAGFX_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
```

- [ ] **Step 2: Запустить: тесты не собираются**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: ошибка сборки `core/settings/ini_edit.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/settings/ini_edit.hpp`:
```cpp
#pragma once
#include <map>
#include <string>
#include <vector>

namespace uagfx::settings {

struct IniEdit {
    std::string section;
    std::string key;
    std::string value;
};

// `text` with the edits put in, read the way parseConfig reads it (from uaRO Pad; sections and keys match without
// regard to case, as Windows reads ROExt's ini). A key already in its section gets the new value and keeps the rest
// of its line, comment included - every copy of it, also in a repeated section (install.bat appends sections). A
// missing key goes after the last line of the last copy of its section, a missing section to the end of the file.
// Every other byte stays as it was; new lines end the way the file's lines do (CRLF for a new file).
std::string applyIniEdits(const std::string& text, const std::vector<IniEdit>& edits);

// What parseConfig sees: "section.key" (both lower case) -> the value as written (trimmed, the comment cut off). The
// last copy of a key wins, as in parseConfig.
std::map<std::string, std::string> readIni(const std::string& text);

}  // namespace uagfx::settings
```

`src/core/settings/ini_edit.cpp`:
```cpp
#include "core/settings/ini_edit.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <utility>

namespace uagfx::settings {
namespace {

const char kBom[] = "\xEF\xBB\xBF";

struct Line {
    std::string body;  // without the line end
    std::string eol;   // "\r\n", "\n" or "" for a last line without one
};

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

std::vector<Line> splitLines(const std::string& text) {
    std::vector<Line> lines;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) {
            lines.push_back(Line{text.substr(pos), ""});
            break;
        }
        const bool crlf = nl > pos && text[nl - 1] == '\r';
        lines.push_back(Line{text.substr(pos, nl - pos - (crlf ? 1 : 0)), crlf ? "\r\n" : "\n"});
        pos = nl + 1;
    }
    return lines;
}

// What parseConfig sees in a line: the part before ';', trimmed (the first line without a BOM).
std::string meaning(const std::string& body, bool first) {
    std::string s = first && body.rfind(kBom, 0) == 0 ? body.substr(3) : body;
    const size_t comment = s.find(';');
    if (comment != std::string::npos) s = s.substr(0, comment);
    return trim(s);
}

bool sectionName(const std::string& meant, std::string& name) {
    if (meant.size() < 2 || meant.front() != '[' || meant.back() != ']') return false;
    name = lower(trim(meant.substr(1, meant.size() - 2)));
    return true;
}

bool keyName(const std::string& meant, std::string& key) {
    const size_t eq = meant.find('=');
    if (eq == std::string::npos) return false;
    key = lower(trim(meant.substr(0, eq)));
    return true;
}

// The line with a new value: what comes before the old value and what follows it (spaces, comment) stays.
std::string withValue(const std::string& body, const std::string& value) {
    const size_t eq = body.find('=');
    size_t start = eq + 1;
    while (start < body.size() && (body[start] == ' ' || body[start] == '\t')) ++start;
    size_t end = body.find(';', start);
    if (end == std::string::npos) end = body.size();
    while (end > start && (body[end - 1] == ' ' || body[end - 1] == '\t')) --end;
    std::string out = body.substr(0, start);
    if (start == body.size() && !out.empty() && out.back() == '=') out += ' ';  // "key =" with nothing after it
    return out + value + body.substr(end);
}

}  // namespace

std::string applyIniEdits(const std::string& text, const std::vector<IniEdit>& edits) {
    std::vector<Line> lines = splitLines(text);
    const std::string eol = text.find("\r\n") != std::string::npos ? "\r\n"
                            : text.find('\n') != std::string::npos ? "\n"
                                                                   : "\r\n";
    std::vector<std::string> sectionOf(lines.size());
    std::string current;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string name;
        if (sectionName(meaning(lines[i].body, i == 0), name)) current = name;
        sectionOf[i] = current;
    }

    std::vector<std::pair<size_t, std::string>> inserts;  // a new line after line i
    std::vector<IniEdit> newSections;
    for (const IniEdit& e : edits) {
        const std::string section = lower(e.section);
        const std::string wanted = lower(e.key);
        bool replaced = false;
        size_t last = lines.size();  // the section's last line with something on it
        for (size_t i = 0; i < lines.size(); ++i) {
            if (sectionOf[i] != section) continue;
            const std::string meant = meaning(lines[i].body, i == 0);
            std::string name;
            std::string key;
            if (!meant.empty()) last = i;
            if (sectionName(meant, name) || !keyName(meant, key) || key != wanted) continue;
            lines[i].body = withValue(lines[i].body, e.value);
            replaced = true;
        }
        if (replaced) continue;
        if (last < lines.size())
            inserts.emplace_back(last, e.key + " = " + e.value);
        else
            newSections.push_back(e);
    }
    std::stable_sort(inserts.begin(), inserts.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });

    std::string out;
    size_t next = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i].body;
        const bool more = next < inserts.size() && inserts[next].first == i;
        out += lines[i].eol.empty() && more ? eol : lines[i].eol;
        for (; next < inserts.size() && inserts[next].first == i; ++next) out += inserts[next].second + eol;
    }
    std::vector<std::string> added;  // section names, in the order the edits name them
    for (const IniEdit& e : newSections)
        if (std::find(added.begin(), added.end(), lower(e.section)) == added.end()) added.push_back(lower(e.section));
    for (const std::string& section : added) {
        if (!out.empty() && out.back() != '\n') out += eol;
        if (!out.empty()) out += eol;
        std::string header;
        for (const IniEdit& e : newSections)
            if (lower(e.section) == section) {
                if (header.empty()) {
                    header = "[" + e.section + "]";
                    out += header + eol;
                }
                out += e.key + " = " + e.value + eol;
            }
    }
    return out;
}

std::map<std::string, std::string> readIni(const std::string& text) {
    std::map<std::string, std::string> out;
    const std::vector<Line> lines = splitLines(text);
    std::string section;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string meant = meaning(lines[i].body, i == 0);
        std::string name;
        if (sectionName(meant, name)) {
            section = name;
            continue;
        }
        std::string key;
        if (!keyName(meant, key)) continue;
        out[section + "." + key] = trim(meant.substr(meant.find('=') + 1));
    }
    return out;
}

}  // namespace uagfx::settings
```

- [ ] **Step 4: Запустить: тесты проходят**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `200 tests, 0 failed checks`, сборка без предупреждений.

- [ ] **Step 5: Коммит**

```bash
git add CMakeLists.txt src/core/settings/ini_edit.hpp src/core/settings/ini_edit.cpp tests/test_ini_edit.cpp
git commit -q -m "Ядро: правка ini без потери комментариев (из uaRO Pad, без учёта регистра, повторённые разделы)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 2: Драйвер применяет ini на лету (spec §3.4)

`liveUpdate` в ядре решает, что из изменённого файла драйвер берёт сразу (`high_resolution`, `ui_filter`, `crisp_text`,
`render_scale`, `sharpen`, `vsync`, `fps_limit`, `log`), а что ждёт перезапуска (`backend`, `mode`, `monitor`, размер окна,
`allow_tearing`). `Context::present` раз в секунду сравнивает время записи и размер `uaro-graphics.ini`; если файл
изменился — читает, применяет и пишет строку в лог. `crisp_text` включается и выключается через `crisp::install` и
`crisp::uninstall` (оба повторяемы: `install` ничего не делает, если уже стоит; картинки, сделанные до него, получают
двойников при первой записи после `markWindows`). Уровень лога меняет новый `wlog::setLevel`.

**Files:**
- Create: `src/core/live_config.hpp`, `src/core/live_config.cpp`
- Modify: `src/win/log.hpp`, `src/win/log.cpp`, `src/win/context.hpp`, `src/win/context.cpp`
- Test: `tests/test_live_config.cpp`

**Interfaces:**
- Produces: `struct LiveUpdate { Config next; std::vector<std::string> applied, waiting; }`; `LiveUpdate liveUpdate(const
  Config& running, const Config& file)`; `bool appliesNow(const std::string& sectionKey)` (namespace `uagfx`) — Task 9
  сверяет с ним схему; `wlog::setLevel(LogLevel)`.

- [ ] **Step 1: Тесты**

`tests/test_live_config.cpp`:
```cpp
#include "core/live_config.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(live_config_applies_what_can_change_in_a_running_game) {
    const Config running = parseConfig("[quality]\nsharpen = 50\n");
    const LiveUpdate u = liveUpdate(
        running, parseConfig("[quality]\nsharpen = 80\nrender_scale = 150\n[display]\nui_filter = smooth\n"
                             "crisp_text = 0\nhigh_resolution = 0\n[frames]\nvsync = 0\nfps_limit = 60\n"
                             "[debug]\nlog = verbose\n"));
    CHECK_EQ(u.next.sharpen, 80);
    CHECK_EQ(u.next.renderScale, 150);
    CHECK(u.next.uiFilter == UiFilter::Smooth);
    CHECK(!u.next.crispText);
    CHECK(!u.next.highResolution);
    CHECK(!u.next.vsync);
    CHECK_EQ(u.next.fpsLimit, 60);
    CHECK(u.next.logLevel == LogLevel::Debug);
    CHECK(u.waiting.empty());
    CHECK_EQ(u.applied.size(), size_t(8));
    CHECK(u.applied[0] == "high_resolution 1 -> 0");
    CHECK(u.applied[1] == "ui_filter sharp -> smooth");
    CHECK(u.applied[3] == "render_scale 100 -> 150");
    CHECK(u.applied[4] == "sharpen 50 -> 80");
    CHECK(u.applied[7] == "log normal -> verbose");
}

TEST(live_config_keeps_start_up_keys_until_the_next_start) {
    const Config running = parseConfig("");
    const LiveUpdate u = liveUpdate(
        running, parseConfig("[renderer]\nbackend = uaro\n[display]\nmode = window\nmonitor = 1\n"
                             "window_width = 1600\nwindow_height = 900\n[frames]\nallow_tearing = 1\n"));
    CHECK(u.applied.empty());
    CHECK(u.next.backend == running.backend);
    CHECK(u.next.mode == running.mode);
    CHECK_EQ(u.next.monitor, 0);
    CHECK_EQ(u.next.windowWidth, 0);
    CHECK(!u.next.allowTearing);
    CHECK_EQ(u.waiting.size(), size_t(6));
    CHECK(u.waiting[0] == "backend");
    CHECK(u.waiting[1] == "mode");
    CHECK(u.waiting[5] == "allow_tearing");
}

TEST(live_config_an_unchanged_file_changes_nothing) {
    const Config c = parseConfig("[quality]\nsharpen = 70\n[display]\nmode = window\n");
    const LiveUpdate u = liveUpdate(c, c);
    CHECK(u.applied.empty());
    CHECK(u.waiting.empty());
    CHECK_EQ(u.next.sharpen, 70);
}

TEST(live_config_names_the_keys_that_apply_now) {
    for (const char* k : {"display.high_resolution", "display.ui_filter", "display.crisp_text", "quality.render_scale",
                          "quality.sharpen", "frames.vsync", "frames.fps_limit", "debug.log"})
        CHECK(appliesNow(k));
    for (const char* k : {"renderer.backend", "display.mode", "display.monitor", "display.window_width",
                          "display.window_height", "frames.allow_tearing", "utility.language", "nothing"})
        CHECK(!appliesNow(k));
}
```

- [ ] **Step 2: Запустить: тесты не собираются**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: ошибка сборки `core/live_config.hpp: No such file or directory`.

- [ ] **Step 3: Реализация в ядре**

`src/core/live_config.hpp`:
```cpp
#pragma once
#include <string>
#include <vector>

#include "core/config.hpp"

namespace uagfx {

// What the driver does with uaro-graphics.ini changed while the game runs (stage 2 spec §3.4).
struct LiveUpdate {
    Config next;                       // the running settings with every key that applies now taken from the file
    std::vector<std::string> applied;  // "sharpen 50 -> 80", in the order of the ini
    std::vector<std::string> waiting;  // keys changed in the file that apply at the next start: "mode"
};
LiveUpdate liveUpdate(const Config& running, const Config& file);

// Whether a key ("section.key", lower case) applies while the game runs (spec §2.2); false for every other key.
bool appliesNow(const std::string& sectionKey);

}  // namespace uagfx
```

`src/core/live_config.cpp`:
```cpp
#include "core/live_config.hpp"

namespace uagfx {
namespace {

const char* logToken(LogLevel l) { return l == LogLevel::Debug ? "verbose" : "normal"; }

std::string change(const char* key, const std::string& was, const std::string& now) {
    return std::string(key) + " " + was + " -> " + now;
}

std::string flag(bool b) { return b ? "1" : "0"; }

}  // namespace

LiveUpdate liveUpdate(const Config& running, const Config& file) {
    LiveUpdate u;
    u.next = running;
    Config& n = u.next;
    if (file.backend != running.backend) u.waiting.push_back("backend");
    if (file.mode != running.mode) u.waiting.push_back("mode");
    if (file.highResolution != running.highResolution) {
        u.applied.push_back(change("high_resolution", flag(running.highResolution), flag(file.highResolution)));
        n.highResolution = file.highResolution;
    }
    if (file.uiFilter != running.uiFilter) {
        u.applied.push_back(change("ui_filter", uiFilterToken(running.uiFilter), uiFilterToken(file.uiFilter)));
        n.uiFilter = file.uiFilter;
    }
    if (file.crispText != running.crispText) {
        u.applied.push_back(change("crisp_text", flag(running.crispText), flag(file.crispText)));
        n.crispText = file.crispText;
    }
    if (file.monitor != running.monitor) u.waiting.push_back("monitor");
    if (file.windowWidth != running.windowWidth) u.waiting.push_back("window_width");
    if (file.windowHeight != running.windowHeight) u.waiting.push_back("window_height");
    if (file.renderScale != running.renderScale) {
        u.applied.push_back(
            change("render_scale", std::to_string(running.renderScale), std::to_string(file.renderScale)));
        n.renderScale = file.renderScale;
    }
    if (file.sharpen != running.sharpen) {
        u.applied.push_back(change("sharpen", std::to_string(running.sharpen), std::to_string(file.sharpen)));
        n.sharpen = file.sharpen;
    }
    if (file.vsync != running.vsync) {
        u.applied.push_back(change("vsync", flag(running.vsync), flag(file.vsync)));
        n.vsync = file.vsync;
    }
    if (file.fpsLimit != running.fpsLimit) {
        u.applied.push_back(change("fps_limit", std::to_string(running.fpsLimit), std::to_string(file.fpsLimit)));
        n.fpsLimit = file.fpsLimit;
    }
    if (file.allowTearing != running.allowTearing) u.waiting.push_back("allow_tearing");
    if (file.logLevel != running.logLevel) {
        u.applied.push_back(change("log", logToken(running.logLevel), logToken(file.logLevel)));
        n.logLevel = file.logLevel;
    }
    return u;
}

bool appliesNow(const std::string& k) {
    return k == "display.high_resolution" || k == "display.ui_filter" || k == "display.crisp_text" ||
           k == "quality.render_scale" || k == "quality.sharpen" || k == "frames.vsync" ||
           k == "frames.fps_limit" || k == "debug.log";
}

}  // namespace uagfx
```

- [ ] **Step 4: Запустить: тесты проходят**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `204 tests, 0 failed checks`.

- [ ] **Step 5: Драйвер: уровень лога и наблюдение за ini**

В `src/win/log.hpp` заменить:
```cpp
bool enabled(LogLevel level);
```
на:
```cpp
bool enabled(LogLevel level);
// [debug] log changed while the game runs (stage 2 spec §3.4).
void setLevel(LogLevel level);
```

В `src/win/log.cpp` заменить:
```cpp
bool enabled(LogLevel level) { return g_file && int(level) <= int(g_level); }
```
на:
```cpp
bool enabled(LogLevel level) { return g_file && int(level) <= int(g_level); }

void setLevel(LogLevel level) {
    if (!g_file) return;
    EnterCriticalSection(&g_lock);
    g_level = level;
    LeaveCriticalSection(&g_lock);
}
```

В `src/win/context.hpp` заменить:
```cpp
    void logStats(bool final);
```
на:
```cpp
    void logStats(bool final);
    // Stage 2 (spec §3.4): uaro-graphics.ini is looked at once a second while the game runs, and what applies now
    // is applied at once.
    void watchSettings();
```

В `src/win/context.hpp` заменить:
```cpp
    bool scaleMade_ = true;    // whether setScale made that target (0.4.1 review)
```
на:
```cpp
    bool scaleMade_ = true;    // whether setScale made that target (0.4.1 review)
    uint64_t settingsChecked_ = 0;  // GetTickCount64 of the last look at uaro-graphics.ini
    bool settingsKnown_ = false;    // its write time and size below are those of the text read last
    FILETIME settingsTime_{};
    uint64_t settingsSize_ = 0;
```

В `src/win/context.cpp` заменить:
```cpp
#include "core/scale.hpp"
```
на:
```cpp
#include "core/live_config.hpp"
#include "core/scale.hpp"
```

В `src/win/context.cpp` заменить:
```cpp
void Context::present(const Rect& source, bool waitForVblank) {
    flush();
    if (!renderer) return;
```
на:
```cpp
void Context::present(const Rect& source, bool waitForVblank) {
    flush();
    if (!renderer) return;
    watchSettings();
```

В `src/win/context.cpp` заменить:
```cpp
void Context::logStats(bool final) {
```
на:
```cpp
void Context::watchSettings() {
    const uint64_t now = GetTickCount64();
    if (settingsChecked_ && now - settingsChecked_ < 1000) return;
    settingsChecked_ = now;
    const std::string path = gameDir() + "\\uaro-graphics.ini";
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a)) return;  // no file: the defaults stay
    const uint64_t size = (uint64_t(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
    if (settingsKnown_ && CompareFileTime(&a.ftLastWriteTime, &settingsTime_) == 0 && size == settingsSize_) return;
    std::string text;
    if (!readTextFile(path, text)) return;  // the settings app is replacing it right now: the next look reads it
    settingsKnown_ = true;
    settingsTime_ = a.ftLastWriteTime;
    settingsSize_ = size;
    const LiveUpdate u = liveUpdate(config, parseConfig(text));
    if (u.applied.empty() && u.waiting.empty()) return;
    const Config was = config;
    config = u.next;  // render_scale, high_resolution, sharpen, vsync and fps_limit are read at every present
    if (config.uiFilter != was.uiFilter) renderer->setUiSharp(config.uiFilter == UiFilter::Sharp);
    if (config.crispText != was.crispText) {
        if (config.crispText)
            crisp::install(true);
        else
            crisp::uninstall();
    }
    if (config.logLevel != was.logLevel) wlog::setLevel(config.logLevel);
    std::string line = "settings: uaro-graphics.ini changed:";
    for (size_t i = 0; i < u.applied.size(); ++i) line += (i ? ", " : " ") + u.applied[i];
    if (!u.waiting.empty()) line += u.applied.empty() ? " at the next start:" : "; at the next start:";
    for (size_t i = 0; i < u.waiting.size(); ++i) line += (i ? ", " : " ") + u.waiting[i];
    LOG_INFO("%s", line.c_str());
}

void Context::logStats(bool final) {
```

- [ ] **Step 6: Запустить: DLL собирается без предупреждений**

Run: `cmake -S . -B build-win --no-warn-unused-cli -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build-win -j 2>&1 | grep -E "warning|error"; python3 tools/check_dll.py build-win/ddraw.dll`
Expected: ни одной строки `warning`/`error`; `OK: build-win/ddraw.dll: 22 exports, hot-patchable DirectDrawCreateEx, imports gdi32.dll kernel32.dll msvcrt.dll user32.dll`.

- [ ] **Step 7: Коммит**

```bash
git add src/core/live_config.hpp src/core/live_config.cpp tests/test_live_config.cpp src/win/log.hpp src/win/log.cpp src/win/context.hpp src/win/context.cpp
git commit -q -m "Драйвер применяет uaro-graphics.ini на лету: резкость, масштаб рендера, фильтр, чёткий текст, VSync, FPS, лог

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 3: Две копии реестра игры (spec §2.1, §4.2, §4.3)

Модель ключа игры: личная копия (VirtualStore) поверх общей (HKLM), по значениям, имена — без учёта регистра.
`planWrite` даёт запись в личную копию, а если новое значение равно общему — удаление личного (тогда значением снова
управляет `setup.exe`). `planReset` — «как в setup.exe». `firstRunWrites` — всё, без чего игра не стартует
(`registry.md` §7): `WIDTH`, `HEIGHT` не 0, устройство HAL, 32 бита, звук; числа — `REG_DWORD`.

**Files:**
- Create: `src/core/settings/game_registry.hpp`, `src/core/settings/game_registry.cpp`
- Test: `tests/test_game_registry.cpp`

**Interfaces:**
- Produces: `RegType`, `RegValue {type, text, bytes; dword(), sz(), binary(), isSz(), asDword()}`, `NameLess`,
  `RegValues`, `RegCopy {exists, keys; find(subkey, name)}`, `GameRegistry {personal, common}`, `effectiveValue`,
  `gameKeyExists`, `RegOp {Set, Delete}`, `RegWrite {subkey, name, op, value}`, `planWrite`, `planReset`, `applyWrites`,
  `halDeviceGuid()`, `firstRunWrites(width, height, fullscreen)` (namespace `uagfx::settings`).

- [ ] **Step 1: Тесты**

`tests/test_game_registry.cpp`:
```cpp
#include "core/settings/game_registry.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

// The user's PC (2026-10-13): the screen only in the common copy, the game's own saves in the personal one.
GameRegistry usersPc() {
    GameRegistry r;
    r.common.exists = true;
    r.common.keys[""]["WIDTH"] = RegValue::dword(1920);
    r.common.keys[""]["HEIGHT"] = RegValue::dword(1080);
    r.common.keys[""]["ISFULLSCREENMODE"] = RegValue::dword(0);
    r.common.keys[""]["TEXTUREMODE"] = RegValue::dword(2);
    r.common.keys["Option"]["soundVolume"] = RegValue::dword(100);
    r.personal.exists = true;
    r.personal.keys[""]["TEXTUREMODE"] = RegValue::dword(1);
    r.personal.keys[""]["SHOWTIPSATSTARTUP"] = RegValue::dword(0);
    return r;
}

}  // namespace

TEST(registry_the_personal_copy_wins_value_by_value) {
    const GameRegistry r = usersPc();
    CHECK(effectiveValue(r, "", "TEXTUREMODE")->asDword() == 1u);
    CHECK(effectiveValue(r, "", "WIDTH")->asDword() == 1920u);
    CHECK(effectiveValue(r, "", "SHOWTIPSATSTARTUP")->asDword() == 0u);
    CHECK(effectiveValue(r, "option", "SOUNDVOLUME")->asDword() == 100u);  // names ignore case, as in Windows
    CHECK(!effectiveValue(r, "", "SOUNDMODE"));
    CHECK(gameKeyExists(r));
    CHECK(!gameKeyExists(GameRegistry{}));
}

TEST(registry_writes_go_to_the_personal_copy) {
    const GameRegistry r = usersPc();
    const auto w = planWrite(r, "", "WIDTH", RegValue::dword(1680));
    CHECK(w.has_value());
    CHECK(w->op == RegOp::Set);
    CHECK(w->value.asDword() == 1680u);
    // Already what the game sees: nothing to do.
    CHECK(!planWrite(r, "", "WIDTH", RegValue::dword(1920)));
    CHECK(!planWrite(r, "", "TEXTUREMODE", RegValue::dword(1)));
}

TEST(registry_a_value_equal_to_setup_exes_drops_the_personal_one) {
    const GameRegistry r = usersPc();
    // TEXTUREMODE 2 is what setup.exe's copy holds: the personal 1 goes, setup.exe is in charge again.
    const auto w = planWrite(r, "", "TEXTUREMODE", RegValue::dword(2));
    CHECK(w.has_value());
    CHECK(w->op == RegOp::Delete);
    // A type mismatch is a different value: a REG_SZ "2" is no REG_DWORD 2.
    CHECK(planWrite(r, "", "TEXTUREMODE", RegValue::sz("2"))->op == RegOp::Set);
}

TEST(registry_reset_returns_to_setup_exes_value) {
    const GameRegistry r = usersPc();
    CHECK(planReset(r, "", "TEXTUREMODE")->op == RegOp::Delete);
    CHECK(!planReset(r, "", "WIDTH"));               // no personal value
    CHECK(!planReset(r, "", "SHOWTIPSATSTARTUP"));   // nothing in setup.exe's copy to fall back on
}

TEST(registry_applying_writes_updates_the_model) {
    GameRegistry r = usersPc();
    applyWrites(r.personal, {*planWrite(r, "", "WIDTH", RegValue::dword(1680)),
                             *planWrite(r, "", "TEXTUREMODE", RegValue::dword(2))});
    CHECK(effectiveValue(r, "", "WIDTH")->asDword() == 1680u);
    CHECK(effectiveValue(r, "", "TEXTUREMODE")->asDword() == 2u);
    CHECK(!r.personal.find("", "TEXTUREMODE"));
}

TEST(registry_first_run_has_all_the_game_needs) {
    // registry.md §7: the key, WIDTH and HEIGHT not 0, the HAL device, 16 or 32 bits, a sound mode; numbers as DWORDs.
    GameRegistry r;
    applyWrites(r.personal, firstRunWrites(1920, 1080, false));
    CHECK(r.personal.exists);
    CHECK(effectiveValue(r, "", "WIDTH")->asDword() == 1920u);
    CHECK(effectiveValue(r, "", "HEIGHT")->asDword() == 1080u);
    CHECK(effectiveValue(r, "", "ISFULLSCREENMODE")->asDword() == 0u);
    CHECK(effectiveValue(r, "", "BITPERPIXEL")->asDword() == 32u);
    CHECK(effectiveValue(r, "", "SOUNDMODE")->asDword() == 1u);
    const auto guid = effectiveValue(r, "", "GUIDDEVICE");
    CHECK(guid->type == uint32_t(RegType::Binary));
    CHECK(guid->bytes == halDeviceGuid());
    CHECK_EQ(guid->bytes.size(), size_t(16));
    CHECK(guid->bytes[0] == 0xE0 && guid->bytes[3] == 0x84 && guid->bytes[15] == 0x6E);
    for (const RegWrite& w : firstRunWrites(800, 600, true))
        CHECK(w.value.isSz() || w.value.type == uint32_t(RegType::Binary) || w.value.asDword().has_value());
    CHECK(effectiveValue(r, "", "TRILINEARFILTER")->asDword() == 0u);
}
```

- [ ] **Step 2: Запустить: тесты не собираются**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: ошибка сборки `core/settings/game_registry.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/settings/game_registry.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace uagfx::settings {

// Registry value types (winnt.h numbers) the game and regedit use.
enum class RegType : uint32_t { Sz = 1, ExpandSz = 2, Binary = 3, Dword = 4, MultiSz = 7, Qword = 11 };

// One value. REG_SZ is UTF-8 text up to the first NUL (the Windows layer converts from UTF-16); every other type
// keeps its bytes as Windows gives them.
struct RegValue {
    uint32_t type = uint32_t(RegType::Dword);
    std::string text;
    std::vector<uint8_t> bytes;
    static RegValue dword(uint32_t v);
    static RegValue sz(const std::string& s);
    static RegValue binary(const std::vector<uint8_t>& b);
    bool isSz() const { return type == uint32_t(RegType::Sz); }
    std::optional<uint32_t> asDword() const;  // a 4-byte REG_DWORD only
    bool operator==(const RegValue& o) const;
    bool operator!=(const RegValue& o) const { return !(*this == o); }
};

// Windows compares key and value names without regard to case (ASCII letters; the game's names are ASCII).
struct NameLess {
    bool operator()(const std::string& a, const std::string& b) const;
};
using RegValues = std::map<std::string, RegValue, NameLess>;

// One copy of the game's key: "" is the key itself, "Option", "UIRectInfo", "ShortcutItem\\uaRO" its subkeys.
struct RegCopy {
    bool exists = false;  // the key itself exists in this copy
    std::map<std::string, RegValues, NameLess> keys;
    const RegValue* find(const std::string& subkey, const std::string& name) const;
};

// The game's key as the game sees it (stage 2 spec §2.1, §4.2): the personal copy (the user's VirtualStore) over the
// common one (HKLM, written by setup.exe).
struct GameRegistry {
    RegCopy personal;
    RegCopy common;
};

std::optional<RegValue> effectiveValue(const GameRegistry& r, const std::string& subkey, const std::string& name);
bool gameKeyExists(const GameRegistry& r);  // in either copy

enum class RegOp { Set, Delete };
// One change of the personal copy.
struct RegWrite {
    std::string subkey;
    std::string name;
    RegOp op = RegOp::Set;
    RegValue value;  // for Set
    bool operator==(const RegWrite& o) const;
};

// The change of the personal copy that makes the game see `v` (spec §4.2): none when it already does; deleting the
// personal value when the common copy holds exactly `v` (setup.exe stays in charge of it); else setting it.
std::optional<RegWrite> planWrite(const GameRegistry& r, const std::string& subkey, const std::string& name,
                                  const RegValue& v);
// "As in setup.exe": deleting the personal value, so the game sees the common one; none when there is no personal
// value or no common one to fall back on.
std::optional<RegWrite> planReset(const GameRegistry& r, const std::string& subkey, const std::string& name);
void applyWrites(RegCopy& personal, const std::vector<RegWrite>& writes);

// The IID of the Direct3D HAL device, as setup.exe stores it in GUIDDEVICE.
std::vector<uint8_t> halDeviceGuid();
// What the game needs to start when neither copy holds its key (spec §4.3), with the screen chosen.
std::vector<RegWrite> firstRunWrites(int width, int height, bool fullscreen);

}  // namespace uagfx::settings
```

`src/core/settings/game_registry.cpp`:
```cpp
#include "core/settings/game_registry.hpp"

#include <cctype>

namespace uagfx::settings {

RegValue RegValue::dword(uint32_t v) {
    RegValue r;
    r.type = uint32_t(RegType::Dword);
    r.bytes = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
    return r;
}

RegValue RegValue::sz(const std::string& s) {
    RegValue r;
    r.type = uint32_t(RegType::Sz);
    r.text = s;
    return r;
}

RegValue RegValue::binary(const std::vector<uint8_t>& b) {
    RegValue r;
    r.type = uint32_t(RegType::Binary);
    r.bytes = b;
    return r;
}

std::optional<uint32_t> RegValue::asDword() const {
    if (type != uint32_t(RegType::Dword) || bytes.size() != 4) return std::nullopt;
    return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}

bool RegValue::operator==(const RegValue& o) const {
    if (type != o.type) return false;
    return isSz() ? text == o.text : bytes == o.bytes;
}

bool NameLess::operator()(const std::string& a, const std::string& b) const {
    const size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i) {
        const int x = std::tolower(static_cast<unsigned char>(a[i]));
        const int y = std::tolower(static_cast<unsigned char>(b[i]));
        if (x != y) return x < y;
    }
    return a.size() < b.size();
}

const RegValue* RegCopy::find(const std::string& subkey, const std::string& name) const {
    const auto k = keys.find(subkey);
    if (k == keys.end()) return nullptr;
    const auto v = k->second.find(name);
    return v == k->second.end() ? nullptr : &v->second;
}

std::optional<RegValue> effectiveValue(const GameRegistry& r, const std::string& subkey, const std::string& name) {
    if (const RegValue* v = r.personal.find(subkey, name)) return *v;
    if (const RegValue* v = r.common.find(subkey, name)) return *v;
    return std::nullopt;
}

bool gameKeyExists(const GameRegistry& r) { return r.personal.exists || r.common.exists; }

bool RegWrite::operator==(const RegWrite& o) const {
    return op == o.op && subkey == o.subkey && name == o.name && (op == RegOp::Delete || value == o.value);
}

std::optional<RegWrite> planWrite(const GameRegistry& r, const std::string& subkey, const std::string& name,
                                  const RegValue& v) {
    const RegValue* mine = r.personal.find(subkey, name);
    const RegValue* common = r.common.find(subkey, name);
    if (common && *common == v) {
        if (!mine) return std::nullopt;
        return RegWrite{subkey, name, RegOp::Delete, RegValue{}};
    }
    if (mine && *mine == v) return std::nullopt;
    return RegWrite{subkey, name, RegOp::Set, v};
}

std::optional<RegWrite> planReset(const GameRegistry& r, const std::string& subkey, const std::string& name) {
    if (!r.personal.find(subkey, name) || !r.common.find(subkey, name)) return std::nullopt;
    return RegWrite{subkey, name, RegOp::Delete, RegValue{}};
}

void applyWrites(RegCopy& personal, const std::vector<RegWrite>& writes) {
    for (const RegWrite& w : writes) {
        if (w.op == RegOp::Set) {
            personal.exists = true;
            personal.keys[w.subkey][w.name] = w.value;
        } else {
            const auto k = personal.keys.find(w.subkey);
            if (k != personal.keys.end()) k->second.erase(w.name);
        }
    }
}

std::vector<uint8_t> halDeviceGuid() {
    // {84E63DE0-46AA-11CF-816F-0000C020156E}: IID_IDirect3DHALDevice, the GUID's bytes in memory order.
    return {0xE0, 0x3D, 0xE6, 0x84, 0xAA, 0x46, 0xCF, 0x11, 0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E};
}

std::vector<RegWrite> firstRunWrites(int width, int height, bool fullscreen) {
    auto dw = [](const char* name, uint32_t v) { return RegWrite{"", name, RegOp::Set, RegValue::dword(v)}; };
    return {
        dw("ISFULLSCREENMODE", fullscreen ? 1 : 0),
        dw("WIDTH", uint32_t(width)),
        dw("HEIGHT", uint32_t(height)),
        dw("BITPERPIXEL", 32),
        RegWrite{"", "GUIDDEVICE", RegOp::Set, RegValue::binary(halDeviceGuid())},
        RegWrite{"", "GUIDDRIVER", RegOp::Set, RegValue::binary(std::vector<uint8_t>(16, 0))},
        RegWrite{"", "DEVICENAME", RegOp::Set, RegValue::sz("Direct3D HAL")},
        dw("SOUNDMODE", 1),
        dw("DIGITALRATETYPE", 0),
        dw("DIGITALBITSTYPE", 0),
        dw("NUMSAMPLETYPE", 0),
        dw("SPEAKERTYPE", 0),
        RegWrite{"", "PROVIDERNAME", RegOp::Set, RegValue::sz("No Provider")},
        dw("ISLIGHTMAP", 1),
        dw("SPRITEMODE", 2),
        dw("TEXTUREMODE", 2),
        dw("TRILINEARFILTER", 0),
        dw("ISVOODOO", 0),
    };
}

}  // namespace uagfx::settings
```

- [ ] **Step 4: Запустить: тесты проходят**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `210 tests, 0 failed checks`, сборка без предупреждений.

- [ ] **Step 5: Коммит**

```bash
git add src/core/settings/game_registry.hpp src/core/settings/game_registry.cpp tests/test_game_registry.cpp
git commit -q -m "Ядро: две копии реестра игры — личная поверх общей, запись в личную, первый запуск без setup.exe

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 4: Файлы .reg для резервных копий (spec §4.6)

Копия ключа игры в формате regedit (`Windows Registry Editor Version 5.00`): ключ первым, затем подключи; строки,
`dword:`, `hex:`, `hex(N):`; чтение — и своих файлов, и regedit (перенос длинных строк `\`, чужие ключи пропускаются).
Ядро работает с UTF-8; UTF-16 с BOM — дело слоя Windows (Task 10).

**Files:**
- Create: `src/core/settings/reg_file.hpp`, `src/core/settings/reg_file.cpp`
- Test: `tests/test_reg_file.cpp`

**Interfaces:**
- Consumes: `RegCopy`, `RegValue`, `halDeviceGuid` (Task 3).
- Produces: `std::string toRegFile(const RegCopy&, const std::string& root)`; `bool fromRegFile(const std::string& text,
  const std::string& root, RegCopy& out)`.

- [ ] **Step 1: Тесты**

`tests/test_reg_file.cpp`:
```cpp
#include "core/settings/reg_file.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

const char kRoot[] = "HKEY_CURRENT_USER\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok";

RegCopy sample() {
    RegCopy c;
    c.exists = true;
    c.keys[""]["WIDTH"] = RegValue::dword(0x780);
    c.keys[""]["ID"] = RegValue::sz("say \"hi\" \\ there");
    c.keys[""]["GUIDDEVICE"] = RegValue::binary(halDeviceGuid());
    c.keys["Option"]["SKINNAME"] = RegValue::sz("<Basic Skin>");
    c.keys["Option"]["g_outdoorViewLatitude"] = RegValue::dword(0xc2340000u);
    c.keys["BookMark\\uAthena\\\xD0\xA2\xD0\xB5\xD1\x81\xD1\x82\xD0\xB5\xD1\x80"]["Level 7 Cookbook"] = RegValue::dword(1);
    RegValue multi;
    multi.type = uint32_t(RegType::MultiSz);
    multi.bytes = {0x61, 0, 0, 0, 0, 0};
    c.keys["Option"]["list"] = multi;
    return c;
}

}  // namespace

TEST(reg_file_writes_regedit_text) {
    const std::string text = toRegFile(sample(), kRoot);
    CHECK(text.rfind("Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software", 0) == 0);
    CHECK(text.find("\"WIDTH\"=dword:00000780\r\n") != std::string::npos);
    CHECK(text.find("\"ID\"=\"say \\\"hi\\\" \\\\ there\"\r\n") != std::string::npos);
    CHECK(text.find("\"GUIDDEVICE\"=hex:e0,3d,e6,84,aa,46,cf,11,81,6f,00,00,c0,20,15,6e\r\n") != std::string::npos);
    CHECK(text.find("\"list\"=hex(7):61,00,00,00,00,00\r\n") != std::string::npos);
    CHECK(text.find("Gravity Soft\\Ragnarok\\Option]\r\n") != std::string::npos);
    CHECK(text.find("[" + std::string(kRoot) + "]") < text.find("\\Option]"));  // the key itself comes first
}

TEST(reg_file_reads_back_what_it_wrote) {
    const RegCopy in = sample();
    RegCopy out;
    CHECK(fromRegFile(toRegFile(in, kRoot), kRoot, out));
    CHECK(out.exists);
    CHECK(out.keys.size() == in.keys.size());
    for (const auto& k : in.keys)
        for (const auto& v : k.second) {
            const RegValue* got = out.find(k.first, v.first);
            CHECK(got != nullptr);
            if (got) CHECK(*got == v.second);
        }
}

TEST(reg_file_reads_regedit_files_with_wrapped_lines_and_other_keys) {
    const std::string text =
        "\xEF\xBB\xBFWindows Registry Editor Version 5.00\r\n\r\n"
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Other]\r\n\"WIDTH\"=dword:00000001\r\n\r\n"
        "[" + std::string(kRoot) + "]\r\n\"GUIDDEVICE\"=hex:e0,3d,e6,84,aa,46,cf,11,\\\r\n  81,6f,00,00,c0,20,15,6e\r\n"
        "@=\"default\"\r\n\"gone\"=-\r\n\r\n"
        "[-" + std::string(kRoot) + "\\Old]\r\n";
    RegCopy out;
    CHECK(fromRegFile(text, kRoot, out));
    CHECK(out.find("", "GUIDDEVICE")->bytes == halDeviceGuid());
    CHECK(out.find("", "")->text == "default");
    CHECK(!out.find("", "WIDTH"));  // that key is someone else's
    CHECK(!out.find("", "gone"));
    CHECK(out.keys.count("Old") == 0);
}

TEST(reg_file_refuses_other_text_and_writes_an_empty_copy_as_a_header) {
    RegCopy out;
    CHECK(!fromRegFile("[display]\nmode = auto\n", kRoot, out));
    CHECK(toRegFile(RegCopy{}, kRoot) == "Windows Registry Editor Version 5.00\r\n\r\n");
    RegCopy empty;
    empty.exists = true;
    CHECK(toRegFile(empty, kRoot) == "Windows Registry Editor Version 5.00\r\n\r\n[" + std::string(kRoot) + "]\r\n\r\n");
}
```

- [ ] **Step 2: Запустить: тесты не собираются**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: ошибка сборки `core/settings/reg_file.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/settings/reg_file.hpp`:
```cpp
#pragma once
#include <string>

#include "core/settings/game_registry.hpp"

namespace uagfx::settings {

// A copy of the game's key as a .reg file (Windows Registry Editor Version 5.00) under `root`, the key's full path
// (e.g. "HKEY_CURRENT_USER\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok").
// UTF-8 with CRLF; the Windows layer stores it as UTF-16 with a BOM, as regedit does (stage 2 spec §4.6).
std::string toRegFile(const RegCopy& copy, const std::string& root);
// Reads what toRegFile writes, and regedit's own files: the keys under `root` (others are skipped), "name"=value lines
// with "text", dword:, hex: and hex(N): values and '\' line continuations. false when it is not a .reg file.
bool fromRegFile(const std::string& text, const std::string& root, RegCopy& out);

}  // namespace uagfx::settings
```

`src/core/settings/reg_file.cpp`:
```cpp
#include "core/settings/reg_file.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace uagfx::settings {
namespace {

std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (char ch : s) {
        if (ch == '\\' || ch == '"') out += '\\';
        out += ch;
    }
    return out + "\"";
}

std::string hexBytes(const std::vector<uint8_t>& b) {
    std::string out;
    char buf[4];
    for (size_t i = 0; i < b.size(); ++i) {
        std::snprintf(buf, sizeof buf, "%02x", b[i]);
        if (i) out += ',';
        out += buf;
    }
    return out;
}

std::string valueText(const RegValue& v) {
    if (v.isSz()) return quoted(v.text);
    if (const auto d = v.asDword()) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "dword:%08x", unsigned(*d));
        return buf;
    }
    if (v.type == uint32_t(RegType::Binary)) return "hex:" + hexBytes(v.bytes);
    char head[16];
    std::snprintf(head, sizeof head, "hex(%x):", unsigned(v.type));
    return head + hexBytes(v.bytes);
}

bool sameText(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

std::string trimRight(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    return s;
}

std::string trimLeft(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) ++a;
    return s.substr(a);
}

// The logical lines: '\' at a line's end joins the next line (regedit wraps long hex values so).
std::vector<std::string> logicalLines(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    bool joining = false;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = trimRight(text.substr(pos, nl - pos));
        if (joining) line = trimLeft(line);
        const bool more = !line.empty() && line.back() == '\\';
        if (more) line.pop_back();
        current += line;
        joining = more;
        if (!more) {
            out.push_back(current);
            current.clear();
        }
        pos = nl + 1;
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

// A quoted string from s[i] (at the opening quote); i ends after the closing quote.
bool readQuoted(const std::string& s, size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    out.clear();
    for (++i; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            out += s[++i];
        } else if (s[i] == '"') {
            ++i;
            return true;
        } else {
            out += s[i];
        }
    }
    return false;
}

bool readHex(const std::string& s, std::vector<uint8_t>& out) {
    out.clear();
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ',' || s[i] == ' ')) ++i;
        if (i >= s.size()) break;
        char* end = nullptr;
        const long b = std::strtol(s.c_str() + i, &end, 16);
        const size_t used = size_t(end - (s.c_str() + i));
        if (used == 0 || used > 2 || b < 0 || b > 255) return false;
        out.push_back(uint8_t(b));
        i += used;
    }
    return true;
}

bool readValue(const std::string& s, RegValue& v) {
    if (!s.empty() && s[0] == '"') {
        size_t i = 0;
        std::string text;
        if (!readQuoted(s, i, text)) return false;
        v = RegValue::sz(text);
        return true;
    }
    if (s.rfind("dword:", 0) == 0) {
        char* end = nullptr;
        const unsigned long d = std::strtoul(s.c_str() + 6, &end, 16);
        if (end == s.c_str() + 6) return false;
        v = RegValue::dword(uint32_t(d));
        return true;
    }
    if (s.rfind("hex:", 0) == 0) {
        std::vector<uint8_t> b;
        if (!readHex(s.substr(4), b)) return false;
        v = RegValue::binary(b);
        return true;
    }
    if (s.rfind("hex(", 0) == 0) {
        const size_t close = s.find("):");
        if (close == std::string::npos) return false;
        v = RegValue{};
        v.type = uint32_t(std::strtoul(s.substr(4, close - 4).c_str(), nullptr, 16));
        if (!readHex(s.substr(close + 2), v.bytes)) return false;
        if (v.type == uint32_t(RegType::Sz)) return false;  // hex(1) needs UTF-16 decoding: not written by us
        return true;
    }
    return false;
}

}  // namespace

std::string toRegFile(const RegCopy& copy, const std::string& root) {
    std::string out = "Windows Registry Editor Version 5.00\r\n\r\n";
    if (!copy.exists) return out;
    auto section = [&](const std::string& subkey, const RegValues* values) {
        out += "[" + root + (subkey.empty() ? "" : "\\" + subkey) + "]\r\n";
        if (values)
            for (const auto& v : *values)
                out += (v.first.empty() ? std::string("@") : quoted(v.first)) + "=" + valueText(v.second) + "\r\n";
        out += "\r\n";
    };
    const auto self = copy.keys.find("");
    section("", self == copy.keys.end() ? nullptr : &self->second);  // the key itself first, even without values
    for (const auto& key : copy.keys)
        if (!key.first.empty()) section(key.first, &key.second);
    return out;
}

bool fromRegFile(const std::string& text, const std::string& root, RegCopy& out) {
    std::string body = text.rfind("\xEF\xBB\xBF", 0) == 0 ? text.substr(3) : text;
    const std::vector<std::string> lines = logicalLines(body);
    size_t first = 0;
    while (first < lines.size() && lines[first].empty()) ++first;
    if (first == lines.size() ||
        (lines[first] != "Windows Registry Editor Version 5.00" && lines[first] != "REGEDIT4"))
        return false;
    out = RegCopy{};
    bool inside = false;
    std::string subkey;
    for (size_t n = first + 1; n < lines.size(); ++n) {
        const std::string& line = lines[n];
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '[') {
            inside = false;
            if (line.size() < 2 || line.back() != ']' || line[1] == '-') continue;
            const std::string path = line.substr(1, line.size() - 2);
            if (sameText(path, root)) {
                subkey.clear();
            } else if (path.size() > root.size() + 1 && sameText(path.substr(0, root.size()), root) &&
                       path[root.size()] == '\\') {
                subkey = path.substr(root.size() + 1);
            } else {
                continue;
            }
            inside = true;
            out.exists = true;
            out.keys[subkey];
            continue;
        }
        if (!inside) continue;
        std::string name;
        size_t i = 0;
        if (line[0] == '@') {
            i = 1;
        } else if (!readQuoted(line, i, name)) {
            continue;
        }
        if (i >= line.size() || line[i] != '=') continue;
        const std::string rest = line.substr(i + 1);
        if (rest == "-") continue;
        RegValue v;
        if (readValue(rest, v)) out.keys[subkey][name] = v;
    }
    return true;
}

}  // namespace uagfx::settings
```

- [ ] **Step 4: Запустить: тесты проходят**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `214 tests, 0 failed checks`, сборка без предупреждений.

- [ ] **Step 5: Коммит**

```bash
git add src/core/settings/reg_file.hpp src/core/settings/reg_file.cpp tests/test_reg_file.cpp
git commit -q -m "Ядро: файлы .reg — запись и чтение копии ключа игры

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 5: Расчёт экрана (spec §5)

По выбору игрока (во весь экран или в окне, монитор, размер интерфейса `u`, размер окна `W`) — что записать: размер
игры `L`, `ISFULLSCREENMODE`, наш `[display]` (`auto` — окно игры как есть, `window` — наше окно `W / d`), размер окна
ROExt. Окно, которое Windows урежет, и игра меньше 640×480 — недопустимы. `currentScreen` — что игра получит сейчас
(с учётом ROExt); `windowSizes` и `defaultWindow` — размеры окна для списка и для первого запуска.

**Files:**
- Create: `src/core/settings/screen.hpp`, `src/core/settings/screen.cpp`
- Test: `tests/test_screen.cpp`

**Interfaces:**
- Consumes: `Backend`, `DisplayMode`, `Config` (`core/config.hpp`).
- Produces: `Size`, `MonitorInfo {primary, pixels, work, scale, refresh, hdr, modes}`, `Show`, `ScreenChoice {show,
  monitor, ui, window, mode}`, `RoextWindow {active, lock, size}`, `Frame {width, height}`, `kUiMin`, `kUiMax`,
  `kGameMin`, `ScreenProblem`, `ScreenPlan`, `planScreen(choice, monitors, backend, roext, frame)`, `uiSteps()`,
  `windowSizes(m, frame)`, `defaultWindow(m, frame)`, `ScreenNow {choice, game, roextOverride}`,
  `currentScreen(regFullscreen, regSize, ini, roext, monitors, backend)`, `integerScale(s)`.

- [ ] **Step 1: Тесты**

`tests/test_screen.cpp`:
```cpp
#include "core/settings/screen.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

namespace {

// The user's monitor: 4K at 175 %, the task bar 72 real pixels high.
MonitorInfo fourK() {
    MonitorInfo m;
    m.primary = true;
    m.pixels = {3840, 2160};
    m.work = {3840, 2088};
    m.scale = 175;
    m.refresh = 120;
    m.modes = {{3840, 2160}, {2560, 1440}, {1920, 1080}};
    return m;
}

MonitorInfo fullHd() {
    MonitorInfo m;
    m.pixels = {1920, 1080};
    m.work = {1920, 1040};
    m.scale = 100;
    return m;
}

ScreenChoice window(int w, int h, int ui, int monitor = 0) {
    ScreenChoice c;
    c.show = Show::Window;
    c.window = {w, h};
    c.ui = ui;
    c.monitor = monitor;
    return c;
}

ScreenChoice fullscreen(int ui, int monitor = 0) {
    ScreenChoice c;
    c.show = Show::Fullscreen;
    c.ui = ui;
    c.monitor = monitor;
    return c;
}

}  // namespace

TEST(screen_the_users_window_keeps_the_games_own_window) {
    // In a window, 3360x1890 at 175 % on a 175 % monitor: the game draws 1920x1080, ROExt or uaRO Pad keep the window.
    const ScreenPlan p = planScreen(window(3360, 1890, 175), {fourK()}, Backend::Uaro, {}, {});
    CHECK(p.problem == ScreenProblem::None);
    CHECK(p.game == (Size{1920, 1080}));
    CHECK(!p.fullscreen);
    CHECK(p.writesIni);
    CHECK(p.mode == DisplayMode::Auto);
    CHECK(p.iniWindow == (Size{0, 0}));
    CHECK(!p.writesRoext);
    CHECK(std::abs(p.s - 1.75) < 0.001);
    CHECK(!integerScale(p.s));
}

TEST(screen_a_bigger_interface_in_the_same_window_is_our_window) {
    const ScreenPlan p = planScreen(window(3360, 1890, 200), {fourK()}, Backend::Uaro, {}, {});
    CHECK(p.problem == ScreenProblem::None);
    CHECK(p.game == (Size{1680, 945}));
    CHECK(p.mode == DisplayMode::Window);
    CHECK(p.iniWindow == (Size{1920, 1080}));  // W / d: the window in the game's own pixels
    CHECK(integerScale(p.s));
}

TEST(screen_full_screen_divides_the_monitor_by_the_interface_size) {
    const ScreenPlan a = planScreen(fullscreen(175), {fourK()}, Backend::Uaro, {}, {});
    CHECK(a.problem == ScreenProblem::None);
    CHECK(a.fullscreen);
    CHECK(a.game == (Size{2194, 1234}));
    CHECK(a.mode == DisplayMode::Auto);
    const ScreenPlan b = planScreen(fullscreen(200), {fourK()}, Backend::Uaro, {}, {});
    CHECK(b.game == (Size{1920, 1080}));
    CHECK(integerScale(b.s));
    const ScreenPlan c = planScreen(fullscreen(100), {fourK()}, Backend::Uaro, {}, {});
    CHECK(c.game == (Size{3840, 2160}));  // no limit on the game's size in full screen (registry.md §2)
}

TEST(screen_a_second_monitor_is_our_window_there) {
    // At 100 % on a 100 % second monitor: the game's window is made on the main one, so our window moves it.
    const ScreenPlan p = planScreen(window(1600, 900, 100, 1), {fourK(), fullHd()}, Backend::Uaro, {}, {});
    CHECK(p.problem == ScreenProblem::None);
    CHECK(p.mode == DisplayMode::Window);
    CHECK_EQ(p.monitor, 1);
    CHECK(p.iniWindow == (Size{1600, 900}));
    const ScreenPlan f = planScreen(fullscreen(100, 1), {fourK(), fullHd()}, Backend::Uaro, {}, {});
    CHECK(f.game == (Size{1920, 1080}));
    CHECK_EQ(f.monitor, 1);
}

TEST(screen_refuses_windows_that_do_not_fit) {
    // The window itself is taller than the work area.
    CHECK(planScreen(window(3840, 2160, 175), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::TooBig);
    // The window fits, but the game's own window (2240x1260 logical with its frame) is larger than the main monitor's
    // work area of 2194x1193 logical: Windows would cut it.
    CHECK(planScreen(window(2240, 1260, 100), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::TooBig);
    CHECK(planScreen(window(1024, 768, 300), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::TooSmall);
    CHECK(planScreen(window(0, 768, 175), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::BadNumber);
    CHECK(planScreen(fullscreen(99), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::BadNumber);
    CHECK(planScreen(fullscreen(401), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::BadNumber);
    CHECK(planScreen(fullscreen(175), {}, Backend::Uaro, {}, {}).problem == ScreenProblem::BadNumber);
}

TEST(screen_dgvoodoo_keeps_windows_scaling_and_real_modes) {
    // A window: the interface size is Windows scaling, whatever was picked; our ini stays as it is.
    const ScreenPlan w = planScreen(window(3360, 1890, 200), {fourK()}, Backend::DgVoodoo, {}, {});
    CHECK(w.problem == ScreenProblem::None);
    CHECK(w.game == (Size{1920, 1080}));
    CHECK(!w.writesIni);
    // Full screen: the monitor mode picked is the game's size.
    ScreenChoice c = fullscreen(175);
    c.mode = {2560, 1440};
    const ScreenPlan f = planScreen(c, {fourK()}, Backend::DgVoodoo, {}, {});
    CHECK(f.fullscreen);
    CHECK(f.game == (Size{2560, 1440}));
    CHECK(!f.writesIni);
}

TEST(screen_roext_gets_the_same_size_in_a_window) {
    RoextWindow roext;
    roext.active = true;
    roext.lock = true;
    CHECK(planScreen(window(3360, 1890, 175), {fourK()}, Backend::Uaro, roext, {}).writesRoext);
    CHECK(!planScreen(fullscreen(175), {fourK()}, Backend::Uaro, roext, {}).writesRoext);
    roext.lock = false;
    CHECK(!planScreen(window(3360, 1890, 175), {fourK()}, Backend::Uaro, roext, {}).writesRoext);
    roext.size = {1600, 863};
    CHECK(planScreen(window(3360, 1890, 175), {fourK()}, Backend::DgVoodoo, roext, {}).writesRoext);
    roext.active = false;  // a renamed ROExt (dinput.dll.bak) reads nothing
    CHECK(!planScreen(window(3360, 1890, 175), {fourK()}, Backend::Uaro, roext, {}).writesRoext);
}

TEST(screen_offers_window_sizes_that_fit) {
    const std::vector<Size> sizes = windowSizes(fourK(), {});
    CHECK(!sizes.empty());
    CHECK(sizes[0] == (Size{3591, 2020}));  // the largest 16:9 window under the task bar
    bool users = false;
    for (Size s : sizes) {
        users = users || s == Size{3360, 1890};
        CHECK(s.width + 28 <= 3840 && s.height + 68 <= 2088);
    }
    CHECK(users);
    const std::vector<Size> small = windowSizes(fullHd(), {});
    CHECK(small[0] == (Size{1780, 1001}));  // 1040 - 39 high, 16:9
    for (Size s : small) CHECK(s != (Size{1920, 1080}));
}

TEST(screen_first_run_takes_the_largest_standard_window) {
    CHECK(defaultWindow(fourK(), {}) == (Size{3360, 1890}));  // 1920x1080 at 175 %
    CHECK(defaultWindow(fullHd(), {}) == (Size{1600, 900}));  // 1920x1080 does not fit above the task bar
    MonitorInfo tiny = fullHd();
    tiny.work = {800, 560};
    CHECK(defaultWindow(tiny, {}) == (Size{0, 0}));
}

TEST(screen_now_reads_what_the_game_will_get) {
    const Config defaults;
    // The user's PC: a window of 1920x1080, mode = auto, uaRO Pad instead of ROExt.
    const ScreenNow a = currentScreen(false, {1920, 1080}, defaults, {}, {fourK()}, Backend::Uaro);
    CHECK(a.choice.show == Show::Window);
    CHECK(a.choice.window == (Size{3360, 1890}));
    CHECK_EQ(a.choice.ui, 175);
    CHECK(a.game == (Size{1920, 1080}));
    CHECK(!a.roextOverride);
    // An active ROExt with its own size is stronger than the registry.
    RoextWindow roext;
    roext.active = true;
    roext.size = {1600, 863};
    const ScreenNow b = currentScreen(false, {1920, 1080}, defaults, roext, {fourK()}, Backend::Uaro);
    CHECK(b.roextOverride);
    CHECK(b.game == (Size{1600, 863}));
    CHECK(b.choice.window == (Size{2800, 1510}));
    // WindowLock without a size covers the work area.
    roext.size = {};
    roext.lock = true;
    CHECK(currentScreen(false, {1920, 1080}, defaults, roext, {fourK()}, Backend::Uaro).game == (Size{2194, 1193}));
    // Full screen.
    const ScreenNow f = currentScreen(true, {2194, 1234}, defaults, {}, {fourK()}, Backend::Uaro);
    CHECK(f.choice.show == Show::Fullscreen);
    CHECK_EQ(f.choice.ui, 175);
    CHECK(f.choice.mode == (Size{2194, 1234}));
    // Our window of 1920x1080 logical around a 1680x945 game.
    Config ours;
    ours.mode = DisplayMode::Window;
    ours.windowWidth = 1920;
    ours.windowHeight = 1080;
    const ScreenNow w = currentScreen(false, {1680, 945}, ours, {}, {fourK()}, Backend::Uaro);
    CHECK(w.choice.window == (Size{3360, 1890}));
    CHECK_EQ(w.choice.ui, 200);
    // planScreen of what the page starts from writes the same again.
    const ScreenPlan again = planScreen(a.choice, {fourK()}, Backend::Uaro, {}, {});
    CHECK(again.game == (Size{1920, 1080}));
    CHECK(again.mode == DisplayMode::Auto);
}
```

- [ ] **Step 2: Запустить: тесты не собираются**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: ошибка сборки `core/settings/screen.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/settings/screen.hpp`:
```cpp
#pragma once
#include <vector>

#include "core/config.hpp"

namespace uagfx::settings {

struct Size {
    int width = 0;
    int height = 0;
    bool operator==(const Size& o) const { return width == o.width && height == o.height; }
    bool operator!=(const Size& o) const { return !(*this == o); }
};

// A monitor, at the index the driver gives it (stage 2 spec §5.2: 0 = the main one, then EnumDisplayMonitors order).
struct MonitorInfo {
    bool primary = false;
    Size pixels;              // P: its mode, real pixels
    Size work;                // A: without the task bar, real pixels
    int scale = 100;          // d: Windows scaling, %
    int refresh = 0;          // Hz
    bool hdr = false;
    std::vector<Size> modes;  // its real 32-bit modes, largest first (dgVoodoo's full screen)
};

enum class Show { Fullscreen, Window };

// What the player picks on the Screen page (spec §5.2).
struct ScreenChoice {
    Show show = Show::Window;
    int monitor = 0;  // index as the driver numbers them
    int ui = 100;     // u, %
    Size window;      // W, real pixels (in a window)
    Size mode;        // dgVoodoo's full screen: the monitor mode picked, which is L
    bool operator==(const ScreenChoice& o) const {
        return show == o.show && monitor == o.monitor && ui == o.ui && window == o.window && mode == o.mode;
    }
};

// The ROExt settings that matter for the screen (spec §4.4).
struct RoextWindow {
    bool active = false;  // an active ROExt reads this ini
    bool lock = false;    // WindowLock: no border; without its own size it covers the screen but the task bar
    Size size;            // WindowWidth x WindowHeight; 0x0 = not set
};

// What the game's own window adds around its picture (caption, border), in the game's (logical) pixels.
struct Frame {
    int width = 16;
    int height = 39;
};

constexpr int kUiMin = 100;
constexpr int kUiMax = 400;
constexpr Size kGameMin{640, 480};  // the interface needs at least this (registry.md §2)

enum class ScreenProblem { None, BadNumber, TooSmall, TooBig };

// What a choice writes (spec §5.3).
struct ScreenPlan {
    ScreenProblem problem = ScreenProblem::None;
    Size game;                // L: WIDTH x HEIGHT
    bool fullscreen = false;  // ISFULLSCREENMODE; BITPERPIXEL is always 32
    bool writesIni = false;   // uaRO Graphics: [display] mode, monitor, window_width, window_height
    DisplayMode mode = DisplayMode::Auto;
    int monitor = 0;
    Size iniWindow;           // window_width x window_height; 0x0 = the game's own size
    bool writesRoext = false; // ROExt WindowWidth x WindowHeight = game
    double s = 1.0;           // how many times the game is enlarged on screen
};
ScreenPlan planScreen(const ScreenChoice& c, const std::vector<MonitorInfo>& monitors, Backend backend,
                      const RoextWindow& roext, const Frame& frame);

const std::vector<int>& uiSteps();  // 100, 125, 150, 175, 200, 250, 300 (spec §5.2)
// The window sizes offered on a monitor (spec §5.2), largest first: the standard game sizes times d and the largest
// one with the monitor's proportions - those that fit its work area with the frame.
std::vector<Size> windowSizes(const MonitorInfo& m, const Frame& frame);
// The window of a first run without setup.exe (spec §4.3): the largest standard game size times d that fits; 0x0
// when none does.
Size defaultWindow(const MonitorInfo& m, const Frame& frame);

// What the game gets now from its stores (spec §5.3 «Сейчас»), as the choice the Screen page starts from.
struct ScreenNow {
    ScreenChoice choice;
    Size game;                   // L the game will use
    bool roextOverride = false;  // ROExt sets the windowed size (its own, or the work area with WindowLock)
};
ScreenNow currentScreen(bool regFullscreen, Size regSize, const Config& ini, const RoextWindow& roext,
                        const std::vector<MonitorInfo>& monitors, Backend backend);

bool integerScale(double s);  // ×2, ×3...: the crispest interface

}  // namespace uagfx::settings
```

`src/core/settings/screen.cpp`:
```cpp
#include "core/settings/screen.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::settings {
namespace {

int scaled(int v, int numerator, int denominator) {
    return int(std::lround(double(v) * numerator / denominator));
}

Size scaled(Size s, int numerator, int denominator) {
    return Size{scaled(s.width, numerator, denominator), scaled(s.height, numerator, denominator)};
}

double fit(Size out, Size game) {
    if (game.width <= 0 || game.height <= 0) return 1.0;
    return std::min(double(out.width) / game.width, double(out.height) / game.height);
}

// The game's own window (the picture and its frame, logical pixels) inside the main monitor's work area.
bool gameWindowFits(Size game, const MonitorInfo& primary, const Frame& frame) {
    const Size work = scaled(primary.work, 100, primary.scale);
    return game.width + frame.width <= work.width && game.height + frame.height <= work.height;
}

// A window of W real pixels with its frame (at the monitor's scale) inside the monitor's work area.
bool windowFits(Size w, const MonitorInfo& m, const Frame& frame) {
    return w.width + scaled(frame.width, m.scale, 100) <= m.work.width &&
           w.height + scaled(frame.height, m.scale, 100) <= m.work.height;
}

}  // namespace

ScreenPlan planScreen(const ScreenChoice& c, const std::vector<MonitorInfo>& monitors, Backend backend,
                      const RoextWindow& roext, const Frame& frame) {
    ScreenPlan p;
    if (monitors.empty()) {
        p.problem = ScreenProblem::BadNumber;
        return p;
    }
    const MonitorInfo& primary = monitors[0];
    const bool uaro = backend == Backend::Uaro;
    // dgVoodoo leaves our ini alone: its window and full screen stay on the main monitor until 0.6.2.
    const int index = uaro && c.monitor >= 0 && size_t(c.monitor) < monitors.size() ? c.monitor : 0;
    const MonitorInfo& m = monitors[size_t(index)];
    if (c.show == Show::Fullscreen) {
        p.fullscreen = true;
        if (uaro) {
            if (c.ui < kUiMin || c.ui > kUiMax) {
                p.problem = ScreenProblem::BadNumber;
                return p;
            }
            p.game = scaled(m.pixels, 100, c.ui);
            p.writesIni = true;
            p.mode = DisplayMode::Auto;
            p.monitor = index;
        } else {
            p.game = c.mode.width > 0 && c.mode.height > 0 ? c.mode : m.pixels;
        }
        p.s = fit(m.pixels, p.game);
    } else {
        if (c.window.width <= 0 || c.window.height <= 0) {
            p.problem = ScreenProblem::BadNumber;
            return p;
        }
        const int ui = uaro ? c.ui : m.scale;
        if (ui < kUiMin || ui > kUiMax) {
            p.problem = ScreenProblem::BadNumber;
            return p;
        }
        p.game = scaled(c.window, 100, ui);
        if (uaro) {
            p.writesIni = true;
            p.monitor = index;
            if (ui == m.scale && m.primary) {
                p.mode = DisplayMode::Auto;  // the game's own window, as ROExt or uaRO Pad make it
            } else {
                p.mode = DisplayMode::Window;
                p.iniWindow = scaled(c.window, 100, m.scale);
            }
        }
        p.writesRoext = roext.active && (roext.lock || roext.size.width > 0);
        p.s = fit(c.window, p.game);
        if (!windowFits(c.window, m, frame) || !gameWindowFits(p.game, primary, frame)) p.problem = ScreenProblem::TooBig;
    }
    if (p.problem == ScreenProblem::None && (p.game.width < kGameMin.width || p.game.height < kGameMin.height))
        p.problem = ScreenProblem::TooSmall;
    return p;
}

const std::vector<int>& uiSteps() {
    static const std::vector<int> steps{100, 125, 150, 175, 200, 250, 300};
    return steps;
}

const Size kStandard[] = {{1920, 1080}, {1600, 900}, {1366, 768}, {1280, 720}, {1024, 768}};

std::vector<Size> windowSizes(const MonitorInfo& m, const Frame& frame) {
    std::vector<Size> out;
    auto add = [&](Size w) {
        if (w.width > 0 && w.height > 0 && windowFits(w, m, frame) && std::find(out.begin(), out.end(), w) == out.end())
            out.push_back(w);
    };
    for (Size game : kStandard) add(scaled(game, m.scale, 100));
    if (m.pixels.width > 0 && m.pixels.height > 0) {  // the largest with the monitor's proportions
        Size w;
        w.height = m.work.height - scaled(frame.height, m.scale, 100);
        w.width = scaled(w.height, m.pixels.width, m.pixels.height);
        const int roomX = m.work.width - scaled(frame.width, m.scale, 100);
        if (w.width > roomX) {
            w.width = roomX;
            w.height = scaled(w.width, m.pixels.height, m.pixels.width);
        }
        add(w);
    }
    std::sort(out.begin(), out.end(),
              [](Size a, Size b) { return long(a.width) * a.height > long(b.width) * b.height; });
    return out;
}

Size defaultWindow(const MonitorInfo& m, const Frame& frame) {
    for (Size game : kStandard) {
        const Size w = scaled(game, m.scale, 100);
        if (windowFits(w, m, frame)) return w;
    }
    return Size{};
}

ScreenNow currentScreen(bool regFullscreen, Size regSize, const Config& ini, const RoextWindow& roext,
                        const std::vector<MonitorInfo>& monitors, Backend backend) {
    ScreenNow n;
    if (monitors.empty()) return n;
    const bool uaro = backend == Backend::Uaro;
    const int index = uaro && ini.monitor >= 0 && size_t(ini.monitor) < monitors.size() ? ini.monitor : 0;
    const MonitorInfo& m = monitors[size_t(index)];
    const MonitorInfo& primary = monitors[0];
    n.game = regSize;
    const bool borderless = uaro && ini.mode == DisplayMode::Borderless;  // a hand-written ini: over the monitor too
    if (regFullscreen || borderless) {
        n.choice.show = Show::Fullscreen;
        n.choice.monitor = index;
        n.choice.mode = regSize;
        n.choice.ui = regSize.width > 0 ? scaled(m.pixels.width, 100, regSize.width) : 100;
        return n;
    }
    n.choice.show = Show::Window;
    if (roext.active && roext.size.width > 0 && roext.size.height > 0) {
        n.game = roext.size;
        n.roextOverride = true;
    } else if (roext.active && roext.lock) {
        n.game = scaled(primary.work, 100, primary.scale);
        n.roextOverride = true;
    }
    if (uaro && ini.mode == DisplayMode::Window) {
        n.choice.monitor = index;
        const Size logical{ini.windowWidth > 0 ? ini.windowWidth : n.game.width,
                           ini.windowHeight > 0 ? ini.windowHeight : n.game.height};
        n.choice.window = scaled(logical, m.scale, 100);
    } else {
        n.choice.monitor = 0;
        n.choice.window = scaled(n.game, primary.scale, 100);
    }
    n.choice.ui = n.game.width > 0 ? scaled(n.choice.window.width, 100, n.game.width) : 100;
    return n;
}

bool integerScale(double s) { return s >= 1.999 && std::fabs(s - std::round(s)) < 0.001; }

}  // namespace uagfx::settings
```

- [ ] **Step 4: Запустить: тесты проходят**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `224 tests, 0 failed checks`, сборка без предупреждений.

- [ ] **Step 5: Коммит**

```bash
git add src/core/settings/screen.hpp src/core/settings/screen.cpp tests/test_screen.cpp
git commit -q -m "Ядро: расчёт экрана — размер игры из размера интерфейса, окно и полный экран, ROExt

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 6: Модуль ввода и флажки совместимости (spec §2.3, §7)

ROExt узнаётся по строке `ROExt` в DLL, имя его ini — по строке `.\<имя>.ini` (иначе — имя DLL); uaRO Pad — по
`uaro-pad.ini`. Игра грузит только `dinput.dll` и `dinput8.dll`, копии `.bak` — нет. Флажки совместимости
(`AppCompatFlags\Layers`): разбор, снятие одного, что каждый ломает.

**Files:**
- Create: `src/core/settings/input_module.hpp`, `src/core/settings/input_module.cpp`, `src/core/settings/compat_flags.hpp`, `src/core/settings/compat_flags.cpp`
- Test: `tests/test_input_module.cpp`

**Interfaces:**
- Consumes: `readIni` (Task 1), `RoextWindow`, `Size` (Task 5).
- Produces: `ModuleKind {Roext, UaroPad, Other}`, `ModuleInfo {kind, ini}`, `identifyModule(fileName, dll)`,
  `loadedByGame(fileName)`, `roextWindow(ini)`; `CompatLayers {tilde, flags}`, `parseLayers`, `formatLayers`,
  `removeLayer`, `LayerEffect {RunAsAdmin, OldWindows, Dpi, Colors, Other}`, `layerEffect`, `samePath`.

- [ ] **Step 1: Тесты**

`tests/test_input_module.cpp`:
```cpp
#include <cstring>

#include "core/settings/compat_flags.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/input_module.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

std::vector<uint8_t> dll(std::initializer_list<const char*> strings) {
    std::vector<uint8_t> d(64, 0x90);  // some code first
    for (const char* s : strings) {
        d.push_back(0);
        d.insert(d.end(), s, s + std::strlen(s));
        d.push_back(0);
    }
    return d;
}

}  // namespace

TEST(input_module_old_roext_names_its_ini) {
    // The ROExt of the original client: dinput8.dll, ".\dinput8.ini" in its strings.
    const ModuleInfo m = identifyModule("dinput8.dll", dll({"ROExt", ".\\dinput8.ini", "WindowLock"}));
    CHECK(m.kind == ModuleKind::Roext);
    CHECK(m.ini == "dinput8.ini");
    // A newer build loaded as dinput.dll names dinput.ini.
    CHECK(identifyModule("dinput.dll.bak", dll({"ROExt", ".\\dinput.ini"})).ini == "dinput.ini");
    // No ini string: the DLL's own name.
    CHECK(identifyModule("dinput.dll", dll({"ROExt"})).ini == "dinput.ini");
}

TEST(input_module_tells_uaro_pad_and_others) {
    const ModuleInfo pad = identifyModule("dinput.dll", dll({"uaro-pad.ini", "uaRO Pad %s", "ROExt"}));
    CHECK(pad.kind == ModuleKind::UaroPad);  // uaRO Pad mentions ROExt in its log lines: its own name wins
    CHECK(pad.ini == "uaro-pad.ini");
    CHECK(identifyModule("dinput.dll", dll({"DirectInputCreateA"})).kind == ModuleKind::Other);
    CHECK(loadedByGame("DINPUT.dll"));
    CHECK(loadedByGame("dinput8.dll"));
    CHECK(!loadedByGame("dinput.dll.bak"));
}

TEST(input_module_reads_roexts_window) {
    const RoextWindow a = roextWindow(readIni("[ROExt]\r\nWindowLock=1\r\nWindowWidth=1600\r\nWindowHeight=863\r\n"));
    CHECK(a.active);
    CHECK(a.lock);
    CHECK(a.size == (Size{1600, 863}));
    const RoextWindow b = roextWindow(readIni("[ROExt]\nWindowLock = 0\nWindowWidth = 1600\nWindowHeight = 0\n"));
    CHECK(!b.lock);
    CHECK(b.size == (Size{0, 0}));  // ignored when either side is 0
}

TEST(compat_flags_parse_and_remove) {
    CompatLayers l = parseLayers("~ RUNASADMIN HIGHDPIAWARE");
    CHECK(l.tilde);
    CHECK_EQ(l.flags.size(), size_t(2));
    CHECK(removeLayer(l, "runasadmin"));
    CHECK(formatLayers(l) == "~ HIGHDPIAWARE");
    CHECK(!removeLayer(l, "WINXPSP3"));
    CHECK(removeLayer(l, "HIGHDPIAWARE"));
    CHECK(formatLayers(l) == "");  // nothing left: the value goes
    CHECK(formatLayers(parseLayers("WINXPSP3")) == "WINXPSP3");
    CHECK(formatLayers(parseLayers("  ")) == "");
}

TEST(compat_flags_say_what_they_break) {
    CHECK(layerEffect("RUNASADMIN") == LayerEffect::RunAsAdmin);
    CHECK(layerEffect("winxpsp3") == LayerEffect::OldWindows);
    CHECK(layerEffect("WIN7RTM") == LayerEffect::OldWindows);
    CHECK(layerEffect("DPIUNAWARE") == LayerEffect::Dpi);
    CHECK(layerEffect("GDIDPISCALING") == LayerEffect::Dpi);
    CHECK(layerEffect("16BITCOLOR") == LayerEffect::Colors);
    CHECK(layerEffect("DISABLEDXMAXIMIZEDWINDOWEDMODE") == LayerEffect::Other);
    CHECK(samePath("C:\\Games\\uaRO3\\gp\\uaRO\\uaRO.exe", "c:/games/uaro3/gp/uaro/UARO.EXE"));
    CHECK(!samePath("C:\\Games\\uaRO.exe", "C:\\Games\\uaRO2.exe"));
}
```

- [ ] **Step 2: Запустить: тесты не собираются**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: ошибка сборки `core/settings/compat_flags.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/settings/input_module.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/settings/screen.hpp"

namespace uagfx::settings {

enum class ModuleKind { Roext, UaroPad, Other };

// What a dinput DLL in the client folder is (stage 2 spec §2.3), told by its bytes, not by its name.
struct ModuleInfo {
    ModuleKind kind = ModuleKind::Other;
    std::string ini;  // the file it reads: ROExt names it in the DLL (".\dinput8.ini"), else <its name>.ini
};
ModuleInfo identifyModule(const std::string& fileName, const std::vector<uint8_t>& dll);

// Whether the game loads this file: dinput.dll and dinput8.dll in its folder; a renamed copy (".bak") is off.
bool loadedByGame(const std::string& fileName);

// ROExt's window settings (spec §4.4) from its ini as readIni gives it.
RoextWindow roextWindow(const std::map<std::string, std::string>& ini);

}  // namespace uagfx::settings
```

`src/core/settings/input_module.cpp`:
```cpp
#include "core/settings/input_module.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace uagfx::settings {
namespace {

std::string lower(std::string s) {
    for (char& ch : s) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool contains(const std::vector<uint8_t>& data, const char* text) {
    const size_t n = std::strlen(text);
    return std::search(data.begin(), data.end(), text, text + n) != data.end();
}

// The first NUL-terminated ASCII string ".\<name>.ini" in the DLL.
std::string iniNamed(const std::vector<uint8_t>& data) {
    for (size_t i = 0; i + 2 < data.size(); ++i) {
        if (data[i] != '.' || data[i + 1] != '\\' || (i > 0 && data[i - 1] != 0)) continue;
        size_t end = i + 2;
        while (end < data.size() && data[end] >= 0x20 && data[end] < 0x7F && data[end] != '\\' && data[end] != '/')
            ++end;
        if (end >= data.size() || data[end] != 0) continue;
        const std::string name(data.begin() + long(i) + 2, data.begin() + long(end));
        if (name.size() > 4 && lower(name.substr(name.size() - 4)) == ".ini") return name;
    }
    return "";
}

int number(const std::map<std::string, std::string>& ini, const char* key) {
    const auto it = ini.find(key);
    return it == ini.end() ? 0 : int(std::strtol(it->second.c_str(), nullptr, 10));
}

}  // namespace

ModuleInfo identifyModule(const std::string& fileName, const std::vector<uint8_t>& dll) {
    ModuleInfo m;
    if (contains(dll, "uaro-pad.ini")) {
        m.kind = ModuleKind::UaroPad;
        m.ini = "uaro-pad.ini";
    } else if (contains(dll, "ROExt")) {
        m.kind = ModuleKind::Roext;
        m.ini = iniNamed(dll);
        if (m.ini.empty()) {
            const size_t dot = fileName.find('.');
            m.ini = fileName.substr(0, dot) + ".ini";
        }
    }
    return m;
}

bool loadedByGame(const std::string& fileName) {
    const std::string name = lower(fileName);
    return name == "dinput.dll" || name == "dinput8.dll";
}

RoextWindow roextWindow(const std::map<std::string, std::string>& ini) {
    RoextWindow w;
    w.active = true;
    w.lock = number(ini, "roext.windowlock") != 0;
    const int width = number(ini, "roext.windowwidth");
    const int height = number(ini, "roext.windowheight");
    if (width > 0 && height > 0) w.size = {width, height};  // ROExt ignores its size when either side is 0
    return w;
}

}  // namespace uagfx::settings
```

`src/core/settings/compat_flags.hpp`:
```cpp
#pragma once
#include <string>
#include <vector>

namespace uagfx::settings {

// The value Windows keeps for an exe under AppCompatFlags\Layers (stage 2 spec §7): "~ RUNASADMIN HIGHDPIAWARE".
struct CompatLayers {
    bool tilde = false;  // "~": set on the file's Compatibility tab
    std::vector<std::string> flags;
};
CompatLayers parseLayers(const std::string& value);
std::string formatLayers(const CompatLayers& l);  // "" when no flag is left: the value is deleted then
bool removeLayer(CompatLayers& l, const std::string& flag);  // case-insensitive; false when it was not there

enum class LayerEffect {
    RunAsAdmin,  // the game does not see the personal copy of its registry key
    OldWindows,  // a Windows XP/7/8 mode: Direct3D 12 and Windows scaling are not what the driver expects
    Dpi,         // Windows scaling overridden: the picture is no longer at the window's real pixels
    Colors,      // 256 colours, 16 bits, 640x480
    Other
};
LayerEffect layerEffect(const std::string& flag);

// Whether a value name under Layers is this exe's path: case-insensitive, '/' and '\' alike.
bool samePath(const std::string& a, const std::string& b);

}  // namespace uagfx::settings
```

`src/core/settings/compat_flags.cpp`:
```cpp
#include "core/settings/compat_flags.hpp"

#include <cctype>
#include <sstream>

namespace uagfx::settings {
namespace {

std::string upper(std::string s) {
    for (char& ch : s) ch = char(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

}  // namespace

CompatLayers parseLayers(const std::string& value) {
    CompatLayers l;
    std::istringstream in(value);
    std::string word;
    while (in >> word) {
        if (word == "~")
            l.tilde = true;
        else
            l.flags.push_back(word);
    }
    return l;
}

std::string formatLayers(const CompatLayers& l) {
    if (l.flags.empty()) return "";
    std::string out = l.tilde ? "~" : "";
    for (const std::string& f : l.flags) out += (out.empty() ? "" : " ") + f;
    return out;
}

bool removeLayer(CompatLayers& l, const std::string& flag) {
    const size_t before = l.flags.size();
    for (size_t i = l.flags.size(); i-- > 0;)
        if (upper(l.flags[i]) == upper(flag)) l.flags.erase(l.flags.begin() + long(i));
    return l.flags.size() != before;
}

LayerEffect layerEffect(const std::string& flag) {
    const std::string f = upper(flag);
    if (f == "RUNASADMIN") return LayerEffect::RunAsAdmin;
    for (const char* os : {"WIN95", "WIN98", "NT4SP5", "WIN2000", "WINXP", "WINXPSP2", "WINXPSP3", "WINSRV03SP1",
                           "WINSRV08SP1", "VISTARTM", "VISTASP1", "VISTASP2", "WIN7RTM", "WIN8RTM", "WIN81RTM"})
        if (f == os) return LayerEffect::OldWindows;
    for (const char* dpi : {"HIGHDPIAWARE", "DPIUNAWARE", "GDIDPISCALING", "PERPROCESSSYSTEMDPIFORCEOFF",
                            "PERPROCESSSYSTEMDPIFORCEON"})
        if (f == dpi) return LayerEffect::Dpi;
    for (const char* c : {"256COLOR", "16BITCOLOR", "640X480"})
        if (f == c) return LayerEffect::Colors;
    return LayerEffect::Other;
}

bool samePath(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char x = char(std::tolower(static_cast<unsigned char>(a[i])));
        char y = char(std::tolower(static_cast<unsigned char>(b[i])));
        if (x == '/') x = '\\';
        if (y == '/') y = '\\';
        if (x != y) return false;
    }
    return true;
}

}  // namespace uagfx::settings
```

- [ ] **Step 4: Запустить: тесты проходят**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `229 tests, 0 failed checks`, сборка без предупреждений.

- [ ] **Step 5: Коммит**

```bash
git add src/core/settings/input_module.hpp src/core/settings/input_module.cpp src/core/settings/compat_flags.hpp src/core/settings/compat_flags.cpp tests/test_input_module.cpp
git commit -q -m "Ядро: модуль ввода (ROExt, uaRO Pad) по содержимому DLL и флажки совместимости Windows

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 7: Пресеты, выдержка из лога, отчёт для форума (spec §7, §8)

Пресеты «Максимум», «Баланс» (значения по умолчанию), «Скорость», «Без улучшений» — правки ini. Выдержка из
`uaro-graphics.log`: версия, рендер, последний fps, ошибки, хвост. Отчёт — по-английски, без логина, имён персонажей
и серверов (их в нём негде взять) и без имени пользователя Windows в путях.

**Files:**
- Create: `src/core/settings/presets.hpp`, `src/core/settings/presets.cpp`, `src/core/settings/log_digest.hpp`, `src/core/settings/log_digest.cpp`, `src/core/settings/report.hpp`, `src/core/settings/report.cpp`
- Test: `tests/test_service.cpp`

**Interfaces:**
- Consumes: `IniEdit`, `applyIniEdits`, `readIni` (Task 1), `Config`.
- Produces: `Preset {Maximum, Balance, Speed, Plain}`, `kPresetCount`, `presetEdits(p)`, `presetOf(config)`;
  `LogDigest {found, version, renderer, fps, errors, errorCount, tail}`, `digestLog(text, keep)`; `ReportFacts`,
  `makeReport(facts)`, `maskUserPaths(text)`.

- [ ] **Step 1: Тесты**

`tests/test_service.cpp`:
```cpp
#include "core/settings/log_digest.hpp"
#include "core/settings/presets.hpp"
#include "core/settings/report.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

TEST(presets_set_the_spec_values) {
    const Config max = parseConfig("[display]\nhigh_resolution = 0\n" +
                                   applyIniEdits("", presetEdits(Preset::Maximum)));
    CHECK(max.highResolution);
    CHECK_EQ(max.renderScale, 200);
    CHECK_EQ(max.sharpen, 50);
    CHECK(max.uiFilter == UiFilter::Sharp);
    CHECK(max.crispText);
    const Config plain = parseConfig(applyIniEdits("", presetEdits(Preset::Plain)));
    CHECK(!plain.highResolution);
    CHECK_EQ(plain.sharpen, 0);
    CHECK(!plain.crispText);
    for (int i = 0; i < kPresetCount; ++i)
        CHECK(presetOf(parseConfig(applyIniEdits("", presetEdits(Preset(i))))) == Preset(i));
}

TEST(presets_the_defaults_are_balance) {
    CHECK(presetOf(Config{}) == Preset::Balance);
    CHECK(!presetOf(parseConfig("[quality]\nsharpen = 70\n")).has_value());
}

TEST(log_digest_reads_the_main_facts) {
    const std::string log =
        "19:20:01.001 INFO  uaRO Graphics 0.6.0 in C:\\Games\\uaRO\\uaRO.exe\r\n"
        "19:20:01.010 INFO  renderer: uaro (backend = uaro, Windows 10, Direct3D 12)\r\n"
        "19:20:02.000 ERROR cannot load C:\\x\\DDraw.dll (error 126)\r\n"
        "19:20:31.000 INFO  3600 frames in 30.0 s (120.0 fps), 0 reads of the frame from the GPU\r\n"
        "19:21:01.000 INFO  3881 frames in 30.0 s (129.4 fps), 1 reads of the frame from the GPU; crisp text: 74 twins\r\n";
    const LogDigest d = digestLog(log, 2);
    CHECK(d.found);
    CHECK(d.version == "0.6.0");
    CHECK(d.renderer == "uaro (backend = uaro, Windows 10, Direct3D 12)");
    CHECK(d.fps == "129.4 fps");
    CHECK_EQ(d.errorCount, size_t(1));
    CHECK(d.errors[0] == "cannot load C:\\x\\DDraw.dll (error 126)");
    CHECK_EQ(d.tail.size(), size_t(2));
    CHECK(d.tail[1].rfind("19:21:01.000", 0) == 0);
    CHECK(!digestLog("").found);
}

TEST(report_hides_user_names_and_lists_the_settings) {
    ReportFacts f;
    f.utilityVersion = "0.6.0";
    f.windows = "10.0 build 26200";
    f.gpu = "NVIDIA GeForce RTX 5060 Ti, 16311 MB, Direct3D 12: OK";
    f.monitors = {"3840x2160, 120 Hz, 175 %, HDR no, main"};
    f.iniText = "; comment\r\n[quality]\r\nsharpen = 80 ; note\r\n";
    f.screen = "window 3360x1890, interface 175 %, game 1920x1080";
    f.log = digestLog("19:20:01.001 INFO  uaRO Graphics 0.6.0 in C:\\Users\\Ivan Petrov\\Games\\uaRO\\uaRO.exe\r\n");
    const std::string r = makeReport(f);
    CHECK(r.find("Ivan") == std::string::npos);
    CHECK(r.find("C:\\Users\\...\\Games\\uaRO\\uaRO.exe") != std::string::npos);
    CHECK(r.find("  quality.sharpen = 80\r\n") != std::string::npos);
    CHECK(r.find("comment") == std::string::npos);
    CHECK(r.find("Installed driver: unknown") != std::string::npos);
    CHECK(maskUserPaths("C:/Users/anna/x and D:\\users\\b\\y") == "C:/Users/.../x and D:\\users\\...\\y");
}
```

- [ ] **Step 2: Запустить: тесты не собираются**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: ошибка сборки `core/settings/log_digest.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/settings/presets.hpp`:
```cpp
#pragma once
#include <optional>
#include <vector>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"

namespace uagfx::settings {

enum class Preset { Maximum, Balance, Speed, Plain };
constexpr int kPresetCount = 4;

// The uaro-graphics.ini edits of a preset (stage 2 spec §8): high_resolution, render_scale, sharpen, ui_filter,
// crisp_text.
std::vector<IniEdit> presetEdits(Preset p);
// The preset whose values these settings hold, if any.
std::optional<Preset> presetOf(const Config& c);

}  // namespace uagfx::settings
```

`src/core/settings/presets.cpp`:
```cpp
#include "core/settings/presets.hpp"

namespace uagfx::settings {
namespace {

struct Values {
    bool highResolution;
    int renderScale;
    int sharpen;
    bool crispText;
};

Values valuesOf(Preset p) {
    switch (p) {
        case Preset::Maximum: return {true, 200, 50, true};
        case Preset::Balance: return {true, 100, 50, true};
        case Preset::Speed: return {true, 100, 0, false};
        case Preset::Plain: break;
    }
    return {false, 100, 0, false};
}

}  // namespace

std::vector<IniEdit> presetEdits(Preset p) {
    const Values v = valuesOf(p);
    return {{"display", "high_resolution", v.highResolution ? "1" : "0"},
            {"quality", "render_scale", std::to_string(v.renderScale)},
            {"quality", "sharpen", std::to_string(v.sharpen)},
            {"display", "ui_filter", "sharp"},
            {"display", "crisp_text", v.crispText ? "1" : "0"}};
}

std::optional<Preset> presetOf(const Config& c) {
    for (int i = 0; i < kPresetCount; ++i) {
        const Values v = valuesOf(Preset(i));
        if (c.highResolution == v.highResolution && c.renderScale == v.renderScale && c.sharpen == v.sharpen &&
            c.uiFilter == UiFilter::Sharp && c.crispText == v.crispText)
            return Preset(i);
    }
    return std::nullopt;
}

}  // namespace uagfx::settings
```

`src/core/settings/log_digest.hpp`:
```cpp
#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace uagfx::settings {

// The main facts of uaro-graphics.log for the Diagnostics page and the report (stage 2 spec §7). Its lines are
// "HH:MM:SS.mmm LEVEL message".
struct LogDigest {
    bool found = false;
    std::string version;              // "0.6.0" from "uaRO Graphics 0.6.0 in ..."
    std::string renderer;             // what follows "renderer: "
    std::string fps;                  // "129.4 fps" from the last stats line
    std::vector<std::string> errors;  // the last 10 ERROR messages
    size_t errorCount = 0;
    std::vector<std::string> tail;    // the last lines, whole, for the report
};
LogDigest digestLog(const std::string& text, size_t keep = 40);

}  // namespace uagfx::settings
```

`src/core/settings/log_digest.cpp`:
```cpp
#include "core/settings/log_digest.hpp"

#include <cctype>

namespace uagfx::settings {
namespace {

bool stamped(const std::string& l) {
    if (l.size() < 19) return false;
    for (size_t i : {0u, 1u, 3u, 4u, 6u, 7u, 9u, 10u, 11u})
        if (!std::isdigit(static_cast<unsigned char>(l[i]))) return false;
    return l[2] == ':' && l[5] == ':' && l[8] == '.' && l[12] == ' ';
}

}  // namespace

LogDigest digestLog(const std::string& text, size_t keep) {
    LogDigest d;
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
        pos = nl + 1;
    }
    d.found = !lines.empty();
    for (const std::string& l : lines) {
        if (!stamped(l)) continue;
        const std::string level = l.substr(13, 5);
        const std::string msg = l.substr(19);
        if (msg.rfind("uaRO Graphics ", 0) == 0) {
            const size_t end = msg.find(' ', 14);
            d.version = msg.substr(14, end == std::string::npos ? std::string::npos : end - 14);
        } else if (msg.rfind("renderer: ", 0) == 0) {
            d.renderer = msg.substr(10);
        }
        const size_t frames = msg.find(" frames in ");
        const size_t open = msg.find(" (", frames == std::string::npos ? 0 : frames);
        const size_t close = msg.find(" fps)", open == std::string::npos ? 0 : open);
        if (frames != std::string::npos && open != std::string::npos && close != std::string::npos)
            d.fps = msg.substr(open + 2, close + 4 - (open + 2));
        if (level.rfind("ERROR", 0) == 0) {
            ++d.errorCount;
            d.errors.push_back(msg);
            if (d.errors.size() > 10) d.errors.erase(d.errors.begin());
        }
    }
    const size_t from = lines.size() > keep ? lines.size() - keep : 0;
    d.tail.assign(lines.begin() + long(from), lines.end());
    return d;
}

}  // namespace uagfx::settings
```

`src/core/settings/report.hpp`:
```cpp
#pragma once
#include <string>
#include <vector>

#include "core/settings/log_digest.hpp"

namespace uagfx::settings {

// What the forum report says (stage 2 spec §7). It carries no login, character or server names: there is no place
// for them here.
struct ReportFacts {
    std::string utilityVersion;
    std::string driverVersion;               // the installed build, "" when unknown
    std::string windows;                     // "10.0 build 26200"
    std::string gpu;                         // "NVIDIA GeForce RTX 5060 Ti, 16311 MB, Direct3D 12: OK"
    std::vector<std::string> monitors;       // one line each
    std::string iniText;                     // uaro-graphics.ini: only "section.key = value" goes into the report
    std::string screen;                      // what the game gets: "window 3360x1890, interface 175 %, game 1920x1080"
    std::vector<std::string> inputModules;   // "dinput.dll: uaRO Pad"
    std::vector<std::string> compatFlags;    // "HKCU: RUNASADMIN"
    std::vector<std::string> problems;       // red and yellow lines of the Diagnostics page
    LogDigest log;
};
// The text, in English for the developers; Windows user names in paths are hidden.
std::string makeReport(const ReportFacts& f);
std::string maskUserPaths(const std::string& text);  // "\Users\<name>\" -> "\Users\...\"

}  // namespace uagfx::settings
```

`src/core/settings/report.cpp`:
```cpp
#include "core/settings/report.hpp"

#include <cctype>

#include "core/settings/ini_edit.hpp"

namespace uagfx::settings {

std::string maskUserPaths(const std::string& text) {
    std::string out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t hit = std::string::npos;
        for (size_t i = pos; i + 7 <= text.size(); ++i) {
            if ((text[i] == '\\' || text[i] == '/') && (text[i + 6] == '\\' || text[i + 6] == '/')) {
                std::string word = text.substr(i + 1, 5);
                for (char& ch : word) ch = char(std::tolower(static_cast<unsigned char>(ch)));
                if (word == "users") {
                    hit = i;
                    break;
                }
            }
        }
        if (hit == std::string::npos) {
            out += text.substr(pos);
            break;
        }
        const size_t nameStart = hit + 7;
        size_t nameEnd = nameStart;
        while (nameEnd < text.size() && text[nameEnd] != '\\' && text[nameEnd] != '/' && text[nameEnd] != '\n' &&
               text[nameEnd] != '\r')
            ++nameEnd;
        out += text.substr(pos, nameStart - pos) + "...";
        pos = nameEnd;
    }
    return out;
}

std::string makeReport(const ReportFacts& f) {
    std::string r = "uaRO Graphics report\r\n";
    r += "Settings app: " + f.utilityVersion + "\r\n";
    r += "Installed driver: " + (f.driverVersion.empty() ? std::string("unknown") : f.driverVersion) + "\r\n";
    r += "Windows: " + f.windows + "\r\n";
    r += "Graphics: " + f.gpu + "\r\n";
    for (const std::string& m : f.monitors) r += "Monitor: " + m + "\r\n";
    r += "Screen: " + f.screen + "\r\n";
    for (const std::string& m : f.inputModules) r += "Input: " + m + "\r\n";
    for (const std::string& c : f.compatFlags) r += "Compatibility: " + c + "\r\n";
    for (const std::string& p : f.problems) r += "Problem: " + p + "\r\n";
    r += "\r\nuaro-graphics.ini:\r\n";
    for (const auto& kv : readIni(f.iniText)) r += "  " + kv.first + " = " + kv.second + "\r\n";
    r += "\r\nuaro-graphics.log";
    if (!f.log.found) {
        r += ": none yet\r\n";
    } else {
        r += " (" + std::to_string(f.log.errorCount) + " errors):\r\n";
        for (const std::string& l : f.log.tail) r += "  " + l + "\r\n";
    }
    return maskUserPaths(r);
}

}  // namespace uagfx::settings
```

- [ ] **Step 4: Запустить: тесты проходят**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `233 tests, 0 failed checks`, сборка без предупреждений.

- [ ] **Step 5: Коммит**

```bash
git add src/core/settings/presets.hpp src/core/settings/presets.cpp src/core/settings/log_digest.hpp src/core/settings/log_digest.cpp src/core/settings/report.hpp src/core/settings/report.cpp tests/test_service.cpp
git commit -q -m "Ядро: пресеты, выдержка из лога и отчёт для форума

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 8: Тексты на трёх языках (spec §6)

Все тексты окна — в одной таблице `{en, uk, ru}` в порядке `Text`; `static_assert` следит, что строк столько же, сколько
значений, тест — что у каждого текста три непустых перевода с одними и теми же подстановками `{N}`. Язык по
умолчанию — язык меню Windows (украинский, русский, иначе английский).

**Files:**
- Create: `src/core/settings/strings.hpp`, `src/core/settings/strings.cpp`
- Test: `tests/test_strings.cpp`

**Interfaces:**
- Produces: `Lang {En, Uk, Ru}`, `kLangCount`, `parseLang`, `langToken`, `langName`, `langForWindows(langId)`,
  `enum class Text` (все тексты), `const char* tr(Lang, Text)` (UTF-8), `std::string fmt(pattern, args)`.

- [ ] **Step 1: Тесты**

`tests/test_strings.cpp`:
```cpp
#include <set>

#include "core/settings/strings.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

std::multiset<std::string> placeholders(const std::string& s) {
    std::multiset<std::string> out;
    for (size_t i = 0; i + 2 < s.size(); ++i)
        if (s[i] == '{' && s[i + 2] == '}' && s[i + 1] >= '1' && s[i + 1] <= '9') out.insert(s.substr(i, 3));
    return out;
}

}  // namespace

TEST(strings_every_text_in_three_languages) {
    for (int t = 0; t < int(Text::Count); ++t) {
        const std::string en = tr(Lang::En, Text(t));
        CHECK(!en.empty());
        for (Lang l : {Lang::Uk, Lang::Ru}) {
            const std::string other = tr(l, Text(t));
            CHECK(!other.empty());
            CHECK(placeholders(other) == placeholders(en));  // the same {N} in every language
        }
    }
}

TEST(strings_fill_placeholders) {
    CHECK(fmt("Game {1}×{2}, {1}", {"1920", "1080"}) == "Game 1920×1080, 1920");
    CHECK(fmt("{3} missing", {"a"}) == " missing");
    CHECK(fmt(tr(Lang::Ru, Text::ScreenResult), {"1920", "1080", "3360", "1890", "1,75"}) ==
          "Игра 1920×1080 → экран 3360×1890, интерфейс ×1,75");
}

TEST(strings_languages_and_windows) {
    CHECK(parseLang("RU") == Lang::Ru);
    CHECK(parseLang("uk") == Lang::Uk);
    CHECK(!parseLang("de").has_value());
    CHECK(std::string(langToken(Lang::Uk)) == "uk");
    CHECK(langForWindows(0x0422) == Lang::Uk);  // uk-UA
    CHECK(langForWindows(0x0419) == Lang::Ru);  // ru-RU
    CHECK(langForWindows(0x0409) == Lang::En);  // en-US
    CHECK(langForWindows(0x0407) == Lang::En);  // de-DE
}
```

- [ ] **Step 2: Запустить: тесты не собираются**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: ошибка сборки `core/settings/strings.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/settings/strings.hpp`:
```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace uagfx::settings {

// The settings app speaks English, Ukrainian and Russian (stage 2 spec §6); the report and the log stay English.
enum class Lang : uint8_t { En, Uk, Ru };
constexpr int kLangCount = 3;

std::optional<Lang> parseLang(const std::string& token);  // "en", "uk", "ru", any case
const char* langToken(Lang l);                            // [utility] language
const char* langName(Lang l);                             // in its own language
// The language of Windows' menus (GetUserDefaultUILanguage): Ukrainian or Russian, else English.
Lang langForWindows(uint16_t langId);

enum class Text : uint16_t {
    AppTitle, PageRender, PageScreen, PageQuality, PageFrames, PageDiagnostics, PageService,
    Apply, ApplyAndPlay, Revert, Close,
    StatusRunning, StatusClosed, StatusPending, StatusApplied, ElevatedBanner,
    AfterRestart, LiveNote, LiveNoteRunning, AskUnsaved,
    RenderHeading, RenderUaro, RenderDgVoodoo, RenderUaroHint, RenderDgVoodooHint, RenderNeedsWindows10,
    RenderNoD3D12, InstalledOurs, InstalledNoDriver, DgVoodooFound, DgVoodooMissing, PauseNote,
    ScreenShow, ScreenFullscreen, ScreenWindow, ScreenMonitor, MonitorItem, MonitorMain, ScreenUi, ScreenUiHint,
    ScreenUiExact, ScreenUiDgVoodoo, ScreenMode, ScreenModeHint, ScreenWindowSize, ScreenWindowSizeHint, WindowItem,
    ScreenResult, ScreenCrispest, ScreenTooBig, ScreenTooSmall, ScreenBadNumber, ScreenNowWindow,
    ScreenNowFullscreen, ScreenSetupDiffers, ShowWindowWord, ShowFullscreenWord, ScreenAsSetup, ScreenRoext,
    ScreenFirstRun, ScreenAltTab,
    QualityHighRes, QualityHighResHint, QualityRenderScale, QualityRenderScaleHint, QualitySharpen,
    QualitySharpenHint, QualityUiFilter, QualityUiFilterHint, UiSharp, UiSmooth, QualityCrispText,
    QualityCrispTextHint,
    FramesVsync, FramesVsyncHint, FramesFpsLimit, FramesFpsLimitHint, FramesTearing, FramesTearingHint,
    DiagCheck, DiagResult, DiagFix, DiagOpenLog, DiagOpenFolder, DiagReport, DiagRecheck, DiagVerboseLog,
    DiagVerboseLogHint, DiagReportDone, DiagWindows, DiagWindowsValue, DiagWindowsOld, DiagGpu, DiagGpuValue,
    DiagD3D12, DiagD3D12Ok, DiagD3D12Failed, DiagMonitor, DiagMonitorValue, Yes, No, DiagDdraw,
    DiagDdrawOther, DiagDdrawNone, DiagDgVoodooOk, DiagDgVoodooMissing, DiagModuleRoext, DiagModuleOther,
    DiagModuleOff, DiagInput, DiagInputNone, DiagFolder, DiagFolderOk, DiagFolderReadOnly, DiagFolderProgramFiles,
    DiagApp, DiagElevated, DiagGame, DiagGameRunning, DiagGameClosed, DiagCompat, DiagCompatUser, DiagCompatAll,
    CompatRunAsAdmin, CompatOldWindows, CompatDpi, CompatColors, CompatOther, CompatAllUsersHint, DiagRegistry,
    DiagRegistryValue, DiagRegistryNone, DiagRegistryDiffers, DiagLogRenderer, DiagLogErrors, DiagLogErrorsNone,
    DiagLogErrorsSome, DiagLogFps, DiagLog, DiagLogNone, DiagFixed,
    ServicePresets, PresetMaximum, PresetBalance, PresetSpeed, PresetPlain, PresetNote, PresetCustom,
    ServiceBackups, BackupRestore, BackupPrune, BackupNote, BackupNone, BackupAsk, ServiceReset, ResetButton,
    ResetAsk, ServiceVersions, VersionsValue, Unknown,
    ErrWriteFile, ErrWriteRegistry, ErrGameRunning, ErrBackup, ErrScreen, ErrLaunch, ErrRestore,
    Count
};

const char* tr(Lang l, Text t);  // UTF-8
// "{1}", "{2}", ... replaced by the arguments.
std::string fmt(const std::string& pattern, const std::vector<std::string>& args);

}  // namespace uagfx::settings
```

`src/core/settings/strings.cpp`:
```cpp
#include "core/settings/strings.hpp"

#include <cctype>

namespace uagfx::settings {
namespace {

struct Entry {
    const char* en;
    const char* uk;
    const char* ru;
};

// In the order of Text.
const Entry kTexts[] = {
    {"uaRO Graphics Settings", "uaRO Graphics Settings", "uaRO Graphics Settings"},
    {"Renderer", "Рендер", "Рендер"},
    {"Screen", "Екран", "Экран"},
    {"Quality", "Якість", "Качество"},
    {"Frames", "Кадри", "Кадры"},
    {"Diagnostics", "Діагностика", "Диагностика"},
    {"Service", "Сервіс", "Сервис"},
    {"&Apply", "&Застосувати", "&Применить"},
    {"Apply and &play", "Застосувати й &грати", "Применить и и&грать"},
    {"&Undo", "&Скасувати", "&Отменить"},
    {"&Close", "За&крити", "За&крыть"},
    {"The game is running.", "Гра запущена.", "Игра запущена."},
    {"The game is not running.", "Гра не запущена.", "Игра не запущена."},
    {"Not applied: {1}.", "Не застосовано: {1}.", "Не применено: {1}."},
    {"Applied.", "Застосовано.", "Применено."},
    {"The settings app runs as administrator: a game started from it would not see its settings. Close it and start "
     "it normally.",
     "Утиліту запущено від імені адміністратора: гра, запущена з неї, не побачить свої налаштування. Закрий її та "
     "запусти звичайно.",
     "Утилита запущена от имени администратора: игра, запущенная из неё, не увидит свои настройки. Закрой её и "
     "запусти обычно."},
    {"↻ after the game restarts", "↻ після перезапуску гри", "↻ после перезапуска игры"},
    {"These settings work in a running game too, without a restart.",
     "Ці налаштування діють і в запущеній грі, без перезапуску.",
     "Эти настройки действуют и в запущенной игре, без перезапуска."},
    {"The game is running: a slider applies as soon as you let it go.",
     "Гра запущена: повзунок застосовується, щойно його відпустиш.",
     "Игра запущена: ползунок применяется, как только его отпустишь."},
    {"Some changes are not applied. Close without them?", "Деякі зміни не застосовано. Закрити без них?",
     "Часть изменений не применена. Закрыть без них?"},
    {"What draws the game", "Чим малювати гру", "Чем рисовать игру"},
    {"uaRO Graphics (Direct3D 12)", "uaRO Graphics (Direct3D 12)", "uaRO Graphics (Direct3D 12)"},
    {"dgVoodoo", "dgVoodoo", "dgVoodoo"},
    {"Our renderer: the picture at the screen's real pixels, sharpening, crisp text. Needs Windows 10 or newer.",
     "Наш рендер: картинка в справжніх точках екрана, різкість, чіткий текст. Потрібна Windows 10 або новіша.",
     "Наш рендер: картинка в настоящих точках экрана, резкость, чёткий текст. Нужна Windows 10 или новее."},
    {"As before: dgVoodoo draws the game.", "Як раніше: гру малює dgVoodoo.", "Как раньше: игру рисует dgVoodoo."},
    {"Our renderer is not available: it needs Windows 10 or newer.",
     "Наш рендер недоступний: потрібна Windows 10 або новіша.", "Наш рендер недоступен: нужна Windows 10 или новее."},
    {"Our renderer is not available: Direct3D 12 does not start ({1}).",
     "Наш рендер недоступний: Direct3D 12 не запускається ({1}).",
     "Наш рендер недоступен: Direct3D 12 не запускается ({1})."},
    {"Installed: uaRO Graphics {1}.", "Встановлено: uaRO Graphics {1}.", "Установлено: uaRO Graphics {1}."},
    {"uaRO Graphics' ddraw.dll is not in the game folder: run install.bat.",
     "ddraw.dll uaRO Graphics немає в теці гри: запусти install.bat.",
     "ddraw.dll uaRO Graphics нет в папке игры: запусти install.bat."},
    {"dgVoodoo is in the dgvoodoo folder.", "dgVoodoo — у теці dgvoodoo.", "dgVoodoo — в папке dgvoodoo."},
    {"dgVoodoo is not in the dgvoodoo folder.", "dgVoodoo немає в теці dgvoodoo.", "dgVoodoo нет в папке dgvoodoo."},
    {"In the game, Pause (or Scroll Lock) switches every improvement off and on, to compare.",
     "У грі клавіша Pause (або Scroll Lock) вимикає й вмикає всі покращення — для порівняння.",
     "В игре клавиша Pause (или Scroll Lock) выключает и включает все улучшения — для сравнения."},
    {"Show", "Показ", "Показ"},
    {"Full screen", "На весь екран", "Во весь экран"},
    {"In a window", "У вікні", "В окне"},
    {"Monitor", "Монітор", "Монитор"},
    {"{1}: {2}×{3}, scaling {4} %", "{1}: {2}×{3}, масштаб {4} %", "{1}: {2}×{3}, масштаб {4} %"},
    {", main", ", основний", ", основной"},
    {"Interface size", "Розмір інтерфейсу", "Размер интерфейса"},
    {"Screen pixels per game pixel. More: a larger interface and less of the world.",
     "Скільки точок екрана на одну точку гри. Більше — більший інтерфейс і менше видно світу.",
     "Сколько точек экрана на одну точку игры. Больше — крупнее интерфейс и меньше видно мира."},
    {"exactly, %", "точно, %", "точно, %"},
    {"With dgVoodoo the interface size is Windows scaling ({1} %).",
     "З dgVoodoo розмір інтерфейсу — це масштаб Windows ({1} %).",
     "С dgVoodoo размер интерфейса — это масштаб Windows ({1} %)."},
    {"Monitor mode", "Режим монітора", "Режим монитора"},
    {"dgVoodoo may switch the monitor to this mode.", "dgVoodoo може перемкнути монітор у цей режим.",
     "dgVoodoo может переключить монитор в этот режим."},
    {"Window size", "Розмір вікна", "Размер окна"},
    {"In the screen's real pixels; only sizes that fit the screen above the task bar.",
     "У справжніх точках екрана; лише ті, що вміщуються над панеллю завдань.",
     "В настоящих точках экрана; только те, что помещаются над панелью задач."},
    {"{1}×{2} (game {3}×{4})", "{1}×{2} (гра {3}×{4})", "{1}×{2} (игра {3}×{4})"},
    {"Game {1}×{2} → screen {3}×{4}, interface ×{5}", "Гра {1}×{2} → екран {3}×{4}, інтерфейс ×{5}",
     "Игра {1}×{2} → экран {3}×{4}, интерфейс ×{5}"},
    {" — the crispest interface", " — найчіткіший інтерфейс", " — самый чёткий интерфейс"},
    {"It will not fit: the window is larger than the screen above the task bar.",
     "Не вміститься: вікно більше за екран над панеллю завдань.",
     "Не поместится: окно больше экрана над панелью задач."},
    {"Too small: the game needs at least 640×480.", "Замало: грі потрібно щонайменше 640×480.",
     "Слишком мало: игре нужно не меньше 640×480."},
    {"Not a valid size.", "Неправильний розмір.", "Неверный размер."},
    {"Now: in a window · window {1}×{2} · interface {3} % · game {4}×{5}",
     "Зараз: у вікні · вікно {1}×{2} · інтерфейс {3} % · гра {4}×{5}",
     "Сейчас: в окне · окно {1}×{2} · интерфейс {3} % · игра {4}×{5}"},
    {"Now: full screen · game {1}×{2} · interface {3} %", "Зараз: на весь екран · гра {1}×{2} · інтерфейс {3} %",
     "Сейчас: во весь экран · игра {1}×{2} · интерфейс {3} %"},
    {"setup.exe has {1}×{2}, {3}; the game uses this app's values.",
     "У setup.exe {1}×{2}, {3}; гра бере значення цієї утиліти.",
     "В setup.exe {1}×{2}, {3}; игра берёт значения этой утилиты."},
    {"in a window", "у вікні", "в окне"},
    {"full screen", "на весь екран", "во весь экран"},
    {"As in setup.exe", "Як у setup.exe", "Как в setup.exe"},
    {"ROExt sets the window size ({1}): the app writes the same size there.",
     "Розмір вікна задає ROExt ({1}): утиліта запише туди той самий розмір.",
     "Размер окна задаёт ROExt ({1}): утилита запишет туда тот же размер."},
    {"The game has no settings in the registry yet: Apply creates everything it needs, no setup.exe required.",
     "У реєстрі ще немає налаштувань гри: «Застосувати» створить усе потрібне, setup.exe не знадобиться.",
     "В реестре ещё нет настроек игры: «Применить» создаст всё нужное, setup.exe не понадобится."},
    {"In full screen the game minimises itself on Alt+Tab: that is how it works.",
     "На весь екран гра сама згортається при Alt+Tab — так вона влаштована.",
     "Во весь экран игра сама сворачивается при Alt+Tab — так она устроена."},
    {"High resolution", "Висока роздільність", "Высокое разрешение"},
    {"Draw the game at the window's real pixels: with Windows scaling the world is sharper.",
     "Малювати гру в справжніх точках вікна: при масштабі Windows світ чіткіший.",
     "Рисовать игру в настоящих точках окна: при масштабе Windows мир чётче."},
    {"Render scale", "Масштаб рендера", "Масштаб рендера"},
    {"The frame is drawn larger and shrunk when shown: smoother edges, more work for the graphics card.",
     "Кадр малюється більшим і зменшується при показі: гладші краї, відеокарта працює більше.",
     "Кадр рисуется больше и уменьшается при показе: глаже края, видеокарта работает больше."},
    {"Sharpening", "Різкість", "Резкость"},
    {"Sharpening of the picture on screen; 0 = off. Screenshots are not sharpened.",
     "Різкість картинки на екрані; 0 — вимкнено. Скриншоти не змінюються.",
     "Резкость картинки на экране; 0 — выключено. Скриншоты не меняются."},
    {"Interface pictures", "Картинки інтерфейсу", "Картинки интерфейса"},
    {"How enlarged interface pictures look. With crisp text the windows come from their crisp twins.",
     "Як виглядають збільшені картинки інтерфейсу. З чітким текстом вікна беруться з чітких двійників.",
     "Как выглядят увеличенные картинки интерфейса. При чётком тексте окна берутся из чётких двойников."},
    {"sharp", "чіткі", "чёткие"},
    {"smooth", "м'які", "мягкие"},
    {"Crisp text", "Чіткий текст", "Чёткий текст"},
    {"The game's text, window frames and fills at the screen's real pixels when the game is enlarged.",
     "Текст, рамки й заливки вікон гри в справжніх точках екрана, коли гру збільшено.",
     "Текст, рамки и заливки окон игры в настоящих точках экрана, когда игра увеличена."},
    {"Vertical sync (VSync)", "Вертикальна синхронізація (VSync)", "Вертикальная синхронизация (VSync)"},
    {"Frames are shown in step with the monitor: no tearing.", "Кадри показуються в такт із монітором: без розривів.",
     "Кадры показываются в такт с монитором: без разрывов."},
    {"FPS limit", "Обмеження FPS", "Ограничение FPS"},
    {"Frames per second at most, 0-1000; 0 = no limit.", "Кадрів на секунду не більше, 0–1000; 0 — без обмеження.",
     "Кадров в секунду не больше, 0–1000; 0 — без ограничения."},
    {"Tearing for G-SYNC and FreeSync", "Розриви для G-SYNC і FreeSync", "Разрывы для G-SYNC и FreeSync"},
    {"With VSync off, G-SYNC and FreeSync monitors show each frame as soon as it is ready.",
     "Без VSync монітори G-SYNC і FreeSync показують кожен кадр, щойно він готовий.",
     "Без VSync мониторы G-SYNC и FreeSync показывают каждый кадр, как только он готов."},
    {"Check", "Перевірка", "Проверка"},
    {"Result", "Результат", "Результат"},
    {"&Fix", "&Виправити", "&Исправить"},
    {"Open the log", "Відкрити лог", "Открыть лог"},
    {"Open the folder", "Відкрити теку", "Открыть папку"},
    {"Report for the forum", "Звіт для форуму", "Отчёт для форума"},
    {"Check again", "Перевірити знову", "Проверить снова"},
    {"Detailed log", "Докладний лог", "Подробный лог"},
    {"The log writes more details: useful when something is wrong.",
     "Лог пише більше подробиць: потрібно, коли щось не так.",
     "Лог пишет больше подробностей: нужно, когда что-то не так."},
    {"The report is on the clipboard and in uaro-graphics-report.txt.",
     "Звіт скопійовано в буфер обміну й збережено в uaro-graphics-report.txt.",
     "Отчёт скопирован в буфер обмена и сохранён в uaro-graphics-report.txt."},
    {"Windows", "Windows", "Windows"},
    {"{1}.{2}, build {3}", "{1}.{2}, збірка {3}", "{1}.{2}, сборка {3}"},
    {" — our renderer needs Windows 10", " — нашому рендеру потрібна Windows 10", " — нашему рендеру нужна Windows 10"},
    {"Graphics card", "Відеокарта", "Видеокарта"},
    {"{1}, {2} MB, driver {3}", "{1}, {2} МБ, драйвер {3}", "{1}, {2} МБ, драйвер {3}"},
    {"Direct3D 12", "Direct3D 12", "Direct3D 12"},
    {"works", "працює", "работает"},
    {"does not start ({1})", "не запускається ({1})", "не запускается ({1})"},
    {"Monitor {1}", "Монітор {1}", "Монитор {1}"},
    {"{1}×{2}, {3} Hz, scaling {4} %, HDR: {5}", "{1}×{2}, {3} Гц, масштаб {4} %, HDR: {5}",
     "{1}×{2}, {3} Гц, масштаб {4} %, HDR: {5}"},
    {"yes", "так", "да"},
    {"no", "ні", "нет"},
    {"ddraw.dll", "ddraw.dll", "ddraw.dll"},
    {"not uaRO Graphics: run install.bat", "не uaRO Graphics: запусти install.bat",
     "не uaRO Graphics: запусти install.bat"},
    {"missing: run install.bat", "немає: запусти install.bat", "нет: запусти install.bat"},
    {"in the dgvoodoo folder", "у теці dgvoodoo", "в папке dgvoodoo"},
    {"not in the dgvoodoo folder: dgVoodoo will not start", "немає в теці dgvoodoo: dgVoodoo не запуститься",
     "нет в папке dgvoodoo: dgVoodoo не запустится"},
    {"ROExt (reads {1})", "ROExt (читає {1})", "ROExt (читает {1})"},
    {"an unknown module", "невідомий модуль", "неизвестный модуль"},
    {" — off", " — вимкнено", " — отключён"},
    {"Input module", "Модуль вводу", "Модуль ввода"},
    {"none: neither ROExt nor uaRO Pad", "немає: ні ROExt, ні uaRO Pad", "нет: ни ROExt, ни uaRO Pad"},
    {"Game folder", "Тека гри", "Папка игры"},
    {"writable", "можна писати", "можно писать"},
    {"not writable: the settings cannot be saved", "не можна писати: налаштування не збережуться",
     "нельзя писать: настройки не сохранятся"},
    {"in Program Files: Windows redirects the game's files; better move the client, e.g. to C:\\Games",
     "у Program Files: Windows підміняє файли гри; краще перенести клієнт, наприклад у C:\\Games",
     "в Program Files: Windows подменяет файлы игры; лучше перенести клиент, например в C:\\Games"},
    {"Settings app", "Утиліта", "Утилита"},
    {"runs as administrator: start it normally", "запущена від імені адміністратора: запусти звичайно",
     "запущена от имени администратора: запусти обычно"},
    {"Game", "Гра", "Игра"},
    {"running", "запущена", "запущена"},
    {"not running", "не запущена", "не запущена"},
    {"Flag {1} ({2})", "Прапорець {1} ({2})", "Флажок {1} ({2})"},
    {"this user", "ваш користувач", "ваш пользователь"},
    {"all users", "усі користувачі", "все пользователи"},
    {"the game does not see its personal settings in the registry",
     "гра не бачить свої особисті налаштування в реєстрі", "игра не видит свои личные настройки в реестре"},
    {"an old Windows mode: in the way of Direct3D 12 and scaling", "режим старої Windows: заважає Direct3D 12 і масштабу",
     "режим старой Windows: мешает Direct3D 12 и масштабу"},
    {"scaling overridden: the picture is not at the real pixels", "масштаб перевизначено: картинка не в справжніх точках",
     "масштаб переопределён: картинка не в настоящих точках"},
    {"old colours or 640×480", "старі кольори або 640×480", "старые цвета или 640×480"},
    {"a Windows compatibility flag", "прапорець сумісності Windows", "флажок совместимости Windows"},
    {"; remove it in uaRO.exe's properties as administrator",
     "; знімається у властивостях uaRO.exe з правами адміністратора",
     "; снимается в свойствах uaRO.exe с правами администратора"},
    {"Game settings in the registry", "Налаштування гри в реєстрі", "Настройки игры в реестре"},
    {"personal copy: {1}, setup.exe's copy: {2}", "особиста копія: {1}, копія setup.exe: {2}",
     "личная копия: {1}, копия setup.exe: {2}"},
    {"none: Apply on the Screen page creates them", "немає: «Застосувати» на сторінці «Екран» створить їх",
     "нет: «Применить» на странице «Экран» создаст их"},
    {"the game uses {1}, setup.exe has {2}", "гра бере {1}, у setup.exe {2}", "игра берёт {1}, в setup.exe {2}"},
    {"Renderer last time", "Рендер минулого разу", "Рендер в прошлый раз"},
    {"Errors in the log", "Помилки в лозі", "Ошибки в логе"},
    {"none", "немає", "нет"},
    {"{1}, the last: {2}", "{1}, остання: {2}", "{1}, последняя: {2}"},
    {"Frames per second", "Кадрів на секунду", "Кадров в секунду"},
    {"Log", "Лог", "Лог"},
    {"none yet: start the game", "ще немає: запусти гру", "ещё нет: запусти игру"},
    {"Fixed.", "Виправлено.", "Исправлено."},
    {"Presets", "Пресети", "Пресеты"},
    {"Maximum", "Максимум", "Максимум"},
    {"Balance", "Баланс", "Баланс"},
    {"Speed", "Швидкість", "Скорость"},
    {"No improvements", "Без покращень", "Без улучшений"},
    {"A preset fills in the Quality page; Apply writes it. Now: {1}.",
     "Пресет заповнює сторінку «Якість»; записує «Застосувати». Зараз: {1}.",
     "Пресет заполняет страницу «Качество»; записывает «Применить». Сейчас: {1}."},
    {"your own values", "свої значення", "свои значения"},
    {"Backups", "Резервні копії", "Резервные копии"},
    {"Restore", "Відновити", "Восстановить"},
    {"Keep the 5 newest", "Залишити 5 останніх", "Оставить 5 последних"},
    {"Before its first write in each run the app copies everything it may change; the 20 newest copies stay.",
     "Перед першим записом у кожному запуску утиліта копіює все, що може змінити; зберігаються 20 останніх копій.",
     "Перед первой записью в каждом запуске утилита копирует всё, что может изменить; хранятся 20 последних копий."},
    {"No backups yet.", "Копій ще немає.", "Копий ещё нет."},
    {"Restore the settings from the copy of {1}? The current ones are copied first.",
     "Повернути налаштування з копії від {1}? Поточні спершу теж скопіюються.",
     "Вернуть настройки из копии от {1}? Нынешние сначала тоже скопируются."},
    {"Reset", "Скидання", "Сброс"},
    {"Reset uaRO Graphics to its defaults", "Скинути uaRO Graphics до типових", "Сбросить uaRO Graphics к умолчаниям"},
    {"Return every uaRO Graphics setting to its default? The renderer and the language stay.",
     "Повернути всі налаштування uaRO Graphics до типових? Рендер і мова залишаться.",
     "Вернуть все настройки uaRO Graphics к умолчаниям? Рендер и язык останутся."},
    {"Versions", "Версії", "Версии"},
    {"Settings app {1} · driver {2}", "Утиліта {1} · драйвер {2}", "Утилита {1} · драйвер {2}"},
    {"unknown", "невідомо", "неизвестно"},
    {"Cannot write {1}.", "Не вдалося записати {1}.", "Не удалось записать {1}."},
    {"Cannot write {1} to the registry.", "Не вдалося записати {1} до реєстру.", "Не удалось записать {1} в реестр."},
    {"Close the game first.", "Спершу закрий гру.", "Сначала закрой игру."},
    {"Cannot make the backup ({1}): nothing was written.", "Не вдалося зробити резервну копію ({1}): нічого не записано.",
     "Не удалось сделать резервную копию ({1}): ничего не записано."},
    {"Screen: {1}", "Екран: {1}", "Экран: {1}"},
    {"Cannot start uaRO.exe (error {1}).", "Не вдалося запустити uaRO.exe (помилка {1}).",
     "Не удалось запустить uaRO.exe (ошибка {1})."},
    {"Cannot restore {1}.", "Не вдалося відновити {1}.", "Не удалось восстановить {1}."},
};
static_assert(sizeof(kTexts) / sizeof(kTexts[0]) == size_t(Text::Count), "a text for every Text");

}  // namespace

std::optional<Lang> parseLang(const std::string& token) {
    std::string t;
    for (char ch : token) t += char(std::tolower(static_cast<unsigned char>(ch)));
    if (t == "en") return Lang::En;
    if (t == "uk") return Lang::Uk;
    if (t == "ru") return Lang::Ru;
    return std::nullopt;
}

const char* langToken(Lang l) { return l == Lang::Uk ? "uk" : l == Lang::Ru ? "ru" : "en"; }

const char* langName(Lang l) { return l == Lang::Uk ? "Українська" : l == Lang::Ru ? "Русский" : "English"; }

Lang langForWindows(uint16_t langId) {
    const uint16_t primary = langId & 0x3FF;
    if (primary == 0x22) return Lang::Uk;
    if (primary == 0x19) return Lang::Ru;
    return Lang::En;
}

const char* tr(Lang l, Text t) {
    const Entry& e = kTexts[size_t(t)];
    return l == Lang::Uk ? e.uk : l == Lang::Ru ? e.ru : e.en;
}

std::string fmt(const std::string& pattern, const std::vector<std::string>& args) {
    std::string out;
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '{' && i + 2 < pattern.size() && pattern[i + 2] == '}' && pattern[i + 1] >= '1' &&
            pattern[i + 1] <= '9') {
            const size_t n = size_t(pattern[i + 1] - '1');
            out += n < args.size() ? args[n] : std::string();
            i += 2;
        } else {
            out += pattern[i];
        }
    }
    return out;
}

}  // namespace uagfx::settings
```

- [ ] **Step 4: Запустить: тесты проходят**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `236 tests, 0 failed checks`, сборка без предупреждений.

- [ ] **Step 5: Коммит**

```bash
git add src/core/settings/strings.hpp src/core/settings/strings.cpp tests/test_strings.cpp
git commit -q -m "Ядро: тексты утилиты на английском, украинском и русском

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 9: Схема ключей ini для страниц (spec §6.1)

Каждый ключ `uaro-graphics.ini`, который страницы правят один к одному: раздел и ключ, страница, элемент (флажок,
ползунок, список, число), пределы, слова файла, тексты. Тесты: каждое слово читается драйвером и возвращается тем же
словом; каждый ключ из `dist/uaro-graphics.ini` — в схеме или пишется страницей «Экран»; сразу действуют ровно те
ключи, которые применяет драйвер.

**Files:**
- Create: `src/core/settings/schema.hpp`, `src/core/settings/schema.cpp`
- Test: `tests/test_schema.cpp`

**Interfaces:**
- Consumes: `IniEdit` (Task 1), `appliesNow` (Task 2), `Text` (Task 8).
- Produces: `Page {Render, Screen, Quality, Frames, Diagnostics, Service}`, `kPageCount`, `pageName(p)`, `Control`,
  `IniSetting {section, key, page, control, min, max, step, tokens, choices, label, hint, unit}`, `iniSettings()`,
  `findSetting(sectionKey)`, `sectionKey(s)`, `tokenOf(s, config)`, `validToken(s, token)`.

- [ ] **Step 1: Тесты**

`tests/test_schema.cpp`:
```cpp
#include <fstream>
#include <sstream>

#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

TEST(schema_every_word_reads_back_through_the_driver) {
    // Each word a page can write is what parseConfig reads, and tokenOf gives the same word back.
    for (const IniSetting& s : iniSettings()) {
        std::vector<std::string> words;
        if (s.control == Control::Slider || s.control == Control::Number)
            words = {std::to_string(s.min), std::to_string(s.max), std::to_string((s.min + s.max) / 2)};
        else
            for (const char* t : s.tokens) words.push_back(t);
        for (const std::string& w : words) {
            CHECK(validToken(s, w));
            const Config c = parseConfig(std::string("[") + s.section + "]\n" + s.key + " = " + w + "\n");
            CHECK(tokenOf(s, c) == w);
        }
        CHECK(!validToken(s, "nonsense"));
        CHECK(findSetting(sectionKey(s)) == &s);
    }
    CHECK(!validToken(*findSetting("quality.render_scale"), "99"));
    CHECK(!validToken(*findSetting("frames.fps_limit"), "1001"));
}

TEST(schema_defaults_match_the_driver) {
    const Config defaults;
    CHECK(tokenOf(*findSetting("quality.sharpen"), defaults) == "50");
    CHECK(tokenOf(*findSetting("renderer.backend"), defaults) == "dgvoodoo");
    CHECK(tokenOf(*findSetting("debug.log"), defaults) == "normal");
}

TEST(schema_covers_every_key_of_the_shipped_ini) {
    // A new driver key must reach a page: every key of dist/uaro-graphics.ini is here or written by the Screen page.
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/uaro-graphics.ini", std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    const auto keys = readIni(ss.str());
    CHECK(keys.size() >= 14);
    for (const auto& kv : keys) {
        const bool screen = kv.first == "display.mode" || kv.first == "display.monitor" ||
                            kv.first == "display.window_width" || kv.first == "display.window_height";
        CHECK(screen || findSetting(kv.first) != nullptr);
    }
}

TEST(schema_live_keys_are_the_specs) {
    // Spec §2.2: the renderer and allow_tearing wait for a restart, the rest of the pages' keys apply at once.
    for (const IniSetting& s : iniSettings()) {
        const std::string k = sectionKey(s);
        CHECK(appliesNow(k) == (k != "renderer.backend" && k != "frames.allow_tearing"));
    }
}
```

- [ ] **Step 2: Запустить: тесты не собираются**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: ошибка сборки `core/settings/schema.hpp: No such file or directory`.

- [ ] **Step 3: Реализация**

`src/core/settings/schema.hpp`:
```cpp
#pragma once
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::settings {

// The settings app's pages in the order of its list (stage 2 spec §6.1; Sound, Game, Input and dgVoodoo come with
// 0.6.1 and 0.6.2).
enum class Page : uint8_t { Render, Screen, Quality, Frames, Diagnostics, Service };
constexpr int kPageCount = 6;
Text pageName(Page p);

enum class Control : uint8_t { Check, Slider, Choice, Number };

// One key of uaro-graphics.ini the pages edit one to one. The Screen page writes [display] mode, monitor,
// window_width and window_height from its own plan (spec §5.3).
struct IniSetting {
    const char* section;
    const char* key;
    Page page;
    Control control;
    int min;
    int max;
    int step;
    std::vector<const char*> tokens;  // Check: {off, on}; Choice: the file's words in the order of `choices`
    std::vector<Text> choices;        // Choice
    Text label;
    Text hint;
    const char* unit;                 // after a slider's number
};
const std::vector<IniSetting>& iniSettings();
const IniSetting* findSetting(const std::string& sectionKey);
std::string sectionKey(const IniSetting& s);  // "quality.sharpen"
// The value the driver uses for it, as the word the file holds: "1", "150", "sharp".
std::string tokenOf(const IniSetting& s, const Config& c);
// Whether a word is one this setting takes (a number in its range, one of its tokens).
bool validToken(const IniSetting& s, const std::string& token);

}  // namespace uagfx::settings
```

`src/core/settings/schema.cpp`:
```cpp
#include "core/settings/schema.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace uagfx::settings {

Text pageName(Page p) {
    switch (p) {
        case Page::Render: return Text::PageRender;
        case Page::Screen: return Text::PageScreen;
        case Page::Quality: return Text::PageQuality;
        case Page::Frames: return Text::PageFrames;
        case Page::Diagnostics: return Text::PageDiagnostics;
        case Page::Service: break;
    }
    return Text::PageService;
}

const std::vector<IniSetting>& iniSettings() {
    static const std::vector<IniSetting> list{
        {"renderer", "backend", Page::Render, Control::Choice, 0, 0, 0, {"uaro", "dgvoodoo"},
         {Text::RenderUaro, Text::RenderDgVoodoo}, Text::RenderHeading, Text::RenderUaroHint, ""},
        {"display", "high_resolution", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityHighRes,
         Text::QualityHighResHint, ""},
        {"quality", "render_scale", Page::Quality, Control::Slider, 100, 200, 10, {}, {}, Text::QualityRenderScale,
         Text::QualityRenderScaleHint, " %"},
        {"quality", "sharpen", Page::Quality, Control::Slider, 0, 100, 5, {}, {}, Text::QualitySharpen,
         Text::QualitySharpenHint, ""},
        {"display", "ui_filter", Page::Quality, Control::Choice, 0, 0, 0, {"sharp", "smooth"},
         {Text::UiSharp, Text::UiSmooth}, Text::QualityUiFilter, Text::QualityUiFilterHint, ""},
        {"display", "crisp_text", Page::Quality, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::QualityCrispText,
         Text::QualityCrispTextHint, ""},
        {"frames", "vsync", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesVsync,
         Text::FramesVsyncHint, ""},
        {"frames", "fps_limit", Page::Frames, Control::Number, 0, 1000, 1, {}, {}, Text::FramesFpsLimit,
         Text::FramesFpsLimitHint, ""},
        {"frames", "allow_tearing", Page::Frames, Control::Check, 0, 1, 1, {"0", "1"}, {}, Text::FramesTearing,
         Text::FramesTearingHint, ""},
        {"debug", "log", Page::Diagnostics, Control::Check, 0, 1, 1, {"normal", "verbose"}, {}, Text::DiagVerboseLog,
         Text::DiagVerboseLogHint, ""},
    };
    return list;
}

const IniSetting* findSetting(const std::string& sectionKey) {
    for (const IniSetting& s : iniSettings())
        if (sectionKey == std::string(s.section) + "." + s.key) return &s;
    return nullptr;
}

std::string sectionKey(const IniSetting& s) { return std::string(s.section) + "." + s.key; }

std::string tokenOf(const IniSetting& s, const Config& c) {
    const std::string k = sectionKey(s);
    if (k == "renderer.backend") return backendToken(c.backend);
    if (k == "display.high_resolution") return c.highResolution ? "1" : "0";
    if (k == "quality.render_scale") return std::to_string(c.renderScale);
    if (k == "quality.sharpen") return std::to_string(c.sharpen);
    if (k == "display.ui_filter") return uiFilterToken(c.uiFilter);
    if (k == "display.crisp_text") return c.crispText ? "1" : "0";
    if (k == "frames.vsync") return c.vsync ? "1" : "0";
    if (k == "frames.fps_limit") return std::to_string(c.fpsLimit);
    if (k == "frames.allow_tearing") return c.allowTearing ? "1" : "0";
    if (k == "debug.log") return c.logLevel == LogLevel::Debug ? "verbose" : "normal";
    return "";
}

bool validToken(const IniSetting& s, const std::string& token) {
    if (s.control == Control::Slider || s.control == Control::Number) {
        char* end = nullptr;
        const long n = std::strtol(token.c_str(), &end, 10);
        return !token.empty() && *end == '\0' && n >= s.min && n <= s.max;
    }
    return std::any_of(s.tokens.begin(), s.tokens.end(), [&](const char* t) { return token == t; });
}

}  // namespace uagfx::settings
```

- [ ] **Step 4: Запустить: тесты проходят**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `240 tests, 0 failed checks`, сборка без предупреждений.

- [ ] **Step 5: Коммит**

```bash
git add src/core/settings/schema.hpp src/core/settings/schema.cpp tests/test_schema.cpp
git commit -q -m "Ядро: схема ключей uaro-graphics.ini для страниц утилиты

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 10: Слой Windows утилиты — файлы, реестр, система, модель (spec §3.3, §4)

Библиотека `uagfx_app`: перевод UTF-8 ↔ UTF-16 и тексты окна (`wtr`, `wtrf`: в `app` — свои имена, иначе вызов
`tr(Lang, Text)` неоднозначен с ядром); файлы (запись через временный файл и `MoveFileExW`, `.reg` в UTF-16 с BOM,
запуск, буфер обмена); реестр (обе копии ключа игры, путь с `WOW6432Node` или без, запись в личную копию, замена её
при восстановлении, флажки совместимости); система (мониторы в нумерации драйвера через `monitorIndex`, масштаб,
режимы, HDR и версия драйвера из DXGI, рамка окна игры при 96 DPI, права администратора, запущена ли игра); модель —
всё, что видят страницы, ожидающие изменения и запись по правилам spec §4 с резервной копией. Логика — в ядре и
покрыта тестами Tasks 1–9; здесь проверка — сборка без предупреждений (тестов Windows-кода на Linux нет).

**Files:**
- Create: `src/app/text.hpp`, `src/app/text.cpp`, `src/app/files.hpp`, `src/app/files.cpp`, `src/app/registry.hpp`,
  `src/app/registry.cpp`, `src/app/system.hpp`, `src/app/system.cpp`, `src/app/model.hpp`, `src/app/model.cpp`
- Modify: `CMakeLists.txt` (библиотека `uagfx_app`)

**Interfaces:**
- Consumes: всё ядро Tasks 1–9; `win::windowsVersion`, `win::probeD3D12` (`src/win/system_probe.cpp`); `monitorIndex`
  (`core/window_plan.hpp`); `kVersion`.
- Produces (namespace `uagfx::app`): `wide`, `utf8`, `wtr`, `wtrf`, `num`, `scaleText`; `exeFolder`, `readFile`,
  `writeFileAtomic`, `fileExists`, `dirExists`, `listFolder`, `makeFolder`, `removeTree`, `folderWritable`,
  `underProgramFiles`, `timestampName`, `regFileToUtf8`, `utf8ToRegFile`, `launch`, `openInShell`, `copyToClipboard`;
  `GameKeyPaths gameKeyPaths()`, `readPersonal`, `readCommon`, `writePersonal`, `replacePersonal`, `CompatEntry`,
  `readCompat`, `writeUserCompat`; `SystemInfo {windows, d3d12Probed, d3d12, gpuDriver, monitors, frame, elevated}`,
  `probeSystem`, `gameRunning`; `Paths`, `ModuleFile`, `CompatFlag`, `Ddraw`, `Snapshot`, `class Model` (`load`,
  `refreshRunning`, `now`, `system`, `paths`, `token`, `setToken`, `backend`, `pendingConfig`, `screenChoice`, `setScreen`,
  `screenNow`, `screenPlan`, `screenAsSetup`, `screenAsSetupPending`, `setupScreen`, `applyPreset`, `pendingCount`,
  `canRevert`, `apply`, `writeNow`, `revert`, `restore`, `keepNewestBackups`, `resetDefaults`, `removeCompatFlag`,
  `resetRegistryValue`, `launchGame`, `savedLanguage`, `saveLanguage`), `problemText(ScreenProblem)`, `defaultIni()`.

- [ ] **Step 1: Тексты, файлы, реестр, система**

`src/app/text.hpp`:
```cpp
#pragma once
#include <string>
#include <vector>

#include "core/settings/strings.hpp"

namespace uagfx::app {

std::wstring wide(const std::string& utf8);
std::string utf8(const std::wstring& w);
// The page texts (stage 2 spec §6) in the chosen language.
std::wstring wtr(settings::Lang l, settings::Text t);
std::wstring wtrf(settings::Lang l, settings::Text t, const std::vector<std::string>& args);
std::string num(long long v);  // "1920"
std::string scaleText(double s);  // "1.75", the decimal point as the language writes it is not needed: always "."

}  // namespace uagfx::app
```

`src/app/text.cpp`:
```cpp
#include "app/text.hpp"

#include <windows.h>

#include <cstdio>

namespace uagfx::app {

std::wstring wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring out(size_t(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), &out[0], n);
    return out;
}

std::string utf8(const std::wstring& w) {
    if (w.empty()) return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), &out[0], n, nullptr, nullptr);
    return out;
}

std::wstring wtr(settings::Lang l, settings::Text t) { return wide(settings::tr(l, t)); }

std::wstring wtrf(settings::Lang l, settings::Text t, const std::vector<std::string>& args) {
    return wide(settings::fmt(settings::tr(l, t), args));
}

std::string num(long long v) { return std::to_string(v); }

std::string scaleText(double s) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", s);
    std::string out = buf;
    while (out.size() > 1 && out.back() == '0') out.pop_back();
    if (!out.empty() && out.back() == '.') out.pop_back();
    return out;
}

}  // namespace uagfx::app
```

`src/app/files.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <string>
#include <vector>

namespace uagfx::app {

std::wstring exeFolder();  // the folder of uaro-graphics.exe: the client folder
bool readFile(const std::wstring& path, std::string& out);
// Written to "<path>.new" and moved over the old file in one step: a reader never sees half of it (spec §4.5).
bool writeFileAtomic(const std::wstring& path, const std::string& bytes);
bool fileExists(const std::wstring& path);
bool dirExists(const std::wstring& path);
std::vector<std::wstring> listFolder(const std::wstring& dir, bool folders);  // names, sorted
bool makeFolder(const std::wstring& path);   // also when it exists
bool removeTree(const std::wstring& path);
bool folderWritable(const std::wstring& dir);
bool underProgramFiles(const std::wstring& dir);
std::wstring timestampName();  // "2026-10-13-192033", for a backup folder
// .reg files are UTF-16 with a BOM; the core reads and writes UTF-8.
std::string regFileToUtf8(const std::string& bytes);
std::string utf8ToRegFile(const std::string& utf8);
// 0 when started; else the Windows error.
unsigned long launch(const std::wstring& file, const std::wstring& folder);
void openInShell(const std::wstring& path);
bool copyToClipboard(HWND owner, const std::wstring& text);

}  // namespace uagfx::app
```

`src/app/files.cpp`:
```cpp
#include "app/files.hpp"

#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cstring>
#include <cwctype>

#include "app/text.hpp"

namespace uagfx::app {

std::wstring exeFolder() {
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path, n < MAX_PATH ? n : 0);
    const size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

bool readFile(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart < (LONGLONG(1) << 30);
    if (ok) {
        out.assign(size_t(size.QuadPart), '\0');
        DWORD got = 0;
        ok = out.empty() || (ReadFile(f, &out[0], DWORD(out.size()), &got, nullptr) && got == out.size());
    }
    CloseHandle(f);
    return ok;
}

bool writeFileAtomic(const std::wstring& path, const std::string& bytes) {
    const std::wstring temp = path + L".new";
    HANDLE f = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    const bool ok = (bytes.empty() || WriteFile(f, bytes.data(), DWORD(bytes.size()), &put, nullptr)) &&
                    put == bytes.size() && FlushFileBuffers(f);
    CloseHandle(f);
    if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str());
        return false;
    }
    return true;
}

bool fileExists(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dirExists(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<std::wstring> listFolder(const std::wstring& dir, bool folders) {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW d;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        const bool folder = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const std::wstring name = d.cFileName;
        if (folder == folders && name != L"." && name != L"..") out.push_back(name);
    } while (FindNextFileW(h, &d));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

bool makeFolder(const std::wstring& path) { return CreateDirectoryW(path.c_str(), nullptr) || dirExists(path); }

bool removeTree(const std::wstring& path) {
    for (const std::wstring& f : listFolder(path, false)) DeleteFileW((path + L"\\" + f).c_str());
    for (const std::wstring& d : listFolder(path, true)) removeTree(path + L"\\" + d);
    return RemoveDirectoryW(path.c_str()) != 0;
}

bool folderWritable(const std::wstring& dir) {
    const std::wstring probe = dir + L"\\uaro-graphics-write-test.tmp";
    HANDLE f = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    CloseHandle(f);
    return true;
}

bool underProgramFiles(const std::wstring& dir) {
    auto lower = [](std::wstring s) {
        for (wchar_t& ch : s) ch = wchar_t(std::towlower(ch));
        return s;
    };
    const std::wstring d = lower(dir) + L"\\";
    for (int csidl : {CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86}) {
        wchar_t path[MAX_PATH];
        if (SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, path) != S_OK) continue;
        const std::wstring p = lower(path) + L"\\";
        if (d.compare(0, p.size(), p) == 0) return true;
    }
    return false;
}

std::wstring timestampName() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t buf[32];
    wsprintfW(buf, L"%04d-%02d-%02d-%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return buf;
}

std::string regFileToUtf8(const std::string& bytes) {
    if (bytes.size() >= 2 && uint8_t(bytes[0]) == 0xFF && uint8_t(bytes[1]) == 0xFE) {
        std::wstring w((bytes.size() - 2) / 2, L'\0');
        if (!w.empty()) std::memcpy(&w[0], bytes.data() + 2, w.size() * 2);
        return utf8(w);
    }
    return bytes;
}

std::string utf8ToRegFile(const std::string& text) {
    const std::wstring w = wide(text);
    std::string out = "\xFF\xFE";
    out.append(reinterpret_cast<const char*>(w.data()), w.size() * 2);
    return out;
}

unsigned long launch(const std::wstring& file, const std::wstring& folder) {
    SHELLEXECUTEINFOW e{};
    e.cbSize = sizeof e;
    e.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    e.lpVerb = L"open";
    e.lpFile = file.c_str();
    e.lpDirectory = folder.c_str();
    e.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&e) ? 0 : GetLastError();
}

void openInShell(const std::wstring& path) {
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool copyToClipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    bool ok = false;
    if (h) {
        if (void* p = GlobalLock(h)) {
            std::memcpy(p, text.c_str(), bytes);
            GlobalUnlock(h);
            ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
        }
        if (!ok) GlobalFree(h);
    }
    CloseClipboard();
    return ok;
}

}  // namespace uagfx::app
```

`src/app/registry.hpp`:
```cpp
#pragma once
#include <string>
#include <vector>

#include "core/settings/game_registry.hpp"

namespace uagfx::app {

// Where the game's key lives (stage 2 spec §2.1). A 32-bit game on 64-bit Windows uses WOW6432Node; the app is
// 32-bit too, but it has a manifest, so Windows does not virtualise it and it opens both copies itself.
struct GameKeyPaths {
    std::wstring personal;    // under HKCU: Software\Classes\VirtualStore\MACHINE\SOFTWARE\[WOW6432Node\]Gravity Soft\Ragnarok
    std::string personalRoot; // the same as a .reg path: HKEY_CURRENT_USER\...
    std::string commonRoot;   // HKEY_LOCAL_MACHINE\SOFTWARE\[WOW6432Node\]Gravity Soft\Ragnarok
};
GameKeyPaths gameKeyPaths();

settings::RegCopy readPersonal();
settings::RegCopy readCommon();
// Writes to the personal copy in order; "" or "NAME (error N)" of the first one that failed.
std::string writePersonal(const std::vector<settings::RegWrite>& writes);
// The personal copy becomes `copy` (a restored backup): its whole tree goes, then every key and value is written.
std::string replacePersonal(const settings::RegCopy& copy);

// AppCompatFlags\Layers values for the game's exe (stage 2 spec §7).
struct CompatEntry {
    bool allUsers = false;  // HKLM: changed only from the file's properties, as administrator
    std::string flags;      // "~ RUNASADMIN"
};
std::vector<CompatEntry> readCompat(const std::wstring& exePath);
// The user's value for the exe; "" deletes it. false when Windows refused.
bool writeUserCompat(const std::wstring& exePath, const std::string& flags);

}  // namespace uagfx::app
```

`src/app/registry.cpp`:
```cpp
#include "app/registry.hpp"

#include <windows.h>

#include "app/text.hpp"
#include "core/settings/compat_flags.hpp"

namespace uagfx::app {
namespace {

using settings::RegCopy;
using settings::RegType;
using settings::RegValue;
using settings::RegWrite;

const wchar_t kLayers[] = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";

bool wow64() {
    BOOL w = FALSE;
    return IsWow64Process(GetCurrentProcess(), &w) && w;
}

RegValue toValue(DWORD type, const std::vector<uint8_t>& data) {
    if (type == REG_SZ) {
        std::wstring w(data.size() / 2, L'\0');
        if (!w.empty()) memcpy(&w[0], data.data(), w.size() * 2);
        const size_t nul = w.find(L'\0');
        if (nul != std::wstring::npos) w.resize(nul);  // the game writes buffers with bytes after the NUL
        return RegValue::sz(utf8(w));
    }
    RegValue v;
    v.type = type;
    v.bytes = data;
    return v;
}

void readTree(HKEY key, const std::string& path, RegCopy& out) {
    auto& values = out.keys[path];
    for (DWORD i = 0;; ++i) {
        wchar_t name[16384];
        DWORD nameLen = 16384;
        DWORD type = 0;
        DWORD size = 0;
        LONG r = RegEnumValueW(key, i, name, &nameLen, nullptr, &type, nullptr, &size);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS && r != ERROR_MORE_DATA) break;
        std::vector<uint8_t> data(size);
        nameLen = 16384;
        r = RegEnumValueW(key, i, name, &nameLen, nullptr, &type, data.empty() ? nullptr : data.data(), &size);
        if (r != ERROR_SUCCESS) continue;
        data.resize(size);
        values[utf8(std::wstring(name, nameLen))] = toValue(type, data);
    }
    for (DWORD i = 0;; ++i) {
        wchar_t name[256];
        DWORD nameLen = 256;
        if (RegEnumKeyExW(key, i, name, &nameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        HKEY sub = nullptr;
        if (RegOpenKeyExW(key, name, 0, KEY_READ, &sub) != ERROR_SUCCESS) continue;
        const std::string child = utf8(std::wstring(name, nameLen));
        readTree(sub, path.empty() ? child : path + "\\" + child, out);
        RegCloseKey(sub);
    }
}

RegCopy readCopy(HKEY root, const std::wstring& path, REGSAM view) {
    RegCopy copy;
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, path.c_str(), 0, KEY_READ | view, &key) != ERROR_SUCCESS) return copy;
    copy.exists = true;
    readTree(key, "", copy);
    RegCloseKey(key);
    return copy;
}

std::wstring keyOf(const std::string& subkey) {
    const std::wstring base = gameKeyPaths().personal;
    return subkey.empty() ? base : base + L"\\" + wide(subkey);
}

LONG setValue(HKEY key, const std::string& name, const RegValue& v) {
    const std::wstring n = wide(name);
    if (v.isSz()) {
        const std::wstring w = wide(v.text);
        return RegSetValueExW(key, n.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(w.c_str()),
                              DWORD((w.size() + 1) * sizeof(wchar_t)));
    }
    return RegSetValueExW(key, n.c_str(), 0, v.type, v.bytes.empty() ? nullptr : v.bytes.data(), DWORD(v.bytes.size()));
}

std::string failure(const std::string& name, LONG r) { return name + " (error " + std::to_string(r) + ")"; }

}  // namespace

GameKeyPaths gameKeyPaths() {
    const bool w = wow64();
    GameKeyPaths p;
    p.personal = std::wstring(L"Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\") +
                 (w ? L"WOW6432Node\\" : L"") + L"Gravity Soft\\Ragnarok";
    p.personalRoot = "HKEY_CURRENT_USER\\" + utf8(p.personal);
    p.commonRoot = std::string("HKEY_LOCAL_MACHINE\\SOFTWARE\\") + (w ? "WOW6432Node\\" : "") + "Gravity Soft\\Ragnarok";
    return p;
}

RegCopy readPersonal() { return readCopy(HKEY_CURRENT_USER, gameKeyPaths().personal, KEY_WOW64_64KEY); }

// The game's own path: a 32-bit process opening HKLM\SOFTWARE gets WOW6432Node by itself.
RegCopy readCommon() { return readCopy(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Gravity Soft\\Ragnarok", KEY_WOW64_32KEY); }

std::string writePersonal(const std::vector<RegWrite>& writes) {
    for (const RegWrite& w : writes) {
        HKEY key = nullptr;
        LONG r = w.op == settings::RegOp::Set
                     ? RegCreateKeyExW(HKEY_CURRENT_USER, keyOf(w.subkey).c_str(), 0, nullptr, 0,
                                       KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key, nullptr)
                     : RegOpenKeyExW(HKEY_CURRENT_USER, keyOf(w.subkey).c_str(), 0, KEY_SET_VALUE | KEY_WOW64_64KEY,
                                     &key);
        if (r == ERROR_FILE_NOT_FOUND && w.op == settings::RegOp::Delete) continue;  // nothing to delete
        if (r != ERROR_SUCCESS) return failure(w.name, r);
        r = w.op == settings::RegOp::Set ? setValue(key, w.name, w.value)
                                         : RegDeleteValueW(key, wide(w.name).c_str());
        RegCloseKey(key);
        if (r != ERROR_SUCCESS && !(r == ERROR_FILE_NOT_FOUND && w.op == settings::RegOp::Delete))
            return failure(w.name, r);
    }
    return "";
}

std::string replacePersonal(const RegCopy& copy) {
    const std::wstring base = gameKeyPaths().personal;
    HKEY parent = nullptr;
    const size_t slash = base.find_last_of(L'\\');
    if (RegOpenKeyExW(HKEY_CURRENT_USER, base.substr(0, slash).c_str(), 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY,
                      &parent) == ERROR_SUCCESS) {
        const LONG r = RegDeleteTreeW(parent, base.substr(slash + 1).c_str());
        RegCloseKey(parent);
        if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) return failure("Ragnarok", r);
    }
    if (!copy.exists) return "";
    std::vector<RegWrite> writes;
    for (const auto& key : copy.keys) {
        HKEY k = nullptr;  // keys without values are made too
        const LONG r = RegCreateKeyExW(HKEY_CURRENT_USER, keyOf(key.first).c_str(), 0, nullptr, 0,
                                       KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr);
        if (r != ERROR_SUCCESS) return failure(key.first.empty() ? "Ragnarok" : key.first, r);
        RegCloseKey(k);
        for (const auto& v : key.second) writes.push_back(RegWrite{key.first, v.first, settings::RegOp::Set, v.second});
    }
    return writePersonal(writes);
}

std::vector<CompatEntry> readCompat(const std::wstring& exePath) {
    std::vector<CompatEntry> out;
    for (bool all : {false, true}) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(all ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, kLayers, 0, KEY_READ | KEY_WOW64_64KEY,
                          &key) != ERROR_SUCCESS)
            continue;
        RegCopy values;
        readTree(key, "", values);
        RegCloseKey(key);
        for (const auto& v : values.keys[""])
            if (v.second.isSz() && settings::samePath(v.first, utf8(exePath)))
                out.push_back(CompatEntry{all, v.second.text});
    }
    return out;
}

bool writeUserCompat(const std::wstring& exePath, const std::string& flags) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kLayers, 0, nullptr, 0, KEY_READ | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr,
                        &key, nullptr) != ERROR_SUCCESS)
        return false;
    // The value's name is the path as Windows wrote it: find it again, whatever its case.
    RegCopy values;
    readTree(key, "", values);
    std::wstring name = exePath;
    for (const auto& v : values.keys[""])
        if (settings::samePath(v.first, utf8(exePath))) name = wide(v.first);
    LONG r;
    if (flags.empty()) {
        r = RegDeleteValueW(key, name.c_str());
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    } else {
        r = setValue(key, utf8(name), RegValue::sz(flags));
    }
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

}  // namespace uagfx::app
```

`src/app/system.hpp`:
```cpp
#pragma once
#include <string>
#include <vector>

#include "core/settings/screen.hpp"
#include "win/system_probe.hpp"

namespace uagfx::app {

// What the Screen and Diagnostics pages need from Windows (stage 2 spec §5, §7), read once at start.
struct SystemInfo {
    win::WindowsVersion windows;
    bool d3d12Probed = false;  // only on Windows 10 and newer
    win::D3D12Probe d3d12;
    std::string gpuDriver;                        // "32.0.15.6094": the main adapter's driver, "" when unknown
    std::vector<settings::MonitorInfo> monitors;  // as the driver numbers them: 0 = the main one
    settings::Frame frame;                        // the game's window frame, in the game's pixels
    bool elevated = false;                        // this app runs as administrator
};
SystemInfo probeSystem();

// Whether uaRO.exe from this folder runs (spec §4.1); a uaRO.exe whose path cannot be read counts as running.
bool gameRunning(const std::wstring& gameExe);

}  // namespace uagfx::app
```

`src/app/system.cpp`:
```cpp
#include "app/system.hpp"

#include <windows.h>

#include <dxgi1_6.h>
#include <tlhelp32.h>

#include <algorithm>

#include "app/text.hpp"
#include "core/settings/compat_flags.hpp"
#include "core/window_plan.hpp"

namespace uagfx::app {
namespace {

using settings::MonitorInfo;
using settings::Size;

struct Found {
    HMONITOR handle;
    MONITORINFOEXW info;
};

BOOL CALLBACK collect(HMONITOR m, HDC, LPRECT, LPARAM out) {
    Found f{m, {}};
    f.info.cbSize = sizeof f.info;
    if (GetMonitorInfoW(m, &f.info)) reinterpret_cast<std::vector<Found>*>(out)->push_back(f);
    return TRUE;
}

// Windows scaling of a monitor in %: GetDpiForMonitor on Windows 8.1 and newer, else the screen's DPI.
int monitorScale(HMONITOR m) {
    using Fn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static const Fn fn = [] {
        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        return shcore ? reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(shcore, "GetDpiForMonitor")))
                      : nullptr;
    }();
    UINT x = 96, y = 96;
    if (!fn || FAILED(fn(m, 0 /* MDT_EFFECTIVE_DPI */, &x, &y))) {
        HDC dc = GetDC(nullptr);
        x = UINT(GetDeviceCaps(dc, LOGPIXELSX));
        ReleaseDC(nullptr, dc);
    }
    return MulDiv(int(x), 100, 96);
}

std::vector<Size> modesOf(const wchar_t* device) {
    std::vector<Size> out;
    DEVMODEW dm{};
    dm.dmSize = sizeof dm;
    for (DWORD i = 0; EnumDisplaySettingsExW(device, i, &dm, 0); ++i) {
        const Size s{int(dm.dmPelsWidth), int(dm.dmPelsHeight)};
        if (dm.dmBitsPerPel == 32 && std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
    }
    std::sort(out.begin(), out.end(),
              [](Size a, Size b) { return long(a.width) * a.height > long(b.width) * b.height; });
    return out;
}

// From DXGI: the monitors whose output shows the HDR10 colour space (HDR on, Windows 10 1703 and newer) and the
// main adapter's driver version.
std::vector<HMONITOR> dxgiFacts(std::string& driver) {
    std::vector<HMONITOR> out;
    HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
    using CreateFn = HRESULT(WINAPI*)(REFIID, void**);
    const auto create =
        dxgi ? reinterpret_cast<CreateFn>(reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory1"))) : nullptr;
    IDXGIFactory1* factory = nullptr;
    if (!create || FAILED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) return out;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT a = 0; factory->EnumAdapters1(a, &adapter) == S_OK; ++a) {
        LARGE_INTEGER v{};
        if (a == 0 && SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &v)))
            driver = std::to_string(HIWORD(v.HighPart)) + "." + std::to_string(LOWORD(v.HighPart)) + "." +
                     std::to_string(HIWORD(v.LowPart)) + "." + std::to_string(LOWORD(v.LowPart));
        IDXGIOutput* output = nullptr;
        for (UINT o = 0; adapter->EnumOutputs(o, &output) == S_OK; ++o) {
            IDXGIOutput6* six = nullptr;
            if (SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), reinterpret_cast<void**>(&six)))) {
                DXGI_OUTPUT_DESC1 d{};
                if (SUCCEEDED(six->GetDesc1(&d)) && d.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
                    out.push_back(d.Monitor);
                six->Release();
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return out;
}

// The frame of the game's window style (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN, registry.md §2)
// at 96 DPI: the game is not DPI-aware, so it measures in those pixels.
settings::Frame gameFrame() {
    const DWORD style = 0x02CA0000;
    RECT r{0, 0, 0, 0};
    using ForDpi = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
    const auto fn = reinterpret_cast<ForDpi>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi")));
    settings::Frame f;
    if (fn && fn(&r, style, FALSE, 0, 96)) {
        f.width = r.right - r.left;
        f.height = r.bottom - r.top;
    } else if (AdjustWindowRectEx(&r, style, FALSE, 0)) {
        HDC dc = GetDC(nullptr);
        const int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        f.width = MulDiv(r.right - r.left, 96, dpi);
        f.height = MulDiv(r.bottom - r.top, 96, dpi);
    }
    return f;
}

bool runsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e{};
    DWORD size = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &e, sizeof e, &size) && e.TokenIsElevated;
    CloseHandle(token);
    return elevated;
}

}  // namespace

SystemInfo probeSystem() {
    SystemInfo s;
    s.windows = win::windowsVersion();
    if (s.windows.major >= 10) {
        s.d3d12 = win::probeD3D12();
        s.d3d12Probed = true;
    }
    std::vector<Found> found;
    EnumDisplayMonitors(nullptr, nullptr, collect, reinterpret_cast<LPARAM>(&found));
    std::vector<bool> primary;
    for (const Found& f : found) primary.push_back((f.info.dwFlags & MONITORINFOF_PRIMARY) != 0);
    const std::vector<HMONITOR> hdr = dxgiFacts(s.gpuDriver);
    for (size_t i = 0; i < found.size(); ++i) {
        const int at = monitorIndex(primary, int(i));
        if (at < 0) continue;
        const Found& f = found[size_t(at)];
        MonitorInfo m;
        m.primary = primary[size_t(at)];
        DEVMODEW dm{};
        dm.dmSize = sizeof dm;
        if (EnumDisplaySettingsExW(f.info.szDevice, ENUM_CURRENT_SETTINGS, &dm, 0)) {
            m.pixels = {int(dm.dmPelsWidth), int(dm.dmPelsHeight)};
            m.refresh = int(dm.dmDisplayFrequency);
        } else {
            m.pixels = {f.info.rcMonitor.right - f.info.rcMonitor.left, f.info.rcMonitor.bottom - f.info.rcMonitor.top};
        }
        // This app is DPI-aware: monitor rectangles are in real pixels.
        m.work = {f.info.rcWork.right - f.info.rcWork.left, f.info.rcWork.bottom - f.info.rcWork.top};
        m.scale = monitorScale(f.handle);
        m.hdr = std::find(hdr.begin(), hdr.end(), f.handle) != hdr.end();
        m.modes = modesOf(f.info.szDevice);
        s.monitors.push_back(m);
    }
    s.frame = gameFrame();
    s.elevated = runsElevated();
    return s;
}

bool gameRunning(const std::wstring& gameExe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W e{};
    e.dwSize = sizeof e;
    bool running = false;
    for (BOOL more = Process32FirstW(snap, &e); more && !running; more = Process32NextW(snap, &e)) {
        if (lstrcmpiW(e.szExeFile, L"uaRO.exe") != 0) continue;
        HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
        if (!p) {
            running = true;  // a game run as administrator: its path cannot be read
            continue;
        }
        wchar_t path[MAX_PATH];
        DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, path, &n))
            running = settings::samePath(utf8(std::wstring(path, n)), utf8(gameExe));
        else
            running = true;
        CloseHandle(p);
    }
    CloseHandle(snap);
    return running;
}

}  // namespace uagfx::app
```

- [ ] **Step 2: Модель**

`src/app/model.hpp`:
```cpp
#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "app/registry.hpp"
#include "app/system.hpp"
#include "core/config.hpp"
#include "core/settings/game_registry.hpp"
#include "core/settings/input_module.hpp"
#include "core/settings/log_digest.hpp"
#include "core/settings/presets.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/screen.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::app {

struct Paths {
    std::wstring dir;  // the client folder
    std::wstring ini, log, game, version, backups, report, ddraw, dgvoodoo;
};

// A dinput DLL in the client folder (stage 2 spec §2.3).
struct ModuleFile {
    std::wstring file;
    bool loaded = false;  // the game loads it
    settings::ModuleInfo info;
    std::wstring iniPath;
    std::string iniText;
    bool iniExists = false;
};

struct CompatFlag {
    bool allUsers = false;
    std::string flag;
};

enum class Ddraw { Ours, Other, None };

// Everything the pages show, read from the client folder and the registry.
struct Snapshot {
    std::string iniText;
    bool iniExists = false;
    Config config;
    settings::GameRegistry registry;
    std::vector<ModuleFile> modules;
    settings::RoextWindow roext;  // of the ROExt the game loads; inactive when there is none
    int roextModule = -1;         // its index in modules
    std::vector<CompatFlag> compat;
    Ddraw ddraw = Ddraw::None;
    std::string installedVersion;  // uaro-graphics\version.txt
    bool dgvoodoo = false;         // dgvoodoo\DDraw.dll
    bool gameRunning = false;
    std::vector<std::wstring> backups;  // folder names, newest first
    settings::LogDigest log;
    bool folderWritable = true;
    bool programFiles = false;
};

// The settings app's state: what is on disk, what the player changed and how it is written (spec §4).
class Model {
public:
    Model();
    void load();             // everything, again
    bool refreshRunning();   // whether the game runs; true when that changed
    const Snapshot& now() const { return now_; }
    const SystemInfo& system() const { return system_; }
    const Paths& paths() const { return paths_; }

    // uaro-graphics.ini keys: the player's change, else the file's value.
    std::string token(const settings::IniSetting& s) const;
    void setToken(const settings::IniSetting& s, const std::string& token);
    Backend backend() const;        // with the player's change
    Config pendingConfig() const;   // the file's settings with the player's changes

    settings::ScreenChoice screenChoice() const;  // the player's, else what the game gets now
    void setScreen(const settings::ScreenChoice& c);
    settings::ScreenNow screenNow() const;
    settings::ScreenPlan screenPlan() const;
    void screenAsSetup();          // "as in setup.exe": the personal screen values go
    bool screenAsSetupPending() const { return screenAsSetup_; }
    std::optional<settings::Size> setupScreen(bool& fullscreen) const;  // setup.exe's, when it differs from the game's

    void applyPreset(settings::Preset p);
    size_t pendingCount() const;
    bool canRevert() const { return pendingCount() > 0 || !wroteNow_.empty(); }

    // Writing (spec §4). Each returns "" or what went wrong, in the language given.
    std::wstring apply(settings::Lang l);
    // While the game runs, one key of uaro-graphics.ini now (a slider let go, spec §6); "Undo" brings it back.
    std::wstring writeNow(const settings::IniSetting& s, const std::string& token, settings::Lang l);
    std::wstring revert(settings::Lang l);
    std::wstring restore(const std::wstring& backup, settings::Lang l);
    void keepNewestBackups(size_t count);
    std::wstring resetDefaults(settings::Lang l);
    std::wstring removeCompatFlag(const CompatFlag& f, settings::Lang l);
    std::wstring resetRegistryValue(const std::string& name, settings::Lang l);
    std::wstring launchGame(settings::Lang l);
    std::optional<settings::Lang> savedLanguage() const;
    void saveLanguage(settings::Lang l);  // the app's own key: no backup for it

private:
    std::string baseIni() const;  // the file, or the shipped defaults when there is none
    std::wstring writeIni(const std::vector<settings::IniEdit>& edits, settings::Lang l);
    std::wstring backupOnce(settings::Lang l);
    std::vector<settings::IniEdit> pendingIniEdits() const;

    Paths paths_;
    SystemInfo system_;
    Snapshot now_;
    std::map<std::string, std::string> ini_;  // "section.key" -> the player's word
    std::optional<settings::ScreenChoice> screen_;
    bool screenAsSetup_ = false;
    std::map<std::string, std::string> wroteNow_;  // "section.key" -> the word before the first writeNow
    bool backedUp_ = false;
};

// What the Screen page says about a plan it cannot write.
settings::Text problemText(settings::ScreenProblem p);

// The text of dist/uaro-graphics.ini built into the app (resource DEFAULT_INI).
std::string defaultIni();

}  // namespace uagfx::app
```

`src/app/model.cpp`:
```cpp
#include "app/model.hpp"

#include <windows.h>

#include <algorithm>
#include <set>

#include "app/files.hpp"
#include "app/text.hpp"
#include "core/settings/compat_flags.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/reg_file.hpp"

namespace uagfx::app {

using namespace settings;

namespace {

std::string firstLine(const std::string& s) {
    const size_t nl = s.find_first_of("\r\n");
    return nl == std::string::npos ? s : s.substr(0, nl);
}

IniEdit editOf(const std::string& sectionKey, const std::string& value) {
    const size_t dot = sectionKey.find('.');
    return IniEdit{sectionKey.substr(0, dot), sectionKey.substr(dot + 1), value};
}

int dword(const RegCopy& c, const char* name) {
    const RegValue* v = c.find("", name);
    return v && v->asDword() ? int(*v->asDword()) : 0;
}

}  // namespace

Text problemText(ScreenProblem p) {
    switch (p) {
        case ScreenProblem::TooBig: return Text::ScreenTooBig;
        case ScreenProblem::TooSmall: return Text::ScreenTooSmall;
        case ScreenProblem::BadNumber:
        case ScreenProblem::None: break;
    }
    return Text::ScreenBadNumber;
}

std::string defaultIni() {
    HRSRC r = FindResourceW(nullptr, L"DEFAULT_INI", MAKEINTRESOURCEW(10));  // RT_RCDATA
    HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
    const void* p = g ? LockResource(g) : nullptr;
    return p ? std::string(static_cast<const char*>(p), SizeofResource(nullptr, r)) : std::string();
}

Model::Model() {
    paths_.dir = exeFolder();
    paths_.ini = paths_.dir + L"\\uaro-graphics.ini";
    paths_.log = paths_.dir + L"\\uaro-graphics.log";
    paths_.game = paths_.dir + L"\\uaRO.exe";
    paths_.version = paths_.dir + L"\\uaro-graphics\\version.txt";
    paths_.backups = paths_.dir + L"\\uaro-graphics-backup";
    paths_.report = paths_.dir + L"\\uaro-graphics-report.txt";
    paths_.ddraw = paths_.dir + L"\\ddraw.dll";
    paths_.dgvoodoo = paths_.dir + L"\\dgvoodoo\\DDraw.dll";
    system_ = probeSystem();
    load();
}

void Model::load() {
    Snapshot s;
    s.iniExists = readFile(paths_.ini, s.iniText);
    s.config = parseConfig(s.iniText);
    s.registry.personal = readPersonal();
    s.registry.common = readCommon();
    for (const wchar_t* name : {L"dinput.dll", L"dinput8.dll", L"dinput.dll.bak", L"dinput8.dll.bak"}) {
        std::string bytes;
        if (!readFile(paths_.dir + L"\\" + name, bytes)) continue;
        ModuleFile m;
        m.file = name;
        m.loaded = loadedByGame(utf8(name));
        m.info = identifyModule(utf8(name), std::vector<uint8_t>(bytes.begin(), bytes.end()));
        if (m.info.kind != ModuleKind::Other) {
            m.iniPath = paths_.dir + L"\\" + wide(m.info.ini);
            m.iniExists = readFile(m.iniPath, m.iniText);
        }
        if (m.loaded && m.info.kind == ModuleKind::Roext && s.roextModule < 0) {
            s.roext = roextWindow(readIni(m.iniText));
            s.roextModule = int(s.modules.size());
        }
        s.modules.push_back(m);
    }
    for (const CompatEntry& e : readCompat(paths_.game))
        for (const std::string& f : parseLayers(e.flags).flags) s.compat.push_back(CompatFlag{e.allUsers, f});
    std::string bytes;
    if (readFile(paths_.ddraw, bytes))
        s.ddraw = bytes.find("uaRO Graphics") != std::string::npos ? Ddraw::Ours : Ddraw::Other;
    if (readFile(paths_.version, bytes)) s.installedVersion = firstLine(bytes);
    s.dgvoodoo = fileExists(paths_.dgvoodoo);
    s.gameRunning = gameRunning(paths_.game);
    s.backups = listFolder(paths_.backups, true);
    std::reverse(s.backups.begin(), s.backups.end());
    if (readFile(paths_.log, bytes)) s.log = digestLog(bytes);
    s.folderWritable = folderWritable(paths_.dir);
    s.programFiles = underProgramFiles(paths_.dir);
    now_ = s;
}

bool Model::refreshRunning() {
    const bool running = gameRunning(paths_.game);
    if (running == now_.gameRunning) return false;
    now_.gameRunning = running;
    return true;
}

std::string Model::token(const IniSetting& s) const {
    const auto it = ini_.find(sectionKey(s));
    return it != ini_.end() ? it->second : tokenOf(s, now_.config);
}

void Model::setToken(const IniSetting& s, const std::string& token) {
    if (!validToken(s, token)) return;
    if (token == tokenOf(s, now_.config))
        ini_.erase(sectionKey(s));
    else
        ini_[sectionKey(s)] = token;
}

Backend Model::backend() const {
    return token(*findSetting("renderer.backend")) == "uaro" ? Backend::Uaro : Backend::DgVoodoo;
}

std::vector<IniEdit> Model::pendingIniEdits() const {
    std::vector<IniEdit> edits;
    for (const auto& kv : ini_) edits.push_back(editOf(kv.first, kv.second));
    return edits;
}

Config Model::pendingConfig() const { return parseConfig(applyIniEdits(now_.iniText, pendingIniEdits())); }

ScreenNow Model::screenNow() const {
    const auto fs = effectiveValue(now_.registry, "", "ISFULLSCREENMODE");
    const bool full = fs && fs->asDword() && *fs->asDword() != 0;
    const auto w = effectiveValue(now_.registry, "", "WIDTH");
    const auto h = effectiveValue(now_.registry, "", "HEIGHT");
    const Size size{w && w->asDword() ? int(*w->asDword()) : 0, h && h->asDword() ? int(*h->asDword()) : 0};
    return currentScreen(full, size, now_.config, now_.roext, system_.monitors, now_.config.backend);
}

ScreenChoice Model::screenChoice() const {
    if (screen_) return *screen_;
    ScreenNow n = screenNow();
    if ((n.game.width <= 0 || n.game.height <= 0) && !system_.monitors.empty()) {  // no settings yet (spec §4.3)
        const MonitorInfo& m = system_.monitors[0];
        n.choice.show = Show::Window;
        n.choice.monitor = 0;
        n.choice.ui = m.scale;
        n.choice.window = defaultWindow(m, system_.frame);
    }
    return n.choice;
}

void Model::setScreen(const ScreenChoice& c) {
    screen_ = c;
    screenAsSetup_ = false;
}

ScreenPlan Model::screenPlan() const {
    return planScreen(screenChoice(), system_.monitors, backend(), now_.roext, system_.frame);
}

void Model::screenAsSetup() {
    screen_.reset();
    screenAsSetup_ = true;
}

std::optional<Size> Model::setupScreen(bool& fullscreen) const {
    bool differs = false;
    for (const char* name : {"ISFULLSCREENMODE", "WIDTH", "HEIGHT"}) {
        const RegValue* mine = now_.registry.personal.find("", name);
        const RegValue* common = now_.registry.common.find("", name);
        differs = differs || (mine && common && *mine != *common);
    }
    if (!differs) return std::nullopt;
    fullscreen = dword(now_.registry.common, "ISFULLSCREENMODE") != 0;
    return Size{dword(now_.registry.common, "WIDTH"), dword(now_.registry.common, "HEIGHT")};
}

void Model::applyPreset(Preset p) {
    for (const IniEdit& e : presetEdits(p))
        if (const IniSetting* s = findSetting(e.section + "." + e.key)) setToken(*s, e.value);
}

size_t Model::pendingCount() const {
    const bool screen = screen_ && !(*screen_ == screenNow().choice);
    return ini_.size() + (screen ? 1 : 0) + (screenAsSetup_ ? 1 : 0);
}

std::string Model::baseIni() const { return now_.iniExists ? now_.iniText : defaultIni(); }

std::wstring Model::writeIni(const std::vector<IniEdit>& edits, Lang l) {
    const std::string text = applyIniEdits(baseIni(), edits);
    if (!writeFileAtomic(paths_.ini, text)) return wtrf(l, Text::ErrWriteFile, {"uaro-graphics.ini"});
    now_.iniText = text;
    now_.iniExists = true;
    now_.config = parseConfig(text);
    return L"";
}

std::wstring Model::backupOnce(Lang l) {
    if (backedUp_) return L"";
    const std::wstring dir = paths_.backups + L"\\" + timestampName();
    if (!makeFolder(paths_.backups) || !makeFolder(dir)) return wtrf(l, Text::ErrBackup, {utf8(dir)});
    std::string list;  // files.txt: "<name in the backup>\t<path in the client folder>"
    std::set<std::wstring> kept;
    auto keep = [&](const std::wstring& relative, const std::wstring& saved) {
        std::string bytes;
        if (kept.count(saved) || !readFile(paths_.dir + L"\\" + relative, bytes)) return true;  // nothing to keep
        kept.insert(saved);
        if (!writeFileAtomic(dir + L"\\" + saved, bytes)) return false;
        list += utf8(saved) + "\t" + utf8(relative) + "\r\n";
        return true;
    };
    bool ok = keep(L"uaro-graphics.ini", L"uaro-graphics.ini") && keep(L"dgVoodoo.conf", L"dgVoodoo.conf") &&
              keep(L"dgvoodoo\\dgVoodoo.conf", L"dgvoodoo-dgVoodoo.conf");
    for (const ModuleFile& m : now_.modules)
        if (ok && m.iniExists) ok = keep(wide(m.info.ini), L"module-" + wide(m.info.ini));
    const GameKeyPaths k = gameKeyPaths();
    ok = ok &&
         writeFileAtomic(dir + L"\\personal.reg", utf8ToRegFile(toRegFile(now_.registry.personal, k.personalRoot))) &&
         writeFileAtomic(dir + L"\\common.reg", utf8ToRegFile(toRegFile(now_.registry.common, k.commonRoot))) &&
         writeFileAtomic(dir + L"\\files.txt", list);
    if (!ok) return wtrf(l, Text::ErrBackup, {utf8(dir)});
    backedUp_ = true;
    keepNewestBackups(20);
    return L"";
}

std::wstring Model::apply(Lang l) {
    refreshRunning();
    std::vector<IniEdit> ini = pendingIniEdits();
    std::vector<RegWrite> reg;
    std::vector<IniEdit> roext;
    const bool firstRun = !gameKeyExists(now_.registry);
    if ((screen_ && !(*screen_ == screenNow().choice)) || firstRun) {
        const ScreenPlan p = screenPlan();
        if (p.problem != ScreenProblem::None) return wtrf(l, Text::ErrScreen, {utf8(wtr(l, problemText(p.problem)))});
        if (firstRun) {
            reg = firstRunWrites(p.game.width, p.game.height, p.fullscreen);
        } else {
            auto add = [&](const char* name, uint32_t v) {
                if (const auto w = planWrite(now_.registry, "", name, RegValue::dword(v))) reg.push_back(*w);
            };
            add("ISFULLSCREENMODE", p.fullscreen ? 1 : 0);
            add("WIDTH", uint32_t(p.game.width));
            add("HEIGHT", uint32_t(p.game.height));
            add("BITPERPIXEL", 32);
        }
        if (p.writesIni) {
            ini.push_back({"display", "mode", displayModeToken(p.mode)});
            ini.push_back({"display", "monitor", num(p.monitor)});
            ini.push_back({"display", "window_width", num(p.iniWindow.width)});
            ini.push_back({"display", "window_height", num(p.iniWindow.height)});
        }
        if (p.writesRoext && now_.roextModule >= 0) {
            if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);  // ROExt's ini only when the game is closed
            roext = {{"ROExt", "WindowWidth", num(p.game.width)}, {"ROExt", "WindowHeight", num(p.game.height)}};
        }
    } else if (screenAsSetup_) {
        for (const char* name : {"ISFULLSCREENMODE", "WIDTH", "HEIGHT", "BITPERPIXEL"})
            if (const auto w = planReset(now_.registry, "", name)) reg.push_back(*w);
    }
    if (!ini.empty() || !reg.empty() || !roext.empty()) {
        std::wstring err = backupOnce(l);
        if (!err.empty()) return err;
        if (!reg.empty()) {
            const std::string failed = writePersonal(reg);
            if (!failed.empty()) {
                load();
                return wtrf(l, Text::ErrWriteRegistry, {failed});
            }
        }
        if (!roext.empty()) {
            const ModuleFile& m = now_.modules[size_t(now_.roextModule)];
            if (!writeFileAtomic(m.iniPath, applyIniEdits(m.iniText, roext))) {
                load();
                return wtrf(l, Text::ErrWriteFile, {m.info.ini});
            }
        }
        if (!ini.empty()) {
            err = writeIni(ini, l);
            if (!err.empty()) {
                load();
                return err;
            }
        }
    }
    ini_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    wroteNow_.clear();
    load();
    return L"";
}

std::wstring Model::writeNow(const IniSetting& s, const std::string& token, Lang l) {
    if (!validToken(s, token)) return L"";
    const std::string key = sectionKey(s);
    if (!wroteNow_.count(key)) wroteNow_[key] = tokenOf(s, now_.config);
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    ini_.erase(key);
    return writeIni({{s.section, s.key, token}}, l);
}

std::wstring Model::revert(Lang l) {
    ini_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    if (wroteNow_.empty()) return L"";
    std::vector<IniEdit> back;
    for (const auto& kv : wroteNow_) back.push_back(editOf(kv.first, kv.second));
    wroteNow_.clear();
    return writeIni(back, l);
}

std::wstring Model::restore(const std::wstring& backup, Lang l) {
    refreshRunning();
    if (now_.gameRunning) return wtr(l, Text::ErrGameRunning);
    const std::wstring dir = paths_.backups + L"\\" + backup;
    std::string list, reg;
    RegCopy personal;
    if (!readFile(dir + L"\\files.txt", list) || !readFile(dir + L"\\personal.reg", reg) ||
        !fromRegFile(regFileToUtf8(reg), gameKeyPaths().personalRoot, personal))
        return wtrf(l, Text::ErrRestore, {utf8(backup)});
    backedUp_ = false;  // what is there now goes into a copy of its own first
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    size_t pos = 0;
    while (pos < list.size()) {
        size_t end = list.find("\r\n", pos);
        if (end == std::string::npos) end = list.size();
        const std::string line = list.substr(pos, end - pos);
        pos = end + 2;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string bytes;
        if (!readFile(dir + L"\\" + wide(line.substr(0, tab)), bytes) ||
            !writeFileAtomic(paths_.dir + L"\\" + wide(line.substr(tab + 1)), bytes)) {
            load();
            return wtrf(l, Text::ErrRestore, {line.substr(tab + 1)});
        }
    }
    const std::string failed = replacePersonal(personal);
    ini_.clear();
    screen_.reset();
    screenAsSetup_ = false;
    wroteNow_.clear();
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

void Model::keepNewestBackups(size_t count) {
    const std::vector<std::wstring> all = listFolder(paths_.backups, true);  // oldest first: names are timestamps
    for (size_t i = 0; i + count < all.size(); ++i) removeTree(paths_.backups + L"\\" + all[i]);
    now_.backups = listFolder(paths_.backups, true);
    std::reverse(now_.backups.begin(), now_.backups.end());
}

std::wstring Model::resetDefaults(Lang l) {
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    std::vector<IniEdit> keep{{"renderer", "backend", backendToken(now_.config.backend)}};
    if (const auto lang = savedLanguage()) keep.push_back({"utility", "language", langToken(*lang)});
    const std::string text = applyIniEdits(defaultIni(), keep);
    if (!writeFileAtomic(paths_.ini, text)) return wtrf(l, Text::ErrWriteFile, {"uaro-graphics.ini"});
    ini_.clear();
    wroteNow_.clear();
    load();
    return L"";
}

std::wstring Model::removeCompatFlag(const CompatFlag& f, Lang l) {
    if (f.allUsers) return L"";  // only from uaRO.exe's properties, as administrator
    for (const CompatEntry& e : readCompat(paths_.game)) {
        if (e.allUsers) continue;
        CompatLayers layers = parseLayers(e.flags);
        if (removeLayer(layers, f.flag) && !writeUserCompat(paths_.game, formatLayers(layers)))
            return wtrf(l, Text::ErrWriteRegistry, {"AppCompatFlags"});
    }
    load();
    return L"";
}

std::wstring Model::resetRegistryValue(const std::string& name, Lang l) {
    const auto w = planReset(now_.registry, "", name);
    if (!w) return L"";
    std::wstring err = backupOnce(l);
    if (!err.empty()) return err;
    const std::string failed = writePersonal({*w});
    load();
    return failed.empty() ? L"" : wtrf(l, Text::ErrWriteRegistry, {failed});
}

std::wstring Model::launchGame(Lang l) {
    const unsigned long e = launch(paths_.game, paths_.dir);
    refreshRunning();
    return e ? wtrf(l, Text::ErrLaunch, {num(static_cast<long long>(e))}) : L"";
}

std::optional<Lang> Model::savedLanguage() const {
    const auto v = readIni(now_.iniText);
    const auto it = v.find("utility.language");
    return it == v.end() ? std::nullopt : parseLang(it->second);
}

void Model::saveLanguage(Lang l) { writeIni({{"utility", "language", langToken(l)}}, l); }

}  // namespace uagfx::app
```

- [ ] **Step 3: Цель сборки**

В `CMakeLists.txt` заменить:
```cmake
    target_link_options(ddraw PRIVATE -static -static-libgcc -static-libstdc++ -s)
else()
```
на:
```cmake
    target_link_options(ddraw PRIVATE -static -static-libgcc -static-libstdc++ -s)

    # The settings app's Windows layer (stage 2 spec §3.3): files, registry, system, its model.
    add_library(uagfx_app STATIC src/app/text.cpp src/app/files.cpp src/app/registry.cpp src/app/system.cpp
                src/app/model.cpp src/win/system_probe.cpp)
    target_include_directories(uagfx_app PUBLIC src)
    target_compile_definitions(uagfx_app PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
                               WIDL_EXPLICIT_AGGREGATE_RETURNS)
    target_compile_options(uagfx_app PRIVATE -Wall -Wextra)
    target_link_libraries(uagfx_app PUBLIC uagfx_core comctl32 shell32 ole32 advapi32)
else()
```

- [ ] **Step 4: Запустить: всё собирается без предупреждений**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: ни одной строки `warning`/`error`, `OK: build-win/ddraw.dll: …`, `240 tests, 0 failed checks`.

- [ ] **Step 5: Коммит**

```bash
git add CMakeLists.txt src/app/text.hpp src/app/text.cpp src/app/files.hpp src/app/files.cpp src/app/registry.hpp src/app/registry.cpp src/app/system.hpp src/app/system.cpp src/app/model.hpp src/app/model.cpp
git commit -q -m "Утилита: слой Windows — файлы, две копии реестра, система, модель с записью по правилам и резервной копией

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 11: Окно утилиты и `uaro-graphics.exe` (spec §3.1, §5–§8)

Окно: список разделов слева, язык вверху справа, строка состояния и кнопки «Применить», «Применить и играть»,
«Отменить», «Закрыть» внизу; красная строка, если утилита запущена от имени администратора. Страницы: Рендер,
Качество, Кадры (ключи ini по схеме, ползунки в запущенной игре пишутся, как их отпустят), Экран (spec §5), Диагностика
(таблица с цветом, «Исправить», отчёт), Сервис (пресеты, копии, сброс, версии). Элементы страницы — дети главного окна,
Tab ходит по всем (`IsDialogMessageW`); перестройка страницы отложена сообщением, чтобы не снести элемент, который её
попросил; номер, набранный в поле, сохраняется до перестройки. Ресурсы: значок (рисует `tools/make_icon.py`), манифест,
`dist/uaro-graphics.ini` как `RCDATA`, версия из `src/core/version.cpp`. `tools/check_exe.py` — сначала, он и есть
проверка задачи.

**Files:**
- Create: `tools/check_exe.py`, `tools/make_icon.py`, `src/app/app.ico` (рисует `make_icon.py`), `src/app/app.rc`,
  `src/app/app.manifest`, `src/app/version_rc.h.in`, `src/app/ui.hpp`, `src/app/ui.cpp`, `src/app/page.hpp`,
  `src/app/page.cpp`, `src/app/page_settings.cpp`, `src/app/page_screen.cpp`, `src/app/page_diagnostics.cpp`,
  `src/app/page_service.cpp`, `src/app/main_window.hpp`, `src/app/main_window.cpp`, `src/app/main.cpp`
- Modify: `CMakeLists.txt` (цель `uaro-graphics`)

**Interfaces:**
- Consumes: `Model` и весь слой Task 10; `iniSettings`, `findSetting`, `tokenOf`, `validToken`, `pageName` (Task 9);
  `appliesNow` (Task 2); тексты (Task 8); `planScreen` и др. (Task 5); `makeReport` (Task 7).
- Produces: `uaro-graphics.exe`; `windowDpi`, `makeFont`, `Tone`, `toneColor`, `class Panel`, `class PageHost`, `class Page`,
  `makeSettingsPage`, `makeScreenPage`, `makeDiagnosticsPage`, `makeServicePage`, `controlText`, `class MainWindow`.

- [ ] **Step 1: Проверка программы**

`tools/check_exe.py`:
```python
#!/usr/bin/env python3
"""Checks the cross-built uaro-graphics.exe, the settings app (stage 2 spec §3.1).

- It is a 32-bit Windows GUI program.
- It imports system DLLs of Windows 7 only, and no function Windows 7 lacks: those are found at run time.
- Its manifest asks for no administrator rights (asInvoker) and per-monitor DPI.
"""
import struct
import sys

ALLOWED_DLLS = {"kernel32.dll", "user32.dll", "gdi32.dll", "comctl32.dll", "shell32.dll", "ole32.dll", "advapi32.dll",
                "msvcrt.dll"}
# Newer than Windows 7: a static import of any of them would keep the program from starting there.
NOT_ON_WINDOWS_7 = {"GetDpiForWindow", "GetDpiForSystem", "SystemParametersInfoForDpi", "AdjustWindowRectExForDpi",
                    "SetThreadDpiAwarenessContext", "SetProcessDpiAwarenessContext", "GetDpiForMonitor",
                    "EnableNonClientDpiScaling", "GetSystemMetricsForDpi", "D3D12CreateDevice", "CreateDXGIFactory1",
                    "CreateDXGIFactory2"}


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
        fail("not a 32-bit (i386) program")
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        fail("not PE32")
    if struct.unpack_from("<H", data, opt + 68)[0] != 2:
        fail("not a GUI program (subsystem %d)" % struct.unpack_from("<H", data, opt + 68)[0])
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

    dlls, functions = [], []
    o = off(imp_rva)
    while struct.unpack_from("<I", data, o + 12)[0]:
        lookup, _, _, name, first = struct.unpack_from("<IIIII", data, o)
        dlls.append(cstr(name).lower())
        t = off(lookup or first)
        while True:
            entry = struct.unpack_from("<I", data, t)[0]
            if entry == 0:
                break
            if not entry & 0x80000000:
                functions.append(cstr(entry + 2))
            t += 4
        o += 20
    bad = [d for d in dlls if d not in ALLOWED_DLLS]
    if bad:
        fail("unexpected imports: " + ", ".join(bad))
    late = sorted(set(functions) & NOT_ON_WINDOWS_7)
    if late:
        fail("imports functions Windows 7 does not have: " + ", ".join(late))
    for needed in (b"asInvoker", b"PerMonitorV2", b"Microsoft.Windows.Common-Controls"):
        if needed not in data:
            fail("the manifest has no " + needed.decode())
    print("OK: %s: GUI, %d functions from %s, asInvoker, per-monitor DPI" % (path, len(functions), " ".join(dlls)))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/uaro-graphics.exe")
```

- [ ] **Step 2: Запустить: программы ещё нет**

Run: `python3 tools/check_exe.py build-win/uaro-graphics.exe`
Expected: `FAIL: build-win/uaro-graphics.exe not found`.

- [ ] **Step 3: Ресурсы: значок, манифест, версия**

`tools/make_icon.py`:
```python
#!/usr/bin/env python3
"""Draws src/app/app.ico, the settings app's icon: a monitor on a blue rounded square, 16, 32 and 48 pixels.

Run it again only to change the picture; the .ico is kept in git.
"""
import struct
import sys
from pathlib import Path

BLUE = (0xDF, 0x6C, 0x2D)  # BGR of #2D6CDF
DARK = (0x5A, 0x2E, 0x14)
WHITE = (0xFF, 0xFF, 0xFF)


def pixel(n, x, y):
    """The colour at (x, y) of an n x n icon, top row first; None = transparent."""
    u, v = (x + 0.5) / n, (y + 0.5) / n
    r = 0.18  # corner radius of the square
    cx = min(max(u, r), 1 - r)
    cy = min(max(v, r), 1 - r)
    if (u - cx) ** 2 + (v - cy) ** 2 > r * r:
        return None
    if 0.2 <= u <= 0.8 and 0.24 <= v <= 0.64:  # the screen with its frame
        return WHITE if 0.26 <= u <= 0.74 and 0.3 <= v <= 0.58 else DARK
    if 0.44 <= u <= 0.56 and 0.64 < v <= 0.74:  # the neck
        return DARK
    if 0.32 <= u <= 0.68 and 0.74 < v <= 0.8:  # the foot
        return DARK
    return BLUE


def image(n):
    rows = []
    mask_rows = []
    for y in range(n - 1, -1, -1):  # bottom-up, as BMP
        row = b""
        bits = 0
        mask = b""
        for x in range(n):
            c = pixel(n, x, y)
            row += bytes(c) + b"\xff" if c else b"\0\0\0\0"
            bits = (bits << 1) | (0 if c else 1)
            if x % 8 == 7:
                mask += bytes([bits])
                bits = 0
        mask += b"\0" * ((4 - len(mask) % 4) % 4)
        rows.append(row)
        mask_rows.append(mask)
    header = struct.pack("<IiiHHIIiiII", 40, n, n * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    return header + b"".join(rows) + b"".join(mask_rows)


def main(out):
    sizes = [16, 32, 48]
    images = [image(n) for n in sizes]
    data = struct.pack("<HHH", 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    for n, img in zip(sizes, images):
        data += struct.pack("<BBBBHHII", n, n, 0, 0, 1, 32, len(img), offset)
        offset += len(img)
    Path(out).write_bytes(data + b"".join(images))
    print("wrote", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "src/app/app.ico")
```

Run: `python3 tools/make_icon.py src/app/app.ico` — Expected: `wrote src/app/app.ico` (файл в git).

`src/app/app.manifest`:
```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0" xmlns:asmv3="urn:schemas-microsoft-com:asm.v3">
  <assemblyIdentity type="win32" name="KWEBTeam.uaRO.GraphicsSettings" version="1.0.0.0" processorArchitecture="x86"/>
  <description>uaRO Graphics Settings</description>
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0"
                        processorArchitecture="x86" publicKeyToken="6595b64144ccf1df" language="*"/>
    </dependentAssembly>
  </dependency>
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="asInvoker" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}"/>
      <supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}"/>
      <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"/>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
    </application>
  </compatibility>
  <asmv3:application>
    <asmv3:windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2, PerMonitor</dpiAwareness>
    </asmv3:windowsSettings>
  </asmv3:application>
</assembly>
```

`src/app/version_rc.h.in`:
```c
#define UAGFX_VERSION_TEXT "@UAGFX_VERSION@"
#define UAGFX_VERSION_NUMBERS @UAGFX_V1@,@UAGFX_V2@,@UAGFX_V3@,0
```

`src/app/app.rc`:
```c
#include <winver.h>

#include "version_rc.h"

// The settings app's icon, manifest (asInvoker, per-monitor DPI, Common Controls 6, Windows 7-11), the shipped
// uaro-graphics.ini for "Reset" and a new file, and its version (stage 2 spec §3.1, §3.3).
1 ICON "app.ico"
1 24 "app.manifest"
DEFAULT_INI RCDATA "uaro-graphics.ini"

VS_VERSION_INFO VERSIONINFO
FILEVERSION UAGFX_VERSION_NUMBERS
PRODUCTVERSION UAGFX_VERSION_NUMBERS
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_APP
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
            VALUE "CompanyName", "KWEB Team"
            VALUE "FileDescription", "uaRO Graphics Settings"
            VALUE "FileVersion", UAGFX_VERSION_TEXT
            VALUE "OriginalFilename", "uaro-graphics.exe"
            VALUE "ProductName", "uaRO Graphics"
            VALUE "ProductVersion", UAGFX_VERSION_TEXT
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
```

- [ ] **Step 4: Раскладка, страницы, окно**

`src/app/ui.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <commctrl.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::app {

int windowDpi(HWND hwnd);  // GetDpiForWindow on Windows 10, else the screen's
inline int px(int at96, int dpi) { return MulDiv(at96, dpi, 96); }
HFONT makeFont(int dpi, bool bold, int percent = 100);  // Windows' message font at that DPI

enum class Tone { Normal, Muted, Good, Warn, Bad };
COLORREF toneColor(Tone t);

// The controls of one page (stage 2 spec §6), laid out from the top in rows: a label on the left, the control on the
// right, notes under it across the page. They are children of the main window, so Tab goes through all of them.
class Panel {
public:
    Panel(HWND owner, HFONT font, HFONT heading, HWND tooltip, int dpi, RECT area);
    ~Panel();
    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;

    HWND heading(const std::wstring& text);
    HWND check(int id, const std::wstring& text, bool on, const std::wstring& hint, bool enabled = true);
    // A radio button in the control column; `label` (the group's name) goes to the label column of its row.
    HWND radio(int id, const std::wstring& label, const std::wstring& text, bool on, bool first, const std::wstring& hint,
               bool enabled = true);
    HWND choice(int id, const std::wstring& label, const std::vector<std::wstring>& items, int selected,
                const std::wstring& hint, bool enabled = true);
    // A trackbar with its number on the right; the number is control id + 1.
    HWND slider(int id, const std::wstring& label, int lo, int hi, int step, int value, const std::wstring& valueText,
                const std::wstring& hint, bool enabled = true);
    HWND number(int id, const std::wstring& label, const std::wstring& value, const std::wstring& hint,
                bool enabled = true);
    // Two numbers with "×" between them: ids id and id + 1.
    HWND pair(int id, const std::wstring& label, int a, int b, const std::wstring& hint, bool enabled = true);
    HWND note(const std::wstring& text, Tone tone = Tone::Muted);
    HWND buttons(const std::vector<std::pair<int, std::wstring>>& list, bool enabled = true);
    HWND listBox(int id, int rows);
    HWND listView(int id, const std::vector<std::pair<std::wstring, int>>& columns, int rows);
    void gap(int at96 = 8);
    bool toneOf(HWND ctl, Tone& tone) const;

private:
    HWND make(const wchar_t* cls, const std::wstring& text, DWORD style, int x, int y, int w, int h, int id,
              DWORD ex = 0);
    HWND label(const std::wstring& text, int height);
    void tip(HWND ctl, const std::wstring& hint);
    int textHeight(const std::wstring& text, int width) const;
    int at(int v) const { return px(v, dpi_); }

    HWND owner_;
    HFONT font_;
    HFONT heading_;
    HWND tooltip_;
    int dpi_;
    RECT area_;
    int y_;
    std::vector<HWND> made_;
    std::map<HWND, Tone> tones_;
};

}  // namespace uagfx::app
```

`src/app/ui.cpp`:
```cpp
#include "app/ui.hpp"

namespace uagfx::app {
namespace {

constexpr int kLabel = 200;  // the label column, at 96 DPI
constexpr int kRow = 26;
constexpr int kGap = 6;

}  // namespace

int windowDpi(HWND hwnd) {
    using Fn = UINT(WINAPI*)(HWND);
    static const Fn fn =
        reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow")));
    if (fn && hwnd) return int(fn(hwnd));
    HDC dc = GetDC(nullptr);
    const int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    return dpi;
}

HFONT makeFont(int dpi, bool bold, int percent) {
    NONCLIENTMETRICSW m{};
    m.cbSize = sizeof m;
    using Fn = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
    static const Fn forDpi = reinterpret_cast<Fn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi")));
    LOGFONTW lf{};
    if (forDpi && forDpi(SPI_GETNONCLIENTMETRICS, sizeof m, &m, 0, UINT(dpi))) {
        lf = m.lfMessageFont;
    } else if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof m, &m, 0)) {
        lf = m.lfMessageFont;
        HDC dc = GetDC(nullptr);
        const int screen = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        lf.lfHeight = MulDiv(lf.lfHeight, dpi, screen);
    } else {
        lf.lfHeight = -MulDiv(9, dpi, 72);
        lstrcpyW(lf.lfFaceName, L"Segoe UI");
    }
    lf.lfHeight = MulDiv(lf.lfHeight, percent, 100);
    if (bold) lf.lfWeight = FW_SEMIBOLD;
    return CreateFontIndirectW(&lf);
}

COLORREF toneColor(Tone t) {
    switch (t) {
        case Tone::Muted: return RGB(96, 96, 96);
        case Tone::Good: return RGB(16, 124, 16);
        case Tone::Warn: return RGB(157, 93, 0);
        case Tone::Bad: return RGB(196, 43, 28);
        case Tone::Normal: break;
    }
    return GetSysColor(COLOR_WINDOWTEXT);
}

Panel::Panel(HWND owner, HFONT font, HFONT heading, HWND tooltip, int dpi, RECT area)
    : owner_(owner), font_(font), heading_(heading), tooltip_(tooltip), dpi_(dpi), area_(area), y_(area.top) {}

Panel::~Panel() {
    for (HWND h : made_) {
        if (tooltip_) {
            TOOLINFOW ti{};
            ti.cbSize = sizeof ti;
            ti.hwnd = owner_;
            ti.uId = reinterpret_cast<UINT_PTR>(h);
            SendMessageW(tooltip_, TTM_DELTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
        }
        DestroyWindow(h);
    }
}

HWND Panel::make(const wchar_t* cls, const std::wstring& text, DWORD style, int x, int y, int w, int h, int id,
                 DWORD ex) {
    HWND c = CreateWindowExW(ex, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, x, y, w, h, owner_,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    made_.push_back(c);
    return c;
}

void Panel::tip(HWND ctl, const std::wstring& hint) {
    if (!tooltip_ || hint.empty()) return;
    TOOLINFOW ti{};
    ti.cbSize = sizeof ti;
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = owner_;
    ti.uId = reinterpret_cast<UINT_PTR>(ctl);
    ti.lpszText = const_cast<wchar_t*>(hint.c_str());
    SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
}

int Panel::textHeight(const std::wstring& text, int width) const {
    HDC dc = GetDC(owner_);
    HGDIOBJ old = SelectObject(dc, font_);
    RECT r{0, 0, width, 0};
    DrawTextW(dc, text.c_str(), int(text.size()), &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    ReleaseDC(owner_, dc);
    return r.bottom - r.top;
}

HWND Panel::label(const std::wstring& text, int height) {
    return make(L"STATIC", text, SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX, area_.left, y_, at(kLabel) - at(8), height, 0);
}

HWND Panel::heading(const std::wstring& text) {
    const int h = at(28);
    HWND c = make(L"STATIC", text, SS_LEFT | SS_NOPREFIX, area_.left, y_, area_.right - area_.left, h, 0);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(heading_), TRUE);
    y_ += h + at(kGap);
    return c;
}

HWND Panel::check(int id, const std::wstring& text, bool on, const std::wstring& hint, bool enabled) {
    HWND c = make(L"BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP, area_.left, y_, area_.right - area_.left, at(kRow), id);
    SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

HWND Panel::radio(int id, const std::wstring& text0, const std::wstring& text, bool on, bool first,
                  const std::wstring& hint, bool enabled) {
    if (!text0.empty()) label(text0, at(kRow));
    HWND c = make(L"BUTTON", text, BS_AUTORADIOBUTTON | WS_TABSTOP | (first ? WS_GROUP : 0), area_.left + at(kLabel),
                  y_, area_.right - area_.left - at(kLabel), at(kRow), id);
    SendMessageW(c, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow);
    return c;
}

HWND Panel::choice(int id, const std::wstring& text, const std::vector<std::wstring>& items, int selected,
                   const std::wstring& hint, bool enabled) {
    label(text, at(kRow));
    HWND c = make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_,
                  area_.right - area_.left - at(kLabel), at(300), id);
    for (const std::wstring& item : items) SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
    SendMessageW(c, CB_SETCURSEL, WPARAM(selected), 0);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

HWND Panel::slider(int id, const std::wstring& text, int lo, int hi, int step, int value, const std::wstring& valueText,
                   const std::wstring& hint, bool enabled) {
    label(text, at(kRow + 4));
    const int width = area_.right - area_.left - at(kLabel) - at(70);
    HWND c = make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, area_.left + at(kLabel), y_, width,
                  at(kRow + 4), id);
    SendMessageW(c, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi));
    SendMessageW(c, TBM_SETTICFREQ, WPARAM(step), 0);
    SendMessageW(c, TBM_SETLINESIZE, 0, LPARAM(step));
    SendMessageW(c, TBM_SETPAGESIZE, 0, LPARAM(step * 2));
    SendMessageW(c, TBM_SETPOS, TRUE, LPARAM(value));
    EnableWindow(c, enabled);
    tip(c, hint);
    make(L"STATIC", valueText, SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX, area_.left + at(kLabel) + width + at(8), y_,
         at(62), at(kRow + 4), id + 1);
    y_ += at(kRow + 4) + at(kGap);
    return c;
}

HWND Panel::number(int id, const std::wstring& text, const std::wstring& value, const std::wstring& hint, bool enabled) {
    label(text, at(kRow));
    HWND c = make(L"EDIT", value, ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, area_.left + at(kLabel), y_, at(90), at(kRow),
                  id, WS_EX_CLIENTEDGE);
    EnableWindow(c, enabled);
    tip(c, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

HWND Panel::pair(int id, const std::wstring& text, int a, int b, const std::wstring& hint, bool enabled) {
    label(text, at(kRow));
    const int x = area_.left + at(kLabel);
    HWND c = make(L"EDIT", std::to_wstring(a), ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, x, y_, at(80), at(kRow), id,
                  WS_EX_CLIENTEDGE);
    make(L"STATIC", L"×", SS_CENTER | SS_CENTERIMAGE, x + at(80), y_, at(24), at(kRow), 0);
    HWND d = make(L"EDIT", std::to_wstring(b), ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, x + at(104), y_, at(80),
                  at(kRow), id + 1, WS_EX_CLIENTEDGE);
    EnableWindow(c, enabled);
    EnableWindow(d, enabled);
    tip(c, hint);
    tip(d, hint);
    y_ += at(kRow) + at(kGap);
    return c;
}

HWND Panel::note(const std::wstring& text, Tone tone) {
    const int width = area_.right - area_.left;
    const int h = textHeight(text, width);
    HWND c = make(L"STATIC", text, SS_LEFT | SS_NOPREFIX, area_.left, y_, width, h, 0);
    tones_[c] = tone;
    y_ += h + at(kGap);
    return c;
}

HWND Panel::buttons(const std::vector<std::pair<int, std::wstring>>& list, bool enabled) {
    HWND first = nullptr;
    int x = area_.left;
    HDC dc = GetDC(owner_);
    HGDIOBJ old = SelectObject(dc, font_);
    for (const auto& b : list) {
        SIZE s{};
        GetTextExtentPoint32W(dc, b.second.c_str(), int(b.second.size()), &s);
        const int w = s.cx + at(28);
        HWND c = make(L"BUTTON", b.second, BS_PUSHBUTTON | WS_TABSTOP, x, y_, w, at(kRow + 4), b.first);
        EnableWindow(c, enabled);
        if (!first) first = c;
        x += w + at(8);
    }
    SelectObject(dc, old);
    ReleaseDC(owner_, dc);
    y_ += at(kRow + 4) + at(kGap);
    return first;
}

HWND Panel::listBox(int id, int rows) {
    const int h = at(rows * 20 + 8);
    HWND c = make(L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP | LBS_NOINTEGRALHEIGHT, area_.left, y_,
                  area_.right - area_.left, h, id, WS_EX_CLIENTEDGE);
    y_ += h + at(kGap);
    return c;
}

HWND Panel::listView(int id, const std::vector<std::pair<std::wstring, int>>& columns, int rows) {
    const int h = at(rows * 22 + 28);
    HWND c = make(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_TABSTOP,
                  area_.left, y_, area_.right - area_.left, h, id, WS_EX_CLIENTEDGE);
    SendMessageW(c, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
    for (size_t i = 0; i < columns.size(); ++i) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.pszText = const_cast<wchar_t*>(columns[i].first.c_str());
        col.cx = at(columns[i].second);
        SendMessageW(c, LVM_INSERTCOLUMNW, WPARAM(i), reinterpret_cast<LPARAM>(&col));
    }
    y_ += h + at(kGap);
    return c;
}

void Panel::gap(int at96) { y_ += at(at96); }

bool Panel::toneOf(HWND ctl, Tone& tone) const {
    const auto it = tones_.find(ctl);
    if (it == tones_.end()) return false;
    tone = it->second;
    return true;
}

}  // namespace uagfx::app
```

`src/app/page.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <commctrl.h>

#include <memory>
#include <string>

#include "app/model.hpp"
#include "app/text.hpp"
#include "app/ui.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::app {

// What a page asks of the main window.
class PageHost {
public:
    virtual ~PageHost() = default;
    virtual Model& model() = 0;
    virtual settings::Lang lang() const = 0;
    virtual HWND window() const = 0;
    virtual void changed() = 0;  // the player's changes changed: the status line and the buttons
    virtual void rebuild() = 0;  // the page again: it shows values that depend on others
    virtual void reload() = 0;   // the stores were written: everything is read again and every page rebuilt
    virtual void message(const std::wstring& text, bool error) = 0;
};

// One page of the list on the left (stage 2 spec §6.1). Its controls' ids start at 1000.
class Page {
public:
    explicit Page(PageHost& host) : host_(host) {}
    virtual ~Page() = default;
    virtual void build(Panel& p) = 0;
    virtual void command(int id, int code) {
        (void)id;
        (void)code;
    }
    virtual void scroll(int id, int code, int pos) {
        (void)id;
        (void)code;
        (void)pos;
    }
    virtual LRESULT notify(const NMHDR* n) {
        (void)n;
        return 0;
    }

protected:
    std::wstring t(settings::Text x) const { return wtr(host_.lang(), x); }
    std::wstring tf(settings::Text x, const std::vector<std::string>& args) const {
        return wtrf(host_.lang(), x, args);
    }
    PageHost& host_;
};

std::unique_ptr<Page> makeSettingsPage(PageHost& host, settings::Page which);  // Render, Quality, Frames
std::unique_ptr<Page> makeScreenPage(PageHost& host);
std::unique_ptr<Page> makeDiagnosticsPage(PageHost& host);
std::unique_ptr<Page> makeServicePage(PageHost& host);

// The window text of a control, as UTF-8 (number fields).
std::string controlText(HWND ctl);

}  // namespace uagfx::app
```

`src/app/page.cpp`:
```cpp
#include "app/page.hpp"

namespace uagfx::app {

std::string controlText(HWND ctl) {
    wchar_t buf[256];
    const int n = GetWindowTextW(ctl, buf, 256);
    return utf8(std::wstring(buf, size_t(n > 0 ? n : 0)));
}

}  // namespace uagfx::app
```

`src/app/page_settings.cpp`:
```cpp
#include <cstdio>
#include <cstdlib>

#include "app/page.hpp"
#include "core/live_config.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kUaro = 1001;
constexpr int kDgVoodoo = 1002;
constexpr int kRows = 1100;  // + 4 x the setting's index in iniSettings()

std::string hresult(long hr) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

// Render, Quality and Frames (stage 2 spec §6.1): uaro-graphics.ini keys one to one.
class SettingsPage : public Page {
public:
    SettingsPage(PageHost& host, settings::Page which) : Page(host), which_(which) {}

    void build(Panel& p) override {
        p.heading(t(pageName(which_)));
        if (which_ == settings::Page::Render)
            buildRender(p);
        else
            buildRows(p);
    }

    void command(int id, int code) override {
        Model& m = host_.model();
        if ((id == kUaro || id == kDgVoodoo) && code == BN_CLICKED) {
            m.setToken(*findSetting("renderer.backend"), id == kUaro ? "uaro" : "dgvoodoo");
            host_.changed();
            return;
        }
        const IniSetting* s = settingOf(id);
        if (!s) return;
        HWND ctl = GetDlgItem(host_.window(), id);
        if (s->control == Control::Check && code == BN_CLICKED) {
            m.setToken(*s, s->tokens[SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0]);
        } else if (s->control == Control::Choice && code == CBN_SELCHANGE) {
            const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && size_t(sel) < s->tokens.size()) m.setToken(*s, s->tokens[size_t(sel)]);
        } else if (s->control == Control::Number && code == EN_KILLFOCUS) {
            const std::string text = controlText(ctl);
            if (validToken(*s, text))
                m.setToken(*s, text);
            else
                host_.rebuild();  // back to the value it had
        } else {
            return;
        }
        host_.changed();
    }

    void scroll(int id, int code, int pos) override {
        const IniSetting* s = settingOf(id);
        if (!s || s->control != Control::Slider) return;
        Model& m = host_.model();
        const int step = s->step > 0 ? s->step : 1;
        int v = (pos + step / 2) / step * step;
        v = v < s->min ? s->min : v > s->max ? s->max : v;
        if (v != pos) SendMessageW(GetDlgItem(host_.window(), id), TBM_SETPOS, TRUE, v);
        const std::string token = std::to_string(v);
        SetDlgItemTextW(host_.window(), id + 1, wide(token + s->unit).c_str());
        m.setToken(*s, token);
        // While the game runs a slider applies when it is let go (spec §6): the driver reads the ini within a second.
        if (code == TB_ENDTRACK && m.now().gameRunning && appliesNow(sectionKey(*s))) {
            const std::wstring err = m.writeNow(*s, token, host_.lang());
            if (!err.empty()) host_.message(err, true);
        }
        host_.changed();
    }

private:
    static const IniSetting* settingOf(int id) {
        if (id < kRows) return nullptr;
        const size_t i = size_t(id - kRows) / 4;
        return (id - kRows) % 4 == 0 && i < iniSettings().size() ? &iniSettings()[i] : nullptr;
    }

    void buildRender(Panel& p) {
        Model& m = host_.model();
        const SystemInfo& sys = m.system();
        const bool win10 = sys.windows.major >= 10;
        const bool d3d12 = sys.d3d12Probed && sys.d3d12.ok;
        const bool uaro = m.token(*findSetting("renderer.backend")) == "uaro";
        p.radio(kUaro, t(Text::RenderHeading), t(Text::RenderUaro), uaro, true, t(Text::RenderUaroHint),
                (win10 && d3d12) || uaro);
        p.radio(kDgVoodoo, L"", t(Text::RenderDgVoodoo), !uaro, false, t(Text::RenderDgVoodooHint));
        p.gap();
        if (!win10)
            p.note(t(Text::RenderNeedsWindows10), Tone::Bad);
        else if (!d3d12)
            p.note(tf(Text::RenderNoD3D12, {hresult(sys.d3d12.hr)}), Tone::Bad);
        p.note(t(Text::AfterRestart));
        p.gap();
        const Snapshot& now = m.now();
        if (now.ddraw == Ddraw::Ours) {
            const std::string v = !now.installedVersion.empty() ? now.installedVersion
                                  : !now.log.version.empty()     ? now.log.version
                                                                 : utf8(t(Text::Unknown));
            p.note(tf(Text::InstalledOurs, {v}), Tone::Good);
        } else {
            p.note(t(Text::InstalledNoDriver), Tone::Bad);
        }
        p.note(t(now.dgvoodoo ? Text::DgVoodooFound : Text::DgVoodooMissing), now.dgvoodoo ? Tone::Good : Tone::Warn);
        p.gap();
        p.note(t(Text::PauseNote));
    }

    void buildRows(Panel& p) {
        Model& m = host_.model();
        const std::vector<IniSetting>& all = iniSettings();
        for (size_t i = 0; i < all.size(); ++i) {
            const IniSetting& s = all[i];
            if (s.page != which_) continue;
            const int id = kRows + int(i) * 4;
            const std::string token = m.token(s);
            switch (s.control) {
                case Control::Check:
                    p.check(id, t(s.label), token == s.tokens[1], t(s.hint));
                    break;
                case Control::Slider:
                    p.slider(id, t(s.label), s.min, s.max, s.step, std::atoi(token.c_str()), wide(token + s.unit),
                             t(s.hint));
                    break;
                case Control::Choice: {
                    std::vector<std::wstring> items;
                    int selected = 0;
                    for (size_t c = 0; c < s.choices.size(); ++c) {
                        items.push_back(t(s.choices[c]));
                        if (token == s.tokens[c]) selected = int(c);
                    }
                    p.choice(id, t(s.label), items, selected, t(s.hint));
                    break;
                }
                case Control::Number:
                    p.number(id, t(s.label), wide(token), t(s.hint));
                    break;
            }
            if (!appliesNow(sectionKey(s))) p.note(t(Text::AfterRestart));
        }
        p.gap();
        p.note(t(m.now().gameRunning ? Text::LiveNoteRunning : Text::LiveNote));
    }

    settings::Page which_;
};

}  // namespace

std::unique_ptr<Page> makeSettingsPage(PageHost& host, settings::Page which) {
    return std::make_unique<SettingsPage>(host, which);
}

}  // namespace uagfx::app
```

`src/app/page_screen.cpp`:
```cpp
#include <algorithm>
#include <cstdlib>

#include "app/page.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kFullscreen = 1201;
constexpr int kWindow = 1202;
constexpr int kMonitor = 1203;
constexpr int kMode = 1204;
constexpr int kUi = 1205;
constexpr int kUiExact = 1206;
constexpr int kSize = 1207;
constexpr int kWidth = 1208;  // kWidth + 1: the height
constexpr int kAsSetup = 1210;

std::string sizeText(Size s) { return num(s.width) + "×" + num(s.height); }

// The Screen page (stage 2 spec §5): how the game is shown, on which monitor, how large its interface is.
class ScreenPage : public Page {
public:
    explicit ScreenPage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        const std::vector<MonitorInfo>& monitors = m.system().monitors;
        const ScreenChoice c = m.screenChoice();
        const bool uaro = m.backend() == Backend::Uaro;
        p.heading(t(Text::PageScreen));
        if (!gameKeyExists(m.now().registry)) p.note(t(Text::ScreenFirstRun), Tone::Warn);
        if (monitors.empty()) return;
        const int index = uaro && c.monitor >= 0 && size_t(c.monitor) < monitors.size() ? c.monitor : 0;
        const MonitorInfo& mon = monitors[size_t(index)];

        p.radio(kFullscreen, t(Text::ScreenShow), t(Text::ScreenFullscreen), c.show == Show::Fullscreen, true, L"");
        p.radio(kWindow, L"", t(Text::ScreenWindow), c.show == Show::Window, false, L"");
        p.gap(6);
        std::vector<std::wstring> items;
        for (size_t i = 0; i < monitors.size(); ++i)
            items.push_back(tf(Text::MonitorItem, {num(long(i) + 1), num(monitors[i].pixels.width),
                                                   num(monitors[i].pixels.height), num(monitors[i].scale)}) +
                            (monitors[i].primary ? t(Text::MonitorMain) : L""));
        p.choice(kMonitor, t(Text::ScreenMonitor), items, index, L"", uaro && monitors.size() > 1);

        if (c.show == Show::Fullscreen && !uaro) {
            modes_ = mon.modes;
            const Size current = c.mode.width > 0 ? c.mode : mon.pixels;
            if (std::find(modes_.begin(), modes_.end(), current) == modes_.end()) modes_.insert(modes_.begin(), current);
            items.clear();
            for (Size s : modes_) items.push_back(wide(sizeText(s)));
            p.choice(kMode, t(Text::ScreenMode), items,
                     int(std::find(modes_.begin(), modes_.end(), current) - modes_.begin()), t(Text::ScreenModeHint));
        } else {
            uis_ = uiSteps();
            const bool fixed = !uaro;  // dgVoodoo in a window: Windows scaling
            const int ui = fixed ? mon.scale : c.ui;
            if (std::find(uis_.begin(), uis_.end(), ui) == uis_.end()) {
                uis_.push_back(ui);
                std::sort(uis_.begin(), uis_.end());
            }
            items.clear();
            for (int u : uis_) items.push_back(wide(num(u) + " %"));
            p.choice(kUi, t(Text::ScreenUi), items, int(std::find(uis_.begin(), uis_.end(), ui) - uis_.begin()),
                     t(Text::ScreenUiHint), !fixed);
            if (fixed)
                p.note(tf(Text::ScreenUiDgVoodoo, {num(mon.scale)}));
            else
                p.number(kUiExact, t(Text::ScreenUiExact), wide(num(c.ui)), t(Text::ScreenUiHint));
        }
        if (c.show == Show::Window) {
            sizes_ = windowSizes(mon, m.system().frame);
            if (c.window.width > 0 && std::find(sizes_.begin(), sizes_.end(), c.window) == sizes_.end())
                sizes_.push_back(c.window);
            const int ui = uaro ? c.ui : mon.scale;
            items.clear();
            int selected = -1;
            for (size_t i = 0; i < sizes_.size(); ++i) {
                const Size s = sizes_[i];
                items.push_back(tf(Text::WindowItem, {num(s.width), num(s.height), num(s.width * 100 / ui),
                                                      num(s.height * 100 / ui)}));
                if (s == c.window) selected = int(i);
            }
            p.choice(kSize, t(Text::ScreenWindowSize), items, selected, t(Text::ScreenWindowSizeHint));
            p.pair(kWidth, L"", c.window.width, c.window.height, t(Text::ScreenWindowSizeHint));
        }

        const ScreenPlan plan = m.screenPlan();
        if (plan.problem != ScreenProblem::None) {
            p.note(t(problemText(plan.problem)), Tone::Bad);
        } else {
            const Size out = c.show == Show::Fullscreen ? mon.pixels : c.window;
            std::wstring line = tf(Text::ScreenResult, {num(plan.game.width), num(plan.game.height), num(out.width),
                                                        num(out.height), scaleText(plan.s)});
            if (integerScale(plan.s)) line += t(Text::ScreenCrispest);
            p.note(line, Tone::Normal);
        }
        if (c.show == Show::Fullscreen) p.note(t(Text::ScreenAltTab));
        p.gap(6);
        const ScreenNow now = m.screenNow();
        if (now.game.width > 0) {
            if (now.choice.show == Show::Window)
                p.note(tf(Text::ScreenNowWindow, {num(now.choice.window.width), num(now.choice.window.height),
                                                  num(now.choice.ui), num(now.game.width), num(now.game.height)}));
            else
                p.note(tf(Text::ScreenNowFullscreen, {num(now.game.width), num(now.game.height), num(now.choice.ui)}));
        }
        bool setupFull = false;
        if (const auto setup = m.setupScreen(setupFull)) {
            p.note(tf(Text::ScreenSetupDiffers, {num(setup->width), num(setup->height),
                                                 utf8(t(setupFull ? Text::ShowFullscreenWord : Text::ShowWindowWord))}),
                   Tone::Warn);
            p.buttons({{kAsSetup, t(Text::ScreenAsSetup)}}, !m.screenAsSetupPending());
        }
        const Snapshot& s = m.now();
        if (s.roext.active && (s.roext.lock || s.roext.size.width > 0) && s.roextModule >= 0) {
            p.note(tf(Text::ScreenRoext, {s.modules[size_t(s.roextModule)].info.ini}));
            if (s.gameRunning && plan.writesRoext) p.note(t(Text::ErrGameRunning), Tone::Warn);  // spec §4.1
        }
        p.note(t(Text::AfterRestart));
    }

    void command(int id, int code) override {
        Model& m = host_.model();
        ScreenChoice c = m.screenChoice();
        HWND ctl = GetDlgItem(host_.window(), id);
        const LRESULT sel = SendMessageW(ctl, CB_GETCURSEL, 0, 0);
        const std::vector<MonitorInfo>& monitors = m.system().monitors;
        if ((id == kFullscreen || id == kWindow) && code == BN_CLICKED) {
            c.show = id == kFullscreen ? Show::Fullscreen : Show::Window;
            if (c.show == Show::Window && c.window.width <= 0 && !monitors.empty())
                c.window = defaultWindow(monitors[0], m.system().frame);
        } else if (id == kMonitor && code == CBN_SELCHANGE && sel >= 0 && size_t(sel) < monitors.size()) {
            c.monitor = int(sel);
            const std::vector<Size> fit = windowSizes(monitors[size_t(sel)], m.system().frame);
            if (c.show == Show::Window && std::find(fit.begin(), fit.end(), c.window) == fit.end())
                c.window = defaultWindow(monitors[size_t(sel)], m.system().frame);
        } else if (id == kMode && code == CBN_SELCHANGE && sel >= 0 && size_t(sel) < modes_.size()) {
            c.mode = modes_[size_t(sel)];
        } else if (id == kUi && code == CBN_SELCHANGE && sel >= 0 && size_t(sel) < uis_.size()) {
            c.ui = uis_[size_t(sel)];
        } else if (id == kUiExact && code == EN_KILLFOCUS) {
            const int v = std::atoi(controlText(ctl).c_str());
            if (v >= kUiMin && v <= kUiMax) c.ui = v;
        } else if (id == kSize && code == CBN_SELCHANGE && sel >= 0 && size_t(sel) < sizes_.size()) {
            c.window = sizes_[size_t(sel)];
        } else if ((id == kWidth || id == kWidth + 1) && code == EN_KILLFOCUS) {
            const int w = std::atoi(controlText(GetDlgItem(host_.window(), kWidth)).c_str());
            const int h = std::atoi(controlText(GetDlgItem(host_.window(), kWidth + 1)).c_str());
            if (w > 0 && h > 0) c.window = Size{w, h};
        } else if (id == kAsSetup && code == BN_CLICKED) {
            m.screenAsSetup();
            host_.changed();
            host_.rebuild();
            return;
        } else {
            return;
        }
        if (!(c == m.screenChoice())) m.setScreen(c);
        host_.changed();
        host_.rebuild();  // the result, the problem and the lists follow the choice
    }

private:
    std::vector<int> uis_;
    std::vector<Size> sizes_;
    std::vector<Size> modes_;
};

}  // namespace

std::unique_ptr<Page> makeScreenPage(PageHost& host) { return std::make_unique<ScreenPage>(host); }

}  // namespace uagfx::app
```

`src/app/page_diagnostics.cpp`:
```cpp
#include <cstdio>
#include <functional>

#include "app/files.hpp"
#include "app/page.hpp"
#include "core/live_config.hpp"
#include "core/settings/compat_flags.hpp"
#include "core/settings/report.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kList = 1301;
constexpr int kFix = 1302;
constexpr int kOpenLog = 1303;
constexpr int kOpenFolder = 1304;
constexpr int kReport = 1305;
constexpr int kRecheck = 1306;
constexpr int kVerbose = 1307;

struct Row {
    std::wstring check;
    std::wstring result;
    Tone tone = Tone::Normal;
    std::function<std::wstring(Lang)> fix;  // empty: nothing to fix here
};

std::string hresult(long hr) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

std::string dwordText(const RegValue* v) {
    return v && v->asDword() ? num(static_cast<long long>(*v->asDword())) : std::string("-");
}

Text effectText(LayerEffect e) {
    switch (e) {
        case LayerEffect::RunAsAdmin: return Text::CompatRunAsAdmin;
        case LayerEffect::OldWindows: return Text::CompatOldWindows;
        case LayerEffect::Dpi: return Text::CompatDpi;
        case LayerEffect::Colors: return Text::CompatColors;
        case LayerEffect::Other: break;
    }
    return Text::CompatOther;
}

// The Diagnostics page (stage 2 spec §7).
class DiagnosticsPage : public Page {
public:
    explicit DiagnosticsPage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        rows_ = rows();
        p.heading(t(Text::PageDiagnostics));
        list_ = p.listView(kList, {{t(Text::DiagCheck), 200}, {t(Text::DiagResult), 440}}, 11);
        for (size_t i = 0; i < rows_.size(); ++i) {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = int(i);
            item.pszText = const_cast<wchar_t*>(rows_[i].check.c_str());
            SendMessageW(list_, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
            item.iSubItem = 1;
            item.pszText = const_cast<wchar_t*>(rows_[i].result.c_str());
            SendMessageW(list_, LVM_SETITEMTEXTW, i, reinterpret_cast<LPARAM>(&item));
        }
        p.buttons({{kFix, t(Text::DiagFix)}, {kRecheck, t(Text::DiagRecheck)}});
        EnableWindow(GetDlgItem(host_.window(), kFix), FALSE);
        p.buttons({{kOpenLog, t(Text::DiagOpenLog)}, {kOpenFolder, t(Text::DiagOpenFolder)}, {kReport, t(Text::DiagReport)}});
        EnableWindow(GetDlgItem(host_.window(), kOpenLog), m.now().log.found);
        p.check(kVerbose, t(Text::DiagVerboseLog), m.token(*findSetting("debug.log")) == "verbose",
                t(Text::DiagVerboseLogHint));
    }

    void command(int id, int code) override {
        Model& m = host_.model();
        if (code != BN_CLICKED) return;
        if (id == kFix) {
            const LRESULT i = SendMessageW(list_, LVM_GETNEXTITEM, WPARAM(-1), LVNI_SELECTED);
            if (i < 0 || size_t(i) >= rows_.size() || !rows_[size_t(i)].fix) return;
            const std::wstring err = rows_[size_t(i)].fix(host_.lang());
            host_.message(err.empty() ? t(Text::DiagFixed) : err, !err.empty());
            host_.reload();
        } else if (id == kRecheck) {
            m.load();
            host_.reload();
        } else if (id == kOpenLog) {
            openInShell(m.paths().log);
        } else if (id == kOpenFolder) {
            openInShell(m.paths().dir);
        } else if (id == kReport) {
            const std::string text = makeReport(facts());
            const bool saved = writeFileAtomic(m.paths().report, text);
            copyToClipboard(host_.window(), wide(text));
            host_.message(saved ? t(Text::DiagReportDone) : tf(Text::ErrWriteFile, {"uaro-graphics-report.txt"}), !saved);
        } else if (id == kVerbose) {
            const IniSetting& s = *findSetting("debug.log");
            const std::string token =
                SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED ? "verbose" : "normal";
            m.setToken(s, token);
            host_.changed();
        }
    }

    LRESULT notify(const NMHDR* n) override {
        if (n->idFrom != kList) return 0;
        if (n->code == NM_CUSTOMDRAW) {
            auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(const_cast<NMHDR*>(n));
            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                const size_t i = size_t(cd->nmcd.dwItemSpec);
                if (i < rows_.size()) cd->clrText = toneColor(rows_[i].tone);
                return CDRF_DODEFAULT;
            }
        } else if (n->code == LVN_ITEMCHANGED) {
            const LRESULT i = SendMessageW(list_, LVM_GETNEXTITEM, WPARAM(-1), LVNI_SELECTED);
            EnableWindow(GetDlgItem(host_.window(), kFix), i >= 0 && size_t(i) < rows_.size() && rows_[size_t(i)].fix);
        }
        return 0;
    }

private:
    std::vector<Row> rows() {
        Model& m = host_.model();
        const SystemInfo& sys = m.system();
        const Snapshot& s = m.now();
        std::vector<Row> out;
        const bool win10 = sys.windows.major >= 10;
        out.push_back({t(Text::DiagWindows),
                       tf(Text::DiagWindowsValue, {num(long(sys.windows.major)), num(long(sys.windows.minor)),
                                                   num(long(sys.windows.build))}) +
                           (win10 ? L"" : t(Text::DiagWindowsOld)),
                       win10 ? Tone::Good : Tone::Warn, {}});
        if (sys.d3d12Probed) {
            out.push_back({t(Text::DiagGpu),
                           tf(Text::DiagGpuValue, {sys.d3d12.adapter, num(static_cast<long long>(sys.d3d12.vramMb)),
                                                   sys.gpuDriver.empty() ? utf8(t(Text::Unknown)) : sys.gpuDriver}),
                           Tone::Normal, {}});
            out.push_back({t(Text::DiagD3D12),
                           sys.d3d12.ok ? t(Text::DiagD3D12Ok) : tf(Text::DiagD3D12Failed, {hresult(sys.d3d12.hr)}),
                           sys.d3d12.ok ? Tone::Good : Tone::Bad, {}});
        }
        for (size_t i = 0; i < sys.monitors.size(); ++i) {
            const MonitorInfo& mon = sys.monitors[i];
            out.push_back({tf(Text::DiagMonitor, {num(long(i) + 1)}),
                           tf(Text::DiagMonitorValue, {num(mon.pixels.width), num(mon.pixels.height), num(mon.refresh),
                                                       num(mon.scale), utf8(t(mon.hdr ? Text::Yes : Text::No))}) +
                               (mon.primary ? t(Text::MonitorMain) : L""),
                           Tone::Normal, {}});
        }
        if (s.ddraw == Ddraw::Ours)
            out.push_back({t(Text::DiagDdraw), wide("uaRO Graphics " + s.installedVersion), Tone::Good, {}});
        else
            out.push_back({t(Text::DiagDdraw), t(s.ddraw == Ddraw::Other ? Text::DiagDdrawOther : Text::DiagDdrawNone),
                           s.ddraw == Ddraw::Other ? Tone::Warn : Tone::Bad, {}});
        out.push_back({L"dgVoodoo", t(s.dgvoodoo ? Text::DiagDgVoodooOk : Text::DiagDgVoodooMissing),
                       s.dgvoodoo ? Tone::Good : (s.config.backend == Backend::DgVoodoo ? Tone::Bad : Tone::Warn), {}});
        if (s.modules.empty()) out.push_back({t(Text::DiagInput), t(Text::DiagInputNone), Tone::Muted, {}});
        for (const ModuleFile& mod : s.modules) {
            std::wstring what = mod.info.kind == ModuleKind::Roext     ? tf(Text::DiagModuleRoext, {mod.info.ini})
                                : mod.info.kind == ModuleKind::UaroPad ? L"uaRO Pad"
                                                                       : t(Text::DiagModuleOther);
            if (!mod.loaded) what += t(Text::DiagModuleOff);
            out.push_back({mod.file, what, mod.loaded ? Tone::Normal : Tone::Muted, {}});
        }
        if (!s.folderWritable)
            out.push_back({t(Text::DiagFolder), t(Text::DiagFolderReadOnly), Tone::Bad, {}});
        else
            out.push_back({t(Text::DiagFolder), t(s.programFiles ? Text::DiagFolderProgramFiles : Text::DiagFolderOk),
                           s.programFiles ? Tone::Warn : Tone::Good, {}});
        if (sys.elevated) out.push_back({t(Text::DiagApp), t(Text::DiagElevated), Tone::Bad, {}});
        out.push_back({t(Text::DiagGame), t(s.gameRunning ? Text::DiagGameRunning : Text::DiagGameClosed),
                       Tone::Normal, {}});
        for (const CompatFlag& f : s.compat) {
            const LayerEffect e = layerEffect(f.flag);
            Row r{tf(Text::DiagCompat, {f.flag, utf8(t(f.allUsers ? Text::DiagCompatAll : Text::DiagCompatUser))}),
                  t(effectText(e)) + (f.allUsers ? t(Text::CompatAllUsersHint) : L""),
                  e == LayerEffect::RunAsAdmin ? Tone::Bad : e == LayerEffect::Other ? Tone::Muted : Tone::Warn, {}};
            if (!f.allUsers) {
                const CompatFlag flag = f;
                r.fix = [&m, flag](Lang l) { return m.removeCompatFlag(flag, l); };
            }
            out.push_back(r);
        }
        if (!gameKeyExists(s.registry)) {
            out.push_back({t(Text::DiagRegistry), t(Text::DiagRegistryNone), Tone::Warn, {}});
        } else {
            out.push_back({t(Text::DiagRegistry),
                           tf(Text::DiagRegistryValue, {utf8(t(s.registry.personal.exists ? Text::Yes : Text::No)),
                                                        utf8(t(s.registry.common.exists ? Text::Yes : Text::No))}),
                           Tone::Normal, {}});
            for (const char* name : {"ISFULLSCREENMODE", "WIDTH", "HEIGHT", "BITPERPIXEL"}) {
                const RegValue* mine = s.registry.personal.find("", name);
                const RegValue* common = s.registry.common.find("", name);
                if (!mine || !common || *mine == *common) continue;
                const std::string n = name;
                out.push_back({wide(n), tf(Text::DiagRegistryDiffers, {dwordText(mine), dwordText(common)}), Tone::Warn,
                               [&m, n](Lang l) { return m.resetRegistryValue(n, l); }});
            }
        }
        if (!s.log.found) {
            out.push_back({t(Text::DiagLog), t(Text::DiagLogNone), Tone::Muted, {}});
        } else {
            if (!s.log.renderer.empty())
                out.push_back({t(Text::DiagLogRenderer), wide(s.log.renderer), Tone::Normal, {}});
            out.push_back({t(Text::DiagLogErrors),
                           s.log.errorCount == 0
                               ? t(Text::DiagLogErrorsNone)
                               : tf(Text::DiagLogErrorsSome, {num(static_cast<long long>(s.log.errorCount)),
                                                              s.log.errors.back()}),
                           s.log.errorCount == 0 ? Tone::Good : Tone::Warn, {}});
            if (!s.log.fps.empty()) out.push_back({t(Text::DiagLogFps), wide(s.log.fps), Tone::Normal, {}});
        }
        return out;
    }

    ReportFacts facts() {
        Model& m = host_.model();
        const SystemInfo& sys = m.system();
        const Snapshot& s = m.now();
        ReportFacts f;
        f.utilityVersion = kVersion;
        f.driverVersion = s.installedVersion;
        f.windows = num(long(sys.windows.major)) + "." + num(long(sys.windows.minor)) + " build " +
                    num(long(sys.windows.build));
        f.gpu = sys.d3d12Probed ? sys.d3d12.adapter + ", " + num(static_cast<long long>(sys.d3d12.vramMb)) +
                                      " MB, driver " + (sys.gpuDriver.empty() ? "unknown" : sys.gpuDriver) +
                                      ", Direct3D 12: " + (sys.d3d12.ok ? "OK" : "failed " + hresult(sys.d3d12.hr))
                                : "not checked (Windows before 10)";
        for (const MonitorInfo& mon : sys.monitors)
            f.monitors.push_back(num(mon.pixels.width) + "x" + num(mon.pixels.height) + ", " + num(mon.refresh) +
                                 " Hz, " + num(mon.scale) + " %, HDR " + (mon.hdr ? "yes" : "no") +
                                 (mon.primary ? ", main" : ""));
        f.iniText = s.iniText;
        const ScreenNow now = m.screenNow();
        f.screen = now.choice.show == Show::Window
                       ? "window " + num(now.choice.window.width) + "x" + num(now.choice.window.height) + ", interface " +
                             num(now.choice.ui) + " %, game " + num(now.game.width) + "x" + num(now.game.height)
                       : "full screen, game " + num(now.game.width) + "x" + num(now.game.height) + ", interface " +
                             num(now.choice.ui) + " %";
        if (now.roextOverride) f.screen += " (ROExt sets the window)";
        for (const ModuleFile& mod : s.modules)
            f.inputModules.push_back(utf8(mod.file) + ": " +
                                     (mod.info.kind == ModuleKind::Roext     ? "ROExt (" + mod.info.ini + ")"
                                      : mod.info.kind == ModuleKind::UaroPad ? std::string("uaRO Pad")
                                                                             : std::string("unknown")) +
                                     (mod.loaded ? "" : ", off"));
        for (const CompatFlag& c : s.compat) f.compatFlags.push_back(std::string(c.allUsers ? "HKLM: " : "HKCU: ") + c.flag);
        for (const Row& r : rows_)
            if (r.tone == Tone::Bad || r.tone == Tone::Warn) f.problems.push_back(utf8(r.check + L": " + r.result));
        f.log = s.log;
        return f;
    }

    std::vector<Row> rows_;
    HWND list_ = nullptr;
};

}  // namespace

std::unique_ptr<Page> makeDiagnosticsPage(PageHost& host) { return std::make_unique<DiagnosticsPage>(host); }

}  // namespace uagfx::app
```

`src/app/page_service.cpp`:
```cpp
#include "app/page.hpp"
#include "core/settings/presets.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kPreset = 1401;  // + Preset
constexpr int kBackups = 1410;
constexpr int kRestore = 1411;
constexpr int kKeepFive = 1412;
constexpr int kReset = 1420;

Text presetName(Preset p) {
    switch (p) {
        case Preset::Maximum: return Text::PresetMaximum;
        case Preset::Balance: return Text::PresetBalance;
        case Preset::Speed: return Text::PresetSpeed;
        case Preset::Plain: break;
    }
    return Text::PresetPlain;
}

// "2026-10-13-192033" -> "2026-10-13 19:20:33"
std::wstring backupLabel(const std::wstring& name) {
    if (name.size() != 17) return name;
    return name.substr(0, 10) + L" " + name.substr(11, 2) + L":" + name.substr(13, 2) + L":" + name.substr(15, 2);
}

// The Service page (stage 2 spec §8, 0.6.0 part): presets, backups, reset, versions.
class ServicePage : public Page {
public:
    explicit ServicePage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        p.heading(t(Text::ServicePresets));
        std::vector<std::pair<int, std::wstring>> presets;
        for (int i = 0; i < kPresetCount; ++i) presets.push_back({kPreset + i, t(presetName(Preset(i)))});
        p.buttons(presets);
        const auto now = presetOf(m.pendingConfig());
        p.note(tf(Text::PresetNote, {utf8(now ? t(presetName(*now)) : t(Text::PresetCustom))}));
        p.gap();
        p.heading(t(Text::ServiceBackups));
        list_ = p.listBox(kBackups, 4);
        for (const std::wstring& b : m.now().backups)
            SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(backupLabel(b).c_str()));
        if (m.now().backups.empty()) p.note(t(Text::BackupNone));
        p.buttons({{kRestore, t(Text::BackupRestore)}, {kKeepFive, t(Text::BackupPrune)}}, !m.now().backups.empty());
        p.note(t(Text::BackupNote));
        p.gap();
        p.heading(t(Text::ServiceReset));
        p.buttons({{kReset, t(Text::ResetButton)}});
        p.gap();
        p.heading(t(Text::ServiceVersions));
        p.note(tf(Text::VersionsValue,
                  {kVersion, m.now().installedVersion.empty() ? utf8(t(Text::Unknown)) : m.now().installedVersion}),
               Tone::Normal);
    }

    void command(int id, int code) override {
        if (code != BN_CLICKED) return;
        Model& m = host_.model();
        const Lang l = host_.lang();
        if (id >= kPreset && id < kPreset + kPresetCount) {
            m.applyPreset(Preset(id - kPreset));
            host_.changed();
            host_.rebuild();
        } else if (id == kRestore) {
            const LRESULT sel = SendMessageW(list_, LB_GETCURSEL, 0, 0);
            if (sel < 0 || size_t(sel) >= m.now().backups.size()) return;
            const std::wstring name = m.now().backups[size_t(sel)];
            const std::wstring ask = tf(Text::BackupAsk, {utf8(backupLabel(name))});
            if (MessageBoxW(host_.window(), ask.c_str(), t(Text::AppTitle).c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
                return;
            const std::wstring err = m.restore(name, l);
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        } else if (id == kKeepFive) {
            m.keepNewestBackups(5);
            host_.reload();
        } else if (id == kReset) {
            if (MessageBoxW(host_.window(), t(Text::ResetAsk).c_str(), t(Text::AppTitle).c_str(),
                            MB_YESNO | MB_ICONQUESTION) != IDYES)
                return;
            const std::wstring err = m.resetDefaults(l);
            if (!err.empty()) host_.message(err, true);
            host_.reload();
        }
    }

private:
    HWND list_ = nullptr;
};

}  // namespace

std::unique_ptr<Page> makeServicePage(PageHost& host) { return std::make_unique<ServicePage>(host); }

}  // namespace uagfx::app
```

`src/app/main_window.hpp`:
```cpp
#pragma once
#include <windows.h>

#include <memory>
#include <string>

#include "app/model.hpp"
#include "app/page.hpp"
#include "app/ui.hpp"

namespace uagfx::app {

// The settings app's window (stage 2 spec §6): the list of pages on the left, the page, the language at the top
// right, the status line and the buttons at the bottom.
class MainWindow : public PageHost {
public:
    int run(HINSTANCE inst, int show);

    Model& model() override { return *model_; }
    settings::Lang lang() const override { return lang_; }
    HWND window() const override { return hwnd_; }
    void changed() override;
    void rebuild() override;
    void reload() override;
    void message(const std::wstring& text, bool error) override;

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void create();
    void makeFonts();
    void layout();
    void retext();
    void buildPage();
    void updateStatus();
    bool confirmClose();
    void doApply(bool play);
    RECT pageArea() const;

    std::unique_ptr<Model> model_;
    HWND hwnd_ = nullptr;
    settings::Lang lang_ = settings::Lang::En;
    int dpi_ = 96;
    HFONT font_ = nullptr;
    HFONT heading_ = nullptr;
    HWND sidebar_ = nullptr;
    HWND langBox_ = nullptr;
    HWND banner_ = nullptr;
    HWND status_ = nullptr;
    HWND tooltip_ = nullptr;
    HWND apply_ = nullptr;
    HWND play_ = nullptr;
    HWND undo_ = nullptr;
    HWND close_ = nullptr;
    int page_ = 0;
    std::unique_ptr<Panel> panel_;
    std::unique_ptr<Page> current_;
    bool quiet_ = false;          // the page is being made or taken down: its notifications are not the player's
    bool rebuildPosted_ = false;
    std::wstring flash_;          // the last result in the status line, until the next change
};

}  // namespace uagfx::app
```

`src/app/main_window.cpp`:
```cpp
#include "app/main_window.hpp"

#include <commctrl.h>

namespace uagfx::app {
namespace {

using namespace settings;

const wchar_t kClass[] = L"uaroGraphicsSettings";
constexpr int kWidth = 880;  // the client area at 96 DPI
constexpr int kHeight = 600;
constexpr int kSidebar = 10;
constexpr int kLang = 11;
constexpr int kApply = 20;
constexpr int kPlay = 21;
constexpr int kUndo = 22;
constexpr int kCloseButton = 23;
constexpr UINT kRebuild = WM_APP + 1;
constexpr UINT_PTR kTimer = 1;

}  // namespace

int MainWindow::run(HINSTANCE inst, int show) {
    if (HWND other = FindWindowW(kClass, nullptr)) {  // one settings app at a time: two would write over each other
        ShowWindow(other, SW_RESTORE);
        SetForegroundWindow(other);
        return 0;
    }
    model_ = std::make_unique<Model>();
    const auto saved = model_->savedLanguage();
    lang_ = saved ? *saved : langForWindows(GetUserDefaultUILanguage());
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    hwnd_ = CreateWindowExW(0, kClass, wtr(lang_, Text::AppTitle).c_str(),
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
                            kWidth, kHeight, nullptr, nullptr, inst, this);
    if (!hwnd_) return 1;
    ShowWindow(hwnd_, show);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(hwnd_, &msg)) continue;  // Tab, arrows and Alt+letter between the controls
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return int(msg.wParam);
}

LRESULT CALLBACK MainWindow::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

void MainWindow::makeFonts() {
    if (font_) DeleteObject(font_);
    if (heading_) DeleteObject(heading_);
    font_ = makeFont(dpi_, false);
    heading_ = makeFont(dpi_, true, 115);
}

void MainWindow::create() {
    dpi_ = windowDpi(hwnd_);
    makeFonts();
    HINSTANCE inst = GetModuleHandleW(nullptr);
    auto child = [&](const wchar_t* cls, DWORD style, int id, DWORD ex = 0) {
        HWND c = CreateWindowExW(ex, cls, L"", WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, hwnd_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return c;
    };
    sidebar_ = child(L"LISTBOX", LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_TABSTOP | WS_VSCROLL, kSidebar, WS_EX_CLIENTEDGE);
    langBox_ = child(WC_COMBOBOXW, CBS_DROPDOWNLIST | WS_TABSTOP, kLang);
    for (int i = 0; i < kLangCount; ++i)
        SendMessageW(langBox_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(langName(Lang(i))).c_str()));
    SendMessageW(langBox_, CB_SETCURSEL, WPARAM(lang_), 0);
    banner_ = child(L"STATIC", SS_LEFT | SS_NOPREFIX, 0);
    ShowWindow(banner_, model_->system().elevated ? SW_SHOW : SW_HIDE);
    status_ = child(L"STATIC", SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX | SS_ENDELLIPSIS, 0);
    apply_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, kApply);
    play_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, kPlay);
    undo_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, kUndo);
    close_ = child(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, kCloseButton);
    tooltip_ = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd_, nullptr, inst, nullptr);
    SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, px(380, dpi_));
    SendMessageW(tooltip_, TTM_SETDELAYTIME, TTDT_AUTOPOP, 20000);
    // The client area of 880 x 600 at this DPI.
    RECT r{0, 0, px(kWidth, dpi_), px(kHeight, dpi_)};
    AdjustWindowRectEx(&r, DWORD(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE, 0);
    SetWindowPos(hwnd_, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
    layout();
    retext();
    SendMessageW(sidebar_, LB_SETCURSEL, 0, 0);
    buildPage();
    SetTimer(hwnd_, kTimer, 2000, nullptr);  // whether the game was started or closed meanwhile
}

RECT MainWindow::pageArea() const {
    RECT client;
    GetClientRect(hwnd_, &client);
    return RECT{px(204, dpi_), px(56, dpi_), client.right - px(16, dpi_), client.bottom - px(56, dpi_)};
}

void MainWindow::layout() {
    RECT c;
    GetClientRect(hwnd_, &c);
    const int w = c.right;
    const int h = c.bottom;
    auto at = [&](int v) { return px(v, dpi_); };
    for (HWND ctl : {sidebar_, langBox_, banner_, status_, apply_, play_, undo_, close_})
        SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    MoveWindow(sidebar_, at(12), at(12), at(180), h - at(12) - at(60), TRUE);
    SendMessageW(sidebar_, LB_SETITEMHEIGHT, 0, at(30));
    MoveWindow(langBox_, w - at(12) - at(150), at(12), at(150), at(240), TRUE);
    MoveWindow(banner_, at(204), at(8), w - at(204) - at(12) - at(160), at(44), TRUE);
    const int by = h - at(46);
    const int bh = at(32);
    int x = w - at(12);
    for (auto b : {std::make_pair(close_, 110), std::make_pair(undo_, 120), std::make_pair(play_, 220),
                   std::make_pair(apply_, 130)}) {
        x -= at(b.second);
        MoveWindow(b.first, x, by, at(b.second), bh, TRUE);
        x -= at(8);
    }
    MoveWindow(status_, at(12), by, x - at(12), bh, TRUE);
}

void MainWindow::retext() {
    SetWindowTextW(hwnd_, wtr(lang_, Text::AppTitle).c_str());
    const int selected = int(SendMessageW(sidebar_, LB_GETCURSEL, 0, 0));
    SendMessageW(sidebar_, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < kPageCount; ++i)
        SendMessageW(sidebar_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wtr(lang_, pageName(settings::Page(i))).c_str()));
    SendMessageW(sidebar_, LB_SETCURSEL, WPARAM(selected < 0 ? 0 : selected), 0);
    SetWindowTextW(banner_, wtr(lang_, Text::ElevatedBanner).c_str());
    SetWindowTextW(apply_, wtr(lang_, Text::Apply).c_str());
    SetWindowTextW(play_, wtr(lang_, Text::ApplyAndPlay).c_str());
    SetWindowTextW(undo_, wtr(lang_, Text::Revert).c_str());
    SetWindowTextW(close_, wtr(lang_, Text::Close).c_str());
}

void MainWindow::buildPage() {
    rebuildPosted_ = false;
    HWND focus = GetFocus();
    const int focusId = focus && IsChild(hwnd_, focus) ? GetDlgCtrlID(focus) : 0;
    if (focusId >= 1000 && current_) {
        current_->command(focusId, EN_KILLFOCUS);  // a number being typed is kept, not lost with its field
        rebuildPosted_ = false;
        SetFocus(sidebar_);
    }
    quiet_ = true;
    current_.reset();
    panel_.reset();
    panel_ = std::make_unique<Panel>(hwnd_, font_, heading_, tooltip_, dpi_, pageArea());
    switch (settings::Page(page_)) {
        case settings::Page::Screen: current_ = makeScreenPage(*this); break;
        case settings::Page::Diagnostics: current_ = makeDiagnosticsPage(*this); break;
        case settings::Page::Service: current_ = makeServicePage(*this); break;
        default: current_ = makeSettingsPage(*this, settings::Page(page_)); break;
    }
    current_->build(*panel_);
    quiet_ = false;
    if (focusId >= 1000)
        if (HWND f = GetDlgItem(hwnd_, focusId)) SetFocus(f);
    updateStatus();
}

void MainWindow::updateStatus() {
    const size_t pending = model_->pendingCount();
    std::wstring text = flash_;
    if (text.empty()) {
        text = wtr(lang_, model_->now().gameRunning ? Text::StatusRunning : Text::StatusClosed);
        if (pending) text += L" " + wtrf(lang_, Text::StatusPending, {num(static_cast<long long>(pending))});
    }
    SetWindowTextW(status_, text.c_str());
    EnableWindow(apply_, pending > 0);
    EnableWindow(play_, !model_->system().elevated);
    EnableWindow(undo_, model_->canRevert());
}

void MainWindow::changed() {
    flash_.clear();
    updateStatus();
}

void MainWindow::rebuild() {
    if (rebuildPosted_) return;
    rebuildPosted_ = true;
    PostMessageW(hwnd_, kRebuild, 0, 0);  // after this message: the control that asked is not taken down under it
}

void MainWindow::reload() {
    retext();
    buildPage();
}

void MainWindow::message(const std::wstring& text, bool error) {
    MessageBoxW(hwnd_, text.c_str(), wtr(lang_, Text::AppTitle).c_str(), error ? MB_ICONWARNING : MB_ICONINFORMATION);
}

bool MainWindow::confirmClose() {
    return model_->pendingCount() == 0 || MessageBoxW(hwnd_, wtr(lang_, Text::AskUnsaved).c_str(),
                                                      wtr(lang_, Text::AppTitle).c_str(),
                                                      MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void MainWindow::doApply(bool play) {
    std::wstring err = model_->apply(lang_);
    if (err.empty() && play) err = model_->launchGame(lang_);
    if (!err.empty())
        message(err, true);
    else
        flash_ = wtr(lang_, Text::StatusApplied);
    reload();
}

LRESULT MainWindow::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            create();
            return 0;
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (id == kSidebar && code == LBN_SELCHANGE) {
                const LRESULT sel = SendMessageW(sidebar_, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel != page_) {
                    page_ = int(sel);
                    buildPage();
                }
            } else if (id == kLang && code == CBN_SELCHANGE) {
                const LRESULT sel = SendMessageW(langBox_, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < kLangCount) {
                    lang_ = Lang(sel);
                    model_->saveLanguage(lang_);
                    reload();
                }
            } else if (id == kApply && code == BN_CLICKED) {
                doApply(false);
            } else if (id == kPlay && code == BN_CLICKED) {
                doApply(true);
            } else if (id == kUndo && code == BN_CLICKED) {
                const std::wstring err = model_->revert(lang_);
                if (!err.empty()) message(err, true);
                flash_.clear();
                model_->load();
                reload();
            } else if (id == kCloseButton && code == BN_CLICKED) {
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            } else if (id == IDOK) {  // Enter: a number field takes what was typed
                HWND focus = GetFocus();
                const int focusId = focus ? GetDlgCtrlID(focus) : 0;
                if (focusId >= 1000 && current_) current_->command(focusId, EN_KILLFOCUS);
            } else if (id == IDCANCEL) {
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            } else if (id >= 1000 && current_ && !quiet_) {
                current_->command(id, code);
            }
            return 0;
        }
        case WM_HSCROLL:
            if (lp && current_ && !quiet_) {
                HWND ctl = reinterpret_cast<HWND>(lp);
                current_->scroll(GetDlgCtrlID(ctl), LOWORD(wp), int(SendMessageW(ctl, TBM_GETPOS, 0, 0)));
            }
            return 0;
        case WM_NOTIFY: {
            const auto* n = reinterpret_cast<const NMHDR*>(lp);
            if (n && n->idFrom >= 1000 && current_ && !quiet_) return current_->notify(n);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            HWND ctl = reinterpret_cast<HWND>(lp);
            Tone tone = Tone::Normal;
            if (ctl == banner_)
                SetTextColor(dc, toneColor(Tone::Bad));
            else if (panel_ && panel_->toneOf(ctl, tone))
                SetTextColor(dc, toneColor(tone));
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        case kRebuild:
            if (rebuildPosted_) buildPage();
            return 0;
        case WM_TIMER:
            if (wp == kTimer && model_->refreshRunning()) buildPage();  // the game started or closed
            return 0;
        case WM_DPICHANGED: {
            dpi_ = HIWORD(wp);
            makeFonts();
            const RECT* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, px(380, dpi_));
            layout();
            buildPage();
            return 0;
        }
        case WM_CLOSE:
            if (confirmClose()) DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kTimer);
            current_.reset();
            panel_.reset();
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace uagfx::app
```

`src/app/main.cpp`:
```cpp
#include <windows.h>

#include <commctrl.h>
#include <objbase.h>

#include "app/main_window.hpp"

// uaro-graphics.exe: the uaRO Graphics Settings app (stage 2 spec).
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);  // ShellExecuteEx needs it
    uagfx::app::MainWindow window;
    const int code = window.run(inst, show);
    CoUninitialize();
    return code;
}
```

- [ ] **Step 5: Цель сборки**

В `CMakeLists.txt` заменить:
```cmake
    target_link_libraries(uagfx_app PUBLIC uagfx_core comctl32 shell32 ole32 advapi32)
else()
```
на:
```cmake
    target_link_libraries(uagfx_app PUBLIC uagfx_core comctl32 shell32 ole32 advapi32)

    # uaro-graphics.exe, the settings app (stage 2 spec): its version comes from src/core/version.cpp.
    enable_language(RC)
    file(STRINGS src/core/version.cpp UAGFX_VERSION_LINE REGEX "kVersion = ")
    string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" UAGFX_VERSION "${UAGFX_VERSION_LINE}")
    set(UAGFX_V1 ${CMAKE_MATCH_1})
    set(UAGFX_V2 ${CMAKE_MATCH_2})
    set(UAGFX_V3 ${CMAKE_MATCH_3})
    configure_file(src/app/version_rc.h.in ${CMAKE_BINARY_DIR}/app/version_rc.h)
    add_executable(uaro-graphics WIN32 src/app/main.cpp src/app/main_window.cpp src/app/ui.cpp src/app/page.cpp
                   src/app/page_settings.cpp src/app/page_screen.cpp src/app/page_diagnostics.cpp
                   src/app/page_service.cpp src/app/app.rc)
    target_include_directories(uaro-graphics PRIVATE src/app dist ${CMAKE_BINARY_DIR}/app)
    target_compile_options(uaro-graphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-graphics PRIVATE uagfx_app)
    target_link_options(uaro-graphics PRIVATE -static -static-libgcc -static-libstdc++ -s -municode)
else()
```

- [ ] **Step 6: Запустить: программа собирается и проходит проверку**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh`
Expected: ни одной строки `warning`/`error`; `OK: build-win/ddraw.dll: …`; `OK: build-win/uaro-graphics.exe: GUI, … from
advapi32.dll comctl32.dll gdi32.dll kernel32.dll msvcrt.dll ole32.dll shell32.dll user32.dll, asInvoker, per-monitor DPI`;
`240 tests, 0 failed checks`.

- [ ] **Step 7: Коммит**

```bash
git add CMakeLists.txt tools/check_exe.py tools/make_icon.py src/app/app.ico src/app/app.rc src/app/app.manifest src/app/version_rc.h.in src/app/ui.hpp src/app/ui.cpp src/app/page.hpp src/app/page.cpp src/app/page_settings.cpp src/app/page_screen.cpp src/app/page_diagnostics.cpp src/app/page_service.cpp src/app/main_window.hpp src/app/main_window.cpp src/app/main.cpp
git commit -q -m "Утилита uaRO Graphics Settings: окно со списком разделов, Рендер, Экран, Качество, Кадры, Диагностика, Сервис

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 12: Пакет, скрипты, версия 0.6.0, чек-лист (spec §9, §10)

Утилита едет в пакете (`uaro-graphics\uaro-graphics.exe`), `install.bat` кладёт её рядом с `uaRO.exe` и не трогает,
пока она открыта; `update.bat` ждёт до 10 секунд, пока она закроется; `uninstall.bat` убирает её и оставляет
`uaro-graphics-backup`. Заголовок ini называет утилиту. Версия — 0.6.0; чек-лист — spec §10. Спецификация v1 §5
отсылает к спецификации этапа 2.

**Files:**
- Modify: `tests/test_version.cpp`, `src/core/version.cpp`, `tools/check_package.py`, `tools/package.sh`,
  `dist/install.bat`, `dist/uninstall.bat`, `dist/update.bat`, `dist/uaro-graphics.ini`, `dist/TEST.ru.md`,
  `docs/superpowers/specs/2026-10-13-uaro-graphics-design.md`

- [ ] **Step 1: Тест версии**

В `tests/test_version.cpp` заменить:
```cpp
TEST(version_is_0_5_2) { CHECK(std::strcmp(uagfx::kVersion, "0.5.2") == 0); }
```
на:
```cpp
TEST(version_is_0_6_0) { CHECK(std::strcmp(uagfx::kVersion, "0.6.0") == 0); }
```

- [ ] **Step 2: Запустить: версия ещё 0.5.2**

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1`
Expected: `FAIL version_is_0_6_0`, `240 tests, 1 failed checks`.

- [ ] **Step 3: Версия 0.6.0**

В `src/core/version.cpp` заменить:
```cpp
const char* const kVersion = "0.5.2";
```
на:
```cpp
const char* const kVersion = "0.6.0";
```

Run: `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-host -j 2>&1 | grep -E "warning|error"; ./build-host/uagfx_tests | tail -1` — Expected: `240 tests, 0 failed checks`.

- [ ] **Step 4: Проверка пакета ждёт утилиту**

В `tools/check_package.py` заменить:
```python
            "uaro-graphics/ddraw.dll", "uaro-graphics/uaro-graphics.ini", "uaro-graphics/version.txt"}
```
на:
```python
            "uaro-graphics/ddraw.dll", "uaro-graphics/uaro-graphics.exe", "uaro-graphics/uaro-graphics.ini",
            "uaro-graphics/version.txt"}
```

Run: `tools/package.sh`
Expected: `FAIL: layout [...], expected [...]` — в пакете ещё нет `uaro-graphics/uaro-graphics.exe`.

- [ ] **Step 5: Пакет, скрипты, ini, чек-лист, ссылка в спецификации v1**

В `tools/package.sh` заменить:
```bash
python3 tools/check_dll.py build-win/ddraw.dll
```
на:
```bash
python3 tools/check_dll.py build-win/ddraw.dll
python3 tools/check_exe.py build-win/uaro-graphics.exe
```

В `tools/package.sh` заменить:
```bash
cp build-win/ddraw.dll dist/uaro-graphics.ini "$stage/uaro-graphics/"
```
на:
```bash
cp build-win/ddraw.dll build-win/uaro-graphics.exe dist/uaro-graphics.ini "$stage/uaro-graphics/"
```

В `dist/install.bat` заменить:
```bat
rem A client under Program Files can only be changed with administrator rights.
(echo test) > "uaro-graphics-write-test.tmp" 2>nul || goto :noaccess
```
на:
```bat
tasklist /fi "imagename eq uaro-graphics.exe" 2>nul | find /i "uaro-graphics.exe" >nul && (
    echo uaRO Graphics Settings is open: close it and run install.bat again.
    goto :fail
)
rem A client under Program Files can only be changed with administrator rights.
(echo test) > "uaro-graphics-write-test.tmp" 2>nul || goto :noaccess
```

В `dist/install.bat` заменить:
```bat
copy /y "uaro-graphics\ddraw.dll" "ddraw.dll" >nul || goto :noaccess
```
на:
```bat
copy /y "uaro-graphics\ddraw.dll" "ddraw.dll" >nul || goto :noaccess
rem The settings app, uaRO Graphics Settings, lives next to uaRO.exe.
if exist "uaro-graphics\uaro-graphics.exe" copy /y "uaro-graphics\uaro-graphics.exe" "uaro-graphics.exe" >nul || goto :noaccess
```

В `dist/install.bat` заменить:
```bat
echo uaRO Graphics is installed. Start the game as usual.
echo To draw with the uaRO Graphics renderer (Direct3D 12), run use-uaro.bat; use-dgvoodoo.bat switches back.
```
на:
```bat
echo uaRO Graphics is installed. Start the game as usual.
echo Every setting is in uaro-graphics.exe (uaRO Graphics Settings) in this folder.
echo To draw with the uaRO Graphics renderer (Direct3D 12), choose it there or run use-uaro.bat; use-dgvoodoo.bat switches back.
```

В `dist/uninstall.bat` заменить:
```bat
rem The list of pipelines met in the game is a cache of uaRO Graphics: it goes too.
```
на:
```bat
if exist "uaro-graphics.exe" del /f "uaro-graphics.exe"
rem The list of pipelines met in the game is a cache of uaRO Graphics: it goes too.
```

В `dist/uninstall.bat` заменить:
```bat
echo uaro-graphics.ini and uaro-graphics.log stay here; delete them if you like.
```
на:
```bat
echo uaro-graphics.ini and uaro-graphics.log stay here; delete them if you like.
echo The uaro-graphics-backup folder keeps the copies of your settings the settings app made.
```

В `dist/update.bat` заменить:
```bat
where curl >nul 2>&1 || goto :oldwindows
```
на:
```bat
rem The settings app may have started this update: give it up to 10 seconds to close, its exe is replaced too.
set /a waited=0
:waitapp
tasklist /fi "imagename eq uaro-graphics.exe" 2>nul | find /i "uaro-graphics.exe" >nul || goto :appclosed
if %waited% geq 10 (
    echo uaRO Graphics Settings is open: close it and run update.bat again.
    goto :fail
)
timeout /t 1 /nobreak >nul
set /a waited+=1
goto :waitapp
:appclosed
where curl >nul 2>&1 || goto :oldwindows
```

В `dist/uaro-graphics.ini` заменить:
```ini
; The settings app (coming in a later version) edits this file and keeps these comments.
```
на:
```ini
; uaro-graphics.exe (uaRO Graphics Settings) edits this file and keeps these comments.
```

`dist/TEST.ru.md`:
```markdown
# uaRO Graphics 0.6.0 — проверка

Что нового: утилита настроек **uaRO Graphics Settings** — `uaro-graphics.exe` рядом с `uaRO.exe`.
- Рендер, экран (во весь экран или в окне, размер интерфейса, монитор), качество, кадры, диагностика, пресеты и
  резервные копии — в одном окне, на русском, украинском или английском.
- Разрешение игры меняется без `setup.exe` и без прав администратора.
- Резкость, масштаб рендера, фильтр интерфейса, чёткий текст, VSync и лимит FPS меняются в запущенной игре.
- Перед первой записью утилита сохраняет копию всех настроек: «Сервис → Резервные копии → Восстановить».

## Установка

Закрой игру и дважды щёлкни `update.bat` в папке клиента.

## Проверка

1. `update.bat` в конце написал `uaRO Graphics 0.6.0 build … is installed`.
2. Запусти `uaro-graphics.exe`. Окно чёткое, язык — как у Windows (переключатель вверху справа). Пройди все разделы
   слева.
3. «Диагностика»: монитор 3840×2160, масштаб 175 %, Direct3D 12 «работает», ddraw.dll — uaRO Graphics 0.6.0.
4. «Экран»: строка «Сейчас: в окне · окно 3360×1890 · интерфейс 175 % · игра 1920×1080».
5. **Во весь экран** (первая проверка этого пути с нашим рендером). Рендер — uaRO Graphics. «Экран» → «Во весь
   экран», размер интерфейса 175 % → «Применить и играть». Игра на весь монитор без рамки, картинка и текст чёткие,
   мышь попадает куда надо. Alt+Tab: игра сворачивается и возвращается. Потом верни «В окне» с окном 3360×1890.
6. **Окно с другим интерфейсом.** «Экран» → «В окне», окно 3360×1890, размер интерфейса 200 % → «Применить и
   играть». Окно того же размера, с рамкой, интерфейс крупнее и пометка «самый чёткий». Мышь попадает. Потом верни
   175 %.
7. **На лету.** Пока игра идёт, в утилите «Качество»: двигай «Резкость» и «Масштаб рендера» и отпускай — в игре
   меняется примерно через секунду. «Отменить» возвращает как было.
8. «Сервис» → «Резервные копии»: в списке есть копия этого запуска. «Восстановить» её — настройки вернутся.
9. Запусти утилиту правой кнопкой «Запуск от имени администратора»: вверху красная строка, «Применить и играть»
   серая. Закрой.

## Что прислать

Файл `uaro-graphics.log` и коротко по каждому пункту. Если утилита что-то показывает не так — снимок её окна
(Win+Shift+S). Кнопка «Отчёт для форума» в «Диагностике» копирует всё нужное в буфер обмена.

## Если что-то не так

Вернуть настройки: «Сервис» → «Резервные копии» → «Восстановить». Вернуть dgVoodoo без утилиты: `use-dgvoodoo.bat`.

## Удаление

Дважды щёлкни `uninstall.bat` — всё вернётся как было. Папка `uaro-graphics-backup` с копиями настроек останется.
```

В `docs/superpowers/specs/2026-10-13-uaro-graphics-design.md` заменить:
```markdown
## 5. Утилита «uaRO Graphics Settings» (`uaro-graphics.exe`)
```
на:
```markdown
## 5. Утилита «uaRO Graphics Settings» (`uaro-graphics.exe`)

*Заменено спецификацией этапа 2 `2026-10-13-stage2-settings-utility-design.md` (выпуски 0.6.0–0.6.2).*
```

- [ ] **Step 6: Запустить: пакет собирается, всё проходит**

Run: `/tmp/claude-0/-root-uaro-gamepad/836e7f85-2c81-436d-8c88-271823c49c25/scratchpad/verify4.sh && tools/package.sh`
Expected: проверки задачи как в Task 11, `240 tests, 0 failed checks`; `OK: build-win/uaro-graphics.zip: TEST.ru.md,
install.bat, uaro-graphics/ddraw.dll, uaro-graphics/uaro-graphics.exe, uaro-graphics/uaro-graphics.ini,
uaro-graphics/version.txt, uninstall.bat, update.bat, use-dgvoodoo.bat, use-uaro.bat`.

- [ ] **Step 7: Коммит**

```bash
git add tests/test_version.cpp src/core/version.cpp tools/check_package.py tools/package.sh dist/install.bat dist/uninstall.bat dist/update.bat dist/uaro-graphics.ini dist/TEST.ru.md docs/superpowers/specs/2026-10-13-uaro-graphics-design.md
git commit -q -m "0.6.0: утилита в пакете, install/update/uninstall, чек-лист

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```


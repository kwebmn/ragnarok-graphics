// 3.0.24-beta.57 (the owner 28.11: «полную проверку всех функций ... как на ноутбуке так и на стационарном ... чтобы
// эта проверка по мсп не занимала много времени»; docs/superpowers/plans/2026-11-28-full-check-probe-tour.md, task 4):
// the full check's jobs in the game's frames - the probe's settings and the tour's windows. core/check_probe and
// core/check_tour say each step; here they are done: a value written the Alt+O way, the renderer's raw frame asked
// and read back (the portrait's way), the card's passes, the windows' words, the logs' new errors, a key pressed as
// the MCP's key tool presses it. One line a check into uaro-graphics\logs\check-<time>-<kind>.json.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/check_probe.hpp"
#include "core/check_tour.hpp"
#include "core/layout.hpp"
#include "core/live_config.hpp"
#include "core/png_write.hpp"
#include "core/setting_change.hpp"
#include "core/settings/schema.hpp"
#include "core/shot_spec.hpp"
#include "core/version.hpp"
#include "win/context.hpp"
#include "win/dpi.hpp"
#include "win/log.hpp"
#include "win/mcp_input.hpp"
#include "win/paths.hpp"
#include "win/uaui.hpp"
#include "win/ui_sheet.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win {
namespace {

mcp::CallResult said(const std::string& text, bool error = false) {
    mcp::CallResult r;
    r.isError = error;
    r.content.push_back(mcp::Content::textOf(text));
    return r;
}

std::string ours(const char* name) { return gameDir() + "\\" + layout::kOurs + "\\" + name; }
std::string logsFile(const std::string& name) { return gameDir() + "\\" + layout::kLogs + "\\" + name; }
std::string frameFile() { return ours("check-frame.bgra"); }
std::string planFile() { return logsFile("check-restart.json"); }

std::string stamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char b[32];
    std::snprintf(b, sizeof b, "%04d%02d%02d-%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}

uint64_t sizeOf(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a)) return 0;
    return (uint64_t(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
}

// A log from `at` on (a log begun anew - smaller than `at` - from its start), 1 MB at most; written meanwhile by
// the driver or the renderer, so shared
std::string tailFrom(const std::string& path, uint64_t at) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return "";
    LARGE_INTEGER size{};
    std::string out;
    if (GetFileSizeEx(f, &size)) {
        const uint64_t all = uint64_t(size.QuadPart), from = all >= at ? at : 0;
        const uint64_t n = std::min<uint64_t>(all - from, uint64_t(1) << 20);
        LARGE_INTEGER pos{};
        pos.QuadPart = LONGLONG(from);
        DWORD got = 0;
        out.assign(size_t(n), '\0');
        if (n && SetFilePointerEx(f, pos, nullptr, FILE_BEGIN) && ReadFile(f, out.data(), DWORD(n), &got, nullptr))
            out.resize(got);
        else
            out.clear();
    }
    CloseHandle(f);
    return out;
}

// The renderer's raw frame once whole (it writes it from its own process: a part read waits for the next frame)
bool readFrame(std::vector<uint32_t>& px, int& w, int& h) {
    if (sizeOf(frameFile()) < 12) return false;
    std::string bytes;
    return readTextFile(frameFile(), bytes) && shotspec::readRaw(bytes, px, w, h);
}

void savePicture(const std::vector<uint32_t>& px, int w, int h, const std::string& name) {
    if (px.empty() || w <= 0 || h <= 0) return;
    int sw = 0, sh = 0;
    const std::vector<uint8_t> small =
        shotspec::shrunk(reinterpret_cast<const uint8_t*>(px.data()), w, h, size_t(w) * 4, 4, sw, sh);
    writePng(logsFile(name), small.data(), sw, sh, size_t(sw) * 4);
}

// The tour's keys pressed as the MCP's key tool presses them - from a thread of their own: the press holds Alt for
// the game's next frame (mcpinput::key sleeps), which the game's own thread would never reach meanwhile
std::atomic<bool> g_pressing{false};
struct Press {
    mcpinput::Game game;
    mcp::KeyCombo combo;
    std::string words;
};

DWORD WINAPI pressThread(void* p) {
    const std::unique_ptr<Press> press(static_cast<Press*>(p));
    mcpinput::key(press->game, press->combo, press->words);
    g_pressing = false;
    return 0;
}

void pressAsync(const mcpinput::Game& g, const std::string& keys) {
    mcp::KeyCombo k;
    std::string why;
    if (!mcp::keyCombo(keys, k, why)) return;
    g_pressing = true;
    auto* p = new Press{g, k, keys};
    HANDLE t = CreateThread(nullptr, 0, &pressThread, p, 0, nullptr);
    if (!t) {
        delete p;
        g_pressing = false;
        return;
    }
    CloseHandle(t);
}

bool writing(checkprobe::Phase p) {
    return p == checkprobe::Phase::Applied || p == checkprobe::Phase::Settling || p == checkprobe::Phase::After ||
           p == checkprobe::Phase::Back;
}

}  // namespace

void Context::checkLine(const json::Value& row) {
    if (checkFile_.empty()) return;
    if (FILE* f = std::fopen(checkFile_.c_str(), "ab")) {
        const std::string line = json::dump(row) + "\n";
        std::fwrite(line.data(), 1, line.size(), f);
        std::fclose(f);
    }
}

void Context::checkWrite(const std::string& key, const std::string& value) {
    std::vector<settings::IniEdit> edits;
    switch (settingchange::edits(config, cardLevel(), key, value, edits)) {
        case settingchange::Outcome::Edits:
            if (!editIniFile(ourFile(layout::File::Ini), edits)) {
                writeRefused_ = "uaro-graphics.ini could not be written";
                return;
            }
            settingsKnown_ = false;  // read at the next frame, not in a second
            settingsChecked_ = 0;
            return;
        case settingchange::Outcome::Same: return;
        case settingchange::Outcome::Unknown: writeRefused_ = "no such setting"; return;
        case settingchange::Outcome::Invalid: writeRefused_ = "the value " + value + " refused as Alt+O would"; return;
    }
}

std::string Context::checkShotWanted() {
    if (!shotWanted_) return "";
    shotWanted_ = false;
    shotSent_ = true;
    DeleteFileA(frameFile().c_str());  // never a frame of before read for this one
    return frameFile();
}

void Context::checkFrame(bool minimised) {
    if (!probeOn_ && !tourOn_) return;
    mcpCalledMs_ = GetTickCount64();  // driven while a check runs: in front for its frames (drivenOverMcp)
    const uint64_t ms = GetTickCount64();
    if (probeOn_) probeFrame(ms, minimised);
    if (tourOn_ && !minimised) tourFrame(ms);
}

void Context::probeTrace() {
    checkprobe::Row& r = probe_.row;
    if (!frameBefore_.empty() && !frameAfter_.empty()) {
        r.framed = true;
        r.frame = checkprobe::diff(frameBefore_, beforeW_, beforeH_, frameAfter_, afterW_, afterH_);
    }
    if (r.reached) {
        r.passes = checkprobe::passesDelta(passesBefore_, lastGpuKnown_ ? lastGpu_.effects
                                                                       : std::map<std::string, double>{});
        r.uiChanged = checkprobe::printsMoved(printBefore_, printAgain_, uaui::wordsPrint());
    }
    r.logErrors = checkprobe::errorLines(tailFrom(ourFile(layout::File::DriverLog), logAt_[0])) +
                  checkprobe::errorLines(tailFrom(ourFile(layout::File::Render64Log), logAt_[1]));
    // the frames of a row worth an eye kept, a quarter of their size: a fault, or an effect that drew nothing
    const bool still = r.framed && !r.frame.sizeChanged && r.frame.changed < 0.35 && r.frame.mean < 0.12 &&
                       r.passes.members().empty() && r.uiChanged == 0;
    if (r.framed && !r.frame.sizeChanged && (!r.error.empty() || (probePhoto_ && still))) {
        savePicture(frameBefore_, beforeW_, beforeH_, "check-" + r.key + "-before.png");
        savePicture(frameAfter_, afterW_, afterH_, "check-" + r.key + "-after.png");
    }
    frameBefore_.clear();
    frameAfter_.clear();
}

void Context::probeFrame(uint64_t ms, bool minimised) {
    checkprobe::Seen s;
    s.ms = ms;
    s.minimised = minimised;
    if (probe_.at < probe_.items.size())
        if (const settings::IniSetting* st = settings::findSetting(probe_.items[probe_.at].key))
            s.value = settings::tokenOf(*st, config);
    s.loading = rendererLoading_;
    s.gpuSerial = lastGpuKnown_ ? lastGpu_.serial : 0;
    if (shotSent_ && !minimised) {
        std::vector<uint32_t> px;
        int w = 0, h = 0;
        if (readFrame(px, w, h)) {
            shotSent_ = false;
            s.shotReady = true;
            if (probe_.phase == checkprobe::Phase::Before)
                frameBefore_ = std::move(px), beforeW_ = w, beforeH_ = h;
            else
                frameAfter_ = std::move(px), afterW_ = w, afterH_ = h;
        }
    }
    const checkprobe::Do d = checkprobe::step(probe_, s, checkprobe::Timing{});
    const checkprobe::Item* it = probe_.at < probe_.items.size() ? &probe_.items[probe_.at] : nullptr;
    switch (d) {
        case checkprobe::Do::ShotBefore:
            shotWanted_ = true, shotSent_ = false;
            frameBefore_.clear(), frameAfter_.clear();
            passesBefore_ = lastGpuKnown_ ? lastGpu_.effects : std::map<std::string, double>{};
            printBefore_ = uaui::wordsPrint();
            logAt_[0] = sizeOf(ourFile(layout::File::DriverLog));
            logAt_[1] = sizeOf(ourFile(layout::File::Render64Log));
            writeRefused_.clear();
            break;
        case checkprobe::Do::Write:
            printAgain_ = uaui::wordsPrint();  // the documents that move by themselves (the clock) told apart
            if (it) checkWrite(it->key, it->to);
            break;
        case checkprobe::Do::ShotAfter: shotWanted_ = true, shotSent_ = false; break;
        case checkprobe::Do::WriteBack:
            probeTrace();
            if (it) checkWrite(it->key, it->from);
            break;
        case checkprobe::Do::Record: {
            json::Value row = checkprobe::rowJson(probe_.row);
            if (!writeRefused_.empty()) row.set("refused", writeRefused_);
            checkLine(row);
            break;
        }
        case checkprobe::Do::Finish: {
            probeOn_ = false;
            if (probePhoto_) photo_ = photoBefore_;
            json::Value end = json::Value::object();
            end.set("check", "end");
            end.set("kind", "probe");
            checkLine(end);
            LOG_INFO("check: the probe ended - %u setting(s), %s", unsigned(probe_.items.size()), checkFile_.c_str());
            break;
        }
        case checkprobe::Do::Nothing: break;
    }
}

void Context::tourFrame(uint64_t ms) {
    if (g_pressing) return;  // a key still being pressed: the next step after it
    checktour::Seen s;
    s.ms = ms;
    s.docs = uaui::shownWindows();
    const std::optional<winstack::Win> top = windowstack::top();
    s.padFocus = top ? uisheet::navFocus(int(*top)) : -2;
    const mcpinput::Game game{window, int(shadow.width()), int(shadow.height())};
    switch (checktour::step(tour_, s)) {
        case checktour::Do::Keys: pressAsync(game, tour_.list[tour_.at]->keys); break;
        case checktour::Do::Check: {
            json::Value problems = json::Value::array();
            const SIZE px = window ? clientPixels(window, config.highResolution) : SIZE{0, 0};
            for (const std::string& doc : tour_.row.opened) {
                for (const json::Value& one : uaui::check(doc, false).items())
                    if (!one["problems"].items().empty()) problems.push(one);
                int x = 0, y = 0, w = 0, h = 0;
                if (px.cx > 0 && uaui::boxOf(doc, x, y, w, h) && !checktour::inside(x, y, w, h, px.cx, px.cy))
                    tour_.row.inside = false;
            }
            tour_.row.problems = problems;
            break;
        }
        case checktour::Do::PadStep:
            if (top) uisheet::padNav(int(*top), uisheet::NavCommand::Down);
            break;
        case checktour::Do::Esc: pressAsync(game, "esc"); break;
        case checktour::Do::Record: checkLine(checktour::rowJson(tour_.row)); break;
        case checktour::Do::Finish: {
            tourOn_ = false;
            json::Value end = json::Value::object();
            end.set("check", "end");
            end.set("kind", "tour");
            checkLine(end);
            LOG_INFO("check: the tour ended - %s", checkFile_.c_str());
            break;
        }
        case checktour::Do::Nothing: break;
    }
}

mcp::CallResult Context::checkCall(const std::string& tool, const json::Value& args) {
    const std::string action = args["action"].asString();
    const auto head = [this](const char* kind, size_t items) {
        checkFile_ = logsFile("check-" + stamp() + "-" + kind + ".json");
        json::Value h = json::Value::object();
        h.set("check", "start");
        h.set("kind", kind);
        h.set("version", std::string(kVersion));
        h.set("api", apiLabel_);
        if (renderer) h.set("adapter", renderer->adapterName());
        h.set("map", map_);
        h.set("items", int64_t(items));
        checkLine(h);
    };
    const std::string fileName = [this] {
        const size_t slash = checkFile_.rfind('\\');
        return slash == std::string::npos ? checkFile_ : "logs\\" + checkFile_.substr(slash + 1);
    }();
    if (tool == "tour") {
        if (action == "status") {
            json::Value o = checktour::status(tour_);
            o.set("running", tourOn_);
            o.set("file", fileName);
            return said(json::dump(o));
        }
        if (action == "stop") {
            tourOn_ = false;
            return said("the tour stopped");
        }
        if (probeOn_ || tourOn_) return said("a check runs already - its status, or stop it first", true);
        tour_ = checktour::Job{};
        tour_.here = action == "here";
        for (const json::Value& n : args["only"].items()) tour_.only.push_back(n.asString());
        size_t n = tour_.here ? 1 : 0;
        if (!tour_.here)
            for (const checktour::Stop& s : checktour::stops())
                if (tour_.only.empty() || std::find(tour_.only.begin(), tour_.only.end(), s.name) != tour_.only.end())
                    ++n;
        tourOn_ = true;
        head("tour", n);
        LOG_INFO("check: the tour started (%u stop(s))", unsigned(n));
        return said("the tour started: " + std::to_string(n) + " stop(s); the lines into logs\\" +
                    checkFile_.substr(checkFile_.rfind('\\') + 1) + " - ask its status");
    }
    // the probe
    if (action == "status") {
        json::Value o = checkprobe::status(probe_);
        o.set("running", probeOn_);
        o.set("file", fileName);
        return said(json::dump(o));
    }
    if (action == "stop") {
        if (probeOn_ && probe_.at < probe_.items.size() && writing(probe_.phase))
            checkWrite(probe_.items[probe_.at].key, probe_.items[probe_.at].from);  // its old value back now
        if (probeOn_ && probePhoto_) photo_ = photoBefore_;
        probeOn_ = false;
        return said("the probe stopped at " + std::to_string(probe_.at) + " of " + std::to_string(probe_.items.size()));
    }
    if (action == "verify") {
        std::string text;
        json::Value plan;
        std::string error;
        if (!readTextFile(planFile(), text) || !json::parse(text, plan, error))
            return said("no restart plan (logs\\check-restart.json) - probe restart first", true);
        const bool back = plan["phase"].asInt() == 2;
        const std::vector<checkprobe::Row> rows = checkprobe::verifyPlan(plan, back, [this](const std::string& k) {
            const settings::IniSetting* st = settings::findSetting(k);
            return st ? settings::tokenOf(*st, config) : std::string();
        });
        head(back ? "restart-back" : "restart", rows.size());
        size_t good = 0;
        for (const checkprobe::Row& r : rows) {
            checkLine(checkprobe::rowJson(r));
            if (r.skip.empty() && (back ? r.restored : r.reached)) ++good;
        }
        if (back) {
            DeleteFileA(planFile().c_str());
            return said("back after the restart: " + std::to_string(good) + " of " + std::to_string(rows.size()));
        }
        std::vector<settings::IniEdit> edits;
        for (const json::Value& o : plan["items"].items())
            if (o["skip"].asString().empty())
                settingchange::edits(config, cardLevel(), o["key"].asString(), o["from"].asString(), edits);
        plan.set("phase", 2);
        const bool written = edits.empty() || editIniFile(ourFile(layout::File::Ini), edits);
        writeFileAtomic(planFile(), json::dump(plan));
        return said("taken after the restart: " + std::to_string(good) + " of " + std::to_string(rows.size()) +
                        (written ? "; the old values written back - restart the game again, then probe verify"
                                 : "; the old values could NOT be written back"),
                    !written);
    }
    if (probeOn_ || tourOn_) return said("a check runs already - its status, or stop it first", true);
    std::vector<std::string> only;
    for (const json::Value& k : args["keys"].items()) only.push_back(k.asString());
    const std::string prefix = args["prefix"].asString();
    std::vector<checkprobe::Item> items;
    size_t left = 0;
    const bool restart = action == "restart";
    for (const settings::IniSetting& s : settings::iniSettings()) {
        const std::string key = settings::sectionKey(s);
        if (key.compare(0, prefix.size(), prefix) != 0) continue;
        if (!only.empty() && std::find(only.begin(), only.end(), key) == only.end()) continue;
        if (appliesNow(key) == restart) continue;  // start: those at once; restart: those at a start
        // a start's batch leaves the client's files and the renderer's own to one at a time - unless named
        if (restart && only.empty() && (key.rfind("client.", 0) == 0 || key.rfind("renderer.", 0) == 0)) continue;
        checkprobe::Item it;
        it.key = key;
        it.from = settings::tokenOf(s, config);
        it.skip = checkprobe::skipReason(key);
        if (it.skip.empty()) it.to = checkprobe::testValue(s, it.from);
        if (!it.skip.empty() || it.to.empty()) ++left;
        items.push_back(std::move(it));
    }
    if (items.empty()) return said("no setting matches - see the settings tool (its live and skip)", true);
    if (restart) {
        std::vector<settings::IniEdit> edits;
        for (const checkprobe::Item& it : items)
            if (it.skip.empty() && !it.to.empty()) settingchange::edits(config, cardLevel(), it.key, it.to, edits);
        json::Value plan = checkprobe::restartPlan(items);
        plan.set("phase", 1);
        if (!writeFileAtomic(planFile(), json::dump(plan)))
            return said("logs\\check-restart.json could not be written", true);
        if (!edits.empty() && !editIniFile(ourFile(layout::File::Ini), edits))
            return said("uaro-graphics.ini could not be written", true);
        LOG_INFO("check: %u value(s) that wait for a start written for a restart", unsigned(items.size() - left));
        return said("written " + std::to_string(items.size() - left) + " value(s) that wait for a start (" +
                    std::to_string(left) + " left alone) - restart the game (the watchdog's game restart), then "
                    "probe verify");
    }
    probe_ = checkprobe::Job{};
    probe_.items = std::move(items);
    probeOn_ = true;
    probePhoto_ = args["photo"].asBool(false);
    photoBefore_ = photo_;
    if (probePhoto_) photo_ = true;
    shotWanted_ = shotSent_ = false;
    head("probe", probe_.items.size());
    LOG_INFO("check: the probe started (%u setting(s))", unsigned(probe_.items.size()));
    return said("the probe started: " + std::to_string(probe_.items.size()) + " setting(s), " + std::to_string(left) +
                " left alone; the lines into logs\\" + checkFile_.substr(checkFile_.rfind('\\') + 1) +
                " - ask its status");
}

}  // namespace uagfx::win

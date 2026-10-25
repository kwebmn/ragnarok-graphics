#include <windows.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "core/effect_groups.hpp"
#include "core/graphics_test.hpp"
#include "core/layout.hpp"
#include "core/light_mode.hpp"
#include "core/log_files.hpp"
#include "core/settings/strings.hpp"
#include "core/version.hpp"
#include "win/context.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"

// 3.0.5-beta.6 (the user: «есть также идея сделать некий режим теста в игре, тоисть я в консоле выбираю пункт
// тестировать графику и запускается тест где ты собираешь данные и отправляешь их на сервер»; «делай без снимков»):
// «Тест графіки» - the console's row starts it on a map; for 75 s the driver shows each set in turn
// (core/graphics_test), the camera on the same path each time, and measures them; then the player's camera and set
// are back, and the table goes to the developers as a report of kind "test" (the settings app sends it).
namespace uagfx::win {

namespace {

using graphics_test::kSets;
using settings::Text;

std::wstring wide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring w(size_t(std::max(n, 0)), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), w.data(), n);
    return w;
}

std::string said(Text t, const std::vector<std::string>& args = {}) {
    return settings::fmt(settings::tr(startup().lang, t), args);
}

std::string resultFile() { return gameDir() + "\\uaro-graphics\\graphics-test.txt"; }

}  // namespace

double Context::testClockS() {
    static LARGE_INTEGER frequency{};
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return double(now.QuadPart) / double(frequency.QuadPart);
}

void Context::testNote(const std::string& utf8) {
    keepNoteUntil_ = 0;  // keep_fps's line ends no other
    if (enhanced_) fps_.setNote(wide(utf8));
}

// A set as the Effects page would draw it: the player's file with that set, every group on at its full strength (the
// same on every computer); the player's own as it is
EffectSettings Context::testSet(graphics_test::Set s) const {
    using graphics_test::Set;
    EffectSettings file = config.effects;
    if (s != Set::Own) {
        file.preset = s == Set::Off      ? EffectPreset::Off
                      : s == Set::Low    ? EffectPreset::Weak
                      : s == Set::Medium ? EffectPreset::Medium
                                         : EffectPreset::Strong;
        for (int g = 0; g < kEffectGroups; ++g) {
            file.groups[g] = true;
            file.groupStrength[g] = 100;
        }
        for (int g = 0; g < kOldEffectGroups; ++g) {
            file.legacyGroups[g] = true;
            file.legacyStrength[g] = 100;
        }
    }
    return madeEffects(file);
}

void Context::testStart() {
    if (test_.running()) return;
    float yaw = 0.0f, distance = 0.0f;
    bool outdoor = true, ownLimits = false;
    int limitFrom = 0, limitSpan = 0;
    const std::string map = readGameMap();
    if (map.empty() || !readCameraPose(yaw, distance) ||
        !readCameraLimits(outdoor, ownLimits, limitFrom, limitSpan)) {
        LOG_INFO("test: «Тест графіки» asked with no map shown (or in another build of the exe) - not started");
        sayOnScreen(wide(said(Text::TestNeedsMap)));
        return;
    }
    if (!enhanced_) {  // Pause: the game's own picture - nothing of the sets would show
        LOG_INFO("test: «Тест графіки» asked while Pause keeps the improvements off - not started");
        sayOnScreen(wide(said(Text::CompareOffNote)));
        return;
    }
    graphics_test::zoomRange(outdoor, ownLimits, limitFrom, limitSpan, testFrom_, testTo_);
    testStart_ = {yaw, distance};
    testMap_ = map;
    testLastS_ = testClockS();
    testLeftShown_ = -1;
    testSendAt_ = testSentUntil_ = 0;
    testFx_ = testSet(graphics_test::Set::Off);  // the game's draws ask effects() before the next present's step
    test_.start(testLastS_);
    LOG_INFO("test: «Тест графіки» on %s - %d sets, each %.0f s settling and %.0f s measured; the camera turns once "
             "from %.0f degrees, %.0f to %.0f and back (%s; it was at %.0f)",
             map.c_str(), kSets, graphics_test::kSettleS, graphics_test::kMeasureS, yaw, testFrom_, testTo_,
             ownLimits ? "the map's own limits" : outdoor ? "outdoors" : "a building", distance);
}

void Context::testFrame(bool foreground) {
    testWatchSent();
    if (testSendAt_ && GetTickCount64() >= testSendAt_) {
        testSendAt_ = 0;
        testSend();
    }
    if (!test_.running()) return;
    const char* why = !enhanced_                                      ? "Pause turned the improvements off"
                      : !foreground                                   ? "the game's window went behind"
                      : console_.open()                               ? "the console opened"
                      : !inGameWorld() || readGameMap() != testMap_ ? "the map changed"
                                                                      : nullptr;
    if (why) return testStop(why);
    const double now = testClockS();
    const graphics_test::Step step = test_.frame(now, (now - testLastS_) * 1000.0);
    testLastS_ = now;
    if (step.measureEnds) renderer->profileCut(graphics_test::tagOf(step.ended), true);
    if (step.finished) return testFinish();
    if (step.setBegins) {
        testFx_ = testSet(graphics_test::Set(step.phase));
        LOG_INFO("test: %s - its set shown, %.0f s to settle", graphics_test::tagOf(step.phase).c_str(),
                 graphics_test::kSettleS);
    }
    if (step.measureBegins) renderer->profileCut("", true);  // the settling's frames dropped
    // settling: the game eases the camera to the path's start; measured: written as shown, the same on every computer
    const graphics_test::Pose p = graphics_test::poseAt(testStart_.yaw, testFrom_, testTo_, step.u);
    writeCameraPose(p.yaw, p.distance, step.measuring);
    const int left = std::max(0, int(std::ceil(double(kSets) * graphics_test::kPhaseS - (now - test_.startS()))));
    if (left != testLeftShown_) {
        testLeftShown_ = left;
        static const Text kNames[kSets] = {Text::TestSetOff, Text::FxPresetWeak, Text::FxPresetMedium,
                                           Text::FxPresetStrong, Text::TestSetOwn};
        testNote(said(Text::TestRunning, {said(kNames[step.phase]), std::to_string(step.phase + 1),
                                          std::to_string(kSets), std::to_string(left)}));
    }
}

void Context::testStop(const char* why) {
    test_.stop();
    if (readGameMap() == testMap_) writeCameraPose(testStart_.yaw, testStart_.distance, false);
    renderer->profileCut("", false);
    LOG_INFO("test: stopped - %s; nothing sent", why);
    sayOnScreen(wide(said(Text::TestStopped)));
}

void Context::testFinish() {
    writeCameraPose(testStart_.yaw, testStart_.distance, false);  // eased back by the game
    renderer->profileCut("", false);
    for (int p = 0; p < kSets; ++p) {
        const graphics_test::PhaseResult r = test_.result(p);
        LOG_INFO("test: %s - %d frames in %.1f s, %.1f a second, the slowest tenth %.1f a second; the card %.2f ms a "
                 "frame", graphics_test::tagOf(p).c_str(), r.frames, r.seconds, r.fps, r.slowFps, r.gpuMs);
    }
    testSendAt_ = GetTickCount64() + 1500;  // the renderers' last lines in their logs first
    testNote(said(Text::TestSending));
}

void Context::testSend() {
    std::string logs, text;  // the renderers' tagged lines: this one's log, uaro-render64's
    if (readTextFile(wlog::path(), text)) logs = text;
    if (renderer->remote() &&
        readTextFile(logfiles::slotted(ourFile(layout::File::Render64Log), wlog::slot()), text))
        logs += "\n" + text;
    graphics_test::Head h;
    h.version = kVersion;
    h.map = testMap_;
    h.api = startup().choice.api == Api::D3d11 ? "Direct3D 11" : "Direct3D 12";
    h.gpu = startup().adapter;
    h.width = int(lastOut_.cx);
    h.height = int(lastOut_.cy);
    h.fpsLimit = light::fpsLimit(config.fpsLimit, light_);
    h.vsync = config.vsync;
    h.mode = light_.superLight ? said(Text::QualitySuperLight) : light_.battery ? said(Text::FramesBattery) : "";
    graphics_test::PhaseResult r[kSets];
    for (int p = 0; p < kSets; ++p) r[p] = test_.result(p);
    const std::string table = graphics_test::summary(h, r, graphics_test::profileParts(logs), startup().lang);
    size_t from = 0;
    while (from < table.size()) {  // the table in the log too
        const size_t end = std::min(table.find('\n', from), table.size());
        if (end > from) LOG_INFO("test: %s", table.substr(from, end - from).c_str());
        from = end + 1;
    }
    const std::string file = resultFile();
    DeleteFileA((file + ".sent").c_str());
    if (!writeFileAtomic(file, table)) {
        LOG_ERROR("test: %s could not be written", file.c_str());
        return;
    }
    if (!startTestReport(file)) {
        sayOnScreen(wide(said(Text::TestNoApp)));
        return;
    }
    testSentUntil_ = GetTickCount64() + 90000;
}

// The settings app's word (graphics-test.txt.sent): the report's code, or "error: <why>"
void Context::testWatchSent() {
    const uint64_t now = GetTickCount64();
    if (!testSentUntil_ || now - testSentLook_ < 500) return;
    testSentLook_ = now;
    std::string text;
    if (readTextFile(resultFile() + ".sent", text) && !text.empty()) {
        testSentUntil_ = 0;
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
        LOG_INFO("test: the settings app says: %s", text.c_str());
        const bool sent = text.rfind("R-", 0) == 0;
        sayOnScreen(wide(sent ? said(Text::TestSent, {text})
                              : said(Text::TestNotSent, {text.rfind("error: ", 0) == 0 ? text.substr(7) : text})));
        return;
    }
    if (now >= testSentUntil_) {  // the «sending» line not left on the screen
        testSentUntil_ = 0;
        LOG_INFO("test: no word from the settings app in 90 s - the result stays in %s", resultFile().c_str());
        sayOnScreen(wide(said(Text::TestNoApp)));
    }
}

}  // namespace uagfx::win

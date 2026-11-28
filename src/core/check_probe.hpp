#pragma once
// 3.0.24-beta.57 (the owner 28.11: «полную проверку всех функций ... чтобы эта проверка по мсп не занимала много
// времени»; docs/superpowers/plans/2026-11-28-full-check-probe-tour.md): the full check's setting probe - each setting
// set to another value the Alt+O way, the game's taking it waited for, its trace measured (the frame's change, the
// card's passes, the windows' words, the logs' errors), the value written back and its return waited for. The steps
// here, frame by frame and without Windows; win/context_check does what each step says.
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "core/settings/schema.hpp"

namespace uagfx::checkprobe {

// Why a setting is never touched by the check ("" it is checked): the MCP's own line, the way to the server, the
// renderer's API (a stage of its own), the launcher, the reports and statistics sent out, the log the check reads
std::string skipReason(const std::string& sectionKey);

// The value tried: a switch the other way, a choice the next word (round), a slider a quarter of its range on (back
// from the top), a number a step; "" for a line of text (not tried)
std::string testValue(const settings::IniSetting& s, const std::string& now);

// Two frames' difference: the share of pixels with a channel moved more than 12 (percent) and the channels' mean
// change (0-255); frames of another size are not compared
struct FrameDiff {
    double changed = 0, mean = 0;
    bool sizeChanged = false;
};
FrameDiff diff(const std::vector<uint32_t>& a, int aw, int ah, const std::vector<uint32_t>& b, int bw, int bh);

// The card's passes then and now: {"added": {name: ms}, "gone": [names], "more": {name: +ms}} (from +0.15 ms); an
// empty object when nothing moved
json::Value passesDelta(const std::map<std::string, double>& before, const std::map<std::string, double>& after);

// The documents whose words moved between `before` and `after` (or came, or went), those that moved by themselves
// between `before` and `again` (a second print before the change: the clock, the minimap) left out
int printsMoved(const std::map<std::string, uint64_t>& before, const std::map<std::string, uint64_t>& again,
                const std::map<std::string, uint64_t>& after);

// The log's ERROR lines ("hh:mm:ss.mmm ERROR ...", win/log's)
int errorLines(const std::string& logText);

struct Item {
    std::string key, from, to, skip;  // skip: why not tried
};

struct Row {
    std::string check = "setting";  // "restart": a restart batch's
    std::string key, from, to, skip, error;
    bool reached = false, restored = false;
    int reachMs = 0, loadMs = 0;
    bool framed = false;  // both frames read
    FrameDiff frame;
    json::Value passes;
    int uiChanged = 0, logErrors = 0;
};
json::Value rowJson(const Row& r);

// What the frame shows the probe
struct Seen {
    uint64_t ms = 0;
    std::string value;  // the running settings' word for the item's key
    bool loading = false;    // the renderer still making its one-time things (a shader, the own world)
    bool minimised = false;  // no frames: the probe waits
    bool shotReady = false;  // the frame asked for is read
    uint32_t gpuSerial = 0;  // the card's measurement's number
};

struct Timing {
    int reachMs = 3000, settleMs = 1200, loadingMs = 90000, shotMs = 3000;
};

// What the frame must do: ask the renderer for a frame (before, after), write the value tried or the old one back,
// record the row, end
enum class Do : uint8_t { Nothing, ShotBefore, Write, ShotAfter, WriteBack, Record, Finish };

enum class Phase : uint8_t { Begin, Before, Applied, Settling, After, Back, Ended };

struct Job {
    std::vector<Item> items;
    size_t at = 0;
    Phase phase = Phase::Begin;
    uint64_t since = 0, loadSince = 0, pausedAt = 0;
    uint32_t serial = 0;
    bool loadingAtStart = false;  // a loading under way before the change: the background's, not waited for
    bool told = false;            // Finish said
    Row row;            // the item's now (read it when step says Record)
};

Do step(Job& j, const Seen& s, const Timing& t);

// {"at", "of", "key", "phase", "paused"}
json::Value status(const Job& j);

// The restart batch: every value tried written at once before a restart, checked after it (`back` false: each must
// be its value tried; true, after the values went back and another restart: its old one)
json::Value restartPlan(const std::vector<Item>& items);
std::vector<Row> verifyPlan(const json::Value& plan, bool back,
                            const std::function<std::string(const std::string&)>& valueOf);

}  // namespace uagfx::checkprobe

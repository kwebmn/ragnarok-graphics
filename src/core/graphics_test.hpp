#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/settings/strings.hpp"

// 3.0.5-beta.6 (the user: «есть также идея сделать некий режим теста в игре ... чтобы не стоять кучу времени»; «делай
// без снимков»): «Тест графіки» in the game's console - the driver shows each effects set in turn with the camera on
// the same path each time, measures every one and sends the result as a report of kind "test", a short table on top.
namespace uagfx::graphics_test {

// The sets in their order: the effects off, the three sets (the player's file with that set, every group on at 100 %)
// and the player's own settings as they are. 3.0.5-beta.14 (the user: «делать» - what of «Високі»'s world costs the
// card): a set twice more, without one of its effects each - what the card does less without it is its cost (they
// work in the world's own draws: no pass of their own to time). 3.0.5-beta.23 (the user: «2» - on to «Середні» for the
// weaker cards): «Середні» without the ground's relief, without the map's smooth light (High's hills and materials
// were 3.2 and 3.6 ms of a Radeon 610M's frame)
enum class Set { Off, Low, Medium, MediumNoRelief, MediumNoSmoothLight, High, Own };
constexpr int kSets = 7;
constexpr double kSettleS = 3.0;    // a set's first seconds, not measured: its shaders and pictures made, the camera
                                    // to its start
constexpr double kMeasureS = 12.0;  // then measured: the camera turns once, goes out and back in
constexpr double kPhaseS = kSettleS + kMeasureS;
constexpr double kGpuSkipS = 1.0;  // the card's half-second averages that may still hold the settling's frames

const char* setWord(Set s);  // "off", "low", "medium", "medium-no-relief", "medium-no-smooth-light", "high", "own"
std::string tagOf(int phase);  // the profile's tag of a set's measured window: "test 2/7 low"

// The game's camera: its turn (degrees, 0-360 as the game keeps it) and how far it is from the character
struct Pose {
    float yaw = 0.0f, distance = 0.0f;
};
// The path at u (0..1) of a measured window: a full turn from startYaw; out from `from` to `to` and back, a half sine
Pose poseAt(float startYaw, float from, float to, double u);
// The distances the path goes between: outdoors 300 to 1400; a building map the game's own 230 to 300; a map with
// camera limits of its own (CGameMode+0x284) from limitFrom over limitSpan
void zoomRange(bool outdoor, bool ownLimits, int limitFrom, int limitSpan, float& from, float& to);

struct PhaseResult {
    int frames = 0;
    double seconds = 0.0;
    double fps = 0.0, slowFps = 0.0;        // slowFps: the slowest tenth of the frames, as frames a second
    double gpuMs = -1.0;                    // the card's work a frame, below 0 not measured
    std::map<std::string, double> effects;  // its timed passes' ms (GpuTiming::effects), averaged
};

// One present of the game in the test
struct Step {
    int phase = 0;  // the set shown: 0-4
    bool setBegins = false;      // switch to it, the camera to its path's start
    bool measureBegins = false;  // its measured window begins (the profile's window starts anew)
    bool measureEnds = false;    // the window of `ended` ends (its profile lines into the log)
    int ended = 0;
    bool finished = false;  // the last set measured: the player's settings back, the result made
    bool measuring = false;
    double u = 0.0;  // the camera's path while measuring, 0..1
};

class Run {
public:
    void start(double nowS);
    bool running() const { return running_; }
    void stop() { running_ = false; }
    double startS() const { return startS_; }
    // each present of the game: the time now (s) and since the last present (ms)
    Step frame(double nowS, double frameMs);
    // a new measurement of the card's (GpuTiming): its ms a frame and its timed passes' at nowS
    void gpu(double nowS, double gpuMs, const std::map<std::string, double>& effects);
    PhaseResult result(int phase) const;

private:
    bool running_ = false;
    double startS_ = 0.0;
    int phase_ = -1;
    bool measuring_ = false;
    std::vector<double> frameMs_[kSets];
    double gpuSum_[kSets] = {};
    int gpuSamples_[kSets] = {};
    std::map<std::string, double> effectSums_[kSets];
};

// A set's window in the renderers' tagged lines («profile [test 2/5 low]: the card 6.20 ms a frame - grass 2.10,
// ...»): the card's ms a frame and its parts, the dearest first
struct Parts {
    double cardMs = -1.0;
    std::vector<std::pair<std::string, double>> parts;
};
std::map<int, Parts> profileParts(const std::string& log);  // by phase (0-4)

// The table's first line
struct Head {
    std::string version, map, api, gpu;
    int width = 0, height = 0;  // the game's picture
    int fpsLimit = 0;           // 0 none
    bool vsync = false;
    std::string mode;  // the light mode's name, "" none
};
// The short table on top of the report: each set's frames a second, its slowest tenth, the card's ms and its three
// dearest parts (the profile's, else its timed passes). 3.0.5-beta.14: under it lines of what effects cost the card -
// 3.0.5-beta.23: «Середні»'s relief and smooth light by the card's ms of «Середні» less those of the set without each,
// «Високі»'s ground seams' strips and world's reflections by their spans in the profile; one not measured left out
std::string summary(const Head& h, const PhaseResult (&r)[kSets], const std::map<int, Parts>& parts,
                    settings::Lang lang);
// 3.0.5-beta.7: that table read back (the settings app's «Гра» shows the last test): its first line, the columns'
// headings, each set's cells (one not measured: its name and the words); false when the text is no such table
struct Table {
    std::string head;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};
bool parseSummary(const std::string& text, Table& out);

// 3.0.5-beta.7: the settings app's «Тест графіки» starts the game with this in its environment; the driver begins the
// test on the first map after kWaitS seconds there (a countdown on the screen: the player stands still)
constexpr const char* kStartMark = "UARO_GRAPHICS_TEST";
constexpr double kWaitS = 10.0;

}  // namespace uagfx::graphics_test

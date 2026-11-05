#include "win/portrait.hpp"

#include <windows.h>

#include <algorithm>
#include <map>
#include <tuple>

#include "core/accounts.hpp"
#include "core/class_art.hpp"
#include "core/layout.hpp"
#include "core/shot_spec.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/window_stack.hpp"

namespace uagfx::win::portraitwin {
namespace {

constexpr ULONGLONG kEveryMs = 10 * 60 * 1000, kFirstMs = 60 * 1000, kStillMs = 3000;
constexpr uint32_t kOwnAccount = 0x771F7C;

struct State {
    std::string name;
    ULONGLONG enteredAt = 0, takenAt = 0, stillSince = 0;
    int cellX = -1, cellY = -1;
    std::map<std::tuple<std::string, int>, panel::Picture> faces;
    std::map<std::string, ULONGLONG> stamp;  // a name's file's time read
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

std::string fileOf(const std::string& nameUtf8) {
    char hex[24];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(accounts::loginHash(nameUtf8)));
    return layout::join(layout::join(gameDir(), layout::kPortraits), std::string(hex) + ".bgra");
}

}  // namespace

std::string wanted(bool on, const std::string& nameUtf8, int gameWidth, int gameHeight) {
    State& s = state();
    const ULONGLONG now = GetTickCount64();
    if (!on || nameUtf8.empty() || !inGameWorld() || gameWidth <= 0 || gameHeight <= 0) {
        s.name.clear();
        return "";
    }
    if (nameUtf8 != s.name) {  // another character, or back in the world
        s.name = nameUtf8;
        s.enteredAt = now;
        s.takenAt = 0;
        s.stillSince = now;
    }
    int x = 0, y = 0, w = 0, h = 0;
    if (!readPlayerCell(x, y, w, h)) return "";
    if (x != s.cellX || y != s.cellY) {
        s.cellX = x;
        s.cellY = y;
        s.stillSince = now;
        return "";
    }
    if (now - s.enteredAt < kFirstMs || (s.takenAt && now - s.takenAt < kEveryMs) || now - s.stillSince < kStillMs ||
        windowstack::top())
        return "";
    s.takenAt = now;
    // its sprite's box in the game's pixels, grown to a 3:4 picture standing on its feet; else the screen's middle
    float box[4] = {};
    uint32_t own = 0;
    float cx = float(gameWidth) / 2, feet = float(gameHeight) * 0.55f, tall = float(gameHeight) * 0.22f;
    if (readGame(kOwnAccount, &own, 4) && readActorBox(own, box) && box[2] > box[0] && box[3] > box[1]) {
        cx = (box[0] + box[2]) / 2;
        feet = box[3];
        tall = (box[3] - box[1]) * 1.35f;
    }
    const float ph = std::clamp(tall, float(gameHeight) * 0.12f, float(gameHeight) * 0.6f), pw = ph * 0.75f;
    const float left = std::clamp(cx - pw / 2, 0.0f, float(gameWidth) - pw);
    const float top = std::clamp(feet + ph * 0.06f - ph, 0.0f, float(gameHeight) - ph);
    CreateDirectoryA(layout::join(gameDir(), layout::kPortraits).c_str(), nullptr);
    const std::string file = fileOf(nameUtf8);
    s.stamp.erase(nameUtf8);  // read again when shown
    if (s.told++ < 20) LOG_INFO("portrait: %s taken (%s)", nameUtf8.c_str(), file.c_str());
    return shotspec::withCrop(file, left / float(gameWidth), top / float(gameHeight), pw / float(gameWidth),
                              ph / float(gameHeight));
}

const panel::Picture* face(const std::string& nameUtf8, int side) {
    State& s = state();
    if (nameUtf8.empty() || side <= 0) return nullptr;
    const std::string file = fileOf(nameUtf8);
    WIN32_FILE_ATTRIBUTE_DATA at{};
    if (!GetFileAttributesExA(file.c_str(), GetFileExInfoStandard, &at)) return nullptr;
    const ULONGLONG written = (ULONGLONG(at.ftLastWriteTime.dwHighDateTime) << 32) | at.ftLastWriteTime.dwLowDateTime;
    const auto key = std::make_tuple(nameUtf8, side);
    auto it = s.faces.find(key);
    if (it == s.faces.end() || s.stamp[nameUtf8] != written) {
        s.stamp[nameUtf8] = written;
        std::string bytes;
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        panel::Picture p;
        if (readTextFile(file, bytes) && shotspec::readRaw(bytes, argb, w, h)) {
            p.width = p.height = side;
            p.px.assign(size_t(side) * size_t(side), 0u);
            classart::cover(p, panel::Box{0, 0, float(side), float(side)}, argb, w, h, float(side) * 0.12f,
                            classart::kTopCorners | 12);
        }
        it = s.faces.insert_or_assign(key, std::move(p)).first;
    }
    return it->second.width > 0 ? &it->second : nullptr;
}

}  // namespace uagfx::win::portraitwin

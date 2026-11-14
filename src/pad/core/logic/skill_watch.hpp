#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace uapad {

// 3.0.19-beta.19 (the testers' round 3: «у монка коли прожимаешь азура ... с далека то він просто підходить і касту
// нема»): a skill the client asks for on a target (its 0x0072), how far apart by the server's walks, and the server's
// answer - used it (a hit, 0x01de / 0x0114 / 0x0115, or 0x011a), began its cast (0x013e), refused it (0x0110), or
// none in kWaitMs (uAthena's unit_skilluse_id2 says nothing when it is out of its range) - the lines for the pad's log
class SkillWatch {
public:
    static constexpr uint32_t kWaitMs = 1500;
    static constexpr int kMostLines = 40;  // a session's at most

    void setSelf(uint32_t id) { self_ = id; }
    // The client asks for `skill` (`name` the server's, empty unknown) at `lv` on `target`, `cells` apart by the
    // server's walks (-1 unknown), its range `range` (0 unknown); a new ask takes the place of one unanswered
    std::optional<std::string> asked(uint16_t skill, const std::string& name, uint16_t lv, uint32_t target, int cells,
                                     int range, uint32_t nowMs);
    std::optional<std::string> used(uint16_t skill, uint32_t src, uint32_t nowMs);
    std::optional<std::string> castBegun(uint16_t skill, uint32_t src, uint32_t nowMs);
    std::optional<std::string> refused(uint16_t skill, int cause, uint32_t nowMs);
    std::optional<std::string> tick(uint32_t nowMs);  // no answer in kWaitMs: once

private:
    std::optional<std::string> answer(const char* what, uint32_t nowMs);
    std::optional<std::string> line(std::string text);

    uint32_t self_ = 0;
    bool waiting_ = false;
    uint16_t skill_ = 0;
    uint32_t sentMs_ = 0;
    std::string label_;
    int lines_ = 0;
};

}  // namespace uapad

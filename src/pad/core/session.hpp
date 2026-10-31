#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pad/core/bytes.hpp"
#include "pad/core/config/bindings.hpp"
#include "pad/core/config/config.hpp"
#include "pad/core/logic/controller.hpp"
#include "pad/core/proto/framer.hpp"
#include "pad/core/world/world.hpp"

namespace uapad {

// Protocol state of the client's sockets plus the controller. Called from the game thread only.
class Session {
public:
    using SocketId = uintptr_t;

    explicit Session(const Config& cfg);

    void onClientSent(SocketId s, const uint8_t* data, size_t n, uint32_t nowMs);  // bytes really sent
    void onServerData(SocketId s, const uint8_t* data, size_t n, uint32_t nowMs);  // bytes really received
    void onClosed(SocketId s);
    // Traffic on `s` that the hooks could not see (overlapped WSASend, a call from another thread).
    // On the map socket it ends trust in that direction until the next connection.
    void onUnobserved(SocketId s, Direction dir);

    // Runs the controller. Its requests are queued for injection (and still listed in the output).
    TickOutput tick(const PadState& pad, const ViewInfo& view, uint32_t nowMs);
    // Queued requests that may go out now; requests that waited longer than 500 ms are dropped.
    std::vector<Bytes> takeInjectable(uint32_t nowMs);
    bool canInject() const;
    void markInjectBroken();
    void setMark(MarkStyle style);  // [aim] mark, changed from the menu
    void setClientTurn(std::optional<double> radians) { controller_.setClientTurn(radians); }
    void setWalkRadius(float fraction) { controller_.setWalkRadius(fraction); }
    // Appends the queued client-only packets (the "!" over a new target) to the `n` bytes the client is
    // about to receive on `s` in a buffer of `cap` bytes: only on the map socket, only while the server
    // stream is understood and stands at a packet boundary, only whole packets. Returns the bytes added.
    size_t appendForClient(SocketId s, uint8_t* buf, size_t n, size_t cap, uint32_t nowMs);
    // F1-F9 as read from the client's memory (it never sends its bar). Logs the bar when it changes.
    void setClientBar(const std::array<ClientBarSlot, kClientBarSlots>& bar);

    // Every character's layout (uaro-pad-binds.ini); the one of the character on the map applies at once.
    void setProfiles(const Profiles& profiles);
    const Profiles& profiles() const { return profiles_; }
    // The settings menu saved `bindings` for the character on the map (before anyone logs in: the default).
    void saveBindings(const Bindings& bindings);
    // Asks the server for our character's name (once per map server connection): the menu shows it.
    void askOwnName(uint32_t nowMs);
    uint32_t charId() const { return charId_; }            // from 0x009b; 0 before the first map login
    const std::string& charName() const { return charName_; }  // as the server spells it; may be empty

    SocketId mapSocket() const { return mapSocket_; }
    const WorldState& world() const { return world_; }
    const Controller& controller() const { return controller_; }
    void setFriends(std::vector<uint32_t> ids) { controller_.setFriends(std::move(ids)); }  // 3.0.11-beta.85
    std::vector<std::string> takeLog();

private:
    struct Conn {
        enum class Kind { Unknown, Map, Other };
        Kind kind = Kind::Unknown;
        Bytes head;  // first client bytes, until the connection can be classified
        std::unique_ptr<StreamFramer> up;    // client -> server
        std::unique_ptr<StreamFramer> down;  // server -> client
    };

    void feedUp(Conn& c, const uint8_t* data, size_t n, uint32_t nowMs);
    // Sight / Ruwach / Poison: the client draws the effect on the target; the one before gets its state back.
    void updatePersistentMark(uint32_t nowMs);
    bool justStoppedWalking(uint32_t nowMs) const;

    WorldState world_;
    Controller controller_;
    MarkStyle mark_ = MarkStyle::Gasp;  // [aim] mark
    uint32_t marked_ = 0;   // the monster the client draws our persistent mark on
    bool remark_ = false;   // the server redrew it (or the style changed): send the mark again
    bool padWalking_ = false;
    std::optional<uint32_t> walkStopMs_;  // the stick walk ended: what follows is logged for a moment
    std::optional<std::array<ClientBarSlot, kClientBarSlots>> loggedBar_;
    Profiles profiles_;
    uint32_t charId_ = 0;
    std::string charName_;
    bool nameAsked_ = false;  // our own name was requested on this map server connection
    std::unordered_map<SocketId, Conn> conns_;
    SocketId mapSocket_ = 0;
    bool downOk_ = false;
    bool injectOk_ = false;
    std::deque<std::pair<uint32_t, Bytes>> queue_;
    std::deque<std::pair<uint32_t, Bytes>> toClient_;  // packets made up for the client, with their deadline
    std::vector<std::string> log_;
};

}  // namespace uapad

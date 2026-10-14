#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/bridge_protocol.hpp"
#include "core/bridge_ring.hpp"

namespace uagfx::win::bridge {

using uagfx::bridge::Msg;

// Part 3: the driver's and the renderer's ends of the shared block (core/bridge_protocol.hpp) and its named events.
// Built into ddraw.dll (32-bit) and uaro-render64.exe (64-bit) alike.
class Channel {
public:
    Channel() = default;
    ~Channel();
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    // The driver: makes the block, both rings and the events, named after `name` ("Local\..."). False with a reason.
    bool create(const std::string& name, uint32_t toHostBytes, uint32_t toClientBytes, std::string& error);
    // The renderer: opens what the driver made.
    bool open(const std::string& name, std::string& error);
    void close();
    // The other process: its exit ends every wait (send and receive return false).
    void setPeer(HANDLE process) { peer_ = process; }
    bool peerGone() const;

    // One message, in chunks when it is larger than a ring can take at once; waits while the ring is full. False
    // when the peer is gone.
    bool send(Msg type, const void* data, size_t size);
    bool send(Msg type, const std::vector<uint8_t>& payload) { return send(type, payload.data(), payload.size()); }
    bool send(Msg type) { return send(type, nullptr, 0); }
    // The next whole message: in place in the ring when it came as one chunk, valid until the next receive(). Waits
    // up to `ms` milliseconds (INFINITE too); false on a timeout or when the peer is gone (gone() tells).
    bool receive(Msg& type, const uint8_t*& data, uint32_t& size, DWORD ms);

    // The renderer after each picture it shows; the driver waits until at least `count` have been shown.
    void notifyPresented();
    uint32_t presented() const;
    bool waitPresented(uint32_t count, DWORD ms);

private:
    bool attach(bool host, const std::string& name, std::string& error);
    bool wait(HANDLE event, DWORD ms);  // false: timeout or the peer is gone

    bool host_ = false;
    HANDLE mapping_ = nullptr;
    uint8_t* view_ = nullptr;
    HANDLE dataOut_ = nullptr;   // signalled after writing to the outgoing ring
    HANDLE spaceOut_ = nullptr;  // the peer signals it after reading the outgoing ring
    HANDLE dataIn_ = nullptr;
    HANDLE spaceIn_ = nullptr;
    HANDLE presentedEvent_ = nullptr;
    HANDLE peer_ = nullptr;
    uagfx::bridge::Ring out_;
    uagfx::bridge::Ring in_;
    uagfx::bridge::Assembler assembler_;
    bool consumePending_ = false;  // the message receive() returned in place is still in the ring
};

}  // namespace uagfx::win::bridge

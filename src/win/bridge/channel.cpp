#include "win/bridge/channel.hpp"

#include <algorithm>
#include <new>

namespace uagfx::win::bridge {

using uagfx::bridge::BlockHeader;
using uagfx::bridge::ChunkHeader;
using uagfx::bridge::kMagic;
using uagfx::bridge::kVersion;
using uagfx::bridge::Ring;

namespace {

std::string lastError(const char* what) {
    return std::string(what) + " failed (error " + std::to_string(GetLastError()) + ")";
}

HANDLE event(bool create, const std::string& name) {
    return create ? CreateEventA(nullptr, FALSE, FALSE, name.c_str())
                  : OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name.c_str());
}

}  // namespace

Channel::~Channel() {
    close();
    DeleteCriticalSection(&sendLock_);
}

bool Channel::create(const std::string& name, uint32_t toHostBytes, uint32_t toClientBytes, std::string& error) {
    close();
    const uint64_t size = uint64_t(sizeof(BlockHeader)) + toHostBytes + toClientBytes;
    mapping_ = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, DWORD(size >> 32), DWORD(size),
                                  name.c_str());
    if (!mapping_) {
        error = lastError("CreateFileMapping");
        return false;
    }
    view_ = static_cast<uint8_t*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, SIZE_T(size)));
    if (!view_) {
        error = lastError("MapViewOfFile");
        close();
        return false;
    }
    auto* b = new (view_) BlockHeader{};
    b->magic = kMagic;
    b->version = kVersion;
    b->toHost = toHostBytes;
    b->toClient = toClientBytes;
    b->presented.store(0);
    Ring(view_ + sizeof(BlockHeader), toHostBytes).init();
    Ring(view_ + sizeof(BlockHeader) + toHostBytes, toClientBytes).init();
    return attach(false, name, error);
}

bool Channel::open(const std::string& name, std::string& error) {
    close();
    mapping_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    if (!mapping_) {
        error = lastError("OpenFileMapping");
        return false;
    }
    view_ = static_cast<uint8_t*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (!view_) {
        error = lastError("MapViewOfFile");
        close();
        return false;
    }
    const auto* b = reinterpret_cast<const BlockHeader*>(view_);
    if (b->magic != kMagic || b->version != kVersion) {
        error = "the driver speaks another version of the bridge";
        close();
        return false;
    }
    return attach(true, name, error);
}

bool Channel::attach(bool host, const std::string& name, std::string& error) {
    host_ = host;
    const auto* b = reinterpret_cast<const BlockHeader*>(view_);
    Ring toHost(view_ + sizeof(BlockHeader), b->toHost);
    Ring toClient(view_ + sizeof(BlockHeader) + b->toHost, b->toClient);
    out_ = host ? toClient : toHost;
    in_ = host ? toHost : toClient;
    const bool create = !host;
    HANDLE toHostData = event(create, name + "-to-host-data");
    HANDLE toHostSpace = event(create, name + "-to-host-space");
    HANDLE toClientData = event(create, name + "-to-client-data");
    HANDLE toClientSpace = event(create, name + "-to-client-space");
    presentedEvent_ = event(create, name + "-presented");
    dataOut_ = host ? toClientData : toHostData;
    spaceOut_ = host ? toClientSpace : toHostSpace;
    dataIn_ = host ? toHostData : toClientData;
    spaceIn_ = host ? toHostSpace : toClientSpace;
    if (!dataOut_ || !spaceOut_ || !dataIn_ || !spaceIn_ || !presentedEvent_ || !out_.valid() || !in_.valid()) {
        error = lastError("the bridge's events");
        close();
        return false;
    }
    return true;
}

void Channel::close() {
    for (HANDLE* h : {&dataOut_, &spaceOut_, &dataIn_, &spaceIn_, &presentedEvent_}) {
        if (*h) CloseHandle(*h);
        *h = nullptr;
    }
    if (view_) UnmapViewOfFile(view_);
    view_ = nullptr;
    if (mapping_) CloseHandle(mapping_);
    mapping_ = nullptr;
    out_ = Ring();
    in_ = Ring();
    assembler_.reset();
    consumePending_ = false;
}

bool Channel::peerGone() const { return peer_ && WaitForSingleObject(peer_, 0) == WAIT_OBJECT_0; }

bool Channel::wait(HANDLE event, DWORD ms) {
    HANDLE both[2] = {event, peer_};
    const DWORD r = WaitForMultipleObjects(peer_ ? 2 : 1, both, FALSE, ms);
    return r == WAIT_OBJECT_0;
}

bool Channel::send(Msg type, const void* data, size_t size, DWORD ms) {
    if (!view_ || size > 0xFFFFFFFFu) return false;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    const uint32_t total = uint32_t(size);
    const ULONGLONG start = GetTickCount64();
    uint32_t offset = 0;
    bool ok = true;
    EnterCriticalSection(&sendLock_);
    do {
        const uint32_t chunk = std::min(total - offset, out_.maxChunk());
        while (ok && !out_.tryWrite(uint32_t(type), total, offset, bytes + offset, chunk)) {
            if (peerGone() || (ms != INFINITE && GetTickCount64() - start >= ms)) {
                ok = false;
                break;
            }
            if (whileWaiting_ && !inWhileWaiting_) {
                inWhileWaiting_ = true;
                whileWaiting_(waitingContext_);
                inWhileWaiting_ = false;
            }
            wait(spaceOut_, whileWaiting_ ? 5 : 50);  // a missed signal costs at most this long
        }
        if (!ok) break;
        SetEvent(dataOut_);
        offset += chunk;
    } while (offset < total);
    LeaveCriticalSection(&sendLock_);
    return ok;
}

bool Channel::trySend(Msg type, const std::vector<uint8_t>& payload) {
    if (!view_ || payload.size() > out_.maxChunk()) return false;
    // Never waits, not even for the lock: the main thread may be inside a long send (0.10 review 10).
    if (!TryEnterCriticalSection(&sendLock_)) return false;
    const uint32_t size = uint32_t(payload.size());
    const bool ok = out_.tryWrite(uint32_t(type), size, 0, payload.data(), size);
    LeaveCriticalSection(&sendLock_);
    if (ok) SetEvent(dataOut_);
    return ok;
}

bool Channel::receive(Msg& type, const uint8_t*& data, uint32_t& size, DWORD ms) {
    if (!view_) return false;
    if (consumePending_) {
        in_.consume();
        consumePending_ = false;
        SetEvent(spaceIn_);
    }
    const ULONGLONG start = GetTickCount64();
    for (;;) {
        ChunkHeader c;
        const uint8_t* chunk = nullptr;
        while (in_.peek(c, chunk)) {
            if (c.offset == 0 && c.size == c.total) {  // the whole message in one chunk: in place
                type = Msg(c.type);
                data = chunk;
                size = c.size;
                consumePending_ = true;
                return true;
            }
            const bool fits = assembler_.feed(c, chunk);
            in_.consume();
            SetEvent(spaceIn_);
            if (fits && assembler_.complete()) {
                type = Msg(assembler_.type());
                data = assembler_.data();
                size = assembler_.size();
                assembler_.reset();  // the buffer stays valid until the next feed
                return true;
            }
        }
        if (peerGone()) return false;
        const ULONGLONG spent = GetTickCount64() - start;
        if (ms != INFINITE && spent >= ms) return false;
        const DWORD left = ms == INFINITE ? 100 : DWORD(std::min<ULONGLONG>(ms - spent, 100));
        wait(dataIn_, left);  // short slices: a missed signal or the peer's exit is noticed soon
    }
}

void Channel::notifyPresented() {
    if (!view_) return;
    reinterpret_cast<BlockHeader*>(view_)->presented.fetch_add(1);
    SetEvent(presentedEvent_);
}

uint32_t Channel::presented() const {
    return view_ ? reinterpret_cast<const BlockHeader*>(view_)->presented.load() : 0;
}

bool Channel::waitPresented(uint32_t count, DWORD ms) {
    const ULONGLONG start = GetTickCount64();
    while (int32_t(presented() - count) < 0) {
        if (peerGone()) return false;
        const ULONGLONG spent = GetTickCount64() - start;
        if (spent >= ms) return false;
        wait(presentedEvent_, DWORD(std::min<ULONGLONG>(ms - spent, 20)));
    }
    return true;
}

}  // namespace uagfx::win::bridge

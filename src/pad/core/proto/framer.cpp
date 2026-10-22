#include "pad/core/proto/framer.hpp"

#include "pad/core/proto/packet_tables.hpp"

namespace uapad {

namespace {
constexpr size_t kMaxPacket = 0x8000;
}  // namespace

StreamFramer::StreamFramer(Direction dir, size_t rawPrefix) : dir_(dir), prefixLeft_(rawPrefix) {}

int StreamFramer::lengthOf(uint16_t id) const {
    // clif.c lists 0x0000 as 10 bytes, but nobody sends it: zero runs mean we lost the stream.
    if (id == 0) return 0;
    return dir_ == Direction::ServerToClient ? s2cLength(id) : c2sLength(id);
}

bool StreamFramer::feed(const uint8_t* data, size_t n, std::vector<Bytes>& out) {
    if (desynced_) return false;
    size_t i = 0;
    while (prefixLeft_ > 0 && i < n) {
        prefix_.push_back(data[i++]);
        --prefixLeft_;
    }
    buf_.insert(buf_.end(), data + i, data + n);
    size_t pos = 0;
    while (buf_.size() - pos >= 2) {
        const uint16_t id = rd16(buf_.data(), pos);
        const int len = lengthOf(id);
        size_t need = 0;
        if (len > 0) {
            need = size_t(len);
        } else if (len == -1) {
            if (buf_.size() - pos < 4) break;
            need = rd16(buf_.data(), pos + 2);
            if (need < 4 || need > kMaxPacket) {
                desynced_ = true;
                badId_ = id;
                break;
            }
        } else {
            desynced_ = true;
            badId_ = id;
            break;
        }
        if (buf_.size() - pos < need) break;
        out.emplace_back(buf_.begin() + long(pos), buf_.begin() + long(pos + need));
        pos += need;
    }
    if (desynced_) {
        buf_.clear();
        return false;
    }
    buf_.erase(buf_.begin(), buf_.begin() + long(pos));
    return true;
}

}  // namespace uapad

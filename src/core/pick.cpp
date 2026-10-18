#include "core/pick.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/d3d7.hpp"

namespace uagfx::pick {

bool inTriangle(float ax, float ay, float bx, float by, float cx, float cy, float x, float y) {
    const float d1 = (x - bx) * (ay - by) - (ax - bx) * (y - by);
    const float d2 = (x - cx) * (by - cy) - (bx - cx) * (y - cy);
    const float d3 = (x - ax) * (cy - ay) - (cx - ax) * (y - ay);
    const float area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    if (area == 0.0f) return false;
    const bool negative = d1 < 0 || d2 < 0 || d3 < 0, positive = d1 > 0 || d2 > 0 || d3 > 0;
    return !(negative && positive);
}

void Request::ask(float x, float y, uint64_t nowMs) {
    const bool again = before_ && std::fabs(x - x_) <= 3.0f && std::fabs(y - y_) <= 3.0f && nowMs - at_ < 15000;
    depth_ = again ? depth_ + 1 : 0;
    asked_ = true;
    searched_ = false;
    before_ = true;
    x_ = x;
    y_ = y;
    at_ = nowMs;
    opaque_.clear();
    blended_.clear();
}

void Request::seen(const std::vector<uint32_t>& opaque, const std::vector<uint32_t>& blended) {
    if (!asked_) return;
    searched_ = true;
    opaque_.insert(opaque_.begin(), opaque.begin(), opaque.end());  // a later record over the earlier ones
    blended_.insert(blended_.begin(), blended.begin(), blended.end());
}

bool Request::due(uint32_t& hit) {
    if (!asked_ || !searched_) return false;
    asked_ = false;
    std::vector<uint32_t> order;  // the opaque ones first, each texture once
    for (const std::vector<uint32_t>* list : {&opaque_, &blended_})
        for (uint32_t id : *list)
            if (id && std::find(order.begin(), order.end(), id) == order.end()) order.push_back(id);
    hit = order.empty() ? 0 : order[size_t(depth_) % order.size()];
    return true;
}

std::vector<int> drawsAt(const FrameRecord& r, float x, float y, const std::function<bool(const DrawCmd&)>& wanted) {
    std::vector<int> out;
    const std::vector<uint8_t>& bytes = r.vertexBytes();
    const std::vector<uint16_t>& indices = r.indices();
    for (size_t k = r.draws().size(); k-- > 0;) {
        const DrawCmd& d = r.draws()[k];
        if (d.primitive < d3d7::PT_TRIANGLELIST || d.primitive > d3d7::PT_TRIANGLEFAN || !wanted(d)) continue;
        const uint32_t stride = FrameRecord::stride(d.fvf);
        const uint32_t count = d.indexCount ? d.indexCount : d.vertexCount;
        const auto at = [&](uint32_t i, float& px, float& py) {  // the i-th vertex of the primitive
            const uint32_t v = d.indexCount ? (d.firstIndex + i < indices.size() ? indices[d.firstIndex + i] : ~0u) : i;
            const size_t off = size_t(d.vertexOffset) + size_t(v) * stride;
            if (v >= d.vertexCount || off + 8 > bytes.size()) return false;
            std::memcpy(&px, &bytes[off], 4);
            std::memcpy(&py, &bytes[off + 4], 4);
            return true;
        };
        const uint32_t triangles = d.primitive == d3d7::PT_TRIANGLELIST ? count / 3 : count >= 3 ? count - 2 : 0;
        for (uint32_t t = 0; t < triangles; ++t) {
            uint32_t a = t * 3, b = a + 1, c = a + 2;
            if (d.primitive == d3d7::PT_TRIANGLESTRIP) a = t, b = t + 1, c = t + 2;
            if (d.primitive == d3d7::PT_TRIANGLEFAN) a = 0, b = t + 1, c = t + 2;
            float p[6];
            if (!at(a, p[0], p[1]) || !at(b, p[2], p[3]) || !at(c, p[4], p[5])) break;
            if (inTriangle(p[0], p[1], p[2], p[3], p[4], p[5], x, y)) {
                out.push_back(int(k));
                break;
            }
        }
    }
    return out;
}

}  // namespace uagfx::pick

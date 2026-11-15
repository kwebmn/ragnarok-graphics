#include "core/quad_batch.hpp"

namespace uagfx::quadbatch {

void append(std::vector<uint8_t>& vertices, std::vector<uint16_t>& indices, const uint8_t* quad, size_t stride) {
    const uint16_t base = uint16_t(vertices.size() / stride);
    vertices.insert(vertices.end(), quad, quad + 4 * stride);
    const uint16_t corner[6] = {0, 1, 2, 2, 1, 3};
    for (const uint16_t c : corner) indices.push_back(uint16_t(base + c));
}

bool isBatch(uint32_t vertexCount, uint32_t indexCount) {
    return vertexCount >= 4 && vertexCount % 4 == 0 && indexCount == vertexCount / 4 * 6;
}

}  // namespace uagfx::quadbatch

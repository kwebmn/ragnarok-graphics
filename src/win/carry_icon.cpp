#include "win/carry_icon.hpp"

#include "win/picture_draw.hpp"

namespace uagfx::win::carryicon {
namespace {

struct Put {
    panel::Picture* pic = nullptr;
    uint32_t* texture = nullptr;
    int* texW = nullptr;
    int* texH = nullptr;
    bool* upload = nullptr;
    int x = 0, y = 0;
    float k = 0;
};
Put g_put;

}  // namespace

void put(panel::Picture& pic, uint32_t& texture, int& texW, int& texH, bool& upload, int x, int y, float k) {
    g_put = Put{&pic, &texture, &texW, &texH, &upload, x, y, k};
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    if (!g_put.pic) return;
    drawPart(record, renderer, *g_put.pic, *g_put.texture, *g_put.texW, *g_put.texH, *g_put.upload, g_put.x, g_put.y,
             g_put.k);
    g_put = Put{};
}

}  // namespace uagfx::win::carryicon

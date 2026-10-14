#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/ai/srvgg.hpp"
#include "core/ai/texture_cache.hpp"
#include "test.hpp"

using namespace uagfx::ai;

namespace {

bool loadModel(Model& m, const char* name = "realesr-general-x4v3") {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/ai/" + name + ".bin", std::ios::binary);
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string error;
    return parseModel(file, m, error);
}

}  // namespace

TEST(ai_network_matches_the_independent_python_forward_pass) {
    // tools: an independent pure-Python forward pass read straight from the .pth gave these values for this 5x4
    // input (channel 0, rows 0 and 5, every third column).
    Model m;
    CHECK(loadModel(m));
    CHECK_EQ(m.middle, 32);
    CHECK_EQ(m.scale, 4);
    const int W = 5, H = 4;
    std::vector<float> rgb(3 * W * H), out(3 * W * H * 16);
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                rgb[size_t((c * H + y) * W + x)] = float((c * 37 + y * 11 + x * 7) % 23) / 22.0f;
    upscale(m, rgb.data(), W, H, out.data(), Border::Zero);
    const float row0[6] = {0.0f, 0.123672f, 0.394143f, 0.634569f, 0.70155f, 0.564371f};
    for (int i = 0; i < 6; ++i) CHECK(std::fabs(out[size_t(i * 3)] - row0[i]) < 1e-4f);
    for (float v : out) CHECK(v >= 0.0f && v <= 1.0f);
}

TEST(ai_wrapped_border_keeps_a_repeating_texture_repeating) {
    // The ground repeats one texture every 4x4 tiles (comodo.gnd: 98% of the sand tiles continue their neighbour's
    // quarter). With a zero border the network made each repeat's edges up from black, and the beach showed a seam
    // every 4 tiles (0.11.1). Wrapped, the network sees the texture as the ground shows it: a texture moved round
    // by (dx, dy) comes out moved round by 4 * (dx, dy), so the enlarged one repeats without a seam too.
    Model m;
    CHECK(loadModel(m));
    const int W = 8, H = 6, dx = 3, dy = 2, S = 4, OW = W * S, OH = H * S;
    std::vector<float> rgb(3 * W * H), moved(3 * W * H), out(3 * OW * OH), outMoved(3 * OW * OH);
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                rgb[size_t((c * H + y) * W + x)] = float((c * 29 + y * 13 + x * 5 + x * y) % 31) / 30.0f;
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                moved[size_t((c * H + (y + dy) % H) * W + (x + dx) % W)] = rgb[size_t((c * H + y) * W + x)];
    upscale(m, rgb.data(), W, H, out.data(), Border::Wrap);
    upscale(m, moved.data(), W, H, outMoved.data(), Border::Wrap);
    float worst = 0.0f;
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < OH; ++y)
            for (int x = 0; x < OW; ++x) {
                const float a = out[size_t((c * OH + y) * OW + x)];
                const float b = outMoved[size_t((c * OH + (y + dy * S) % OH) * OW + (x + dx * S) % OW)];
                worst = std::max(worst, std::fabs(a - b));
            }
    CHECK(worst < 1e-5f);
    // The zero border, as Real-ESRGAN was trained, is what the reference check above runs: not this.
    upscale(m, rgb.data(), W, H, out.data(), Border::Zero);
    upscale(m, moved.data(), W, H, outMoved.data(), Border::Zero);
    worst = 0.0f;
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < OH; ++y)
            for (int x = 0; x < OW; ++x) {
                const float a = out[size_t((c * OH + y) * OW + x)];
                const float b = outMoved[size_t((c * OH + (y + dy * S) % OH) * OW + (x + dx * S) % OW)];
                worst = std::max(worst, std::fabs(a - b));
            }
    CHECK(worst > 0.01f);
}

TEST(ai_models_mix_by_denoise_strength) {
    // Real-ESRGAN's denoise strength for realesr-general-x4v3: its weights and the weak-denoise model's
    // (realesr-general-wdn-x4v3) mixed, d * x4v3 + (1 - d) * wdn. 100 is the model alone (0.11.0-0.11.2, sand came out
    // flat), 50 Real-ESRGAN's default, 0 keeps the most grain.
    Model strong, weak, mixed;
    CHECK(loadModel(strong));
    CHECK(loadModel(weak, "realesr-general-wdn-x4v3"));
    std::string error;
    CHECK(mixModels(strong, weak, 100, mixed, error));
    CHECK(mixed.data == strong.data);
    CHECK(mixModels(strong, weak, 0, mixed, error));
    CHECK(mixed.data == weak.data);
    CHECK(mixModels(strong, weak, 50, mixed, error));
    CHECK_EQ(mixed.middle, strong.middle);
    CHECK_EQ(mixed.convs.size(), strong.convs.size());
    float worst = 0.0f;
    for (size_t i = 0; i < mixed.data.size(); ++i)
        worst = std::max(worst, std::fabs(mixed.data[i] - 0.5f * (strong.data[i] + weak.data[i])));
    CHECK(worst < 1e-6f);
    Model other = weak;
    other.data.pop_back();
    CHECK(!mixModels(strong, other, 50, mixed, error) && !error.empty());  // not the same network
}

TEST(ai_model_file_must_be_whole) {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/ai/realesr-general-x4v3.bin", std::ios::binary);
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Model m;
    std::string error;
    CHECK(parseModel(file, m, error));
    file.pop_back();
    CHECK(!parseModel(file, m, error));
    CHECK(!parseModel(std::vector<uint8_t>{'N', 'O', 'P', 'E'}, m, error));
}

TEST(ai_texture_key_names_and_blob_round_trip) {
    std::vector<uint32_t> a(64 * 32), b;
    for (size_t i = 0; i < a.size(); ++i) a[i] = 0xFF000000u | uint32_t(i * 2654435761u >> 8);
    const TextureKey k = textureKey(a.data(), 64, 32);
    CHECK(k.width == 64 && k.height == 32);
    a[100] ^= 1;
    CHECK(textureKey(a.data(), 64, 32).hash != k.hash);  // one texel differs: another texture
    CHECK(textureKey(a.data(), 32, 64).hash != textureKey(a.data(), 64, 32).hash);
    const std::string name = entryName("realesr-general-x4v3", k);
    // the prefix, 16 hex digits and ".tex"
    CHECK(name.rfind("ai\\realesr-general-x4v3\\64x32\\", 0) == 0 && name.size() == 30 + 16 + 4);
    CHECK(entryFolder("realesr-general-x4v3") == "ai\\realesr-general-x4v3\\");  // how upscaled.grf's are told apart
    CHECK(name.rfind(entryFolder("realesr-general-x4v3"), 0) == 0);
    int w = 0, h = 0;
    CHECK(decodeTexture(encodeTexture(a.data(), 64, 32), b, w, h) && w == 64 && h == 32 && b == a);
    std::vector<uint8_t> broken = encodeTexture(a.data(), 64, 32);
    broken.pop_back();
    CHECK(!decodeTexture(broken, b, w, h));
}

TEST(ai_keyed_texels_take_their_neighbours_colour_and_alpha_grows_smooth) {
    // A 4x4 texture: magenta keyed away (alpha 0) in its left column, the rest a flat green.
    std::vector<uint32_t> t(16, 0xFF20C040u);
    for (int y = 0; y < 4; ++y) t[size_t(y * 4)] = 0x00FF00FFu;
    CHECK(hasTransparency(t.data(), 4, 4));
    std::vector<float> planes;
    prepareInput(t.data(), 4, 4, planes);
    for (int y = 0; y < 4; ++y) {
        const size_t i = size_t(y * 4);
        CHECK(std::fabs(planes[i] - 0x20 / 255.0f) < 1e-6f);        // red as the green neighbour, not magenta
        CHECK(std::fabs(planes[16 + i] - 0xC0 / 255.0f) < 1e-6f);
        CHECK(std::fabs(planes[32 + i] - 0x40 / 255.0f) < 1e-6f);
        CHECK_EQ(planes[48 + i], 0.0f);                             // still transparent
        CHECK_EQ(planes[48 + i + 1], 1.0f);
    }
    std::vector<uint8_t> alpha;
    enlargeAlpha(planes.data() + 48, 4, 4, 4, alpha);
    CHECK_EQ(alpha.size(), size_t(256));
    CHECK_EQ(alpha[0], 0);        // deep inside the keyed column
    CHECK_EQ(alpha[15], 255);     // well inside the visible part
    // The edge sits half way between the first two texel centres: 4 output pixels in.
    CHECK_EQ(alpha[3], 0);
    CHECK_EQ(alpha[4], 255);
    std::vector<uint32_t> opaque(16, 0xFF102030u);
    CHECK(!hasTransparency(opaque.data(), 4, 4));
}

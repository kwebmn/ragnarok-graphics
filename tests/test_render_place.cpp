#include <string>

#include "core/bridge_protocol.hpp"
#include "core/render_place.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.20-beta.83 (R-EGT98G: the 64-bit renderer gone with 0xC0000409 five seconds after a map's first frame, as on the
// owner's PC 16.11): where the renderer's thread stood, told by the driver once the process is gone

TEST(render_place_in_a_message_with_its_step_and_the_ones_before) {
    renderplace::Place p;
    p.message = uint32_t(bridge::Msg::Execute);
    p.step = renderplace::kDraws;
    p.sinceMs = 1000;
    p.done[0] = uint32_t(bridge::Msg::Present);
    p.done[1] = uint32_t(bridge::Msg::Execute);
    p.done[2] = uint32_t(bridge::Msg::Camera);
    CHECK_EQ(renderplace::describe(p, 6210),
             std::string("in Execute for 5210 ms (its draws); before it: Present, Execute, Camera"));
}

TEST(render_place_between_messages) {
    renderplace::Place p;
    p.done[0] = uint32_t(bridge::Msg::Present);
    p.done[1] = uint32_t(bridge::Msg::Map);
    CHECK_EQ(renderplace::describe(p, 5000), std::string("between messages; before it: Present, Map"));
}

TEST(render_place_an_unknown_message_by_its_number) {
    renderplace::Place p;
    p.message = 999;
    p.sinceMs = 7000;
    CHECK_EQ(renderplace::describe(p, 7000), std::string("in message 999 for 0 ms; before it: none"));
    CHECK_EQ(std::string(renderplace::messageName(uint32_t(bridge::Msg::GameModelsFrame))),
             std::string("GameModelsFrame"));
    CHECK_EQ(std::string(renderplace::messageName(999)), std::string(""));
}

namespace {

uint32_t g_stepped = 0;
void noteStep(uint32_t s) { g_stepped = s; }

}  // namespace

TEST(render_place_steps_go_to_the_renderers_sink) {
    renderplace::setStepSink(&noteStep);
    renderplace::step(renderplace::kUploads);
    CHECK_EQ(g_stepped, uint32_t(renderplace::kUploads));
    renderplace::setStepSink(nullptr);
    renderplace::step(renderplace::kSubmit);  // no sink: nothing
    CHECK_EQ(g_stepped, uint32_t(renderplace::kUploads));
    CHECK_EQ(std::string(renderplace::stepName(renderplace::kAfterDraws)),
             std::string("the casters and the own ground's notes"));
    CHECK_EQ(std::string(renderplace::stepName(renderplace::kLooks)), std::string("the textures' looks worked out"));
    CHECK_EQ(std::string(renderplace::stepName(renderplace::kShow)), std::string("the swap chain's present"));
}
